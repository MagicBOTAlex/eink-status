#pragma once

#include "ui/canvas.hpp"
#include "ui/types.hpp"

namespace ui {

// The panel can be mounted sideways. The UI is laid out in a logical canvas
// (480x648 in this project) and rotated into the panel's native 648x480 frame
// just before streaming, so pages never have to think about orientation.
enum class Rotation : uint8_t {
  None,
  Cw90,   // rotate the image clockwise
  Ccw90,  // rotate the image counter-clockwise
};

Size rotated_size(Size size, Rotation rotation);

// Maps a logical rect into panel coordinates. Used so a dirty region in page
// space becomes the right partial-refresh window.
Rect rotated_rect(const Rect &rect, Size logical_size, Rotation rotation);

// Copies `src` into `dst`, rotated. `dst` must be sized rotated_size(src).
bool rotate_canvas(const Canvas &src, Canvas &dst, Rotation rotation);

} // namespace ui
