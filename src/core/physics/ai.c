/* The driver model's course line (the AI's racing line, also run for the
   player): where the car is along it, the speed it would take through the
   next section, and the point ahead the model aims at (func_00465078 up to
   where the player's case returns, func_00463EA8, func_00464868 and the
   line lookups under them).

   The model's block sits in the car at +0x14 ("obj" here): +0x1a..0x1f
   state bytes (0x1c line index, 0x1d section, 0x1e state), +0x28 target
   speed, +0x2c blend toward the second line (0x21), +0x30 steering, +0x54
   the aim's lateral offset, +0xe4 session, +0xe8 car, +0xec the lines. */
#include "car.h"

#include <string.h>

#include <math.h>

#define K(bits) gt4_f32(bits)
#define OF(o) GT4_F32(obj + (o))
#define OU8(o) GT4_U8(obj + (o))

#define LINE_MAGIC   0x4e4c4441u   /* "ADLN" */
#define AI_SETTINGS  0x00623e50u   /* +4 blend mode, +5 aim offsets, +8 grip limit, +0xb, +0x74.. */

static float ps2_sqrt(float x) { return x == 0 ? x : sqrtf(fabsf(x)); }

/* A line: count points of `stride` bytes (func_00469F20). Point: +0 kind
   (0 straight, else arc), +1 s8 speed trim %, +2 s8 (-128 skip), +4 x,
   +8 y, +0xc z, +0x10 s16, +0x12 s8 surface, +0x14 distance along the
   course, +0x18 arc radius, +0x1c turn angle, +0x24 arc start angle, +0x28
   byte. */
typedef struct { int wide; uint32_t count, data; } line_t;

static line_t line_get(uint32_t lines, int index)
{
    line_t l;
    uint32_t table = GT4_U32(lines + 8), entry = table ? GT4_U32(table + (uint32_t)index * 4) : 0;
    if (GT4_U32(GT4_U32(GT4_U32(lines) + 0x9c)) == LINE_MAGIC) {
        l.wide = 1;
        l.count = GT4_U32(entry);
        l.data = entry + 0x40;
    } else {
        l.wide = 0;
        l.count = GT4_U32(entry);
        l.data = entry + 4;
    }
    return l;
}

/* func_00469FC8 */
static uint32_t line_point(const line_t *l, int i) { return l->data + (uint32_t)i * (l->wide ? 0x40u : 0x28u); }

/* func_0046A0B0: the course's length. */
static float course_length(uint32_t lines)
{
    return GT4_F32(GT4_U32(GT4_U32(GT4_U32(lines) + 4) + 0xdc) + 0x14);
}

/* func_0046B310: the segment holding distance d, searched outward from
   section `from`. 1 before the first point, 2 past the last, 3 found, 0
   not found; *a and *b are the segment's end and start. */
static int line_find(const line_t *l, int from, int *a, int *b, float d)
{
    int n = (int)l->count, s0, s1, s4 = from;
    if (d < GT4_F32(line_point(l, 0) + 0x14)) { *b = n - 1; *a = 0; return 1; }
    s0 = n - 1;
    if (GT4_F32(line_point(l, s0) + 0x14) <= d) { *b = s0; *a = 0; return 2; }
    for (int k = 0; k <= n; k++) {
        s0 = (k & 1) ? s4 + k : s4 - k;
        s4 = s0;
        if (s0 < 0) s0 += n;
        else if (!(s0 < n)) s0 -= n;
        if (!s0) continue;
        s1 = s0 - 1;
        if (s1 < 0) s1 += n;
        if (GT4_F32(line_point(l, s1) + 0x14) <= d && d < GT4_F32(line_point(l, s0) + 0x14)) {
            *b = s1;
            *a = s0;
            return 3;
        }
    }
    return 0;
}

/* func_0046B7B8: the point of the pit line (4) at distance d: the line
   does not wrap (distances in the first half of the lap count a lap on),
   straight between its points; -1 (nothing written) off either end, else
   the section. */
static int pit_line_at(uint32_t lines, float *x, float *y, float *z, float d)
{
    float len = course_length(lines), t = 0;
    int n, s3 = 0, s2 = 0, v1 = 0;
    line_t l;
    uint32_t a, b;
    if (d < 0) d = d + len;
    else if (len <= d) d = d - len;
    if (d < len * K(0x3f000000u)) d = d + len;
    l = line_get(lines, 4);
    n = (int)l.count;
    if (d < GT4_F32(line_point(&l, 0) + 0x14)) return -1;
    if (GT4_F32(line_point(&l, n - 1) + 0x14) <= d) return -1;
    while (v1 < n - 1) {
        s3 = v1;
        s2 = s3 + 1;
        v1 = s2;
        if (GT4_F32(line_point(&l, s3) + 0x14) <= d && d < GT4_F32(line_point(&l, s2) + 0x14)) {
            float d0 = GT4_F32(line_point(&l, s3) + 0x14);
            t = (d - d0) / (GT4_F32(line_point(&l, s2) + 0x14) - d0);
            break;
        }
    }
    a = line_point(&l, s3);
    b = line_point(&l, s2);
    *x = t * (GT4_F32(b + 4) - GT4_F32(a + 4)) + GT4_F32(a + 4);
    *y = t * (GT4_F32(b + 8) - GT4_F32(a + 8)) + GT4_F32(a + 8);
    *z = t * (GT4_F32(b + 0xc) - GT4_F32(a + 0xc)) + GT4_F32(a + 0xc);
    return s3;
}

/* func_0046B480: the line's point at distance d along the course (straight
   or arc between its two points). 0 when it wrote nothing. The pit line
   (4) goes to func_0046B7B8, as every caller does. */
static int line_at(uint32_t lines, int index, int from, float *x, float *y, float *z, float d)
{
    float len = course_length(lines), t, span;
    int i0 = 0, i1 = 0, code;
    line_t l;
    uint32_t a, b;
    if (index == 4) return pit_line_at(lines, x, y, z, d) >= 0;
    if (d < 0) d = d + len;
    else if (len <= d) d = d - len;
    l = line_get(lines, index);
    code = line_find(&l, from, &i0, &i1, d);
    a = line_point(&l, i1);
    b = line_point(&l, i0);
    if (code == 1) { span = (GT4_F32(b + 0x14) + len) - GT4_F32(a + 0x14); t = (d + len) - GT4_F32(a + 0x14); }
    else if (code == 2) { span = (GT4_F32(b + 0x14) + len) - GT4_F32(a + 0x14); t = d - GT4_F32(a + 0x14); }
    else if (code == 3) { t = d - GT4_F32(a + 0x14); span = GT4_F32(b + 0x14) - GT4_F32(a + 0x14); }
    else return 0;
    t = t / span;
    {
        float ax = GT4_F32(a + 4), ay = GT4_F32(a + 8), bx = GT4_F32(b + 4), by = GT4_F32(b + 8);
        if (!GT4_U8(a)) {
            *x = t * (bx - ax) + ax;
            *y = t * (by - ay) + ay;
        } else {
            float start = GT4_F32(a + 0x24), r = GT4_F32(a + 0x18), cx, cy, a0, a1, ang, two_pi = K(0x40c90fdau);
            float c = -gt4_cosf(start), s = gt4_sinf(start);
            cx = ax - s * r;
            cy = ay + c * r;
            a0 = gt4_atan2f(cx - ax, ay - cy);
            a1 = gt4_atan2f(cx - bx, by - cy);
            if (0 < r) {
                if (a1 < a0) a1 = a1 + two_pi;
            } else {
                r = -r;
                if (a0 < a1) a0 = a0 + two_pi;
            }
            ang = a0 + t * (a1 - a0);
            *x = cx - gt4_cosf(ang) * r;
            *y = cy + gt4_sinf(ang) * r;
        }
        *z = t * (GT4_F32(b + 0xc) - GT4_F32(a + 0xc)) + GT4_F32(a + 0xc);
    }
    return 1;
}

/* func_00464868: the line's point `ahead` metres past the car's distance
   along the course, pushed further (1 m at a time, 9 times) until it is at
   least 5 m from the car. Offsets from the car in *dx, *dy; returns the
   distance used. */
static float aim_point(uint32_t lines, uint32_t car, int index, int from, float *dx, float *dy, float ahead)
{
    float p[3] = { GT4_F32(car + CAR_POSITION), GT4_F32(car + CAR_POSITION + 4), GT4_F32(car + CAR_POSITION + 8) };
    float x = 0, y = 0;
    for (int k = 0; ; ) {
        line_at(lines, index, from, &p[0], &p[1], &p[2], GT4_F32(car + 0x5a8) + ahead);
        x = p[0] - GT4_F32(car + CAR_POSITION);
        y = p[1] - GT4_F32(car + CAR_POSITION + 4);
        if (5.0f <= ps2_sqrt(x * x + y * y)) break;
        if (k < 9) ahead = ahead + 1.0f;
        k++;
        if (!(k < 10)) break;
    }
    *dx = x;
    *dy = y;
    return ahead;
}

/* func_00463BB8: the driver profile of the car's player. */
static uint32_t profile(uint32_t obj)
{
    return GT4_U32(GT4_U32(obj + 0xe4) + GT4_U8(GT4_U32(obj + 0xe8) + CAR_PLAYER) * 4u + 0x10024u);
}

/* func_003F33D8 */
static int replay_mode(uint32_t car) { return (int8_t)GT4_U8(car + CAR_56A) == 1 && !GT4_U8(car + CAR_56B); }

static uint32_t vcall(uint32_t obj, uint32_t table, uint32_t off, uint32_t a1, uint32_t a2)
{
    uint32_t e;
    if (table == 0x10140u) return gt4_session(obj, off, a1, a2, 0, 0, 0, NULL);
    e = GT4_U32(obj + table) + off;
    gt4_orig_a2 = a2;
    gt4_orig_call(GT4_U32(e + 4), obj + (uint32_t)(int32_t)(int16_t)GT4_U16(e), a1, 0, 0);
    return gt4_orig_v0;
}

static void line_advance(uint32_t obj);
static void line_set(uint32_t obj, int line);
static void driver_stats(uint32_t car, int kind, float speed);
static void lane_change(uint32_t obj);
static int line_pick(uint32_t lines, uint32_t i);

/* func_00463EA8: the section the car is in, and how much of the target
   speed the next corner allows (1 = all of it). */
static float corner_speed(uint32_t obj)
{
    uint32_t car = GT4_U32(obj + 0xe8), params = GT4_U32(car + CAR_PARAMS), lines = GT4_U32(obj + 0xec), prof;
    float result = 1.0f, len, dist = GT4_F32(car + 0x5a8), target, grip_cap, need, speed;
    int count, moved = 0, sec, s5;
    line_t l;

    OU8(0x43) = 0;
    if (OU8(0x1c) == 4) return result;
    l = line_get(lines, OU8(0x1c));
    count = (int)l.count;
    len = course_length(lines);
    sec = OU8(0x1d);
    if (!sec && len * 0.5f < dist) {
        OU8(0x1d) = (uint8_t)count;
        moved = 1;
    } else if (sec < count && GT4_F32(line_point(&l, sec) + 0x14) <= dist) {
        OU8(0x1d) = (uint8_t)(sec + 1);
        moved = 1;
    } else if (sec == count && dist < len * 0.5f) {
        OU8(0x1d) = 0;
    } else if (OU8(0x1d) && dist < GT4_F32(line_point(&l, sec - 1) + 0x14)) {
        OU8(0x1d) = (uint8_t)(OU8(0x1d) - 1);
        moved = 1;
    }
    if (moved) {
        if (replay_mode(car) && (uint8_t)(OU8(0x1e) - 1) < 2 && OF(0x24) - K(0x41055554u) < GT4_F32(car + CAR_SPEED))
            driver_stats(car, 0, OF(0x24));
        line_advance(obj);
    }

    prof = profile(obj);
    s5 = OU8(0x1d);
    if (s5 == count) s5 = 0;
    target = OF(0x28);
    if ((int8_t)GT4_U8(line_point(&l, s5) + 2) == -128) {
        s5++;
        if (!(s5 < count)) s5 -= count;
    }
    speed = GT4_F32(car + CAR_SPEED);
    if ((uint8_t)(OU8(0x1e) - 3) < 2) OU8(0x43) = 2;
    else if (OU8(0x1f) && target < speed) {
        float trim = GT4_F32(prof), scale = OF(0xbc), dist2, reach;
        uint8_t mode = GT4_U8(car + CAR_56A);
        if (!(int8_t)mode) scale = 1.0f;
        if (GT4_U8(AI_SETTINGS + 0xb) && (uint8_t)(mode - 1) < 2 && !GT4_U8(car + CAR_56B)) {
            float lo = gt4_curve_eval(gt4_ptr(AI_SETTINGS + 0xdc), scale);
            float hi = gt4_curve_eval(gt4_ptr(AI_SETTINGS + 0xe8), scale);
            scale = scale + (lo + (hi - lo) * ((float)(int32_t)GT4_U16(obj + 0x68) * K(0x38000000u)));   /* func_00463988 */
        }
        trim = trim * scale;
        if ((GT4_U32(car + 0x568) & 0xffff0000u) == 0x10000u) trim = trim * OF(0x70);
        {
            /* grip the corner allows */
            uint32_t layout = gt4_car_layout(car), p = line_point(&l, s5), tp;
            int tyre = GT4_U8(layout + 0xc), axle = GT4_U8(layout + 0x10) * 4;
            int8_t surface = (int8_t)GT4_U8(p + 0x12);
            float g;
            tp = gt4_tyre_params(params, tyre, (uint8_t)surface);
            g = GT4_F32(tp + 8 + (uint32_t)(surface * 4));
            g = g * gt4_surface_factor(car, (uint8_t)surface);
            g = g * gt4_curve_eval(gt4_ptr(tp + 0x1ac), (GT4_F32(params + 0x103c) * 0.5f) / GT4_F32(params + 0x2d4 + (uint32_t)axle));
            g = g * GT4_F32(tp + 0x16c);
            grip_cap = g * GT4_F32(params + 0xff4 + (uint32_t)axle);
            if (!(GT4_U8(car + 0x5b5) & 0x10)) {
                int n = GT4_U8(layout + LAYOUT_WHEELS), front = 0;
                float sum = 0;
                for (int i = 0; i < n; i++)
                    if ((int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i) > 0) {
                        sum = sum + GT4_F32(gt4_car_wheel(car, i) + 0x50);
                        front++;
                    }
                if (front) grip_cap = (grip_cap * sum) / (float)front;
            }
        }
        {
            /* braking distance from the speed now to the target */
            float v0 = speed, v1 = target, k2 = GT4_F32(prof + 0x70), k3 = GT4_F32(prof + 0x74), a, b, g, lim;
            uint32_t p = line_point(&l, s5);
            a = k2 * v0 * v0 + 1.0f;
            b = k2 * v1 * v1 + 1.0f;
            g = grip_cap * K(0x411ccccc);
            if (GT4_U8(p + 0x28) == 1) k3 = 0;
            b = b + k3;
            lim = GT4_F32(params + 0x101c);
            a = a + k3;
            a = 1.0f / (((g * (a + b)) * 0.5f) + ((g * (a + b)) * 0.5f));
            if (lim < a) lim = a;
            lim = lim * (1.0f - gt4_cosf(GT4_F32(p + 0x1c)));
            need = 0.0f + ((v0 + v1) * (v0 - v1)) * lim;
        }
        if (!((uint8_t)(GT4_U8(car + CAR_56A) - 1) < 2) && !GT4_U8(car + CAR_56B)) need = need + speed;
        {
            float f = (float)(int8_t)GT4_U8(line_point(&l, s5) + 1) * K(0x3c23d70au) + 1.0f;
            if (f < K(0x3dcccccc)) f = K(0x3dcccccc);
            need = need * f;
            if (1.0f < trim) need = need / trim;
        }
        {
            uint32_t session = GT4_U32(obj + 0xe4);
            uint8_t mode2 = GT4_U8(car + CAR_56A);
            if (!vcall(session, 0x10140u, 0x30, session, (uint32_t)s5 << 6) && (uint8_t)(mode2 - 1) < 2) {
                need = need + (float)(int32_t)OU8(0x6a);
                if (!GT4_U16(obj + 0x18) && 0 < OF(0x44)) {
                    need = need + 10.0f;
                    if ((int8_t)OU8(0x51) != -1) {
                        uint32_t other = session + 0x6698u + (uint32_t)(int8_t)OU8(0x51) * CAR_SIZE;   /* func_0034C190 */
                        if (0 < GT4_F32(other + 0x58)) need = need + 8.0f;
                    }
                }
            }
        }
        {
            uint32_t p = line_point(&l, s5);
            float dx = GT4_F32(p + 4) - GT4_F32(car + CAR_POSITION), dy = GT4_F32(p + 8) - GT4_F32(car + CAR_POSITION + 4);
            dist2 = ps2_sqrt(dx * dx + dy * dy);
        }
        if (need < dist2) {
            float spare = 0;
            if (trim < 1.0f) spare = need * (1.0f / trim - 1.0f);
            reach = dist2 - need;
            if (reach < spare + 25.0f) OU8(0x43) = 1;   /* func_00469490 */
            if (0 < spare && reach < spare) result = reach / spare;
        } else {
            OU8(0x1e) = OU8(0x1e) ? 4 : 3;
            OU8(0x43) = 2;
            if (replay_mode(car)) driver_stats(car, 1, speed);
        }
    }
    if (OU8(0x1c) == 3) return result;
    {
        float k = GT4_F32(prof + 0x6c);
        if (k < 1.0f) {
            float tq = GT4_F32(car + CAR_ENGINE_TORQUE), eb = GT4_F32(car + CAR_ENGINE_BRAKE), sum = tq + eb;
            if (sum != 0) k = (k * tq + eb) / sum;
        }
        return result * k;
    }
}

