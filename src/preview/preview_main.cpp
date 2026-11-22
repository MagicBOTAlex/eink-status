// Host preview: renders the real pages with the real component tree, the real
// layout code and the real font bitmaps, then writes PGM snapshots and an
// ASCII view. No panel and no webcam.
//
// Build/run with `tools/preview.sh`, or:
//   pio run -e preview && .pio/build/preview/program

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "monitor/model.hpp"
#include "screens/monitor_page.hpp"
#include "screens/status_page.hpp"
#include "ui/app.hpp"
#include "ui/canvas.hpp"
#include "ui/rotation.hpp"
#include "ui/theme.hpp"

namespace {

constexpr int kWidth = app::theme::kWidth;
constexpr int kHeight = app::theme::kHeight;
constexpr int kStride = (kWidth + 7) / 8;

bool g_connected = false;

app::WifiInfo wifi() {
  app::WifiInfo info;
  info.connected = g_connected;
  if (g_connected) {
    info.ssid = "boostiest booster";
    info.ip = "192.168.50.7";
  }
  return info;
}

monitor::Snapshot bench_snapshot(bool down) {
  monitor::Snapshot snap;
  snap.count = monitor::kMaxTargets;
  snap.probe_seconds = 30;
  snap.wifi_connected = true;
  snap.connected_since_ms = 100000;
  std::snprintf(snap.ip, sizeof(snap.ip), "192.168.50.7");
  snap.checked_ms = 3600000;
  snap.known_count = monitor::kMaxTargets;
  snap.up_count = down ? monitor::kMaxTargets - 1 : monitor::kMaxTargets;

  const struct {
    const char *label;
    monitor::Kind kind;
    int latency;
    uint32_t since;
  } rows[] = {
      {"webui", monitor::Kind::Http, 128, 120000},
      {"akupunktur", monitor::Kind::Http, 212, 900000},
      {"ha", monitor::Kind::Http, 6, 3600000},
      {"deprived", monitor::Kind::Http, 104, 1800000},
      {"botserver", monitor::Kind::Ping, 2, 3600000},
      {"metal", monitor::Kind::Ping, 1, 3600000},
      {"botmain", monitor::Kind::Ping, 1, 3600000},
  };

  for (int i = 0; i < snap.count; ++i) {
    monitor::TargetStatus &target = snap.targets[i];
    target.label = rows[i].label;
    target.kind = rows[i].kind;
    target.known = true;
    target.up = !(down && i == 1);
    target.latency_ms = target.up ? rows[i].latency : -1;
    target.since_ms = rows[i].since;
  }
  if (down) {
    snap.targets[1].since_ms = 3000000;
  }
  return snap;
}

bool write_pgm(const std::string &path, const ui::Canvas &canvas) {
  FILE *f = std::fopen(path.c_str(), "wb");
  if (f == nullptr) {
    std::perror(path.c_str());
    return false;
  }
  bool ok = std::fprintf(f, "P5\n%d %d\n255\n", canvas.width(),
                         canvas.height()) > 0;
  for (int y = 0; ok && y < canvas.height(); ++y) {
    for (int x = 0; ok && x < canvas.width(); ++x) {
      ok = std::fputc(canvas.test(x, y) ? 0 : 255, f) != EOF;
    }
  }
  ok = (std::fclose(f) == 0) && ok;
  if (!ok) {
    std::perror(path.c_str());
    return false;
  }
  std::printf("wrote %s\n", path.c_str());
  return true;
}

void snapshot(ui::UiApp &app, ui::Canvas &canvas, const char *name,
              bool ascii = true) {
  app.present(canvas);
  const std::string path = std::string("preview-") + name + ".pgm";
  write_pgm(path, canvas);
  std::printf("--- %s (hash %08x) ---\n", name, (unsigned)canvas.hash());
  if (ascii) {
    std::printf("%s", canvas.to_ascii(canvas.bounds(), 6).c_str());
  }
}

} // namespace

int main() {
  uint8_t buffer[static_cast<size_t>(kStride) * kHeight];
  std::memset(buffer, 0xFF, sizeof(buffer));
  ui::Canvas canvas(buffer, kWidth, kHeight, kStride);

  // The panel is 648x480 and mounted sideways; preview the rotated frame too.
  constexpr int kPanelW = 648;
  constexpr int kPanelH = 480;
  constexpr int kPanelStride = (kPanelW + 7) / 8;
  uint8_t panel_buffer[static_cast<size_t>(kPanelStride) * kPanelH];
  std::memset(panel_buffer, 0xFF, sizeof(panel_buffer));
  ui::Canvas panel(panel_buffer, kPanelW, kPanelH, kPanelStride);
  panel.clear(ui::Color::White);

  ui::UiApp app(kWidth, kHeight);
  app.add_page(std::make_unique<app::StatusPage>(wifi));

  monitor::Snapshot snap = bench_snapshot(false);
  app.add_page(std::make_unique<app::MonitorPage>([&snap] { return snap; }));

  std::printf("%s", app.describe().c_str());

  // Status page (the earlier demo screen).
  app.show_page("status");
  g_connected = true;
  app.tick(10000);
  snapshot(app, canvas, "status", false);

  // Monitor page: all up.
  app.show_page("monitor");
  app.tick(3720000);
  snapshot(app, canvas, "monitor", true);

  // One target down and one probe later.
  snap = bench_snapshot(true);
  snap.checked_ms = 3750000;
  app.tick(3750000);
  snapshot(app, canvas, "monitor-down", true);

  // What the panel actually receives: the logical frame rotated 90 degrees.
  if (ui::rotate_canvas(canvas, panel, ui::Rotation::Cw90)) {
    write_pgm("preview-panel.pgm", panel);
  }

  return 0;
}
