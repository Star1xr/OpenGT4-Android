#include "rtext.h"
#include <string.h>
static uint32_t u32(const uint8_t *p) { return p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static const char *str(const gt4_rtext *r, uint32_t off)
{
    return off < r->size && memchr(r->data + off, 0, r->size - off) ? (const char *)r->data + off : NULL;
}
int gt4_rtext_open(gt4_rtext *r, const uint8_t *d, size_t n)
{
    memset(r, 0, sizeof *r);
    if (n < 16 || memcmp(d, "RT03", 4)) return 0;
    unsigned pages = u32(d + 12);
    if (pages > (n - 16) / 16) return 0;
    gt4_rtext tmp = {d, n, pages};
    for (unsigned i = 0; i < pages; i++) {
        const uint8_t *p = d + 16 + i * 16;
        unsigned count = u32(p + 4), off = u32(p + 8);
        if (!str(&tmp, u32(p)) || off > n || count > (n - off) / 8) return 0;
        for (unsigned j = 0; j < count; j++)
            if (!str(&tmp, u32(d + off + j * 8)) || !str(&tmp, u32(d + off + j * 8 + 4))) return 0;
    }
    *r = tmp;
    return 1;
}
const char *gt4_rtext_get(const gt4_rtext *r, const char *page, const char *key)
{
    if (!page || !key) return NULL;
    for (unsigned i = 0; i < r->pages; i++) {
        const uint8_t *p = r->data + 16 + 16 * i;
        if (strcmp(str(r, u32(p)), page)) continue;
        unsigned count = u32(p + 4), off = u32(p + 8);
        for (unsigned j = 0; j < count; j++) {
            const uint8_t *pair = r->data + off + j * 8;
            if (!strcmp(str(r, u32(pair)), key)) return str(r, u32(pair + 4));
        }
        return NULL;
    }
    return NULL;
}
unsigned gt4_text_next(const char **text)
{
    const unsigned char *p = (const unsigned char *)*text;
    unsigned c = *p;
    if (!c) return 0;
    (*text)++;
    if (c < 128) return c;
    unsigned n = c >= 0xc2 && c <= 0xdf ? 2 : c >= 0xe0 && c <= 0xef ? 3 : c >= 0xf0 && c <= 0xf4 ? 4 : 0;
    if (!n) return 0xfffd;
    c &= (1u << (7 - n)) - 1;
    for (unsigned i = 1; i < n; i++) {
        if ((p[i] & 0xc0) != 0x80) return 0xfffd;
        c = (c << 6) | (p[i] & 63);
    }
    if ((n == 3 && c < 0x800) || (n == 4 && c < 0x10000) || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) return 0xfffd;
    *text += n - 1;
    return c;
}