/* func_00464ED8 */
static void model_reset(uint32_t obj)
{
    uint32_t car = GT4_U32(obj + 0xe8);
    GT4_U32(obj + 0x34) = 0;
    OF(0x30) = GT4_F32(car + 0x56c);
    GT4_U32(obj + 0x38) = 0;
    OU8(0x3e) = 0;
    GT4_U16(obj + 0x3c) = 0;
    if (!GT4_U8(0x00623e54u) && 0 < OF(0x2c)) {
        float v = OF(0x2c) - GT4_F32(car + CAR_DT);
        OF(0x2c) = v;
        if (v < 0) GT4_U32(obj + 0x2c) = 0;
    }
}

/* func_00464E88: how far ahead to aim at this speed. */
static float aim_distance(uint32_t prof, float speed)
{
    float f;
    if (speed <= 0) return GT4_F32(prof + 0x1c);
    f = GT4_F32(prof + 0x18) * speed;
    if (1.0f <= f) return GT4_F32(prof + 0x20);
    return GT4_F32(prof + 0x1c) + f * (GT4_F32(prof + 0x20) - GT4_F32(prof + 0x1c));
}

/* Blend of the aim toward the second line (obj+0x2c, falling at AI_SETTINGS
   + 0x74 per second in mode 1). */
static void second_line(uint32_t obj, uint32_t car, uint32_t lines, float ahead, float *x, float *y, int blend_rate)
{
    float x2, y2;
    aim_point(lines, car, OU8(0x21), OU8(0x1d), &x2, &y2, ahead);
    if (blend_rate && GT4_U8(AI_SETTINGS + 4) == 1) {
        float dx = *x - x2, dy = *y - y2, d = ps2_sqrt(dx * dx + dy * dy), step = GT4_F32(AI_SETTINGS + 0x74) * GT4_F32(car + CAR_DT);
        if (d < step) GT4_U32(obj + 0x2c) = 0;
        else {
            float v = OF(0x2c) - step / d;
            OF(0x2c) = v;
            if (v < 0) GT4_U32(obj + 0x2c) = 0;
        }
    }
    *x = *x + (x2 - *x) * OF(0x2c);
    *y = *y + (y2 - *y) * OF(0x2c);
}

/* func_004649F0: the line's point at distance d (x, y, z to the out
   pointers) and its direction there, from the point 0.5 m on (moved on by
   0.5 m, up to ten times, until they are 0.2 m apart). */
static float line_heading(uint32_t lines, int index, int from, float *ox, float *oy, float *oz, float d)
{
    float x0 = 0, y0 = 0, z0 = 0, x1 = 0, y1 = 0, z1 = 0, dx = 0, dy = 0;
    line_at(lines, index, from, &x0, &y0, &z0, d);
    for (int k = 0; k < 10; k++) {
        d = d + 0.5f;
        line_at(lines, index, from, &x1, &y1, &z1, d);
        dx = x1 - x0;
        dy = y1 - y0;
        if (K(0x3e4ccccc) <= ps2_sqrt(dx * dx + dy * dy)) break;
    }
    *ox = x0;
    *oy = y0;
    *oz = z0;
    return gt4_atan2f(-dx, dy);
}

float gt4_line_heading(uint32_t lines, int index, int from, float d)
{
    float x, y, z;
    return line_heading(lines, index, from, &x, &y, &z, d);
}

/* func_0034C998: the race's AI level (1, 2, else 0). */
static int ai_level(uint32_t session)
{
    int setting = GT4_U8(session + 0xcbdd), level = setting == 0xff ? 0 : setting;   /* func_0034C968 */
    return level == 2 ? 1 : level == 3 ? 2 : 0;
}

/* func_00464BB0: the steering (obj+0x30) toward a, at the rate the
   profile allows (+0x60 per second). */
static void steer_to(uint32_t obj, float a)
{
    float dt = GT4_F32(GT4_U32(obj + 0xe8) + CAR_DT), cur = OF(0x30), step = GT4_F32(profile(obj) + 0x60) * dt;
    if (cur < a) {
        float v = cur + step;
        OF(0x30) = v;
        if (a < v) OF(0x30) = a;
    } else if (a < cur) {
        float v = cur - step;
        OF(0x30) = v;
        if (v < a) OF(0x30) = a;
    }
}

/* func_004646F8: the throttle for speed s against target v: the gap,
   within 10 km/h either way, times k, over base; 0..1. */
static float throttle_for(float s, float v, float base, float k)
{
    float d = s - v, t;
    if (d < K(0xc031c71bu)) d = K(0xc031c71bu);
    else if (K(0x4031c71bu) < d) d = K(0x4031c71bu);
    t = base + k * d;
    if (1.0f < t) return 1.0f;
    return t < 0 ? 0 : t;
}

/* func_00464770: pedals toward speed v (k: how hard): the throttle
   (obj+0x34) from 2 km/h under it; off the throttle 20 km/h over it (just
   over it when braking for a corner or in states 2..4) brakes fully
   (obj+0x38); obj+0x3C holds a part throttle. */
static void pedals(uint32_t obj, float v, float k)
{
    uint32_t b = GT4_U32(obj + 0xe8) + 0x104;
    float t;
    OF(0x34) = throttle_for(GT4_F32(b + 0x53c), v - K(0x3f0e38e3u), 0.75f, k);
    if (!((uint8_t)(OU8(0x1e) - 3) < 2) && !((uint8_t)(GT4_U8(b + 0x467) - 2) < 3)) v = v + K(0x40b1c71bu);
    t = OF(0x34);
    if (t == 0 && v < GT4_F32(b + 0x53c)) OF(0x38) = 1.0f;
    if (t == 1.0f) return;
    if (t != 0) GT4_U16(obj + 0x3c) = 12;
}

void gt4_ai_steer_to(uint32_t obj, float a) { steer_to(obj, a); }
void gt4_ai_pedals(uint32_t obj, float v, float k) { pedals(obj, v, k); }

/* A car told to stop (func_00464C80); the brake-test and drafting
   (func_00469AE8); the throttle cap of a launch (func_003F76E8): still
   the original. */
/* func_003F76E8: the throttle that holds speed `want` against the
   engine's pull in gear (1 flat out, 0 closed). */
static float throttle_hold(uint32_t car, float want)
{
    uint32_t b = car + 0x104, params = GT4_U32(car + CAR_PARAMS);
    int gear = GT4_U8(b + 0x520);
    float v = GT4_F32(b + 0x53c), sum, drag, ratio, t;
    want = want + K(0x3e0e38e3u);
    if (!gear) v = -v;
    sum = GT4_F32(b + 0x4e8) + GT4_F32(b + 0x4ec);
    if (sum <= 0) return K(0x3f800000u);
    v = (v - want) / K(0x3fb1c71bu);
    ratio = GT4_F32(params + 0x29c + (uint32_t)gear * 4);
    drag = GT4_F32(b + 0x4ec) * ratio;
    t = (GT4_F32(b + 0x5bc) + drag) / (sum * ratio) - v;
    if (K(0x3f800000u) < t) return K(0x3f800000u);
    if (t < 0) return 0;
    return t;
}

/* func_00343D60: the brake pedal, and the brake input of every wheel. */
static void brake_all(uint32_t car, float brake)
{
    GT4_F32(car + CAR_BRAKE_PEDAL) = brake;
    for (int i = 0; i < GT4_U8(gt4_car_layout(car) + LAYOUT_WHEELS); i++)
        GT4_F32(gt4_car_wheel(car, i) + WHEEL_BRAKE_IN) = brake;
}

/* func_003455C8: the car stopped for good (driver kind 8), unless it is
   in mode 4. */
static void car_park(uint32_t car)
{
    if ((int8_t)GT4_U8(car + 0x104 + 0x466) != 4) GT4_U8(car + 0x104 + 0x467) = 8;
}

/* func_00464C80: stopping: steer toward `a`, no throttle, and once still
   (under 0.25 km/h and 3 deg/s) parked. In the pits (driver 6) the car
   rolls in on the throttle for the countdown at obj+0x52. */
static void model_stop(uint32_t obj, float a)
{
    uint32_t car = GT4_U32(obj + 0xe8), b = car + 0x104;
    float v, w;
    model_reset(obj);
    steer_to(obj, a);
    GT4_U32(obj + 0x34) = 0;
    v = ps2_sqrt((GT4_F32(b + 0x544) * GT4_F32(b + 0x544) + GT4_F32(b + 0x548) * GT4_F32(b + 0x548))
                 + GT4_F32(b + 0x54c) * GT4_F32(b + 0x54c));
    w = ps2_sqrt((GT4_F32(b + 0x57c) * GT4_F32(b + 0x57c) + GT4_F32(b + 0x584) * GT4_F32(b + 0x584))
                 + GT4_F32(b + 0x580) * GT4_F32(b + 0x580));
    if (v < K(0x3d8e38e3u) && w < K(0x3d56774eu)) {
        GT4_U32(obj + 0x38) = 0;
        car_park(car);
        return;
    }
    if (GT4_U8(b + 0x467) != 6) {
        OF(0x38) = K(0x3f000000u);
        return;
    }
    if (OU8(0x52)) {
        float t = throttle_hold(car, GT4_F32(b + 0x53c) - K(0x3e8e38e3u));
        uint8_t c = OU8(0x52);
        OF(0x34) = t;
        if (c < 0x1e) {
            float k = (float)(int32_t)c * K(0x3d088888u);
            OF(0x34) = t * k;
            OF(0x38) = (K(0x3f800000u) - k) * K(0x3f4ccccc);
        }
        OU8(0x52) = (uint8_t)(c - 1);
    } else {
        OF(0x38) = K(0x3f4ccccc);
    }
}

/* func_00463CD8: the pit limiter's throttle (obj+0x34) for the pit
   speed `v` (km/h; below 13 the idle creep): on until 3/4 of the shift
   rpm, off above 15/16. */
void gt4_ai_pit_throttle(uint32_t obj, int32_t v)
{
    uint32_t car = GT4_U32(obj + 0xe8), b = car + 0x104, params = GT4_U32(car + CAR_PARAMS);
    float grade = (float)(int8_t)GT4_U8(car + 0x5b3), dt, t;
    int on;
    GT4_U32(b + 0x514) = 0;
    if (v < 13) {
        float x = (float)((int16_t)GT4_U16(b + 0x4fa) - (int32_t)GT4_U16(params + 0x262)) / (float)(int32_t)GT4_U16(params + 0x200);
        t = (K(0x3f200000u) - grade * K(0x3b03126eu)) - (x + x);
        OF(0x34) = t;
        if (K(0x3f800000u) < t) OF(0x34) = K(0x3f800000u);
        else if (t < 0) OF(0x34) = 0;
        return;
    }
    dt = GT4_F32(car + CAR_DT);
    on = OU8(0x3f);
    if (!on) {
        if ((int16_t)GT4_U16(b + 0x4fa) < (int32_t)(GT4_U16(params + 0x262) * 3) >> 2) {
            on = 1;
            OU8(0x3f) = 1;
        }
    } else {
        int32_t lim = (int32_t)GT4_U16(params + 0x262) * 15;
        if (!((int16_t)GT4_U16(b + 0x4fa) < lim / 16)) {
            OU8(0x3f) = 0;
            on = 0;
        }
    }
    t = OF(0x34);
    if (on) {
        t = t + dt * (K(0x40a00000u) - grade * K(0x3e4ccccc));
        OF(0x34) = t;
        if (K(0x3f800000u) < t) OF(0x34) = K(0x3f800000u);
    } else {
        t = t - dt * (K(0x41200000u) - grade * K(0x3ecccccc));
        OF(0x34) = t;
        if (t < 0) OF(0x34) = 0;
    }
}

/* func_00464F38: the model's pedals to the car: the throttle eased
   toward obj+0x34 (a spring when D_00623E50 is set, else a ramp for
   sections 1, 2), steering, brake, reverse and the shift timer. */
