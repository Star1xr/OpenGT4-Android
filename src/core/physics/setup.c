/* The car's setup: its physics parameters (car+0x10, read by the step)
   from the record the game builds out of the car's SpecDB rows and its
   tuning (func_003F3B68). Checked against the original with
   tests/physics/setup_run.c. */
#include "car.h"
#include "engine.h"

#include <math.h>
#include <string.h>

#define K(bits) gt4_f32(bits)
#define GT4_S16(a) ((int32_t)(int16_t)GT4_U16(a))
#define GT4_S8(a)  ((int32_t)(int8_t)GT4_U8(a))

#define GAME_SETTINGS 0x00620cd8u   /* D_00620CD8: game options and handicaps */
#define AIR_DENSITY   0x00622308u   /* D_00622308 (float) */
#define SPECIAL_TABLES 0x006205c8u  /* D_006205C8: 3 x 0x78, for the layouts below */

/* func_0035A058: layouts with their own geometry table. */
static int special_layout(uint32_t rec)
{
    return GT4_U8(rec + 2) == 1 || GT4_U8(rec + 1) == 6;
}

/* func_00359F38 / func_00359EF8 */
static uint32_t special_table(uint32_t p)
{
    uint32_t i = GT4_U8(p + 0x33);
    if (i >= 3) i = 0;
    return SPECIAL_TABLES + i * 0x78u;
}

/* func_00466B28 */
static int tyre_class(uint32_t a, uint32_t b)
{
    if (a == 1) return 1;
    if (a != 2) return 0;
    if (b == 3) return 3;
    if (b == 0) return 1;
    if (b < 4) return 2;
    return b == 5 ? 0 : 2;
}

/* func_00359A38 (with func_00359A08/18/28) */
static void curve_init(uint32_t c, uint32_t n, uint32_t xs, uint32_t ys)
{
    GT4_U16(c) = (uint16_t)n;
    GT4_U32(c + 4) = xs;
    GT4_U32(c + 8) = ys;
    GT4_U8(c + 2) = 0;
    GT4_U8(c + 3) = 0;
}

/* func_00360468: a curve against speed (km/h) of angles (units of 0.2 degree). */
static void speed_curve(uint32_t c, uint32_t xs, uint32_t ys, uint32_t n, uint32_t kmh, uint32_t deg5)
{
    for (uint32_t j = 0; j < n; j++) {
        GT4_F32(xs + j * 4) = (float)(int32_t)GT4_U8(kmh + j) * K(0x3e8e38e3u);
        GT4_F32(ys + j * 4) = (float)(int32_t)GT4_U8(deg5 + j) * K(0x3e4ccccc) * K(0x3c8efa34u);
    }
    curve_init(c, n, xs, ys);
}

/* func_00360A78: the assist settings as fractions (D_00620CB0..D_00620CD0). */
static void assist_settings(void)
{
    uint32_t cfg = GAME_SETTINGS, on = GT4_U8(cfg + 0x102) != 0;
    GT4_U32(0x00620cb0u) = on;
    if (!on) return;
    for (int t = 0; t < 2; t++) {
        GT4_F32(0x00620cb8u + t * 4) = (float)(int32_t)GT4_U8(cfg + 0x105 + t) * K(0x3c23d70au);
        GT4_F32(0x00620cc8u + t * 4) = (float)(int32_t)GT4_U8(cfg + 0x109 + t) * K(0x3c23d70au);
        GT4_F32(0x00620cc0u + t * 4) = (float)(int32_t)GT4_U8(cfg + 0x107 + t) / 100.0f;
        GT4_F32(0x00620cd0u + t * 4) = (float)(int32_t)GT4_U8(cfg + 0x10b + t) / 100.0f;
    }
}

/* func_00361008: slip curves of a tyre from its record (src): points on
   the negative side (src+9, read backwards) and the positive side (src+0xA),
   the zero point shared; the peaks go to +0x164..+0x16C. */
static void tyre_slip_curves(uint32_t t, uint32_t src)
{
    uint32_t n, m;
    float best = 0;
    GT4_U8(src + 0x20) = 0;
    GT4_U8(src + 0x40) = GT4_U8(src + 0x48);
    GT4_U8(src + 0x30) = 0;
    GT4_U8(src + 0x28) = 0;
    GT4_U8(src + 0x38) = 0;
    n = GT4_U8(src + 9);
    m = GT4_U8(src + 0xa);
    GT4_F32(t + 0x164) = 0;
    for (uint32_t j = 0; j < n; j++) {
        uint32_t x = t + 0x100 + (n - j) * 4;
        float s = -(float)(int32_t)GT4_U8(src + 0x20 + j) / 255.0f, f;
        GT4_F32(x) = s;
        f = (float)(int32_t)GT4_U8(src + 0x30 + j) / 200.0f;
        GT4_F32(x + 0x30) = f;
        if (best < f) {
            GT4_F32(t + 0x164) = s;
            best = f;
        }
        GT4_F32(t + 0x178 + (n - j) * 4) = (float)(int32_t)GT4_U8(src + 0x40 + j) / 200.0f;
    }
    best = 0;
    GT4_F32(t + 0x168) = 0;
    GT4_F32(t + 0x16c) = 1.0f;
    for (uint32_t j = 0; j < m; j++) {
        uint32_t x = t + 0x100 + (n + j) * 4;
        float s = (float)(int32_t)GT4_U8(src + 0x28 + j) / 255.0f, f;
        GT4_F32(x) = s;
        f = (float)(int32_t)GT4_U8(src + 0x38 + j) / 200.0f;
        GT4_F32(x + 0x30) = f;
        if (best < f) {
            best = f;
            GT4_F32(t + 0x168) = s;
            GT4_F32(t + 0x16c) = best;
        }
        GT4_F32(t + 0x178 + (n + j) * 4) = (float)(int32_t)GT4_U8(src + 0x48 + j) / 200.0f;
    }
    curve_init(t + 0xf8, n + m - 1, t + 0x104, t + 0x134);
    curve_init(t + 0x170, n + m - 1, t + 0x104, t + 0x17c);
}

/* func_00361220: side force against slip angle, and the other component. */
static void tyre_side_curves(uint32_t t, uint32_t src)
{
    uint32_t n = GT4_U8(src + 8), m;
    float range = (float)(int32_t)(GT4_U8(src + 2) ? GT4_U8(src + 2) : 90) * K(0x3c8efa34u);
    GT4_U8(src + 0x10) = 0;
    GT4_U8(src + 0x18) = 0;
    if (K(0x3fc90fdau) < range) range = K(0x3fc90fdau);
    for (uint32_t j = 0; j < n; j++) {
        GT4_F32(t + 0x78 + j * 4) = (float)(int32_t)GT4_U8(src + 0x10 + j) * range / 255.0f;
        GT4_F32(t + 0x98 + j * 4) = (float)(int32_t)GT4_U8(src + 0x18 + j) / 200.0f;
    }
    curve_init(t + TYRE_SIDE_CURVE, n, t + 0x78, t + 0x98);
    GT4_U8(src + 0x50) = 0;
    m = GT4_U8(src + 0xc);
    GT4_U8(src + 0x56) = 0;
    for (uint32_t j = 0; j < m; j++) {
        GT4_F32(t + 0xc8 + j * 4) = (float)(int32_t)GT4_U8(src + 0x50 + j) * K(0x3fc90fd9u) / 255.0f;
        GT4_F32(t + 0xe0 + j * 4) = (float)(int32_t)GT4_U8(src + 0x56 + j) / 200.0f;
    }
    curve_init(t + TYRE_6C_CURVE, m, t + 0xc8, t + 0xe0);
}

/* func_00353800: the slip angle of the peak side force. */
static float tyre_peak_slip(uint32_t t)
{
    const gt4_curve *c = gt4_ptr(t + TYRE_SIDE_CURVE);
    if (GT4_U8(t + 0x1d9)) {
        float best = 0, at = 0;
        for (int i = 0; i < c->n; i++) {
            float y = GT4_F32(c->y + (uint32_t)i * 4);
            if (best < y) {
                best = y;
                at = GT4_F32(c->x + (uint32_t)i * 4);
            }
        }
        return at;
    } else {
        float best = 0;
        int deg = 90;
        for (int i = 0; i < 46; i++) {
            float side, f = gt4_curve_eval(c, gt4_slip_fold((float)i * K(0x3c8efa34u) - 0.0f, &side)) * side;
            if (best < f) {
                best = f;
                deg = i;
            }
        }
        return (float)deg * K(0x3c8efa34u);
    }
}

#define TYRE_GRIP_SCALE 0x00620d97u   /* D_00620CE7 + 0xB0: [3] percent */

/* func_003613D8: grip per surface, load sensitivity, and the tyre's base mu. */
static void tyre_grip(uint32_t t, uint32_t src, float scale)
{
    float g = (float)(int32_t)(GT4_U8(src) ? GT4_U8(src) : 100) * K(0x3c23d70au), r;
    uint32_t m;
    for (int i = 0; i < 24; i++) {
        uint32_t v = GT4_U8(src + 0x64 + (uint32_t)i);
        GT4_F32(t + TYRE_MU + (uint32_t)i * 4) = v ? (float)(int32_t)v * K(0x3c23d70au) * g : g;
    }
    r = scale / g;
    GT4_F32(t + 0x68) = r;
    GT4_F32(t + 4) = (float)(int32_t)GT4_U8(src + 1) * K(0x3c23d70au);
    if (1.0f < r) GT4_F32(t + 0x68) = 1.0f;
    m = GT4_U8(src + 0xb);
    GT4_U8(src + 0x5c) = 0;
    for (uint32_t j = 0; j < m; j++) {
        GT4_F32(t + 0x1b8 + j * 4) = (float)(int32_t)GT4_U8(src + 0x5c + j) * 20.0f * K(0x411ccccc);
        GT4_F32(t + 0x1c8 + j * 4) = (float)(int32_t)GT4_U8(src + 0x60 + j) / 200.0f;
    }
    curve_init(t + TYRE_LOAD_CURVE, m, t + 0x1b8, t + 0x1c8);
    GT4_F32(t) = gt4_curve_eval(gt4_ptr(t + TYRE_COEF_B), 0) * K(0x411ccccc);
}

/* func_00361570: the tyre blocks, per axle and surface kind (rec+0x1F0:
   the kind of each of the 24 surfaces). which: 1 front only, 2 rear only. */
