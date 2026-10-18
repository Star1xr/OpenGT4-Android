/*
 * Tex1 -> RGBA. See tex1.h and docs/TEX1_FORMAT.md.
 *
 * PS2 video memory is organised in 8 KB pages of 32 blocks of 256 bytes.
 * Where a pixel lands depends on its format: PSMCT32 (32-bit), PSMT8 (8-bit
 * palette index) and PSMT4 (4-bit) use different page sizes, block orders
 * and in-block column orders. The tables below are that layout. The images
 * are uploaded as PSMCT32 (or in their own format) and read as PSMT8/PSMT4,
 * so the conversion writes through one layout and reads through the other.
 * Checked visually against the game's own images (docs/TEX1_FORMAT.md).
 */
#include "tex1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VRAM_SIZE (4u * 1024 * 1024)

enum { PSMCT32 = 0x00, PSMCT24 = 0x01, PSMCT16 = 0x02, PSMCT16S = 0x0a, PSMT8 = 0x13, PSMT4 = 0x14 };

static const uint8_t blk32[4][8] = {
    { 0, 1, 4, 5, 16, 17, 20, 21 }, { 2, 3, 6, 7, 18, 19, 22, 23 },
    { 8, 9, 12, 13, 24, 25, 28, 29 }, { 10, 11, 14, 15, 26, 27, 30, 31 } };
static const uint8_t col32[8][8] = {
    { 0, 1, 4, 5, 8, 9, 12, 13 }, { 2, 3, 6, 7, 10, 11, 14, 15 },
    { 16, 17, 20, 21, 24, 25, 28, 29 }, { 18, 19, 22, 23, 26, 27, 30, 31 },
    { 32, 33, 36, 37, 40, 41, 44, 45 }, { 34, 35, 38, 39, 42, 43, 46, 47 },
    { 48, 49, 52, 53, 56, 57, 60, 61 }, { 50, 51, 54, 55, 58, 59, 62, 63 } };
static const uint8_t col8[16][16] = {
    { 0, 4, 16, 20, 32, 36, 48, 52, 2, 6, 18, 22, 34, 38, 50, 54 },
    { 8, 12, 24, 28, 40, 44, 56, 60, 10, 14, 26, 30, 42, 46, 58, 62 },
    { 33, 37, 49, 53, 1, 5, 17, 21, 35, 39, 51, 55, 3, 7, 19, 23 },
    { 41, 45, 57, 61, 9, 13, 25, 29, 43, 47, 59, 63, 11, 15, 27, 31 },
    { 96, 100, 112, 116, 64, 68, 80, 84, 98, 102, 114, 118, 66, 70, 82, 86 },
    { 104, 108, 120, 124, 72, 76, 88, 92, 106, 110, 122, 126, 74, 78, 90, 94 },
    { 65, 69, 81, 85, 97, 101, 113, 117, 67, 71, 83, 87, 99, 103, 115, 119 },
    { 73, 77, 89, 93, 105, 109, 121, 125, 75, 79, 91, 95, 107, 111, 123, 127 },
    { 128, 132, 144, 148, 160, 164, 176, 180, 130, 134, 146, 150, 162, 166, 178, 182 },
    { 136, 140, 152, 156, 168, 172, 184, 188, 138, 142, 154, 158, 170, 174, 186, 190 },
    { 161, 165, 177, 181, 129, 133, 145, 149, 163, 167, 179, 183, 131, 135, 147, 151 },
    { 169, 173, 185, 189, 137, 141, 153, 157, 171, 175, 187, 191, 139, 143, 155, 159 },
    { 224, 228, 240, 244, 192, 196, 208, 212, 226, 230, 242, 246, 194, 198, 210, 214 },
    { 232, 236, 248, 252, 200, 204, 216, 220, 234, 238, 250, 254, 202, 206, 218, 222 },
    { 193, 197, 209, 213, 225, 229, 241, 245, 195, 199, 211, 215, 227, 231, 243, 247 },
    { 201, 205, 217, 221, 233, 237, 249, 253, 203, 207, 219, 223, 235, 239, 251, 255 } };
