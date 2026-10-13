/* The course's driving lines at the race start: the ADLN block of the
   course's .ad file (at course+0x9C) made ready for the driver model
   (func_0046B0A8, func_0046B9F0): offsets to addresses, coordinates to the
   game's axes, each point's ground height, slope, surface and distance
   along the course (func_003955E8 with the course's segments). */
#include "car.h"

#include <math.h>
#include <string.h>

#define LINE_MAGIC  0x4e4c4441u   /* "ADLN" */
#define LINE_ORDER  0x00624018u   /* D_00624018: the order the lines are built in */
#define PI          K(0x40490fdau)
#define TWO_PI      K(0x40c90fdau)
#define K(bits) gt4_f32(bits)

static float ps2_sqrt(float x) { return x == 0 ? x : sqrtf(fabsf(x)); }
/* The PS2 FPU reads an Inf or NaN pattern as +/-FLT_MAX (some line files
   hold one, e.g. a heading in Sarthe's). */
static float ps2_in(float x)
{
    uint32_t v;
    memcpy(&v, &x, 4);
    if ((v & 0x7f800000u) == 0x7f800000u) v = (v & 0x80000000u) | 0x7f7fffffu;
    memcpy(&x, &v, 4);
    return x;
}
static uint32_t runway_of(uint32_t course) { return GT4_U32(GT4_U32(course + 4) + 0xdc); }   /* func_00396800 */

/* func_003E6610: where p falls in the quad a, b (one edge) and c, d (the
   next), in x and z: out[0], out[1] the quad's coordinates, out[2] the
   height of p above the quad. 0 when p is outside. */
static int quad_coords(float out[3], const float *p, const float *a, const float *b, const float *c, const float *d)
{
    float acx = c[0] - a[0], acz = c[2] - a[2], dbx = b[0] - d[0], dbz = b[2] - d[2];
    float px = p[0] - a[0], pz = p[2] - a[2], e, f, abx, abz, dpx, dpz, dcx, dcz, g, u, v;
    if (acx == 0 && acz == 0 && dbx == 0 && dbz == 0) return 0;
    e = pz * acx - px * acz;
    if (0 < e) return 0;
    abx = b[0] - a[0];
    abz = b[2] - a[2];
    f = pz * abx - px * abz;
    if (f < 0) return 0;
    dpx = p[0] - d[0];
    dpz = p[2] - d[2];
    if (0 < dpz * dbx - dpx * dbz) return 0;
    dcx = c[0] - d[0];
    dcz = c[2] - d[2];
    if (dpz * dcx - dpx * dcz < 0) return 0;
    {
        float sx = -dcx - abx, sz = -dcz - abz;
        g = pz * sx - px * sz;
        if (acx == 0 && acz == 0) {
            u = -f / g;
            v = g / (abz * sx - abx * sz);
        } else {
            float h = abz * acx - abx * acz, k = acz * sx - acx * sz;
            float s = g + h, t = h - g, r = ps2_sqrt(s * s + f * 4.0f * k);
            v = f * -2.0f / (s - r);
            u = (e + e) / (t - r);
        }
    }
    out[0] = u;
    out[1] = v;
    out[2] = p[1] - ((1.0f - u) * (1.0f - v) * a[1] + u * (1.0f - v) * b[1] + (1.0f - u) * v * c[1] + u * v * d[1]);
    return 1;
}

/* func_003E6858: distance along the course of p, from the segments of the
   kd tree leaf p is over (*id: triangle, leaf; found by a ray when unset). */