void gt4_ai_apply(uint32_t obj)
{
    uint32_t car = GT4_U32(obj + 0xe8), b = car + 0x104;
    float dt = GT4_F32(car + CAR_DT), t;
    if (GT4_U8(AI_SETTINGS)) {
        float cur = GT4_F32(b + 0x488), rate = OF(0x14);
        rate = rate + ((GT4_F32(AI_SETTINGS + 0x24) * (OF(0x34) - cur)) - GT4_F32(AI_SETTINGS + 0x44) * rate) * dt;
        cur = cur + rate * dt;
        GT4_F32(b + 0x488) = cur;
        if (K(0x3f800000u) < cur) {
            GT4_F32(b + 0x488) = K(0x3f800000u);
            GT4_U32(obj + 0x14) = 0;
        } else if (cur < 0) {
            GT4_U32(b + 0x488) = 0;
            GT4_U32(obj + 0x14) = 0;
        } else {
            OF(0x14) = rate;
        }
        t = GT4_F32(b + 0x488);
    } else {
        t = OF(0x34);
        if ((uint8_t)(OU8(0x1e) - 1) < 2) {
            float cur = GT4_F32(b + 0x488), want = t;
            if (cur < want) {
                t = cur + dt;
                if (want < t) t = want;
            } else {
                t = cur - dt;
                if (t < want) t = want;
            }
        }
        GT4_F32(b + 0x488) = t;
    }
    GT4_F32(b + 0x494) = t;
    GT4_F32(b + 0x468) = OF(0x30);
    brake_all(car, OF(0x38));
    GT4_U8(b + 0x462) = OU8(0x3e);
    if (GT4_U16(obj + 0x3c)) GT4_U8(b + 0x526) = OU8(0x3c);
}

#define STEER_TO(obj, a)        steer_to((obj), (a))
#define PEDALS(obj, v, k)       pedals((obj), (v), (k))
#define STOP(obj, a)            model_stop((obj), (a))

/* func_00465078: the driver model's step. Where the car aims (the line
   `ahead` metres on, blended toward the second line, offset sideways),
   kept at car+0x720; for the player's car that is all. An AI car then
   decides how it is going (obj+0x1B: 0 on the line, 1 turning back to it,
   2 reversing out), steers toward the aim with a PID on the line's heading
   and the car's slip (gains at AI_SETTINGS), and picks its target speed
   and pedals (a slower car ahead, the line's own speeds, the start).
   Always 1. */
int gt4_ai_model(uint32_t obj)
{
    uint32_t car = GT4_U32(obj + 0xe8), b = car + 0x104, session = GT4_U32(obj + 0xe4), lines = GT4_U32(obj + 0xec), prof;
    float ratio = 1.0f, ahead, x, y, offset = 0, heading_c, heading_s, speed;
    float turn, angle, steer = 0, slip_avg = 0, over = 0;       /* f30, f20, f26, sp60, sp48 */
    int held = 0, dir = 0, reverse = 0, moving = 0, fine;       /* sp4C, sp3C, sp40, sp5C, fp */
    uint32_t h_front, h_rear;

    if ((uint8_t)(GT4_U8(b + 0x466) - 1) < 2) held = GT4_U8(b + 0x4bc) != 0;
    if (held) ratio = OF(0x34);
    model_reset(obj);
    if (held) OF(0x34) = ratio;
    if (GT4_U8(b + 0x467) == 7) {
        STOP(obj, 0.0f);
        return 1;
    }
    prof = profile(obj);
    ratio = corner_speed(obj);
    {
        uint32_t h = session + 0xda6cu + 0xc + GT4_U8(b + 0x440) * 0x4f0u;
        h_front = GT4_U32(h + 0x434);                           /* func_0045CC48 */
        h_rear = GT4_U32(h + 0x438);                            /* func_0045CC50 */
    }
    heading_c = -gt4_cosf(GT4_F32(b + 0x570));
    heading_s = gt4_sinf(GT4_F32(b + 0x570));

    /* how far ahead to aim */
    ahead = 5.0f;
    {
        uint8_t st = GT4_U8(b + 0x467);
        if (st == 3) {
        } else if (st == 4 && OU8(0x20)) {
            ahead = 10.0f;
            if (GT4_F32(b + 0x53c) < K(0x41055554u)) ahead = 5.0f;
        } else if (OU8(0x1b)) {
            ahead = OU8(0x1b) == 1 ? 10.0f : 40.0f;
        } else {
            int calm = 1;
            ahead = aim_distance(prof, GT4_F32(b + 0x53c));
            if (OU8(0x1a)) {
                calm = 0;
                if (K(0x41055554u) <= GT4_F32(b + 0x53c)) OU8(0x1a) = 1;
            }
            {
                uint32_t third = GT4_U32(session + 0xf89cu) / 3u;   /* func_00358418 */
                int32_t lead = (int16_t)GT4_U16(session + GT4_U8(b + 0x440) * 2u + 0xf878u) * 1000;
                int32_t a = (calm ? 5000 : 2000) - (int32_t)third + lead;
                if (a > 0) ahead = ahead + ((float)a * (calm ? 200.0f : 40.0f)) / (calm ? 5000.0f : 2000.0f);
            }
            if (OU8(0x52)) ahead = ahead + (float)(int32_t)OU8(0x52) * K(0x3eaaaaaau);
        }
    }
    ahead = aim_point(lines, car, OU8(0x1c), OU8(0x1d), &x, &y, ahead);
    if (0 < OF(0x2c)) second_line(obj, car, lines, ahead, &x, &y, 1);

    /* the aim's sideways offset */
    if (GT4_U8(AI_SETTINGS + 5) && !OU8(0x1b)) {
        uint8_t flags = GT4_U8(AI_SETTINGS + 5);
        if (flags & 1) {
            float lx, ly, k;
            aim_point(lines, car, OU8(0x1c), OU8(0x1d), &lx, &ly, 0);
            if (OF(0x2c) != 0) second_line(obj, car, lines, 0, &lx, &ly, 0);
            k = (uint8_t)(OU8(0x1c) - 3) < 2 ? GT4_F32(AI_SETTINGS + 0x7c) : GT4_F32(AI_SETTINGS + 0x78);
            offset = k * (gt4_sinf(GT4_F32(b + 0x570)) * lx + gt4_cosf(GT4_F32(b + 0x570)) * ly) + offset;
        }
        if ((flags & 2) && OU8(0x1c) != 4) {
            float o = 0;
            if (OF(0x58) < 2.0f) o = (2.0f - OF(0x58)) + o;
            if (OF(0x5c) < 2.0f) o = o - (2.0f - OF(0x5c));
            offset = offset + o;
        }
        if (!ai_level(session) && (flags & 4) && (!OU8(0x1e) || !(int8_t)OU8(0x43))) {
            uint32_t layout = gt4_car_layout(car);
            int left = 0, right = 0, n = GT4_U8(layout + LAYOUT_WHEELS);
            for (int i = 0; i < n; i++)
                if (GT4_U32(0x00622430u + GT4_U8(car + CAR_GROUND + (uint32_t)i * GROUND_SIZE + GROUND_SURFACE) * 4u)) {
                    uint8_t side = GT4_U8(layout + 0x1c + i);   /* func_003F4EA0 */
                    if (!side) left = 1;
                    else if (side == 1) right = 1;
                }
            if (left - right) offset = offset + (float)(left - right) * 0.5f;
        }
        {
            float cap = GT4_F32(AI_SETTINGS + 0x84);
            if (GT4_U8(AI_SETTINGS + 8)) {
                float g = fabsf(GT4_F32(b + 0x58c)), hi = GT4_F32(AI_SETTINGS + 0x8c), lo;
                if (hi <= g) cap = 0;
                else {
                    lo = GT4_F32(AI_SETTINGS + 0x88);
                    if (lo < g) cap = (hi - g) / (hi - lo);
                }
            }
            if (offset < -cap) offset = -cap;
            else if (cap < offset) offset = cap;
        }
        {
            float cur = OF(0x54), rate = GT4_F32(AI_SETTINGS + 0x80) * GT4_F32(car + CAR_DT);
            if (cur < offset) {
                float v = cur + rate;
                OF(0x54) = v;
                if (offset < v) { OF(0x54) = offset; cur = offset; } else cur = v;
            } else if (offset < cur) {
                float v = cur - rate;
                OF(0x54) = v;
                if (v < offset) { OF(0x54) = offset; cur = offset; } else cur = v;
            }
            x = x + cur * GT4_F32(b + 0x18);
            y = y + cur * GT4_F32(b + 0x1c);
        }
    }
    GT4_F32(b + 0x61c) = GT4_F32(b) + x;
    GT4_F32(b + 0x620) = GT4_F32(b + 4) + y;
    if (!(int8_t)GT4_U8(b + 0x466)) {
        if (!GT4_U8(b + 0x467)) return 1;                     /* the player's car */
        if (GT4_U16(b + 0x4ba)) return 1;
    }
    if (held) return 1;

    /* how the car is going: on the line (0), turning back to it (1),
       reversing out (2); kept at least 180 frames once changed */
    turn = gt4_angle_diff(gt4_atan2f(-x, y), GT4_F32(b + 0x570));
    angle = turn < 0 ? -turn : turn;
    speed = GT4_F32(b + 0x53c);
    {
        uint8_t st = OU8(0x1b);
        float lim = K(0x3f860a90u);
        if (st == 1) {
            if (0 <= speed) lim = K(0x3e32b8c1u);
            else if (K(0xc1055554u) < speed) lim = K(0x3e32b8c1u) - (speed * K(0x3f5f66f1u)) / K(0x41055554u);
            dir = -1;
        } else if (angle < lim || st == 2) {
            if (K(0xbf0e38e3u) < speed) {
                dir = 1;
                if (speed < 0 && !GT4_U8(b + 0x522)) reverse = 1;
            } else reverse = 1;
        } else if (K(0x3f0e38e3u) < speed) reverse = 1;
        else dir = -1;
        if (OU8(0x6b)) OU8(0x6b) = (uint8_t)(OU8(0x6b) - 1);
        else {
            uint8_t was = st;
            if (st == 1) {
                if (angle < lim) {
                    if (K(0x3f555554u) < speed) OU8(0x1b) = 0;
                    else OU8(0x1b) = 2;
                } else if (h_rear) OU8(0x1b) = 2;
            } else if (st == 2) {
                if (angle < K(0x3f32b8c1u) && K(0x41055554u) < speed) OU8(0x1b) = 0;
                else if (h_front) {
                    float px, py, pz, a = line_heading(lines, OU8(0x1c), OU8(0x1d), &px, &py, &pz, GT4_F32(b + 0x4a4));
                    float d = gt4_angle_diff(a, GT4_F32(b + 0x570));
                    if (d < 0) d = -d;
                    if (K(0x3f860a90u) < d) OU8(0x1b) = 1;
                    else if (speed < K(0x3e8e38e3u) && K(0xbe8e38e3u) < speed) OU8(0x1b) = 1;
                    else if (K(0x3f860a90u) < angle) OU8(0x1b) = 1;
                }
            } else {
                if (lim <= angle) OU8(0x1b) = 2;
                else if (h_front && speed < K(0x3e8e38e3u) && K(0xbe8e38e3u) < speed) OU8(0x1b) = 1;
                else if (h_rear && K(0x3f490fd9u) < angle) OU8(0x1b) = 2;
            }
            if (GT4_U8(GT4_U32(car + CAR_PARAMS) + 0x32) == 4 && OU8(0x1b) == 1) OU8(0x1b) = 2;
            if (OU8(0x1b) != was) OU8(0x6b) = 0xb4;
        }
    }

    /* steering */
    fine = GT4_U8(AI_SETTINGS + 2) < 2;
    if (OU8(0x1b)) fine = 1;
    if (K(0x3f0e38e3u) < speed) moving = 1;
    else if (speed < K(0xbf0e38e3u)) moving = -1;
    if (moving && dir == 1) {
        uint32_t params = GT4_U32(car + CAR_PARAMS), layout = gt4_car_layout(car);
        float peak = 0, avg, target_slip, gain, lat, s0 = 0;     /* f27, f24, f28 */
        int n = 0, m = 0;
        for (int i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++)
            if ((int8_t)GT4_U8(layout + 0x3c + i) > 0) {
                n++;
                peak = peak + GT4_F32(gt4_tyre_params(params, i, GT4_U8(car + (uint32_t)i * 0xec + 0x1ed)) + 0xb8);
            }
        if (n) peak = peak / (float)n;
        avg = 0;
        for (int i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++)
            if ((int8_t)GT4_U8(layout + 0x3c + i) > 0) {
                m++;
                s0 = s0 + GT4_F32(car + (uint32_t)i * 0xec + 0x194);
            }
        if (m) slip_avg = slip_avg + s0 / (float)m;
        if (OU8(0x1b)) {
            if (K(0x3fb2b8c1u) < turn) steer = peak;
            else if (turn < K(0xbfb2b8c1u)) steer = -peak;
        }
        if (steer == 0) {
            float vx = GT4_F32(b + 0x544), vy = GT4_F32(b + 0x548), v, d2 = x * x + y * y, fc, fs, dt;
            v = ps2_sqrt(vx * vx + vy * vy);
            if (v < K(0x3fb1c71bu)) { fc = heading_c; fs = heading_s; }
            else {
                float a = gt4_atan2f(-vx, vy);
                fc = -gt4_cosf(a);
                fs = gt4_sinf(a);
            }
            target_slip = ((v * -2.0f) * (fs * x - fc * y)) / d2;
            if (GT4_F32(b + 0x53c) < 0) target_slip = -target_slip;
            lat = GT4_F32(b + 0x57c);
            gain = GT4_F32(prof + 0xc);
            if (GT4_U8(AI_SETTINGS + 2)) {
                float k = -(GT4_F32(b + 0x46c) - peak) / peak;
                if (1.0f < k) k = 1.0f;
                else if (k < -1.0f) k = -1.0f;
                gain = gain * k;
                if (!fine) {
                    float e, de, u, lx, ly, lz, ang, e1, e2, e3, rate;
                    dt = GT4_F32(car + CAR_DT);
                    e = slip_avg - OF(0x30);
                    de = e - OF(0);
                    OF(0) = e;
                    u = 0 - (GT4_F32(AI_SETTINGS + 0x30) * GT4_F32(b + 0x44c)) * de;
                    u = u + GT4_F32(AI_SETTINGS + 0x10) * e;
                    e = target_slip - lat;
                    de = e - OF(4);
                    OF(4) = e;
                    u = u + (GT4_F32(AI_SETTINGS + 0x34) * GT4_F32(b + 0x44c)) * de;
                    if ((0 < target_slip && lat < target_slip) || (target_slip < 0 && target_slip < lat))
                        u = u + (GT4_F32(AI_SETTINGS + 0x14) * e) * gain;
                    ang = line_heading(lines, OU8(0x1c), OU8(0x1d), &lx, &ly, &lz, GT4_F32(b + 0x4a4));
                    e1 = gt4_angle_diff(ang, gt4_atan2f(-GT4_F32(b + 0x544), GT4_F32(b + 0x548)));
                    e3 = gt4_angle_diff(ang, GT4_F32(b + 0x570));
                    e2 = (GT4_F32(b) - lx) * gt4_sinf(ang) + (GT4_F32(b + 4) - ly) * gt4_cosf(ang);
                    if (GT4_F32(AI_SETTINGS + 0x5c) < e2) e2 = GT4_F32(AI_SETTINGS + 0x5c);
                    else if (e2 < -GT4_F32(AI_SETTINGS + 0x5c)) e2 = -GT4_F32(AI_SETTINGS + 0x5c);
                    de = e1 - OF(8);
                    OF(8) = e1;
                    u = u + (GT4_F32(AI_SETTINGS + 0x38) * GT4_F32(b + 0x44c)) * de;
                    u = u + GT4_F32(AI_SETTINGS + 0x18) * e1;
                    de = e2 - OF(0xc);
                    OF(0xc) = e2;
                    u = u - (GT4_F32(AI_SETTINGS + 0x3c) * GT4_F32(b + 0x44c)) * de;
                    u = u + (GT4_F32(AI_SETTINGS + 0x1c) * e2) * peak;
                    de = e3 - OF(0x10);
                    OF(0x10) = e3;
                    u = u - (GT4_F32(AI_SETTINGS + 0x40) * GT4_F32(b + 0x44c)) * de;
                    u = u - GT4_F32(AI_SETTINGS + 0x20) * e3;
                    u = u - GT4_F32(AI_SETTINGS + 0x2c) * GT4_F32(b + 0x484);
                    rate = GT4_F32(b + 0x484) + u * dt;
                    {
                        float lim = GT4_F32(profile(obj) + 0x60);
                        if (lim < rate) rate = lim;
                        else if (rate < -lim) rate = -lim;
                    }
                    {
                        float a = OF(0x30) + rate * dt;
                        OF(0x30) = a;
                        if (K(0x3f490fd9u) < a) { OF(0x30) = K(0x3f490fd9u); GT4_F32(b + 0x484) = 0; }
                        else if (a < K(0xbf490fd9u)) { OF(0x30) = K(0xbf490fd9u); GT4_F32(b + 0x484) = 0; }
                        else GT4_F32(b + 0x484) = rate;
                    }
                }
            }
            if (0 < target_slip) {
                if (lat < target_slip) {
                    steer = steer + (target_slip - lat) * gain;
                    if (peak < steer) {
                        over = steer / peak;
                        if (!ai_level(session)) steer = peak;
                    }
                }
            } else if (target_slip < 0) {
                if (target_slip < lat) {
                    steer = steer - (lat - target_slip) * gain;
                    if (steer < -peak) {
                        over = -steer / peak;
                        if (!ai_level(session)) steer = -peak;
                    }
                }
            }
        }
        steer = steer + slip_avg;
        if (K(0x3f490fd9u) < steer) steer = K(0x3f490fd9u);
        else if (steer < K(0xbf490fd9u)) steer = K(0xbf490fd9u);
        if (moving == -1) steer = -steer;
    } else if (moving) {
        if (turn < K(0xbeb2b8c1u)) steer = K(0xbf060a90u);
        else if (K(0x3eb2b8c1u) < turn) steer = K(0x3f060a90u);
        else steer = turn * K(0x3fbffffeu);
        if (moving == -1) steer = -steer;
    }
    {
        float lim = GT4_F32(GT4_U32(car + CAR_PARAMS) + 0x54);
        if (lim < steer) steer = lim;
        else if (steer < -lim) steer = -lim;
    }
    if (GT4_U8(b + 0x467) == 6) {
        STOP(obj, steer);
        return 1;
    }
    if ((uint8_t)(GT4_U8(b + 0x466) - 1) < 2 || GT4_U8(b + 0x467)) {
        if (fine) STEER_TO(obj, steer);
    }

    /* pedals */
    if (dir == 1) {
        float cruise, target;                                   /* f21, f20 */
        uint8_t how;                                            /* s7 */
        if (reverse == 1) OF(0x38) = K(0x3db851ebu);
        OU8(0x20) = 0;
        if (OU8(0x1c) == 4) {
            cruise = K(0x40de38e2u);
            how = 3;
            OU8(0x20) = 1;
            target = cruise;
        } else if (OU8(0x1c) == 3) {
            float len = course_length(lines), d = GT4_F32(b + 0x4a4);
            line_t l = line_get(lines, 4);
            if (d < len * 0.5f) d = d + len;
            if (d < GT4_F32(line_point(&l, 1) + 0x14) || d < GT4_F32(line_point(&l, 2) + 0x14)
                || !(d < GT4_F32(line_point(&l, (int)l.count - 3) + 0x14))) {
                cruise = OF(0x24);
                how = OU8(0x1e);
                target = OF(0x28);
            } else {
                how = 1;
                cruise = GT4_F32(prof + 0x64) + K(0x3f0e38e3u);
                OU8(0x20) = 1;
                if (OF(0x24) < cruise) {
                    OU8(0x20) = 0;
                    cruise = OF(0x24);
                    how = OU8(0x1e);
                }
                target = GT4_F32(prof + 0x64) + K(0x3f0e38e3u);
                if (OF(0x28) < target) target = OF(0x28);
            }
        } else {
            int8_t rival = (int8_t)OU8(0x51);
            cruise = OF(0x24);
            target = OF(0x28);
            how = OU8(0x1e);
            if (rival != -1 && (int16_t)GT4_U16(obj + 0x18) < 0x78) {
                float v = GT4_F32(session + 0x6698u + (uint32_t)rival * CAR_SIZE + 0x640);   /* func_0034C190 */
                int8_t close = (int8_t)OU8(0x43);
                if (close > 0 && close < 3) v = v - K(0x40b1c71bu);
                else {
                    if (close > 0 && close < 6) v = v - K(0x4031c71bu);
                    v = v - K(0x3fb1c71bu);
                }
                if (v < K(0x41055554u)) v = K(0x41055554u);
                how = 4;
                if (v < target) target = v;
            }
            if (OU8(0x1b) == 2) {
                cruise = K(0x415e38e2u);
                goto reversing;
            }
        }
        if (OU8(0x1b) == 2) {
reversing:
            PEDALS(obj, cruise, -0.5f);
            GT4_U16(obj + 0x22) = 0;
        } else {
            switch (how < 5 ? how : 5) {
            case 0:
                GT4_U16(obj + 0x22) = 0;
                OF(0x34) = 1.0f;
                break;
            case 1: case 2: {
                float v = cruise;
                if (!(GT4_U8(b + 0x4b1) & 0x10) && !((int32_t)GT4_U16(obj + 0x22) < (int32_t)GT4_U32(prof + 0x68))) {
                    v = v + GT4_F32(prof + 0x5c);
                    if (GT4_U8(prof + 0x78)) {
                        float a = turn < 0 ? -turn : turn, lim = GT4_F32(prof + 0x7c);
                        if (a < lim) v = v + (GT4_F32(prof + 0x80) * (lim - a)) / lim;
                    }
                }
                PEDALS(obj, v, -0.5f);
                if (!(int8_t)OU8(0x43)) {
                    uint32_t params = GT4_U32(car + CAR_PARAMS), layout = gt4_car_layout(car);
                    int n = GT4_U8(layout + LAYOUT_WHEELS);
                    float mu = 0, r;
                    for (int i = 0; i < n; i++)
                        mu = mu + GT4_F32(gt4_tyre_params(params, i, GT4_U8(car + (uint32_t)i * 0xec + 0x1ed)));
                    mu = mu / (float)n;
                    r = GT4_F32(car + 0x664) / mu;
                    if (r < 0) r = -r;
                    if (K(0x3f666666u) < r) OU8(0x43) = 5;
                    else if (0.5f < r) OU8(0x43) = how == 1 ? 4 : 3;
                }
                GT4_U16(obj + 0x22) = (uint16_t)(GT4_U16(obj + 0x22) + 1);
                break;
            }
            case 3:
                PEDALS(obj, target, -2.0f);
                GT4_U16(obj + 0x22) = 0;
                break;
            case 4:
                PEDALS(obj, target, -1.0f);
                GT4_U16(obj + 0x22) = (uint16_t)(GT4_U16(obj + 0x22) + 1);
                break;
            }
            if (!(GT4_U16(obj + 0x22) < 0x1771)) GT4_U16(obj + 0x22) = 0x1770;
        }
        OF(0x34) = OF(0x34) * ratio;
        if ((int32_t)((int8_t)GT4_U8(b + 0x4af) * 100 + 5000) < (int32_t)(GT4_U32(session + 0xf89cu) / 3u)
            && !GT4_U8(session + 0xcbe1u))
            lane_change(obj);
    } else {
        if (dir == -1) {
            OU8(0x3e) = 1;
            OF(0x34) = 1.0f;
        } else OF(0x34) = 0;
        OF(0x38) = reverse ? 1.0f : 0;
        GT4_U16(obj + 0x22) = 0;
    }

    if (GT4_U8(0x00620de7u)) over = GT4_F32(b + 0x470);
    if (0 < over && !(GT4_U8(b + 0x520) < 2)) {
        float o = GT4_U8(0x00620de7u) ? over : over - 1.0f, k;
        k = 1.0f - GT4_F32(prof + 0x10) * o;
        if (k < 0) k = 0;
        OF(0x34) = OF(0x34) * k;
        {
            float v = OF(0x38) + o * GT4_F32(prof + 0x14);
            OF(0x38) = v;
            if (1.0f < v) OF(0x38) = 1.0f;
        }
    }
    if (GT4_U8(b + 0x467) == 5) {
        uint32_t start = session + 0xf818u;
        int a = GT4_U8(start + ((int8_t)GT4_U8(b + 0x466) == 2 ? 0x43 : 0x42)), c = OU8(0x52);
        float cap = K(0x41b1c71bu), k = 1.0f, t;
        if (!(a < 0x51)) a = 0;
        if (a) cap = (float)a * K(0x3e8e38e3u);
        if (c) {
            if (!a) {
                cap = GT4_F32(b + 0x53c) - K(0x3e8e38e3u);
                if (c < 0x1e) k = (float)c * K(0x3d088888u);
            }
            OU8(0x52) = (uint8_t)(c - 1);
        }
        t = throttle_hold(car, cap) * k;
        if (t < OF(0x34)) OF(0x34) = t;
    }
    return 1;
}