void gt4_tyre_blocks(uint32_t p, uint32_t rec, uint32_t which)
{
    int8_t used[6] = { 0 };
    uint32_t first = 0, end = 6;
    for (uint32_t s = 0; s < 24; s++) {
        uint32_t v = GT4_U8(rec + 0x1f0 + s);
        if (v >= 3) v = 0;
        GT4_U8(p + PARAMS_TYRE_INDEX + s) = (uint8_t)v;
        GT4_U8(p + PARAMS_TYRE_INDEX + 0x18 + s) = (uint8_t)(v + 3);
        used[v]++;
        used[v + 3]++;
    }
    if (which == 1) end = 3;
    else if (which == 2) first = 3;
    for (uint32_t k = first; k < end; k++) {
        uint32_t t = p + PARAMS_TYRES + k * TYRE_SIZE, i, src;
        float peak;
        if (!used[k]) continue;
        i = GT4_U8(rec + 0x1e0) ? k : k % 3;
        src = rec + 0x210 + i * 0xa8;
        GT4_U8(t + 0x1d8) = GT4_U8(src + 3);
        GT4_U8(t + 0x1d9) = GT4_U8(src + 4);
        GT4_U8(t + 0x1da) = GT4_U8(src + 5);
        tyre_slip_curves(t, src);
        tyre_side_curves(t, src);
        peak = tyre_peak_slip(t);
        GT4_F32(t + 0xb8) = K(0x3f490fd9u) < peak ? K(0x3f490fd9u) : peak;
        tyre_grip(t, src, (float)(int32_t)GT4_U8(TYRE_GRIP_SCALE + i % 3) * K(0x3c23d70au));
    }
}

static float ps2_sqrt(float x) { return x == 0 ? x : sqrtf(fabsf(x)); }

/* func_003608E8: a damper's two slopes and knees (bump or rebound). */
static void damper_points(float *k0, float *f0, float *k1, float *f1, uint32_t src, float scale)
{
    *k0 = (float)(int32_t)GT4_U8(src + 0) * K(0x3c23d70au);
    *f0 = (float)(int32_t)GT4_U8(src + 1) * 5.0f * K(0x411ccccc);
    *k1 = (float)(int32_t)GT4_U8(src + 2) * K(0x3c23d70au);
    *f1 = (float)(int32_t)GT4_U8(src + 3) * 5.0f * K(0x411ccccc);
    *k0 = *k0 * scale;
    *k1 = *k1 * scale;
}

/* func_00360980: the quadratic through the two damper points (a x^2 + b x
   with c the speed of the knee), zero when they do not make one. */
static void damper_fit(uint32_t a, uint32_t b, uint32_t c, float x0, float y0, float x1, float y1)
{
    float d, e, q, s;
    GT4_F32(c) = 0;
    GT4_F32(b) = 0;
    GT4_F32(a) = 0;
    if (y0 == 0 || y1 == 0 || x0 == 0 || x1 == 0) return;
    if (x1 <= x0 || y1 <= y0) return;
    d = y0 * x1 - y1 * x0;
    if (d <= 0) return;
    e = y0 * y0 * x1 - y1 * y1 * x0;
    if (0 < e) return;
    q = e / (d + d);
    s = y0 * y1 * (y1 - y0) / d;
    GT4_F32(c) = ps2_sqrt(s);
    GT4_F32(a) = -(q * q) / s;
    GT4_F32(b) = -GT4_F32(c) * ps2_sqrt(-GT4_F32(a));
}

/* func_00360B60: an axle's spring, dampers and bump stops from its record
   (src); ratio: the spring's motion ratio in percent. */
static void spring_params(uint32_t a, uint32_t src, uint32_t ratio)
{
    float scale, k0, f0, k1, f1;
    GT4_F32(a + 0x14) = (float)(int32_t)GT4_U8(src) * K(0x4474fffeu) * ((float)(int32_t)ratio * K(0x3c23d70au));
    scale = 100.0f / (float)(int32_t)ratio;                                   /* func_003608C0 */
    for (int i = 0; i < 2; i++) {
        uint32_t o = (uint32_t)i * 0xc;
        damper_points(&k0, &f0, &k1, &f1, src + 4 + (uint32_t)i * 4, scale);
        if (GT4_U8(GAME_SETTINGS + 0x101)) {
            GT4_F32(a + 0x18 + o) = k0;
            GT4_F32(a + 0x1c + o) = f0;
            GT4_F32(a + 0x20 + o) = k1;
            GT4_F32(a + 0x48 + (uint32_t)i * 4) = f1;
        } else {
            damper_fit(a + 0x18 + o, a + 0x1c + o, a + 0x20 + o, k0, f0, k1, f1);
        }
    }
    GT4_F32(a + 0x30) = (float)(int32_t)GT4_U8(src + 1) * 10.0f * K(0x411ccccc);
    GT4_F32(a + 0x34) = (float)(int32_t)GT4_U8(src + 2) * 10000.0f * K(0x411ccccc);
    GT4_F32(a + 0x38) = (float)(int32_t)GT4_U8(src + 3) * 0.25f;
    GT4_F32(a + 0x3c) = (float)(int32_t)GT4_U8(src + 3) * 0.25f * K(0x411ccccc);
    GT4_F32(a + 0x40) = (float)(int32_t)GT4_U8(src + 0xd) * 10.0f * K(0x411ccccc);
}

/* func_00361798: each tyre's mu scaled by its load curve at the static
   load (func_003573A8: the load over the axle's reference); +0x104A says
   whether the rear tyres differ from the front ones. */
void gt4_tyre_static_load(uint32_t p, uint32_t which)
{
    uint32_t first = 0, end = 6;
    if (which == 1) end = 3;
    else if (which == 2) first = 3;
    for (uint32_t k = first; k < end; k++) {
        uint32_t t = p + PARAMS_TYRES + k * TYRE_SIZE, axle = k / 3;
        uint32_t lay = GT4_LAYOUTS + GT4_U8(p + PARAMS_LAYOUT) * 84u;
        uint32_t wheel = GT4_U8(lay + 0xc + axle), wa = 0;
        float load;
        if (GT4_F32(t + TYRE_MU_SCALE) == 0) continue;
        load = GT4_F32(p + 0x1040 + axle * 4) * 0.5f + GT4_F32(p + 0x344 + axle * 0x58);
        if ((int32_t)wheel < (int32_t)GT4_U8(lay + LAYOUT_WHEELS)) wa = GT4_U8(lay + LAYOUT_AXLE + wheel);
        load = load / GT4_F32(p + PARAMS_LOAD_REF + wa * 4);
        GT4_F32(t + TYRE_MU_SCALE) = GT4_F32(t + TYRE_MU_SCALE) * gt4_curve_eval(gt4_ptr(t + TYRE_LOAD_CURVE), load);
    }
    GT4_U8(p + 0x104a) = GT4_F32(p + PARAMS_TYRES + TYRE_MU) != GT4_F32(p + PARAMS_TYRES + 3 * TYRE_SIZE + TYRE_MU);
}

/* func_00360DC8: an axle's static deflection and travel limits. */
static void axle_travel(uint32_t p, uint32_t a, uint32_t rec, uint32_t i)
{
    float k = GT4_F32(a + 0x14), sag, h, lim, bump, reb;
    uint32_t o = i * 4;
    int16_t stop;
    if (k != 0) {
        uint32_t lay = GT4_LAYOUTS + GT4_U8(p + PARAMS_LAYOUT) * 84u;
        GT4_F32(a + 0x10) = -GT4_F32(p + 0x1040 + o) * (1.0f / (float)(int32_t)GT4_U8(lay + 4 + i)) / k;
    }
    sag = GT4_F32(a + 0x10);
    GT4_F32(a + 0xc) = GT4_F32(p + PARAMS_2C4 + o) - GT4_F32(p + PARAMS_ROLL_HEIGHT + o) + sag;
    lim = (float)(int32_t)GT4_U8(rec + 0x92 + i) * K(0x3a83126eu);
    h = GT4_F32(p + PARAMS_ROLL_HEIGHT + o);
    reb = sag - h;
    if (h < lim) lim = h;
    GT4_F32(a + 4) = reb + lim;
    stop = (int16_t)GT4_U16(rec + 0x66 + i * 24);
    bump = (float)stop * K(0x3a83126eu);
    if (GT4_F32(p + PARAMS_ROLL_HEIGHT + o) < bump) bump = GT4_F32(p + PARAMS_ROLL_HEIGHT + o);
    if (!stop || bump <= lim) GT4_F32(a) = GT4_F32(a + 4);
    else GT4_F32(a) = reb + bump;
    {
        float top = GT4_F32(a + 0x3c) / k, v = sag + (float)(int32_t)GT4_U8(rec + 0x94 + i) * K(0x3a83126eu);
        GT4_F32(a + 8) = v;
        if (top < v) GT4_F32(a + 8) = top;
    }
}

#define RPM_TO_RAD    K(0x3dd6774eu)   /* 2 pi / 60 */
#define ROLLING_CURVE 0x00620938u      /* D_00620938: gt4_curve against speed, then +0x4C a factor */
#define CLASS_POWER   0x00622320u      /* D_00622320: [class][turbo] low, high (units of 10 PS) */

/* func_0034F3F8's output (0x100 bytes, as the original's stack buffer):
   +0 peak power (PS), +4 peak torque (kgf m), +8 s16 rpm of the power peak,
   +0xA of the torque peak, +0xC u16 redline, +0xE s16 point count, +0x10
   s16 rpm of each point (to 100), +0x40 power and +0xA0 torque at them. */
typedef struct { uint8_t b[0x100]; } engine_summary;
static int16_t sum_s16(const engine_summary *s, uint32_t o) { int16_t v; memcpy(&v, s->b + o, 2); return v; }
static float sum_f32(const engine_summary *s, uint32_t o) { float v; memcpy(&v, s->b + o, 4); return v; }
static void sum_set16(engine_summary *s, uint32_t o, int v) { int16_t x = (int16_t)v; memcpy(s->b + o, &x, 2); }
static void sum_setf(engine_summary *s, uint32_t o, float v) { memcpy(s->b + o, &v, 4); }

static float engine_torque(uint32_t e, float speed, uint32_t wear)
{
    return gt4_engine_torque(gt4_ptr(e), speed, wear ? gt4_ptr(wear) : NULL);
}

/* func_0034F3F8 */
static void engine_summarize(uint32_t e, engine_summary *s, uint32_t wear)
{
    float best_p = 0, best_t = 0;
    int count = 0, p_rpm = 0, t_rpm = 0, rpm, end;
    uint32_t at = 0x10, pw = 0x40;
    for (int i = 0; i < (int16_t)GT4_U16(e); i++) {
        int r = (int)(GT4_F32(e + 0xc + (uint32_t)i * 4) * 60.0f / K(0x40c90fdau));
        r = (r + 50) / 100 * 100;
        if (count <= 0 || sum_s16(s, 0xe + (uint32_t)count * 2) < r) {
            sum_set16(s, 0x10 + (uint32_t)count * 2, r);
            count++;
        }
    }
    sum_set16(s, 0xe, count);
    rpm = sum_s16(s, 0x10);
    sum_set16(s, 0xc, (int)GT4_U16(e + ENGINE_REDLINE));
    end = GT4_U16(e + ENGINE_REDLINE);
    if (end < sum_s16(s, 0xe + (uint32_t)count * 2)) end = sum_s16(s, 0xe + (uint32_t)count * 2);
    while (!(end < rpm)) {
        float r = (float)rpm, speed = r * RPM_TO_RAD, t, p;
        int more;
        t = gt4_engine_power_cap(e, engine_torque(e, speed, wear), speed);
        t = t / K(0x411ccccc);
        p = t * r;
        more = best_t < t;
        p = p / K(0x44330cccu);
        if (sum_s16(s, at) == rpm) {
            sum_setf(s, pw, p);
            at += 2;
            sum_setf(s, pw + 0x60, t);
            pw += 4;
        }
        if ((int)(uint16_t)sum_s16(s, 0xc) < rpm) { rpm += 100; continue; }
        if (more) { best_t = t; t_rpm = rpm; }
        if (best_p < p) { best_p = p; p_rpm = rpm; }
        rpm += 100;
    }
    sum_set16(s, 0xa, t_rpm);
    sum_setf(s, 0, best_p);
    sum_set16(s, 8, p_rpm);
    sum_setf(s, 4, best_t);
}