static float course_distance_at(uint32_t rw, const float *p, uint32_t *id)
{
    /* D_006A39F8 = 0x7FFFFFFF: on the PS2 (no NaN) the largest float */
    float best = K(0x7f7fffffu), dist = 0, q[3];
    int32_t leaf = (int16_t)(*id >> 16);
    uint32_t l;
    int start, end;
    if (leaf <= 0) {
        gt4_ray_hit hit;
        if (!gt4_course_ray_down(rw, &hit, p)) return 0;
        *id = (uint32_t)(uint16_t)hit.tri | (uint32_t)(uint16_t)hit.leaf << 16;
        leaf = hit.leaf;
    }
    l = GT4_U32(rw + 0x7c) + (uint32_t)(leaf * 16);
    start = (int16_t)GT4_U16(l + 4);
    end = start + (int16_t)GT4_U16(l + 2);
    for (int i = start; i < end; i++) {
        int k = (int16_t)GT4_U16(GT4_U32(rw + 0x64) + (uint32_t)i * 2), next = k + 1;
        uint32_t base = GT4_U32(rw + 0x60), a = base + (uint32_t)k * 40, b = next == GT4_U16(rw + 0x3c) ? base : base + (uint32_t)next * 40;
        float *fa = gt4_ptr(a), *fb = gt4_ptr(b);
        if (!quad_coords(q, p, fa, fb, fa + 3, fb + 3) && !quad_coords(q, p, fa + 3, fb + 3, fa + 6, fb + 6)) continue;
        q[2] = fabsf(q[2]);
        if (!(q[2] < best)) continue;
        best = q[2];
        {
            float d0 = GT4_F32(a + 0x24), d1 = GT4_F32(b + 0x24), len = GT4_F32(rw + 0x14);
            if (d1 < d0) d1 = d1 + len;
            dist = d0 + (d1 - d0) * q[0];
            if (len < dist) dist = dist - len;
        }
    }
    return dist;
}

/* func_003955E8: distance along the course of p; *id its ground id,
   found (and kept) from 1000 m above when unset. */
static float course_distance(uint32_t course, const float *p, uint32_t *id)
{
    uint32_t rw = runway_of(course), leaf_id;
    if ((int16_t)(*id >> 16) <= 0) {
        gt4_ray_hit hit;
        float from[3] = { p[0], p[1] + 1000.0f, p[2] };
        if (!gt4_course_ray_down(rw, &hit, from)) {
            *id = 0xffffffffu;
            return 0;
        }
        *id = (uint32_t)(uint16_t)hit.tri | (uint32_t)(uint16_t)hit.leaf << 16;
    }
    leaf_id = *id;
    return course_distance_at(rw, p, &leaf_id);
}

/* func_003955E8 on a guest record: x, y, z, then the ground id at +0xC */
float gt4_course_distance(uint32_t course, uint32_t q)
{
    uint32_t id = GT4_U32(q + 0xc);
    float d = course_distance(course, gt4_ptr(q), &id);
    GT4_U32(q + 0xc) = id;
    return d;
}

/* func_0046A0C8 (func_00395760): ground height under (x, -z) from h + 5 m;
   *miss says whether there is none. */
static float ground_y(uint32_t course, float x, float z, float h, uint32_t *miss, gt4_ray_hit *hit)
{
    gt4_ray_hit tmp;
    float p[3] = { x, h + 5.0f, -z };
    if (!hit) hit = &tmp;
    if (!gt4_course_ray_down(runway_of(course), hit, p)) {
        *miss = 1;
        return 0;
    }
    *miss = 0;
    return hit->pos[1];
}

/* func_0046A0C8: ground_y with the ground id: *id takes the hit's
   (func_003956F8) and is kept on a miss. */
float gt4_line_ground(uint32_t course, float x, float z, float h, uint32_t *id, int *miss)
{
    gt4_ray_hit hit;
    uint32_t m;
    float y = ground_y(course, x, z, h, &m, &hit);
    *miss = (int)m;
    if (!m) *id = (uint32_t)(uint16_t)hit.tri | (uint32_t)(int32_t)hit.leaf << 16;
    return y;
}

/* func_003F4E40: the surface, mapped for some kinds of faces */
static uint32_t surface_of(const gt4_ray_hit *h)
{
    static const uint8_t map[13] = { 0x14, 0x15, 2, 3, 4, 5, 6, 0x14, 8, 0x16, 0x15, 0x15, 0x15 };
    uint32_t s = h->surface & 0xff;
    if (!(h->kind & 2)) return s;
    return s < 13 ? map[s] : s;
}