/* The player's car (mode 0): the same step, which stops after the aim. */
int gt4_ai_player_aim(uint32_t obj) { return gt4_ai_model(obj); }

/* func_00464E40: the model reset and its steering and pedals released. */
static void model_idle(uint32_t obj)
{
    model_reset(obj);
    steer_to(obj, 0.0f);
    OF(0x34) = 0;
    OF(0x38) = 0;
}

/* func_003F3400: a car driven by the model (an AI car, or the player's in
   a demo): the controller kind from the command (+1: 2 pad, 4 wheel, 6 ...
   to car+0x569), then the model's step (func_00465078); cars held still
   (car+0x5C0), stopping (+0x5BE) or starting (+0x56B = 8) get their own
   handling, and car+0x618 says whether the clutch is in. */
void gt4_ai_drive(uint32_t car, uint32_t cmd)
{
    uint32_t b = car + 0x104, obj = car + 0x14, params = GT4_U32(car + CAR_PARAMS);
    uint8_t kind = GT4_U8(cmd + 1);
    GT4_U32(b + 0x490) = 0;
    GT4_U8(b + 0x462) = 0;
    GT4_U8(b + 0x463) = 0;
    if (kind == 4) GT4_U8(b + 0x465) = 0;
    else if (kind < 5) GT4_U8(b + 0x465) = kind == 2 ? 2 : 1;
    else GT4_U8(b + 0x465) = kind == 6 ? 3 : 1;
    if (GT4_U8(b + 0x4bc)) gt4_ai_model(obj);
    params = GT4_U32(car + CAR_PARAMS);
    if (GT4_U8(params + 0x234 + 0x12) == 3) {                 /* func_00368398 */
        gt4_split_clutch(car);
        if (GT4_F32(b + 0x514) == 0 && GT4_U8(b + 0x467) == 8) { model_idle(obj); return; }
        if (!GT4_U8(b + 0x4bc)) gt4_ai_model(obj);
        return;
    }
    if (GT4_U8(params + 0x246) == 4) {
        GT4_F32(b + 0x514) = 1.0f;
        if (GT4_U8(b + 0x4bc) || GT4_U16(b + 0x4ba)) goto still;
        if (GT4_U8(b + 0x467) == 8) model_idle(obj);
        else gt4_ai_model(obj);
        return;
    }
    if (GT4_U8(b + 0x4bc)) {
        uint32_t session = GT4_U32(car + 4), idx = GT4_U8(b + 0x440), v;
        v = GT4_U32(GT4_U32(session) + 0xbc);                   /* func_0034C1C8 */
        if ((int32_t)idx >= 0 && (int32_t)idx < (int32_t)GT4_U32(session + 0xf8a0u))
            v = v + GT4_U32(session + 0xcbfcu + idx * 4);
        gt4_ai_pit_throttle(obj, (int32_t)v);
        goto still;
    }
    if (GT4_U16(b + 0x4ba)) {
        model_idle(obj);
        gt4_ai_apply(obj);
        return;
    }
    if (GT4_U8(b + 0x467) == 8) {
        GT4_F32(b + 0x514) = 0;
        model_idle(obj);
        return;
    }
    if (!GT4_U8(b + 0x522)) {
        GT4_F32(b + 0x514) = 0;
        if ((int32_t)(GT4_U16(params + 0x262) * 3) >> 2 < (int16_t)GT4_U16(b + 0x4fa)) GT4_F32(b + 0x514) = 1.0f;
    } else {
        GT4_F32(b + 0x514) = 1.0f;
        if ((int16_t)GT4_U16(b + 0x4fa) < (int32_t)GT4_U16(params + 0x202)) GT4_F32(b + 0x514) = 0;
    }
    gt4_ai_model(obj);
    return;
still:
    brake_all(car, 0.0f);                                     /* brakes off */
    GT4_F32(b + 0x468) = 0;
}

/* func_0036BEE0 (func_003F1E30): the rpm the model shifts at, scaled by
   car+0x83 / 128 in replay mode. */
uint32_t gt4_shift_rpm(uint32_t car)
{
    uint32_t rpm = GT4_U16(GT4_U32(car + CAR_PARAMS) + 0x262);
    if ((GT4_U32(car + 0x568) & 0xffff0000u) == 0x10000u) rpm = (rpm * GT4_U8(car + 0x83)) >> 7;
    return rpm;
}

/* func_0036BF68: the gear for a speed: the first whose shift rpm (less 2000
   in first, 500 above) gives more than it. */
int gt4_gear_for_speed(uint32_t car, float speed)
{
    uint32_t drive = GT4_U32(car + CAR_PARAMS) + PARAMS_DRIVE, n;
    if (speed < 0) return 0;
    if (GT4_U8(drive + DRIVE_GEARBOX) == 3 || GT4_U8(drive + DRIVE_GEARBOX) == 1) return 1;
    n = GT4_U8(drive + 2);
    for (uint32_t g = 1; g < n; g++) {
        int32_t margin = g == 1 ? 2000 : 500;
        float v = (float)((int32_t)gt4_shift_rpm(car) - margin) * K(0x3dd6774eu)
                  / GT4_F32(GT4_U32(car + CAR_PARAMS) + 0x2a0 + (g - 1) * 4);
        if (speed < v) return (int)g;
    }
    return (int)n;
}

