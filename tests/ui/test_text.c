/* Synthetic fixtures only. No game assets are needed by these tests. */
#include "font.h"
#include "rtext.h"
#include "glyph_sdf.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { put16(p, v); put16(p + 2, v >> 16); }

static void rtext_test(void)
{
    uint8_t b[128] = {0};
    memcpy(b, "RT03", 4); put32(b + 12, 2);
    put32(b + 16, 64); put32(b + 20, 1); put32(b + 24, 48);
    put32(b + 32, 72); put32(b + 36, 1); put32(b + 40, 56);
    put32(b + 48, 80); put32(b + 52, 88);
    put32(b + 56, 80); put32(b + 60, 96);
    memcpy(b + 64, "PageA", 6); memcpy(b + 72, "PageB", 6);
    memcpy(b + 80, "item", 5); memcpy(b + 88, "First", 6); memcpy(b + 96, "Second", 7);
    gt4_rtext r;
    CHECK(gt4_rtext_open(&r, b, sizeof b));
    CHECK(!strcmp(gt4_rtext_get(&r, "PageA", "item"), "First"));
    CHECK(!strcmp(gt4_rtext_get(&r, "PageB", "item"), "Second"));
    CHECK(!gt4_rtext_get(&r, "absent", "item"));
    CHECK(!gt4_rtext_get(&r, "PageA", "absent"));
    for (size_t n = 0; n < 103; n++) CHECK(!gt4_rtext_open(&r, b, n));
    put32(b + 52, 0xffffffff); CHECK(!gt4_rtext_open(&r, b, sizeof b)); CHECK(!r.pages);
    put32(b + 52, 88); put32(b + 20, 0xffffffff); CHECK(!gt4_rtext_open(&r, b, sizeof b));
    const char *s = "A\xc2\xa9\xe2\x82\xac\xf0\x9f\x98\x80";
    CHECK(gt4_text_next(&s) == 'A'); CHECK(gt4_text_next(&s) == 0xa9);
    CHECK(gt4_text_next(&s) == 0x20ac); CHECK(gt4_text_next(&s) == 0x1f600); CHECK(gt4_text_next(&s) == 0);
    s = "\xe2"; CHECK(gt4_text_next(&s) == 0xfffd); CHECK(!*s);
    s = "\xed\xa0\x80"; CHECK(gt4_text_next(&s) == 0xfffd);
    s = "\xc0\xaf"; CHECK(gt4_text_next(&s) == 0xfffd);
}