/* func_0046A140: the ground at a line point: height, the slope along and
   across the heading (from four points half a metre away), surface. */
static int ground_at(uint32_t course, float x, float z, float h, float heading,
                     float *y, float *pitch, float *roll, uint32_t *surface, uint32_t *flag)
{
    gt4_ray_hit hit;
    uint32_t miss, m[4];
    float c, s, y0, ya, yb, yc, yd;
    *roll = 0;
    *pitch = 0;
    y0 = ground_y(course, x, z, h, &miss, &hit);
    if (miss) {
        *y = 0;
        *flag = 0;
        *surface = 0;
        return 0;
    }
    *y = y0;
    *flag = hit.kind & 1;
    *surface = surface_of(&hit);
    c = gt4_cosf(heading) * 0.5f;
    s = gt4_sinf(heading) * 0.5f;
    ya = ground_y(course, x - c, z + s, h, &m[0], NULL);
    yb = ground_y(course, x + c, z - s, h, &m[1], NULL);
    yc = ground_y(course, x - s, z - c, h, &m[2], NULL);
    yd = ground_y(course, x + s, z + c, h, &m[3], NULL);
    if (m[0] && m[1]) return 0;
    if (m[2] && m[3]) return 0;
    if (m[0]) *pitch = gt4_atan2f(y0 - yb, 0.5f);
    else if (m[1]) *pitch = gt4_atan2f(ya - y0, 0.5f);
    else *pitch = gt4_atan2f(ya - yb, 1.0f);
    if (m[2]) *roll = gt4_atan2f(y0 - yd, 0.5f);
    else if (m[3]) *roll = gt4_atan2f(yc - y0, 0.5f);
    else *roll = gt4_atan2f(yc - yd, 1.0f);
    return 1;
}

/* func_0046A050 */
static uint32_t line_of(uint32_t lines, uint32_t i)
{
    uint32_t t = GT4_U32(lines + 8);
    return t ? GT4_U32(t + i * 4) : 0;
}

/* func_0046B248: offsets to addresses, lines without points dropped, the
   points' x and z negated and their heading turned half round */
static void adln_relocate(uint32_t adln)
{
    uint32_t base = adln;
    if (GT4_U32(adln + 4)) return;
    GT4_U32(adln + 4) = base;
    GT4_U32(adln + 0x18) = GT4_U32(adln + 0x18) + base;
    for (uint32_t k = 0; k < GT4_U32(adln + 0x14); k++) {
        uint32_t e = GT4_U32(adln + 0x18) + k * 4, line;
        int32_t n;
        if (!GT4_U32(e)) continue;
        line = GT4_U32(e) = GT4_U32(adln + 4) + GT4_U32(e);
        n = (int32_t)GT4_U32(line);
        if (!n) { GT4_U32(e) = 0; continue; }
        for (uint32_t p = line + 0x40; n > 0; n--, p += 0x40) {
            GT4_F32(p + 4) = -GT4_F32(p + 4);
            GT4_F32(p + 8) = -GT4_F32(p + 8);
            GT4_F32(p + 0x24) = ps2_in(GT4_F32(p + 0x24)) + PI;
        }
    }
}

/* 0046BCB0: the older fixed-point format used by the licence courses.
   Its points start at +4 and have stride 0x28 (00469F20/00469FC8). */