/* func_00469E58: the gear the model wants for its target speed (obj+0x40,
   -1 none; obj+0x41 set in the states 3 and 4). */
void gt4_ai_gear(uint32_t obj)
{
    uint32_t car = GT4_U32(obj + 0xe8);
    float speed = GT4_F32(car + CAR_SPEED), target = OF(0x28);
    uint8_t state = OU8(0x1e);
    OU8(0x41) = 0;
    OU8(0x40) = 0xff;
    if (state == 4 && !(target < OF(0x24))) target = OF(0x24);
    if (speed < target) return;
    if (state == 3 || state == 4) OU8(0x41) = 1;
    OU8(0x40) = (uint8_t)gt4_gear_for_speed(car, target);
}

/* func_0046BDC0: distance along the course of point i of line 4 of a lines
   object (i < 0 counts from the end); 0 when the object has none. */
float gt4_line_point_distance(uint32_t lines, int i)
{
    line_t l;
    if (!GT4_U16(lines + 0xc)) return 0;
    l = line_get(lines, 4);
    if (i < 0) i += (int)l.count;
    return GT4_F32(line_point(&l, i) + 0x14);
}

/* func_0046BE20 / func_0046BE70: float `off` of point i of line 4 (no
   wrap), 0 when the object has none */
float gt4_line_point_value(uint32_t lines, int i, uint32_t off)
{
    line_t l;
    if (!GT4_U16(lines + 0xc)) return 0;
    l = line_get(lines, 4);
    return GT4_F32(line_point(&l, i) + off);
}

/* func_0046BB48: the first line present from the fallback list of line i
   (D_00624030, 6 per line, -1 ends; i >= 8 as 6), -1 when none is. */
static int line_pick(uint32_t lines, uint32_t i)
{
    uint32_t list = 0x00624030u + (i < 8 ? i : 6) * 6;
    uint32_t table = GT4_U32(lines + 8);
    for (int k = 0; k < 6; k++) {
        int e = (int8_t)GT4_U8(list + (uint32_t)k);
        if (e == -1) break;
        if (table && GT4_U32(table + (uint32_t)e * 4)) return e;           /* func_0046A050 */
    }
    return -1;
}

/* func_0034C518's line part: line i (through its fallbacks,
   func_0046BB48) at distance d along the course, in the physics frame;
   0 when there is none. The replay's cameras measure the course by it. */
int gt4_line_course_point(uint32_t lines, int i, float d, float out[3])
{
    int e = line_pick(lines, (uint32_t)i);
    if (e < 0) return 0;
    return line_at(lines, e, 0, &out[0], &out[1], &out[2], d);
}

/* func_0046B108: of the lines 6, 2 and 1 (D_00624020), the one whose
   point at distance d is nearest (x, y), under 100 m; 6 when none is. */
int gt4_line_nearest(uint32_t lines, float x, float y, float d)
{
    float best = 100.0f;
    int found = 6;
    for (uint32_t k = 0; k < 3; k++) {
        int want = (int)GT4_U32(0x00624020u + k * 4), got = line_pick(lines, (uint32_t)want);
        float p[3] = { 0, 0, 0 }, dx, dy, dist;
        if (got != want) continue;
        line_at(lines, got, 0, &p[0], &p[1], &p[2], d);
        dx = p[0] - x;
        dy = p[1] - y;
        dist = ps2_sqrt(dx * dx + dy * dy);
        if (dist < best) {
            best = dist;
            found = got;
        }
    }
    return found;
}

/* func_0046BEC0: where car i starts on line 4 (point 8 - i): position to
   pos, the vector at +0x1C to dir, the s16 at +0x10 to out. Nothing when
   the lines object has no line 4. */
void gt4_line_start(uint32_t lines, int i, uint32_t pos, uint32_t dir, uint32_t out)
{
    line_t l;
    uint32_t p;
    if (!GT4_U16(lines + 0xc)) return;
    l = line_get(lines, 4);
    p = line_point(&l, 8 - i);
    for (uint32_t k = 0; k < 3; k++) {
        GT4_F32(pos + k * 4) = GT4_F32(p + 4 + k * 4);
        GT4_F32(dir + k * 4) = GT4_F32(p + 0x1c + k * 4);
    }
    GT4_U32(out) = (uint32_t)(int32_t)(int16_t)GT4_U16(p + 0x10);
}

/* func_0046ADE8: a rolling start on line `want` (or the first present
   from its fallback list) at distance d along the course (with `clamp`,
   not before the line's last point less a lap): position x, y and the
   ground's height to px, py, pz, the heading toward the point 10 m on to
   heading, the ground id to ground. 0 when no line is present. */
int gt4_line_rolling_start(uint32_t lines, uint32_t want, uint32_t ground, uint32_t px, uint32_t py,
                           uint32_t pz, uint32_t heading, int clamp, float d)
{
    float len = course_length(lines), x = 0, y = 0, z = 0, x2 = 0, y2 = 0, z2 = 0, h;
    int li = line_pick(lines, want), n, from, miss;
    uint32_t id, id2;
    line_t l;
    if (li == -1) return 0;
    l = line_get(lines, li);
    n = (int)l.count;
    from = n - 1;
    if (clamp) {
        float last = GT4_F32(line_point(&l, n - 1) + 0x14) - len;
        if (d < last) d = last;
    }
    while (d < 0) d = d + len;
    while (len <= d) d = d - len;
    for (int i = 0; i < n; i++)
        if (d <= GT4_F32(line_point(&l, i) + 0x14)) {
            from = i;
            break;
        }
    line_at(lines, li, from, &x, &y, &z, d);
    line_at(lines, li, from, &x2, &y2, &z2, d + 10.0f);
    id = (uint32_t)(int32_t)(int16_t)GT4_U16(line_point(&l, from) + 0x10);
    GT4_F32(px) = x;
    GT4_F32(py) = y;
    h = gt4_line_ground(GT4_U32(lines), x, y, z, &id, &miss);
    GT4_F32(pz) = h;
    if (miss) {
        id2 = 0xffffffffu;
        h = gt4_line_ground(GT4_U32(lines), x, y, z + 1.0f, &id2, &miss);
        GT4_F32(pz) = h;
        if (!miss) id = id2;
    }
    GT4_F32(heading) = gt4_atan2f(x - x2, y2 - y);
    GT4_U32(ground) = id;
    return 1;
}

/* func_004639E0: the model at the race start: its random seed (the grid
   slot + 1 in a race), no memory of the others, target speed the top
   speed, the line (`line`, or with -1 the one nearest the car), its
   section (func_00463C00), the rest cleared. */
void gt4_ai_start(uint32_t obj, uint32_t line)
{
    uint32_t car = GT4_U32(obj + 0xe8), b = car + 0x104, lines = GT4_U32(obj + 0xec);
    int l;
    GT4_U16(obj + 0x68) = (uint16_t)((int8_t)GT4_U8(b + 0x466) == 2 ? GT4_U8(b + 0x440) + 1 : 1);
    GT4_U16(obj + 0x68) = (uint16_t)(((GT4_U16(obj + 0x68) * 0x41c64e6du + 0x3039u) >> 16) & 0x7fff);   /* func_00463960 */
    for (uint32_t k = 0; k < 0x40; k++) GT4_U8(obj + 0x78 + k) = 0xff;
    OU8(0x1b) = 0;
    OU8(0x6b) = 0;
    OF(0x28) = GT4_F32(GT4_U32(car + CAR_PARAMS) + 0xfd8);
    OF(0x24) = OF(0x28);
    if (line == 0xffffffffu) l = gt4_line_nearest(lines, GT4_F32(b), GT4_F32(b + 4), GT4_F32(b + 0x4a4));   /* func_004639B0 */
    else l = line_pick(lines, line);
    l = line_pick(lines, (uint32_t)l);
    line_set(obj, l);
    OU8(0x20) = 0;
    GT4_U16(obj + 0x22) = 0;
    OU8(0x3f) = 0;
    OU8(0x52) = 0;
    GT4_U32(obj + 0x44) = 0;                                /* func_004694A0 */
    for (uint32_t k = 0; k < 8; k++) OU8(0x4f - k) = 0;
    OU8(0x53) = 0xff;
    OF(0x64) = -1000.0f;
    OF(0x60) = 1000.0f;
    OF(0x58) = 1000.0f;
    OF(0x5c) = 1000.0f;
    GT4_U16(obj + 0x18) = 0;
    OU8(0x6a) = 0;
    OU8(0x51) = 0xff;
    OU8(0x50) = 0;
    OU8(0x41) = 0;                                          /* func_00469E48 */
    OU8(0x40) = 0xff;
    OU8(0x1a) = 0;
}

/* func_0046AB80: going back from the end of line 6 (or its fallback), the
   distance before the lap's end (d0 < 0) of the first tight corner (an arc
   of radius under `tight` after one that is not); d0 when there is none. */
float gt4_line_tight_corner(uint32_t lines, float d0, float tight)
{
    int li = line_pick(lines, 6), n;
    float len, limit;
    line_t l;
    if (li == -1) return d0;
    l = line_get(lines, li);
    n = (int)l.count;
    len = course_length(lines);
    limit = len + d0;
    for (int i = n - 1; i >= 0; i--) {
        int j = i - 1 < 0 ? i - 1 + n : i - 1;
        uint32_t a = line_point(&l, i), b = line_point(&l, j);
        int ta = GT4_U8(a) != 0, tb = GT4_U8(b) != 0;
        if (tb) {
            float r = GT4_F32(b + 0x18);
            if (tight < r || r < -tight) tb = 0;
        }
        if (ta) {
            float r = GT4_F32(a + 0x18);
            if (tight < r || r < -tight) ta = 0;
        }
        if (!tb && ta && !(limit < GT4_F32(a + 0x14))) return GT4_F32(a + 0x14) - len;
    }
    return d0;
}

/* The cars around each car (func_003F4638): for every pair near enough,
   where each sits in the other's frame, the sectors the AI's model reads
   (obj+0x48..0x4F), the closest car ahead along the course and the draft.
   Pair tables in the conductor, six cars each way: the draft gap[a][b]
   (float, +0xCC14) and the sector code sector[a][b] (byte, +0xCCA4). */
#define NCARS(c)        ((int)GT4_U32((c) + 0x10000u - 0x760u))
#define CAR_AT(c, i)    ((c) + 0x6698u + (uint32_t)(i) * 0x10a0u)
#define PAIR_GAP(c, a, b)    GT4_F32((c) + 0x10000u - 0x33ecu + (uint32_t)((a) * 6 + (b)) * 4)
#define PAIR_SECTOR(c, a, b) GT4_U8((c) + 0x10000u - 0x335cu + (uint32_t)((a) * 6 + (b)))
#define PF(car, o)      GT4_F32(GT4_U32((car) + CAR_PARAMS) + (o))   /* +0 length ahead, +4 behind, +8 width, +0xc height */
#define ONE_F           K(0x3f800000u)

/* func_004694A0: the model's view of the others, cleared. */
static void nearby_clear(uint32_t obj)
{
    GT4_U32(obj + 0x44) = 0;
    for (int k = 7; k >= 0; k--) OU8(0x48 + k) = 0;
    OU8(0x53) = 0xff;
    OF(0x64) = K(0xc47a0000u);
    OF(0x60) = K(0x447a0000u);
    OF(0x58) = K(0x447a0000u);
    OF(0x5c) = K(0x447a0000u);
}

/* func_00343BF0: a car out of the reckoning. */
static int nearby_out(uint32_t car)
{
    if (GT4_U8(car + 0x545)) return 1;
    return GT4_U8(car + 0x14 + 0x1c) == 4;
}

/* func_003F40F8: b seen from a (out[0], out[1]) and a from b (out[2],
   out[3]), out[4] the distance; 0 when apart. */
static int nearby_frame(float *out, uint32_t a, uint32_t b)
{
    uint32_t pa = a + 0x104, pb = b + 0x104;
    float d[3], dist;
    if (nearby_out(a) || nearby_out(b)) return 0;
    if ((GT4_U8(pa + 0x4b1) & 0x10) != (GT4_U8(pb + 0x4b1) & 0x10)) return 0;
    for (int k = 0; k < 3; k++) d[k] = GT4_F32(pb + k * 4) - GT4_F32(pa + k * 4);
    dist = ps2_sqrt((d[0] * d[0] + d[1] * d[1]) + d[2] * d[2]);
    out[4] = dist;
    if (K(0x42c80000u) < dist) return 0;
    if (d[2] < -(PF(b, 0xc) + K(0x40a00000u))) return 0;
    if (PF(a, 0xc) + K(0x40a00000u) < d[2]) return 0;
    out[0] = out[1] = out[2] = out[3] = 0;
    for (int k = 0; k < 3; k++) {
        out[0] = out[0] + GT4_F32(pa + 0xc + k * 4) * d[k];
        out[1] = out[1] + GT4_F32(pa + 0x18 + k * 4) * d[k];
        out[2] = out[2] - GT4_F32(pb + 0xc + k * 4) * d[k];
        out[3] = out[3] - GT4_F32(pb + 0x18 + k * 4) * d[k];
    }
    return 1;
}

static void mark2(uint32_t obj, int o) { if ((int8_t)OU8(o) < 2) OU8(o) = 2; }
static void mark1(uint32_t obj, int o) { if ((int8_t)OU8(o) <= 0) OU8(o) = 1; }

/* func_004694F8: the sectors around the car the other one is in (2 close,
   1 further): 0x48 ahead, 0x49/0x4A ahead left/right, 0x4B/0x4C
   alongside (nearest gaps at 0x58/0x5C), 0x4D/0x4E behind left/right,
   0x4F behind. */
static void nearby_sectors(uint32_t obj, float dist, float x, float y, float ahead, float behind, float width)
{
    float wide = width + K(0x40400000u);
    if (K(0x41a00000u) < dist) return;
    if (y < -wide || wide < y) return;
    if (ahead * K(0x3f666666u) < x) {
        float side = width * K(0x3f400000u);
        x = x - ahead;
        if (y < -side) {
            if (x < K(0x40a00000u)) mark2(obj, 0x49);
            else if (x < K(0x41a00000u)) mark1(obj, 0x49);
            return;
        }
        if (side < y) {
            if (x < K(0x40a00000u)) mark2(obj, 0x4a);
            else if (x < K(0x41a00000u)) mark1(obj, 0x4a);
            return;
        }
        if (x < K(0x41a00000u)) mark2(obj, 0x48);
        if (y < -(width * K(0x3f000000u))) {
            if (x < K(0x40a00000u)) mark2(obj, 0x49);
            else if (x < K(0x41a00000u)) mark1(obj, 0x49);
        }
        if (width * K(0x3f000000u) < y) {
            if (x < K(0x40a00000u)) mark2(obj, 0x4a);
            else if (x < K(0x41a00000u)) mark1(obj, 0x4a);
        }
        return;
    }
    if (x < -(behind * K(0x3f666666u))) {
        if (y < -width) { mark2(obj, 0x4d); return; }
        if (width < y) { mark2(obj, 0x4e); return; }
        mark2(obj, 0x4f);
        if (0 < y) mark1(obj, 0x4e);
        else mark1(obj, 0x4d);
        return;
    }
    if (y < 0) {
        float g = -y - width;
        if (g < OF(0x58)) OF(0x58) = g;
        mark2(obj, 0x4b);
        if (ahead * K(0x3f000000u) < x) mark1(obj, 0x49);
        else if (x < -(behind * K(0x3f000000u))) mark1(obj, 0x4d);
    } else {
        float g = y - width;
        if (g < OF(0x5c)) OF(0x5c) = g;
        mark2(obj, 0x4c);
        if (ahead * K(0x3f000000u) < x) mark1(obj, 0x4a);
        else if (x < -(behind * K(0x3f000000u))) mark1(obj, 0x4e);
    }
}

