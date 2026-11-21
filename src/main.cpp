#include <cstdio>
#include <cstring>

#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "epd.hpp"
#include "wifi.hpp"

namespace {

constexpr const char *TAG = "MAIN";

// Set to 1 to run the panel liveness/polarity probes at boot. These were used
// to establish that the bench panel was not responding at all; keep them for
// the next hardware session.
#define EPD_DIAG 0

// LVGL renders in horizontal strips. 648 px = 81 bytes per row; 60 rows plus
// room for the indexed-format palette in front of the pixels.
constexpr size_t kDrawRows = 60;
constexpr size_t kDrawBufBytes =
    static_cast<size_t>(epd::kStride) * kDrawRows + 64;

constexpr uint32_t kCounterPeriodMs = 2000;
constexpr uint32_t kTickPeriodMs = 7000;
constexpr uint32_t kWifiPollMs = 1000;
constexpr uint32_t kFullRefreshPeriodMs = 120000;

epd::Driver g_driver;
uint8_t *g_next = nullptr;
uint8_t *g_prev = nullptr;
lv_display_t *g_disp = nullptr;
lv_obj_t *g_counter = nullptr;
lv_obj_t *g_status = nullptr;
lv_obj_t *g_mode = nullptr;
lv_obj_t *g_tick = nullptr;

lv_area_t g_dirty = {0, 0, -1, -1};

uint32_t tick_cb() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

uint8_t *alloc_frame() {
  uint8_t *p = static_cast<uint8_t *>(
      heap_caps_calloc(1, epd::kFrameBytes, MALLOC_CAP_SPIRAM));
  if (p == nullptr) {
    ESP_LOGW(TAG, "PSRAM frame allocation failed; falling back to internal RAM");
    p = static_cast<uint8_t *>(
        heap_caps_calloc(1, epd::kFrameBytes, MALLOC_CAP_8BIT));
  }
  return p;
}

void dirty_join(const lv_area_t *area) {
  if (g_dirty.x1 > g_dirty.x2) {
    g_dirty = *area;
    return;
  }
  g_dirty.x1 = area->x1 < g_dirty.x1 ? area->x1 : g_dirty.x1;
  g_dirty.y1 = area->y1 < g_dirty.y1 ? area->y1 : g_dirty.y1;
  g_dirty.x2 = area->x2 > g_dirty.x2 ? area->x2 : g_dirty.x2;
  g_dirty.y2 = area->y2 > g_dirty.y2 ? area->y2 : g_dirty.y2;
}

void dirty_reset() { g_dirty = {0, 0, -1, -1}; }

// LVGL 9 indexed formats put a palette in front of the pixel bytes of every
// draw buffer. The flush callback receives the buffer start, not the first
// pixel.
constexpr uint32_t kPaletteBytes =
    LV_COLOR_INDEXED_PALETTE_SIZE(LV_COLOR_FORMAT_I1) * sizeof(lv_color32_t);

void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  if (area->x2 < 0 || area->y2 < 0 || area->x1 >= epd::kWidth ||
      area->y1 >= epd::kHeight) {
    lv_display_flush_ready(disp);
    return;
  }

  const int32_t w = lv_area_get_width(area);
  const uint32_t src_stride = lv_draw_buf_width_to_stride(
      w, lv_display_get_color_format(disp));
  const int32_t copy_bytes = (w + 7) / 8;
  const int32_t dst_x_byte = area->x1 / 8;

  const uint8_t *src = px_map + kPaletteBytes;
  for (int32_t y = area->y1; y <= area->y2; ++y) {
    std::memcpy(g_next + static_cast<size_t>(y) * epd::kStride + dst_x_byte,
                src + static_cast<size_t>(y - area->y1) * src_stride,
                copy_bytes);
  }

  dirty_join(area);
  lv_display_flush_ready(disp);
}

void build_ui() {
  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_obj_set_style_border_width(scr, 0, 0);

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "eink-status");
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 16, 12);

  g_mode = lv_label_create(scr);
  lv_label_set_text(g_mode, "partial");
  lv_obj_set_style_text_color(g_mode, lv_color_white(), 0);
  lv_obj_set_style_text_font(g_mode, &lv_font_montserrat_14, 0);
  lv_obj_align(g_mode, LV_ALIGN_TOP_RIGHT, -16, 12);

  g_counter = lv_label_create(scr);
  lv_label_set_text(g_counter, "0000");
  lv_obj_set_style_text_color(g_counter, lv_color_white(), 0);
  lv_obj_set_style_text_font(g_counter, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_letter_space(g_counter, 10, 0);
  lv_obj_align(g_counter, LV_ALIGN_CENTER, 0, -18);

  lv_obj_t *subtitle = lv_label_create(scr);
  lv_label_set_text(subtitle, "LVGL 1bpp / GDEY0583T81");
  lv_obj_set_style_text_color(subtitle, lv_color_white(), 0);
  lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_14, 0);
  lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, 40);

  g_status = lv_label_create(scr);
  lv_label_set_text(g_status, "wifi: starting");
  lv_obj_set_style_text_color(g_status, lv_color_white(), 0);
  lv_obj_set_style_text_font(g_status, &lv_font_montserrat_14, 0);
  lv_obj_align(g_status, LV_ALIGN_BOTTOM_LEFT, 16, -12);

  g_tick = lv_obj_create(scr);
  lv_obj_set_size(g_tick, 36, 36);
  lv_obj_set_style_bg_color(g_tick, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(g_tick, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_tick, 0, 0);
  lv_obj_set_style_radius(g_tick, 0, 0);
  lv_obj_align(g_tick, LV_ALIGN_BOTTOM_RIGHT, -16, -10);
}

