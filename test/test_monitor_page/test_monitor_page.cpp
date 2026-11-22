#include <unity.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "monitor/model.hpp"
#include "screens/monitor_page.hpp"
#include "ui/app.hpp"
#include "ui/components.hpp"
#include "ui/theme.hpp"

void setUp(void) {}
void tearDown(void) {}

namespace {

constexpr int kW = app::theme::kWidth;
constexpr int kH = app::theme::kHeight;
constexpr int kStride = (kW + 7) / 8;

struct Harness {
  std::vector<uint8_t> bytes =
      std::vector<uint8_t>(static_cast<size_t>(kStride) * kH, 0xFF);
  ui::Canvas canvas{bytes.data(), kW, kH, kStride};
  ui::UiApp app{kW, kH};
  monitor::Snapshot snap;
  app::MonitorPage *page = nullptr;

  Harness() {
    canvas.clear(ui::Color::White);
    auto &added =
        app.add_page(std::make_unique<app::MonitorPage>([this] { return snap; }));
    page = static_cast<app::MonitorPage *>(&added);
  }

  // A realistic snapshot: one up, one down, one unknown.
  void fill() {
    snap = monitor::Snapshot{};
    snap.count = 3;
    snap.probe_seconds = 30;
    snap.wifi_connected = true;
    snap.connected_since_ms = 100000;
    std::snprintf(snap.ip, sizeof(snap.ip), "192.168.50.7");
    snap.checked_ms = 195000;
    snap.known_count = 2;
    snap.up_count = 1;

    snap.targets[0].label = "webui";
    snap.targets[0].kind = monitor::Kind::Http;
    snap.targets[0].known = true;
    snap.targets[0].up = true;
    snap.targets[0].latency_ms = 121;
    snap.targets[0].since_ms = 95000; // 105 s up

    snap.targets[1].label = "metal";
    snap.targets[1].kind = monitor::Kind::Ping;
    snap.targets[1].known = true;
    snap.targets[1].up = false;
    snap.targets[1].latency_ms = -1;
    snap.targets[1].since_ms = 180000;

    snap.targets[2].label = "ha";
    snap.targets[2].kind = monitor::Kind::Http;
  }
};

} // namespace

void test_rows_follow_the_snapshot_count(void) {
  Harness h;
  h.fill();
  h.app.tick(200000);
  h.app.present(h.canvas);

  TEST_ASSERT_TRUE(h.page->name_label(0).visible());
  TEST_ASSERT_TRUE(h.page->name_label(2).visible());
  TEST_ASSERT_FALSE(h.page->name_label(3).visible());
  TEST_ASSERT_FALSE(h.page->name_label(monitor::kMaxTargets - 1).visible());
}

void test_row_text_and_state(void) {
  Harness h;
  h.fill();
  h.app.tick(200000);
  h.app.present(h.canvas);

  TEST_ASSERT_EQUAL_STRING("webui", h.page->name_label(0).text().c_str());
  TEST_ASSERT_EQUAL_STRING("121ms", h.page->latency_label(0).text().c_str());
  TEST_ASSERT_EQUAL_STRING("--", h.page->uptime_label(2).text().c_str());

  // Status is an icon now: check / cross / dash, not text.
  TEST_ASSERT_EQUAL_INT((int)ui::IconKind::Check,
                        (int)h.page->status_icon(0).kind());
  TEST_ASSERT_EQUAL_INT((int)ui::IconKind::Cross,
                        (int)h.page->status_icon(1).kind());
  TEST_ASSERT_EQUAL_INT((int)ui::IconKind::Dash,
                        (int)h.page->status_icon(2).kind());

  // Down rows invert their status cell.
  TEST_ASSERT_EQUAL_INT(
      (int)ui::Color::Black,
      (int)h.page->status_box(1).params().get_color(ui::Box::kColor));
  TEST_ASSERT_EQUAL_INT(
      (int)ui::Color::White,
      (int)h.page->status_icon(1).params().get_color(ui::Icon::kColor));
  TEST_ASSERT_EQUAL_INT(
      (int)ui::Color::White,
      (int)h.page->status_box(0).params().get_color(ui::Box::kColor));
  TEST_ASSERT_EQUAL_INT(
      (int)ui::Color::Black,
      (int)h.page->status_icon(0).params().get_color(ui::Icon::kColor));
}