static void font_test(void)
{
    uint8_t b[292] = {0};
    memcpy(b, "FT01", 4); put16(b + 16, 1); put16(b + 18, 1);
    put32(b + 20, 48); put32(b + 24, 52); put32(b + 48, 56); put32(b + 52, 128);
    put32(b + 56, 88); put32(b + 60, 96); put16(b + 64, 'A'); put16(b + 66, 'A');
    b[69] = b[70] = 2; memcpy(b + 88, "test", 5);
    b[97] = 254; b[98] = b[99] = 2; put32(b + 100, 3);
    uint8_t *t = b + 128;
    memcpy(t, "Tex1", 4); put16(t + 20, 1); put16(t + 22, 1);
    put32(t + 24, 48); put32(t + 28, 88);
    uint64_t tex0 = (1ull << 14) | (2ull << 26) | (2ull << 30);
    put32(t + 48, (uint32_t)tex0); put32(t + 52, (uint32_t)(tex0 >> 32));
    put32(t + 88, 100); t[94] = 1; put16(t + 96, 4); put16(t + 98, 4);
    for (int i = 0; i < 16; i++) { t[100 + i * 4] = 255; t[103 + i * 4] = 128; }
    gt4_font f;
    CHECK(gt4_font_decode(b, sizeof b, "test", &f) == 1);
    CHECK(f.count == 1 && f.height == 2 && f.baseline == 2);
    const gt4_glyph *g = gt4_font_glyph(&f, 'A');
    CHECK(g && g->y == -2 && g->w == 2 && g->advance == 3);
    CHECK(!gt4_font_glyph(&f, 'B'));
    CHECK(gt4_font_line_width(&f,"AA",1)==7);
    CHECK(gt4_font_line_width(&f,"A\nAA",1)==3);
    CHECK(gt4_font_line_width(&f,"",1)==0);
    CHECK(gt4_font_line_width(&f,"AA",0)==6);
    CHECK(gt4_font_line_width(&f,"\tAA\t",1)==7);
    CHECK(f.atlases[0].native_alpha[0]==128);
    CHECK(f.atlases[0].width == 4 && f.atlases[0].rgba[0] == 255 && f.atlases[0].rgba[3] == 255);
    gt4_font_free(&f);
    t[103]=192;
    CHECK(gt4_font_decode(b,sizeof b,"test",&f)==1);
    CHECK(f.atlases[0].rgba[3]==255 && f.atlases[0].native_alpha[0]==192);
    gt4_font_free(&f);t[103]=128;
    CHECK(gt4_font_decode(b, sizeof b, "missing", &f) == 0);
    for (size_t n = 0; n < sizeof b; n++) CHECK(gt4_font_decode(b, n, "test", &f) == -1);
    b[98] = b[99] = 255;
    CHECK(gt4_font_decode(b, sizeof b, "test", &f) == 1);
    CHECK(f.glyphs[0].w == 0 && f.glyphs[0].advance == 3); gt4_font_free(&f);
    b[98] = b[99] = 2; put16(b + 104, 4);
    CHECK(gt4_font_decode(b, sizeof b, "test", &f) == -1);
    put32(b + 60, 0xffffffff); CHECK(gt4_font_decode(b, sizeof b, "test", &f) == -1);
}
static void frame_palette_test(void)
{
    uint8_t b[160] = {0};
    memcpy(b, "Tex1", 4); put16(b+0x14,1); put16(b+0x16,2);
    put32(b+0x18,0x30); put32(b+0x1c,0x58);
    uint64_t tex0 = 1ull | (2ull<<14) | (0x14ull<<20) | (3ull<<26) | (2ull<<30) | (2ull<<51);
    put32(b+0x30,(uint32_t)tex0); put32(b+0x34,(uint32_t)(tex0>>32));
    put32(b+0x58,0x70); put16(b+0x5c,1); b[0x5e]=2; b[0x5f]=0x14;
    put16(b+0x60,8); put16(b+0x62,4);
    put32(b+0x64,0x80); b[0x6a]=1; b[0x6b]=2; put16(b+0x6c,8); put16(b+0x6e,2);
    b[0x70]=0x10; put16(b+0x80,0x801f); /* opaque red, then transparent black */
    tex1_image *im = NULL;
    CHECK(tex1_decode(b,sizeof b,&im)==1);
    CHECK(im[0].width==8 && im[0].height==4);
    CHECK(im[0].rgba[0]==255 && im[0].rgba[1]==0 && im[0].rgba[3]==255);
    CHECK(im[0].rgba[7]==0);
    CHECK(im[0].native_alpha[0]==128 && im[0].native_alpha[1]==0);
    CHECK(im[0].color_component==0 && im[0].texture_function==0);tex1_free(im,1);
    uint64_t clamp=2ull|(2ull<<2)|(2ull<<4)|(6ull<<14)|(1ull<<24)|(3ull<<34);
    put32(b+0x50,(uint32_t)clamp);put32(b+0x54,(uint32_t)(clamp>>32));
    CHECK(tex1_decode(b,sizeof b,&im)==1);
    CHECK(im[0].wrap_u==2 && im[0].wrap_v==2);
    CHECK(im[0].min_u==2 && im[0].max_u==6 && im[0].min_v==1 && im[0].max_v==3);
    CHECK(im[0].width==7 && im[0].height==4);tex1_free(im,1);
    for(size_t n=0;n<sizeof b;n++) { im=NULL; CHECK(tex1_decode(b,n,&im)==-1); }
}
static void sdf_progress(void *user){(*(unsigned *)user)++;}
static void sdf_loading_test(void)
{
    uint8_t rgba[32*32*4]={0};
    for(int y=8;y<24;y++)for(int x=9;x<23;x++)rgba[(y*32+x)*4+3]=255;
    int w1,h1,w2,h2;unsigned ticks=0;
    uint8_t *a=gt4_glyph_sdf(rgba,32,32,4,&w1,&h1);
    uint8_t *b=gt4_glyph_sdf_presented(rgba,32,32,4,&w2,&h2,sdf_progress,&ticks);
    CHECK(a && b && ticks>8 && w1==w2 && h1==h2);
    CHECK(!memcmp(a,b,(size_t)w1*h1*4));free(a);free(b);
}
/* KNJI: the subfont's table picks the glyph; outside the range or 0xFFFF
   uses glyph 0; the left pixel is the low nibble; palette 2 is the ink. */
static void kanji_test(void)
{
    uint8_t b[0x110] = {0};
    memcpy(b, "KNJI", 4); put16(b + 0x10, 1); put16(b + 0x12, 2); put32(b + 0x14, 0xa0); put32(b + 0x18, 0x20);
    for (int i = 0; i < 16; i++) { put32(b + 0x20 + 4 * i, 0x80000000u); put32(b + 0x60 + 4 * i, 0x00ffffffu | (uint32_t)(i * 8) << 24); }
    put32(b + 0xa0, 0xb0);
    put16(b + 0xb0, 0x25cf); put16(b + 0xb2, 0x25d0); put32(b + 0xb4, 0x100); put32(b + 0xb8, 2);
    put16(b + 0xbc, 2); put16(b + 0xbe, 2); put32(b + 0xc0, 0xf0);
    put16(b + 0xf0, 1); put16(b + 0xf2, 0xffff);
    b[0x100] = 0x00; b[0x101] = 0x00; b[0x102] = 0x0f; b[0x103] = 0x00;
    uint8_t ink[16], out[16]; int w = 0, h = 0;
    CHECK(gt4_kanji_glyph(b, sizeof b, 0, 0x25cf, &w, &h, ink, out, sizeof ink) && w == 2 && h == 2);
    CHECK(ink[3] == 240 && ink[7] == 0 && out[3] == 255);
    CHECK(gt4_kanji_glyph(b, sizeof b, 5, 0x25d0, &w, &h, ink, out, sizeof ink) && ink[3] == 0);
    CHECK(gt4_kanji_glyph(b, sizeof b, 0, 0x41, &w, &h, ink, out, sizeof ink) && ink[3] == 0);
    CHECK(!gt4_kanji_glyph(b, sizeof b, 0, 0x25cf, &w, &h, ink, out, 15));
    for (size_t n = 0; n < 0x102; n++) CHECK(!gt4_kanji_glyph(b, n, 0, 0x25cf, &w, &h, ink, out, sizeof ink));
}

int main(void) { sdf_loading_test(); rtext_test(); font_test(); frame_palette_test(); kanji_test(); puts("native text formats: passed"); return 0; }
