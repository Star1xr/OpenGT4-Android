/*
 * A course's camera motions (course_motion.h). Mot1 bank layout (the
 * loader 0x00427698, the player func_0042B440):
 *   +0x10 table A (16 bytes: +0 the packed motions' count in its low half,
 *   +4 the packed motions, 32 bytes each, +8 the keyframe sequences, +0xC
 *   the parameter list), +0x14 u16 A count, +0x16 u16 B count, +0x1C
 *   table B (the named channels).
 *   A packed motion: +0 its end, +4 where it loops back to, +0x10 its
 *   name, +0x14 its curves' count, +0x18 the curves' headers (16 bytes:
 *   +0xC the data: u32 n, a leading word, then keys of time, value and two
 *   control values, the last key ending after its value).
 *   The parameter list: 0x0B the look-at (u16 mask, nine components), 0x0A
 *   the field of view (u8 mask, one), 0x01 the end; a set mask bit is a
 *   u16 curve of the motion, a clear one a float constant.
 */
#include "course_motion.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct { float t, v, c1, c2; } key;
typedef struct { key *keys; int count; } curve;
typedef struct { char name[32]; float end, loop; curve *curves; int ncurves; } motion;

struct gt4_motion_bank {
    motion *motions;
    int nmotions;
    int look_curve[9], fov_curve;   /* -1: the constant */
    float look_value[9], fov_value;
};

static uint32_t u32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static float f32(const uint8_t *p) { float f; uint32_t v = u32(p); memcpy(&f, &v, 4); return f; }
static int span(size_t size, size_t at, size_t len) { return at <= size && len <= size - at; }

void gt4_motion_bank_close(gt4_motion_bank *b)
{
    if (!b) return;
    for (int i = 0; i < b->nmotions; i++) {
        for (int c = 0; c < b->motions[i].ncurves; c++) free(b->motions[i].curves[c].keys);
        free(b->motions[i].curves);
    }
    free(b->motions);
    free(b);
}

static int read_curve(const uint8_t *d, size_t size, size_t at, curve *c)
{
    uint32_t n;
    const uint8_t *f;
    if (!span(size, at, 4)) return 0;
    n = u32(d + at);
    if (n < 2 || n > 4096 || !span(size, at + 4, (size_t)(n + 1) * 4)) return 0;
    f = d + at + 4;
    c->count = (int)((n - 2) / 4 + 1);
    c->keys = calloc((size_t)c->count, sizeof *c->keys);
    if (!c->keys) return 0;
    for (int k = 0; k < c->count; k++) {
        size_t j = 1 + 4 * (size_t)k;
        c->keys[k].t = f32(f + 4 * j);
        c->keys[k].v = f32(f + 4 * (j + 1));
        c->keys[k].c1 = j + 2 <= n ? f32(f + 4 * (j + 2)) : c->keys[k].v;
        c->keys[k].c2 = j + 3 <= n ? f32(f + 4 * (j + 3)) : c->keys[k].v;
    }
    return 1;
}

gt4_motion_bank *gt4_motion_bank_open(const uint8_t *d, size_t size)
{
    gt4_motion_bank *b;
    size_t a, packed, params;
    unsigned n;
    if (!d || size < 0x20 || memcmp(d, "Mot1", 4) || !u16(d + 0x14)) return NULL;
    a = u32(d + 0x10);
    if (!span(size, a, 16)) return NULL;
    n = u16(d + a);
    packed = u32(d + a + 4);
    params = u32(d + a + 12);
    if (!n || n > 64 || !span(size, packed, (size_t)n * 32) || !span(size, params, 1)) return NULL;
    b = calloc(1, sizeof *b);
    if (!b) return NULL;
    b->motions = calloc(n, sizeof *b->motions);
    if (!b->motions) goto fail;
    b->nmotions = (int)n;
    for (unsigned i = 0; i < n; i++) {
        const uint8_t *r = d + packed + i * 32;
        motion *m = &b->motions[i];
        size_t name = u32(r + 0x10), list = u32(r + 0x18);
        uint32_t nc = u32(r + 0x14);
        m->end = f32(r);
        m->loop = f32(r + 4);
        if (name < size) {
            const uint8_t *z = memchr(d + name, 0, size - name);
            if (z && (size_t)(z - (d + name)) < sizeof m->name) memcpy(m->name, d + name, (size_t)(z - (d + name)));
        }
        if (nc > 64 || !span(size, list, (size_t)nc * 4)) goto fail;
        m->curves = calloc(nc ? nc : 1, sizeof *m->curves);
        if (!m->curves) goto fail;
        m->ncurves = (int)nc;
        for (uint32_t c = 0; c < nc; c++) {
            size_t h = u32(d + list + 4 * c);
            if (!span(size, h, 16) || !read_curve(d, size, u32(d + h + 12), &m->curves[c])) goto fail;
        }
    }
    for (int c = 0; c < 9; c++) b->look_curve[c] = -1;
    b->fov_curve = -1;
    b->fov_value = 1;
    for (size_t p = params; p < size;) {
        unsigned type = d[p], mask, count;
        size_t at;
        int *cv;
        float *val;
        if (type == 0x0B && span(size, p, 3)) { mask = u16(d + p + 1); count = 9; at = p + 3; cv = b->look_curve; val = b->look_value; }
        else if (type == 0x0A && span(size, p, 2)) { mask = d[p + 1]; count = 1; at = p + 2; cv = &b->fov_curve; val = &b->fov_value; }
        else break;
        for (unsigned c = 0; c < count; c++) {
            if (mask & (1u << c)) {
                if (!span(size, at, 2)) goto fail;
                cv[c] = u16(d + at);
                at += 2;
            } else {
                if (!span(size, at, 4)) goto fail;
                cv[c] = -1;
                val[c] = f32(d + at);
                at += 4;
            }
        }
        p = at;
    }
    return b;
fail:
    gt4_motion_bank_close(b);
    return NULL;
}

int gt4_motion_bank_count(const gt4_motion_bank *b) { return b ? b->nmotions : 0; }
const char *gt4_motion_bank_name(const gt4_motion_bank *b, int i) { return b && i >= 0 && i < b->nmotions ? b->motions[i].name : ""; }

static float evaluate(const curve *c, float t)
{
    if (t <= c->keys[0].t) return c->keys[0].v;
    for (int k = 0; k + 1 < c->count; k++) {
        const key *a = &c->keys[k], *b = &c->keys[k + 1];
        if (t <= b->t) {
            float s = b->t > a->t ? (t - a->t) / (b->t - a->t) : 0, u = 1 - s;
            return u * u * u * a->v + 3 * u * u * s * a->c1 + 3 * u * s * s * a->c2 + s * s * s * b->v;
        }
    }
    return c->keys[c->count - 1].v;
}

static float component(const motion *m, int cv, float value, float t)
{
    return cv >= 0 && cv < m->ncurves ? evaluate(&m->curves[cv], t) : value;
}

int gt4_motion_bank_camera(const gt4_motion_bank *b, int i, float t, float look[9], float *fov)
{
    const motion *m;
    if (!b || i < 0 || i >= b->nmotions) return 0;
    m = &b->motions[i];
    /* func_0042D430: before 0 the start, past the end looped back */
    if (t < 0) t = 0;
    else if (t >= m->end) {
        float len = m->end - m->loop;
        t = len > 0 ? m->loop + fmodf(t - m->loop, len) : m->end;
    }
    for (int c = 0; c < 9; c++) look[c] = component(m, b->look_curve[c], b->look_value[c], t);
    *fov = component(m, b->fov_curve, b->fov_value, t);
    return 1;
}
