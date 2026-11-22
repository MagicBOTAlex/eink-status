#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "control/commands.hpp"
#include "epd.hpp"
#include "monitor/prober.hpp"
#include "panel_plan.hpp"
#include "screens/monitor_page.hpp"
#include "screens/status_page.hpp"
#include "ui/app.hpp"
#include "ui/canvas.hpp"
#include "ui/rotation.hpp"
#include "ui/theme.hpp"
#include "web/server.hpp"
#include "wifi.hpp"

namespace {

constexpr const char *TAG = "MAIN";

constexpr uint32_t kLoopPeriodMs = 20;
// The monitor repaints small regions often; a full flash every 15 minutes is
// enough to keep the panel free of ghosting.
constexpr uint32_t kFullRefreshPeriodMs = 15 * 60 * 1000;
constexpr uint32_t kFullRetryMs = 5000;
constexpr size_t kMaxPartialRegions = 4;
// A repaint this large is cheaper as one full-screen refresh, and the full
// waveform gives better contrast.
constexpr long kFullRefreshArea = (long)epd::kWidth * epd::kHeight / 2;

// The panel is mounted sideways: lay the UI out in 480x648 and rotate it into
// the panel's native 648x480 frame. Flip to Ccw90 if the image comes out
// upside down.
constexpr ui::Rotation kPanelRotation = ui::Rotation::Cw90;

constexpr int kLogicalStride = (app::theme::kWidth + 7) / 8;

epd::Driver g_driver;
ui::UiApp g_app(app::theme::kWidth, app::theme::kHeight);
ui::Canvas *g_logical = nullptr;
ui::Canvas *g_panel = nullptr;

uint32_t now_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

app::WifiInfo read_wifi() {
  const netwifi::Status status = netwifi::get_status();
  app::WifiInfo info;
  info.connected = status.connected;
  info.ssid = status.ssid;
  info.ip = status.ip;
  return info;
}

// Rotates the freshly drawn logical frame into the panel buffer.
void sync_panel() {
  if (g_logical != nullptr && g_panel != nullptr) {
    ui::rotate_canvas(*g_logical, *g_panel, kPanelRotation);
  }
}

// Returns false when the panel did not complete the refresh; the caller then
// retries instead of pretending the glass is up to date.
bool full_refresh(const char *why) {
  ui::Page *page = g_app.current_page();
  ESP_LOGI(TAG, "full refresh: %s", why);

  if (page != nullptr) {
    page->on_display_mode(ui::DisplayMode::Full);
  }
  // Make sure the frame buffer contains what the page looks like right now.
  g_app.present(*g_logical);
  sync_panel();

  const bool ok = g_driver.full_refresh(g_panel->pixels());
  if (page != nullptr) {
    page->on_display_mode(ok ? ui::DisplayMode::Partial : ui::DisplayMode::Error);
  }
  if (!ok) {
    ESP_LOGE(TAG, "full refresh failed");
  }
  return ok;
}

void log_dirty(const std::vector<ui::Rect> &regions) {
  if (regions.empty()) {
    return;
  }
  std::string line = "dirty:";
  for (const ui::Rect &r : regions) {
    line += " " + r.to_string();
  }
  ESP_LOGI(TAG, "%s", line.c_str());
}

bool partial_refresh(const std::vector<ui::Rect> &regions) {
  const ui::Size logical{app::theme::kWidth, app::theme::kHeight};

  for (const ui::Rect &rect : regions) {
    const ui::Rect panel_rect = ui::rotated_rect(rect, logical, kPanelRotation);
    const panel::Window window = panel::to_window(panel_rect);
    if (!panel::valid(window)) {
      ESP_LOGE(TAG, "refusing invalid window for %s",
               panel_rect.to_string().c_str());
      return false;
    }
    const epd::Rect area{window.x1, window.y1, window.x2, window.y2};
    if (!g_driver.partial_update(g_panel->pixels(), area)) {
      return false;
    }
  }
  return true;
}

} // namespace