/* func_003F4520: the other car's sector code, lengthwise (1..6) and
   sideways (0x10..0x70); 0 beyond 50 m. */
static uint8_t nearby_code(float dist, float x, float y, float ahead, float behind, float width)
{
    uint8_t v;
    if (K(0x42480000u) < dist) return 0;
    if (ahead * K(0x40800000u) < x) v = 6;
    else if (ahead < x) v = 5;
    else if (0 < x) v = 4;
    else if (-behind <= x) v = 3;
    else if (-behind * K(0x40800000u) <= x) v = 2;
    else v = 1;
    if (y < -width * K(0x40800000u)) return v | 0x10;
    if (y < -width) return v | 0x20;
    if (y < -width * K(0x3f000000u)) return v | 0x30;
    if (y <= width * K(0x3f000000u)) return v | 0x40;
    if (y <= width) return v | 0x50;
    if (y <= width * K(0x40800000u)) return v | 0x60;
    return v | 0x70;
}

/* func_003F42D0: b ahead of a along the course (its length from the
   race), the gap between them and how fast a closes it. */
static int nearby_ahead(uint32_t a, uint32_t b, float y, float *gap, float *closing)
{
    uint32_t course = GT4_U32(GT4_U32(GT4_U32(GT4_U32(a + 4) + 0x10000u - 0x3428u) + 4) + 0xdc);
    float len = GT4_F32(course + 0x14), sa = GT4_F32(a + 0x5a8), sb = GT4_F32(b + 0x5a8), d, rel;
    *gap = 0;
    *closing = 0;
    if (len * K(0x3f400000u) < sa && sb < len * K(0x3e800000u)) d = (sb + len) - sa;
    else d = sb - sa;
    d = (d - PF(b, 4)) - PF(a, 0);
    if (d <= 0) return 0;
    rel = GT4_F32(b + 0x754) - GT4_F32(a + 0x754);
    if (d < K(0x41a00000u)) {
        float half = (PF(a, 8) + PF(b, 8)) * K(0x3f000000u);
        if (half < y || y < -half) return 0;
    }
    *gap = d;
    *closing = -rel;
    return 1;
}

/* func_00469868: forget b as the car to follow. */
static void nearby_forget(uint32_t a, uint32_t b)
{
    uint32_t obj = a + 0x14;
    if ((int8_t)OU8(0x51) == (int)GT4_U8(b + 0x544)) OU8(0x51) = 0xff;
}

/* func_00469888: the car ahead soonest reached (a+0x67 its id, +0x74 the
   gap, +0x78 the closing speed). */
static void nearby_soonest(uint32_t a, uint32_t b, float gap, float closing)
{
    float f = GT4_F32(a + 0x78);
    if (0 < closing) {
        if (!(f <= 0) && !(gap / closing < GT4_F32(a + 0x74) / f)) return;
    } else {
        if (0 < f) return;
        if (!(gap < GT4_F32(a + 0x74))) return;
    }
    GT4_F32(a + 0x74) = gap;
    GT4_F32(a + 0x78) = closing;
    GT4_U8(a + 0x67) = GT4_U8(b + 0x544);
}

/* func_00469918: how much b ahead weighs on a's model (obj+0x44), and b
   as the car to follow (obj+0x51) when close and being caught. */
static void nearby_follow(uint32_t a, uint32_t b, float gap, float closing)
{
    uint32_t obj = a + 0x14;
    int id, follow = -1;
    float w = 0;
    nearby_soonest(a, b, gap, closing);
    id = GT4_U8(b + 0x544);
    if ((int8_t)OU8(0x51) == id && K(0x40c00000u) < gap) nearby_forget(a, b);
    if ((int8_t)OU8(0x51) != id && 0 < closing && gap < K(0x40a00000u)) {
        w = K(0x40000000u);
        follow = id;
    } else if (K(0x40b1c71bu) < closing) {
        w = ONE_F;
    } else if (K(0x3ce38e37u) < closing && gap / closing < K(0x40a00000u)) {
        w = K(0x3f000000u);
    }
    if (w == 0) {
        if (gap < K(0x3f000000u)) w = K(0x3d4ccccc);
        else if (gap < K(0x40800000u)) w = K(0x3dccccccu);
    }
    if (OF(0x44) < w) {
        if (follow == -1) {
            OF(0x44) = w;
        } else {
            OU8(0x51) = (uint8_t)follow;
            OF(0x44) = ONE_F;
        }
    }
}

/* func_003F44A0: the draft of b on a car behind it at (x, y) in b's
   frame, above 100 km/h: 1 right behind, 0 at 1.5 times b's speed in
   metres. */
static float nearby_draft(uint32_t b, float x, float y)
{
    float v = GT4_F32(b + CAR_SPEED), w = PF(b, 8), reach;
    if (!(K(0x41de38e2u) < v)) return 0;
    reach = v + v * K(0x3f000000u);
    if (!(x < 0) || !(-reach < x) || !(-w < y) || !(y < w)) return 0;
    return (reach + x) / reach;
}

void gt4_cars_nearby(uint32_t c)
{
    for (int i = 0; i < NCARS(c); i++) GT4_U8(CAR_AT(c, i) + 0x7e) = 0;
    for (int i = 0; i < NCARS(c); i++) nearby_clear(CAR_AT(c, i) + 0x14);
    for (int i = 0; i < NCARS(c); i++)
        for (int j = i + 1; j < NCARS(c); j++) {
            uint32_t a = CAR_AT(c, i), b = CAR_AT(c, j);
            float f[5], ahead, behind, width, gap, closing;
            if (!nearby_frame(f, a, b)) {
                PAIR_GAP(c, i, j) = 0;
                PAIR_GAP(c, j, i) = 0;
                PAIR_SECTOR(c, i, j) = 0;
                PAIR_SECTOR(c, j, i) = 0;
                continue;
            }
            width = (PF(a, 8) + PF(b, 8)) * K(0x3f000000u);
            ahead = PF(a, 0) + PF(b, 4);
            behind = PF(a, 4) + PF(b, 0);
            nearby_sectors(a + 0x14, f[4], f[0], f[1], ahead, behind, width);
            nearby_sectors(b + 0x14, f[4], f[2], f[3], behind, ahead, width);
            PAIR_SECTOR(c, i, j) = nearby_code(f[4], f[0], f[1], ahead, behind, width);
            PAIR_SECTOR(c, j, i) = nearby_code(f[4], f[2], f[3], behind, ahead, width);
            if (GT4_U8(c + 0x10000u - 0x7e6u) == 6) {
                nearby_forget(a, b);
                nearby_forget(b, a);
            } else {
                if (nearby_ahead(a, b, f[1], &gap, &closing)) nearby_follow(a, b, gap, closing);
                else nearby_forget(a, b);
                if (nearby_ahead(b, a, f[3], &gap, &closing)) nearby_follow(b, a, gap, closing);
                else nearby_forget(b, a);
            }
            PAIR_GAP(c, i, j) = nearby_draft(b, f[2], f[3]);
            PAIR_GAP(c, j, i) = nearby_draft(a, f[0], f[1]);
        }
    for (int i = 0; i < NCARS(c); i++) {
        float sum = 0;
        for (int j = 0; j < NCARS(c); j++) sum = sum + PAIR_GAP(c, i, j);
        if (ONE_F < sum) sum = ONE_F;
        GT4_F32(CAR_AT(c, i) + 0x6a0) = sum;
    }
    for (int i = 0; i < NCARS(c); i++) {
        int count = 0, k = i;
        for (int s = 0; s < NCARS(c); s++) {
            k = (int8_t)GT4_U8(CAR_AT(c, k) + 0x65);
            if (k == -1) break;
            count++;
        }
        GT4_U8(CAR_AT(c, i) + 0x7e) = (uint8_t)count;
    }
}

/* The driver's statistics for replays and B-Spec (func_004684A8): at
   car+0x8C a 8x8 table of u16 counts, row `kind`, filled around the
   column for the speed; then the column for the car's power band, the
   half of it counted in row 3 (0x30), split with row 4 (0x38) when
   racing among others (any of car+0x5C..0x63 set). */
static void stats_add(uint32_t base, int row, int col, int add)   /* func_004682A0 */
{
    uint32_t p = base + (uint32_t)(row * 8 + col) * 2;
    int v = GT4_U16(p) + add;
    if (0xffff < v) v = 0xffff;
    GT4_U16(p) = (uint16_t)v;
}

static void stats_spread(uint32_t base, int row, int i)            /* func_004682D0 */
{
    int t = i - 4, q = t / 8, lo = 64, hi = 0, next = 8;
    t = t - q * 8;
    if (!(t + 8 < 9)) {
        hi = t * 8;
        lo = (8 - t) * 8;
        next = q + 1;
    }
    if ((uint32_t)q < 8) stats_add(base, row, q, lo);
    if ((uint32_t)next < 8) stats_add(base, row, next, hi);
}

static void driver_stats(uint32_t car, int kind, float speed)
{
    uint32_t race = GT4_U32(car + 4), base = car + 0x8c, p;
    float lo = K(0x41855554u), span = (kind ? K(0x429071c6u) : K(0x427471c6u)) - lo, f, x;
    int crowd = 0, col, v;
    f = (speed - lo) * (ONE_F / span);
    if (f < 0) f = 0;
    else if (ONE_F < f) f = ONE_F;
    stats_spread(base, kind, (int32_t)(f * K(0x42800000u)));
    if (!vcall(race, 0x10140u, 0x30, 0, 0) && !(NCARS(race) < 2))
        for (int k = 0; k < 8; k++)
            if ((int8_t)GT4_U8(car + 0x5c + (uint32_t)k)) { crowd = 1; break; }
    x = (GT4_F32(GT4_U32(car + CAR_PARAMS) + 0xfe0) - K(0x3eb33333u)) / K(0x3f0cccccu);   /* func_004683B8 */
    if (x < 0) x = 0;
    else if (ONE_F < x) x = ONE_F;
    v = (int32_t)(x * K(0x41000000u));
    if (!(v < 8)) v = 7;
    col = v / 2;
    if (!crowd) {
        p = base + (uint32_t)col * 2 + 0x30;
        v = GT4_U16(p) + 0x1000;
    } else {
        p = base + (uint32_t)col * 2 + 0x30;
        v = GT4_U16(p) + 0x1000 / 3;
        GT4_U16(p) = (uint16_t)(0xffff < v ? 0xffff : v);
        p = base + (uint32_t)col * 2 + 0x38;
        v = GT4_U16(p) + 0x2000 / 3;
    }
    GT4_U16(p) = (uint16_t)(0xffff < v ? 0xffff : v);
}

/* func_004674F0: the target speed through section `sec` of the model's
   line: the grip of the tyres on its surface (front and rear by the
   profile's balance, per driven wheel when tyre wear is on), the corner's
   radius and banking, the profile's aero and skill terms, capped by the
   top speed, at least 20 km/h. */
static float section_speed(uint32_t obj, int sec)
{
    uint32_t car = GT4_U32(obj + 0xe8), params = GT4_U32(car + CAR_PARAMS), b = car + 0x104;
    uint32_t layout = gt4_car_layout(car), p, front, rear, prof;
    line_t l = line_get(GT4_U32(obj + 0xec), OU8(0x1c));
    float radius, bal, g[2], f, g2, add, v = 0, skill, base, trim, top;
    int surf;
    p = line_point(&l, sec);
    surf = (int8_t)GT4_U8(p + 0x12);
    radius = GT4_F32(p + 0x18);
    bal = GT4_F32(profile(obj) + 0x58);
    front = gt4_tyre_params(params, GT4_U8(layout + 0xc), (uint8_t)surf);
    rear = gt4_tyre_params(params, GT4_U8(layout + 0xd), (uint8_t)surf);
    if ((int8_t)GT4_U8(params + 0x104a) || (int8_t)GT4_U8(b + 0x5c2)) bal = (bal + K(0x3f000000u)) * K(0x3f000000u);
    g[0] = (bal * GT4_F32(front + 8 + (uint32_t)surf * 4)) * GT4_F32(front);
    g[1] = ((ONE_F - bal) * GT4_F32(rear + 8 + (uint32_t)surf * 4)) * GT4_F32(rear);
    {   /* func_00467470: the profile's grip on this surface's tyre */
        int8_t mode = (int8_t)GT4_U8(b + 0x466);
        f = ONE_F;
        if (mode == 1 || mode == 2 || GT4_U8(b + 0x467))
            f = GT4_F32(profile(obj) + 0xa8 + GT4_U8(params + (uint32_t)surf + 0xf98) * 4u);
    }
    g[0] = g[0] * f;
    g[1] = g[1] * f;
    {
        uint32_t flags = GT4_U32(obj + 0xe4) + 4;
        if ((GT4_U8(flags + 0x68) || GT4_U8(flags + 0x69)) && !(GT4_U8(b + 0x4b1) & 0x10)) {
            for (int t = 0; t < GT4_U8(layout + 1); t++) {
                int ax = GT4_U8(layout + 0x14 + (uint32_t)t);
                v = v + (GT4_F32(car + 0x1b4 + (uint32_t)t * 0xec) * g[ax]) / (float)(int32_t)GT4_U8(layout + 4 + (uint32_t)ax);
            }
        } else {
            v = g[0] + g[1];
        }
    }
    if ((int8_t)GT4_U8(b + 0x5c2)) v = v * K(0x3f666666u);
    f = GT4_F32(p + 0x20);
    if (0 <= radius) {
        v = v * radius;
        if (f < 0) v = v / (gt4_cosf(f) + ONE_F);
    } else {
        v = v * -radius;
        if (0 < f) v = v / (ONE_F - gt4_cosf(f));
    }
    prof = profile(obj);
    add = GT4_U8(p + 0x28) == 1 ? 0 : GT4_F32(prof + 0x74);
    g2 = add + ONE_F;
    for (int k = 0; k < 3; k++) {
        f = ps2_sqrt(v * g2);
        g2 = ((GT4_F32(prof + 0x70) * f) * f + ONE_F) + add;
    }
    prof = profile(obj);
    skill = OF(0xb8);
    base = GT4_F32(prof + 8);
    trim = GT4_F32(prof + 4);
    if (!(int8_t)GT4_U8(b + 0x466)) skill = ONE_F;
    if (GT4_U8(AI_SETTINGS + 0xa) && (uint8_t)(GT4_U8(b + 0x466) - 1) < 2 && !GT4_U8(b + 0x467)) {
        float lo = gt4_curve_eval(gt4_ptr(AI_SETTINGS + 0xac), skill), hi = gt4_curve_eval(gt4_ptr(AI_SETTINGS + 0xb8), skill);
        skill = skill + (lo + (hi - lo) * ((float)(int32_t)GT4_U16(obj + 0x68) * K(0x38000000u)));   /* func_00463988 */
    }
    trim = trim * skill;
    if ((GT4_U32(b + 0x464) & 0xffff0000u) == 0x10000u) trim = trim * OF(0x74);
    if (GT4_U8(AI_SETTINGS + 9) && ((GT4_U32(obj + 0x48) & 0xff00ff00u) || (int8_t)OU8(0x4c) || (int8_t)OU8(0x4a))) {
        float k = (int8_t)GT4_U8(b + 0x466) ? OF(0xc0) : ONE_F;
        trim = trim * (ONE_F - GT4_F32(AI_SETTINGS + 0x90) * k);
    }
    {
        int8_t s = (int8_t)GT4_U8(p + 2);
        base = base + (s == -128 ? 0 : (float)s) * K(0x3e8e38e3u);
    }
    if (!GT4_U8(params + (uint32_t)surf + 0xf98)) trim = trim * gt4_curve_eval(gt4_ptr(0x00623fa8u), GT4_F32(params + 0x418));
    v = trim * f + base;
    top = GT4_F32(params + 0xfd8);
    if (top < v) return top;
    if (v < K(0x40b1c71bu)) return K(0x40b1c71bu);
    return v;
}

