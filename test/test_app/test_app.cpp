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
  TEST_ASSERT_TRUE(h.app.needs_present());
  const std::vector<ui::Rect> dirty = h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_size_t(1, dirty.size());
  TEST_ASSERT_TRUE(dirty[0] == (ui::Rect{0, 0, kW, kH}));
  TEST_ASSERT_FALSE(h.app.page_changed());
  TEST_ASSERT_FALSE(h.app.needs_present());
}

void test_second_present_is_empty(void) {
  Harness h;
  h.app.add_page(std::make_unique<ui::Page>("home"));
  h.app.present(h.canvas);
  TEST_ASSERT_TRUE(h.app.present(h.canvas).empty());
}

void test_needs_present_tracks_parameter_changes(void) {
  Harness h;
  auto &page = h.app.add_page(std::make_unique<ui::Page>("home"));
  auto &label = page.add<ui::Label>("greeting", label_params("AB", 5, 5));
  h.app.present(h.canvas);

  TEST_ASSERT_FALSE(h.app.needs_present());
  label.set_text("AB"); // same value: no refresh
  TEST_ASSERT_FALSE(h.app.needs_present());

  label.set_text("B");
  TEST_ASSERT_TRUE(h.app.needs_present());
  h.app.present(h.canvas);
  TEST_ASSERT_FALSE(h.app.needs_present());

  h.app.invalidate();
  TEST_ASSERT_TRUE(h.app.needs_present());
}

void test_changed_label_reports_its_bounds(void) {
  Harness h;
  auto &page = h.app.add_page(std::make_unique<ui::Page>("home"));
  auto &label = page.add<ui::Label>("greeting", label_params("AB", 5, 5));
  h.app.present(h.canvas);
  TEST_ASSERT_TRUE(label.bounds() == (ui::Rect{5, 5, 5, 2}));

  label.set_text("B");
  std::vector<ui::Rect> dirty = h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_size_t(1, dirty.size());
  // Union of the old 5x2 box and the new 2x2 box.
  TEST_ASSERT_TRUE(dirty[0] == (ui::Rect{5, 5, 5, 2}));
  TEST_ASSERT_FALSE(h.canvas.test(8, 5)); // old 'B' erased

  TEST_ASSERT_TRUE(h.app.present(h.canvas).empty());
}

void test_same_value_set_is_not_dirty(void) {
  Harness h;
  auto &page = h.app.add_page(std::make_unique<ui::Page>("home"));
  auto &label = page.add<ui::Label>("greeting", label_params("A", 5, 5));
  h.app.present(h.canvas);

  label.set_text("A");
  label.set_color(ui::Color::Black);
  TEST_ASSERT_TRUE(h.app.present(h.canvas).empty());
}

void test_moved_component_reports_old_and_new_area(void) {
  Harness h;
  auto &page = h.app.add_page(std::make_unique<ui::Page>("home"));
  auto &label = page.add<ui::Label>("greeting", label_params("A", 5, 5));
  h.app.present(h.canvas);

  label.set(ui::Component::kX, ui::Param(20));
  std::vector<ui::Rect> dirty = h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_size_t(1, dirty.size());
  TEST_ASSERT_TRUE(dirty[0] == (ui::Rect{5, 5, 18, 2}));
  TEST_ASSERT_FALSE(h.canvas.test(5, 5));
  TEST_ASSERT_TRUE(h.canvas.test(20, 5));
}

void test_two_distant_changes_produce_two_regions(void) {
  Harness h;
  auto &page = h.app.add_page(std::make_unique<ui::Page>("home"));
  auto &first = page.add<ui::Label>("first", label_params("A", 1, 1));
  auto &second = page.add<ui::Label>("second", label_params("A", 60, 40));
  h.app.present(h.canvas);

  first.set_text("B");
  second.set_text("B");
  std::vector<ui::Rect> dirty = h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_size_t(2, dirty.size());
  TEST_ASSERT_TRUE(dirty[0] == (ui::Rect{1, 1, 3, 2}));
  TEST_ASSERT_TRUE(dirty[1] == (ui::Rect{60, 40, 3, 2}));
}

