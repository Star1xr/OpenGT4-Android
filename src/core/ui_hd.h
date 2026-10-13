/*
 * Local HD cache of the menu images (docs/UI_HD.md): upscaled copies made
 * on the user's machine from their disc by tools/c/gt4hdui.c, found by a
 * hash of the decoded image. File "<hash>.hdi": "HDI2", u32 width, u32
 * height (little endian), then the width * height RGBA texels as raw
 * deflate.
 */
#ifndef OPENGT4_UI_HD_H
#define OPENGT4_UI_HD_H

#include <stdint.h>

/* FNV-1a 64 over the size and the texels of a decoded image. */
uint64_t gt4_ui_hd_hash(const uint8_t *rgba, int w, int h);
/* Reads one cache file; NULL if absent or malformed. free() the result. */
uint8_t *gt4_ui_hd_read(const char *path, int *w, int *h);

#endif
