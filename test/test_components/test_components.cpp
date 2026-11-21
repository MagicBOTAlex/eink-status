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

constexpr int kW = 40;
constexpr int kH = 20;
constexpr int kStride = (kW + 7) / 8;

ui::ParamSet label_params(const char *text, int x = 0, int y = 0) {
  return {
      {ui::Label::kText, ui::Param(text)},
      {ui::Label::kFont, ui::Param(&test::test_font())},
      {ui::Component::kX, ui::Param(x)},
      {ui::Component::kY, ui::Param(y)},
  };
}

int black_pixels(const ui::Canvas &canvas) {
  int count = 0;
  for (int y = 0; y < canvas.height(); ++y) {
    for (int x = 0; x < canvas.width(); ++x) {
      if (canvas.test(x, y)) {
        ++count;
      }
    }
  }
  return count;
}

} // namespace

void test_label_measures_its_text(void) {
  ui::Label label("l", label_params("AB"));
  TEST_ASSERT_TRUE(label.bounds().empty());

  label.layout({0, 0, 100, 50});
  TEST_ASSERT_TRUE(label.bounds() == (ui::Rect{0, 0, 5, 2}));

  ui::Label empty("e", label_params(""));
  TEST_ASSERT_TRUE(empty.measure({100, 50}) == (ui::Size{0, 2}));

  // Ink that overhangs the advance is part of the measurement.
  ui::Label overhang("o", label_params(test::kOverhangText));
  TEST_ASSERT_TRUE(overhang.measure({100, 50}) ==
                   (ui::Size{test::kOverhangInk, 2}));
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
  ui::Label label("l", label_params("A", 5, 5));
  label.layout({0, 0, 100, 50});
  label.set_visible(false);
  TEST_ASSERT_FALSE(label.visible());

  // Separate buffers: two Canvases over the same memory would make this
  // assertion meaningless.
  std::vector<uint8_t> drawn_bytes(kStride * kH, 0xFF);
  std::vector<uint8_t> reference_bytes(kStride * kH, 0xFF);
  ui::Canvas drawn(drawn_bytes.data(), kW, kH, kStride);
  ui::Canvas reference(reference_bytes.data(), kW, kH, kStride);
  drawn.clear(ui::Color::White);
  reference.clear(ui::Color::White);

  label.draw(drawn);
  TEST_ASSERT_EQUAL_INT(0, black_pixels(drawn));
  TEST_ASSERT_EQUAL_UINT32(reference.hash(), drawn.hash());
}

void test_draw_is_clipped_to_bounds(void) {
  // 3 px wide box holding a 4 px glyph: the overhanging column must not leak.
  ui::ParamSet params = label_params(test::kOverhangText, 5, 5);
  params.set(ui::Component::kH, ui::Param(2));
  params.set(ui::Component::kW, ui::Param(3));
  ui::Label label("l", params);
  label.layout({0, 0, 100, 50});

  std::vector<uint8_t> bytes(kStride * kH, 0xFF);
  ui::Canvas canvas(bytes.data(), kW, kH, kStride);
  canvas.clear(ui::Color::White);
  label.draw(canvas);

  TEST_ASSERT_TRUE(canvas.test(5, 5));
  TEST_ASSERT_TRUE(canvas.test(7, 5));
  TEST_ASSERT_FALSE(canvas.test(8, 5)); // clipped at bounds.right() = 8
}

void test_rule_is_one_pixel_high_by_default(void) {
  ui::Rule rule("line", {{ui::Component::kX, ui::Param(2)},
                         {ui::Component::kW, ui::Param(10)}});
  rule.layout({0, 0, 100, 50});
  TEST_ASSERT_TRUE(rule.bounds() == (ui::Rect{2, 0, 10, 1}));

  std::vector<uint8_t> bytes(kStride * kH, 0xFF);
  ui::Canvas canvas(bytes.data(), kW, kH, kStride);
  canvas.clear(ui::Color::White);
  rule.draw(canvas);
  TEST_ASSERT_EQUAL_INT(10, black_pixels(canvas));
  TEST_ASSERT_TRUE(canvas.test(2, 0));
  TEST_ASSERT_FALSE(canvas.test(2, 1));
}