/* Blocks in a page for the layouts with 32 blocks in 4 columns and 8 rows:
   PSMT4 and PSMCT16 (see docs/TEX1_FORMAT.md). */
static const uint8_t blk4[8][4] = {
    { 0, 2, 8, 10 }, { 1, 3, 9, 11 }, { 4, 6, 12, 14 }, { 5, 7, 13, 15 },
    { 16, 18, 24, 26 }, { 17, 19, 25, 27 }, { 20, 22, 28, 30 }, { 21, 23, 29, 31 } };
/* PSMCT16S holds the same blocks in a different order. */
static const uint8_t blk16s[8][4] = {
    { 0, 2, 16, 18 }, { 1, 3, 17, 19 }, { 8, 10, 24, 26 }, { 9, 11, 25, 27 },
    { 4, 6, 20, 22 }, { 5, 7, 21, 23 }, { 12, 14, 28, 30 }, { 13, 15, 29, 31 } };
/* PSMT4 in-block nibble order for rows 0-7; rows 8-15 are the same plus 256. */
static const uint16_t col4[8][32] = {
    { 0, 8, 32, 40, 64, 72, 96, 104, 2, 10, 34, 42, 66, 74, 98, 106, 4, 12, 36, 44, 68, 76, 100, 108, 6, 14, 38, 46, 70, 78, 102, 110 },
    { 16, 24, 48, 56, 80, 88, 112, 120, 18, 26, 50, 58, 82, 90, 114, 122, 20, 28, 52, 60, 84, 92, 116, 124, 22, 30, 54, 62, 86, 94, 118, 126 },
    { 65, 73, 97, 105, 1, 9, 33, 41, 67, 75, 99, 107, 3, 11, 35, 43, 69, 77, 101, 109, 5, 13, 37, 45, 71, 79, 103, 111, 7, 15, 39, 47 },
    { 81, 89, 113, 121, 17, 25, 49, 57, 83, 91, 115, 123, 19, 27, 51, 59, 85, 93, 117, 125, 21, 29, 53, 61, 87, 95, 119, 127, 23, 31, 55, 63 },
    { 192, 200, 224, 232, 128, 136, 160, 168, 194, 202, 226, 234, 130, 138, 162, 170, 196, 204, 228, 236, 132, 140, 164, 172, 198, 206, 230, 238, 134, 142, 166, 174 },
    { 208, 216, 240, 248, 144, 152, 176, 184, 210, 218, 242, 250, 146, 154, 178, 186, 212, 220, 244, 252, 148, 156, 180, 188, 214, 222, 246, 254, 150, 158, 182, 190 },
    { 129, 137, 161, 169, 193, 201, 225, 233, 131, 139, 163, 171, 195, 203, 227, 235, 133, 141, 165, 173, 197, 205, 229, 237, 135, 143, 167, 175, 199, 207, 231, 239 },
    { 145, 153, 177, 185, 209, 217, 241, 249, 147, 155, 179, 187, 211, 219, 243, 251, 149, 157, 181, 189, 213, 221, 245, 253, 151, 159, 183, 191, 215, 223, 247, 255 } };

/* Byte addresses. tbp in 256-byte blocks, bw (buffer width) in 64-pixel units. */
static uint32_t addr32(uint32_t tbp, uint32_t bw, uint32_t x, uint32_t y)
{
    uint32_t page = (y / 32) * bw + x / 64;
    uint32_t blk = tbp + page * 32 + blk32[(y % 32) / 8][(x % 64) / 8];
    return (blk * 256 + col32[y % 8][x % 8] * 4u) & (VRAM_SIZE - 1);   /* GS memory wraps */
}

static uint32_t addr8(uint32_t tbp, uint32_t bw, uint32_t x, uint32_t y)
{
    uint32_t page = (y / 64) * (bw / 2) + x / 128;
    uint32_t blk = tbp + page * 32 + blk32[(y % 64) / 16][(x % 128) / 16];
    return (blk * 256 + col8[y % 16][x % 16]) & (VRAM_SIZE - 1);
}

