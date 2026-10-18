/* Tex1 PSMCT16/PSMCT16S, and what happens to the rest of a file when one
   image or one transfer uses a format the decoder does not take.

   The 16-bit layout is checked through a 32-bit read of the same video
   memory: a 16-bit block is 16x8 pixels, and the two halves of every 32-bit
   unit are the pixels x and x+8 of one of its columns. Where those units land
   is the block and column order the 32-bit tables already use, so a 32-bit
   texture over the same base address shows what the 16-bit upload wrote. The
   expected words below are read straight off those tables (docs/TEX1_FORMAT.md),
   not from the decoder under test. */
#include "tex1.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do{if(!(x)){fprintf(stderr,"tex1 formats line %d: %s\n",__LINE__,#x);return 1;}}while(0)

#define TEXTURES 0x30

static unsigned char data[8192];

static void le16p(unsigned char *p, unsigned v)
{p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);}
static void le32p(unsigned char *p, unsigned v)
{le16p(p,v&0xffffu);le16p(p+2,v>>16);}
static void le64p(unsigned char *p, unsigned long long v)
{le32p(p,(unsigned)v);le32p(p+4,(unsigned)(v>>32));}

/* TEX0: TBP0..13, TBW14..19, PSM20..25, TW26..29, TH30..33. */
static void texture(unsigned char *t, unsigned psm, unsigned tw, unsigned th, unsigned tbp, unsigned tbw)
{
    le64p(t, (unsigned long long)tbp | ((unsigned long long)tbw << 14) |
             ((unsigned long long)psm << 20) | ((unsigned long long)tw << 26) |
             ((unsigned long long)th << 30));
}

/* A transfer: data offset, TBP, buffer width, pixel format, width, height. */
static void transfer(unsigned char *t, unsigned off, unsigned tbp, unsigned bw,
                     unsigned psm, unsigned w, unsigned h)
{
    le32p(t, off); le16p(t + 4, tbp); t[6] = (unsigned char)bw; t[7] = (unsigned char)psm;
    le16p(t + 8, w); le16p(t + 10, h);
}

/* Header with ntex textures and ntr transfers; returns the data offset. */
static unsigned build(unsigned ntex, unsigned ntr)
{
    memset(data, 0, sizeof data);
    memcpy(data, "Tex1", 4);
    le32p(data + 0x0c, (unsigned)sizeof data);
    le16p(data + 0x14, ntex);
    le16p(data + 0x16, ntr);
    le32p(data + 0x18, TEXTURES);
    le32p(data + 0x1c, TEXTURES + 0x28 * ntex);
    return TEXTURES + 0x28 * ntex + 12 * ntr;
}

static int same(const unsigned char *p, unsigned r, unsigned g, unsigned b, unsigned a)
{return p[0]==r && p[1]==g && p[2]==b && p[3]==a;}

/* 0x8000 | v: opaque RGB5A1, the low bits the pixel's own number. */
static unsigned value(unsigned x, unsigned y, unsigned width)
{return 0x8000u | (y * width + x + 1);}

/* The doubling the decoder applies to five-bit channels. */
static unsigned expand(unsigned v)
{return (v << 3) | (v >> 2);}

