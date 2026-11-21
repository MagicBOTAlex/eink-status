#include "epd.hpp"

#include <cstring>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace epd {
namespace {

constexpr const char *TAG = "EPD";
constexpr size_t kDmaChunk = 4096;

void delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

} // namespace

bool Driver::begin() {
  gpio_config_t out = {};
  out.pin_bit_mask = (1ULL << kPinCs) | (1ULL << kPinDc) | (1ULL << kPinRst) |
                     (1ULL << kPinPwr);
  out.mode = GPIO_MODE_OUTPUT;
  out.pull_up_en = GPIO_PULLUP_DISABLE;
  out.pull_down_en = GPIO_PULLDOWN_DISABLE;
  out.intr_type = GPIO_INTR_DISABLE;
  ESP_ERROR_CHECK(gpio_config(&out));

  gpio_config_t in = {};
  in.pin_bit_mask = (1ULL << kPinBusy);
  in.mode = GPIO_MODE_INPUT;
  in.pull_up_en = GPIO_PULLUP_ENABLE;
  in.pull_down_en = GPIO_PULLDOWN_DISABLE;
  in.intr_type = GPIO_INTR_DISABLE;
  ESP_ERROR_CHECK(gpio_config(&in));

  gpio_set_level(kPinCs, 1);
  gpio_set_level(kPinDc, 1);
  gpio_set_level(kPinRst, 1);
  gpio_set_level(kPinPwr, 1);

  spi_bus_config_t bus = {};
  bus.mosi_io_num = kPinMosi;
  bus.miso_io_num = -1;
  bus.sclk_io_num = kPinSclk;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = static_cast<int>(kDmaChunk);

  esp_err_t err = spi_bus_initialize(kSpiHost, &bus, SPI_DMA_CH_AUTO);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(err));
    return false;
  }

  spi_device_interface_config_t dev = {};
  dev.clock_speed_hz = kSpiClockHz;
  dev.mode = 0;
  dev.spics_io_num = -1; // CS is a plain GPIO
  dev.queue_size = 7;
  dev.flags = SPI_DEVICE_HALFDUPLEX;

  err = spi_bus_add_device(kSpiHost, &dev, &spi_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(err));
    return false;
  }

  dma_buf_ = static_cast<uint8_t *>(
      heap_caps_malloc(kDmaChunk, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  if (dma_buf_ == nullptr) {
    ESP_LOGE(TAG, "no DMA-capable scratch buffer");
    return false;
  }

  ESP_LOGI(TAG, "SPI up: MOSI=%d CLK=%d CS=%d DC=%d RST=%d BUSY=%d PWR=%d @ %d Hz",
           (int)kPinMosi, (int)kPinSclk, (int)kPinCs, (int)kPinDc, (int)kPinRst,
           (int)kPinBusy, (int)kPinPwr, kSpiClockHz);
  ESP_LOGI(TAG, "BUSY level before reset: %d", busy_level());
  return true;
}

void Driver::send_raw(const uint8_t *data, size_t len) {
  if (len == 0) {
    return;
  }
  if (soft_mode_) {
    for (size_t i = 0; i < len; ++i) {
      const uint8_t b = data[i];
      for (int bit = 7; bit >= 0; --bit) {
        gpio_set_level(kPinMosi, (b >> bit) & 1);
        esp_rom_delay_us(2);
        gpio_set_level(kPinSclk, 1);
        esp_rom_delay_us(2);
        gpio_set_level(kPinSclk, 0);
      }
    }
    return;
  }
  spi_transaction_t t = {};
  t.length = len * 8;
  t.tx_buffer = data;
  esp_err_t err = spi_device_polling_transmit(spi_, &t);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "SPI tx %u B failed: %s", (unsigned)len,
             esp_err_to_name(err));
  }
}

void Driver::send_cmd(uint8_t cmd) {
  gpio_set_level(kPinCs, 1);
  gpio_set_level(kPinDc, 0);
  gpio_set_level(kPinCs, 0);
  send_raw(&cmd, 1);
  gpio_set_level(kPinCs, 1);
}

void Driver::send_data(uint8_t data) {
  gpio_set_level(kPinCs, 1);
  gpio_set_level(kPinDc, 1);
  gpio_set_level(kPinCs, 0);
  send_raw(&data, 1);
  gpio_set_level(kPinCs, 1);
}