/* Returns the byte address; *shift is 0 or 4 for the nibble. */
static uint32_t addr4(uint32_t tbp, uint32_t bw, uint32_t x, uint32_t y, unsigned *shift)
{
    uint32_t page = (y / 128) * (bw / 2) + x / 128;
    uint32_t blk = tbp + page * 32 + blk4[(y % 128) / 16][(x % 128) / 32];
    uint32_t nib = col4[y % 8][x % 32] + ((y % 16) >= 8 ? 256u : 0u);
    *shift = (nib & 1) * 4;
    return (blk * 256 + nib / 2) & (VRAM_SIZE - 1);
}

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint64_t le64(const uint8_t *p) { return le32(p) | (uint64_t)le32(p + 4) << 32; }

/* PSMCT16: a 256-byte block holds 16x8 pixels, two rows to a 32-byte line.
   Every 32-bit unit holds the pixels of one column's two halves (x and x+8),
   so the 32-bit column table gives the unit and its lowest bit the half.
   PSMCT16S is the same layout with the page's blocks in another order. */
static uint32_t addr16(uint32_t tbp, uint32_t bw, uint32_t x, uint32_t y, int cts)
{
    uint32_t page = (y / 64) * bw + x / 64;
    const uint8_t *row = cts ? blk16s[(y % 64) / 8] : blk4[(y % 64) / 8];
    uint32_t blk = tbp + page * 32 + row[(x % 64) / 16];
    uint32_t half = col32[y % 8][x % 8] * 2 + (x % 16) / 8;
    return (blk * 256 + half * 2) & (VRAM_SIZE - 1);
}

/* Formats the textures can be read back in. PSMT8H, PSMT4HL and PSMT4HH write
   into parts of a 32-bit unit and are not read here. */
static int readable(uint32_t psm)
{
    return psm == PSMCT32 || psm == PSMCT24 || psm == PSMCT16 || psm == PSMCT16S ||
           psm == PSMT8 || psm == PSMT4;
}

/* Formats a transfer can be written into video memory with. PSMCT24 is not
   one of them: the disc's 24-bit images come from a 32-bit buffer read as
   PSMCT24, which is why PSMCT24 is readable and not writable. */
static int writable(uint32_t psm)
{
    return psm == PSMCT32 || psm == PSMCT16 || psm == PSMCT16S || psm == PSMT8 || psm == PSMT4;
}

enum { RPT_TRANSFER, RPT_TEXTURE, RPT_PALETTE, RPT_PALETTE_DATA, RPT_MIP,
       RPT_HEADER, RPT_OFFSET, RPT_SHORT, RPT_BIG, RPT_MANY_LEVELS, RPT_ALLOC };

/* Each format this decoder cannot take is named once, with the size it was
   used at: one image used to drop the whole file, so the log has to say which
   one it was. The failures below (a transfer outside the block, a texture over
   1024, levels over 6, a short transfer, no memory) drop a whole texture block
   and used to be silent: a car or a course with those blocks missing had to be
   guessed at, never read off the log. */
