/*
 * Glyph atlases as signed distance fields, so the disc's bitmap letters can
 * be drawn with sharp edges at any window size (docs/MENU_PRESENTATION.md,
 * "Text sharpness").
 */
#ifndef OPENGT4_GLYPH_SDF_H
#define OPENGT4_GLYPH_SDF_H

#include <stdint.h>

/* The atlas `scale` times larger, RGBA: r = g = b the original coverage
   (bilinear, the disc's soft look), a the signed distance to the letter
   edge found on a bicubic resampling of that coverage (0.5 on the edge,
   0..1 over +-2 source texels). NULL on failure; free() the result. */
uint8_t *gt4_glyph_sdf(const uint8_t *rgba, int w, int h, int scale, int *out_w, int *out_h);

/* Optional presentation-only checkpoints; identical atlas output. */
uint8_t *gt4_glyph_sdf_presented(const uint8_t *rgba,int w,int h,int scale,int *out_w,int *out_h,void (*progress)(void *),void *user);

#endif
