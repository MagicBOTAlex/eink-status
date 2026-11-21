#include <unity.h>

#include <cstdint>
#include <string>
#include <vector>

#include "common/test_helpers.hpp"
#include "ui/font.hpp"
#include "ui/fonts/cozette_16.hpp"
#include "ui/fonts/cozette_96.hpp"

void setUp(void) {}
void tearDown(void) {}

void test_utf8_roundtrip_danish(void) {
  const uint32_t codepoints[] = {0x00C6, 0x00D8, 0x00C5, 0x00E6, 0x00F8, 0x00E5};
  for (uint32_t cp : codepoints) {
    const std::string encoded = ui::utf8_encode(cp);
    TEST_ASSERT_EQUAL_size_t(2, encoded.size());
    const char *cursor = encoded.c_str();
    TEST_ASSERT_EQUAL_UINT32(cp, ui::utf8_next(cursor));
    TEST_ASSERT_EQUAL_INT(0, *cursor); // consumed both bytes
  }
  TEST_ASSERT_EQUAL_STRING("\xC3\x86", ui::utf8_encode(0x00C6).c_str());
}

void test_utf8_malformed_is_replaced(void) {
  const char bad[] = {'A', static_cast<char>(0xFF), 0};
  const char *cursor = bad;
  TEST_ASSERT_EQUAL_UINT32('A', ui::utf8_next(cursor));
  TEST_ASSERT_EQUAL_UINT32(0xFFFD, ui::utf8_next(cursor));
  TEST_ASSERT_EQUAL_INT(0, *cursor);
}

// Overlong encodings, UTF-16 surrogates and values past U+10FFFF are rejected
// one byte at a time instead of being rendered as bogus glyphs.
void test_utf8_rejects_invalid_sequences(void) {
  struct Case {
    const char *text;
    size_t size;
    uint32_t first;
    size_t consumed;
  };
  const Case cases[] = {
      {"\xC0\x80", 2, 0xFFFD, 1},             // overlong NUL
      {"\xED\xA0\x80", 3, 0xFFFD, 1},         // surrogate D800
      {"\xF4\x90\x80\x80", 4, 0xFFFD, 1},     // U+110000
      {"\xE2\x82", 2, 0xFFFD, 1},             // truncated at end of string
      {"\xC3\x86", 2, 0x00C6, 2},             // valid AE
  };

  for (const Case &c : cases) {
    const std::string text(c.text, c.size);
    size_t index = 0;
    TEST_ASSERT_EQUAL_UINT32(c.first, ui::utf8_next(text, index));
    TEST_ASSERT_EQUAL_size_t(c.consumed, index);
  }
}

// std::string can hold NUL bytes; decoding must still make progress.
void test_utf8_embedded_nul_terminates(void) {
  const std::string text("A\0B", 3);
  TEST_ASSERT_EQUAL_size_t(3, ui::utf8_length(text));

  size_t index = 0;
  TEST_ASSERT_EQUAL_UINT32('A', ui::utf8_next(text, index));
  TEST_ASSERT_EQUAL_UINT32(0xFFFD, ui::utf8_next(text, index));
  TEST_ASSERT_EQUAL_UINT32('B', ui::utf8_next(text, index));

  // And the drawing helpers, which loop over the string, must terminate.
  TEST_ASSERT_EQUAL_INT(3 + 1 + 2, ui::font_text_width(test::test_font(), text));
  TEST_ASSERT_EQUAL_size_t(2, ui::utf8_length("\xC3\x86\xC3"));
}

void test_utf8_length(void) {
  TEST_ASSERT_EQUAL_size_t(3, ui::utf8_length("A\xC3\x86\xC3\x98"));
}

void test_text_width_is_advance_sum(void) {
  const ui::Font &font = test::test_font();
  TEST_ASSERT_EQUAL_INT(5, ui::font_text_width(font, "AB"));
  TEST_ASSERT_EQUAL_INT(8, ui::font_text_width(font, "A A"));
  TEST_ASSERT_EQUAL_INT(0, ui::font_text_width(font, ""));
  TEST_ASSERT_EQUAL_INT(3, ui::font_text_width(font, "\xC3\x86"));
}

// '4' in Cozette has more ink than advance; measurement must include it or a
// label paints outside the box the dirty tracker knows about.
void test_ink_width_covers_overhanging_glyphs(void) {
  const ui::Glyph *four = ui::font_glyph(ui::fonts::kCozette16, '4');
  TEST_ASSERT_NOT_NULL(four);
  TEST_ASSERT_TRUE(four->xoff + four->width > four->advance);

  TEST_ASSERT_EQUAL_INT(four->xoff + four->width,
                        ui::font_text_ink_width(ui::fonts::kCozette16, "4"));
  TEST_ASSERT_EQUAL_INT(ui::font_text_width(ui::fonts::kCozette16, "4"),
                        four->advance);

  // Ink width never shrinks below the advance sum.
  TEST_ASSERT_EQUAL_INT(ui::font_text_width(ui::fonts::kCozette16, "AB"),
                        ui::font_text_ink_width(ui::fonts::kCozette16, "AB"));

  // The synthetic font's glyph whose ink runs past its advance.
  TEST_ASSERT_EQUAL_INT(test::kOverhangInk,
                        ui::font_text_ink_width(test::test_font(),
                                                test::kOverhangText));
}

void test_real_fonts_cover_the_whole_charset(void) {
  // Exactly the code points tools/gen_fonts.py emits.
  std::vector<uint32_t> expected;
  for (uint32_t cp = 0x20; cp <= 0x7E; ++cp) {
    expected.push_back(cp);
  }
  for (uint32_t cp : {0x00C6u, 0x00D8u, 0x00C5u, 0x00E6u, 0x00F8u, 0x00E5u}) {
    expected.push_back(cp);
  }

  for (uint32_t cp : expected) {
    TEST_ASSERT_TRUE(ui::font_has_glyph(ui::fonts::kCozette16, cp));
    TEST_ASSERT_TRUE(ui::font_has_glyph(ui::fonts::kCozette96, cp));
  }
  TEST_ASSERT_EQUAL_size_t(expected.size(), ui::fonts::kCozette16.count);
  TEST_ASSERT_FALSE(ui::font_has_glyph(ui::fonts::kCozette16, 0x20AC)); // euro
}

void test_cozette96_metrics(void) {
  const ui::Font &font = ui::fonts::kCozette96;
  TEST_ASSERT_EQUAL_INT(97, font.line_height);
  TEST_ASSERT_EQUAL_INT(74, font.ascent);
  TEST_ASSERT_EQUAL_INT(7 * 44, ui::font_text_width(font, "Success"));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_utf8_roundtrip_danish);
  RUN_TEST(test_utf8_malformed_is_replaced);
  RUN_TEST(test_utf8_rejects_invalid_sequences);
  RUN_TEST(test_utf8_embedded_nul_terminates);
  RUN_TEST(test_utf8_length);
  RUN_TEST(test_text_width_is_advance_sum);
  RUN_TEST(test_ink_width_covers_overhanging_glyphs);
  RUN_TEST(test_real_fonts_cover_the_whole_charset);
  RUN_TEST(test_cozette96_metrics);
  return UNITY_END();
}
