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
void settle() { esp_rom_delay_us(200); }

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

  // working_half leaves BUSY as a plain floating input, so match it exactly
  // before adding any pull resistor back.
  gpio_config_t in = {};
  in.pin_bit_mask = (1ULL << kPinBusy);
  in.mode = GPIO_MODE_INPUT;
  in.pull_up_en = GPIO_PULLUP_DISABLE;
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

  int actual_khz = 0;
  if (spi_device_get_actual_freq(spi_, &actual_khz) != ESP_OK) {
    actual_khz = 0;
  }
  ESP_LOGI(TAG,
           "SPI up: MOSI=%d CLK=%d CS=%d DC=%d RST=%d BUSY=%d PWR=%d "
           "requested=%d actual=%d Hz",
           (int)kPinMosi, (int)kPinSclk, (int)kPinCs, (int)kPinDc, (int)kPinRst,
           (int)kPinBusy, (int)kPinPwr, kSpiClockHz, actual_khz * 1000);
  ESP_LOGI(TAG, "BUSY level before reset: %d", busy_level());
  return true;
}

void Driver::send_raw(const uint8_t *data, size_t len) {
  if (len == 0) {
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
  settle();
  gpio_set_level(kPinDc, 0);
  settle();
  gpio_set_level(kPinCs, 0);
  settle();
  send_raw(&cmd, 1);
  settle();
  gpio_set_level(kPinCs, 1);
  settle();
}

void Driver::send_data(uint8_t data) {
  gpio_set_level(kPinCs, 1);
  settle();
  gpio_set_level(kPinDc, 1);
  settle();
  gpio_set_level(kPinCs, 0);
  settle();
  send_raw(&data, 1);
  settle();
  gpio_set_level(kPinCs, 1);
  settle();
}

void Driver::start_stream(uint8_t cmd) {
  send_cmd(cmd);
  gpio_set_level(kPinDc, 1);
  settle();
  gpio_set_level(kPinCs, 0);
  settle();
}

void Driver::end_stream() {
  settle();
  gpio_set_level(kPinCs, 1);
  settle();
}

void Driver::power_output(bool high) {
  gpio_config_t o = {};
  o.pin_bit_mask = (1ULL << kPinPwr);
  o.mode = GPIO_MODE_OUTPUT;
  gpio_config(&o);
  gpio_set_level(kPinPwr, high ? 1 : 0);
}

void Driver::power_high_z() {
  gpio_config_t i = {};
  i.pin_bit_mask = (1ULL << kPinPwr);
  i.mode = GPIO_MODE_INPUT;
  i.pull_up_en = GPIO_PULLUP_ENABLE;
  i.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpio_config(&i);
}

void Driver::reset_panel() {
  // The working_half sketch holds RST low for a real 1 ms. vTaskDelay(1) rounds
  // down to zero ticks at the default 100 Hz tick, so use a microsecond delay:
  // with a ~0 us pulse the UC8179 never resets and stays stuck asserting BUSY.
  gpio_set_level(kPinRst, 0);
  esp_rom_delay_us(1000);
  gpio_set_level(kPinRst, 1);
  delay_ms(200);
  ESP_LOGI(TAG, "panel reset done, BUSY=%d", busy_level());
}

// Init follows the working_half / reference escreen.h sequence exactly, and
// deliberately omits TRES (0x61), 0x15, 0x60 and the 0x01/0x06 booster setup.
// This controller's OTP already describes the 648x480 panel; the extra
// commands were what kept the panel silent.
void Driver::init_mode(bool partial) {
  if (!partial) {
    // Drive voltages. The Good Display sample relies on OTP defaults; with
    // those, solid areas switch but 1-2 px glyph strokes stay grey. These are
    // Seeed's UC8179 values (VGH=20V, VGL=-20V, VDH=15V, VDL=-15V).
    send_cmd(0x01);
    send_data(0x07);
    send_data(0x07);
    send_data(0x3F);
    send_data(0x3F);

    send_cmd(0x06); // booster soft start
    send_data(0x17);
    send_data(0x17);
    send_data(0x28);
    send_data(0x17);
  }

  send_cmd(0x00); // panel setting
  send_data(0x1F);

  send_cmd(0x04); // power on
  delay_ms(100);
  (void)wait_idle(5000, "power on");

  if (partial) {
    send_cmd(0x50);
    send_data(0x29);
    send_data(0x07);
    // Partial waveform.
    send_cmd(0xE0);
    send_data(0x02);
    send_cmd(0xE5);
    send_data(0x58);
  } else {
    send_cmd(0x50); // VCOM and data interval
    send_data(0x21);
    send_data(0x07);
  }

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
  send_data(0x01); // PT_SCAN
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
      const size_t n =
          (row_bytes - offset < space) ? (row_bytes - offset) : space;
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
  // Exactly the working_half WaitUntilIdle loop: poll GET_STATUS (0x71), then
  // sample BUSY. On this HAT the status command is part of what makes the line
  // observable at all.
  uint32_t waited = 0;
  while (true) {
    send_cmd(0x71);
    if (!is_busy()) {
      break;
    }
    if (waited >= timeout_ms) {
      ESP_LOGW(TAG, "%s: BUSY did not release after %u ms", what,
               (unsigned)waited);
      return false;
    }
    delay_ms(10);
    waited += 10;
  }
  delay_ms(200);
  ESP_LOGI(TAG, "%s: idle (%u ms)", what, (unsigned)waited);
  return true;
}

bool Driver::draw_and_refresh(const uint8_t *frame, int x1_byte, int x2_byte,
                              int y1, int y2) {
  if (!wait_idle(20000, "pre-draw")) {
    return false;
  }

  set_window(x1_byte, x2_byte, y1, y2);
  start_stream(0x13);
  write_window(frame, kStride, x1_byte, x2_byte, y1, y2);
  end_stream();

  send_cmd(0x12);
  delay_ms(10);
  return wait_idle(45000, "refresh");
}

bool Driver::clear(uint8_t level) {
  if (!wait_idle(30000, "pre-clear")) {
    return false;
  }

  init_mode(false);

  // Both planes are loaded so the first partial update starts from a known
  // image (0x10 = previous, 0x13 = next).
  start_stream(0x10);
  write_fill(level, kFrameBytes);
  end_stream();

  start_stream(0x13);
  write_fill(level, kFrameBytes);
  end_stream();

  send_cmd(0x12);
  delay_ms(10);
  return wait_idle(45000, "clear");
}

bool Driver::full_refresh(const uint8_t *frame) {
  if (!wait_idle(30000, "pre-full")) {
    return false;
  }

  init_mode(false);

  // OLD = white, NEW = current frame, full-screen, no partial window.
  start_stream(0x10);
  write_fill(0xFF, kFrameBytes);
  end_stream();

  start_stream(0x13);
  write_window(frame, kStride, 0, kStride - 1, 0, kHeight - 1);
  end_stream();

  send_cmd(0x12);
  delay_ms(10);
  const bool ok = wait_idle(45000, "full refresh");
  init_mode(true);
  return ok;
}

bool Driver::partial_update(const uint8_t *frame, const Rect &area) {
  if (!partial_mode_) {
    init_mode(true);
  }

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

  const bool ok = draw_and_refresh(frame, x1b, x2b, y1, y2);
  ESP_LOGI(TAG, "partial x=%d..%d y=%d..%d -> %s", x1b * 8, x2b * 8 + 7, y1,
           y2, ok ? "ok" : "TIMEOUT");
  return ok;
}

} // namespace epd