/* func_004679C0: on to the next section: the target speed of the one
   after it. */
static void line_advance(uint32_t obj)
{
    uint32_t car = GT4_U32(obj + 0xe8), params = GT4_U32(car + CAR_PARAMS);
    line_t l = line_get(GT4_U32(obj + 0xec), OU8(0x1c));
    int count = (int)l.count, s;
    uint8_t kind;
    GT4_U16(obj + 0x68) = (uint16_t)(((GT4_U16(obj + 0x68) * 0x41c64e6du + 0x3039u) >> 16) & 0x7fff);   /* func_00463960 */
    s = OU8(0x1d);
    OU8(0x1e) = OU8(0x1f);
    if (s == count) s = 0;
    OF(0x24) = OF(0x28);
    if ((int8_t)GT4_U8(line_point(&l, s) + 2) == -128) {
        s++;
        if (!(s < count)) s -= count;
    }
    kind = GT4_U8(line_point(&l, s));
    OU8(0x1f) = kind;
    if (!kind) OF(0x28) = GT4_F32(params + 0xfd8);
    else if (kind < 3) OF(0x28) = section_speed(obj, s);
}

/* func_00467AA8: the section of the new line the car is in. */
static void line_enter(uint32_t obj)
{
    uint32_t car = GT4_U32(obj + 0xe8), params = GT4_U32(car + CAR_PARAMS);
    line_t l = line_get(GT4_U32(obj + 0xec), OU8(0x1c));
    int count = (int)l.count, s;
    float d = GT4_F32(car + 0x104 + 0x4a4);
    uint8_t kind;
    OU8(0x1d) = (uint8_t)count;
    for (int k = 0; k < count; k++)
        if (d < GT4_F32(line_point(&l, k) + 0x14)) {
            OU8(0x1d) = (uint8_t)k;
            break;
        }
    s = OU8(0x1d) + count - 1;
    while (!(s < count)) s -= count;
    if ((int8_t)GT4_U8(line_point(&l, s) + 2) == -128) {
        s--;
        if (s < 0) s += count;
    }
    kind = GT4_U8(line_point(&l, s));
    OU8(0x1f) = kind;
    OF(0x28) = kind ? section_speed(obj, s) : GT4_F32(params + 0xfd8);
    line_advance(obj);
}

/* func_00463C00: straight onto line `line`. */
static void line_set(uint32_t obj, int line)
{
    OU8(0x21) = (uint8_t)line;
    GT4_U32(obj + 0x54) = 0;
    OU8(0x1c) = (uint8_t)line;
    GT4_U32(obj + 0x2c) = 0;
    line_enter(obj);
}

/* func_00463C28: over to line `line` (the first present of its list),
   blending from the one it leaves (obj+0x2C back to 1, or its complement
   when returning); `now` switches at once. 0 when no line is present. */
int gt4_ai_line_switch(uint32_t obj, int line, int now)
{
    int got = line_pick(GT4_U32(obj + 0xec), (uint32_t)line);
    uint8_t old;
    if (got == -1) return 0;
    if (now) {
        line_set(obj, got);
        return 1;
    }
    OF(0x2c) = line == OU8(0x21) ? ONE_F - OF(0x2c) : ONE_F;
    old = OU8(0x1c);
    OU8(0x1c) = (uint8_t)got;
    OU8(0x21) = old;
    line_enter(obj);
    return 1;
}

/* func_0046BC00 / func_0046BC40: the line to the left / right of `line`
   (1 left, 6 middle, 2 right), -1 when none. */
static int lane_left(uint32_t lines, int line)
{
    uint32_t t = GT4_U32(lines + 8);
    if (line == 1 && GT4_U32(t + 0x18)) return 6;
    if (line == 1 || line == 6) return GT4_U32(t + 8) ? 2 : -1;
    return -1;
}

static int lane_right(uint32_t lines, int line)
{
    uint32_t t = GT4_U32(lines + 8);
    if (line == 2 && GT4_U32(t + 0x18)) return 6;
    if (line == 2 || line == 6) return GT4_U32(t + 4) ? 1 : -1;
    return -1;
}

/* func_00469AE8: changing lanes to pass. A car ahead being caught
   (obj+0x44) moves the model two lanes over, to the side the corner
   allows (the section kind obj+0x1F: 1 left, 2 right) and the sectors are
   free; back to the middle line when nothing is around. The lane change
   holds for 180 steps (obj+0x18). */
static void lane_change(uint32_t obj)
{
    uint32_t car = GT4_U32(obj + 0xe8), lines = GT4_U32(obj + 0xec), b = car + 0x104;
    int pass = 1, line;
    float want;
    if ((int16_t)GT4_U16(obj + 0x18) > 0) GT4_U16(obj + 0x18) = (uint16_t)(GT4_U16(obj + 0x18) - 1);
    line = OU8(0x1c);
    if (line == 4) return;
    if (line == 3) {
        int squeezed;
        if ((int8_t)OU8(0x4b) && (int8_t)OU8(0x49)) squeezed = 1;
        else squeezed = (int8_t)OU8(0x4c) && (int8_t)OU8(0x4a);
        if (squeezed) {
            if (GT4_F32(b + 0x53c) < K(0x41055554u)) GT4_U16(obj + 0x18) = (uint16_t)(GT4_U16(obj + 0x18) + 1);
            else OF(0x34) = OF(0x34) * K(0x3f000000u);
        }
        pass = 0;
    }
    switch ((int8_t)OU8(0x43)) {
    case 1: case 2: case 5: pass = 0; break;
    case 3: case 4: if ((uint8_t)(line - 1) < 2) pass = -1; break;
    default: break;
    }
    want = (int16_t)GT4_U16(obj + 0x18) > 0 ? 0 : OF(0x44);
    if (0 < OF(0x2c)) pass = 0;
    if ((int8_t)GT4_U8(car + 0x56a) == 1 && !OU8(0x6e) && pass == 1) pass = -1;
    if (pass > 0 && 0 < want) {
        int l2 = lane_left(lines, line), r2 = lane_right(lines, line), to = -1, t;
        t = lane_left(lines, l2);
        if (!(t < 0)) l2 = t;
        t = lane_right(lines, r2);
        if (!(t < 0)) r2 = t;
        if (OU8(0x1f) == 1 && l2 >= 0) to = l2;
        else if (OU8(0x1f) == 2 && r2 >= 0) to = r2;
        else if (OU8(0x1f) != 1 && l2 >= 0 && (int8_t)OU8(0x43) != 3) to = l2;
        else if (r2 >= 0 && (int8_t)OU8(0x43) != 4) to = r2;
        if (to >= 0) {
            gt4_ai_line_switch(obj, to, 0);
            GT4_U16(obj + 0x18) = 180;
            if (replay_mode(car)) driver_stats(car, 2, GT4_F32(b + 0x53c));
            want = 0;
        }
    }
    OF(0x34) = OF(0x34) * (ONE_F - want);
    if (!pass) return;
    if (((int8_t)OU8(0x48) + (int8_t)OU8(0x4c)) + ((int8_t)OU8(0x4a) + (int8_t)OU8(0x49)) + (int8_t)OU8(0x4b)) return;
    if (OU8(0x1c) == 6) return;
    gt4_ai_line_switch(obj, 6, 0);
}

/* func_00464C38: the driver kind (car+0x56B) the model takes over with;
   into the pits (6), and 5 unless coming from 4, start the 120-step
   countdown at obj+0x52. */
void gt4_car_driver_mode(uint32_t car, int mode)
{
    uint32_t b = car + 0x104, obj = car + 0x14;
    uint8_t old = GT4_U8(b + 0x467);
    GT4_U8(b + 0x467) = (uint8_t)mode;
    if (mode == 6 || (mode < 7 && mode == 5 && old != 4)) OU8(0x52) = 0x78;
}

/* func_00345218 */
static int middle_line_car(uint32_t car) { return GT4_U8(car + 0x30) == 7; }

/* func_00345520 (6, into the pits) and func_003454A8 (5): the model takes
   the car over, back to the middle line unless the race says otherwise
   (cond+0xCBE1); not in mode 4. func_00345598 (7): without the line. */
int gt4_car_take_over(uint32_t car, int mode)
{
    if ((int8_t)GT4_U8(car + CAR_56A) == 4) return 0;
    if (mode != 7 && !GT4_U8(GT4_U32(car + 4) + 0x10000u - 0x341fu) && middle_line_car(car))
        gt4_ai_line_switch(car + 0x14, 6, 1);
    gt4_car_driver_mode(car, mode);
    return 1;
}

/* func_003452F0: the model drives the car normally again (driver kind 1),
   back onto the middle line when it was waiting (2, 3, 4) or nothing is
   pressed. */
static int take_back(uint32_t car)
{
    uint32_t b = car + 0x104;
    uint8_t kind;
    int line = 0;
    if ((int8_t)GT4_U8(b + 0x466) == 4) return 0;
    kind = GT4_U8(b + 0x467);
    if (kind == 2 || kind == 3 || kind == 4) line = 1;
    if (!(GT4_U32(b + 0x464) & 0xffff0000u)) line = 1;
    if (line && middle_line_car(car)) gt4_ai_line_switch(car + 0x14, 6, 1);
    GT4_U8(b + 0x467) = 1;
    return 1;
}

/* func_00467148: the model's handicap from the race settings (per profile
   n, cond+0xF824/0xF828/0xF82C percentages, 100 unchanged) and, when
   D_00620DE9 is set in a race, a spread by grid slot. */
static void model_handicap(uint32_t obj, uint32_t n)
{
    uint32_t race = GT4_U32(obj + 0xe4), prof = profile(obj);
    int a = GT4_U8(race + 0xf824u + n), sp = GT4_U8(race + 0xf828u + n), sk = GT4_U8(race + 0xf82cu + n), lift = 0;
    if (!a) a = 100;
    if (!(a < 101)) a = 100;
    if (!sk) sk = 100;
    if (!sp) sp = 100;
    if (GT4_U8(0x00620de9u)) {
        uint32_t car = GT4_U32(obj + 0xe8);
        if ((int8_t)GT4_U8(car + CAR_56A) == 2) {
            int slot = GT4_U8(car + 0x544);
            if (!GT4_U8(AI_SETTINGS + 0xb)) sp = sp - (slot % 3) * 7;
            if (!GT4_U8(AI_SETTINGS + 0xa)) {
                sk = sk - ((slot * 4) & 4);
                lift = -(((slot * 7) % 5) * 2);
            }
            GT4_F32(prof + 0xc) = GT4_F32(prof + 0xc) * ((float)(((slot + 1) % 3) * 3 + 0x61) * K(0x3c23d70au));
        }
    }
    if (a != 100) GT4_F32(prof + 0x6c) = (float)a * K(0x3c23d70au);
    if (sp != 100) OF(0xbc) = OF(0xbc) * ((float)sp * K(0x3c23d70au));
    if (sk != 100) OF(0xb8) = OF(0xb8) * ((float)sk * K(0x3c23d70au));
    if (lift) GT4_F32(prof + 8) = GT4_F32(prof + 8) + (float)lift * K(0x3e8e38e3u);
}

/* func_00343D78: the car's race is over (or it pulls off): the model
   takes it (a player's car gets its handicap first), in the way `how`
   asks, or the race's setting (cond+0xCBDE) when that is 1; 4 picks by
   the car's flags. */
void gt4_car_finish_drive(uint32_t car, int how)
{
    uint32_t b = car + 0x104;
    uint8_t setting;
    if (how == 4) how = (GT4_U8(b + 0x4b1) >> 3) & 2;
    if (!GT4_U8(b + 0x4b2)) {
        GT4_U8(b + 0x4b2) = 1;
        if ((uint8_t)(GT4_U8(b + 0x467) - 2) < 3) return;
    }
    if (!(int8_t)GT4_U8(b + 0x466)) model_handicap(car + 0x14, GT4_U8(GT4_U32(car + CAR_PARAMS) + 0x31));   /* func_004673D0: nothing */
    setting = GT4_U8(GT4_U32(car + 4) + 0x10000u - 0x3422u);
    if (setting == 1) how = how ? how : 1;
    if (how == 1) gt4_car_take_over(car, 6);
    else if (how == 2) gt4_car_take_over(car, 7);
    else if (how == 3) gt4_car_take_over(car, 5);
    else take_back(car);
}

int gt4_car_take_back(uint32_t car) { return take_back(car); }
void gt4_ai_handicap(uint32_t obj, uint32_t n) { model_handicap(obj, n); }

/* func_00359A38 */
static void curve_set(uint32_t c, uint32_t n, uint32_t xs, uint32_t ys)
{
    GT4_U16(c) = (uint16_t)n;
    GT4_U32(c + 4) = xs;
    GT4_U32(c + 8) = ys;
    GT4_U8(c + 2) = 0;
    GT4_U8(c + 3) = 0;
}

/* func_00468EE8: the driver model's settings (AI_SETTINGS, 0x164 bytes)
   from their bytes (src, D_00623FB8): tenths, hundredths, and the curves
   at +0xAC, +0xB8, +0xDC, +0xE8, +0x158. */
