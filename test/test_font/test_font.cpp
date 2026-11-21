#include <unity.h>

#include <string>

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

void test_real_fonts_cover_the_charset(void) {
  const uint32_t required[] = {' ', 'A', 'z', '0', '~', 0x00C6, 0x00D8,
                               0x00C5, 0x00E6, 0x00F8, 0x00E5};
  for (uint32_t cp : required) {
    TEST_ASSERT_TRUE(ui::font_has_glyph(ui::fonts::kCozette16, cp));
    TEST_ASSERT_TRUE(ui::font_has_glyph(ui::fonts::kCozette96, cp));
  }
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
  RUN_TEST(test_utf8_length);
  RUN_TEST(test_text_width_is_advance_sum);
  RUN_TEST(test_real_fonts_cover_the_charset);
  RUN_TEST(test_cozette96_metrics);
  return UNITY_END();
}
