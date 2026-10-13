/*
 * GT4 "Tex1" texture sets (.img files, and the images inside .gpb packages),
 * converted to plain RGBA when loaded. The file is a snapshot of PS2 video
 * memory: blocks uploaded in one pixel format and read back in another. The
 * conversion reproduces that layout once, at load time; nothing about the PS2
 * graphics chip is emulated while the game runs. Format: docs/TEX1_FORMAT.md.
 */
#ifndef OPENGT4_TEX1_H
#define OPENGT4_TEX1_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int      width, height;   /* the image's own size (from its clamp region) */
    int      tex_w, tex_h;    /* the power-of-two texture it was stored in */
    uint8_t *rgba;            /* width * height * 4, alpha 0..255 */
    uint8_t *native_alpha;    /* original 0..255 coefficient; 128 is unity */
    uint8_t color_component,texture_function;
    uint8_t wrap_u,wrap_v;
    uint8_t magnify,minify;   /* TEX1 MMAG (bit 5) and MMIN (bits 6..8): 0 nearest, 1 bilinear */
    uint16_t min_u,max_u,min_v,max_v;
    uint8_t mip_count; /* authored TEX1.MXL, levels 1..6 */
    uint8_t *mip_rgba[6]; /* full power-of-two levels; alpha is native GS */
} tex1_image;

/* Decodes every texture in a Tex1 file. On success returns the number of
   images and sets *out (free with tex1_free). Returns -1 on bad data. */
int  tex1_decode(const uint8_t *data, size_t size, tex1_image **out);
void tex1_free(tex1_image *images, int count);

#endif /* OPENGT4_TEX1_H */