void gt4_ai_settings_load(uint32_t dst, uint32_t src)
{
#define SB(o) ((float)(int32_t)GT4_U8(src + (o)))
    const float tenth = K(0x3dccccccu), hundredth = K(0x3c23d70au), hundred = K(0x42c80000u);
    uint32_t n;
    memset(gt4_ptr(dst), 0, 0x164);
    GT4_U8(dst) = GT4_U8(src);
    GT4_U8(dst + 1) = GT4_U8(src + 1);
    GT4_U8(dst + 2) = GT4_U8(src + 2);
    GT4_U8(dst + 5) = 0;
    GT4_U32(dst + 0x78) = 0;
    GT4_U32(dst + 0x7c) = 0;
    GT4_U32(dst + 0x84) = 0;
    GT4_U32(dst + 0x80) = 0;
    if (GT4_U8(src + 0x28)) {
        GT4_U8(dst + 5) = 1;
        GT4_F32(dst + 0x78) = SB(0x2b) * tenth;
        GT4_F32(dst + 0x7c) = SB(0x2c) * tenth;
    }
    if (GT4_U8(src + 0x29)) GT4_U8(dst + 5) = (uint8_t)(GT4_U8(dst + 5) | 2);
    if (GT4_U8(src + 0x2a)) GT4_U8(dst + 5) = (uint8_t)(GT4_U8(dst + 5) | 4);
    if (GT4_U8(dst + 5)) {
        GT4_F32(dst + 0x80) = SB(0x2d) * tenth;
        GT4_F32(dst + 0x84) = SB(0x2e) * tenth;
    }
    GT4_U8(dst + 8) = GT4_U8(src + 0x30);
    if (GT4_U8(src + 0x30)) {
        GT4_F32(dst + 0x88) = SB(0x31) * hundredth;
        GT4_F32(dst + 0x8c) = SB(0x32) * hundredth;
    }
    GT4_U8(dst + 9) = GT4_U8(src + 0x34);
    GT4_F32(dst + 0x90) = SB(0x35) * hundredth;
    GT4_U8(dst + 4) = GT4_U8(src + 0x24);
    GT4_F32(dst + 0x74) = SB(0x25) * tenth;
    GT4_F32(dst + 0x10) = SB(9);
    GT4_F32(dst + 0x14) = SB(0xa);
    GT4_F32(dst + 0x18) = SB(0xb) * tenth;
    GT4_F32(dst + 0x1c) = SB(0xc) * tenth;
    GT4_F32(dst + 0x20) = SB(0xd) * tenth;
    GT4_F32(dst + 0x24) = SB(0xe) * tenth;
    GT4_F32(dst + 0x2c) = SB(0x10) * tenth;
    GT4_F32(dst + 0x30) = SB(0x11) * tenth;
    GT4_F32(dst + 0x34) = SB(0x12) * tenth;
    GT4_F32(dst + 0x38) = SB(0x13) * tenth;
    GT4_F32(dst + 0x3c) = SB(0x14) * tenth;
    GT4_F32(dst + 0x40) = SB(0x15) * tenth;
    GT4_F32(dst + 0x44) = SB(0x16) * tenth;
    GT4_U8(dst + 0xa) = 0;
    GT4_F32(dst + 0x5c) = SB(0x1c) * hundredth;
    GT4_F32(dst + 0x94) = SB(0x39) / hundred;
    GT4_F32(dst + 0x98) = SB(0x3a) / hundred;
    GT4_F32(dst + 0x9c) = -SB(0x3b) / hundred;
    GT4_F32(dst + 0xa0) = -SB(0x3c) / hundred;
    GT4_F32(dst + 0xa4) = SB(0x3d) / hundred;
    GT4_F32(dst + 0xa8) = SB(0x3e) / hundred;
    curve_set(dst + 0xac, 2, dst + 0x94, dst + 0x9c);
    curve_set(dst + 0xb8, 2, dst + 0x94, dst + 0xa4);
    GT4_U8(dst + 0xb) = 0;
    GT4_F32(dst + 0xc4) = SB(0x40) / hundred;
    GT4_F32(dst + 0xc8) = SB(0x41) / hundred;
    GT4_F32(dst + 0xcc) = -SB(0x42) / hundred;
    GT4_F32(dst + 0xd0) = -SB(0x43) / hundred;
    GT4_F32(dst + 0xd4) = SB(0x44) / hundred;
    GT4_F32(dst + 0xd8) = SB(0x45) / hundred;
    curve_set(dst + 0xdc, 2, dst + 0xc4, dst + 0xcc);
    curve_set(dst + 0xe8, 2, dst + 0xc4, dst + 0xd4);
    GT4_F32(dst + 0xf4) = SB(0x46) / hundred;
    n = GT4_U8(src + 0x47);
    for (uint32_t k = 0; k < n; k++) {
        GT4_F32(dst + 0xf8 + k * 4) = SB(0x48 + k) / hundred;
        GT4_F32(dst + 0x128 + k * 4) = SB(0x54 + k) / hundred;
    }
    curve_set(dst + 0x158, n, dst + 0xf8, dst + 0x128);
#undef SB
}

/* func_00466C20: a driver profile from the race's row of profiles
   (func_00466B90: the course's own, +0xA0 by class n < 4, else
   D_00623DC0 + 0x40 for modes 2 and 3) for the model obj of params class
   n. */
void gt4_ai_profile_init(uint32_t obj, uint32_t n, uint32_t mode)
{
    uint32_t cond = GT4_U32(obj + 0xe4), row, prof = profile(obj), a3 = prof + 0x24, v;
    const float hundredth = K(0x3c23d70au), f4 = K(0x3e8e38e3u), f5 = K(0x3c8efa34u);
    uint8_t bits;
    float f3;
#define RB(o) ((float)(int32_t)GT4_U8(row + (o)))
    row = GT4_U32(GT4_U32(cond + 0xcbd8u) + (n < 4 ? n : 0) * 4 + 0xa0);
    if (!row) row = 0x00623dc0u + ((mode == 2 || mode == 3) ? 0x40u : 0);
    GT4_F32(prof + 0xc) = (float)(int32_t)(GT4_U8(row + 3) * 5) * hundredth;
    GT4_F32(prof + 0x1c) = RB(7);
    GT4_F32(prof + 0x20) = RB(8);
    GT4_F32(prof + 0x6c) = K(0x3f800000u);
    GT4_F32(prof + 0x18) = K(0x3f800000u) / ((float)(int32_t)(GT4_U8(row + 6) * 2) * f4);
    f3 = RB(0) * hundredth;
    GT4_F32(prof) = f3;
    GT4_F32(prof + 4) = RB(1) * hundredth;
    GT4_F32(prof + 8) = (float)((int32_t)GT4_U8(row + 2) - 100) * f4;
    GT4_F32(prof + 0x10) = RB(4) * hundredth;
    GT4_F32(prof + 0x14) = RB(5) * hundredth;
    GT4_F32(prof + 0x58) = RB(0xe) * hundredth;
    GT4_F32(prof + 0x5c) = RB(0xf) * f4;
    GT4_F32(prof + 0x60) = (float)(int32_t)(GT4_U8(row + 0x10) * 10) * f5;
    GT4_U32(prof + 0x68) = GT4_U8(row + 0x11) * 10u;
    GT4_F32(prof + 0x64) = (float)(int32_t)(GT4_U8(row + 0x12) ? GT4_U8(row + 0x12) : 0x50) * f4;
    GT4_F32(prof) = f3 * GT4_F32(0x00623f44u);
    bits = GT4_U8(row + 0x13);
    GT4_F32(a3) = RB(9) * hundredth;
    GT4_F32(a3 + 4) = RB(0xa) * hundredth;
    GT4_F32(a3 + 8) = RB(0x14) * hundredth;
    GT4_U8(a3 + 0xc) = bits & 1;
    GT4_U8(a3 + 0xd) = (bits >> 1) & 1;
    GT4_U8(a3 + 0xe) = (bits >> 2) & 1;
    GT4_U8(a3 + 0xf) = (bits >> 3) & 1;
    v = GT4_U32(a3 + 0xc);
    GT4_U8(a3 + 0x30) = (uint8_t)((v & 0xffff) && GT4_F32(a3) != 0);
    GT4_U8(a3 + 0x31) = (uint8_t)((v & 0xffff0000u) && GT4_F32(a3 + 8) != 0);
    GT4_F32(a3 + 0x10) = RB(0x15) * hundredth;
    GT4_F32(a3 + 0x14) = RB(0x16) * hundredth;
    GT4_U8(a3 + 0x1c) = (bits >> 4) & 1;
    GT4_U8(a3 + 0x1d) = (bits >> 5) & 1;
    GT4_U8(a3 + 0x1e) = (bits >> 6) & 1;
    GT4_U8(a3 + 0x1f) = bits >> 7;
    GT4_F32(a3 + 0x18) = RB(0x17) * hundredth;
    v = GT4_U32(a3 + 0x1c);
    GT4_U8(a3 + 0x32) = (uint8_t)((v & 0xffff) && GT4_F32(a3 + 0x10) != 0);
    GT4_U8(a3 + 0x33) = (uint8_t)((v & 0xffff0000u) && GT4_F32(a3 + 0x18) != 0);
    GT4_F32(a3 + 0x24) = RB(0xb) * K(0x3dccccccu);
    GT4_F32(a3 + 0x28) = RB(0xc) * hundredth;
    GT4_F32(a3 + 0x20) = RB(0xd) * K(0x3dccccccu);
    GT4_F32(a3 + 0x2c) = (GT4_U8(row + 0x18) == 100 || !GT4_U8(row + 0x18)) ? K(0x3f800000u) : RB(0x18) * hundredth;
    GT4_U8(prof + 0x78) = 0;
    GT4_U32(prof + 0x7c) = 0;
    GT4_U32(prof + 0x80) = 0;
    if (GT4_U8(row + 0x1d) && GT4_U8(row + 0x1c)) {
        GT4_U8(prof + 0x78) = 1;
        GT4_F32(prof + 0x7c) = RB(0x1c) * f5;
        GT4_F32(prof + 0x80) = RB(0x1d) * f4;
    }
    {
        uint8_t k = GT4_U8(0x00623fd8u + n);
        if (k && k != 100) GT4_F32(prof + 0x80) = GT4_F32(prof + 0x80) * ((float)(int32_t)k / K(0x42c80000u));
    }
    for (uint32_t k = 0; k < 3; k++) {
        uint8_t c = GT4_U8(row + 0x20 + k);
        GT4_F32(prof + 0xa8 + k * 4) = K(0x3f800000u);
        if (GT4_U8(row + 0x1b) == 0x20 && c && c != 100) GT4_F32(prof + 0xa8 + k * 4) = (float)(int32_t)c / K(0x42c80000u);
    }
    GT4_F32(obj + 0xc0) = K(0x3f800000u);
    GT4_F32(obj + 0xb8) = K(0x3f800000u);
    GT4_F32(obj + 0xbc) = K(0x3f800000u);
#undef RB
}

/* func_00468BC8: a + b/100... the factor a/100 + (b/100)(x/1000). */
static float style_factor(uint8_t a, int8_t b, uint32_t x)
{
    float fx = (int32_t)x < 0 ? (float)(int32_t)((x >> 1) | (x & 1)) * 2.0f : (float)(int32_t)x;
    return (float)(int32_t)a / K(0x42c80000u) + ((float)(int32_t)b / K(0x42c80000u)) * (fx / K(0x447a0000u));
}

/* func_00468C50: when the race asks (D_00623E48), the model's speed
   factors (obj+0xB8..0xC0) from the player's driving style recorded in
   car+0x8C (three rows of 8 u16 by level, and the peaks at +0x30/+0x38),
   around the level of the car (func_00468418: params+0xFE0). */
static void player_style(uint32_t car)
{
    uint32_t t = car + 0x8c, s1 = 0, s4 = 0, s5 = 0, s6 = 0, s7 = 0, fp = 0, s2, s3 = 0xff, s0;
    int32_t level, lo, hi, side;
    float x;
    if (!GT4_U8(0x00623e48u)) return;
    x = (GT4_F32(GT4_U32(car + CAR_PARAMS) + 0xfe0) - K(0x3eb33333u)) / K(0x3f0cccccu);   /* func_004683B8 */
    if (x < 0) x = 0;
    else if (K(0x3f800000u) < x) x = K(0x3f800000u);
    level = (int32_t)(x * K(0x41000000u));
    if (!(level < 8)) level = 7;
    lo = level < 5 ? 0 : level - 4;                           /* func_00467BE8 */
    hi = level < 3 ? level + 5 : 8;
    for (int32_t k = lo; k < hi; k++) {
        uint32_t w = GT4_U8(0x00623e40u + (uint32_t)k), a = GT4_U16(t + (uint32_t)k * 2), b = GT4_U16(t + 0x10 + (uint32_t)k * 2), c = GT4_U16(t + 0x20 + (uint32_t)k * 2);
        s6 += (a <= 0x400 ? a : 0x400) * w;
        s4 += (b <= 0x400 ? b : 0x400) * w;
        s5 += (c <= 0x100 ? c : 0x100) * w;
        fp += w << 8;
        s7 += w << 10;
        s1 += w << 10;
    }
    s0 = (uint32_t)(level / 2);
    side = GT4_U8(GT4_U32(car + 4) + 0x10000u - 0x3419u) ? 1 : -1;   /* func_00468A40 */
    s2 = 0xff;
    if (side >= 0) s2 = GT4_U16(t + 0x30 + s0 * 2) >> 8;
    if (side > 0) s3 = GT4_U16(t + 0x38 + s0 * 2) >> 8;
    s6 = s6 * 700u / s1;
    if (!(s6 < 701)) s6 = 700;
    s2 = s2 * 300u / 255u;
    if (!(s2 < 301)) s2 = 300;
    s3 = s3 * 300u / 255u;
    if (!(s3 < 301)) s3 = 300;
    s4 = s4 * 700u / s7;
    if (!(s4 < 701)) s4 = 700;
    s5 = s5 * 700u / fp;
    if (!(s5 < 701)) s5 = 700;
    GT4_F32(car + 0x14 + 0xb8) = style_factor(GT4_U8(0x00623e4au), (int8_t)GT4_U8(0x00623e4bu), s6 + s2);
    GT4_F32(car + 0x14 + 0xbc) = style_factor(GT4_U8(0x00623e4cu), (int8_t)GT4_U8(0x00623e4du), s4 + s2);
    GT4_F32(car + 0x14 + 0xc0) = style_factor(GT4_U8(0x00623e4eu), (int8_t)GT4_U8(0x00623e4fu), s5 + s3);
}

/* func_003F1E60: after the cars: each one a player drives (driver kind
   < 2, the pad not the vibration's) takes the race's driving style record
   (race+0x20, 0x40 bytes) and the model's factors from it. */
void gt4_race_player_styles(uint32_t cond)
{
    uint32_t race = GT4_U32(cond);
    for (uint32_t i = 0; (int32_t)i < (int32_t)GT4_U32(cond + 0xf8a0u); i++) {
        uint32_t car = cond + 0x6698u + i * CAR_SIZE, b = car + 0x104;
        if (!(GT4_U8(b + 0x466) < 2) || GT4_U8(b + 0x441)) continue;
        memmove(gt4_ptr(car + 0x8c), gt4_ptr(race + 0x20), 0x40);
        player_style(car);
    }
}