void test_header_and_footer(void) {
  Harness h;
  h.fill();
  h.app.tick(200000); // 3m20s since boot, 1m40s since the link came up
  h.app.present(h.canvas);

  TEST_ASSERT_EQUAL_STRING("1/2 up", h.page->summary_label().text().c_str());
  TEST_ASSERT_EQUAL_STRING("192.168.50.7", h.page->ip_label().text().c_str());
  TEST_ASSERT_EQUAL_STRING("probe 30s", h.page->interval_label().text().c_str());
  TEST_ASSERT_EQUAL_STRING("checked just now",
                           h.page->checked_label().text().c_str());
  TEST_ASSERT_EQUAL_STRING("power 0h03m   net 0h01m",
                           h.page->power_label().text().c_str());
}

void test_offline_and_starting_states(void) {
  Harness h;
  h.app.tick(1000);
  h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_STRING("starting", h.page->summary_label().text().c_str());
  TEST_ASSERT_EQUAL_STRING("waiting for first round",
                           h.page->checked_label().text().c_str());
  TEST_ASSERT_FALSE(h.page->name_label(0).visible());

  h.fill();
  h.snap.wifi_connected = false;
  h.app.tick(300000);
  h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_STRING("OFFLINE", h.page->summary_label().text().c_str());
  TEST_ASSERT_TRUE(strstr(h.page->power_label().text().c_str(), "net down") != nullptr);
}

void test_status_change_repaints_one_row(void) {
  Harness h;
  h.fill();
  h.app.tick(200000);
  h.app.present(h.canvas);

  // metal comes back up; only its row should be dirty.
  h.snap.targets[1].up = true;
  h.snap.targets[1].known = true;
  h.snap.targets[1].latency_ms = 4;
  h.snap.targets[1].since_ms = 300000;
  h.snap.up_count = 2;

  h.app.tick(200200); // 200 ms later: no uptime minute rolls over
  const std::vector<ui::Rect> dirty = h.app.present(h.canvas);

  // The header summary plus the row's changed cells (latency, status box,
  // status text, uptime), merged into one rect. Row 0 is untouched.
  TEST_ASSERT_EQUAL_size_t(2, dirty.size());
  TEST_ASSERT_TRUE(dirty[0] == (ui::Rect{268, 8, 200, 30}));
  TEST_ASSERT_TRUE(dirty[1] == (ui::Rect{186, 76, 282, 36}));
  TEST_ASSERT_EQUAL_STRING("2/2 up", h.page->summary_label().text().c_str());
}

void test_latency_update_repaints_only_its_row(void) {
  Harness h;
  h.fill();
  h.app.tick(200000);
  h.app.present(h.canvas);

  h.snap.targets[0].latency_ms = 99;
  h.app.tick(200400);
  const std::vector<ui::Rect> dirty = h.app.present(h.canvas);

  // Only the latency cell repaints: the pixel diff does the work, not the row.
  TEST_ASSERT_EQUAL_size_t(1, dirty.size());
  TEST_ASSERT_TRUE(dirty[0] == (ui::Rect{186, 40, 90, 36}));
}

void test_page_validates_cleanly(void) {
  Harness h;
  TEST_ASSERT_EQUAL_size_t(0, h.app.validation_errors().size());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_rows_follow_the_snapshot_count);
  RUN_TEST(test_row_text_and_state);
  RUN_TEST(test_header_and_footer);
  RUN_TEST(test_offline_and_starting_states);
  RUN_TEST(test_status_change_repaints_one_row);
  RUN_TEST(test_latency_update_repaints_only_its_row);
  RUN_TEST(test_page_validates_cleanly);
  return UNITY_END();
}
