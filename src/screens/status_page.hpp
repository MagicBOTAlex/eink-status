#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "ui/components.hpp"
#include "ui/page.hpp"

namespace app {

struct WifiInfo {
  bool connected = false;
  std::string ssid;
  std::string ip;
};

// The one screen this firmware has so far: a headline, a live counter and the
// WiFi state. Everything is expressed through component parameters, so the
// page is identical on the panel, in the host tests and in the preview tool.
class StatusPage : public ui::Page {
public:
  // Injected so the page never talks to the network stack directly. Tests pass
  // a stub; the device passes netwifi::get_status().
  using WifiSource = std::function<WifiInfo()>;

  explicit StatusPage(WifiSource source = {});

  // "full" while a full-screen refresh is running, "partial" otherwise.
  void set_mode(const std::string &mode);

  // Panel state from the driver, via the framework hook.
  void on_display_mode(ui::DisplayMode mode) override;

  ui::Label &counter_label() { return *counter_; }
  ui::Label &wifi_label() { return *wifi_; }
  ui::Label &mode_label() { return *mode_; }
  ui::Box &tick_box() { return *tick_; }

protected:
  void update_self(uint32_t now_ms) override;

private:
  WifiSource wifi_source_;
  ui::Label *counter_ = nullptr;
  ui::Label *wifi_ = nullptr;
  ui::Label *mode_ = nullptr;
  ui::Box *tick_ = nullptr;

  uint32_t last_counter_ms_ = 0;
  uint32_t last_tick_ms_ = 0;
  uint32_t last_wifi_ms_ = 0;
  bool wifi_polled_ = false;
  int counter_value_ = 0;
  bool tick_on_ = true;
  std::string last_wifi_text_;
};

} // namespace app
