#include <unity.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "screens/status_page.hpp"
#include "ui/app.hpp"
#include "ui/theme.hpp"

void setUp(void) {}
void tearDown(void) {}

namespace {

constexpr int kW = app::theme::kWidth;
constexpr int kH = app::theme::kHeight;
constexpr int kStride = (kW + 7) / 8;

app::WifiInfo g_wifi;

app::WifiInfo wifi_source() { return g_wifi; }

struct Harness {
  std::vector<uint8_t> bytes =
      std::vector<uint8_t>(static_cast<size_t>(kStride) * kH, 0xFF);
  ui::Canvas canvas{bytes.data(), kW, kH, kStride};
  ui::UiApp app{kW, kH};
  app::StatusPage *page = nullptr;

  Harness() {
    canvas.clear(ui::Color::White);
    auto &added = app.add_page(std::make_unique<app::StatusPage>(wifi_source));
    page = static_cast<app::StatusPage *>(&added);
  }

  void resetWifi() { g_wifi = app::WifiInfo{}; }
};

bool contains(const std::vector<ui::Rect> &rects, const ui::Rect &want) {
  for (const ui::Rect &r : rects) {
    if (r.contains(want)) {
      return true;
    }
  }
  return false;
}

} // namespace

void test_counter_ticks(void) {
  Harness h;
  h.resetWifi();
  h.app.tick(0);
  h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_STRING("0000", h.page->counter_label().text().c_str());

  h.app.tick(2000);
  h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_STRING("0001", h.page->counter_label().text().c_str());

  h.app.tick(2500); // still inside the period
  h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_STRING("0001", h.page->counter_label().text().c_str());

  h.app.tick(4000);
  h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_STRING("0002", h.page->counter_label().text().c_str());
}

void test_tick_box_toggles(void) {
  Harness h;
  h.resetWifi();
  h.app.tick(0);
  TEST_ASSERT_EQUAL_INT((int)ui::Color::Black,
                        (int)h.page->tick_box().params().get_color(ui::Box::kColor));

  h.app.tick(7000);
  TEST_ASSERT_EQUAL_INT((int)ui::Color::White,
                        (int)h.page->tick_box().params().get_color(ui::Box::kColor));

  h.app.tick(14000);
  TEST_ASSERT_EQUAL_INT((int)ui::Color::Black,
                        (int)h.page->tick_box().params().get_color(ui::Box::kColor));
}

void test_wifi_label_follows_source(void) {
  Harness h;
  h.resetWifi();
  h.app.tick(0);
  TEST_ASSERT_EQUAL_STRING("wifi: connecting...",
                           h.page->wifi_label().text().c_str());

  g_wifi.connected = true;
  g_wifi.ssid = "Boost Booster";
  g_wifi.ip = "192.168.50.7";
  h.app.tick(1000);
  TEST_ASSERT_EQUAL_STRING("wifi: Boost Booster (192.168.50.7)",
                           h.page->wifi_label().text().c_str());

  g_wifi.connected = false;
  h.app.tick(2000);
  TEST_ASSERT_EQUAL_STRING("wifi: connecting...",
                           h.page->wifi_label().text().c_str());
}

void test_dirty_region_is_just_the_counter(void) {
  Harness h;
  h.resetWifi();
  h.app.tick(0);
  h.app.present(h.canvas);

  h.app.tick(2000);
  const std::vector<ui::Rect> dirty = h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_size_t(1, dirty.size());
  TEST_ASSERT_TRUE(contains(dirty, h.page->counter_label().bounds()));
}

void test_mode_label_change_is_dirty(void) {
  Harness h;
  h.resetWifi();
  h.app.tick(0);
  h.app.present(h.canvas);

  h.page->set_mode("full");
  const std::vector<ui::Rect> dirty = h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_size_t(1, dirty.size());
  TEST_ASSERT_TRUE(contains(dirty, h.page->mode_label().bounds()));
  TEST_ASSERT_EQUAL_STRING("full", h.page->mode_label().text().c_str());
}

void test_counter_wraps_at_four_digits(void) {
  Harness h;
  h.resetWifi();
  h.app.tick(0);
  h.app.present(h.canvas);
  // 10000 ticks of the 2 s counter would take too long: drive the wrap by
  // stepping the clock past 4 digits' worth of updates.
  for (int i = 0; i < 10001; ++i) {
    h.app.tick(static_cast<uint32_t>(i + 1) * 2000);
  }
  TEST_ASSERT_EQUAL_STRING("0001", h.page->counter_label().text().c_str());
}

void test_boot_frame_golden(void) {
  Harness h;
  h.resetWifi();
  h.app.tick(0);
  h.app.present(h.canvas);

  const uint32_t hash = h.canvas.hash();
  if (std::getenv("UPDATE_GOLDEN") != nullptr) {
    std::printf("UPDATE_GOLDEN: boot frame hash = 0x%08X\n", (unsigned)hash);
    return;
  }
  TEST_ASSERT_EQUAL_HEX32(0x05A65D51u, hash);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_counter_ticks);
  RUN_TEST(test_tick_box_toggles);
  RUN_TEST(test_wifi_label_follows_source);
  RUN_TEST(test_dirty_region_is_just_the_counter);
  RUN_TEST(test_mode_label_change_is_dirty);
  RUN_TEST(test_counter_wraps_at_four_digits);
  RUN_TEST(test_boot_frame_golden);
  return UNITY_END();
}
