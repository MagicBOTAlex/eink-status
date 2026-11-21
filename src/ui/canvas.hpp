#pragma once

#include <cstdint>
#include <string>

#include "ui/font.hpp"
#include "ui/types.hpp"

namespace ui {

// A 1 bpp drawing surface: bit 0 = black, bit 1 = white. This is exactly the
// panel RAM layout, so the device streams canvas bytes straight to the
// controller and host tests inspect the same bytes.
class Canvas {
public:
  Canvas(uint8_t *pixels, int width, int height, int stride);

  int width() const { return width_; }
  int height() const { return height_; }
  int stride() const { return stride_; }
  uint8_t *pixels() { return pixels_; }
  const uint8_t *pixels() const { return pixels_; }
  size_t byte_size() const { return static_cast<size_t>(stride_) * height_; }
  Rect bounds() const { return {0, 0, width_, height_}; }

  void clear(Color color = Color::White);
  void fill_rect(const Rect &rect, Color color);
  void stroke_rect(const Rect &rect, Color color, int thickness = 1);

  // Draws `text` inside `box`, aligned horizontally by `halign` and vertically
  // by `valign` using the font's line box. Glyphs are clipped to the canvas.
  void draw_text(const Font &font, const Rect &box, const std::string &text,
                 HAlign halign = HAlign::Left, VAlign valign = VAlign::Top,
                 Color color = Color::Black);

  // The box draw_text will paint into: the font line box placed inside `box`.
  static Rect text_box(const Font &font, const Rect &box, const std::string &text,
                       HAlign halign, VAlign valign);

  void blit(const uint8_t *bitmap, int bw, int bh, int x, int y, Color color);

  // true = black. Out-of-range reads return false (white).
  bool test(int x, int y) const;
  void plot(int x, int y, Color color);

  // FNV-1a over the whole buffer: cheap golden value for snapshot tests.
  uint32_t hash() const;

  bool region_equal(const Canvas &other, const Rect &region) const;

  // ASCII art of a region, one character per `scale` x `scale` block. Used by
  // the preview tool and by failing tests.
  std::string to_ascii(const Rect &region, int scale = 2) const;

private:
  uint8_t *pixels_;
  int width_;
  int height_;
  int stride_;
};

} // namespace ui