void test_container_background_change_is_reported(void) {
  Harness h;
  auto &page = h.app.add_page(std::make_unique<ui::Page>("home"));
  auto &card = page.add<ui::Container>(
      "card", ui::ParamSet{{ui::Component::kX, ui::Param(10)},
                           {ui::Component::kY, ui::Param(10)},
                           {ui::Component::kW, ui::Param(30)},
                           {ui::Component::kH, ui::Param(20)}});
  card.add<ui::Label>("inside", label_params("A", 2, 2));
  h.app.present(h.canvas);

  card.set(ui::Container::kBg, ui::Param(ui::Color::Black));
  const std::vector<ui::Rect> dirty = h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_size_t(1, dirty.size());
  TEST_ASSERT_TRUE(dirty[0] == (ui::Rect{10, 10, 30, 20}));
  TEST_ASSERT_TRUE(h.canvas.test(10, 10));
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

  // Staying hidden is not dirty.
  TEST_ASSERT_TRUE(h.app.present(h.canvas).empty());
}

void test_invalidate_reports_whole_screen(void) {
  Harness h;
  h.app.add_page(std::make_unique<ui::Page>("home"));
  h.app.present(h.canvas);

  h.app.invalidate();
  const std::vector<ui::Rect> dirty = h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_size_t(1, dirty.size());
  TEST_ASSERT_TRUE(dirty[0] == (ui::Rect{0, 0, kW, kH}));
}

void test_unknown_page_is_rejected(void) {
  Harness h;
  h.app.add_page(std::make_unique<ui::Page>("home"));
  TEST_ASSERT_FALSE(h.app.show_page("missing"));
  TEST_ASSERT_EQUAL_STRING("home", h.app.page_name().c_str());
  TEST_ASSERT_EQUAL_size_t(1, h.app.page_count());
}

void test_page_switch_invalidates_and_back_again(void) {
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

  std::vector<ui::Rect> dirty = h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_size_t(1, dirty.size());
  TEST_ASSERT_TRUE(dirty[0] == (ui::Rect{0, 0, kW, kH}));
  TEST_ASSERT_TRUE(h.canvas.test(0, 0)); // black page background

  TEST_ASSERT_TRUE(h.app.show_page("home"));
  dirty = h.app.present(h.canvas);
  TEST_ASSERT_EQUAL_size_t(1, dirty.size());
  TEST_ASSERT_TRUE(dirty[0] == (ui::Rect{0, 0, kW, kH}));
  TEST_ASSERT_FALSE(h.canvas.test(0, 0)); // white again
}

void test_mismatched_canvas_is_ignored(void) {
  Harness h;
  h.app.add_page(std::make_unique<ui::Page>("home"));
  h.app.present(h.canvas);

  std::vector<uint8_t> small_bytes(4 * 4, 0xFF);
  ui::Canvas small(small_bytes.data(), 8, 4, 1);
  TEST_ASSERT_TRUE(h.app.present(small).empty());
  TEST_ASSERT_TRUE(h.app.present(h.canvas).empty());
}

void test_validation_errors_are_reported(void) {
  Harness h;
  auto &page = h.app.add_page(std::make_unique<ui::Page>("home"));
  page.add<ui::Label>("broken", ui::ParamSet{
                                    {ui::Label::kText, ui::Param("A")},
                                    {ui::Label::kFont, ui::Param()},
                                    {"bogus", ui::Param(1)},
                                });

  const auto &issues = h.app.validation_errors();
  TEST_ASSERT_EQUAL_size_t(2, issues.size());
  TEST_ASSERT_EQUAL_STRING("Label#broken", issues[0].component.c_str());
  TEST_ASSERT_EQUAL_STRING("bogus", issues[0].error.key.c_str());
  TEST_ASSERT_EQUAL_STRING("font", issues[1].error.key.c_str());
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
  RUN_TEST(test_needs_present_tracks_parameter_changes);
  RUN_TEST(test_changed_label_reports_its_bounds);
  RUN_TEST(test_same_value_set_is_not_dirty);
  RUN_TEST(test_moved_component_reports_old_and_new_area);
  RUN_TEST(test_two_distant_changes_produce_two_regions);
  RUN_TEST(test_container_background_change_is_reported);
  RUN_TEST(test_hidden_component_reports_its_old_area);
  RUN_TEST(test_invalidate_reports_whole_screen);
  RUN_TEST(test_unknown_page_is_rejected);
  RUN_TEST(test_page_switch_invalidates_and_back_again);
  RUN_TEST(test_mismatched_canvas_is_ignored);
  RUN_TEST(test_validation_errors_are_reported);
  RUN_TEST(test_describe_lists_pages);
  return UNITY_END();
}