void set_mode_label(const char *text) {
  if (g_mode != nullptr) {
    lv_label_set_text(g_mode, text);
  }
}

void copy_area(uint8_t *dst, const uint8_t *src, int x1_byte, int x2_byte,
               int y1, int y2) {
  const size_t n = static_cast<size_t>(x2_byte - x1_byte + 1);
  for (int y = y1; y <= y2; ++y) {
    std::memcpy(dst + static_cast<size_t>(y) * epd::kStride + x1_byte,
                src + static_cast<size_t>(y) * epd::kStride + x1_byte, n);
  }
}

bool send_dirty_area() {
  if (g_dirty.x1 > g_dirty.x2 || g_dirty.y1 > g_dirty.y2) {
    dirty_reset();
    return true;
  }

  epd::Rect area = {g_dirty.x1 & ~7, g_dirty.y1,
                    (g_dirty.x2 | 7), g_dirty.y2};
  if (area.x1 < 0) {
    area.x1 = 0;
  }
  if (area.y1 < 0) {
    area.y1 = 0;
  }
  if (area.x2 >= epd::kWidth) {
    area.x2 = epd::kWidth - 1;
  }
  if (area.y2 >= epd::kHeight) {
    area.y2 = epd::kHeight - 1;
  }

  const bool ok = g_driver.partial_update(g_prev, g_next, area);
  if (ok) {
    copy_area(g_prev, g_next, area.x1 / 8, area.x2 / 8, area.y1, area.y2);
  }
  dirty_reset();
  return ok;
}

void full_refresh_now(const char *why) {
  ESP_LOGI(TAG, "full refresh: %s", why);
  set_mode_label("full");
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(g_disp);
  dirty_reset();

  const bool ok = g_driver.full_refresh(g_next);
  if (ok) {
    std::memcpy(g_prev, g_next, epd::kFrameBytes);
    set_mode_label("partial");
  } else {
    ESP_LOGE(TAG, "full refresh failed");
  }
}

void refresh_wifi_label() {
  static char last[64] = {};
  const netwifi::Status st = netwifi::get_status();
  char line[64];
  if (st.connected) {
    std::snprintf(line, sizeof(line), "wifi: %s (%s)", st.ssid, st.ip);
  } else {
    std::snprintf(line, sizeof(line), "wifi: connecting...");
  }
  if (std::strcmp(line, last) != 0) {
    std::snprintf(last, sizeof(last), "%s", line);
    lv_label_set_text(g_status, line);
    ESP_LOGI(TAG, "%s", line);
  }
}

} // namespace

