#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

#include "ui/canvas.hpp"
#include "ui/types.hpp"

namespace test {

// A tiny synthetic font so pixel tests do not depend on Cozette metrics:
//   ' ' advance 2, no ink
//   'A' 2x2 solid block, advance 3
//   'B' 1x1 dot at the glyph origin, advance 2
//   AE  (U+00C6) 2x2 solid block, advance 3
inline const ui::Glyph *test_glyphs() {
  static const ui::Glyph glyphs[] = {
      {' ', 0, 0, 0, 0, 2, 0},
      {'A', 2, 2, 0, 0, 3, 0},
      {'B', 1, 1, 0, 0, 2, 2},
      {0x00C6, 2, 2, 0, 0, 3, 3},
  };
  return glyphs;
}

inline const ui::Font &test_font() {
  static const uint8_t bitmap[] = {
      0xC0, 0xC0, // A rows
      0x80,       // B
      0xC0, 0xC0, // AE rows
  };
  static const ui::Font font = {"test", 2, 2, 0, 2, test_glyphs(), 4, bitmap};
  return font;
}

inline ui::Rect rect_of(const ui::Canvas &canvas) { return canvas.bounds(); }

} // namespace test
