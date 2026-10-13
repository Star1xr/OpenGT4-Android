#include "gpb.h"

#include "inflate.h"

#include <stdlib.h>
#include <string.h>

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

int gpb_count(const uint8_t *pkg, size_t size)
{
    if (size < 0x10 || memcmp(pkg, "gpb2", 4)) return 0;
    uint32_t n = le32(pkg + 0x0c);
    if ((uint64_t)0x10 + 16ull * n > size) return 0;
    return (int)n;
}

/* Name of pair i, NUL-terminated inside the package, or NULL. */
const char *gpb_name(const uint8_t *pkg, size_t size, int i)
{
    if (i < 0 || i >= gpb_count(pkg, size)) return NULL;
    uint32_t off = le32(pkg + 0x10 + 16 * (size_t)i);
    if (off >= size || !memchr(pkg + off, 0, size - off)) return NULL;
    return (const char *)pkg + off;
}

const uint8_t *gpb_find(const uint8_t *pkg, size_t size, const char *name, size_t *out_size)
{
    int n = gpb_count(pkg, size);
    for (int i = 0; i < n; i++) {
        const char *nm = gpb_name(pkg, size, i);
        if (!nm || strcmp(nm, name)) continue;
        const uint8_t *pair = pkg + 0x10 + 16 * (size_t)i;
        uint32_t off = le32(pair + 4), len = le32(pair + 8);
        if (off > size || len > size - off) return NULL;
        if (out_size) *out_size = len;
        return pkg + off;
    }
    return NULL;
}

/* Some assets are stored compressed inside the package, in the same form as
   GT4.VOL files: magic C5 EE F7 FF, the negated size, raw deflate
   (MakerRoot.gpb's NL/ES/BE/AU flags on the US disc). */
#define GPB_PACKED 0xFFF7EEC5u
uint8_t *gpb_load(const uint8_t *pkg, size_t pkg_size, const char *name, size_t *size)
{
    size_t n = 0;
    const uint8_t *data = gpb_find(pkg, pkg_size, name, &n);
    if (!data) return NULL;
    uint8_t *out = NULL;
    if (n >= 8 && le32(data) == GPB_PACKED) {
        uint32_t full = (uint32_t)(-(int32_t)le32(data + 4));
        if (full == 0 || full > (64u << 20)) return NULL;
        out = malloc(full);
        if (!out) return NULL;
        if (core_inflate_raw(data + 8, n - 8, out, full) != full) { free(out); return NULL; }
        n = full;
    } else {
        out = malloc(n ? n : 1);
        if (!out) return NULL;
        memcpy(out, data, n);
    }
    *size = n;
    return out;
}
