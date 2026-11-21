#include "ui/canvas.hpp"

#include <algorithm>
#include <cstring>

namespace ui {
namespace {

inline uint8_t mask_for(int bit) { return static_cast<uint8_t>(0x80u >> bit); }

} // namespace

Canvas::Canvas(uint8_t *pixels, int width, int height, int stride)
    : pixels_(pixels), width_(width), height_(height), stride_(stride),
      clip_{0, 0, width, height} {}

void Canvas::set_clip(const Rect &clip) {
  clip_ = clip.intersect({0, 0, width_, height_});
}

void Canvas::clear(Color color) {
  if (color == Color::Invert) {
    for (size_t i = 0; i < byte_size(); ++i) {
      pixels_[i] = static_cast<uint8_t>(~pixels_[i]);
    }
    return;
  }
  std::memset(pixels_, color == Color::White ? 0xFF : 0x00, byte_size());
}

void Canvas::plot(int x, int y, Color color) {
  if (!clip_.contains(x, y)) {
    return;
  }
  uint8_t *byte = pixels_ + static_cast<size_t>(y) * stride_ + (x >> 3);
  const uint8_t mask = mask_for(x & 7);
  switch (color) {
  case Color::Black:
    *byte = static_cast<uint8_t>(*byte & ~mask);
    break;
  case Color::White:
    *byte = static_cast<uint8_t>(*byte | mask);
    break;
  case Color::Invert:
    *byte = static_cast<uint8_t>(*byte ^ mask);
    break;
  }
}

void Canvas::fill_rect(const Rect &rect, Color color) {
  const Rect b = rect.intersect(clip_);
  if (b.empty()) {
    return;
  }

  const uint8_t fill = color == Color::White ? 0xFF : 0x00;

  for (int y = b.y; y < b.bottom(); ++y) {
    uint8_t *row = pixels_ + static_cast<size_t>(y) * stride_;
    int x = b.x;

    if (color == Color::Invert) {
      for (; x < b.right(); ++x) {
        row[x >> 3] ^= mask_for(x & 7);
      }
      continue;
    }

    // Leading partial byte.
    for (; x < b.right() && (x & 7) != 0; ++x) {
      const uint8_t mask = mask_for(x & 7);
      row[x >> 3] = fill ? static_cast<uint8_t>(row[x >> 3] | mask)
                         : static_cast<uint8_t>(row[x >> 3] & ~mask);
    }

    // Whole bytes.
    const int whole = (b.right() - x) / 8;
    if (whole > 0) {
      std::memset(row + (x >> 3), fill, static_cast<size_t>(whole));
      x += whole * 8;
    }

    // Trailing bits.
    for (; x < b.right(); ++x) {
      const uint8_t mask = mask_for(x & 7);
      row[x >> 3] = fill ? static_cast<uint8_t>(row[x >> 3] | mask)
                         : static_cast<uint8_t>(row[x >> 3] & ~mask);
    }
  }
}

void Canvas::stroke_rect(const Rect &rect, Color color, int thickness) {
  if (rect.empty() || thickness <= 0) {
    return;
  }
  // Whole band if the border would cover the rect.
  if (thickness * 2 >= rect.w || thickness * 2 >= rect.h) {
    fill_rect(rect, color);
    return;
  }
  // Paint each side exactly once: with Color::Invert, overlapping sides would
  // cancel out at the corners.
  fill_rect({rect.x, rect.y, rect.w, thickness}, color);
  fill_rect({rect.x, rect.bottom() - thickness, rect.w, thickness}, color);
  fill_rect({rect.x, rect.y + thickness, thickness, rect.h - 2 * thickness},
            color);
  fill_rect({rect.right() - thickness, rect.y + thickness, thickness,
             rect.h - 2 * thickness},
            color);
}

Rect Canvas::text_box(const Font &font, const Rect &box,
                      const std::string &text, HAlign halign, VAlign valign) {
  const int tw = font_text_ink_width(font, text);
  const int th = font.line_height;

  int x = box.x;
  if (halign == HAlign::Center) {
    x = box.x + (box.w - tw) / 2;
  } else if (halign == HAlign::Right) {
    x = box.x + box.w - tw;
  }

  int y = box.y;
  if (valign == VAlign::Middle) {
    y = box.y + (box.h - th) / 2;
  } else if (valign == VAlign::Bottom) {
    y = box.y + box.h - th;
  }

  return {x, y, tw, th};
}

void Canvas::draw_text(const Font &font, const Rect &box,
                       const std::string &text, HAlign halign, VAlign valign,
                       Color color) {
  const Rect line = text_box(font, box, text, halign, valign);
  int pen_x = line.x;

  size_t i = 0;
  while (i < text.size()) {
    const uint32_t cp = utf8_next(text, i);
    const Glyph *glyph = font_glyph(font, cp);
    if (glyph == nullptr) {
      pen_x += font.size / 2;
      continue;
    }
    if (glyph->width && glyph->height) {
      const int stride = (glyph->width + 7) / 8;
      blit(font.bitmap + glyph->offset, stride * 8, glyph->height,
           pen_x + glyph->xoff, line.y + glyph->yoff, color);
    }
    pen_x += glyph->advance;
  }
}

void Canvas::blit(const uint8_t *bitmap, int bw, int bh, int x, int y,
                  Color color) {
  const int stride = bw / 8;
  for (int row = 0; row < bh; ++row) {
    const int py = y + row;
    if (py < 0 || py >= height_) {
      continue;
    }
    const uint8_t *src = bitmap + static_cast<size_t>(row) * stride;
    for (int col = 0; col < stride * 8; ++col) {
      if ((src[col >> 3] & mask_for(col & 7)) == 0) {
        continue;
      }
      plot(x + col, py, color);
    }
  }
}

bool Canvas::test(int x, int y) const {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) {
    return false;
  }
  const uint8_t byte =
      pixels_[static_cast<size_t>(y) * stride_ + (x >> 3)];
  return (byte & mask_for(x & 7)) == 0;
}

