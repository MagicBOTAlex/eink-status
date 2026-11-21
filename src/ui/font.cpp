#include "ui/font.hpp"

namespace ui {

const Glyph *font_glyph(const Font &font, uint32_t codepoint) {
  for (uint16_t i = 0; i < font.count; ++i) {
    if (font.glyphs[i].codepoint == codepoint) {
      return &font.glyphs[i];
    }
  }
  return nullptr;
}

bool font_has_glyph(const Font &font, uint32_t codepoint) {
  return font_glyph(font, codepoint) != nullptr;
}

uint32_t utf8_next(const char *&cursor) {
  const auto *p = reinterpret_cast<const unsigned char *>(cursor);
  if (*p == 0) {
    return 0;
  }

  uint32_t cp = 0xFFFD;
  int extra = 0;

  if (*p < 0x80) {
    cp = *p;
    extra = 0;
  } else if ((*p & 0xE0) == 0xC0) {
    cp = *p & 0x1F;
    extra = 1;
  } else if ((*p & 0xF0) == 0xE0) {
    cp = *p & 0x0F;
    extra = 2;
  } else if ((*p & 0xF8) == 0xF0) {
    cp = *p & 0x07;
    extra = 3;
  }

  if (extra == 0 && *p >= 0x80) {
    ++cursor;
    return 0xFFFD;
  }

  for (int i = 1; i <= extra; ++i) {
    if ((p[i] & 0xC0) != 0x80) {
      ++cursor;
      return 0xFFFD;
    }
    cp = (cp << 6) | (p[i] & 0x3F);
  }

  cursor += 1 + extra;
  return cp;
}

uint32_t utf8_next(const std::string &text, size_t &index) {
  const char *p = text.c_str() + index;
  const char *before = p;
  const uint32_t cp = utf8_next(p);
  index += static_cast<size_t>(p - before);
  if (index > text.size()) {
    index = text.size();
  }
  return cp;
}

std::string utf8_encode(uint32_t codepoint) {
  std::string out;
  if (codepoint < 0x80) {
    out.push_back(static_cast<char>(codepoint));
  } else if (codepoint < 0x800) {
    out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else if (codepoint < 0x10000) {
    out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  }
  return out;
}

int font_text_width(const Font &font, const std::string &text) {
  int width = 0;
  size_t i = 0;
  while (i < text.size()) {
    const uint32_t cp = utf8_next(text, i);
    const Glyph *glyph = font_glyph(font, cp);
    width += glyph ? glyph->advance : font.size / 2;
  }
  return width;
}

int font_text_width(const Font &font, const char *text) {
  return font_text_width(font, std::string(text ? text : ""));
}

size_t utf8_length(const std::string &text) {
  size_t count = 0;
  size_t i = 0;
  while (i < text.size()) {
    utf8_next(text, i);
    ++count;
  }
  return count;
}

} // namespace ui
