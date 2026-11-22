#include "ui/rotation.hpp"

#include <cstring>

namespace ui {

Size rotated_size(Size size, Rotation rotation) {
  switch (rotation) {
  case Rotation::None:
    return size;
  case Rotation::Cw90:
  case Rotation::Ccw90:
    return {size.h, size.w};
  }
  return size;
}

Rect rotated_rect(const Rect &rect, Size logical_size, Rotation rotation) {
  switch (rotation) {
  case Rotation::None:
    return rect;
  case Rotation::Cw90:
    // (x, y) -> (H-1-y, x)
    return {logical_size.h - rect.y - rect.h, rect.x, rect.h, rect.w};
  case Rotation::Ccw90:
    // (x, y) -> (y, W-1-x)
    return {rect.y, logical_size.w - rect.x - rect.w, rect.h, rect.w};
  }
  return rect;
}

bool rotate_canvas(const Canvas &src, Canvas &dst, Rotation rotation) {
  const Size want = rotated_size({src.width(), src.height()}, rotation);
  if (dst.width() != want.w || dst.height() != want.h) {
    return false;
  }

  if (rotation == Rotation::None) {
    for (int y = 0; y < dst.height(); ++y) {
      std::memcpy(dst.pixels() + static_cast<size_t>(y) * dst.stride(),
                  src.pixels() + static_cast<size_t>(y) * src.stride(),
                  static_cast<size_t>(dst.stride()));
    }
    return true;
  }

  const int src_w = src.width();
  const int src_h = src.height();

  for (int py = 0; py < dst.height(); ++py) {
    for (int px = 0; px < dst.width(); ++px) {
      int sx = 0;
      int sy = 0;
      if (rotation == Rotation::Cw90) {
        sx = py;
        sy = src_h - 1 - px;
      } else {
        sx = src_w - 1 - py;
        sy = px;
      }
      dst.plot(px, py, src.test(sx, sy) ? Color::Black : Color::White);
    }
  }
  return true;
}

} // namespace ui
