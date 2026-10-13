#include "boot_notice.h"

#include "inflate.h"

#include <stdlib.h>
#include <string.h>

/* SCUS_973.28 carries notice2005.img as a gzip member (read at 0x1000940,
   whose address 0x01036D80 maps into the ELF's data segment). Found by
   its stored file name rather than by a fixed offset. */
static uint8_t *legal_image(const char *disc, size_t *size)
{
    static const char name[] = "notice2005.img";
    size_t n = 0;
    uint8_t *elf = gt4_disc_read_root(disc, "SCUS_973.28", &n), *out = NULL;
    for (size_t p = 0; elf && p + 10 < n && !out; p++) {
        if (elf[p] != 0x1f || elf[p+1] != 0x8b || elf[p+2] != 8 || !(elf[p+3] & 8)) continue;
        size_t at = p + 10;
        if (elf[p+3] & 4) {                          /* FEXTRA */
            if (at + 2 > n) continue;
            at += 2 + (elf[at] | elf[at+1] << 8);
        }
        const uint8_t *end = at < n ? memchr(elf + at, 0, n - at) : NULL;
        size_t len = end ? (size_t)(end - (elf + at)) : 0;
        if (!end || len < sizeof name - 1 || memcmp(end - (sizeof name - 1), name, sizeof name - 1)) continue;
        at += len + 1;
        if (elf[p+3] & 16) {                         /* FCOMMENT */
            end = at < n ? memchr(elf + at, 0, n - at) : NULL;
            if (!end) continue;
            at = (size_t)(end - elf) + 1;
        }
        if (elf[p+3] & 2) at += 2;                   /* FHCRC */
        if (at >= n) continue;
        size_t cap = 1u << 20, got;
        out = malloc(cap);
        if (out && (got = core_inflate_raw(elf + at, n - at, out, cap)) != (size_t)-1 && got < cap)
            *size = got;
        else { free(out); out = NULL; }
    }
    free(elf);
    return out;
}

void gt4_boot_notices_open(gt4_boot_notices *n, const char *disc, gt4_vol *vol)
{
    memset(n, 0, sizeof *n);
    size_t size = 0;
    uint8_t *data = legal_image(disc, &size);
    if (data && (n->legal_count = tex1_decode(data, size, &n->legal)) < 0) n->legal_count = 0;
    free(data);
    data = vol ? gt4_vol_read(vol, "/advertise/us/main.img", &size) : NULL;
    if (data && (n->main_count = tex1_decode(data, size, &n->main)) < 0) n->main_count = 0;
    free(data);
}

void gt4_boot_notices_close(gt4_boot_notices *n)
{
    if (n->legal_count > 0) tex1_free(n->legal, n->legal_count);
    if (n->main_count > 0) tex1_free(n->main, n->main_count);
    memset(n, 0, sizeof *n);
}

const tex1_image *gt4_boot_notice_image(const gt4_boot_notices *n, int i)
{
    /* The boot executable's list (0x0102DCA8) is {0}; CORE's (D_00618700)
       is {2}: image 2 of main.img. Its 0, 1 and 3 are unused on this disc. */
    if (i == GT4_NOTICE_LEGAL) return n->legal_count > 0 ? &n->legal[0] : NULL;
    if (i == GT4_NOTICE_PRESENTS) return n->main_count > 2 ? &n->main[2] : NULL;
    return NULL;
}

float gt4_boot_notice_level(int i, double t)
{
    /* Both loops step once per 1/30 s: the boot executable waits two vblanks
       per step (func_0100B6F8(2)); CORE counts holds in seconds * 30. */
    if (t < 0) t = 0;
    int step = (int)(t * 30);
    if (i == GT4_NOTICE_LEGAL) {
        /* 0x01000BA0: steps 0-14 fade in, 15-134 hold, 135-150 fade out.
           The last notice also holds while CORE loads; our load is not
           tied to it, so only the authored minimum is kept. */
        if (step < 15) return step / 15.0f;
        if (step < 135) return 1;
        if (step <= 150) return (150 - step) / 15.0f;
        return -1;
    }
    /* CORE 0x00109048/0x00109080: 15-step fade in, 3.0 * 30 steps held,
       15-step fade out; also held while the boot project loads. */
    if (step < 15) return (step + 1) / 15.0f;
    if (step < 105) return 1;
    if (step < 120) return (119 - step) / 15.0f;
    return -1;
}
