#include "screens/status_page.hpp"

#include <cstdio>

#include "ui/theme.hpp"

namespace app {
namespace {

constexpr uint32_t kCounterPeriodMs = 2000;
constexpr uint32_t kTickPeriodMs = 7000;

ui::ParamSet label_params(const char *text, const ui::Font &font) {
  return {
      {ui::Label::kText, ui::Param(text)},
      {ui::Label::kFont, ui::Param(&font)},
      {ui::Label::kColor, ui::Param(theme::kInk)},
  };
}

} // namespace

StatusPage::StatusPage(WifiSource source)
    : Page("status", {{ui::Container::kBg, ui::Param(theme::kPaper)}}),
      wifi_source_(std::move(source)) {
  // Header row.
  {
    ui::ParamSet params = label_params("eink-status", theme::small_font());
    params.set(ui::Component::kX, ui::Param(theme::kMargin));
    params.set(ui::Component::kY, ui::Param(12));
    add<ui::Label>("title", params);
  }
  {
    ui::ParamSet params = label_params("partial", theme::small_font());
    params.set(ui::Component::kAnchor, ui::Param(ui::Align::TopRight));
    params.set(ui::Component::kX, ui::Param(-theme::kMargin));
    params.set(ui::Component::kY, ui::Param(12));
    mode_ = &add<ui::Label>("mode", params);
  }

  // Headline.
  {
    ui::ParamSet params = label_params("Success", theme::headline_font());
    params.set(ui::Component::kAnchor, ui::Param(ui::Align::Center));
    params.set(ui::Component::kY, ui::Param(-30));
    add<ui::Label>("headline", params);
  }

  // Live counter.
  {
    ui::ParamSet params = label_params("0000", theme::small_font());
    params.set(ui::Component::kW, ui::Param(80));
    params.set(ui::Component::kH, ui::Param(20));
    params.set(ui::Component::kAnchor, ui::Param(ui::Align::Center));
    params.set(ui::Component::kY, ui::Param(45));
    params.set(ui::Label::kHAlign, ui::Param(ui::HAlign::Center));
    params.set(ui::Label::kVAlign, ui::Param(ui::VAlign::Middle));
    counter_ = &add<ui::Label>("counter", params);
  }

  // Panel subtitle.
  {
    ui::ParamSet params =
        label_params("GDEY0583T81 - 648x480", theme::small_font());
    params.set(ui::Component::kAnchor, ui::Param(ui::Align::Center));
    params.set(ui::Component::kY, ui::Param(80));
    add<ui::Label>("subtitle", params);
  }

  // WiFi state, bottom left.
  {
    ui::ParamSet params = label_params("wifi: starting", theme::small_font());
    params.set(ui::Component::kAnchor, ui::Param(ui::Align::BottomLeft));
    params.set(ui::Component::kX, ui::Param(theme::kMargin));
    params.set(ui::Component::kY, ui::Param(-12));
    wifi_ = &add<ui::Label>("wifi", params);
  }

  // Heartbeat square, bottom right.
  {
    ui::ParamSet params{
        {ui::Component::kW, ui::Param(36)},
        {ui::Component::kH, ui::Param(36)},
        {ui::Component::kAnchor, ui::Param(ui::Align::BottomRight)},
        {ui::Component::kX, ui::Param(-theme::kMargin)},
        {ui::Component::kY, ui::Param(-10)},
        {ui::Box::kColor, ui::Param(ui::Color::Black)},
    };
    tick_ = &add<ui::Box>("tick", params);
  }
}

void StatusPage::set_mode(const std::string &mode) { mode_->set_text(mode); }

void StatusPage::on_display_mode(ui::DisplayMode mode) {
  switch (mode) {
  case ui::DisplayMode::Full:
    set_mode("full");
    break;
  case ui::DisplayMode::Partial:
    set_mode("partial");
    break;
  case ui::DisplayMode::Error:
    set_mode("error");
    break;
  }
}

void StatusPage::update_self(uint32_t now_ms) {
  if (now_ms - last_counter_ms_ >= kCounterPeriodMs) {
    last_counter_ms_ = now_ms;
    ++counter_value_;
    char text[8];
    std::snprintf(text, sizeof(text), "%04d", counter_value_ % 10000);
    counter_->set_text(text);
  }

  if (now_ms - last_tick_ms_ >= kTickPeriodMs) {
    last_tick_ms_ = now_ms;
    tick_on_ = !tick_on_;
    tick_->set_color(tick_on_ ? ui::Color::Black : ui::Color::White);
  }

  // Poll the (mutex-protected) WiFi status at most once a second: the UI loop
  // runs at 50 Hz and nothing needs fresher state than that.
  if (wifi_source_ && (!wifi_polled_ || now_ms - last_wifi_ms_ >= 1000)) {
    wifi_polled_ = true;
    last_wifi_ms_ = now_ms;

    const WifiInfo info = wifi_source_();
    std::string text;
    if (info.connected) {
      text = "wifi: " + info.ssid + " (" + info.ip + ")";
    } else {
      text = "wifi: connecting...";
    }
    if (text != last_wifi_text_) {
      last_wifi_text_ = text;
      wifi_->set_text(text);
    }
  }
}

} // namespace app