uint32_t Canvas::hash() const {
  uint32_t h = 2166136261u;
  const uint8_t *p = pixels_;
  for (size_t i = 0; i < byte_size(); ++i) {
    h = (h ^ p[i]) * 16777619u;
  }
  return h;
}

bool Canvas::region_equal(const Canvas &other, const Rect &region) const {
  const Rect b = region.intersect(bounds()).intersect(other.bounds());
  if (b.empty()) {
    return true;
  }

  for (int y = b.y; y < b.bottom(); ++y) {
    const uint8_t *a = pixels_ + static_cast<size_t>(y) * stride_;
    const uint8_t *c = other.pixels_ + static_cast<size_t>(y) * other.stride_;
    int x = b.x;
    while (x < b.right()) {
      const int bit = x & 7;
      const int span = std::min(8 - bit, b.right() - x);
      const uint8_t mask = static_cast<uint8_t>(
          static_cast<uint8_t>(0xFFu >> bit) &
          static_cast<uint8_t>(0xFFu << (8 - bit - span)));
      if ((a[x >> 3] & mask) != (c[x >> 3] & mask)) {
        return false;
      }
      x += span;
    }
  }
  return true;
}

std::string Canvas::to_ascii(const Rect &region, int scale) const {
  if (scale < 1) {
    scale = 1;
  }
  const Rect b = region.intersect(bounds());
  std::string out;
  if (b.empty()) {
    return out;
  }
  out.reserve(static_cast<size_t>((b.h / scale + 1) * (b.w / scale + 2)));
  for (int y = b.y; y < b.bottom(); y += scale) {
    for (int x = b.x; x < b.right(); x += scale) {
      bool any = false;
      for (int dy = 0; dy < scale && !any; ++dy) {
        for (int dx = 0; dx < scale && !any; ++dx) {
          any = test(x + dx, y + dy);
        }
      }
      out.push_back(any ? '#' : '.');
    }
    out.push_back('\n');
  }
  return out;
}

} // namespace ui