void test_box_fill_and_border(void) {
  ui::Box fill("fill", {{ui::Component::kW, ui::Param(4)},
                        {ui::Component::kH, ui::Param(3)},
                        {ui::Box::kColor, ui::Param(ui::Color::Black)}});
  fill.layout({0, 0, 100, 50});
  std::vector<uint8_t> bytes(kStride * kH, 0xFF);
  ui::Canvas canvas(bytes.data(), kW, kH, kStride);
  canvas.clear(ui::Color::White);
  fill.draw(canvas);
  TEST_ASSERT_EQUAL_INT(12, black_pixels(canvas));

  // Border-only box leaves the interior alone.
  canvas.clear(ui::Color::White);
  ui::Box border("border", {{ui::Component::kW, ui::Param(6)},
                            {ui::Component::kH, ui::Param(6)},
                            {ui::Box::kBorder, ui::Param(1)}});
  border.layout({0, 0, 100, 50});
  border.draw(canvas);
  TEST_ASSERT_TRUE(canvas.test(0, 0));
  TEST_ASSERT_FALSE(canvas.test(3, 3));
  TEST_ASSERT_EQUAL_INT(20, black_pixels(canvas));
}

void test_generation_only_changes_on_real_changes(void) {
  ui::Label label("l", label_params("AB"));
  const uint32_t base = label.generation();

  label.set(ui::Label::kText, ui::Param("AB")); // same value
  TEST_ASSERT_EQUAL_UINT32(base, label.generation());

  label.set(ui::Label::kText, ui::Param("B"));
  TEST_ASSERT_EQUAL_UINT32(base + 1, label.generation());

  label.set(ui::ParamSet{{ui::Label::kText, ui::Param("B")}}); // same value
  TEST_ASSERT_EQUAL_UINT32(base + 1, label.generation());

  label.remove_param(ui::Label::kText);
  TEST_ASSERT_EQUAL_UINT32(base + 2, label.generation());
  TEST_ASSERT_FALSE(label.has_param(ui::Label::kText));
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

  // Present but empty is also missing.
  ui::Label empty_font("e", {{ui::Label::kText, ui::Param("x")},
                             {ui::Label::kFont, ui::Param()}});
  const std::vector<ui::ParamError> empty = empty_font.validate();
  TEST_ASSERT_EQUAL_size_t(1, empty.size());
  TEST_ASSERT_EQUAL_STRING("font", empty[0].key.c_str());
}

void test_container_layout_and_children(void) {
  ui::Container panel("panel", {
                                     {ui::Component::kW, ui::Param(50)},
                                     {ui::Component::kH, ui::Param(20)},
                                     {ui::Component::kX, ui::Param(10)},
                                     {ui::Component::kY, ui::Param(5)},
                                 });

  ui::Label &child = panel.add<ui::Label>("child", label_params("AB", 2, 3));
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

void test_page_covers_screen_and_rejects_layout_params(void) {
  ui::Page page("home");
  ui::Label &child = page.add<ui::Label>("hello", label_params("AB", 7, 8));

  page.layout(120, 60);
  TEST_ASSERT_TRUE(page.bounds() == (ui::Rect{0, 0, 120, 60}));
  TEST_ASSERT_TRUE(child.bounds() == (ui::Rect{7, 8, 5, 2}));
  TEST_ASSERT_EQUAL_STRING("home", page.name().c_str());

  // Pages are pinned to the screen: layout parameters are not in the schema.
  const std::vector<ui::ParamError> errors =
      ui::validate_params({{"x", ui::Param(5)}}, page.schema());
  TEST_ASSERT_EQUAL_size_t(1, errors.size());
  TEST_ASSERT_EQUAL_STRING("unknown parameter", errors[0].message.c_str());

  TEST_ASSERT_EQUAL_size_t(0, page.validate().size());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_label_measures_its_text);
  RUN_TEST(test_geometry_params_win_over_measure);
  RUN_TEST(test_anchor_center);
  RUN_TEST(test_hidden_components_do_not_draw);
  RUN_TEST(test_draw_is_clipped_to_bounds);
  RUN_TEST(test_rule_is_one_pixel_high_by_default);
  RUN_TEST(test_box_fill_and_border);
  RUN_TEST(test_generation_only_changes_on_real_changes);
  RUN_TEST(test_validate_reports_unknown_and_missing);
  RUN_TEST(test_container_layout_and_children);
  RUN_TEST(test_describe_tree);
  RUN_TEST(test_page_covers_screen_and_rejects_layout_params);
  return UNITY_END();
}