static int case_16bit(void)
{
    unsigned at = build(2, 1);
    texture(data + TEXTURES, 0x02, 4, 4, 0, 1);            /* PSMCT16 16x16 */
    texture(data + TEXTURES + 0x28, 0x00, 4, 3, 0, 1);     /* PSMCT32 16x8, same memory */
    transfer(data + TEXTURES + 0x50, at, 0, 1, 0x02, 16, 16);
    for (unsigned y = 0; y < 16; y++) for (unsigned x = 0; x < 16; x++)
        le16p(data + at + (y * 16 + x) * 2, value(x, y, 16));

    tex1_image *im = NULL;
    CHECK(tex1_decode(data, at + 16 * 16 * 2, &im) == 2);
    CHECK(im[0].rgba && im[0].native_alpha && im[0].width == 16 && im[0].height == 16);
    /* RGB5A1, alpha 0x80 for the top bit. */
    CHECK(same(im[0].rgba + 0, expand(1), 0, 0, 255) && im[0].native_alpha[0] == 128);
    CHECK(same(im[0].rgba + 4, expand(2), 0, 0, 255));
    CHECK(same(im[0].rgba + 8 * 4, expand(9), 0, 0, 255));           /* the second 8 columns */
    CHECK(same(im[0].rgba + (15 * 16 + 15) * 4, 0, expand(8), 0, 255));
    CHECK(im[0].mip_count == 0);

    /* The 32-bit read of the same blocks. Each word holds the pixels x and
       x+8 of a column, placed by the 32-bit block and column tables. */
    CHECK(im[1].rgba && im[1].width == 16 && im[1].height == 8);
    const unsigned char *o = im[1].rgba;
    CHECK(same(o + (0 * 16 + 0) * 4, 0x01, 0x80, 0x09, 255));   /* (0,0) and (8,0) */
    CHECK(same(o + (0 * 16 + 1) * 4, 0x02, 0x80, 0x0a, 255));   /* (1,0) and (9,0) */
    CHECK(same(o + (1 * 16 + 0) * 4, 0x11, 0x80, 0x19, 255));   /* (0,1) and (8,1) */
    CHECK(same(o + (2 * 16 + 0) * 4, 0x21, 0x80, 0x29, 255));   /* (0,2) and (8,2) */
    CHECK(same(o + (0 * 16 + 8) * 4, 0x81, 0x80, 0x89, 255));   /* (0,8) and (8,8) */
    CHECK(same(o + (1 * 16 + 8) * 4, 0x91, 0x80, 0x99, 255));   /* (0,9) and (8,9) */
    CHECK(im[1].native_alpha[0] == 0x80);
    tex1_free(im, 2);
    return 0;
}

static int case_16s(void)
{
    /* PSMCT16S holds the same blocks in another order: the third block of a
       page is block 16, where PSMCT16 has block 8. */
    unsigned at = build(1, 1);
    texture(data + TEXTURES, 0x00, 6, 4, 0, 1);            /* PSMCT32 64x16 */
    transfer(data + TEXTURES + 0x28, at, 0, 1, 0x0a, 64, 16);
    for (unsigned y = 0; y < 16; y++) for (unsigned x = 0; x < 64; x++)
        le16p(data + at + (y * 64 + x) * 2, value(x, y, 64));

    tex1_image *im = NULL;
    CHECK(tex1_decode(data, at + 64 * 16 * 2, &im) == 1);
    const unsigned char *o = im->rgba;
    CHECK(same(o + (0 * 64 + 32) * 4, 0x21, 0x80, 0x29, 255));   /* (32,0) and (40,0) */
    CHECK(same(o + (8 * 64 + 32) * 4, 0x31, 0x80, 0x39, 255));   /* (48,0) and (56,0) */
    tex1_free(im, 1);
    return 0;
}

static int case_mips(void)
{
    /* 16-bit mip levels come from their own base address, as the 32-bit ones
       do, and carry the GS alpha rather than the doubled one. */
    unsigned at = build(1, 2);
    unsigned char *t = data + TEXTURES;
    texture(t, 0x02, 4, 4, 0, 1);
    le64p(t + 8, 1ull << 2);                    /* TEX1.MXL: one level */
    le64p(t + 16, 32ull | (1ull << 14));        /* MIPTBP1 level 1: TBP 32, TBW 1 */
    transfer(data + TEXTURES + 0x28, at, 0, 1, 0x02, 16, 16);
    transfer(data + TEXTURES + 0x34, at + 16 * 16 * 2, 32, 1, 0x02, 8, 8);
    for (unsigned y = 0; y < 16; y++) for (unsigned x = 0; x < 16; x++)
        le16p(data + at + (y * 16 + x) * 2, value(x, y, 16));
    for (unsigned y = 0; y < 8; y++) for (unsigned x = 0; x < 8; x++)
        le16p(data + at + 16 * 16 * 2 + (y * 8 + x) * 2, value(x, y, 8));

    tex1_image *im = NULL;
    CHECK(tex1_decode(data, at + 16 * 16 * 2 + 8 * 8 * 2, &im) == 1);
    CHECK(im->mip_count == 1 && im->mip_rgba[0]);
    CHECK(same(im->mip_rgba[0] + 0, expand(1), 0, 0, 128));
    CHECK(same(im->mip_rgba[0] + (7 * 8 + 7) * 4, 0, expand(2), 0, 128));
    tex1_free(im, 1);
    return 0;
}

