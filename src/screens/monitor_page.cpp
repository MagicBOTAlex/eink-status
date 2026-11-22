#include "screens/monitor_page.hpp"

#include <cstdio>

#include "ui/theme.hpp"

namespace app {
namespace {

// 480x648 logical layout (the panel is rotated 90 degrees on the way out).
// Seven 68 px rows fit between the two rules.
constexpr int kMargin = 12;
constexpr int kHeaderY = 8;
constexpr int kRuleTop = 40;
constexpr int kRowTop = 48;
constexpr int kRowHeight = 68;
constexpr int kNameW = 160;
constexpr int kLatencyX = 176;
constexpr int kLatencyW = 94;
constexpr int kStatusX = 278;
constexpr int kStatusW = 86;
constexpr int kStatusH = 44;
constexpr int kUptimeX = 372;
constexpr int kUptimeW = 96;
constexpr int kRuleBottom = 528;
constexpr int kFooter1Y = 536;
constexpr int kFooter2Y = 568;

ui::ParamSet text_params(const char *text, const ui::Font &font, int x, int y,
                         int w, int h) {
  return {
      {ui::Label::kText, ui::Param(text)},
      {ui::Label::kFont, ui::Param(&font)},
      {ui::Label::kColor, ui::Param(ui::Color::Black)},
      {ui::Component::kX, ui::Param(x)},
      {ui::Component::kY, ui::Param(y)},
      {ui::Component::kW, ui::Param(w)},
      {ui::Component::kH, ui::Param(h)},
      {ui::Label::kVAlign, ui::Param(ui::VAlign::Middle)},
  };
}

} // namespace

MonitorPage::MonitorPage(SnapshotSource source)
    : Page("monitor", {{ui::Container::kBg, ui::Param(ui::Color::White)}}),
      source_(std::move(source)) {
  {
    ui::ParamSet params =
        text_params("net monitor", theme::label_font(), kMargin, kHeaderY, 240,
                    30);
    add<ui::Label>("title", params);
  }
  {
    ui::ParamSet params = text_params("starting", theme::label_font(), 0,
                                      kHeaderY, 200, 30);
    params.set(ui::Component::kAnchor, ui::Param(ui::Align::TopRight));
    params.set(ui::Component::kX, ui::Param(-kMargin));
    params.set(ui::Label::kHAlign, ui::Param(ui::HAlign::Right));
    summary_ = &add<ui::Label>("summary", params);
  }
  {
    ui::ParamSet params{
        {ui::Component::kX, ui::Param(kMargin)},
        {ui::Component::kY, ui::Param(kRuleTop)},
        {ui::Component::kW, ui::Param(theme::kWidth - 2 * kMargin)},
    };
    add<ui::Rule>("rule-top", params);
  }

  for (int i = 0; i < monitor::kMaxTargets; ++i) {
    const int y = kRowTop + i * kRowHeight;
    Row &row = rows_[i];

    {
      ui::ParamSet params =
          text_params("-", theme::row_font(), kMargin, y, kNameW, kRowHeight);
      row.name = &add<ui::Label>("name", params);
    }
    {
      ui::ParamSet params =
          text_params("--", theme::row_font(), kLatencyX, y, kLatencyW,
                      kRowHeight);
      params.set(ui::Label::kHAlign, ui::Param(ui::HAlign::Right));
      row.latency = &add<ui::Label>("latency", params);
    }
    {
      // The box is added before the label so it paints behind the text.
      ui::ParamSet params{
          {ui::Component::kX, ui::Param(kStatusX)},
          {ui::Component::kY, ui::Param(y + (kRowHeight - kStatusH) / 2)},
          {ui::Component::kW, ui::Param(kStatusW)},
          {ui::Component::kH, ui::Param(kStatusH)},
          {ui::Box::kColor, ui::Param(ui::Color::White)},
      };
      row.box = &add<ui::Box>("status-box", params);
    }
    {
      // A vector icon, not "UP"/"DOWN" text: 32 px glyph stems are too thin
      // to read reliably on this panel.
      ui::ParamSet params{
          {ui::Component::kX, ui::Param(kStatusX)},
          {ui::Component::kY, ui::Param(y + (kRowHeight - kStatusH) / 2)},
          {ui::Component::kW, ui::Param(kStatusW)},
          {ui::Component::kH, ui::Param(kStatusH)},
          {ui::Icon::kIcon, ui::Param(ui::IconKind::Dash)},
          {ui::Icon::kColor, ui::Param(ui::Color::Black)},
      };
      row.status = &add<ui::Icon>("status", params);
    }
    {
      ui::ParamSet params =
          text_params("--", theme::row_font(), kUptimeX, y, kUptimeW,
                      kRowHeight);
      params.set(ui::Label::kHAlign, ui::Param(ui::HAlign::Right));
      row.uptime = &add<ui::Label>("uptime", params);
    }
  }

  {
    ui::ParamSet params{
        {ui::Component::kX, ui::Param(kMargin)},
        {ui::Component::kY, ui::Param(kRuleBottom)},
        {ui::Component::kW, ui::Param(theme::kWidth - 2 * kMargin)},
    };
    add<ui::Rule>("rule-bottom", params);
  }

  {
    ui::ParamSet params = text_params("power --", theme::label_font(), kMargin,
                                      kFooter1Y, 320, 30);
    power_ = &add<ui::Label>("power", params);
  }
  {
    ui::ParamSet params =
        text_params("no ip", theme::label_font(), 0, kFooter1Y, 170, 30);
    params.set(ui::Component::kAnchor, ui::Param(ui::Align::TopRight));
    params.set(ui::Component::kX, ui::Param(-kMargin));
    params.set(ui::Label::kHAlign, ui::Param(ui::HAlign::Right));
    ip_ = &add<ui::Label>("ip", params);
  }
  {
    ui::ParamSet params = text_params("waiting for first round",
                                      theme::label_font(), kMargin, kFooter2Y,
                                      320, 30);
    checked_ = &add<ui::Label>("checked", params);
  }
  {
    ui::ParamSet params =
        text_params("probe 30s", theme::label_font(), 0, kFooter2Y, 160, 30);
    params.set(ui::Component::kAnchor, ui::Param(ui::Align::TopRight));
    params.set(ui::Component::kX, ui::Param(-kMargin));
    params.set(ui::Label::kHAlign, ui::Param(ui::HAlign::Right));
    interval_ = &add<ui::Label>("interval", params);
  }
}

ui::Label &MonitorPage::name_label(int index) { return *rows_[index].name; }
ui::Label &MonitorPage::latency_label(int index) {
  return *rows_[index].latency;
}
ui::Icon &MonitorPage::status_icon(int index) {
  return *rows_[index].status;
}
ui::Label &MonitorPage::uptime_label(int index) {
  return *rows_[index].uptime;
}
ui::Box &MonitorPage::status_box(int index) { return *rows_[index].box; }

void MonitorPage::update_self(uint32_t now_ms) {
  // The prober publishes every few seconds; repainting faster than this only
  // burns CPU and e-ink refreshes.
  if (updated_once_ && now_ms - last_update_ms_ < 200) {
    return;
  }
  last_update_ms_ = now_ms;
  updated_once_ = true;

  const monitor::Snapshot snap = source_ ? source_() : monitor::Snapshot{};

  for (int i = 0; i < monitor::kMaxTargets; ++i) {
    Row &row = rows_[i];
    const bool active = i < snap.count;
    row.name->set_visible(active);
    row.latency->set_visible(active);
    row.box->set_visible(active);
    row.status->set_visible(active);
    row.uptime->set_visible(active);
    if (!active) {
      continue;
    }

    const monitor::TargetStatus &status = snap.targets[i];
    row.name->set_text(status.label);
    row.latency->set_text(monitor::format_latency(status.latency_ms));
    row.uptime->set_text(status.known
                             ? monitor::format_since(now_ms, status.since_ms)
                             : "--");

    const bool down = status.known && !status.up;
    row.status->set_icon(!status.known
                             ? ui::IconKind::Dash
                             : (status.up ? ui::IconKind::Check
                                          : ui::IconKind::Cross));
    row.box->set_color(down ? ui::Color::Black : ui::Color::White);
    row.status->set(ui::Icon::kColor,
                    ui::Param(down ? ui::Color::White : ui::Color::Black));
  }

  char text[96];
  if (snap.checked_ms == 0) {
    std::snprintf(text, sizeof(text), "starting");
  } else if (!snap.wifi_connected) {
    std::snprintf(text, sizeof(text), "OFFLINE");
  } else {
    std::snprintf(text, sizeof(text), "%d/%d up", snap.up_count,
                  snap.known_count);
  }
  summary_->set_text(text);

  char power[96];
  const std::string net =
      snap.wifi_connected && snap.connected_since_ms != 0
          ? monitor::format_uptime(
                static_cast<uint32_t>(now_ms - snap.connected_since_ms))
          : std::string("down");
  std::snprintf(power, sizeof(power), "power %s   net %s",
                monitor::format_uptime(now_ms).c_str(), net.c_str());
  power_->set_text(power);

  ip_->set_text(snap.ip[0] != '\0' ? snap.ip : "no ip");

  if (snap.checked_ms == 0) {
    checked_->set_text("waiting for first round");
  } else {
    checked_->set_text("checked " + monitor::format_ago(now_ms - snap.checked_ms));
  }

  char interval[32];
  std::snprintf(interval, sizeof(interval), "probe %ds", snap.probe_seconds);
  interval_->set_text(interval);
}

} // namespace app
