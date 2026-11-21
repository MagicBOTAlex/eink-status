#include <unity.h>

#include <vector>

#include "common/test_helpers.hpp"
#include "ui/types.hpp"

// Unity calls these around every test; define them exactly once in the suite.
void setUp(void) {}
void tearDown(void) {}

namespace {

bool same(const ui::Rect &a, const ui::Rect &b) { return a == b; }

} // namespace

void test_rect_intersect(void) {
  const ui::Rect a{0, 0, 10, 10};
  const ui::Rect b{5, 5, 10, 10};
  TEST_ASSERT_TRUE(same(a.intersect(b), {5, 5, 5, 5}));
  TEST_ASSERT_TRUE(same(a.intersect({20, 20, 5, 5}), ui::Rect{}));
}

void test_rect_unite(void) {
  TEST_ASSERT_TRUE(same(ui::Rect{0, 0, 10, 10}.unite({20, 5, 5, 5}),
                        {0, 0, 25, 10}));
  TEST_ASSERT_TRUE(same(ui::Rect{}.unite({3, 4, 5, 6}), {3, 4, 5, 6}));
}

void test_rect_contains(void) {
  const ui::Rect r{10, 10, 5, 5};
  TEST_ASSERT_TRUE(r.contains(10, 10));
  TEST_ASSERT_TRUE(r.contains(14, 14));
  TEST_ASSERT_FALSE(r.contains(15, 14));
  TEST_ASSERT_FALSE(r.contains(9, 10));
}

void test_anchor_rect_center(void) {
  const ui::Rect parent{0, 0, 100, 50};
  TEST_ASSERT_TRUE(same(ui::anchor_rect(parent, {10, 10}, ui::Align::Center),
                        {45, 20, 10, 10}));
  TEST_ASSERT_TRUE(
      same(ui::anchor_rect(parent, {10, 10}, ui::Align::TopLeft), {0, 0, 10, 10}));
  TEST_ASSERT_TRUE(same(
      ui::anchor_rect(parent, {10, 10}, ui::Align::BottomRight), {90, 40, 10, 10}));
}

void test_anchor_rect_offsets(void) {
  const ui::Rect parent{0, 0, 100, 50};
  // Bottom-right with a negative offset is how page margins are expressed.
  TEST_ASSERT_TRUE(same(
      ui::anchor_rect(parent, {10, 10}, ui::Align::BottomRight, -16, -10),
      {74, 30, 10, 10}));
  TEST_ASSERT_TRUE(same(
      ui::anchor_rect(parent, {10, 10}, ui::Align::Center, 0, 7), {45, 27, 10, 10}));
}

void test_merge_rects(void) {
  const std::vector<ui::Rect> merged =
      ui::merge_rects({{0, 0, 10, 10}, {5, 5, 10, 10}, {30, 30, 4, 4}}, 0);
  TEST_ASSERT_EQUAL_size_t(2, merged.size());
  TEST_ASSERT_TRUE(same(merged[0], {0, 0, 15, 15}));
  TEST_ASSERT_TRUE(same(merged[1], {30, 30, 4, 4}));

  // A gap of 5 pulls rects 5 px apart together, and empty rects are dropped.
  const std::vector<ui::Rect> gap =
      ui::merge_rects({{0, 0, 4, 4}, {9, 0, 4, 4}, {0, 0, 0, 0}}, 5);
  TEST_ASSERT_EQUAL_size_t(1, gap.size());
  TEST_ASSERT_TRUE(same(gap[0], {0, 0, 13, 4}));
}

void test_align_strings(void) {
  ui::Align align = ui::Align::TopLeft;
  TEST_ASSERT_TRUE(ui::align_from_string("center", &align));
  TEST_ASSERT_EQUAL_INT((int)ui::Align::Center, (int)align);
  TEST_ASSERT_FALSE(ui::align_from_string("nonsense", nullptr));

  ui::HAlign h = ui::HAlign::Left;
  TEST_ASSERT_TRUE(ui::halign_from_string("right", &h));
  TEST_ASSERT_EQUAL_INT((int)ui::HAlign::Right, (int)h);

  ui::VAlign v = ui::VAlign::Top;
  TEST_ASSERT_TRUE(ui::valign_from_string("middle", &v));
  TEST_ASSERT_EQUAL_INT((int)ui::VAlign::Middle, (int)v);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_rect_intersect);
  RUN_TEST(test_rect_unite);
  RUN_TEST(test_rect_contains);
  RUN_TEST(test_anchor_rect_center);
  RUN_TEST(test_anchor_rect_offsets);
  RUN_TEST(test_merge_rects);
  RUN_TEST(test_align_strings);
  return UNITY_END();
}
