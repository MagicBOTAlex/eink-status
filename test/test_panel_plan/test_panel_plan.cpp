#include <unity.h>

#include <vector>

#include "panel_plan.hpp"
#include "ui/types.hpp"

void setUp(void) {}
void tearDown(void) {}

namespace {

constexpr long kFullArea = 648L * 480L / 2;

} // namespace

// The bug that hid for a whole session: ui::Rect is (x, y, w, h) but the panel
// window takes inclusive edges.
void test_to_window_uses_inclusive_edges(void) {
  const panel::Window counter = panel::to_window({284, 275, 80, 20});
  TEST_ASSERT_TRUE(counter == (panel::Window{284, 275, 363, 294}));

  const panel::Window single = panel::to_window({5, 6, 1, 1});
  TEST_ASSERT_TRUE(single == (panel::Window{5, 6, 5, 6}));

  // An empty rect maps to an explicitly invalid window, not a 1x1 one.
  const panel::Window empty = panel::to_window({5, 6, 0, 4});
  TEST_ASSERT_TRUE(empty == (panel::Window{0, 0, -1, -1}));
  TEST_ASSERT_FALSE(panel::valid(empty));
}

void test_valid_rejects_inverted_windows(void) {
  TEST_ASSERT_TRUE(panel::valid({0, 0, 647, 479}));
  TEST_ASSERT_FALSE(panel::valid({284, 275, 80, 20}));
  TEST_ASSERT_FALSE(panel::valid({10, 20, 5, 19}));
}

void test_choose(void) {
  TEST_ASSERT_EQUAL_INT((int)panel::Action::None,
                        (int)panel::choose({}, 4, kFullArea));

  const std::vector<ui::Rect> one{{0, 0, 10, 10}};
  TEST_ASSERT_EQUAL_INT((int)panel::Action::Partial,
                        (int)panel::choose(one, 4, kFullArea));

  // Exactly at the region cap is still partial.
  const std::vector<ui::Rect> four(4, ui::Rect{0, 0, 1, 1});
  TEST_ASSERT_EQUAL_INT((int)panel::Action::Partial,
                        (int)panel::choose(four, 4, kFullArea));

  const std::vector<ui::Rect> five(5, ui::Rect{0, 0, 1, 1});
  TEST_ASSERT_EQUAL_INT((int)panel::Action::Full,
                        (int)panel::choose(five, 4, kFullArea));

  // Exactly at the area limit is partial, one pixel over is full.
  const std::vector<ui::Rect> at_limit{{0, 0, 300, 518}};
  TEST_ASSERT_EQUAL_INT((int)panel::Action::Partial,
                        (int)panel::choose(at_limit, 4, 300L * 518));
  const std::vector<ui::Rect> over_limit{{0, 0, 300, 519}};
  TEST_ASSERT_EQUAL_INT((int)panel::Action::Full,
                        (int)panel::choose(over_limit, 4, 300L * 518));

  // Areas accumulate across regions.
  const std::vector<ui::Rect> split{{0, 0, 200, 300}, {200, 0, 200, 300}};
  TEST_ASSERT_EQUAL_INT((int)panel::Action::Full,
                        (int)panel::choose(split, 4, 100000));
}

void test_due_is_wrap_safe(void) {
  TEST_ASSERT_TRUE(panel::due(5000, 0, 5000));
  TEST_ASSERT_FALSE(panel::due(4999, 0, 5000));

  // Crossing the uint32 wrap must still report one elapsed period.
  const uint32_t last = 0xFFFFF000u;
  const uint32_t now = 0x00001000u; // 0x2000 later
  TEST_ASSERT_TRUE(panel::due(now, last, 0x2000));
  TEST_ASSERT_FALSE(panel::due(now, last, 0x2001));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_to_window_uses_inclusive_edges);
  RUN_TEST(test_valid_rejects_inverted_windows);
  RUN_TEST(test_choose);
  RUN_TEST(test_due_is_wrap_safe);
  return UNITY_END();
}