static void legacy_relocate(uint32_t adln)
{
    if ((int32_t)GT4_U32(adln) < 0) return;
    GT4_U32(adln) = 0u - GT4_U32(adln);
    for (unsigned k=0;k<8;k++) {
        uint32_t e=adln+8+k*4,line;
        if (!GT4_U32(e)) continue;
        line=GT4_U32(e)=adln+GT4_U32(e);
        int32_t n=(int32_t)GT4_U32(line);
        if (!n) {GT4_U32(e)=0;continue;}
        for (uint32_t p=line+4;n>0;n--,p+=0x28) {
            GT4_F32(p+4)=-(float)(int32_t)GT4_U32(p+4)*K(0x39800000u);
            GT4_F32(p+8)=-(float)(int32_t)GT4_U32(p+8)*K(0x39800000u);
            GT4_F32(p+0x18)=(float)(int32_t)GT4_U32(p+0x18)*K(0x39800000u);
            GT4_F32(p+0x24)=((float)(int32_t)GT4_U32(p+0x24)*TWO_PI)*K(0x39800000u)+PI;
        }
    }
}
/* func_0046A510: both authored line formats. */
static void lines_build(uint32_t lines, uint32_t adln)
{
    uint32_t course = GT4_U32(lines), rw = runway_of(course);
    float len = GT4_F32(rw + 0x14);                                       /* func_0046A0B0 */
    int wide=GT4_U32(adln)==LINE_MAGIC;
    if (wide) {
        adln_relocate(adln);
        GT4_U32(lines + 4) = GT4_U32(adln + 0x14);
        GT4_U32(lines + 8) = GT4_U32(adln + 0x18);
    } else {
        legacy_relocate(adln);
        GT4_U32(lines+4)=GT4_U32(adln+4);
        GT4_U32(lines+8)=adln+8;
    }
    for (uint32_t k = 0; k < 8; k++) {
        uint32_t idx = GT4_U8(LINE_ORDER + k), line = line_of(lines, idx), wrapped = 0, past = 0;
        float last = 0, px = 0, pz = 0, h = 1000.0f;
        int n;
        if (!line) continue;
        n = (int32_t)GT4_U32(line);
#define PT(i) (line + (wide?0x40u:4u) + (uint32_t)(i) * (wide?0x40u:0x28u))
        for (int i = 0; i < n; i++) {
            uint32_t pt = PT(i), miss, hint;
            float x = GT4_F32(pt + 4), z = GT4_F32(pt + 8), dist;
            if (i > 0) {
                float dx = x - px, dz = z - pz, from, y;
                from = h + (ps2_sqrt(dx * dx + dz * dz) * K(0x3dccccccu) + 1.0f);
                y = ground_y(course, x, z, from, &miss, NULL);
                if (miss) {
                    y = ground_y(course, x, z, from + 100.0f, &miss, NULL);
                    if (miss) y = h + 3.0f;
                }
                h = (float)((int32_t)y + 2);
            }
            px = x;
            pz = z;
            hint = (uint32_t)(int32_t)(int16_t)GT4_U16(pt + 0x10);
            if (GT4_U8(pt)) GT4_U8(pt) = (uint8_t)(idx == 5 ? 1 : 0 < GT4_F32(pt + 0x18) ? 1 : 2);
            if (idx == 4 && !(i < 3) && !(n - 4 < i)) {
                /* near the start: onto the grid slot within 5 m */
                uint32_t cnt = GT4_U16(rw + 0x52);
                float best = K(0x47c35000u);
                int bi = -1;
                for (uint32_t j = 0; j < cnt; j++) {
                    uint32_t e = GT4_U32(rw + 0x90) + j * 16;
                    float ex = GT4_F32(e) - x, ez = -GT4_F32(e + 8) - z, d = ps2_sqrt(ex * ex + ez * ez);
                    if (5.0f < d) continue;
                    if (d < best || bi == -1) { best = d; bi = (int)j; }
                }
                if (bi != -1) {
                    uint32_t e = GT4_U32(rw + 0x90) + (uint32_t)bi * 16;
                    GT4_F32(pt + 4) = x = GT4_F32(e);
                    GT4_F32(pt + 8) = z = -GT4_F32(e + 8);
                    GT4_F32(pt + 0x24) = gt4_angle_wrap(GT4_F32(e + 0xc) + PI);
                }
            }
            {
                float q[3] = { x, h, -z };
                dist = course_distance(course, q, &hint);
            }
            GT4_F32(pt + 0x14) = dist;
            GT4_U32(pt + 0xc) = 0;
            GT4_U32(pt + 0x1c) = 0;
            GT4_U32(pt + 0x20) = 0;
            GT4_U8(pt + 0x12) = 0;
            GT4_U8(pt + 0x13) = 0;
            if (idx != 5) {
                float y, pitch, roll;
                uint32_t surface, flag;
                if (ground_at(course, GT4_F32(pt + 4), GT4_F32(pt + 8), h, GT4_F32(pt + 0x24), &y, &pitch, &roll, &surface, &flag)) {
                    h = y;
                    GT4_F32(pt + 0xc) = y;
                    GT4_F32(pt + 0x1c) = pitch;
                    GT4_F32(pt + 0x20) = roll;
                    GT4_U8(pt + 0x12) = (uint8_t)surface;
                    GT4_U8(pt + 0x13) = 0;   /* func_003956F8 leaves +0x32 at 0 */
                    (void)flag;
                }
            }
            dist = GT4_F32(pt + 0x14);
            if (idx == 4) {
                if (dist < len * 0.5f) past = 1;
                if (past == 1) GT4_F32(pt + 0x14) = dist = dist + len;
            }
            if (dist <= last) wrapped = 1;
            last = dist;
        }
        for (int i = 0; i < n; i++) {
            uint32_t a = PT(i), b = PT(i < n - 1 ? i + 1 : 0);
            float dx = GT4_F32(b + 4) - GT4_F32(a + 4), dz = GT4_F32(b + 8) - GT4_F32(a + 8);
            uint8_t type = GT4_U8(a);
            GT4_U32(a + 0x28) = 0;
            GT4_F32(a + 0x2c) = ps2_sqrt(dx * dx + dz * dz);
            if ((uint8_t)(type - 1) < 2) {
                float r = GT4_F32(a + 0x18), ha = GT4_F32(a + 0x24), hb = GT4_F32(b + 0x24), ang;
                if (r < 0) r = -r;
                if (type == 1) { if (hb < ha) ha = ha - TWO_PI; }
                else if (ha < hb) hb = hb - TWO_PI;
                ang = hb - ha;
                if (ang < 0) ang = -ang;
                GT4_F32(a + 0x2c) = r * ang;
            }
            GT4_F32(a + 0x30) = GT4_F32(b + 0xc) - GT4_F32(a + 0xc);
            if (0 < GT4_F32(a + 0x2c)) GT4_F32(a + 0x34) = gt4_atan2f(GT4_F32(a + 0x30), GT4_F32(a + 0x2c));
        }
        for (int i = 0; i < n; i++) {
            uint32_t a = PT(i), p = PT(i > 0 ? i - 1 : n - 1);
            if (GT4_F32(a + 0x34) - GT4_F32(p + 0x34) < K(0xbc8efa34u)) GT4_U8(a + 0x28) = 1;
        }
#undef PT
        if (wrapped && ((uint32_t)(idx - 1) < 2 || idx == 7)) GT4_U32(GT4_U32(lines + 8) + idx * 4) = 0;
    }
}

