#include <unity.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "panel_plan.hpp"
#include "screens/status_page.hpp"
#include "ui/app.hpp"
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
  app::WifiInfo wifi;
  int wifi_calls = 0;
  app::StatusPage *page = nullptr;

  Harness() {
    canvas.clear(ui::Color::White);
    auto &added = app.add_page(std::make_unique<app::StatusPage>([this] {
      ++wifi_calls;
      return wifi;
    }));
    page = static_cast<app::StatusPage *>(&added);
  }
};

void write_artifact(const ui::Canvas &canvas) {
  FILE *f = std::fopen("test_boot.pgm", "wb");
  if (f == nullptr) {
    return;
  }
  std::fprintf(f, "P5\n%d %d\n255\n", canvas.width(), canvas.height());
  for (int y = 0; y < canvas.height(); ++y) {
    for (int x = 0; x < canvas.width(); ++x) {
      std::fputc(canvas.test(x, y) ? 0 : 255, f);
    }
  }
  std::fclose(f);
  std::printf("wrote test_boot.pgm\n%s",
              canvas.to_ascii(canvas.bounds(), 6).c_str());
}

} // namespace

void test_counter_ticks(void) {
  Harness h;
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

void test_counter_survives_uint32_wrap(void) {
  Harness h;
  const uint32_t before_wrap = 0xFFFFFF00u;
  // The first tick always advances the counter (the device boots seconds
  // after the clock started), so the baseline is "0001".
  h.app.tick(before_wrap);
  h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_STRING("0001", h.page->counter_label().text().c_str());

  // 512 ms later across the wrap: not yet a counter period.
  h.app.tick(0x00000100u);
  h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_STRING("0001", h.page->counter_label().text().c_str());

  // 2560 ms after the last tick, across the wrap: one period elapsed.
  h.app.tick(0x00000900u);
  h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_STRING("0002", h.page->counter_label().text().c_str());
}

void test_tick_box_toggles(void) {
  Harness h;
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

void test_wifi_label_follows_source_and_is_throttled(void) {
  Harness h;
  h.app.tick(0);
  TEST_ASSERT_EQUAL_INT(1, h.wifi_calls);
  TEST_ASSERT_EQUAL_STRING("wifi: connecting...",
                           h.page->wifi_label().text().c_str());

  // 50 Hz UI ticks must not poll the WiFi stack 50 times a second.
  for (uint32_t now = 20; now < 1000; now += 20) {
    h.app.tick(now);
  }
  TEST_ASSERT_EQUAL_INT(1, h.wifi_calls);

  h.app.tick(1000);
  TEST_ASSERT_EQUAL_INT(2, h.wifi_calls);

  h.wifi.connected = true;
  h.wifi.ssid = "Boost Booster";
  h.wifi.ip = "192.168.50.7";
  h.app.tick(2000);
  TEST_ASSERT_EQUAL_STRING("wifi: Boost Booster (192.168.50.7)",
                           h.page->wifi_label().text().c_str());

  h.wifi.connected = false;
  h.app.tick(3000);
  TEST_ASSERT_EQUAL_STRING("wifi: connecting...",
                           h.page->wifi_label().text().c_str());
}

void test_counter_dirty_rect_is_exact(void) {
  Harness h;
  h.app.tick(0);
  h.app.present(h.canvas);

  h.app.tick(2000);
  const std::vector<ui::Rect> dirty = h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_size_t(1, dirty.size());
  TEST_ASSERT_TRUE(dirty[0] == h.page->counter_label().bounds());
  TEST_ASSERT_TRUE(dirty[0] == (ui::Rect{284, 275, 80, 20}));
}

void test_mode_label_change_erases_the_old_text(void) {
  Harness h;
  h.app.tick(0);
  h.app.present(h.canvas);

  // Find a pixel that "partial" inks but "full" will not cover.
  const ui::Rect mode = h.page->mode_label().bounds();
  int ink_x = -1;
  int ink_y = -1;
  for (int y = mode.y; y < mode.bottom() && ink_y < 0; ++y) {
    for (int x = mode.x; x < 604 && ink_x < 0; ++x) {
      if (h.canvas.test(x, y)) {
        ink_x = x;
        ink_y = y;
      }
    }
  }
  TEST_ASSERT_TRUE(ink_x >= 0);

  h.page->set_mode("full");
  const std::vector<ui::Rect> dirty = h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_size_t(1, dirty.size());
  TEST_ASSERT_TRUE(dirty[0] == mode); // old bounds are included
  TEST_ASSERT_FALSE(h.canvas.test(ink_x, ink_y));
}

void test_counter_wraps_at_four_digits(void) {
  Harness h;
  h.app.tick(0);
  h.app.present(h.canvas);
  for (int i = 0; i < 10001; ++i) {
    h.app.tick(static_cast<uint32_t>(i + 1) * 2000);
  }
  TEST_ASSERT_EQUAL_STRING("0001", h.page->counter_label().text().c_str());
}

void test_dirty_regions_map_to_valid_panel_windows(void) {
  Harness h;
  h.app.tick(0);
  h.app.present(h.canvas);
  h.app.tick(2000);
  const std::vector<ui::Rect> dirty = h.app.present(h.canvas);

  for (const ui::Rect &rect : dirty) {
    const panel::Window window = panel::to_window(rect);
    TEST_ASSERT_TRUE(panel::valid(window));
    TEST_ASSERT_TRUE(window.x2 >= window.x1);
    TEST_ASSERT_TRUE(window.y2 >= window.y1);
  }

  // The counter is the regression case: converting (x, y, w, h) as if it were
  // (x1, y1, x2, y2) used to invert the window.
  const panel::Window counter = panel::to_window({284, 275, 80, 20});
  TEST_ASSERT_TRUE(counter == (panel::Window{284, 275, 363, 294}));
  TEST_ASSERT_TRUE(panel::valid(counter));
}

void test_page_validates_cleanly(void) {
  Harness h;
  TEST_ASSERT_EQUAL_size_t(0, h.app.validation_errors().size());
  TEST_ASSERT_EQUAL_size_t(0, h.page->validate().size());
}

void test_boot_frame_golden(void) {
  Harness h;
  h.app.tick(0);
  h.app.present(h.canvas);

  const uint32_t hash = h.canvas.hash();
  const bool print_only = std::getenv("PRINT_GOLDEN") != nullptr;
  const bool in_ci = std::getenv("CI") != nullptr;

  if (print_only && !in_ci) {
    std::printf("boot frame hash = 0x%08X\n", (unsigned)hash);
    return;
  }

  if (hash != 0x05A65D51u) {
    write_artifact(h.canvas);
  }
  TEST_ASSERT_EQUAL_HEX32(0x05A65D51u, hash);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_counter_ticks);
  RUN_TEST(test_counter_survives_uint32_wrap);
  RUN_TEST(test_tick_box_toggles);
  RUN_TEST(test_wifi_label_follows_source_and_is_throttled);
  RUN_TEST(test_counter_dirty_rect_is_exact);
  RUN_TEST(test_mode_label_change_erases_the_old_text);
  RUN_TEST(test_counter_wraps_at_four_digits);
  RUN_TEST(test_dirty_regions_map_to_valid_panel_windows);
  RUN_TEST(test_page_validates_cleanly);
  RUN_TEST(test_boot_frame_golden);
  return UNITY_END();
}
