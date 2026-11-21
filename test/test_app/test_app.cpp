#include <unity.h>

#include <cstring>
#include <memory>
#include <vector>

#include "common/test_helpers.hpp"
#include "ui/app.hpp"
#include "ui/components.hpp"

void setUp(void) {}
void tearDown(void) {}

namespace {

constexpr int kW = 100;
constexpr int kH = 50;
constexpr int kStride = (kW + 7) / 8;

ui::ParamSet label_params(const char *text, int x, int y) {
  return {
      {ui::Label::kText, ui::Param(text)},
      {ui::Label::kFont, ui::Param(&test::test_font())},
      {ui::Component::kX, ui::Param(x)},
      {ui::Component::kY, ui::Param(y)},
  };
}

struct Harness {
  std::vector<uint8_t> bytes =
      std::vector<uint8_t>(static_cast<size_t>(kStride) * kH, 0xFF);
  ui::Canvas canvas{bytes.data(), kW, kH, kStride};
  ui::UiApp app{kW, kH};

  Harness() { canvas.clear(ui::Color::White); }
};

} // namespace

void test_first_present_is_full_screen(void) {
  Harness h;
  h.app.add_page(std::make_unique<ui::Page>("home"));
  TEST_ASSERT_TRUE(h.app.page_changed());
  const std::vector<ui::Rect> dirty = h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_size_t(1, dirty.size());
  TEST_ASSERT_TRUE(dirty[0] == (ui::Rect{0, 0, kW, kH}));
  TEST_ASSERT_FALSE(h.app.page_changed());
}

void test_second_present_is_empty(void) {
  Harness h;
  h.app.add_page(std::make_unique<ui::Page>("home"));
  h.app.present(h.canvas);
  TEST_ASSERT_TRUE(h.app.present(h.canvas).empty());
  TEST_ASSERT_FALSE(h.app.page_changed());
}

void test_changed_label_reports_its_bounds(void) {
  Harness h;
  auto &page = h.app.add_page(std::make_unique<ui::Page>("home"));
  auto &label = page.add<ui::Label>("greeting", label_params("AB", 5, 5));
  h.app.present(h.canvas);
  TEST_ASSERT_TRUE(label.bounds() == (ui::Rect{5, 5, 5, 2}));

  label.set_text("B");
  const std::vector<ui::Rect> dirty = h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_size_t(1, dirty.size());
  // Union of the old 5x2 box and the new 2x2 box.
  TEST_ASSERT_TRUE(dirty[0] == (ui::Rect{5, 5, 5, 2}));

  // Nothing changed this time.
  TEST_ASSERT_TRUE(h.app.present(h.canvas).empty());
}

void test_hidden_component_reports_its_old_area(void) {
  Harness h;
  auto &page = h.app.add_page(std::make_unique<ui::Page>("home"));
  auto &label = page.add<ui::Label>("greeting", label_params("A", 8, 9));
  h.app.present(h.canvas);

  label.set_visible(false);
  const std::vector<ui::Rect> dirty = h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_size_t(1, dirty.size());
  TEST_ASSERT_TRUE(dirty[0] == (ui::Rect{8, 9, 3, 2}));
  TEST_ASSERT_FALSE(h.canvas.test(8, 9)); // background repainted under it
}

void test_unknown_page_is_rejected(void) {
  Harness h;
  h.app.add_page(std::make_unique<ui::Page>("home"));
  TEST_ASSERT_FALSE(h.app.show_page("missing"));
  TEST_ASSERT_EQUAL_STRING("home", h.app.page_name().c_str());
  TEST_ASSERT_EQUAL_size_t(1, h.app.page_count());
}

void test_page_switch_invalidates_screen(void) {
  Harness h;
  auto &home = h.app.add_page(std::make_unique<ui::Page>("home"));
  home.add<ui::Label>("a", label_params("AB", 1, 1));

  auto &other = h.app.add_page(std::make_unique<ui::Page>(
      "other",
      ui::ParamSet{{ui::Container::kBg, ui::Param(ui::Color::Black)}}));
  other.add<ui::Label>("b", label_params("B", 2, 2));

  h.app.present(h.canvas);
  TEST_ASSERT_TRUE(h.app.show_page("other"));
  TEST_ASSERT_TRUE(h.app.page_changed());

  const std::vector<ui::Rect> dirty = h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_size_t(1, dirty.size());
  TEST_ASSERT_TRUE(dirty[0] == (ui::Rect{0, 0, kW, kH}));
  TEST_ASSERT_TRUE(h.canvas.test(0, 0)); // black page background, no label ink
}

void test_describe_lists_pages(void) {
  Harness h;
  h.app.add_page(std::make_unique<ui::Page>("home"));
  const std::string text = h.app.describe();
  TEST_ASSERT_NOT_NULL(strstr(text.c_str(), "UiApp 100x50 pages=1"));
  TEST_ASSERT_NOT_NULL(strstr(text.c_str(), "*   Page#page:home"));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_first_present_is_full_screen);
  RUN_TEST(test_second_present_is_empty);
  RUN_TEST(test_changed_label_reports_its_bounds);
  RUN_TEST(test_hidden_component_reports_its_old_area);
  RUN_TEST(test_unknown_page_is_rejected);
  RUN_TEST(test_page_switch_invalidates_screen);
  RUN_TEST(test_describe_lists_pages);
  return UNITY_END();
}