void Driver::start_stream(uint8_t cmd) {
  send_cmd(cmd);
  gpio_set_level(kPinDc, 1);
  gpio_set_level(kPinCs, 0);
}

void Driver::end_stream() { gpio_set_level(kPinCs, 1); }

void Driver::reset_panel() {
  gpio_set_level(kPinRst, 1);
  delay_ms(20);
  gpio_set_level(kPinRst, 0);
  delay_ms(10);
  gpio_set_level(kPinRst, 1);
  delay_ms(20);
  ESP_LOGI(TAG, "panel reset done, BUSY=%d", busy_level());
}

void Driver::init_mode(bool partial) {
  send_cmd(0x00);
  send_data(0x1F);

  // The controller powers up with a 680 px default resolution; this panel is
  // 648 px. Without TRES the data counter expects 85-byte rows and the 81-byte
  // rows we send never fill the frame, so 0x12 never starts a refresh.
  send_cmd(0x61);
  send_data((kWidth >> 8) & 0xFF);
  send_data(kWidth & 0xFF);
  send_data((kHeight >> 8) & 0xFF);
  send_data(kHeight & 0xFF);

  send_cmd(0x04); // power on
  delay_ms(100);

  send_cmd(0x50);
  send_data(0x29);
  send_data(0x07);

  send_cmd(0xE0); // cascade / clock output
  send_data(0x02);

  // Forced temperature: the Good Display sample uses a higher value for the
  // partial waveform. Same values as ESPHome's GDEY0583T81 driver.
  send_cmd(0xE5);
  send_data(partial ? 0x6E : 0x5A);

  partial_mode_ = partial;
  ESP_LOGI(TAG, "init mode: %s", partial ? "partial" : "full");
}

bool Driver::reset_and_init(bool partial) {
  reset_panel();
  init_mode(partial);
  return wait_idle(5000, "init");
}

void Driver::set_window(int x1_byte, int x2_byte, int y1, int y2) {
  send_cmd(0x50);
  send_data(0xA9);
  send_data(0x07);

  send_cmd(0x91); // partial in

  send_cmd(0x90); // partial window
  const uint16_t xs = static_cast<uint16_t>(x1_byte * 8);
  const uint16_t xe = static_cast<uint16_t>(x2_byte * 8 + 7);
  const uint16_t ys = static_cast<uint16_t>(y1);
  const uint16_t ye = static_cast<uint16_t>(y2);
  send_data(xs >> 8);
  send_data(xs & 0xFF);
  send_data(xe >> 8);
  send_data(xe & 0xFF);
  send_data(ys >> 8);
  send_data(ys & 0xFF);
  send_data(ye >> 8);
  send_data(ye & 0xFF);
  send_data(0x01); // PT_SCAN: scan inside and outside the window
}

void Driver::write_window(const uint8_t *src, int stride, int x1_byte,
                          int x2_byte, int y1, int y2) {
  const size_t row_bytes = static_cast<size_t>(x2_byte - x1_byte + 1);
  size_t used = 0;

  for (int y = y1; y <= y2; ++y) {
    const uint8_t *row = src + static_cast<size_t>(y) * stride + x1_byte;
    size_t offset = 0;
    while (offset < row_bytes) {
      const size_t space = kDmaChunk - used;
      const size_t n = (row_bytes - offset < space) ? (row_bytes - offset) : space;
      std::memcpy(dma_buf_ + used, row + offset, n);
      used += n;
      offset += n;
      if (used == kDmaChunk) {
        send_raw(dma_buf_, used);
        used = 0;
      }
    }
  }

  if (used > 0) {
    send_raw(dma_buf_, used);
  }
}

void Driver::write_fill(uint8_t value, size_t count) {
  std::memset(dma_buf_, value, kDmaChunk);
  while (count > 0) {
    const size_t n = count < kDmaChunk ? count : kDmaChunk;
    send_raw(dma_buf_, n);
    count -= n;
  }
}

bool Driver::wait_idle(uint32_t timeout_ms, const char *what) {
  uint32_t waited = 0;
  while (is_busy()) {
    if (waited >= timeout_ms) {
      ESP_LOGW(TAG, "%s: BUSY did not release after %u ms", what,
               (unsigned)waited);
      return false;
    }
    delay_ms(10);
    waited += 10;
  }
  ESP_LOGI(TAG, "%s: idle (%u ms)", what, (unsigned)waited);
  return true;
}

