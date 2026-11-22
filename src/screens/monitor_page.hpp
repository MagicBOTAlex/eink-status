#pragma once

#include <cstdint>
#include <functional>

#include "monitor/model.hpp"
#include "ui/components.hpp"
#include "ui/page.hpp"

namespace app {

// Network monitor: one row per configured target (status, latency, streak) and
// a footer with this device's own power/network uptime.
class MonitorPage : public ui::Page {
public:
  // Injected so the page never touches the network or NVS directly. The device
  // passes monitor::snapshot(); tests pass a stub.
  using SnapshotSource = std::function<monitor::Snapshot()>;

  explicit MonitorPage(SnapshotSource source);

  ui::Label &name_label(int index);
  ui::Label &latency_label(int index);
  ui::Icon &status_icon(int index);
  ui::Label &uptime_label(int index);
  ui::Box &status_box(int index);
  ui::Label &summary_label() { return *summary_; }
  ui::Label &power_label() { return *power_; }
  ui::Label &ip_label() { return *ip_; }
  ui::Label &checked_label() { return *checked_; }
  ui::Label &interval_label() { return *interval_; }

protected:
  void update_self(uint32_t now_ms) override;

private:
  struct Row {
    ui::Label *name = nullptr;
    ui::Label *latency = nullptr;
    ui::Box *box = nullptr;
    ui::Icon *status = nullptr;
    ui::Label *uptime = nullptr;
  };

  SnapshotSource source_;
  Row rows_[monitor::kMaxTargets];
  ui::Label *summary_ = nullptr;
  ui::Label *power_ = nullptr;
  ui::Label *ip_ = nullptr;
  ui::Label *checked_ = nullptr;
  ui::Label *interval_ = nullptr;

  uint32_t last_update_ms_ = 0;
  bool updated_once_ = false;
};

} // namespace app