static void report_once(unsigned kind, unsigned a, unsigned b, unsigned c)
{
    static struct { unsigned kind, a, b, c; } seen[64];
    static int count, full;
    int i;
    for (i = 0; i < count; i++)
        if (seen[i].kind == kind && seen[i].a == a && seen[i].b == b && seen[i].c == c) return;
    /* Each distinct failure is named once and then left alone: a damaged disc
       can repeat one for every image, and the log has to stay readable. Once
       the table is full the later ones stay unnamed instead of flooding it. */
    if (count < (int)(sizeof seen / sizeof seen[0])) {
        seen[count].kind = kind; seen[count].a = a; seen[count].b = b; seen[count].c = c; count++;
    } else {
        if (full) return;
        full = 1;
        fprintf(stderr, "tex1: further texture failures not shown\n");
        return;
    }
    switch (kind) {
    case RPT_TRANSFER:
        fprintf(stderr, "tex1: transfer format 0x%x at %ux%u not written, left out of video memory\n", a, b, c);
        break;
    case RPT_TEXTURE:
        fprintf(stderr, "tex1: texture format 0x%x at %ux%u not decoded, image left blank\n", a, b, c);
        break;
    case RPT_PALETTE:
        fprintf(stderr, "tex1: palette format 0x%x at %ux%u not decoded, image left blank\n", a, b, c);
        break;
    case RPT_PALETTE_DATA:
        fprintf(stderr, "tex1: palette at 0x%x has no 8x2 transfer, image left blank\n", a);
        break;
    case RPT_MIP:
        fprintf(stderr, "tex1: mip format 0x%x at %ux%u not decoded, levels dropped\n", a, b, c);
        break;
    case RPT_HEADER:
        if (a == 2) fprintf(stderr, "tex1: block of %u bytes is shorter than a header, block dropped\n", b);
        else if (a == 1) fprintf(stderr, "tex1: block of %u bytes does not start with Tex1, block dropped\n", b);
        else if (a == 3) fprintf(stderr, "tex1: no memory for %u images, block dropped\n", b);
        else fprintf(stderr, "tex1: header out of range (%u images, %u transfers), block dropped\n", b, c);
        break;
    case RPT_OFFSET:
        fprintf(stderr, "tex1: transfer format 0x%x at offset 0x%x starts past the block (%u bytes), block dropped\n", a, b, c);
        break;
    case RPT_SHORT:
        fprintf(stderr, "tex1: transfer format 0x%x at offset 0x%x has less data than it needs (%u bytes), block dropped\n", a, b, c);
        break;
    case RPT_BIG:
        fprintf(stderr, "tex1: texture format 0x%x is %ux%u, over 1024, block dropped\n", a, b, c);
        break;
    case RPT_MANY_LEVELS:
        fprintf(stderr, "tex1: texture format 0x%x asks for %u mip levels, over 6, block dropped\n", a, b);
        break;
    case RPT_ALLOC:
        fprintf(stderr, "tex1: no memory for a %ux%u texture, block dropped\n", b, c);
        break;
    }
}

static int upload(uint8_t *vram, const uint8_t *src, size_t avail, uint32_t tbp, uint32_t bw,
                  int psm, uint32_t w, uint32_t h)
{
    if (psm == PSMCT32) {
        if ((uint64_t)w * h * 4 > avail) return 0;
        for (uint32_t y = 0; y < h; y++)
            for (uint32_t x = 0; x < w; x++)
                memcpy(vram + addr32(tbp, bw, x, y), src + ((size_t)y * w + x) * 4, 4);
    } else if (psm == PSMCT16 || psm == PSMCT16S) {
        if ((uint64_t)w * h * 2 > avail) return 0;
        for (uint32_t y = 0; y < h; y++)
            for (uint32_t x = 0; x < w; x++) {
                const uint8_t *s = src + ((size_t)y * w + x) * 2;
                uint32_t a = addr16(tbp, bw, x, y, psm == PSMCT16S);
                vram[a] = s[0]; vram[a + 1] = s[1];
            }
    } else if (psm == PSMT8) {
        if ((uint64_t)w * h > avail) return 0;
        for (uint32_t y = 0; y < h; y++)
            for (uint32_t x = 0; x < w; x++)
                vram[addr8(tbp, bw, x, y)] = src[(size_t)y * w + x];
    } else if (psm == PSMT4) {
        if ((uint64_t)w * h / 2 > avail) return 0;
        for (uint32_t y = 0; y < h; y++)
            for (uint32_t x = 0; x < w; x++) {
                size_t i = (size_t)y * w + x;
                unsigned v = (src[i / 2] >> ((i & 1) * 4)) & 15, sh;
                uint32_t a = addr4(tbp, bw, x, y, &sh);
                vram[a] = (uint8_t)((vram[a] & ~(15u << sh)) | (v << sh));
            }
    } else {
        return 0;
    }
    return 1;
}

/* PSMCT16 is RGB5A1: five bits per colour, doubled, and the top bit is the
   GS alpha 0x80 (the same expansion as the 16-entry palettes). */
