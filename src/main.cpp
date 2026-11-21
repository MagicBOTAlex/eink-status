#include <cstdio>
#include <memory>
#include <vector>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "epd.hpp"
#include "screens/status_page.hpp"
#include "ui/app.hpp"
#include "ui/canvas.hpp"
#include "ui/theme.hpp"
#include "wifi.hpp"

namespace {

constexpr const char *TAG = "MAIN";

constexpr uint32_t kLoopPeriodMs = 20;
constexpr uint32_t kFullRefreshPeriodMs = 120000;
// A region this large is cheaper to paint with a full-screen refresh, and the
// full waveform gives the best contrast for a repaint of most of the screen.
constexpr long kFullRefreshArea = (long)epd::kWidth * epd::kHeight / 2;
constexpr size_t kMaxPartialRegions = 4;

epd::Driver g_driver;
ui::UiApp g_app(app::theme::kWidth, app::theme::kHeight);
uint8_t *g_frame = nullptr;

uint32_t now_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

app::WifiInfo read_wifi() {
  const netwifi::Status status = netwifi::get_status();
  app::WifiInfo info;
  info.connected = status.connected;
  info.ssid = status.ssid;
  info.ip = status.ip;
  return info;
}

void log_regions(const std::vector<ui::Rect> &regions) {
  if (regions.empty()) {
    return;
  }
  std::string line = "dirty:";
  for (const ui::Rect &r : regions) {
    line += " " + r.to_string();
  }
  ESP_LOGI(TAG, "%s", line.c_str());
}

bool full_refresh(const char *why, ui::Canvas &canvas, app::StatusPage &page) {
  ESP_LOGI(TAG, "full refresh: %s", why);
  page.set_mode("full");
  g_app.present(canvas);
  const bool ok = g_driver.full_refresh(g_frame);
  page.set_mode("partial");
  return ok;
}

} // namespace

extern "C" void app_main() {
  ESP_LOGI(TAG, "boot");
  ESP_LOGI(TAG, "heap: internal free=%u psram free=%u",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

  g_frame = static_cast<uint8_t *>(
      heap_caps_calloc(1, epd::kFrameBytes, MALLOC_CAP_SPIRAM));
  if (g_frame == nullptr) {
    ESP_LOGE(TAG, "frame buffer allocation failed");
    return;
  }
  ui::Canvas canvas(g_frame, epd::kWidth, epd::kHeight, epd::kStride);
  canvas.clear(ui::Color::White);

  g_app.add_page(std::make_unique<app::StatusPage>(read_wifi));
  auto *page = static_cast<app::StatusPage *>(g_app.current_page());
  ESP_LOGI(TAG, "ui tree:\n%s", g_app.describe().c_str());

  if (!g_driver.begin()) {
    ESP_LOGE(TAG, "EPD driver init failed");
    return;
  }

  ESP_LOGI(TAG, "panel bring-up: PWR high + full init + white clear");
  g_driver.power_output(true);
  vTaskDelay(pdMS_TO_TICKS(300));
  g_driver.reset_and_init(false);
  if (!g_driver.clear(0xFF)) {
    ESP_LOGE(TAG, "white clear did not complete");
  }

  g_app.tick(now_ms());
  g_app.present(canvas);
  ESP_LOGI(TAG, "frame hash=%08x", (unsigned)canvas.hash());

  full_refresh("first frame", canvas, *page);
  netwifi::start();

  uint32_t last_full = now_ms();
  size_t presented = 0;

  for (;;) {
    const uint32_t now = now_ms();
    g_app.tick(now);

    const bool page_switch = g_app.page_changed();
    const std::vector<ui::Rect> regions = g_app.present(canvas);

    if (page_switch) {
      full_refresh("page switch", canvas, *page);
      last_full = now;
    } else if (!regions.empty()) {
      long area = 0;
      for (const ui::Rect &r : regions) {
        area += r.area();
      }

      if (regions.size() > kMaxPartialRegions || area > kFullRefreshArea) {
        full_refresh("large dirty area", canvas, *page);
        last_full = now;
      } else {
        log_regions(regions);
        for (const ui::Rect &r : regions) {
          const epd::Rect area{r.x, r.y, r.w, r.h};
          if (!g_driver.partial_update(g_frame, area)) {
            ESP_LOGW(TAG, "partial failed, resyncing with a full refresh");
            full_refresh("partial timeout", canvas, *page);
            last_full = now;
            break;
          }
        }
      }
    }

    if (now - last_full >= kFullRefreshPeriodMs) {
      last_full = now;
      full_refresh("periodic ghost cleanup", canvas, *page);
    }

    if ((++presented % 250) == 0) {
      ESP_LOGI(TAG, "heap: internal free=%u",
               (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    }

    vTaskDelay(pdMS_TO_TICKS(kLoopPeriodMs));
  }
}
