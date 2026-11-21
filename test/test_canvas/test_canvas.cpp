#include <unity.h>

#include <cstring>
#include <vector>

#include "common/test_helpers.hpp"
#include "ui/canvas.hpp"

void setUp(void) {}
void tearDown(void) {}

namespace {

constexpr int kW = 40;
constexpr int kH = 20;
constexpr int kStride = (kW + 7) / 8;

struct Surface {
  std::vector<uint8_t> bytes =
      std::vector<uint8_t>(static_cast<size_t>(kStride) * kH, 0xFF);
  ui::Canvas canvas{bytes.data(), kW, kH, kStride};

  Surface() { canvas.clear(ui::Color::White); }
};

int black_pixels(const ui::Canvas &canvas, const ui::Rect &rect) {
  int count = 0;
  for (int y = rect.y; y < rect.bottom(); ++y) {
    for (int x = rect.x; x < rect.right(); ++x) {
      if (canvas.test(x, y)) {
        ++count;
      }
    }
  }
  return count;
}

} // namespace

void test_clear_and_hash(void) {
  Surface a;
  TEST_ASSERT_EQUAL_INT(0, black_pixels(a.canvas, a.canvas.bounds()));

  const uint32_t white = a.canvas.hash();
  a.canvas.fill_rect({0, 0, 1, 1}, ui::Color::Black);
  TEST_ASSERT_TRUE(white != a.canvas.hash());

  a.canvas.clear(ui::Color::White);
  TEST_ASSERT_EQUAL_UINT32(white, a.canvas.hash());

  a.canvas.clear(ui::Color::Black);
  TEST_ASSERT_EQUAL_INT(kW * kH, black_pixels(a.canvas, a.canvas.bounds()));
}

void test_fill_rect_unaligned(void) {
  Surface s;
  s.canvas.fill_rect({3, 2, 5, 3}, ui::Color::Black);

  TEST_ASSERT_EQUAL_INT(15, black_pixels(s.canvas, s.canvas.bounds()));
  TEST_ASSERT_TRUE(s.canvas.test(3, 2));
  TEST_ASSERT_TRUE(s.canvas.test(7, 4));
  TEST_ASSERT_FALSE(s.canvas.test(2, 2)); // left of the rect
  TEST_ASSERT_FALSE(s.canvas.test(8, 2)); // right of the rect
  TEST_ASSERT_FALSE(s.canvas.test(3, 5)); // below the rect
}

void test_fill_rect_clips(void) {
  Surface s;
  s.canvas.fill_rect({-5, -5, 10, 10}, ui::Color::Black);
  TEST_ASSERT_EQUAL_INT(25, black_pixels(s.canvas, s.canvas.bounds()));
  TEST_ASSERT_TRUE(s.canvas.test(0, 0));
  TEST_ASSERT_TRUE(s.canvas.test(4, 4));
  TEST_ASSERT_FALSE(s.canvas.test(5, 5));
}

void test_stroke_rect(void) {
  Surface s;
  s.canvas.stroke_rect({2, 2, 8, 6}, ui::Color::Black);
  TEST_ASSERT_TRUE(s.canvas.test(2, 2));
  TEST_ASSERT_TRUE(s.canvas.test(9, 2));
  TEST_ASSERT_TRUE(s.canvas.test(2, 7));
  TEST_ASSERT_TRUE(s.canvas.test(9, 7));
  TEST_ASSERT_FALSE(s.canvas.test(5, 4)); // interior stays white
  TEST_ASSERT_EQUAL_INT(2 * 8 + 2 * 4, black_pixels(s.canvas, {0, 0, kW, kH}));
}

void test_text_positions(void) {
  Surface s;
  s.canvas.draw_text(test::test_font(), {0, 0, 20, 4}, "AB");
  // 'A' is a 2x2 block at the pen origin.
  TEST_ASSERT_TRUE(s.canvas.test(0, 0));
  TEST_ASSERT_TRUE(s.canvas.test(1, 1));
  // 'B' is a single pixel after a 3 px advance.
  TEST_ASSERT_TRUE(s.canvas.test(3, 0));
  TEST_ASSERT_FALSE(s.canvas.test(4, 0));
}

void test_text_alignment(void) {
  Surface s;
  // "AB" is 5 px wide and the line box is 2 px high inside a 20x10 box.
  s.canvas.draw_text(test::test_font(), {10, 0, 20, 10}, "AB",
                     ui::HAlign::Center, ui::VAlign::Middle);
  TEST_ASSERT_TRUE(s.canvas.test(17, 4));
  TEST_ASSERT_TRUE(s.canvas.test(18, 5));
  TEST_ASSERT_TRUE(s.canvas.test(20, 4)); // 'B' at 17 + 3
  TEST_ASSERT_FALSE(s.canvas.test(16, 4));

  Surface right;
  right.canvas.draw_text(test::test_font(), {10, 0, 20, 10}, "AB",
                         ui::HAlign::Right, ui::VAlign::Bottom);
  TEST_ASSERT_TRUE(right.canvas.test(25, 8)); // 30 - 5 + 0, 10 - 2
}

void test_text_clips_at_edges(void) {
  Surface s;
  s.canvas.draw_text(test::test_font(), {-1, -1, 20, 4}, "A");
  // Only the bottom-right pixel of the 2x2 block is on-canvas.
  TEST_ASSERT_TRUE(s.canvas.test(0, 0));
  TEST_ASSERT_FALSE(s.canvas.test(1, 1));
}

void test_region_equal(void) {
  Surface a;
  Surface b;
  TEST_ASSERT_TRUE(a.canvas.region_equal(b.canvas, a.canvas.bounds()));

  a.canvas.fill_rect({10, 10, 2, 2}, ui::Color::Black);
  TEST_ASSERT_FALSE(a.canvas.region_equal(b.canvas, a.canvas.bounds()));
  TEST_ASSERT_TRUE(a.canvas.region_equal(b.canvas, {0, 0, 10, 20}));
}

void test_to_ascii(void) {
  Surface s;
  s.canvas.fill_rect({0, 0, 1, 1}, ui::Color::Black);
  TEST_ASSERT_EQUAL_STRING("#.\n..\n", s.canvas.to_ascii({0, 0, 2, 2}, 1).c_str());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_clear_and_hash);
  RUN_TEST(test_fill_rect_unaligned);
  RUN_TEST(test_fill_rect_clips);
  RUN_TEST(test_stroke_rect);
  RUN_TEST(test_text_positions);
  RUN_TEST(test_text_alignment);
  RUN_TEST(test_text_clips_at_edges);
  RUN_TEST(test_region_equal);
  RUN_TEST(test_to_ascii);
  return UNITY_END();
}
