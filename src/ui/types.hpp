#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ui {

// A pixel is black when its bit is 0 and white when its bit is 1, matching the
// panel's RAM format. `Invert` flips the bits under the shape.
enum class Color : uint8_t { Black = 0, White = 1, Invert = 2 };

enum class Align : uint8_t {
  TopLeft,
  TopCenter,
  TopRight,
  CenterLeft,
  Center,
  CenterRight,
  BottomLeft,
  BottomCenter,
  BottomRight,
};

enum class HAlign : uint8_t { Left, Center, Right };
enum class VAlign : uint8_t { Top, Middle, Bottom };

struct Point {
  int x = 0;
  int y = 0;
};

struct Size {
  int w = 0;
  int h = 0;

  bool empty() const { return w <= 0 || h <= 0; }

  bool operator==(const Size &o) const { return w == o.w && h == o.h; }
  bool operator!=(const Size &o) const { return !(*this == o); }
};

struct Rect {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;

  int right() const { return x + w; }   // exclusive
  int bottom() const { return y + h; }  // exclusive
  bool empty() const { return w <= 0 || h <= 0; }
  long area() const { return empty() ? 0 : (long)w * h; }

  bool contains(int px, int py) const {
    return !empty() && px >= x && px < right() && py >= y && py < bottom();
  }
  bool contains(const Rect &other) const {
    return !other.empty() && other.x >= x && other.y >= y &&
           other.right() <= right() && other.bottom() <= bottom();
  }
  bool intersects(const Rect &other) const {
    return !empty() && !other.empty() && x < other.right() &&
           other.x < right() && y < other.bottom() && other.y < bottom();
  }

  Rect intersect(const Rect &other) const;
  Rect unite(const Rect &other) const;
  Rect inset(int dx, int dy) const { return {x + dx, y + dy, w - 2 * dx, h - 2 * dy}; }
  Rect offset(int dx, int dy) const { return {x + dx, y + dy, w, h}; }

  bool operator==(const Rect &o) const {
    return x == o.x && y == o.y && w == o.w && h == o.h;
  }
  bool operator!=(const Rect &o) const { return !(*this == o); }

  std::string to_string() const;
};

// Places a `size` rect inside `parent` according to `align`, then applies the
// (dx, dy) offset. The result is clamped to `parent` in size only; the caller
// decides what to do with out-of-bounds placement.
Rect anchor_rect(const Rect &parent, Size size, Align align, int dx = 0,
                 int dy = 0);

// Text alignment helpers, exposed because components and tests use them.
const char *to_string(Align align);
const char *to_string(HAlign align);
const char *to_string(VAlign align);
bool align_from_string(const std::string &text, Align *out);
bool halign_from_string(const std::string &text, HAlign *out);
bool valign_from_string(const std::string &text, VAlign *out);

// Merges rects that overlap or are closer than `gap` pixels, dropping empty
// ones. The result is sorted top-to-bottom then left-to-right so refresh order
// is deterministic.
std::vector<Rect> merge_rects(std::vector<Rect> rects, int gap = 0);

} // namespace ui