/* func_003F1FB0: a slipping converter (drive kind byte 0x12 == 1) caps the
   engine speed at its stall speed; the ratio is kept above the minimum. */
void gt4_converter(uint32_t d, float *speed, float *ratio)
{
    float stall;
    if (GT4_U8(d + 0x12) != 1) return;
    stall = (float)(int32_t)GT4_U16(d + 0x30) * RPM_TO_RAD;
    if (*speed < stall) { *ratio = 1.0f; return; }
    *ratio = stall / *speed;
    if (*ratio < GT4_F32(d + 0x40 + GT4_U8(d + 2) * 4u)) *ratio = GT4_F32(d + 0x40 + GT4_U8(d + 2) * 4u);
    *speed = *ratio * *speed;
}

/* func_00360520: top speed, by stepping the speed up and seeing where the
   drive force still beats the resistances (p+0xFD8, m/s). */
static void top_speed(uint32_t p, uint32_t wear)
{
    uint32_t d = p + PARAMS_DRIVE, e = p + PARAMS_ENGINE;
    int end, step, kmh = 0, gear = 1;
    if (GT4_U8(d + 0x12) == 3) {                                  /* func_00368398 */
        GT4_F32(p + 0xfd8) = K(0x4247ffffu);
        return;
    }
    end = (int)(GT4_F32(p + 0xfd8) * K(0x40666666u));
    step = end / 500 + 1;
    if (end <= 0) return;
    do {
        float v = (float)kmh * K(0x3e8e38e3u), v2, ratio = 1.0f, speed, drive, resist;
        uint32_t g = p + 0x29c + (uint32_t)gear * 4;
        int rpm;
        kmh += step;
        speed = v * GT4_F32(g);
        v2 = v * v;
        gt4_converter(d, &speed, &ratio);
        rpm = (int)(speed * 60.0f / K(0x40c90fdau));
        drive = gt4_engine_power_cap(e, engine_torque(e, speed, wear), speed) * GT4_F32(g);
        if (GT4_U8(d + 0x12) == 1) drive = drive * ratio;
        resist = gt4_curve_eval(gt4_ptr(ROLLING_CURVE), v) * GT4_F32(ROLLING_CURVE + 0x4c)
                     * (GT4_F32(p + 0x103c) + GT4_F32(p + PARAMS_DOWNFORCE) * v2 + GT4_F32(p + PARAMS_DOWNFORCE + 4) * v2)
                 + GT4_F32(p + PARAMS_DRAG) * v2;
        if (resist < drive) GT4_F32(p + 0xfd8) = v;
        if (GT4_U8(d + 0x12) != 1 && gear < GT4_U8(d + 2)) gear += GT4_U16(d + 0x2e) < rpm;
    } while (kmh < end);
}

/* func_003F3278: where the power sits in its class (p+0x10DC, 0.25..0.75). */
static void power_class(uint32_t p, uint32_t cls, float power)
{
    uint32_t e;
    float lo, hi;
    if (cls >= 4) cls = 3;
    e = CLASS_POWER + (cls * 2 + (0 < GT4_F32(p + 0x230))) * 2;
    lo = (float)(int32_t)(GT4_U8(e) * 10);
    hi = (float)(int32_t)(GT4_U8(e + 1) * 10);
    if (power < lo) GT4_F32(p + 0x10dc) = 0;
    else if (hi <= power) GT4_F32(p + 0x10dc) = 0.75f;
    else GT4_F32(p + 0x10dc) = (power - lo) * 0.5f / (hi - lo) + 0.25f;
}

/* func_00355978: the hybrid's constants (D_008438C0..D_00843920) from
   the setting bytes at D_0062045A..D_00620474. */
static void hybrid_constants(void)
{
#define HB(o) ((float)(int32_t)GT4_U8(0x00620400u + (o)))
#define HF(o) GT4_F32(0x00843800u + (o))
    const float hundred = K(0x42c80000u), per = K(0x4474fffeu);
    float base = (HB(0x5a) + HB(0x5a)) + HB(0x5b), pw = HB(0x61) * base;
    HF(0xc0) = HB(0x5a);
    HF(0xc4) = HB(0x5b);
    HF(0xc8) = (float)(int32_t)(GT4_U8(0x0062045cu) * 10);
    HF(0xcc) = (float)(int32_t)(GT4_U8(0x0062045du) * 10);
    HF(0xd0) = (float)(int32_t)(GT4_U8(0x0062045eu) * 10);
    HF(0xd4) = (float)(int32_t)(GT4_U8(0x0062045fu) * 10);
    HF(0xd8) = HB(0x60) * K(0x3dccccccu);
    HF(0xdc) = pw;
    HF(0xe0) = (float)(int32_t)(GT4_U8(0x00620462u) * 5);
    HF(0xe4) = (float)(int32_t)(GT4_U8(0x00620463u) * 5);
    HF(0xe8) = HB(0x64) * hundred;
    HF(0xec) = HB(0x65) / K(0x47c35000u);
    HF(0xf0) = HB(0x66) / hundred;
    HF(0xf4) = HB(0x67);
    HF(0xf8) = (pw * HB(0x68)) / hundred;
    HF(0xfc) = (pw * HB(0x69)) / hundred;
    HF(0x100) = HB(0x6a) / HB(0x5a);
    HF(0x104) = HB(0x6e) / hundred;
    HF(0x108) = HB(0x6f) / per;
    HF(0x10c) = HB(0x70) / per;
    HF(0x110) = HB(0x71) / hundred;
    HF(0x114) = HB(0x72) / per;
    HF(0x118) = HB(0x73) / per;
    HF(0x11c) = HB(0x74) / hundred;
    HF(0x120) = K(0x45bb8000u) / base;
#undef HB
#undef HF
}

/* func_003F86E8: a 0..1 rating of the car from power, torque, grip, mass
   and aerodynamics. */
static float car_rating(uint32_t p, const engine_summary *s)
{
    uint32_t d = p + PARAMS_DRIVE;
    float power = sum_f32(s, 0), extra, aero, grip, m, sm, r;
    if (GT4_U8(d) == 7) {                                 /* func_00356E08: the hybrid's motor */
        hybrid_constants();
        extra = ((GT4_F32(0x008438c0u) + GT4_F32(0x008438c0u)) + GT4_F32(0x008438c4u)) / K(0x3f3c49bau);
    }
    else extra = GT4_U8(d + 0x13) == 4 ? K(0x41598a02u) : 0;
    power = power + extra;
    aero = (GT4_F32(p + PARAMS_DOWNFORCE) + GT4_F32(p + PARAMS_DOWNFORCE + 4)) * 0.5f - GT4_F32(p + PARAMS_DRAG) + 1.0f;
    grip = (GT4_F32(p + PARAMS_AXLE_GRIP) + GT4_F32(p + PARAMS_AXLE_GRIP + 4)) * 0.5f;
    if (GT4_U8(p + PARAMS_AERO_KIND) == 7) grip = grip * (GT4_F32(p + PARAMS_AERO_EXTRA) / GT4_F32(p + 0x103c) + 1.0f);
    m = GT4_F32(p + PARAMS_MASS);
    sm = ps2_sqrt(m);
    r = GT4_F32(p + PARAMS_TYRES + TYRE_MU);
    r = r * r * power * power * ps2_sqrt(sum_f32(s, 4)) / (m * sm) * aero * grip;
    r = ps2_sqrt(ps2_sqrt(ps2_sqrt(r))) / 2.5f;
    if (1.0f < r) r = 1.0f;
    if (r < 0) r = K(0x3c23d70au);
    return r;
}

/* func_00360780: what follows from the rest: top speed, brake balance,
   the engine summary, the converter's stall speed, the class and rating. */
static void finish_params(uint32_t p, uint32_t rec, uint32_t wear)
{
    uint32_t d = p + PARAMS_DRIVE;
    engine_summary s;
    float top = (float)(int32_t)GT4_U16(p + 0x200) * RPM_TO_RAD / GT4_F32(p + 0x29c + GT4_U8(p + 0x236) * 4u), m;
    GT4_F32(p + 0xfd8) = top;
    if (K(0x43a6aaaau) < top) GT4_F32(p + 0xfd8) = K(0x43a6aaaau);
    m = GT4_F32(p + PARAMS_MASS);
    {
        uint32_t lay = GT4_LAYOUTS + GT4_U8(p + PARAMS_LAYOUT) * 84u;
        float b = GT4_F32(p + PARAMS_BRAKE) * GT4_F32(p + PARAMS_INV_RADIUS) * (float)(int32_t)GT4_U8(lay + LAYOUT_FRONT_WHEELS);
        GT4_F32(p + 0x1020) = b;
        GT4_F32(p + 0x101c) = m / (b + b);
    }
    memset(&s, 0, sizeof s);
    engine_summarize(p + PARAMS_ENGINE, &s, wear);
    if (GT4_U8(d + 0x12) == 1) {                                               /* func_003F1F78 */
        int32_t lim = (int32_t)GT4_U16(d + 0x2e) - 500, t = (uint16_t)sum_s16(&s, 8);
        GT4_U16(d + 0x30) = (uint16_t)(lim < t ? lim : t);
    }
    top_speed(p, wear);
    power_class(p, GT4_U8(rec + 0x120), sum_f32(&s, 0));
    GT4_F32(p + 0xfe0) = car_rating(p, &s);
}

/* The engine summary's peak power (PS) for parameters p made with wear
   (the parameters' own finish passes the entry) */
float gt4_engine_peak_power(uint32_t p, uint32_t wear)
{
    engine_summary s;
    memset(&s, 0, sizeof s);
    engine_summarize(p + PARAMS_ENGINE, &s, wear);
    return sum_f32(&s, 0);
}

/* func_0034EB00: boost of turbo stage j at rpm (+0xD4 s16 knee rpm, +0xD8
   curve kind, +0xE4 peak boost, +0xF4 shape). */
static float boost_stage(uint32_t e, int rpm, uint32_t j)
{
    int knee = (int16_t)GT4_U16(e + 0xd4 + j * 2);
    float peak = GT4_F32(e + 0xe4 + j * 4), shape = GT4_F32(e + 0xf4 + j * 4);
    if (rpm < knee) {
        uint8_t kind = GT4_U8(e + 0xd8 + j);
        if (kind == 1) return shape * (float)rpm * (float)rpm;
        if (kind == 0) {
            float d = (float)(rpm - knee);
            return peak - shape * d * d;
        }
        if (kind == 2) return peak * (float)rpm / (float)knee;
        return 0;
    }
    if (rpm < knee * 3) {
        float d = (float)(rpm - knee);
        return peak - shape * d * d * 0.25f;
    }
    return 0;
}

