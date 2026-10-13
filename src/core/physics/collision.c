/* Course collision: the vertical ray query against the course's kd tree of
   triangles (func_003E5A30 and what it calls) and the ground under each
   wheel (func_00344458). */
#include "car.h"

#include <math.h>

/* Collision mesh of a course (the object at [[car+8]+4]+0xDC). */
#define MESH_BOX     0x20   /* float[6]: min x,y,z, max x,y,z */
#define MESH_DEPTH   0x4c   /* u16: levels of the kd tree */
#define MESH_VERTS   0x74   /* 16 bytes each: x, y, z, pad; bytes +0xE, +0xF */
#define MESH_TRIS    0x78   /* 12 bytes each: u16 v[3], u8 surface, u8 kind */
#define MESH_LEAVES  0x7c   /* 16 bytes each: s16 count, +8 u16 triangle list */
#define MESH_SPLITS  0x80   /* byte per node: axis << 6 | position in 64ths */

/* PS2 min.s/max.s return one of the operands as it is. */
static float ps2_min(float a, float b) { return gt4_select(a <= b, a, b); }
static float ps2_max(float a, float b) { return gt4_select(a >= b, a, b); }

/* func_003E5F40, func_003E5FA8 */
static void cross(float o[3], const float u[3], const float v[3])
{
    o[0] = u[1] * v[2] - u[2] * v[1];
    o[1] = u[2] * v[0] - u[0] * v[2];
    o[2] = u[0] * v[1] - u[1] * v[0];
}
static float dot(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

static const float *vertex(uint32_t mesh, int i) { return gt4_ptr(GT4_U32(mesh + MESH_VERTS) + i * 16); }

/* func_003E5FD8: the nearest triangle of a leaf that the segment start->end
   crosses from above (the triangle's front side). */
static int leaf_hit(uint32_t mesh, gt4_ray_hit *out, const float s[3], const float e[3], int leaf)
{
    uint32_t lp = GT4_U32(mesh + MESH_LEAVES) + leaf * 16, tris = GT4_U32(mesh + MESH_TRIS), tri;
    float dir[3] = { e[0] - s[0], e[1] - s[1], e[2] - s[2] };
    float best_t = INFINITY, n[3] = { 0, 0, 0 }, w0 = 0, w1 = 0, w2 = 0;
    const float *v0, *v1, *v2;
    int best = -1, count = (int16_t)GT4_U16(lp);
    for (int j = 0; j < count; j++) {
        int idx = GT4_U16(GT4_U32(lp + 8) + j * 2);
        float a[3], b[3], c[3], m[3], d0, d1, d2, den, t;
        tri = tris + idx * 12;
        v0 = vertex(mesh, GT4_U16(tri)); v1 = vertex(mesh, GT4_U16(tri + 2)); v2 = vertex(mesh, GT4_U16(tri + 4));
        for (int k = 0; k < 3; k++) { a[k] = v0[k] - s[k]; b[k] = v1[k] - s[k]; }
        cross(m, dir, a);
        d0 = dot(b, m);
        if (0 < d0) continue;
        for (int k = 0; k < 3; k++) c[k] = v2[k] - s[k];
        d1 = dot(c, m);
        if (d1 < 0) continue;
        cross(m, b, c);
        d2 = dot(dir, m);
        if (0 < d2) continue;
        for (int k = 0; k < 3; k++) { b[k] = v1[k] - v0[k]; c[k] = v2[k] - v0[k]; }
        cross(m, b, c);
        den = dot(m, dir);
        if (den == 0) t = 0;
        else {
            t = dot(m, a);
            if (0 < t || t < den) continue;
            t = t / den;
        }
        if (!(t < best_t)) continue;
        w0 = -d1; w1 = d0; w2 = d2;
        n[0] = m[0]; n[1] = m[1]; n[2] = m[2];
        best = idx;
        best_t = t;
    }
    if (best < 0) return 0;

    for (int k = 0; k < 3; k++) out->pos[k] = (e[k] - s[k]) * best_t + s[k];
    out->leaf = (int16_t)leaf;
    out->tri = (int16_t)best;
    tri = tris + best * 12;
    v0 = vertex(mesh, GT4_U16(tri)); v1 = vertex(mesh, GT4_U16(tri + 2)); v2 = vertex(mesh, GT4_U16(tri + 4));
    /* Keep the point inside the triangle's box. */
    for (int k = 0; k < 3; k++) {
        float lo, hi;
        if (v0[k] < v1[k]) { lo = ps2_min(v0[k], v2[k]); hi = ps2_max(v1[k], v2[k]); }
        else               { hi = ps2_max(v0[k], v2[k]); lo = ps2_min(v1[k], v2[k]); }
        out->pos[k] = ps2_max(lo, ps2_min(hi, out->pos[k]));
    }
    out->surface = GT4_U8(tri + 6) & 0x1f;
    out->kind = GT4_U8(tri + 7);
    {
        /* Per-vertex bytes interpolated with the barycentric weights. */
        const uint8_t *b0 = (const uint8_t *)v0, *b1 = (const uint8_t *)v1, *b2 = (const uint8_t *)v2;
        float sum = (w1 + w2) + w0;
        float r = 1.0f / sqrtf(fabsf((n[0] * n[0] + n[1] * n[1]) + n[2] * n[2]));
        float f0 = (float)b0[0xe] / 255.0f, f1 = (float)b1[0xe] / 255.0f, f2 = (float)b2[0xe] / 255.0f, g;
        g = f0 + ((f1 - f0) * w0 + (f2 - f0) * w1) / sum;
        out->u = ps2_min(ps2_max(g, 0.0f), 1.0f);
        out->normal[0] = n[0] * r;
        out->normal[1] = n[1] * r;
        out->normal[2] = n[2] * r;
        f0 = (float)(b0[0xf] & 0x7f) / 127.0f; f1 = (float)(b1[0xf] & 0x7f) / 127.0f; f2 = (float)(b2[0xf] & 0x7f) / 127.0f;
        g = f0 + ((f1 - f0) * w0 + (f2 - f0) * w1) / sum;
        out->v = ps2_min(ps2_max(g, 0.0f), 1.0f);
        out->plane = -dot(out->normal, v0);
    }
    return 1;
}

/* func_003E5BC8: walks the kd tree along the segment, near side first.
   node indexes the split bytes (children at node+1 and node+(1<<depth));
   leaf collects the path bits. */
/* A split byte of 0xFF (axis 3: a node the tree leaves empty, as on
   Suzuka) makes the original read one float past the box and the points
   (the next words of its stack frame) and write one past its own box and
   mid-point: stack slots of no use. Here those are padding, zero, so the
   host's stack stays whole; where the original then goes depends on what
   its stack held. */
static int walk(uint32_t mesh, gt4_ray_hit *out, const float *box, const float *s, const float *e,
                int node, int depth, int leaf)
{
    uint8_t split;
    int axis;
    float frac, at, ds, de, sub[8] = { 0 }, mid[4] = { 0 };
    if (depth < 0) return leaf_hit(mesh, out, s, e, leaf);
    split = GT4_U8(GT4_U32(mesh + MESH_SPLITS) + node);
    axis = split >> 6;
    frac = ((float)(split & 0x3f) + 0.5f) * 0.015625f;
    at = box[axis] * (1.0f - frac) + box[3 + axis] * frac;
    ds = s[axis] - at;
    de = e[axis] - at;
    leaf <<= 1;
    memcpy(sub, box, 6 * sizeof *sub);
    if (ds * de < 0) {
        float inv = 1.0f / (e[axis] - s[axis]), ts = (at - s[axis]) * inv, te = de * inv;
        mid[0] = te * s[0] + ts * e[0];
        mid[1] = te * s[1] + ts * e[1];
        mid[2] = te * s[2] + ts * e[2];
        mid[axis] = at;
        if (ds < 0) {
            sub[3 + axis] = at;
            if (walk(mesh, out, sub, s, mid, node + 1, depth - 1, leaf)) return 1;
            sub[axis] = at;
            sub[3 + axis] = box[3 + axis];
            return walk(mesh, out, sub, mid, e, node + (1 << depth), depth - 1, leaf + 1);
        }
        sub[axis] = at;
        if (walk(mesh, out, sub, s, mid, node + (1 << depth), depth - 1, leaf + 1)) return 1;
        sub[axis] = box[axis];
        sub[3 + axis] = at;
        return walk(mesh, out, sub, mid, e, node + 1, depth - 1, leaf);
    }
    if (ds < 0) {
        sub[3 + axis] = at;
        return walk(mesh, out, sub, s, e, node + 1, depth - 1, leaf);
    }
    sub[axis] = at;
    return walk(mesh, out, sub, s, e, node + (1 << depth), depth - 1, leaf + 1);
}

/* func_003E5A30: the first triangle straight below p (y up), down to the
   bottom of the mesh. 1 on a hit, with out filled. */
int gt4_course_ray_down(uint32_t mesh, gt4_ray_hit *out, const float p[3])
{
    const float *box = gt4_ptr(mesh + MESH_BOX);
    float s[4] = { 0 }, e[4] = { 0 };
    if (p[1] < box[1]) return 0;
    s[0] = p[0]; s[1] = box[4] < p[1] ? box[4] : p[1]; s[2] = p[2];
    e[0] = p[0]; e[1] = box[1]; e[2] = p[2];
    return walk(mesh, out, box, s, e, 0, GT4_U16(mesh + MESH_DEPTH) - 1, 0);
}

/* func_00395760 (func_003957F0 is the same for four blocks in a row):
   the ground under one wheel, from 5 m above its ground point. */
static void ground_query(uint32_t course, uint32_t g)
{
    gt4_ray_hit hit;
    float p[3] = { GT4_F32(g + GROUND_X), GT4_F32(g + GROUND_Y) + 5.0f, GT4_F32(g + GROUND_Z) };
    uint32_t mesh = GT4_U32(GT4_U32(course + 4) + 0xdc);   /* func_00396800 */
    if (!gt4_course_ray_down(mesh, &hit, p)) {
        GT4_U8(g + GROUND_MISS) = 1;
        return;
    }
    /* func_003956F8 */
    GT4_U8(g + GROUND_MISS) = 0;
    GT4_F32(g + GROUND_HIT_Y) = hit.pos[1];
    GT4_F32(g + GROUND_NORMAL + 0) = hit.normal[0];
    GT4_F32(g + GROUND_NORMAL + 4) = hit.normal[1];
    GT4_F32(g + GROUND_NORMAL + 8) = hit.normal[2];
    GT4_F32(g + GROUND_PLANE) = hit.plane;
    GT4_U32(g + GROUND_ID) = (uint32_t)(uint16_t)hit.tri | (uint32_t)(int32_t)hit.leaf << 16;
    GT4_U8(g + GROUND_SURFACE) = (uint8_t)hit.surface;
    GT4_U8(g + 0x32) = 0;
    GT4_U8(g + GROUND_KIND) = (uint8_t)hit.kind;
    GT4_F32(g + GROUND_U) = hit.u;
    GT4_F32(g + GROUND_V) = hit.v;
}

/* func_00344458: ground under every wheel; a face steeper than 60 degrees
   counts as flat ground. */
void gt4_car_ground_query(uint32_t car)
{
    int n = GT4_U8(gt4_car_layout(car) + LAYOUT_WHEELS);
    for (int i = 0; i < n; i++) ground_query(GT4_U32(car + CAR_COURSE), car + CAR_GROUND + i * GROUND_SIZE);
    for (int i = 0; i < n; i++) {
        uint32_t g = car + CAR_GROUND + i * GROUND_SIZE;
        if (GT4_F32(g + GROUND_NORMAL + 4) < 0.5f) {
            GT4_U32(g + GROUND_NORMAL + 0) = 0;
            GT4_F32(g + GROUND_NORMAL + 4) = 1.0f;
            GT4_U32(g + GROUND_NORMAL + 8) = 0;
        }
    }
}

void gt4_ground_query(uint32_t course, uint32_t g) { ground_query(course, g); }