static void rgb5a1(uint16_t c, uint8_t out[4], uint8_t *native_alpha)
{
    for (unsigned k = 0; k < 3; k++) {
        unsigned v = (c >> (k * 5)) & 31;
        out[k] = (uint8_t)((v << 3) | (v >> 2));
    }
    unsigned a = c & 0x8000 ? 128u : 0u;
    out[3] = (uint8_t)(a ? 255 : 0);
    if (native_alpha) *native_alpha = (uint8_t)a;
}

/* Palette entry i (0..255) of a 32-bit CLUT stored at cbp with the default
   storage mode (CSM1): bits 3 and 4 of the index are swapped. PS2 alpha is
   0..0x80 for opaque; it is doubled and clamped to 0..255. */
static void clut(const uint8_t *vram, uint32_t cbp, unsigned i, uint8_t out[4], uint8_t *native_alpha)
{
    unsigned j = (i & 0xe7) | ((i & 0x08) << 1) | ((i & 0x10) >> 1);
    const uint8_t *p = vram + addr32(cbp, 1, j % 16, j / 16);
    *native_alpha=p[3];
    out[0] = p[0]; out[1] = p[1]; out[2] = p[2];
    out[3] = (uint8_t)(p[3] >= 0x80 ? 255 : p[3] * 2);
}