extern "C" void app_main() {
  ESP_LOGI(TAG, "boot");
#if EPD_DIAG
  ESP_LOGI(TAG, "diagnostic build: waiting 5 s for the console");
  vTaskDelay(pdMS_TO_TICKS(5000));
#endif
  ESP_LOGI(TAG, "heap: internal free=%u largest=%u psram free=%u",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

  g_next = alloc_frame();
  g_prev = alloc_frame();
  if (g_next == nullptr || g_prev == nullptr) {
    ESP_LOGE(TAG, "frame buffers unavailable");
    return;
  }
  std::memset(g_next, 0xFF, epd::kFrameBytes);
  std::memset(g_prev, 0xFF, epd::kFrameBytes);

  if (!g_driver.begin()) {
    ESP_LOGE(TAG, "EPD driver init failed");
    return;
  }

#if EPD_DIAG
  {
    gpio_config_t probe = {};
    probe.pin_bit_mask = (1ULL << epd::kPinBusy);
    probe.mode = GPIO_MODE_INPUT;
    probe.intr_type = GPIO_INTR_DISABLE;
    probe.pull_down_en = GPIO_PULLDOWN_ENABLE;
    probe.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&probe);
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGI(TAG, "BUSY probe pull-down: %d", gpio_get_level(epd::kPinBusy));
    probe.pull_down_en = GPIO_PULLDOWN_DISABLE;
    probe.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&probe);
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGI(TAG, "BUSY probe pull-up: %d", gpio_get_level(epd::kPinBusy));
  }

  ESP_LOGI(TAG, "diagnostic: PWR high, hardware SPI");
  gpio_set_level(epd::kPinPwr, 1);
  vTaskDelay(pdMS_TO_TICKS(300));
  g_driver.refresh_liveness_test("pwr-high", 0xFF, 0x00);

  ESP_LOGI(TAG, "diagnostic: PWR high-Z, hardware SPI");
  {
    gpio_config_t pwr = {};
    pwr.pin_bit_mask = (1ULL << epd::kPinPwr);
    pwr.mode = GPIO_MODE_INPUT;
    pwr.pull_up_en = GPIO_PULLUP_ENABLE;
    pwr.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&pwr);
  }
  vTaskDelay(pdMS_TO_TICKS(300));
  g_driver.refresh_liveness_test("pwr-hiz", 0xFF, 0x00);

  ESP_LOGI(TAG, "diagnostic: PWR high, software SPI");
  {
    gpio_config_t pwr = {};
    pwr.pin_bit_mask = (1ULL << epd::kPinPwr);
    pwr.mode = GPIO_MODE_OUTPUT;
    gpio_config(&pwr);
  }
  gpio_set_level(epd::kPinPwr, 1);
  vTaskDelay(pdMS_TO_TICKS(300));
  g_driver.soft_spi_liveness_test();

  ESP_LOGI(TAG, "diagnostic done; PWR high");
  gpio_set_level(epd::kPinPwr, 1);
  vTaskDelay(pdMS_TO_TICKS(300));
#endif

  ESP_LOGI(TAG, "panel bring-up: full white pass");
  g_driver.reset_and_init(false);
  if (!g_driver.full_refresh(g_next)) {
    ESP_LOGE(TAG, "initial white full refresh failed");
  }

  ESP_LOGI(TAG, "LVGL init");
  lv_init();
  lv_tick_set_cb(tick_cb);

  g_disp = lv_display_create(epd::kWidth, epd::kHeight);
  lv_display_set_color_format(g_disp, LV_COLOR_FORMAT_I1);

  uint8_t *draw_buf = static_cast<uint8_t *>(
      heap_caps_malloc(kDrawBufBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
  if (draw_buf == nullptr) {
    ESP_LOGE(TAG, "LVGL draw buffer allocation failed");
    return;
  }
  lv_display_set_buffers(g_disp, draw_buf, nullptr, kDrawBufBytes,
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(g_disp, flush_cb);
  lv_display_set_default(g_disp);

  build_ui();

  ESP_LOGI(TAG, "first LVGL frame");
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(g_disp);

  {
    uint32_t hash = 2166136261u;
    size_t zero = 0;
    size_t ones = 0;
    for (size_t i = 0; i < epd::kFrameBytes; ++i) {
      const uint8_t b = g_next[i];
      hash = (hash ^ b) * 16777619u;
      if (b == 0x00) {
        ++zero;
      } else if (b == 0xFF) {
        ++ones;
      }
    }
    ESP_LOGI(TAG, "frame hash=%08x zero_bytes=%u ff_bytes=%u total=%u",
             (unsigned)hash, (unsigned)zero, (unsigned)ones,
             (unsigned)epd::kFrameBytes);
  }

  full_refresh_now("first frame");

  netwifi::start();

  uint32_t last_counter = 0;
  uint32_t last_tick = 0;
  uint32_t last_wifi = 0;
  uint32_t last_full = 0;
  int counter = 0;
  bool tick_white = true;

  for (;;) {
    lv_timer_handler();
    lv_refr_now(g_disp);

    if (g_dirty.x1 <= g_dirty.x2) {
      if (!send_dirty_area()) {
        ESP_LOGW(TAG, "partial failed; resyncing with a full refresh");
        full_refresh_now("partial timeout");
      }
    }

    const uint32_t now = tick_cb();

    if (now - last_counter >= kCounterPeriodMs) {
      last_counter = now;
      char text[16];
      std::snprintf(text, sizeof(text), "%04d", counter++);
      lv_label_set_text(g_counter, text);
    }

    if (now - last_tick >= kTickPeriodMs) {
      last_tick = now;
      tick_white = !tick_white;
      lv_obj_set_style_bg_color(g_tick,
                                tick_white ? lv_color_white() : lv_color_black(),
                                0);
    }

    if (now - last_wifi >= kWifiPollMs) {
      last_wifi = now;
      refresh_wifi_label();
    }

    if (now - last_full >= kFullRefreshPeriodMs) {
      last_full = now;
      full_refresh_now("periodic ghost cleanup");
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
