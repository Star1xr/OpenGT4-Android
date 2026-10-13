#include "font.h"
#include "rtext.h"
#include <stdlib.h>
#include <string.h>

static unsigned u16(const uint8_t *p) { return p[0] | (unsigned)p[1] << 8; }
static uint32_t u32(const uint8_t *p) { return u16(p) | (uint32_t)u16(p + 2) << 16; }
static int span(size_t n, uint32_t p, uint64_t len) { return p <= n && len <= n - p; }

void gt4_font_free(gt4_font *f)
{
    free(f->glyphs);
    free(f->contours);
    tex1_free(f->atlases, f->natlases);
    memset(f, 0, sizeof *f);
}

const gt4_glyph *gt4_font_glyph(const gt4_font *f, unsigned c)
{
    return c >= f->first && c - f->first < f->count ? &f->glyphs[c - f->first] : NULL;
}

int gt4_font_decode(const uint8_t *d, size_t n, const char *name, gt4_font *out)
{
    gt4_font f = {0};
    memset(out, 0, sizeof *out);
    if (n < 48 || memcmp(d, "FT01", 4)) return -1;
    unsigned count = u16(d + 16);
    uint32_t faces = u32(d + 20), textures = u32(d + 24);
    /* Both offset arrays have one entry per face in the supported disc. */
    if (!span(n, faces, 4ull * count) || !span(n, textures, 4ull * count)) return -1;
    for (unsigned i = 0; i < count; i++) {
        uint32_t off = u32(d + faces + 4 * i);
        if (!span(n, off, 32)) return -1;
        const uint8_t *h = d + off;
        uint32_t label = u32(h);
        if (label >= n || !memchr(d + label, 0, n - label)) return -1;
        if (strcmp((const char *)d + label, name)) continue;
        f.first = u16(h + 8);
        unsigned last = u16(h + 10);
        if (last < f.first) return -1;
        f.count = last - f.first + 1;
        f.height = h[13]; f.baseline = h[14];
        f.contour_height=h[15];f.pair_gap=h[21];
        f.kanji_w=h[16];f.kanji_h=h[17];f.kanji_x=(int8_t)h[18];f.kanji_y=(int8_t)h[19];
        f.kanji_advance=h[20];f.kanji_set=h[22];
        f.space_advance=h[12]>>1;
        uint32_t glyphs = u32(h + 4), tex = u32(d + textures + 4 * i);
        if (!span(n, glyphs, 20ull * f.count) || tex >= n) return -1;
        f.natlases = tex1_decode(d + tex, n - tex, &f.atlases);
        if (f.natlases <= 0) goto fail;
        f.glyphs = calloc(f.count, sizeof *f.glyphs);
        if (!f.glyphs) goto fail;
        if(f.contour_height){
            f.contours=malloc((size_t)f.count*f.contour_height*2);
            if(!f.contours)goto fail;
        }
        for (unsigned j = 0; j < f.count; j++) {
            const uint8_t *g = d + glyphs + 20 * j;
            gt4_glyph *a = &f.glyphs[j];
            a->x = (int8_t)g[0]; a->y = (int8_t)g[1];
            a->w = g[2]; a->h = g[3]; a->advance = (int32_t)u32(g + 4);
            a->bitmap_width=g[2];
            a->u = u16(g + 8); a->v = u16(g + 10);
            if(f.contours)for(unsigned side=0;side<2;side++){
                uint32_t contour=u32(g+12+side*4);
                if(!span(n,contour,f.contour_height))goto fail;
                memcpy(f.contours+((size_t)j*2+side)*f.contour_height,d+contour,f.contour_height);
            }
            /* A space has ff/ff dimensions: advance without drawing. */
            if (a->w == 255 && a->h == 255) a->w = a->h = 0;
            if (a->advance < 0 || a->advance > 1024 ||
                a->u + a->w > f.atlases[0].width || a->v + a->h > f.atlases[0].height) goto fail;
        }
        if(f.first<=32 && 32-f.first<f.count)f.space_advance=(unsigned)f.glyphs[32-f.first].advance;
        *out = f;
        return 1;
    }
    return 0;
fail:
    gt4_font_free(&f);
    return -1;
}

float gt4_font_line_width(const gt4_font *f, const char *text, float spacing)
{
    float width=0; int count=0;
    while (*text && *text!='\n' && *text!='\r') {
        unsigned code=gt4_text_next(&text);
        if(code<32)continue; /* RT03 contains nonprinting tabs, including am_sunday. */
        if(code>=GT4_KANJI_FIRST && f->kanji_advance){   /* 00492748: +1 after a glyph */
            if(count++)width+=spacing+1;
            width+=f->kanji_advance;continue;
        }
        const gt4_glyph *g=gt4_font_glyph(f,code);
        if (!g) g=gt4_font_glyph(f,'?');
        if (!g) continue;
        if (count++) width+=spacing;
        width+=g->advance;
    }
    return width;
}

int gt4_kanji_glyph(const uint8_t *d, size_t n, unsigned set, unsigned code,
                    int *w, int *h, uint8_t *ink, uint8_t *outlined, size_t capacity)
{
    if(!d || n<0xb0 || memcmp(d,"KNJI",4))return 0;
    unsigned sets=u16(d+0x10),palettes=u16(d+0x12);uint32_t list=u32(d+0x14),clut=u32(d+0x18);
    if(palettes<2 || !span(n,clut,0x80))return 0;
    /* 00491C10: an index past the count selects subfont 0. */
    if(set>=sets)set=0;
    if(!sets || !span(n,list,4ull*sets))return 0;
    uint32_t o=u32(d+list+4*set);if(!span(n,o,0x18))return 0;
    unsigned first=u16(d+o),last=u16(d+o+2),gw=u16(d+o+0xc),gh=u16(d+o+0xe);
    uint32_t glyphs=u32(d+o+4),bytes=u32(d+o+8),table=u32(d+o+0x10);
    if(last<first || !gw || !gh || (size_t)gw*gh*4>capacity || bytes<((size_t)gw*gh+1)/2 ||
       !span(n,table,2ull*(last-first+1)))return 0;
    unsigned index=code>=first && code<=last?u16(d+table+2*(code-first)):0;
    if(index==0xffff)index=0;
    uint64_t at=glyphs+(uint64_t)index*bytes;
    if(at>n || bytes>n-at)return 0;
    const uint8_t *g=d+glyphs+(size_t)index*bytes;
    for(unsigned i=0;i<gw*gh;i++){
        unsigned c=(g[i/2]>>(i&1?4:0))&15;   /* PSMT4: the left pixel in the low nibble */
        const uint8_t *a=d+clut+0x40+4*c,*b=d+clut+4*c;
        for(unsigned k=0;k<3;k++){ink[i*4+k]=a[k];outlined[i*4+k]=b[k];}
        /* PS2 alpha 0x80 is opaque */
        ink[i*4+3]=(uint8_t)(a[3]>=0x80?255:a[3]*2);outlined[i*4+3]=(uint8_t)(b[3]>=0x80?255:b[3]*2);
    }
    *w=(int)gw;*h=(int)gh;return 1;
}
