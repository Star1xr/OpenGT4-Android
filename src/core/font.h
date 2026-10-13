#ifndef OPENGT4_FONT_H
#define OPENGT4_FONT_H
#include "tex1.h"

typedef struct {
    int x, y, w, h, advance, u, v;
    int bitmap_width; /* unmodified FT01 width, including the ff sentinel */
} gt4_glyph;
typedef struct {
    unsigned first, count;
    int height, baseline;
    gt4_glyph *glyphs;
    tex1_image *atlases;
    int natlases;
    /* FT01 contours consumed by 00492978 +0x1EC..+0x344. */
    unsigned contour_height, pair_gap;
    unsigned space_advance;
    int8_t *contours;
    /* Face +0x10..+0x16: the cell of a KNJI glyph drawn for codes from
       0x300 (00492EF0 -> 00492748): width, height, signed x/y offsets,
       advance; +0x16 picks the KNJI subfont (004900B8 -> 00491C10). */
    uint8_t kanji_w,kanji_h,kanji_advance,kanji_set;
    int8_t kanji_x,kanji_y;
} gt4_font;

/* Decode one named face from FT01. Owns its output, not the input bytes.
   Returns 1 on success, 0 if absent, -1 for malformed data. */
int gt4_font_decode(const uint8_t *data, size_t size, const char *name, gt4_font *out);
const gt4_glyph *gt4_font_glyph(const gt4_font *font, unsigned codepoint);
float gt4_font_line_width(const gt4_font *font, const char *text, float spacing);
void gt4_font_free(gt4_font *font);

/* KNJI files (/font/*.kf): subfonts of 4-bit glyphs; +0x12 palettes of 16
   colours from +0x18, the first outlined and the second ink. 00491CB8: a code outside the subfont's
   range, or the table's 0xFFFF, uses glyph 0. Writes the w*h glyph as RGBA
   with the ink and outlined palettes; returns 0 for malformed data. */
#define GT4_KANJI_FIRST 0x300u
int gt4_kanji_glyph(const uint8_t *data, size_t size, unsigned set, unsigned code,
                    int *w, int *h, uint8_t *ink, uint8_t *outlined, size_t capacity);
#endif