/* func_0034EC30: 1 + the boost of both stages. */
static float boost(uint32_t e, int rpm0, int rpm1)
{
    float total = 0;
    int rpm[2] = { (int16_t)rpm0, (int16_t)rpm1 };
    for (uint32_t j = 0; j < 2; j++)
        if (GT4_F32(e + 0xe4 + j * 4) != 0) total = total + boost_stage(e, rpm[j], j);
    return total + 1.0f;
}

/* func_0034ECF0: the boost, capped at 1 + the maximum (+0xFC). */
float gt4_engine_boost(uint32_t e, int rpm0, int rpm1)
{
    float b = boost(e, (int16_t)rpm0, (int16_t)rpm1), cap = GT4_F32(e + 0xfc) + 1.0f;
    return cap < b ? cap : b;
}

/* func_0034ED48: the turbo from the record (rec+0x113 maximum boost,
   0x114.. per stage: kind, rpm, peak). */
static void turbo_params(uint32_t rec, uint32_t e)
{
    uint32_t v = GT4_U8(rec + 0x113), a, b, c;
    if (!v) GT4_U32(e + 0xfc) = 0;
    else {
        GT4_F32(e + 0xfc) = (float)(int32_t)v * K(0x3dccccccu);
        if (!GT4_U8(rec + 0x116) || !GT4_U8(rec + 0x118) || !GT4_U8(rec + 0x11a)) GT4_U32(e + 0xfc) = 0;
    }
    if (GT4_F32(e + 0xfc) == 0) {
        GT4_F32(e + 0xf8) = 0;
        GT4_U16(e + 0xd6) = 0;
        GT4_U8(e + 0xd8) = 0;
        GT4_F32(e + 0xe4) = 0;
        GT4_F32(e + 0xec) = 0;
        GT4_F32(e + 0xf4) = 0;
        GT4_U16(e + 0xd4) = 0;
        GT4_U8(e + 0xd9) = 0;
        GT4_F32(e + 0xe8) = 0;
        GT4_F32(e + 0xf0) = 0;
        return;
    }
    GT4_U8(e + 0xd8) = GT4_U8(rec + 0x114) < 3 ? GT4_U8(rec + 0x114) : 0;
    {
        float peak = (float)(int32_t)GT4_U8(rec + 0x118) * K(0x3dccccccu), r;
        GT4_F32(e + 0xe4) = peak;
        GT4_F32(e + 0xec) = (float)(int32_t)GT4_U8(rec + 0x11a) * K(0x3dccccccu);
        GT4_F32(e + 0xf8) = 0;
        GT4_U8(e + 0xd9) = 0;
        GT4_F32(e + 0xe8) = 0;
        GT4_F32(e + 0xf0) = 0;
        GT4_U16(e + 0xd6) = 0;
        GT4_U16(e + 0xd4) = (uint16_t)(GT4_U8(rec + 0x116) * 100);
        r = (float)(int16_t)(GT4_U8(rec + 0x116) * 100);
        GT4_F32(e + 0xf4) = peak / (r * r);
    }
    a = GT4_U8(rec + 0x119);
    b = GT4_U8(rec + 0x11b);
    c = GT4_U8(rec + 0x117);
    if (!(a + b + c) || !a || !c || !b) return;
    GT4_U8(e + 0xd9) = GT4_U8(rec + 0x115) < 3 ? GT4_U8(rec + 0x115) : 0;
    {
        float peak = (float)(int32_t)GT4_U8(rec + 0x119) * K(0x3dccccccu), r;
        GT4_F32(e + 0xe8) = peak;
        GT4_F32(e + 0xf0) = (float)(int32_t)GT4_U8(rec + 0x11b) * K(0x3dccccccu);
        GT4_U16(e + 0xd6) = (uint16_t)(GT4_U8(rec + 0x117) * 100);
        r = (float)(int16_t)(GT4_U8(rec + 0x117) * 100);
        GT4_F32(e + 0xf8) = peak / (r * r);
    }
}

/* func_0034EF30: the engine block (e) from the record: redline, the torque
   curve (rpm at rec+0xC0 in hundreds, torque at rec+0xD8 in kgf m scaled by
   the tuning factors), engine braking, the turbo, and the power cap of the
   race (cap_ps, 0 = none). */
static void engine_params(uint32_t e, uint32_t rec, int32_t cap_ps)
{
    uint32_t n;
    float gain, lo, span, scale, s0, sn;
    GT4_U16(e + 0xce) = (uint16_t)(GT4_U8(rec + 0xbc) * 10);
    if (GT4_U8(rec + 0xbd)) GT4_U16(e + ENGINE_REDLINE) = (uint16_t)(GT4_U8(rec + 0xbd) * 100);
    else GT4_U16(e + ENGINE_REDLINE) = (uint16_t)(GT4_U8(rec + 0xbf + GT4_U8(rec + 0xbf)) * 100);
    {
        int32_t floor = (int32_t)GT4_U8(rec + 0xbe) * 100 + 200;
        if ((int32_t)GT4_U16(e + ENGINE_REDLINE) < floor) GT4_U16(e + ENGINE_REDLINE) = (uint16_t)floor;
    }
    n = GT4_U8(rec + 0xbf);
    if (!GT4_U16(rec + 0xd6 + n * 2)) {
        GT4_U16(rec + 0xd6 + n * 2) = GT4_U16(rec + 0xd4 + n * 2);
        n = GT4_U8(rec + 0xbf);
    }
    for (uint32_t i = 0; i < n; i++)
        GT4_F32(e + 0xc + i * 4) = (float)(int32_t)(GT4_U8(rec + 0xc0 + i) * 100) * RPM_TO_RAD;
    lo = GT4_U8(rec + 0x10c) ? (float)(int32_t)GT4_U8(rec + 0x10c) * K(0x3c23d70au) : 1.0f;
    gain = GT4_U16(rec + 0x10a) ? (float)(int32_t)GT4_U16(rec + 0x10a) * K(0x3a83126eu) : 1.0f;
    s0 = GT4_F32(e + 0xc);
    sn = GT4_F32(e + 0xc + n * 4 - 4);
    scale = GT4_U16(rec + 0x108) ? (float)(int32_t)GT4_U16(rec + 0x108) * K(0x3a83126eu) : 1.0f;
    scale = scale * K(0x3c23d70au);
    {
        uint32_t pct = GT4_U8(rec + 7);
        if (pct && pct != 100) scale = scale * ((float)(int32_t)pct / 100.0f);
    }
    if (n) {
        gain = gain - lo;
        span = sn - s0;
        for (uint32_t i = 0; i < n; i++) {
            int32_t t = (int16_t)GT4_U16(rec + 0xd8 + i * 2);
            float f;
            if (t < -1) t += 0x10000;
            f = lo + gain * (GT4_F32(e + 0xc + i * 4) - s0) / span;
            GT4_F32(e + 0x6c + i * 4) = (float)t * K(0x411ccccc) * (scale * f);
        }
    }
    curve_init(e, GT4_U8(rec + 0xbf), e + 0xc, e + 0x6c);
    {
        float b = (float)(int32_t)GT4_U16(rec + 0xba) * K(0x3dccccccu) * K(0x411ccccc);
        float top = GT4_F32(e + 0xc + GT4_U8(rec + 0xbf) * 4u - 4);
        GT4_F32(e + 0xd0) = GT4_U8(GAME_SETTINGS + 0x13c) ? b * K(0x3f666666u) : b / top;
    }
    turbo_params(rec, e);
    for (int i = 0; i < (int16_t)GT4_U16(e); i++) {
        int rpm = (int16_t)(GT4_U8(rec + 0xc0 + (uint32_t)i) * 100);
        float r = gt4_engine_boost(e, rpm, rpm);
        r = r / boost(e, rpm, rpm);
        GT4_F32(e + 0x6c + (uint32_t)i * 4) = GT4_F32(e + 0x6c + (uint32_t)i * 4) * r;
    }
    GT4_U8(e + ENGINE_CAP) = 0;
    GT4_U32(e + ENGINE_CAP_POWER) = 0;
    if (cap_ps > 0) {
        float soft = (float)(int32_t)GT4_U8(GAME_SETTINGS + 8) * K(0x3c23d70au);
        float w = (float)cap_ps * K(0x411ccccc) * K(0x44330cccu) * K(0x40c90fdau);
        GT4_U8(e + ENGINE_CAP) = 1;
        GT4_F32(e + ENGINE_CAP_SOFT) = soft;
        GT4_F32(e + ENGINE_CAP_POWER) = w / 60.0f / (1.0f - soft * 0.5f);
    }
}

#define DIFF_LETTERS 0x00620300u   /* D_00620300: "frnmhvyt", the differential kinds */
#define GEAR_TABLE   0x00620448u   /* D_00620448: 6 bytes per gear count 5..7 */
#define ELECTRIC     0x00620308u   /* D_00620308..: settings of drive kind 5 */

/* func_0034AAD8: index of a differential letter (the letter when unknown). */
static int8_t diff_kind(int8_t c)
{
    if ((int8_t)GT4_U8(DIFF_LETTERS) == c) return 0;
    for (int i = 1; i < 8; i++)
        if ((int8_t)GT4_U8(DIFF_LETTERS + (uint32_t)i) == c) return (int8_t)i;
    return c;
}

/* func_0034BC98: the drive kind (written to kind_at) from the drivetrain
   byte (rec+0x128) and the 4WD kind (rec+0x129); flag_at: a centre split
   exists, f_at/g_at: its parameters. Guest addresses. */
static uint32_t drive_kind(uint32_t rec, uint32_t kind_at, uint32_t flag_at, uint32_t f_at, uint32_t g_at)
{
    uint32_t c = GT4_U8(rec + 0x128);
    GT4_U32(f_at) = 0;
    GT4_U32(g_at) = 0;
    if (c >= 5) return GT4_U8(kind_at);
    if (c == 1) {
        GT4_U8(kind_at) = 1;
        GT4_U8(flag_at) = 0;
        return 1;
    }
    if (c != 2) {
        GT4_U8(kind_at) = 0;
        GT4_U8(flag_at) = 1;
        return 0;
    }
    GT4_F32(f_at) = (float)(int32_t)GT4_U8(rec + 0x12a) * K(0x3c23d70au);
    GT4_U8(flag_at) = 1;
    switch (GT4_U8(rec + 0x129)) {
    case 0:
        GT4_U8(kind_at) = 2;
        GT4_U8(flag_at) = 0;
        return 2;
    case 2: {
        uint32_t v;
        GT4_U8(kind_at) = 4;
        if (!(GT4_U8(rec + 0x12a) < 50)) GT4_F32(f_at) = 1.0f;
        v = GT4_U8(rec + 0x12b) ? GT4_U8(rec + 0x12b) : 8;
        GT4_F32(g_at) = (float)(int32_t)(v * 10) * K(0x411ccccc);
        return 4;
    }
    case 3: {
        float g;
        GT4_U8(kind_at) = 5;
        GT4_F32(f_at) = GT4_F32(f_at) * K(0x3eb851edu);
        g = (float)(int32_t)(GT4_U8(ELECTRIC + 1) * 10) * K(0x411ccccc);
        GT4_F32(f_at) = GT4_F32(f_at) * ((float)(int32_t)GT4_U8(ELECTRIC) / 100.0f);
        GT4_F32(0x0062030cu) = (float)(int32_t)GT4_U8(0x00620310u) / 100.0f;
        GT4_F32(0x00620314u) = (float)(int32_t)GT4_U8(0x00620318u) / 100.0f;
        GT4_F32(g_at) = g;
        return 5;
    }
    case 4: {
        uint32_t v;
        GT4_U8(kind_at) = 6;
        if (GT4_F32(f_at) != 0)
            GT4_F32(f_at) = 1.0f / (GT4_F32(f_at) * 100.0f * K(0x3c8efa34u));
        v = GT4_U8(rec + 0x12b) ? GT4_U8(rec + 0x12b) : 15;
        GT4_F32(g_at) = (float)(int32_t)(v * 10) * K(0x411ccccc);
        return 6;
    }
    case 5:
        GT4_U8(kind_at) = 7;
        hybrid_constants();
        return GT4_U8(kind_at);
    case 6:
        GT4_U8(kind_at) = 9;
        GT4_U8(flag_at) = 0;
        return 9;
    case 7: GT4_U8(kind_at) = 8; return 8;
    case 8: GT4_U8(kind_at) = 10; return 10;
    default: GT4_U8(kind_at) = 3; return 3;
    }
}

