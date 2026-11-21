#include <unity.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "common/test_helpers.hpp"
#include "ui/components.hpp"
#include "ui/page.hpp"

void setUp(void) {}
void tearDown(void) {}

namespace {

ui::ParamSet label_params(const char *text, int x = 0, int y = 0) {
  return {
      {ui::Label::kText, ui::Param(text)},
      {ui::Label::kFont, ui::Param(&test::test_font())},
      {ui::Component::kX, ui::Param(x)},
      {ui::Component::kY, ui::Param(y)},
  };
}

} // namespace

void test_label_measures_its_text(void) {
  ui::Label label("l", label_params("AB"));
  TEST_ASSERT_TRUE(label.bounds().empty());

  label.layout({0, 0, 100, 50});
  TEST_ASSERT_TRUE(label.bounds() == (ui::Rect{0, 0, 5, 2}));

  ui::Label empty("e", label_params(""));
  TEST_ASSERT_TRUE(empty.measure({100, 50}) == (ui::Size{0, 2}));
}

void test_geometry_params_win_over_measure(void) {
  ui::ParamSet params = label_params("AB", 5, 6);
  params.set(ui::Component::kW, ui::Param(20));
  params.set(ui::Component::kH, ui::Param(10));
  ui::Label label("l", params);

  label.layout({0, 0, 100, 50});
  TEST_ASSERT_TRUE(label.bounds() == (ui::Rect{5, 6, 20, 10}));
}

void test_anchor_center(void) {
  ui::ParamSet params = label_params("AB");
  params.set(ui::Component::kAnchor, ui::Param(ui::Align::Center));
  ui::Label label("l", params);

  label.layout({0, 0, 100, 50});
  TEST_ASSERT_TRUE(label.bounds() == (ui::Rect{47, 24, 5, 2}));
}

void test_hidden_components_do_not_draw(void) {
  ui::Label label("l", label_params("A"));
  label.layout({0, 0, 100, 50});
  label.set_visible(false);
  TEST_ASSERT_FALSE(label.visible());

  std::vector<uint8_t> bytes(13 * 50, 0xFF);
  ui::Canvas canvas(bytes.data(), 100, 50, 13);
  ui::Canvas reference(bytes.data(), 100, 50, 13);
  reference.clear(ui::Color::White);
  label.draw(canvas);
  TEST_ASSERT_FALSE(canvas.test(0, 0));
  TEST_ASSERT_EQUAL_UINT32(reference.hash(), canvas.hash());
}

void test_validate_reports_unknown_and_missing(void) {
  ui::ParamSet params = label_params("A");
  params.set("bogus", ui::Param(1));
  ui::Label label("l", params);

  const std::vector<ui::ParamError> errors = label.validate();
  TEST_ASSERT_EQUAL_size_t(1, errors.size());
  TEST_ASSERT_EQUAL_STRING("bogus", errors[0].key.c_str());
  TEST_ASSERT_EQUAL_STRING("unknown parameter", errors[0].message.c_str());

  ui::Label no_font("n", {{ui::Label::kText, ui::Param("x")}});
  const std::vector<ui::ParamError> missing = no_font.validate();
  TEST_ASSERT_EQUAL_size_t(1, missing.size());
  TEST_ASSERT_EQUAL_STRING("font", missing[0].key.c_str());
  TEST_ASSERT_EQUAL_STRING("missing required parameter",
                           missing[0].message.c_str());
}

void test_container_layout_and_children(void) {
  ui::Container panel("panel", {
                                     {ui::Component::kW, ui::Param(50)},
                                     {ui::Component::kH, ui::Param(20)},
                                     {ui::Component::kX, ui::Param(10)},
                                     {ui::Component::kY, ui::Param(5)},
                                 });

  ui::Label &child =
      panel.add<ui::Label>("child", label_params("AB", 2, 3));
  panel.layout({0, 0, 100, 100});

  TEST_ASSERT_TRUE(panel.bounds() == (ui::Rect{10, 5, 50, 20}));
  TEST_ASSERT_TRUE(child.bounds() == (ui::Rect{12, 8, 5, 2}));
}

void test_describe_tree(void) {
  ui::Container panel("panel", {{ui::Component::kW, ui::Param(50)},
                                {ui::Component::kH, ui::Param(20)}});
  panel.add<ui::Label>("child", label_params("AB"));
  const std::string text = panel.describe_tree();
  TEST_ASSERT_NOT_NULL(strstr(text.c_str(), "Container#panel"));
  TEST_ASSERT_NOT_NULL(strstr(text.c_str(), "Label#child"));
  TEST_ASSERT_NOT_NULL(strstr(text.c_str(), "text=text(\"AB\")"));
}

void test_page_covers_screen(void) {
  ui::Page page("home", {{ui::Container::kBg, ui::Param(ui::Color::White)}});
  ui::Label &child = page.add<ui::Label>("hello", label_params("AB", 7, 8));

  page.layout(120, 60);
  TEST_ASSERT_TRUE(page.bounds() == (ui::Rect{0, 0, 120, 60}));
  TEST_ASSERT_TRUE(child.bounds() == (ui::Rect{7, 8, 5, 2}));
  TEST_ASSERT_EQUAL_STRING("home", page.name().c_str());

  // A page rejects layout parameters it cannot honour.
  const std::vector<ui::ParamError> errors = page.validate();
  TEST_ASSERT_EQUAL_size_t(0, errors.size());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_label_measures_its_text);
  RUN_TEST(test_geometry_params_win_over_measure);
  RUN_TEST(test_anchor_center);
  RUN_TEST(test_hidden_components_do_not_draw);
  RUN_TEST(test_validate_reports_unknown_and_missing);
  RUN_TEST(test_container_layout_and_children);
  RUN_TEST(test_describe_tree);
  RUN_TEST(test_page_covers_screen);
  return UNITY_END();
}