int tex1_decode(const uint8_t *d, size_t size, tex1_image **out)
{
    if (size < 0x30 || (size >= 4 && memcmp(d, "Tex1", 4))) {
        report_once(RPT_HEADER, size < 0x30 ? 2u : 1u, (unsigned)size, 0);
        return -1;
    }
    unsigned ntex = le16(d + 0x14), ntr = le16(d + 0x16);
    uint32_t ptex = le32(d + 0x18), ptr = le32(d + 0x1c);
    if ((uint64_t)ptex + 0x28ull * ntex > size || (uint64_t)ptr + 12ull * ntr > size) {
        report_once(RPT_HEADER, 0, ntex, ntr);
        return -1;
    }
    /* A block the authors left empty (no textures, no transfers) is not a
       failure: several cars carry four of them, and counting those as
       unsupported said the same thing as a real decode failure. */
    if (!ntex) {
        if (ntr) { report_once(RPT_HEADER, 0, ntex, ntr); return -1; }
        *out = NULL;
        return 0;
    }

    uint8_t *vram = calloc(1, VRAM_SIZE);
    tex1_image *imgs = calloc(ntex, sizeof *imgs);
    if (!vram || !imgs) { free(vram); free(imgs); report_once(RPT_HEADER, 3u, ntex, 0); return -1; }

    for (unsigned i = 0; i < ntr; i++) {
        const uint8_t *t = d + ptr + 12 * i;
        uint32_t p = le32(t), tbp = le16(t + 4), bw = t[6], psm = t[7], w = le16(t + 8), h = le16(t + 10);
        /* Tiny frame fills store a standalone 16-entry RGB5A1 palette. It is
           read straight out of the file below and is left out of video memory,
           where it would write over the image it belongs to. */
        if (psm == PSMCT16 && w == 8 && h == 2 && p <= size && size - p >= 32) continue;
        /* A transfer this decoder cannot write leaves that part of video
           memory as it was: the images that do not read it still draw. */
        if (!writable(psm)) { report_once(RPT_TRANSFER, psm, w, h); continue; }
        if (p >= size) { report_once(RPT_OFFSET, psm, p, (unsigned)size); goto fail; }
        if (!upload(vram, d + p, size - p, tbp, bw, (int)psm, w, h)) {
            report_once(RPT_SHORT, psm, p, (unsigned)(size - p));
            goto fail;
        }
    }

    for (unsigned i = 0; i < ntex; i++) {
        const uint8_t *t = d + ptex + 0x28 * i;
        uint64_t tex0 = le64(t), clamp = le64(t + 0x20);
        uint32_t tbp = tex0 & 0x3fff, tbw = (tex0 >> 14) & 0x3f, psm = (tex0 >> 20) & 0x3f;
        uint32_t tw = 1u << ((tex0 >> 26) & 0xf), th = 1u << ((tex0 >> 30) & 0xf);
        uint32_t cbp = (tex0 >> 37) & 0x3fff, csa = (tex0 >> 56) & 0x1f;
        uint32_t w = tw, h = th;
        /* REGION_CLAMP (mode 2) gives the image's own size. */
        if ((clamp & 3) == 2 && ((clamp >> 2) & 3) == 2) {
            uint32_t maxu = (clamp >> 14) & 0x3ff, maxv = (clamp >> 34) & 0x3ff;
            if (maxu < tw) w = maxu + 1;
            if (maxv < th) h = maxv + 1;
        }
        if (tw > 1024 || th > 1024) { report_once(RPT_BIG, psm, tw, th); goto fail; }
        imgs[i].width = (int)w; imgs[i].height = (int)h;
        imgs[i].tex_w = (int)tw; imgs[i].tex_h = (int)th;
        imgs[i].wrap_u=(uint8_t)(clamp&3);imgs[i].wrap_v=(uint8_t)((clamp>>2)&3);
        imgs[i].min_u=(uint16_t)((clamp>>4)&1023);imgs[i].max_u=(uint16_t)((clamp>>14)&1023);
        imgs[i].min_v=(uint16_t)((clamp>>24)&1023);imgs[i].max_v=(uint16_t)((clamp>>34)&1023);
        imgs[i].color_component=(uint8_t)((tex0>>34)&1);
        imgs[i].texture_function=(uint8_t)((tex0>>35)&3);
        {uint64_t tex1 = le64(t + 8); imgs[i].magnify=(uint8_t)((tex1>>5)&1); imgs[i].minify=(uint8_t)((tex1>>6)&7);}
        imgs[i].native_alpha=calloc((size_t)w*h,1);
        imgs[i].rgba = calloc((size_t)w * h, 4);
        if (!imgs[i].rgba || !imgs[i].native_alpha) { report_once(RPT_ALLOC, 0, w, h); goto fail; }
        /* A format this decoder cannot read leaves this image in the list,
           blank, and goes on with the next one: one image used to drop the
           whole file with it. */
        if (!readable(psm)) { report_once(RPT_TEXTURE, psm, tw, th); continue; }
        uint8_t pal[256][4],palalpha[256];
        unsigned cpsm = (unsigned)((tex0 >> 51) & 15);
        if (cpsm == 2 && psm == PSMT4 && csa == 0 && !((tex0 >> 55) & 1)) {
            const uint8_t *palette = NULL;
            for (unsigned j = 0; j < ntr; j++) {
                const uint8_t *tr = d + ptr + 12 * j;
                uint32_t off = le32(tr);
                if (tr[7] == PSMCT16 && le16(tr + 4) == cbp && le16(tr + 8) == 8 && le16(tr + 10) == 2 &&
                    off <= size && size - off >= 32) palette = d + off;
            }
            if (!palette) { report_once(RPT_PALETTE_DATA, cbp, tw, th); continue; }
            for (unsigned k = 0; k < 16; k++)
                rgb5a1(le16(palette + k * 2), pal[k], &palalpha[k]);
        } else if (cpsm != 0) { report_once(RPT_PALETTE, cpsm, tw, th); continue; }
        else if (psm == PSMT8)
            for (unsigned k = 0; k < 256; k++) clut(vram, cbp, k, pal[k], &palalpha[k]);
        else if (psm == PSMT4)
            for (unsigned k = 0; k < 16; k++) clut(vram, cbp, csa * 16 + k, pal[k], &palalpha[k]);
        uint8_t *o = imgs[i].rgba;
        for (uint32_t y = 0; y < h; y++)
            for (uint32_t x = 0; x < w; x++, o += 4) {
                if (psm == PSMT8) {
                    unsigned index=vram[addr8(tbp,tbw,x,y)];memcpy(o,pal[index],4);imgs[i].native_alpha[y*w+x]=palalpha[index];
                } else if (psm == PSMT4) {
                    unsigned sh; uint32_t a = addr4(tbp, tbw, x, y, &sh);
                    unsigned index=(vram[a]>>sh)&15;memcpy(o,pal[index],4);imgs[i].native_alpha[y*w+x]=palalpha[index];
                } else if (psm == PSMCT32) {
                    const uint8_t *p = vram + addr32(tbp, tbw, x, y);
                    imgs[i].native_alpha[y*w+x]=p[3];
                    o[0] = p[0]; o[1] = p[1]; o[2] = p[2];
                    o[3] = (uint8_t)(p[3] >= 0x80 ? 255 : p[3] * 2);
                } else if (psm == PSMCT24) {
                    /* the 32-bit layout, its alpha byte not read: the GS
                       gives TEXA.TA0, taken as 0x80 (the course's spark
                       image, header +0x9C, drawn with TCC 0 anyway) */
                    const uint8_t *p = vram + addr32(tbp, tbw, x, y);
                    imgs[i].native_alpha[y*w+x]=0x80;
                    o[0] = p[0]; o[1] = p[1]; o[2] = p[2];
                    o[3] = 255;
                } else if (psm == PSMCT16 || psm == PSMCT16S) {
                    const uint8_t *p = vram + addr16(tbp, tbw, x, y, psm == PSMCT16S);
                    rgb5a1((uint16_t)(p[0] | p[1] << 8), o, &imgs[i].native_alpha[y*w+x]);
                }
            }
        /* Tex1 descriptor +08 TEX1.MXL, +10/+18 MIPTBP1/2: use the
           disc's authored levels, not averages generated from level zero.
           Base address is in 256-byte blocks; each level packs TBP14/TBW6. */
        unsigned levels=(unsigned)((le64(t+8)>>2)&7);
        if(levels>6){report_once(RPT_MANY_LEVELS,psm,levels,0);goto fail;}
        imgs[i].mip_count=(uint8_t)levels;
        for(unsigned level=1;level<=levels;level++) {
            uint64_t reg=le64(t+(level<=3?16:24));
            unsigned shift=((level-1)%3)*20;
            uint32_t bp=(uint32_t)(reg>>shift)&0x3fff,bw=(uint32_t)(reg>>(shift+14))&63;
            uint32_t mw=tw>>level,mh=th>>level;if(!mw)mw=1;if(!mh)mh=1;
            uint8_t *pixels=malloc((size_t)mw*mh*4);if(!pixels){report_once(RPT_ALLOC,0,mw,mh);goto fail;}
            imgs[i].mip_rgba[level-1]=pixels;
            for(uint32_t y=0;y<mh;y++)for(uint32_t x=0;x<mw;x++) {
                uint8_t *pixel=pixels+((size_t)y*mw+x)*4;
                if(psm==PSMT8 || psm==PSMT4) {
                    unsigned index;
                    if(psm==PSMT8)index=vram[addr8(bp,bw,x,y)];
                    else {unsigned sh;uint32_t a=addr4(bp,bw,x,y,&sh);index=(vram[a]>>sh)&15;}
                    memcpy(pixel,pal[index],3);pixel[3]=palalpha[index];
                } else if(psm==PSMCT32 || psm==PSMCT24) {
                    memcpy(pixel,vram+addr32(bp,bw,x,y),4);if(psm==PSMCT24)pixel[3]=128;
                } else if(psm==PSMCT16 || psm==PSMCT16S) {
                    /* A mip level carries the GS alpha, which is what the
                       PSMCT32 branch copies above. */
                    const uint8_t *p=vram+addr16(bp,bw,x,y,psm==PSMCT16S);
                    rgb5a1((uint16_t)(p[0]|p[1]<<8),pixel,NULL);
                    pixel[3]=(uint8_t)(p[1]&0x80?128:0);
                } else {
                    /* No branch for this format: its levels are dropped. */
                    report_once(RPT_MIP,psm,mw,mh);imgs[i].mip_count=0;
                }
            }
        }
    }
    free(vram);
    *out = imgs;
    return (int)ntex;
fail:
    free(vram);
    tex1_free(imgs, (int)ntex);
    return -1;
}

void tex1_free(tex1_image *images, int count)
{
    if (!images) return;
    for (int i = 0; i < count; i++){
        free(images[i].rgba);free(images[i].native_alpha);
        for(unsigned level=0;level<6;level++)free(images[i].mip_rgba[level]);}
    free(images);
}
