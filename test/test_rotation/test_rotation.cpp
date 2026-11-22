#include <unity.h>

#include <vector>

#include "ui/canvas.hpp"
#include "ui/rotation.hpp"

void setUp(void) {}
void tearDown(void) {}

namespace {

constexpr int kW = 3;
constexpr int kH = 2;
constexpr int kStride = 1;

struct Surface {
  std::vector<uint8_t> bytes = std::vector<uint8_t>(8, 0xFF);
  ui::Canvas canvas{bytes.data(), kW, kH, kStride};
  Surface() { canvas.clear(ui::Color::White); }
};

struct Rotated {
  std::vector<uint8_t> bytes = std::vector<uint8_t>(8, 0xFF);
  ui::Canvas canvas{bytes.data(), kH, kW, kStride};
  Rotated() { canvas.clear(ui::Color::White); }
};

// A single pixel at the top-left: CW puts it top-right, CCW bottom-left, so a
// wrong direction cannot pass.
void paint(Surface &s) { s.canvas.plot(0, 0, ui::Color::Black); }

std::string row(const ui::Canvas &c, int y) {
  std::string out;
  for (int x = 0; x < c.width(); ++x) {
    out.push_back(c.test(x, y) ? '#' : '.');
  }
  return out;
}

} // namespace

void test_rotated_size(void) {
  TEST_ASSERT_TRUE(ui::rotated_size({480, 648}, ui::Rotation::None) ==
                   (ui::Size{480, 648}));
  TEST_ASSERT_TRUE(ui::rotated_size({480, 648}, ui::Rotation::Cw90) ==
                   (ui::Size{648, 480}));
  TEST_ASSERT_TRUE(ui::rotated_size({480, 648}, ui::Rotation::Ccw90) ==
                   (ui::Size{648, 480}));
}

void test_rotated_rect_cw(void) {
  // Logical 480x648. A 100x60 cell at (10, 20) becomes 60 wide, 100 tall in
  // the 648x480 panel, at x = 648 - 20 - 60 = 568.
  const ui::Rect panel =
      ui::rotated_rect({10, 20, 100, 60}, {480, 648}, ui::Rotation::Cw90);
  TEST_ASSERT_TRUE(panel == (ui::Rect{568, 10, 60, 100}));

  // Full frame maps to the full panel.
  TEST_ASSERT_TRUE(ui::rotated_rect({0, 0, 480, 648}, {480, 648},
                                    ui::Rotation::Cw90) ==
                   (ui::Rect{0, 0, 648, 480}));
}

void test_rotated_rect_ccw(void) {
  // x' = y, y' = 480 - 10 - 100 = 370.
  const ui::Rect panel =
      ui::rotated_rect({10, 20, 100, 60}, {480, 648}, ui::Rotation::Ccw90);
  TEST_ASSERT_TRUE(panel == (ui::Rect{20, 370, 60, 100}));

  TEST_ASSERT_TRUE(ui::rotated_rect({0, 0, 480, 648}, {480, 648},
                                    ui::Rotation::Ccw90) ==
                   (ui::Rect{0, 0, 648, 480}));
}

void test_rotate_canvas_cw(void) {
  Surface src;
  paint(src);
  Rotated dst;

  TEST_ASSERT_TRUE(ui::rotate_canvas(src.canvas, dst.canvas, ui::Rotation::Cw90));

  // Clockwise: the old top-left becomes the new top-right.
  TEST_ASSERT_EQUAL_STRING(".#", row(dst.canvas, 0).c_str());
  TEST_ASSERT_EQUAL_STRING("..", row(dst.canvas, 1).c_str());
  TEST_ASSERT_EQUAL_STRING("..", row(dst.canvas, 2).c_str());
}

void test_rotate_canvas_ccw(void) {
  Surface src;
  paint(src);
  Rotated dst;

  TEST_ASSERT_TRUE(
      ui::rotate_canvas(src.canvas, dst.canvas, ui::Rotation::Ccw90));

  // Counter-clockwise: the old top-left becomes the new bottom-left.
  TEST_ASSERT_EQUAL_STRING("..", row(dst.canvas, 0).c_str());
  TEST_ASSERT_EQUAL_STRING("..", row(dst.canvas, 1).c_str());
  TEST_ASSERT_EQUAL_STRING("#.", row(dst.canvas, 2).c_str());
}

void test_rotate_round_trip(void) {
  Surface src;
  paint(src);

  Rotated once;
  TEST_ASSERT_TRUE(ui::rotate_canvas(src.canvas, once.canvas, ui::Rotation::Cw90));

  Surface back;
  TEST_ASSERT_TRUE(
      ui::rotate_canvas(once.canvas, back.canvas, ui::Rotation::Ccw90));

  TEST_ASSERT_EQUAL_UINT32(src.canvas.hash(), back.canvas.hash());
}

void test_rotate_rejects_wrong_destination(void) {
  Surface src;
  Surface wrong; // 3x2, but CW needs 2x3
  TEST_ASSERT_FALSE(ui::rotate_canvas(src.canvas, wrong.canvas, ui::Rotation::Cw90));
}

void test_full_frame_sizes(void) {
  std::vector<uint8_t> logical_bytes(60 * 648, 0xFF);
  std::vector<uint8_t> panel_bytes(81 * 480, 0xFF);
  ui::Canvas logical(logical_bytes.data(), 480, 648, 60);
  ui::Canvas panel(panel_bytes.data(), 648, 480, 81);
  logical.clear(ui::Color::White);
  panel.clear(ui::Color::White);

  // One black pixel near the top-left of the logical frame.
  logical.plot(0, 0, ui::Color::Black);
  TEST_ASSERT_TRUE(ui::rotate_canvas(logical, panel, ui::Rotation::Cw90));
  TEST_ASSERT_TRUE(panel.test(647, 0));

  // And it maps back through rotated_rect.
  const ui::Rect mapped = ui::rotated_rect({0, 0, 1, 1}, {480, 648},
                                           ui::Rotation::Cw90);
  TEST_ASSERT_TRUE(mapped == (ui::Rect{647, 0, 1, 1}));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_rotated_size);
  RUN_TEST(test_rotated_rect_cw);
  RUN_TEST(test_rotated_rect_ccw);
  RUN_TEST(test_rotate_canvas_cw);
  RUN_TEST(test_rotate_canvas_ccw);
  RUN_TEST(test_rotate_round_trip);
  RUN_TEST(test_rotate_rejects_wrong_destination);
  RUN_TEST(test_full_frame_sizes);
  return UNITY_END();
}