/* func_0034BA30: an axle's wheel: radius (r_at), tyre width (w_at) and
   sidewall height (h_at), from the sizes in the record (rec+0x1C8 width in
   5 mm, +0x1CA rim in inches, +0x1CC aspect, +0x1E2 rim offset). */
static void wheel_size(uint32_t rec, uint32_t i, uint32_t r_at, uint32_t w_at, uint32_t h_at)
{
    uint32_t w = GT4_U8(rec + 0x1c8 + i) * 5u, rim = GT4_U8(rec + 0x1ca + i), aspect = GT4_U8(rec + 0x1cc + i);
    float d, half, side;
    int8_t off;
    uint8_t fit;
    GT4_F32(w_at) = (float)(int32_t)(w ? w : 10) * K(0x3a83126eu);
    if (rim < 61) { d = (float)(int32_t)rim * K(0x3cd013a9u); half = 0.5f; }
    else if (rim % 10 == 0) { d = (float)(int32_t)(rim / 10) * K(0x3cd013a9u); half = 0.5f; }
    else { d = (float)(int32_t)rim * K(0x3cd013a9u); half = K(0x3d4ccccc); }
    half = d * half;
    side = GT4_F32(w_at) * (float)(int32_t)aspect * K(0x3d4ccccc);
    GT4_F32(h_at) = side;
    GT4_F32(r_at) = half + side;
    off = (int8_t)GT4_U8(rec + 0x1e2 + i);
    if (off) {
        float o = (float)off * K(0x3cd013a9u) * 0.5f;
        if (half + o < K(0x3cd013a9u)) o = K(0x3cd013a9u) - half;
        GT4_F32(h_at) = GT4_F32(h_at) - o;
        if (GT4_F32(h_at) < K(0x3c5013a9u)) GT4_F32(h_at) = K(0x3c5013a9u);
    }
    fit = GT4_U8(rec + 0x1e4 + i);
    if (fit && fit != 100) GT4_F32(w_at) = GT4_F32(w_at) * ((float)(int32_t)fit / 100.0f);
    if (special_layout(rec)) {
        uint32_t k = GT4_U8(rec + 0x23), t;
        if (k >= 3) k = 0;
        t = SPECIAL_TABLES + k * 0x78u;
        GT4_F32(w_at) = GT4_F32(t + (i ? 0xc : 4));
        GT4_F32(r_at) = GT4_F32(t + (i ? 8 : 0));
        GT4_F32(h_at) = GT4_F32(w_at) * 0.5f;
    }
}

/* func_00354FF8: gear ratios (at out: x1000, reverse first) for a top
   speed in km/h, from the table of the gear count. buf: 0x18 bytes of
   guest memory. */
static void auto_ratios(uint32_t rec, int32_t top, uint32_t out, int clear, uint32_t buf)
{
    uint32_t n = GT4_U8(rec + 0x155), row, t;
    float step, r1, r2, r3, ratio, g1, g2, gl, f;
    if (clear) GT4_U8(rec + 0x156) = 0;
    drive_kind(rec, buf + 0xc, buf + 0xd, buf + 0x10, buf + 0x14);
    wheel_size(rec, GT4_U8(buf + 0xd), buf, buf + 4, buf + 8);
    row = n < 5 ? 5 : n < 8 ? n : 7;
    t = GEAR_TABLE - 0x1e + row * 6;
    f = (float)(top - 150) / 250.0f;
    r1 = 200.0f / (float)(int32_t)GT4_U8(t + 1);
    r2 = 200.0f / (float)(int32_t)GT4_U8(t + 0);
    ratio = (float)(int32_t)(GT4_U8(rec + 0xbd) * 100) * RPM_TO_RAD * GT4_F32(buf)
            / ((float)top * K(0x3e8e38e3u)) / ((float)(int32_t)(int16_t)GT4_U16(rec + 0x150) * K(0x3a83126eu));
    g1 = r1 + f * (r2 - r1);
    r2 = 200.0f / (float)(int32_t)GT4_U8(t + 2);
    r1 = 200.0f / (float)(int32_t)GT4_U8(t + 3);
    g2 = r1 + f * (r2 - r1);
    r2 = 200.0f / (float)(int32_t)GT4_U8(t + 4);
    r3 = 200.0f / (float)(int32_t)GT4_U8(t + 5);
    gl = r3 + f * (r2 - r3);
    step = g1;
    for (uint32_t g = n; (int32_t)g > 0; g--) {
        if (g == 1) ratio = ratio * gl;
        GT4_U16(out + g * 2) = (uint16_t)(int32_t)(ratio * 1000.0f);
        ratio = ratio * step;
        step = step * g2;
    }
    {
        int32_t a = (int16_t)GT4_U16(out + 2), b = (int16_t)GT4_U16(out + 4);
        GT4_U16(out) = (uint16_t)((a * 3 + b) / 4);
    }
}

void gt4_car_auto_ratios(uint32_t rec, int32_t top, uint32_t out, int clear, uint32_t buf)
{
    auto_ratios(rec, top, out, clear, buf);
}

/* func_0034ABC8: ratios from the top speed setting when asked (rec+0x156). */
static void auto_ratios_if_set(uint32_t rec, uint32_t buf)
{
    int32_t top;
    if (!GT4_U8(rec + 0x156) || !GT4_U8(rec + 0x157)) return;
    top = GT4_U8(rec + 0x157) == 0xff ? 0xff : GT4_U8(rec + 0x157) * 10;
    for (uint32_t k = 0; k < 10; k++) GT4_U16(rec + 0x138 + k * 2) = 0xffff;
    auto_ratios(rec, top, rec + 0x138, 0, buf);
}

/* func_0034ACA8: the drivetrain block (d) from the record: drive kind,
   differentials, gear ratios, wheels, clutch and shift points. */
