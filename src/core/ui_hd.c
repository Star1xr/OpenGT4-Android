#include "ui_hd.h"
#include "inflate.h"
#include "platform/platform.h"

#include <stdlib.h>
#include <string.h>

uint64_t gt4_ui_hd_hash(const uint8_t *rgba, int w, int h)
{
    uint64_t x = 1469598103934665603ull;
    const uint32_t size[2] = { (uint32_t)w, (uint32_t)h };
    const uint8_t *s = (const uint8_t *)size;
    for (int i = 0; i < 8; i++) { x ^= s[i]; x *= 1099511628211ull; }
    for (size_t i = 0; i < (size_t)w * h * 4; i++) { x ^= rgba[i]; x *= 1099511628211ull; }
    return x;
}

uint8_t *gt4_ui_hd_read(const char *path, int *w, int *h)
{
    plat_file *f = plat_file_open(path);
    if (!f) return NULL;
    uint8_t head[12];
    uint8_t *px = NULL;
    if (plat_file_read_at(f, 0, head, 12) == 12 && !memcmp(head, "HDI2", 4)) {
        uint32_t ww = head[4] | head[5] << 8 | head[6] << 16 | (uint32_t)head[7] << 24;
        uint32_t hh = head[8] | head[9] << 8 | head[10] << 16 | (uint32_t)head[11] << 24;
        size_t n = (size_t)ww * hh * 4, zn = (size_t)(plat_file_size(f) - 12);
        uint8_t *z = ww && hh && ww <= 8192 && hh <= 8192 && zn ? malloc(zn) : NULL;
        if (z && plat_file_read_at(f, 12, z, zn) == zn && (px = malloc(n)) != NULL) {
            if (core_inflate_raw(z, zn, px, n) == n) { *w = (int)ww; *h = (int)hh; }
            else { free(px); px = NULL; }
        }
        free(z);
    }
    plat_file_close(f);
    return px;
}