static int case_isolation(void)
{
    /* An image in a format the decoder does not read: blank in place, and the
       image next to it still decodes. */
    unsigned at = build(2, 1);
    texture(data + TEXTURES, 0x1b, 4, 4, 0, 1);            /* PSMT8H */
    texture(data + TEXTURES + 0x28, 0x00, 2, 2, 0, 1);     /* PSMCT32 4x4 */
    transfer(data + TEXTURES + 0x50, at, 0, 1, 0x00, 4, 4);
    for (unsigned i = 0; i < 16; i++) {
        data[at + i * 4] = 10; data[at + i * 4 + 1] = 20;
        data[at + i * 4 + 2] = 30; data[at + i * 4 + 3] = 0x80;
    }
    tex1_image *im = NULL;
    CHECK(tex1_decode(data, at + 64, &im) == 2);
    CHECK(im[0].rgba && im[0].native_alpha);
    for (unsigned i = 0; i < 16; i++) CHECK(!im[0].rgba[i * 4] && !im[0].rgba[i * 4 + 3] && !im[0].native_alpha[i]);
    CHECK(im[1].width == 4 && im[1].height == 4);
    for (unsigned i = 0; i < 16; i++)
        CHECK(same(im[1].rgba + i * 4, 10, 20, 30, 255) && im[1].native_alpha[i] == 0x80);
    tex1_free(im, 2);
    return 0;
}

static int case_transfer(void)
{
    /* A transfer in a format the decoder cannot write (PSMCT24) leaves video
       memory alone: the image that reads it is blank, and the file is not
       dropped. */
    unsigned at = build(1, 1);
    texture(data + TEXTURES, 0x00, 2, 2, 0, 1);
    transfer(data + TEXTURES + 0x28, at, 0, 1, 0x01, 4, 4);
    memset(data + at, 0xaa, 64);
    tex1_image *im = NULL;
    CHECK(tex1_decode(data, at + 64, &im) == 1);
    for (unsigned i = 0; i < 16; i++) CHECK(!im->rgba[i * 4] && !im->rgba[i * 4 + 3]);
    tex1_free(im, 1);
    return 0;
}

static int case_palette_missing(void)
{
    /* A 4-bit image whose 8x2 palette transfer is not in the file. */
    unsigned at = build(1, 0);
    unsigned char *t = data + TEXTURES;
    le64p(t, (0x14ull << 20) | (4ull << 26) | (4ull << 30) | (2ull << 51));   /* PSMT4, CPSM 2 */
    tex1_image *im = NULL;
    CHECK(tex1_decode(data, at, &im) == 1);
    CHECK(im->rgba && im->native_alpha);
    for (unsigned i = 0; i < 256; i++) CHECK(!im->rgba[i * 4 + 3] && !im->native_alpha[i]);
    tex1_free(im, 1);
    return 0;
}

static int case_truncation(void)
{
    unsigned at = build(1, 1);
    texture(data + TEXTURES, 0x02, 4, 4, 0, 1);
    transfer(data + TEXTURES + 0x28, at, 0, 1, 0x02, 16, 16);
    for (unsigned n = 0; n < at + 16 * 16 * 2; n++) {
        tex1_image *im = NULL;
        CHECK(tex1_decode(data, n, &im) == -1);
    }
    return 0;
}

int main(void)
{
    if (case_16bit() || case_16s() || case_mips() || case_isolation() ||
        case_transfer() || case_palette_missing() || case_truncation()) return 1;
    puts("tex1 16-bit formats and isolation: passed");
    return 0;
}
