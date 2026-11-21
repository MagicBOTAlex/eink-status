#pragma once

#include <cstdint>
#include <string>

namespace ui {

// One rasterised glyph. `xoff` is the ink offset from the pen position and
// `yoff` from the top of the line box, both in pixels, so a renderer only has
// to add and blit. Bitmaps are 1 bpp, MSB-first, one row per `(width+7)/8`
// bytes, with a set bit meaning black.
struct Glyph {
  uint32_t codepoint;
  uint8_t width;
  uint8_t height;
  int8_t xoff;
  int8_t yoff;
  uint8_t advance;
  uint16_t offset;
};

struct Font {
  const char *name;
  uint8_t line_height;
  uint8_t ascent;
  uint8_t descent;
  uint8_t size;
  const Glyph *glyphs;
  uint16_t count;
  const uint8_t *bitmap;
};

const Glyph *font_glyph(const Font &font, uint32_t codepoint);
bool font_has_glyph(const Font &font, uint32_t codepoint);

// Decodes one UTF-8 code point and advances `cursor`. Returns 0xFFFD for a
// malformed sequence (and advances one byte) so rendering never hangs.
uint32_t utf8_next(const char *&cursor);
uint32_t utf8_next(const std::string &text, size_t &index);
std::string utf8_encode(uint32_t codepoint);

int font_text_width(const Font &font, const std::string &text);
int font_text_width(const Font &font, const char *text);

// Number of code points, which is what `font_text_width` iterates over.
size_t utf8_length(const std::string &text);

} // namespace ui