/* func_0046B0A8: the lines object (lines: course, count, table, and +0xC
   whether lines 3 and 4 exist) of a course. */
void gt4_lines_setup(uint32_t lines, uint32_t course)
{
    GT4_U32(lines) = course;
    lines_build(lines, GT4_U32(course + 0x9c));
    GT4_U16(lines + 0xc) = 1;
    if (!line_of(lines, 3) || !line_of(lines, 4)) GT4_U16(lines + 0xc) = 0;
}

/* func_0046B9F0: line 5's per-point values at +0x38 cleared. */
int gt4_lines_clear(uint32_t lines)
{
    {
        uint32_t line = line_of(lines, 5);
        if (line) {
            int32_t n = (int32_t)GT4_U32(line);
            int wide=GT4_U32(GT4_U32(GT4_U32(lines)+0x9c))==LINE_MAGIC;
            for (int32_t i = 0; i < n; i++) {
                uint32_t p=line+(wide?0x40u:4u)+(uint32_t)i*(wide?0x40u:0x28u);
                GT4_U32(p + 0x38) = 0;
                GT4_U32(p + 0x3c) = 0;
            }
            return 1;
        }
    }
    return 0;
}

/* func_003955E8 on a point of the host: x, y, z; *id its ground id */
float gt4_course_distance_p(uint32_t course, const float p[3], uint32_t *id)
{
    return course_distance(course, p, id);
}