static void drive_params(uint32_t rec, uint32_t d, uint32_t e, uint32_t buf)
{
    float final, lsd[2];
    int driven[2] = { 0, 0 };
    drive_kind(rec, d, d + 3, d + 4, d + 8);
    for (uint32_t k = 0; k < 2; k++) GT4_U8(d + 0x10 + k) = (uint8_t)diff_kind((int8_t)GT4_U8(rec + 0x168 + k));
    switch (GT4_U8(d)) {                                                     /* func_0034AC48 */
    case 0: case 7: driven[1] = 1; break;
    case 1: driven[0] = 1; break;
    default: if (GT4_U8(d) < 11) driven[0] = driven[1] = 1; break;
    }
    for (uint32_t k = 0; k < 2; k++) {
        if (driven[k]) { if (!GT4_U8(d + 0x10 + k)) GT4_U8(d + 0x10 + k) = 2; }
        else if (GT4_U8(d + 0x10 + k)) GT4_U8(d + 0x10 + k) = 0;
    }
    {
        uint32_t lay = GT4_LAYOUTS + GT4_U8(rec + 2) * 84u;
        for (uint32_t j = 0; j < GT4_U8(lay + LAYOUT_DIFFS); j++) {
            uint32_t idx, at;
            if (GT4_U8(lay + LAYOUT_DIFF_WHEELS + j) != 1) continue;
            idx = j < GT4_U8(lay + LAYOUT_DIFFS) ? GT4_U8(lay + LAYOUT_DIFF_INDEX + j) : 0;
            at = d + 0x10 + idx;
            if (GT4_U8(at)) GT4_U8(at) = 1;
        }
    }
    for (uint32_t k = 0; k < 2; k++) {
        uint32_t c = GT4_U8(d + 0x10 + k), o = k * 4, r = rec + 0x160 + k;
        if (c >= 8) continue;
        switch (c) {
        case 0: case 1: case 2:
            GT4_U32(d + 0x14 + o) = 0;
            GT4_U32(d + 0x1c + o) = 0;
            GT4_U32(d + 0x24 + o) = 0;
            break;
        case 3: case 4: case 6:
            GT4_F32(d + 0x14 + o) = (float)(int32_t)GT4_U8(r + 0xa) * K(0x411ccccc);
            GT4_F32(d + 0x1c + o) = (float)(int32_t)GT4_U8(r + 0xc) * K(0x3c23d70au);
            GT4_F32(d + 0x24 + o) = (float)(int32_t)GT4_U8(r + 0xe) * K(0x3c23d70au);
            break;
        case 5: {
            float v = (float)(int32_t)GT4_U8(r + 0xa);
            GT4_F32(d + 0x14 + o) = v * v;
            GT4_F32(d + 0x1c + o) = (float)(int32_t)GT4_U8(r + 0xc) * K(0x3dccccccu) * K(0x411ccccc);
            GT4_U32(d + 0x24 + o) = 0;
            break;
        }
        case 7:
            GT4_F32(d + 0x14 + o) = (float)(int32_t)GT4_U8(r + 0xa) * K(0x3c23d70au);
            GT4_F32(d + 0x1c + o) = (float)(int32_t)GT4_U8(r + 0xc) * K(0x3c23d70au);
            GT4_F32(d + 0x24 + o) = (float)(int32_t)GT4_U8(r + 0xe) * K(0x3c23d70au);
            break;
        }
    }
    auto_ratios_if_set(rec, buf);
    /* gearbox */
    GT4_U8(d + 2) = GT4_U8(rec + 0x155);
    GT4_U8(d + 0x12) = GT4_U8(rec + 0x158);
    if (GT4_U8(rec + 0x158) == 5) GT4_U8(d + 0x12) = 2;
    else if (GT4_U8(rec + 0x158) == 6) GT4_U8(d + 0x12) = 0;
    if (GT4_U8(d + 2) >= 10) GT4_U8(d + 2) = 9;
    else if (GT4_U8(d + 0x12) == 3 && GT4_U8(d + 2) != 1) GT4_U8(d + 2) = 1;
    else if (GT4_U8(rec + 0x158) == 1 && GT4_U8(d + 2) < 2) GT4_U8(d + 0x12) = 0;
    {
        int32_t f = (int16_t)GT4_U16(rec + 0x150);
        final = (float)(f < 500 ? 4000 : f) * K(0x3a83126eu);
    }
    {
        uint32_t g = 0;
        int32_t r = (int16_t)GT4_U16(rec + 0x138);
        for (;;) {
            if (r < 100) {
                if (g >= 2) { GT4_U8(d + 2) = (uint8_t)(g - 1); break; }
                r = g == 1 ? 4000 : 3500;
            }
            GT4_F32(d + 0x40 + g * 4) = (float)r * K(0x3a83126eu) * final;
            g++;
            if ((int32_t)GT4_U8(d + 2) < (int32_t)g) break;
            r = (int16_t)GT4_U16(rec + 0x138 + g * 2);
        }
    }
    {
        float rev = -GT4_F32(d + 0x40);
        uint32_t kind = GT4_U8(rec + 0x154);
        int32_t v = (int16_t)GT4_U16(rec + 0x152);
        GT4_F32(d + 0x40) = rev;
        if (kind && kind != 4 && !(v < 500)) {
            float m = (float)v * K(0x3a83126eu);
            if (kind == 2) {
                m = m / final;
                GT4_F32(d + 0x40) = rev * m;
                for (uint32_t g = 5; g <= GT4_U8(d + 2); g++) GT4_F32(d + 0x40 + g * 4) = GT4_F32(d + 0x40 + g * 4) * m;
            } else if (kind == 1) {
                for (uint32_t g = 0; g <= GT4_U8(d + 2); g++) GT4_F32(d + 0x40 + g * 4) = GT4_F32(d + 0x40 + g * 4) * m;
            } else if (kind == 3) {
                m = m / final;
                GT4_F32(d + 0x40) = rev * m;
                for (uint32_t g = 3; g < 5; g++) GT4_F32(d + 0x40 + g * 4) = GT4_F32(d + 0x40 + g * 4) * m;
            }
        }
    }
    /* wheels */
    for (uint32_t k = 0; k < 2; k++) {
        uint32_t o = k * 4;
        wheel_size(rec, k, d + 0x90 + o, d + 0xa0 + o, d + 0xa8 + o);
        if (GT4_F32(d + 0x90 + o) != 0) GT4_F32(d + 0x98 + o) = 1.0f / GT4_F32(d + 0x90 + o);
    }
    GT4_U8(d + 1) = 0;
    if (GT4_U8(d) == 5 && GT4_F32(d + 0x90) != GT4_F32(d + 0x94)) GT4_U8(d + 1) = 1;
    if (GT4_U8(rec + 0x154) == 4) GT4_U8(d + 1) = 1;
    for (uint32_t k = 0; k < 2; k++) lsd[k] = (float)(int32_t)(int16_t)GT4_U16(rec + 0x174 + k * 2) * K(0x3c23d70au);
    for (uint32_t k = 0; k < 2; k++) {
        float i = lsd[k], r;
        if (i == 0) continue;
        GT4_F32(d + 0xb4 + k * 4) = GT4_F32(d + 0x90 + k * 4) / i;
        r = GT4_F32(d + 0x98 + k * 4);
        GT4_F32(d + 0xcc + k * 4) = i * r * r;
    }
    GT4_F32(d + 0xc) = (float)(int32_t)GT4_U16(rec + 0xb8) * K(0x3a83126eu);
    if (GT4_U16(rec + 0xb8)) GT4_F32(d + 0xc8) = 1.0f / GT4_F32(d + 0xc);
    for (uint32_t k = 0; k < 2; k++) {
        float w = (float)(int32_t)(int16_t)GT4_U16(rec + 0x164 + k * 2) * K(0x3c23d70au);
        if (GT4_U8(d + 1) && k != GT4_U8(d + 3))
            w = w * (GT4_F32(d + 0x90 + GT4_U8(d + 3) * 4u) / GT4_F32(d + 0x90 + k * 4));
        GT4_F32(d + 0xbc + k * 4) = w * final + (lsd[k] + lsd[k]);
    }
    GT4_F32(d + 0xc4) = 1.0f / (GT4_F32(d + 0xbc) + GT4_F32(d + 0xc0));
    GT4_U16(d + 0x2c) = (uint16_t)(GT4_U8(rec + 0x160) * 10);
    GT4_U16(d + 0x2e) = (uint16_t)(GT4_U8(rec + 0xbe) * 100);
    GT4_F32(d + 0xb0) = (float)(int32_t)GT4_U8(rec + 0x162) * K(0x411ccccc);
    for (uint32_t g = 0; g <= GT4_U8(d + 2); g++)
        GT4_F32(d + 0x68 + g * 4) = GT4_F32(d + 0x40 + g * 4) / GT4_F32(d + 0x90 + GT4_U8(d + 3) * 4u);
    if ((uint8_t)(GT4_U8(d + 0x12) - 1) < 2) GT4_U16(d + 0x2c) = GT4_U16(e + 0xce);        /* func_003F1F98 */
    else if ((int32_t)GT4_U16(d + 0x2c) - 100 < (int32_t)GT4_U16(e + 0xce)) GT4_U16(d + 0x2c) = (uint16_t)(GT4_U16(e + 0xce) + 100);
    /* upshift points */
    for (uint32_t g = 1; (int32_t)g < (int32_t)GT4_U8(d + 2); g++) {
        int32_t up = (int32_t)((float)(int32_t)GT4_U16(d + 0x2e) * RPM_TO_RAD * GT4_F32(d + 0x48 + (g - 1) * 4)
                               / GT4_F32(d + 0x44 + (g - 1) * 4) * 60.0f / K(0x40c90fdau));
        int32_t lo = GT4_U16(d + 0x2c), drop;
        if (GT4_U8(GAME_SETTINGS + 0x112) && g == 1) up = (lo + up) / 2;
        drop = (int32_t)(GT4_F32(d + 0x70 + (g - 1) * 4) * K(0x4031c71bu) * 60.0f / K(0x40c90fdau));
        up = up - drop;
        lo = lo + 500;
        if (up < lo) up = lo;
        GT4_U16(d + 0x30 + (g - 1) * 2) = (uint16_t)up;
    }
    if ((uint8_t)(GT4_U8(d + 0x12) - 1) < 2 && GT4_U8(d + 2) >= 2)
        for (uint32_t g = 2; g <= GT4_U8(d + 2); g++) GT4_F32(d + 0x48 + (g - 2) * 4) = GT4_F32(d + 0x48 + (g - 2) * 4) / GT4_F32(d + 0x44);
    {
        uint16_t red = GT4_U16(d + 0x2e);                                     /* func_00367D90 */
        GT4_U8(d + 0x13) = (uint8_t)(red < 4500 ? 1 : red < 5000 ? 2 : 3);
    }
    if (GT4_U8(rec + 1) == 5) GT4_U8(d + 0x13) = 4;
}

