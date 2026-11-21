/*******************************************************************************
 * Size: 16 px
 * Bpp: 1
 * Opts: --size 16 --bpp 1 --format lvgl --font CozetteVector-nerd.ttf -r 0x20-0x7E -r 0xC5-0xC6 -r 0xD8 -r 0xE5-0xE6 -r 0xF8 --no-compress --lv-include lvgl.h --lv-font-name cozette_vector_16 -o firmware/src/fonts/cozette_vector_16.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef COZETTE_VECTOR_16
#define COZETTE_VECTOR_16 1
#endif

#if COZETTE_VECTOR_16

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0x20,

    /* U+0022 "\"" */
    0x99, 0x99,

    /* U+0023 "#" */
    0x51, 0x45, 0x3f, 0x51, 0x45, 0x3f, 0x51, 0x45,
    0x0,

    /* U+0024 "$" */
    0x11, 0xe1, 0x25, 0x91, 0xe1, 0x45, 0x16, 0x51,
    0x1e, 0x10,

    /* U+0025 "%" */
    0x42, 0x40, 0x11, 0x8, 0x41, 0x18, 0x88, 0x50,
    0x2,

    /* U+0026 "&" */
    0x11, 0xa4, 0x84, 0x10, 0x47, 0x62, 0x8a, 0x28,
    0x9d,

    /* U+0027 "'" */
    0xf0,

    /* U+0028 "(" */
    0x29, 0x49, 0x24, 0x91, 0x24, 0x40,

    /* U+0029 ")" */
    0x89, 0x22, 0x49, 0x25, 0x29, 0x0,

    /* U+002A "*" */
    0x48, 0x4f, 0xc4, 0x11, 0x20,

    /* U+002B "+" */
    0x10, 0x4f, 0xc4, 0x10, 0x40,

    /* U+002C "," */
    0xf6,

    /* U+002D "-" */
    0xfc,

    /* U+002E "." */
    0xf0,

    /* U+002F "/" */
    0x4, 0x10, 0x82, 0x0, 0x41, 0x0, 0x41, 0x8,
    0x20,

    /* U+0030 "0" */
    0x7a, 0x28, 0x61, 0x86, 0x59, 0x61, 0x86, 0x17,
    0x80,

    /* U+0031 "1" */
    0x10, 0x47, 0x24, 0x10, 0x41, 0x4, 0x10, 0x4f,
    0xc0,

    /* U+0032 "2" */
    0x7a, 0x28, 0x41, 0x0, 0x21, 0x10, 0x82, 0xf,
    0xc0,

    /* U+0033 "3" */
    0x7a, 0x8, 0x41, 0x8, 0x70, 0x41, 0x86, 0x7,
    0x80,

    /* U+0034 "4" */
    0x4, 0x8, 0x30, 0xa2, 0x48, 0xa1, 0x7f, 0x4,
    0x8, 0x10,

    /* U+0035 "5" */
    0xfe, 0x8, 0x20, 0x83, 0xf0, 0x41, 0x86, 0x7,
    0x80,

    /* U+0036 "6" */
    0x18, 0x84, 0x20, 0x83, 0xf8, 0x61, 0x86, 0x7,
    0x80,

    /* U+0037 "7" */
    0xfc, 0x10, 0x42, 0x8, 0x2, 0x8, 0x1, 0x4,
    0x0,

    /* U+0038 "8" */
    0x7a, 0x28, 0x61, 0x81, 0xe8, 0x61, 0x86, 0x17,
    0x80,

    /* U+0039 "9" */
    0x7a, 0x8, 0x61, 0x86, 0x17, 0xc1, 0x8, 0x46,
    0x0,

    /* U+003A ":" */
    0xf0, 0xf,

    /* U+003B ";" */
    0xf0, 0xf, 0x60,

    /* U+003C "<" */
    0x8, 0x88, 0x88, 0x20, 0xc2, 0x8,

    /* U+003D "=" */
    0xfc, 0x0, 0x3f,

    /* U+003E ">" */
    0x82, 0x8, 0x20, 0x89, 0x90, 0x80,

    /* U+003F "?" */
    0x7a, 0x8, 0x41, 0x8, 0x21, 0x4, 0x0, 0x1,
    0x0,

    /* U+0040 "@" */
    0x7a, 0x8, 0x61, 0x86, 0x79, 0x67, 0x82, 0x7,
    0xc0,

    /* U+0041 "A" */
    0x7a, 0x8, 0x61, 0x86, 0x1f, 0xe1, 0x86, 0x18,
    0x40,

    /* U+0042 "B" */
    0xfa, 0x28, 0x61, 0x83, 0xe8, 0x61, 0x86, 0x1f,
    0x80,

    /* U+0043 "C" */
    0x7a, 0x28, 0x60, 0x82, 0x8, 0x20, 0x86, 0x27,
    0x80,

    /* U+0044 "D" */
    0xf2, 0x48, 0xa1, 0x86, 0x18, 0x61, 0x8a, 0x4f,
    0x0,

    /* U+0045 "E" */
    0xfe, 0x8, 0x20, 0x83, 0xe8, 0x20, 0x82, 0xf,
    0xc0,

    /* U+0046 "F" */
    0xfe, 0x8, 0x20, 0x83, 0xe8, 0x20, 0x82, 0x8,
    0x0,

    /* U+0047 "G" */
    0x7a, 0x28, 0x60, 0x82, 0x8, 0xe1, 0x86, 0x17,
    0x80,

    /* U+0048 "H" */
    0x86, 0x18, 0x61, 0x87, 0xf8, 0x61, 0x86, 0x18,
    0x40,

    /* U+0049 "I" */
    0xf4, 0x44, 0x44, 0x44, 0x44, 0xf0,

    /* U+004A "J" */
    0x3c, 0x10, 0x41, 0x4, 0x10, 0x61, 0x86, 0x7,
    0x80,

    /* U+004B "K" */
    0x86, 0x28, 0xa4, 0x93, 0xc8, 0xa2, 0x8a, 0x18,
    0x40,

    /* U+004C "L" */
    0x82, 0x8, 0x20, 0x82, 0x8, 0x20, 0x82, 0xf,
    0xc0,

    /* U+004D "M" */
    0x83, 0x7, 0xbc, 0x99, 0x30, 0x60, 0xc1, 0x83,
    0x6, 0x8,

    /* U+004E "N" */
    0x86, 0x1c, 0x71, 0xa6, 0x59, 0x63, 0x8e, 0x18,
    0x40,

    /* U+004F "O" */
    0x7a, 0x8, 0x61, 0x86, 0x18, 0x61, 0x86, 0x17,
    0x80,

    /* U+0050 "P" */
    0xfa, 0x28, 0x61, 0x86, 0x1f, 0xa0, 0x82, 0x8,
    0x0,

    /* U+0051 "Q" */
    0x7a, 0x28, 0x61, 0x86, 0x18, 0x61, 0x8a, 0x66,
    0x41,

    /* U+0052 "R" */
    0xfa, 0x28, 0x61, 0x83, 0xe8, 0xa1, 0x86, 0x18,
    0x40,

    /* U+0053 "S" */
    0x7a, 0x28, 0x60, 0x81, 0xf0, 0x41, 0x86, 0x7,
    0x80,

    /* U+0054 "T" */
    0xfc, 0x41, 0x4, 0x10, 0x41, 0x4, 0x10, 0x41,
    0x0,

    /* U+0055 "U" */
    0x86, 0x18, 0x61, 0x86, 0x18, 0x61, 0x86, 0x7,
    0x80,

    /* U+0056 "V" */
    0x86, 0x18, 0x61, 0x49, 0x24, 0x92, 0x30, 0x41,
    0x0,

    /* U+0057 "W" */
    0x86, 0x18, 0x61, 0x96, 0x59, 0x9e, 0x49, 0x24,
    0x80,

    /* U+0058 "X" */
    0x86, 0x18, 0x12, 0x0, 0x41, 0x12, 0x2, 0x18,
    0x40,

    /* U+0059 "Y" */
    0x86, 0x18, 0x61, 0x89, 0x23, 0x4, 0x10, 0x41,
    0x0,

    /* U+005A "Z" */
    0xfc, 0x20, 0x84, 0x10, 0x4, 0x10, 0x82, 0xf,
    0xc0,

    /* U+005B "[" */
    0xf2, 0x49, 0x24, 0x92, 0x49, 0xc0,

    /* U+005C "\\" */
    0x82, 0x4, 0x10, 0x20, 0x41, 0x4, 0x8, 0x20,
    0x41,

    /* U+005D "]" */
    0xe4, 0x92, 0x49, 0x24, 0x93, 0xc0,

    /* U+005E "^" */
    0x10, 0xc4, 0xa1,

    /* U+005F "_" */
    0xfc,

    /* U+0060 "`" */
    0x90,

    /* U+0061 "a" */
    0x7e, 0x18, 0x61, 0x86, 0x78, 0x59,

    /* U+0062 "b" */
    0x82, 0x8, 0x20, 0xfa, 0x18, 0x61, 0x86, 0x18,
    0x3e,

    /* U+0063 "c" */
    0x7a, 0x28, 0x60, 0x82, 0x18, 0x9e,

    /* U+0064 "d" */
    0x4, 0x10, 0x41, 0x7e, 0x18, 0x61, 0x86, 0x10,
    0x5f,

    /* U+0065 "e" */
    0x7a, 0x18, 0x7f, 0x82, 0x18, 0x9e,

    /* U+0066 "f" */
    0x1e, 0x40, 0x81, 0xf, 0x84, 0x8, 0x10, 0x20,
    0x40, 0x81, 0x0,

    /* U+0067 "g" */
    0x7e, 0x18, 0x61, 0x86, 0x10, 0x5f, 0x4, 0x10,
    0x5e,

    /* U+0068 "h" */
    0x82, 0x8, 0x20, 0xfa, 0x28, 0x61, 0x86, 0x18,
    0x61,

    /* U+0069 "i" */
    0x40, 0xc, 0x44, 0x44, 0x44, 0x30,

    /* U+006A "j" */
    0x20, 0x32, 0x49, 0x24, 0xd4,

    /* U+006B "k" */
    0x82, 0x8, 0x20, 0x86, 0x28, 0xa4, 0xf2, 0x28,
    0xa1,

    /* U+006C "l" */
    0xc9, 0x24, 0x92, 0x49, 0x30,

    /* U+006D "m" */
    0xea, 0xe9, 0x65, 0x96, 0x59, 0x65,

    /* U+006E "n" */
    0xfa, 0x28, 0x61, 0x86, 0x18, 0x61,

    /* U+006F "o" */
    0x7a, 0x18, 0x61, 0x86, 0x18, 0x9e,

    /* U+0070 "p" */
    0xfa, 0x18, 0x61, 0x86, 0x18, 0x3e, 0x82, 0x8,
    0x20,

    /* U+0071 "q" */
    0x7d, 0xa, 0x14, 0x28, 0x50, 0x81, 0x3e, 0x4,
    0x8, 0x10, 0x30,

    /* U+0072 "r" */
    0xfa, 0x28, 0x60, 0x82, 0x8, 0x20,

    /* U+0073 "s" */
    0x7e, 0x8, 0x1e, 0x4, 0x10, 0x3e,

    /* U+0074 "t" */
    0x20, 0x40, 0x87, 0xc2, 0x4, 0x8, 0x10, 0x20,
    0x40, 0x78,

    /* U+0075 "u" */
    0x86, 0x18, 0x61, 0x86, 0x10, 0x5f,

    /* U+0076 "v" */
    0x86, 0x18, 0x92, 0x48, 0xc1, 0x4,

    /* U+0077 "w" */
    0x86, 0x18, 0x65, 0x96, 0xe4, 0x92,

    /* U+0078 "x" */
    0x84, 0x4, 0x84, 0x11, 0x20, 0x21,

    /* U+0079 "y" */
    0x86, 0x18, 0x61, 0x86, 0x18, 0x5f, 0x4, 0x10,
    0x9e,

    /* U+007A "z" */
    0xfc, 0x20, 0x84, 0x42, 0x8, 0x3f,

    /* U+007B "{" */
    0xc, 0x41, 0x4, 0x10, 0x4e, 0xc, 0x10, 0x41,
    0x4, 0x10, 0x30,

    /* U+007C "|" */
    0xff, 0xfe,

    /* U+007D "}" */
    0xc0, 0xc1, 0x4, 0x10, 0x40, 0xc4, 0x10, 0x41,
    0x4, 0x23, 0x0,

    /* U+007E "~" */
    0x46, 0xd9, 0x22,

    /* U+00C5 "Å" */
    0x11, 0xa1, 0x0, 0x1, 0xe8, 0x21, 0x87, 0xf8,
    0x61, 0x84,

    /* U+00C6 "Æ" */
    0x7e, 0x49, 0x24, 0x92, 0x7f, 0x24, 0x92, 0x49,
    0xc0,

    /* U+00D8 "Ø" */
    0x5, 0xe8, 0xa3, 0x8e, 0x39, 0x65, 0xc7, 0x1c,
    0x5e, 0x80,

    /* U+00E5 "å" */
    0x11, 0xa1, 0x0, 0x1, 0xf8, 0x61, 0x86, 0x18,
    0xc1, 0x74,

    /* U+00E6 "æ" */
    0x7a, 0x59, 0x67, 0x92, 0x59, 0x92,

    /* U+00F8 "ø" */
    0x5, 0xe8, 0xe3, 0x96, 0x5e, 0x58, 0x7a, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 118, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 118, .box_w = 1, .box_h = 11, .ofs_x = 4, .ofs_y = 0},
    {.bitmap_index = 3, .adv_w = 118, .box_w = 4, .box_h = 4, .ofs_x = 2, .ofs_y = 8},
    {.bitmap_index = 5, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 14, .adv_w = 118, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 24, .adv_w = 118, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 33, .adv_w = 118, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 42, .adv_w = 118, .box_w = 1, .box_h = 4, .ofs_x = 4, .ofs_y = 8},
    {.bitmap_index = 43, .adv_w = 118, .box_w = 3, .box_h = 14, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 49, .adv_w = 118, .box_w = 3, .box_h = 14, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 55, .adv_w = 118, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 60, .adv_w = 118, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 65, .adv_w = 118, .box_w = 2, .box_h = 4, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 66, .adv_w = 118, .box_w = 6, .box_h = 1, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 67, .adv_w = 118, .box_w = 2, .box_h = 2, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 68, .adv_w = 118, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 77, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 86, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 95, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 104, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 113, .adv_w = 118, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 123, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 132, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 141, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 150, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 159, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 168, .adv_w = 118, .box_w = 2, .box_h = 8, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 170, .adv_w = 118, .box_w = 2, .box_h = 10, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 173, .adv_w = 118, .box_w = 5, .box_h = 9, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 179, .adv_w = 118, .box_w = 6, .box_h = 4, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 182, .adv_w = 118, .box_w = 5, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 188, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 197, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 206, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 215, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 224, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 233, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 242, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 251, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 260, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 269, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 278, .adv_w = 118, .box_w = 4, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 284, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 293, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 302, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 311, .adv_w = 118, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 321, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 330, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 339, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 348, .adv_w = 118, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 357, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 366, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 375, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 384, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 393, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 402, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 411, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 420, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 429, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 438, .adv_w = 118, .box_w = 3, .box_h = 14, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 444, .adv_w = 118, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 453, .adv_w = 118, .box_w = 3, .box_h = 14, .ofs_x = 3, .ofs_y = -2},
    {.bitmap_index = 459, .adv_w = 118, .box_w = 6, .box_h = 4, .ofs_x = 1, .ofs_y = 9},
    {.bitmap_index = 462, .adv_w = 118, .box_w = 6, .box_h = 1, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 463, .adv_w = 118, .box_w = 2, .box_h = 2, .ofs_x = 2, .ofs_y = 9},
    {.bitmap_index = 464, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 470, .adv_w = 118, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 479, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 485, .adv_w = 118, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 494, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 500, .adv_w = 118, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 511, .adv_w = 118, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 520, .adv_w = 118, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 529, .adv_w = 118, .box_w = 4, .box_h = 11, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 535, .adv_w = 118, .box_w = 3, .box_h = 13, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 540, .adv_w = 118, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 549, .adv_w = 118, .box_w = 3, .box_h = 12, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 554, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 560, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 566, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 572, .adv_w = 118, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 581, .adv_w = 118, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 592, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 598, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 604, .adv_w = 118, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 614, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 620, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 626, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 632, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 638, .adv_w = 118, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 647, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 653, .adv_w = 118, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 664, .adv_w = 118, .box_w = 1, .box_h = 15, .ofs_x = 4, .ofs_y = -3},
    {.bitmap_index = 666, .adv_w = 118, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 677, .adv_w = 118, .box_w = 6, .box_h = 4, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 680, .adv_w = 118, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 690, .adv_w = 118, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 699, .adv_w = 118, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 709, .adv_w = 118, .box_w = 6, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 719, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 725, .adv_w = 118, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = -1}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_1[] = {
    0x0, 0x1, 0x13, 0x20, 0x21, 0x33
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 197, .range_length = 52, .glyph_id_start = 96,
        .unicode_list = unicode_list_1, .glyph_id_ofs_list = NULL, .list_length = 6, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 2,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t cozette_vector_16 = {
#else
lv_font_t cozette_vector_16 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 17,          /*The maximum line height required by the font*/
    .base_line = 4,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if COZETTE_VECTOR_16*/

