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

// 13 px wide: the last byte is only partly used, which is where mask bugs hide.
struct Narrow {
  static constexpr int w = 13;
  static constexpr int h = 5;
  static constexpr int stride = 2;
  std::vector<uint8_t> bytes = std::vector<uint8_t>(stride * h, 0xFF);
  ui::Canvas canvas{bytes.data(), w, h, stride};

  Narrow() { canvas.clear(ui::Color::White); }
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

  a.canvas.clear(ui::Color::Invert);
  TEST_ASSERT_EQUAL_INT(0, black_pixels(a.canvas, a.canvas.bounds()));
  TEST_ASSERT_EQUAL_UINT32(white, a.canvas.hash());
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

// A 13 px wide surface: writing the last 3 pixels must not touch the padding
// bits of the last byte, and region_equal must ignore them too.
void test_narrow_surface_edge_bytes(void) {
  Narrow n;
  n.canvas.fill_rect({10, 0, 3, 1}, ui::Color::Black);
  TEST_ASSERT_TRUE(n.canvas.test(10, 0));
  TEST_ASSERT_TRUE(n.canvas.test(12, 0));
  TEST_ASSERT_FALSE(n.canvas.test(9, 0));
  // Byte 1 holds pixels 8..15; x = 10..12 clear bits 5..3, leaving 0xC7. The
  // padding pixels 13..15 (bits 2..0) must stay white.
  TEST_ASSERT_EQUAL_UINT8(0xC7, n.bytes[1]);
  TEST_ASSERT_EQUAL_UINT8(0x07, n.bytes[1] & 0x07);
  TEST_ASSERT_EQUAL_UINT8(0xFF, n.bytes[3]); // row 1 untouched

  Narrow other;
  TEST_ASSERT_FALSE(n.canvas.region_equal(other.canvas, {12, 0, 1, 1}));
  // Pixels 0..9 are white in both, so that region compares equal.
  TEST_ASSERT_TRUE(n.canvas.region_equal(other.canvas, {0, 0, 10, 1}));

  // Aligned run exercises the swept memset path.
  Narrow aligned;
  aligned.canvas.fill_rect({8, 2, 5, 1}, ui::Color::Black);
  TEST_ASSERT_EQUAL_INT(5, black_pixels(aligned.canvas, aligned.canvas.bounds()));
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

void test_stroke_rect_invert_touches_corners_once(void) {
  Surface s;
  s.canvas.stroke_rect({2, 2, 8, 8}, ui::Color::Invert);
  // Every border pixel was white; exactly one inversion makes it black, so the
  // corners must not cancel out.
  TEST_ASSERT_TRUE(s.canvas.test(2, 2));
  TEST_ASSERT_TRUE(s.canvas.test(9, 9));
  TEST_ASSERT_TRUE(s.canvas.test(2, 5)); // left edge
  TEST_ASSERT_TRUE(s.canvas.test(5, 2)); // top edge
  TEST_ASSERT_FALSE(s.canvas.test(5, 5)); // interior untouched
}

void test_stroke_rect_thick(void) {
  Surface s;
  s.canvas.stroke_rect({1, 1, 10, 8}, ui::Color::Black, 2);
  TEST_ASSERT_TRUE(s.canvas.test(1, 1));
  TEST_ASSERT_TRUE(s.canvas.test(2, 2));
  TEST_ASSERT_TRUE(s.canvas.test(8, 7));
  TEST_ASSERT_FALSE(s.canvas.test(3, 3)); // interior
  TEST_ASSERT_EQUAL_INT(2 * 10 * 2 + 2 * (8 - 4) * 2,
                        black_pixels(s.canvas, s.canvas.bounds()));
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

void test_clip_limits_drawing(void) {
  Surface s;
  s.canvas.set_clip({5, 5, 10, 4});
  s.canvas.fill_rect({0, 0, 40, 20}, ui::Color::Black);
  TEST_ASSERT_EQUAL_INT(40, black_pixels(s.canvas, s.canvas.bounds()));
  TEST_ASSERT_TRUE(s.canvas.test(5, 5));
  TEST_ASSERT_TRUE(s.canvas.test(14, 8));
  TEST_ASSERT_FALSE(s.canvas.test(4, 5));
  TEST_ASSERT_FALSE(s.canvas.test(15, 5));

  s.canvas.set_clip({-2, -2, 4, 4}); // intersected with the canvas
  s.canvas.clear(ui::Color::White);
  s.canvas.fill_rect({0, 0, 4, 4}, ui::Color::Black);
  TEST_ASSERT_EQUAL_INT(4, black_pixels(s.canvas, s.canvas.bounds()));

  s.canvas.set_clip({-5, -5, 3, 3}); // entirely off-canvas: nothing draws
  s.canvas.clear(ui::Color::White);
  s.canvas.fill_rect({0, 0, 4, 4}, ui::Color::Black);
  TEST_ASSERT_EQUAL_INT(0, black_pixels(s.canvas, s.canvas.bounds()));

  s.canvas.set_clip(s.canvas.bounds());
}

void test_blit_multibyte_glyph(void) {
  Surface s;
  // 9 px wide, MSB-first, stride 2: the second byte contributes one pixel.
  const uint8_t bitmap[] = {0xFF, 0x80};
  s.canvas.blit(bitmap, 16, 1, 3, 4, ui::Color::Black);
  TEST_ASSERT_TRUE(s.canvas.test(3, 4));
  TEST_ASSERT_TRUE(s.canvas.test(11, 4)); // 9th pixel
  TEST_ASSERT_FALSE(s.canvas.test(12, 4));
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
  RUN_TEST(test_narrow_surface_edge_bytes);
  RUN_TEST(test_stroke_rect);
  RUN_TEST(test_stroke_rect_invert_touches_corners_once);
  RUN_TEST(test_stroke_rect_thick);
  RUN_TEST(test_text_positions);
  RUN_TEST(test_text_alignment);
  RUN_TEST(test_text_clips_at_edges);
  RUN_TEST(test_clip_limits_drawing);
  RUN_TEST(test_blit_multibyte_glyph);
  RUN_TEST(test_region_equal);
  RUN_TEST(test_to_ascii);
  return UNITY_END();
}
