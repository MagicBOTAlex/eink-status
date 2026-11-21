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

#include "screens/status_page.hpp"
#include "ui/app.hpp"
#include "ui/canvas.hpp"
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

  ui::UiApp app(kWidth, kHeight);
  app.add_page(std::make_unique<app::StatusPage>(wifi));
  auto *page = static_cast<app::StatusPage *>(app.current_page());

  std::printf("%s", app.describe().c_str());

  app.tick(0);
  snapshot(app, canvas, "t0");

  app.tick(2000);
  snapshot(app, canvas, "t2s", false);

  app.tick(9000);
  snapshot(app, canvas, "t9s", false);

  g_connected = true;
  app.tick(10000);
  snapshot(app, canvas, "wifi", false);

  page->set_mode("full");
  snapshot(app, canvas, "full", false);

  return 0;
}