extern "C" void app_main() {
  ESP_LOGI(TAG, "boot");
  ESP_LOGI(TAG, "heap: internal free=%u psram free=%u",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

  uint8_t *logical_bytes = static_cast<uint8_t *>(heap_caps_calloc(
      1, static_cast<size_t>(kLogicalStride) * app::theme::kHeight,
      MALLOC_CAP_SPIRAM));
  uint8_t *panel_bytes = static_cast<uint8_t *>(
      heap_caps_calloc(1, epd::kFrameBytes, MALLOC_CAP_SPIRAM));
  if (logical_bytes == nullptr || panel_bytes == nullptr) {
    ESP_LOGE(TAG, "frame buffer allocation failed");
    return;
  }

  ui::Canvas logical_canvas(logical_bytes, app::theme::kWidth,
                            app::theme::kHeight, kLogicalStride);
  ui::Canvas panel_canvas(panel_bytes, epd::kWidth, epd::kHeight, epd::kStride);
  logical_canvas.clear(ui::Color::White);
  panel_canvas.clear(ui::Color::White);
  g_logical = &logical_canvas;
  g_panel = &panel_canvas;

  // The monitor is the default screen; the old status page stays registered
  // so page switching has somewhere to go once there is an input device.
  g_app.add_page(std::make_unique<app::MonitorPage>(monitor::snapshot));
  g_app.add_page(std::make_unique<app::StatusPage>(read_wifi));
  g_app.show_page("monitor");

  ESP_LOGI(TAG, "ui tree:\n%s", g_app.describe().c_str());
  for (const auto &issue : g_app.validation_errors()) {
    ESP_LOGE(TAG, "ui: %s: %s: %s", issue.component.c_str(),
             issue.error.key.c_str(), issue.error.message.c_str());
  }

  if (!g_driver.begin()) {
    ESP_LOGE(TAG, "EPD driver init failed");
    return;
  }

  ESP_LOGI(TAG, "panel bring-up: PWR high + full init + white clear");
  g_driver.power_output(true);
  vTaskDelay(pdMS_TO_TICKS(300));
  if (!g_driver.reset_and_init(false)) {
    ESP_LOGE(TAG, "panel init timed out; will retry on the first refresh");
  }
  if (!g_driver.clear(0xFF)) {
    ESP_LOGE(TAG, "white clear did not complete");
  }

  netwifi::start();
  monitor::start();
  web::start();

  bool need_full = true;
  uint32_t next_full_attempt = 0; // immediate
  uint32_t last_full = now_ms();
  uint32_t presented = 0;

  for (;;) {
    const uint32_t now = now_ms();

    const control::Command command = control::queue().take();
    if (command.action != control::Action::None) {
      switch (command.action) {
      case control::Action::Refresh:
        need_full = true;
        next_full_attempt = 0;
        break;
      case control::Action::ProbeNow:
        monitor::request_probe();
        break;
      case control::Action::Reboot:
        ESP_LOGW(TAG, "reboot requested from the web UI");
        esp_restart();
        break;
      case control::Action::ShowPage:
        if (!g_app.show_page(command.page)) {
          ESP_LOGW(TAG, "no such page: %s", command.page.c_str());
        }
        break;
      case control::Action::None:
        break;
      }
    }

    g_app.tick(now);

    if (!g_app.needs_present() && !need_full) {
      vTaskDelay(pdMS_TO_TICKS(kLoopPeriodMs));
      continue;
    }

    const bool page_switch = g_app.page_changed();
    const std::vector<ui::Rect> regions = g_app.present(*g_logical);
    sync_panel();

    if (need_full) {
      if (static_cast<int32_t>(now - next_full_attempt) >= 0) {
        if (full_refresh(page_switch ? "page switch" : "repaint")) {
          need_full = false;
          last_full = now;
        } else {
          next_full_attempt = now + kFullRetryMs;
        }
      }
    } else if (!regions.empty()) {
      const panel::Action action =
          panel::choose(regions, kMaxPartialRegions, kFullRefreshArea);

      if (action == panel::Action::Full) {
        ESP_LOGI(TAG, "dirty area too large for partial refresh");
        need_full = true;
        next_full_attempt = 0;
      } else if (action == panel::Action::Partial) {
        log_dirty(regions);
        if (!partial_refresh(regions)) {
          ESP_LOGW(TAG, "partial refresh failed; scheduling a full refresh");
          need_full = true;
          next_full_attempt = now + kFullRetryMs;
        }
      }
    }

    if (!need_full && panel::due(now, last_full, kFullRefreshPeriodMs)) {
      need_full = true;
      next_full_attempt = 0;
      ESP_LOGI(TAG, "periodic ghost cleanup due");
    }

    if ((++presented % 250) == 0) {
      ESP_LOGI(TAG, "heap: internal free=%u",
               (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    }

    vTaskDelay(pdMS_TO_TICKS(kLoopPeriodMs));
  }
}