bool Driver::full_refresh(const uint8_t *frame) {
  if (!wait_idle(30000, "pre-full")) {
    return false;
  }

  init_mode(false);

  // A full update is a plain full-frame NEW write with no partial window. The
  // controller's full waveform drives every pixel (including the white phases
  // that clear ghosting). This mirrors the GoodDisplay/ESPHome sequence.
  start_stream(0x13);
  write_window(frame, kStride, 0, kStride - 1, 0, kHeight - 1);
  end_stream();

  send_cmd(0x12);
  delay_ms(20);
  const bool ok = wait_idle(45000, "full refresh");

  // Leave the controller in the fast partial configuration for normal updates.
  init_mode(true);
  return ok;
}

void Driver::soft_init() {
  gpio_config_t o = {};
  o.pin_bit_mask = (1ULL << kPinMosi) | (1ULL << kPinSclk);
  o.mode = GPIO_MODE_OUTPUT;
  o.pull_up_en = GPIO_PULLUP_DISABLE;
  o.pull_down_en = GPIO_PULLDOWN_DISABLE;
  o.intr_type = GPIO_INTR_DISABLE;
  ESP_ERROR_CHECK(gpio_config(&o));
  gpio_set_level(kPinMosi, 0);
  gpio_set_level(kPinSclk, 0);
  soft_mode_ = true;
  ESP_LOGI(TAG, "software SPI enabled (MOSI=%d CLK=%d)", (int)kPinMosi,
           (int)kPinSclk);
}

bool Driver::soft_spi_liveness_test() {
  soft_init();
  reset_panel();
  init_mode(false);

  start_stream(0x13);
  write_fill(0x00, kFrameBytes);
  end_stream();

  send_cmd(0x12);

  bool asserted = false;
  for (int i = 0; i < 300; ++i) {
    if (is_busy()) {
      asserted = true;
      break;
    }
    delay_ms(10);
  }
  ESP_LOGI(TAG, "soft-spi: BUSY asserted=%d level=%d", asserted, busy_level());
  if (asserted) {
    wait_idle(45000, "soft-spi");
  }
  soft_mode_ = false;
  return asserted;
}

bool Driver::refresh_liveness_test(const char *label, uint8_t old_fill,
                                   uint8_t new_fill) {
  (void)old_fill;
  reset_panel();
  init_mode(false);

  start_stream(0x13);
  write_fill(new_fill, kFrameBytes);
  end_stream();

  send_cmd(0x12);

  bool asserted = false;
  for (int i = 0; i < 100; ++i) {
    if (is_busy()) {
      asserted = true;
      break;
    }
    delay_ms(10);
  }
  ESP_LOGI(TAG, "%s: BUSY asserted=%d level=%d", label, asserted, busy_level());
  if (asserted) {
    wait_idle(45000, label);
  }
  return asserted;
}

bool Driver::partial_update(const uint8_t *old_frame, const uint8_t *new_frame,
                            const Rect &area) {
  if (!partial_mode_) {
    init_mode(true);
  }

  // Whole byte columns only; the panel addresses 8-pixel banks.
  int x1b = area.x1 / 8;
  int x2b = area.x2 / 8;
  if (x1b < 0) {
    x1b = 0;
  }
  if (x2b > kStride - 1) {
    x2b = kStride - 1;
  }
  int y1 = area.y1 < 0 ? 0 : area.y1;
  int y2 = area.y2 >= kHeight ? kHeight - 1 : area.y2;
  if (x1b > x2b || y1 > y2) {
    return true;
  }

  if (!wait_idle(20000, "pre-partial")) {
    return false;
  }

  set_window(x1b, x2b, y1, y2);
  start_stream(0x10);
  write_window(old_frame, kStride, x1b, x2b, y1, y2);
  end_stream();

  start_stream(0x13);
  write_window(new_frame, kStride, x1b, x2b, y1, y2);
  end_stream();

  send_cmd(0x12);
  delay_ms(20);
  const bool ok = wait_idle(20000, "partial refresh");
  if (ok) {
    send_cmd(0x92); // partial out, back to normal mode
  }
  ESP_LOGI(TAG, "partial x=%d..%d y=%d..%d -> %s", x1b * 8, x2b * 8 + 7, y1,
           y2, ok ? "ok" : "TIMEOUT");
  return ok;
}

} // namespace epd