/* func_00361908: the physics parameters (params) from the car's record (rec). */
void gt4_car_params(uint32_t p, uint32_t rec, int32_t mode, uint32_t kind, uint32_t race, uint32_t entry, uint32_t buf)
{
    float front, rear, extra = 0, len, aero;
    uint32_t cfg = GAME_SETTINGS;
    (void)kind;   /* passed on by the original to parts that do not read it */

    GT4_U8(p + 0x10d3) = 0;
    GT4_U8(p + 0x33) = GT4_U8(rec + 0x23);
    if (GT4_U8(rec + 0x210) >= 150) GT4_U8(p + 0x10d3) = 1;
    GT4_U8(p + 0x32) = GT4_U8(rec + 1);
    GT4_U8(p + 0x30) = GT4_U8(rec + 2);                  /* func_003594E0 */
    if (GT4_U8(rec + 3)) GT4_U8(p + 0x10d2) = GT4_U8(rec + 3);
    else {
        uint8_t k = GT4_U8(rec + 0x128);
        GT4_U8(p + 0x10d2) = (uint8_t)(k == 3 ? 2 : k == 4 ? 3 : 1);
    }
    GT4_U8(p + 0x1076) = GT4_U8(rec + 0xa8);
    if (GT4_U8(rec + 0x128) >= 5) GT4_U8(rec + 0x128) = 0;

    /* mass (kg), and its percentage */
    GT4_U16(p + 0x1048) = GT4_U16(rec + 0x20);
    GT4_F32(p + PARAMS_MASS) = GT4_S16(rec + 0x20) ? (float)GT4_S16(rec + 0x20) : 1200.0f;
    {
        uint32_t pct = GT4_U8(rec + 8);
        if (pct && pct != 100) {
            int32_t m = (int32_t)GT4_S16(p + 0x1048) * (int32_t)pct / 100;
            GT4_F32(p + PARAMS_MASS) = GT4_F32(p + PARAMS_MASS) * ((float)(int32_t)pct / 100.0f);
            GT4_U16(p + 0x1048) = (uint16_t)m;
        }
    }
    GT4_F32(p + 0x0) = (float)GT4_S16(rec + 0x12) * K(0x3a83126eu);
    front = (float)(int32_t)GT4_U8(rec + 0x22) * K(0x3c23d70au);
    GT4_F32(p + 0x4) = (float)GT4_S16(rec + 0x14) * K(0x3a83126eu);
    GT4_F32(p + 0x18) = (float)GT4_S16(rec + 0x1a) * K(0x3a83126eu);
    if (special_layout(rec)) GT4_F32(p + 0x18) = GT4_F32(special_table(p) + 0x74);
    len = GT4_F32(p + 0x18);
    if (GT4_U8(rec + 5)) {
        /* ballast: its mass at its position moves the weight split */
        float at = (float)(50 - (int32_t)GT4_S8(rec + 6)) * K(0x3c23d70au);
        float m = GT4_F32(p + PARAMS_MASS), b = (float)(int32_t)GT4_U8(rec + 5), total = m + b;
        float was = front, dm, dw;
        GT4_F32(p + PARAMS_MASS) = total;
        GT4_U16(p + 0x1048) = (uint16_t)(GT4_U16(p + 0x1048) + GT4_U8(rec + 5));
        front = (m * was + b * at) / total;
        dw = len * (front - at);
        dm = len * (was - front);
        extra = m * dm * dm + b * dw * dw;
    }
    {
        float f = len * front, r;
        rear = 1.0f - front;
        GT4_F32(p + 0x24) = front;
        r = len - f;
        GT4_F32(p + 0x14) = f;
        GT4_F32(p + 0x20) = rear;
        GT4_F32(p + 0x10) = r;
        GT4_F32(p + PARAMS_50) = -(r - f) * 0.5f;
    }
    if (special_layout(rec)) {
        uint32_t t = special_table(p);
        GT4_F32(p + PARAMS_50) = -(GT4_F32(t + 0x14) * rear + GT4_F32(t + 0x70) * front);
    }
    GT4_F32(p + 0x0) = GT4_F32(p + 0x0) - GT4_F32(p + PARAMS_50);
    GT4_F32(p + 0x4) = GT4_F32(p + 0x4) + GT4_F32(p + PARAMS_50);
    GT4_F32(p + 0x8) = (float)GT4_S16(rec + 0x16) * K(0x3a83126eu);
    GT4_F32(p + 0x28) = (float)((int32_t)GT4_S16(rec + 0x1c) - 2 * (int32_t)GT4_S8(rec + 0x96)) * K(0x3a83126eu) * 0.5f;
    GT4_F32(p + 0x2c) = (float)((int32_t)GT4_S16(rec + 0x1e) - 2 * (int32_t)GT4_S8(rec + 0x97)) * K(0x3a83126eu) * 0.5f;
    GT4_F32(p + 0xc) = (float)GT4_S16(rec + 0x18) * K(0x3a83126eu);
    GT4_F32(p + PARAMS_MASS) = GT4_F32(p + PARAMS_MASS)
        + (float)(int32_t)(GT4_U8(p + PARAMS_DRIVE) == 7 ? GT4_U8(cfg + 3) : GT4_U8(cfg + 3) + GT4_U8(cfg + 4));

    engine_params(p + PARAMS_ENGINE, rec, (int32_t)GT4_U8(race + 3) * 10);
    drive_params(rec, p + PARAMS_DRIVE, p + PARAMS_ENGINE, buf);
    GT4_F32(p + PARAMS_FUEL_RATE) = (float)(int32_t)GT4_U8(rec + 0x112);
    GT4_U8(p + 0x104b) = GT4_U8(rec + 0x11c);
    /* the tyres narrow the track when they do not fit */
    for (int i = 0; i < 2; i++) {
        uint8_t fit = GT4_U8(rec + 0x1e4 + i);
        int16_t w = GT4_S16(rec + 0x30 + i * 2);
        float half, edge, lim;
        if (!fit || fit == 100 || !w) continue;
        half = GT4_F32(p + 0x28 + i * 4);
        edge = half + GT4_F32(p + PARAMS_LOAD_REF + i * 4) * 0.5f;
        lim = (float)w * K(0x3a83126eu) * 0.5f;
        if (lim < edge) GT4_F32(p + 0x28 + i * 4) = half - (edge - lim);
    }
    GT4_F32(p + 0x1c) = GT4_F32(p + 0x28) * GT4_F32(p + 0x24) + GT4_F32(p + 0x2c) * (1.0f - GT4_F32(p + 0x24));
    GT4_F32(p + 0x1c) = GT4_F32(p + 0x1c) + GT4_F32(p + 0x1c);
    GT4_F32(p + 0x10d4) = (float)(int32_t)GT4_U8(rec + 0x11d) * K(0x3c23d70au);
    GT4_F32(p + 0x10d8) = (float)((int32_t)GT4_S16(rec + 0x11e) * 100);
    /* func_0036CD98: shift points */
    GT4_U8(p + 0x1074) = GT4_U8(rec + 0x110) ? GT4_U8(rec + 0x110) : (uint8_t)(GT4_U16(p + 0x262) / 100u);
    GT4_U8(p + 0x1075) = GT4_U8(rec + 0x111) ? GT4_U8(rec + 0x111)
                                               : (uint8_t)(((int32_t)GT4_U16(p + 0x200) + 900) / 1000);
    GT4_U8(p + 0x31) = (uint8_t)tyre_class(GT4_U8(rec + 0x128), GT4_U8(rec + 0x129));   /* func_00466B28 */
    {
        uint32_t a = GT4_U8(cfg + 0x118), b = GT4_U8(cfg + 0x119);
        GT4_F32(p + PARAMS_SLIP_SCALE) = K(0x3fc90fd9u) / ((float)(int32_t)(a ? a : 50) * K(0x3e8e38e3u));
        GT4_F32(p + 0xfc8) = 1.0f / ((float)(int32_t)(b ? b : 50) * K(0x3e8e38e3u));
    }
    gt4_tyre_blocks(p, rec, 0);
    /* camber curves per axle */
    for (int i = 0; i < 2; i++) {
        uint32_t n = GT4_U8(rec + 0x1ce + i), curve = p + PARAMS_CAMBER_CURVE + i * 12;
        uint32_t at = p + 0x3ec + (n + i * 4) * 4;
        for (uint32_t j = 0; j < n; j++, at -= 4) {
            uint32_t o = 0x1d0 + i * 4 + j;
            GT4_F32(at - 0x20) = (float)(128 - (int32_t)GT4_U8(rec + o)) * K(0x3d8efa34u) * 0.03125f;
            GT4_F32(at) = (float)(int32_t)GT4_U8(rec + 8 + o) / 200.0f;
        }
        curve_init(curve, n, p + i * 16 + 0x3d0, p + i * 16 + 0x3f0);        /* func_00359A38 */
    }
    for (int k = 0; k < 2; k++) {
        float lo = (float)(int32_t)GT4_U8(cfg + 0xbc) * K(0x42c3ffffu);
        float d = (float)(int32_t)GT4_U8(cfg + 0xbd) * K(0x42c3ffffu) - lo;
        GT4_F32(p + 0x1088 + k * 4) = lo - d;
        GT4_F32(p + 0x1090 + k * 4) = d / (GT4_F32(p + PARAMS_LOAD_REF + k * 4) * K(0x46191fffu));
    }
    {
        uint32_t grip[2] = { GT4_U8(rec + 0x2a), GT4_U8(rec + 0x2b) };
        for (int k = 0; k < 2; k++)
            GT4_F32(p + PARAMS_AXLE_GRIP + k * 4) = (float)(int32_t)(grip[k] + 900) * K(0x3a83126eu);
        if (mode == 2) {
            uint32_t t = race + GT4_U8(p + 0x31);
            grip[0] = GT4_U8(t + 4);
            grip[1] = GT4_U8(t + 8);
            for (int k = 0; k < 2; k++) {
                if (!grip[k]) grip[k] = 100;
                GT4_F32(p + PARAMS_AXLE_GRIP + k * 4) =
                    GT4_F32(p + PARAMS_AXLE_GRIP + k * 4) * ((float)(int32_t)grip[k] * K(0x3c23d70au));
            }
        }
    }
    /* aerodynamics */
    if (GT4_S16(rec + 0x40)) aero = (float)GT4_S16(rec + 0x40) / 10000.0f * (GT4_F32(AIR_DENSITY) * 0.5f);
    else aero = GT4_F32(p + 0x8) * GT4_F32(p + 0xc) * GT4_F32(AIR_DENSITY) * 0.5f;
    {
        float cd = (float)(int32_t)GT4_U8(rec + 0x42) * K(0x3c23d70au);
        GT4_F32(p + 0x1064) = cd;
        GT4_F32(p + PARAMS_DRAG) = aero * cd;
        GT4_F32(p + PARAMS_DOWNFORCE) = aero * ((float)(int32_t)GT4_U8(rec + 0x43) * K(0x3c23d70au));
        GT4_F32(p + PARAMS_DOWNFORCE + 4) = aero * ((float)(int32_t)GT4_U8(rec + 0x44) * K(0x3c23d70au));
    }
    GT4_U8(p + PARAMS_AERO_KIND) = GT4_U8(rec + 0x4c);
    if (GT4_U8(rec + 0x4c)) {
        GT4_F32(p + 0x1078) = (float)(int32_t)GT4_U8(rec + 0x4d) * K(0x3e8e38e3u);
        GT4_F32(p + 0x107c) = (float)(int32_t)GT4_U8(rec + 0x4e) * K(0x3e8e38e3u);
        GT4_F32(p + 0x1080) = GT4_U8(rec + 0x4f) ? 10.0f / (float)(int32_t)GT4_U8(rec + 0x4f) : 300.0f;
        GT4_F32(p + PARAMS_AERO_EXTRA) = aero * ((float)(int32_t)GT4_U8(rec + 0x50) * K(0x3c23d70au));
        if (GT4_U8(p + PARAMS_AERO_KIND) == 7)
            GT4_F32(p + PARAMS_AERO_EXTRA) = (float)(int32_t)GT4_U8(rec + 0x50) * K(0x42c3ffffu);
    } else {
        GT4_U32(p + 0x1078) = GT4_U32(p + 0x107c) = GT4_U32(p + 0x1080) = GT4_U32(p + PARAMS_AERO_EXTRA) = 0;
    }
    GT4_F32(p + PARAMS_DRAG) = GT4_F32(p + PARAMS_DRAG)
        + (GT4_F32(p + PARAMS_DOWNFORCE) + GT4_F32(p + PARAMS_DOWNFORCE + 4))
          * ((float)(int32_t)GT4_U8(cfg + 0xa) * K(0x3a83126eu));
    /* brakes */
    for (int k = 0; k < 2; k++) {
        uint32_t b = GT4_U8(rec + 0x178 + k);
        GT4_F32(p + PARAMS_BRAKE + k * 4) = (float)(int32_t)(b * 10) * K(0x411ccccc);
        GT4_F32(p + 0x1024 + k * 4) = (float)(int32_t)GT4_U8(rec + 0x1a8 + k) * K(0x3dccccccu);
        GT4_F32(p + 0x102c + k * 4) = (float)(int32_t)GT4_U8(rec + 0x1aa + k) * K(0x3c23d70au);
        if (!b) GT4_U32(p + 0x1034 + k * 4) = 0;
        else GT4_F32(p + 0x1034 + k * 4) = GT4_F32(p + PARAMS_2C4 + k * 4) / GT4_F32(p + PARAMS_BRAKE + k * 4);
    }
    GT4_F32(p + PARAMS_HANDBRAKE) = (float)(int32_t)(GT4_U8(rec + 0x17a) * 10) * K(0x411ccccc);
    GT4_U8(p + 0x10d0) = GT4_U8(rec + 0x2c);
    GT4_F32(p + 0x10cc) = (float)(int32_t)GT4_U8(rec + 0x2d) * K(0x3c23d70au);
    /* driving aids */
    {
        uint32_t a = p + 0x1098, fl = GT4_U8(rec + 0x1b8);
        GT4_F32(a + 0) = (float)(int32_t)GT4_U8(rec + 0x1ba) * K(0x3c23d70au);
        GT4_F32(a + 4) = (float)(int32_t)GT4_U8(rec + 0x1bb) * K(0x3c23d70au);
        GT4_F32(a + 8) = (float)(int32_t)GT4_U8(rec + 0x1bc) * K(0x3c23d70au);
        GT4_U8(a + 0xc) = (uint8_t)(fl & 1);
        GT4_U8(a + 0xd) = (uint8_t)((fl >> 1) & 1);
        GT4_U8(a + 0xe) = (uint8_t)((fl >> 2) & 1);
        GT4_U8(a + 0xf) = (uint8_t)((fl >> 3) & 1);
        GT4_U8(a + 0x30) = (uint8_t)((GT4_U32(a + 0xc) & 0xffffu) && GT4_F32(a + 0) != 0);
        GT4_U8(a + 0x31) = (uint8_t)((GT4_U32(a + 0xc) & 0xffff0000u) && GT4_F32(a + 8) != 0);
        GT4_F32(a + 0x10) = (float)(int32_t)GT4_U8(rec + 0x1bd) * K(0x3c23d70au);
        GT4_F32(a + 0x14) = (float)(int32_t)GT4_U8(rec + 0x1be) * K(0x3c23d70au);
        GT4_F32(a + 0x18) = (float)(int32_t)GT4_U8(rec + 0x1bf) * K(0x3c23d70au);
        GT4_U8(a + 0x1c) = (uint8_t)((fl >> 4) & 1);
        GT4_U8(a + 0x1d) = (uint8_t)((fl >> 5) & 1);
        GT4_U8(a + 0x1e) = (uint8_t)((fl >> 6) & 1);
        GT4_U8(a + 0x1f) = (uint8_t)(fl >> 7);
        GT4_U8(a + 0x32) = (uint8_t)((GT4_U32(a + 0x1c) & 0xffffu) && GT4_F32(a + 0x10) != 0);
        GT4_U8(a + 0x33) = (uint8_t)((GT4_U32(a + 0x1c) & 0xffff0000u) && GT4_F32(a + 0x18) != 0);
        GT4_F32(a + 0x24) = (float)(int32_t)GT4_U8(rec + 0x1c0) * K(0x3dccccccu);
        GT4_F32(a + 0x28) = (float)(int32_t)GT4_U8(rec + 0x1c1) * K(0x3c23d70au);
        GT4_F32(a + 0x20) = (float)(int32_t)GT4_U8(rec + 0x1c2) * K(0x3dccccccu);
        {
            uint32_t v = GT4_U8(rec + 0x1c3);
            GT4_F32(a + 0x2c) = (v == 100 || !v) ? 1.0f : (float)(int32_t)v * K(0x3c23d70au);
        }
    }
    /* steering */
    GT4_F32(p + PARAMS_STEER_MAX) = (float)(int32_t)GT4_U8(rec + 0x28) * K(0x3c8efa34u);
    speed_curve(p + 0x58, p + 0x64, p + 0x7c, GT4_U8(rec + 0x180), rec + 0x184, rec + 0x18a);
    speed_curve(p + 0x94, p + 0xa0, p + 0xb8, GT4_U8(rec + 0x182), rec + 0x19c, rec + 0x1a2);
    speed_curve(p + 0xd0, p + 0xdc, p + 0xf4, GT4_U8(rec + 0x181), rec + 0x190, rec + 0x196);
    {
        float wb = GT4_F32(p + 0x18), half = GT4_F32(p + 0x28);
        GT4_F32(p + 0x10c) = gt4_atan2f(wb, wb - half) / K(0x3f490fd9u);
        GT4_F32(p + 0x110) = gt4_atan2f(wb, wb + half) / K(0x3f490fd9u);
    }
    if (!GT4_U8(rec + 0x29)) GT4_U32(p + PARAMS_CASTER) = 0;
    else {
        float a = (float)(int32_t)GT4_U8(rec + 0x29) * K(0x3c8efa34u);
        if (special_layout(rec)) a = GT4_F32(special_table(p) + 0x30);
        GT4_F32(p + PARAMS_CASTER) = gt4_cosf(a);
    }
    /* suspension geometry per axle */
    for (int i = 0; i < 2; i++) {
        uint32_t o = (uint32_t)i * 4;
        float y = (float)((int32_t)GT4_U8(rec + 0x8e + i) - 0x80) * 0.5f * K(0x3a83126eu);
        GT4_F32(p + 0x1008 + o) = gt4_atan2f(y, GT4_F32(p + PARAMS_2C4 + o));
        GT4_F32(p + PARAMS_CAMBER + o) = -((float)(int32_t)GT4_U8(rec + 0x88 + i) * K(0x3dccccccu) * K(0x3c8efa34u));
        GT4_F32(p + PARAMS_CAMBER_GAIN + o) = (float)(int32_t)GT4_S8(rec + 0x90 + i) * K(0x3c8efa34u);
        {
            gt4_curve *c = (gt4_curve *)gt4_ptr(p + PARAMS_CAMBER_CURVE + (uint32_t)i * 12);
            GT4_F32(p + 0xff4 + o) = gt4_curve_eval(c, GT4_F32(p + PARAMS_CAMBER + o));
        }
        GT4_F32(p + PARAMS_ARM + o) = (float)(int32_t)GT4_U8(rec + 0x98 + i) * K(0x3c23d70au);
    }
    /* inertia (the ballast term is not in the inverse) */
    {
        float yaw = GT4_U8(rec + 0x24) ? (float)(int32_t)GT4_U8(rec + 0x24) * 100.0f : 6000.0f;
        float pitch = GT4_U8(rec + 0x25) ? (float)(int32_t)GT4_U8(rec + 0x25) * 100.0f : 4000.0f;
        float roll = GT4_U8(rec + 0x26) ? (float)(int32_t)GT4_U8(rec + 0x26) * 100.0f : 2000.0f;
        float cg, h, m;
        GT4_F32(p + 0x114) = yaw;
        if (GT4_U8(rec + 5)) GT4_F32(p + 0x114) = yaw + extra;
        GT4_F32(p + PARAMS_YAW_INERTIA) = 1.0f / yaw;
        GT4_F32(p + PARAMS_PITCH_ARM) = GT4_F32(p + 0x10) / pitch;
        GT4_F32(p + PARAMS_PITCH_ARM + 4) = GT4_F32(p + 0x14) / pitch;
        GT4_F32(p + PARAMS_ROLL_ARM) = GT4_F32(p + 0x28) / roll;
        GT4_F32(p + PARAMS_ROLL_ARM + 4) = GT4_F32(p + 0x2c) / roll;
        assist_settings();                                                  /* func_00360A78 */
        spring_params(p + PARAMS_AXLES, rec + 0x58, GT4_U8(rec + 0x8c));       /* func_00360B60 */
        spring_params(p + PARAMS_AXLES + 0x58, rec + 0x70, GT4_U8(rec + 0x8d));
        GT4_F32(p + PARAMS_ROLL_HEIGHT) = (float)(GT4_S16(rec + 0x9a) ? (int32_t)GT4_S16(rec + 0x9a)
                                                                    : (int32_t)GT4_U8(rec + 0x8a)) * K(0x3a83126eu);
        GT4_F32(p + PARAMS_ROLL_HEIGHT + 4) = (float)(GT4_S16(rec + 0x9c) ? (int32_t)GT4_S16(rec + 0x9c)
                                                                        : (int32_t)GT4_U8(rec + 0x8b)) * K(0x3a83126eu);
        if (GT4_U8(p + PARAMS_AERO_KIND) == 7) {
            float d = GT4_F32(p + PARAMS_ROLL_HEIGHT) - GT4_F32(p + PARAMS_ROLL_HEIGHT + 4);
            if (d < 0) d = -d;
            if (K(0x3cf5c28fu) < d) {
                GT4_F32(p + PARAMS_AERO_EXTRA) = 0;
                GT4_U8(p + PARAMS_AERO_KIND) = 0;
            }
        }
        h = GT4_F32(p + PARAMS_ROLL_HEIGHT) * GT4_F32(p + 0x24) + GT4_F32(p + PARAMS_ROLL_HEIGHT + 4) * GT4_F32(p + 0x20);
        GT4_F32(p + 0x3c) = h;
        GT4_F32(p + 0x103c) = GT4_F32(p + PARAMS_MASS) * K(0x411ccccc);
        cg = (float)(int32_t)GT4_U8(rec + 0x27) * K(0x3a83126eu);
        GT4_F32(p + PARAMS_CG_HEIGHT) = cg;
        h = (cg + h) * 0.5f;
        GT4_F32(p + PARAMS_PITCH_TRANSFER) = h / GT4_F32(p + 0x18);
        GT4_F32(p + PARAMS_ROLL_TRANSFER) = h / GT4_F32(p + 0x1c);
        /* sprung mass: less the unsprung mass of the wheels */
        {
            uint32_t lay = GT4_LAYOUTS + GT4_U8(p + PARAMS_LAYOUT) * 84u;
            uint32_t nf = GT4_U8(lay + LAYOUT_FRONT_WHEELS), nr = GT4_U8(lay + LAYOUT_REAR_WHEELS);
            float s;
            m = GT4_F32(p + PARAMS_MASS);
            if (nf == 2 && nr == 2) s = m - (GT4_F32(p + 0x340) + GT4_F32(p + 0x398)) * 2.0f;
            else s = m - (GT4_F32(p + 0x340) * (float)(int32_t)nf + GT4_F32(p + 0x398) * (float)(int32_t)nr);
            if (!(s <= 0)) {
                float w = s * K(0x411ccccc), fz, rz;
                float ratio = s / m;
                GT4_F32(p + PARAMS_MASS_B) = s;
                GT4_F32(p + PARAMS_ROLL_TRANSFER) = s * GT4_F32(p + PARAMS_ROLL_TRANSFER);
                GT4_F32(p + 0x1044) = w * GT4_F32(p + 0x20);
                GT4_F32(p + PARAMS_PITCH_TRANSFER) = ratio * GT4_F32(p + PARAMS_PITCH_TRANSFER);
                GT4_F32(p + 0x1040) = w * GT4_F32(p + 0x24);
                GT4_F32(p + 0x1050) = (float)(int32_t)GT4_U8(rec + 0x38);
                GT4_F32(p + 0x1054) = (float)(int32_t)GT4_U8(rec + 0x39);
                if (!GT4_S16(rec + 0x3a)) {
                    fz = GT4_F32(p + 0x1040);
                    rz = GT4_F32(p + 0x1044);
                } else {
                    float t = (float)(int32_t)GT4_S16(rec + 0x3a) * K(0x411ccccc);
                    rz = t * GT4_F32(p + 0x20);
                    fz = t * GT4_F32(p + 0x24);
                }
                m = GT4_F32(p + PARAMS_MASS);
                GT4_F32(p + 0x1058) = (float)(int32_t)nf / fz;
                GT4_F32(p + 0x105c) = (float)(int32_t)nr / rz;
            }
        }
        {
            float d = (float)(int32_t)GT4_U8(cfg + 7) * K(0x3c23d70au);
            float a = pitch + m * GT4_F32(p + 0x10) * GT4_F32(p + 0x10);
            float b = pitch + m * GT4_F32(p + 0x14) * GT4_F32(p + 0x14);
            GT4_F32(p + 0x12c) = d / a;
            GT4_F32(p + 0x130) = d / b;
        }
    }
    gt4_tyre_static_load(p, 0);
    for (uint32_t i = 0; i < 2; i++) axle_travel(p, p + PARAMS_AXLES + i * 0x58, rec, i);
    {
        float f0 = GT4_F32(p + 0x310), r0 = GT4_F32(p + 0x368);
        float df = GT4_F32(p + 0x314) - f0, dr = GT4_F32(p + 0x36c) - r0;
        GT4_F32(p + 0x34c) = f0;
        GT4_F32(p + 0x3a4) = r0;
        if (dr < df) GT4_F32(p + 0x3a4) = r0 - (df - dr);
        else if (df < dr) GT4_F32(p + 0x34c) = f0 - (dr - df);
    }
    finish_params(p, rec, entry);
}
