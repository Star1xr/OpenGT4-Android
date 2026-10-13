/* Driving aids (func_003F79A0): the understeer and oversteer measures
   (func_003F71E8), traction control on the throttle (func_003F9D00), ABS
   and stability control on each wheel's brake (func_003F9870), and the
   throttle limits for speed limits, pit lane and empty tank.

   Aid settings: params + 0x1098 for the player's car, the driver profile +
   0x24 for cars the model drives: +0 / +8 stability gains, +4 / +0x14
   thresholds, +0x10 / +0x18 brake gains, +0x20 traction control level,
   +0x24 / +0x28 grip-use fade, +0x2c scale, bytes +0x30.. switches. */
#include "car.h"

#include <math.h>

#define K(bits) gt4_f32(bits)
#define F(o) GT4_F32(car + (o))

#define AID_TABLE 0x00620cd8u   /* global switches and levels */

enum {
    CAR_UNDERSTEER      = 0x574,   /* 0..1, filtered (rate at +0x578) */
    CAR_OVERSTEER       = 0x57c,   /* rate at +0x580 */
    CAR_ABS_ON          = 0x6fc,   /* u8: 1 available, 2 acting */
    CAR_TCS_ON          = 0x6fd,   /* u8 */
    CAR_TCS_THROTTLE    = 0x748,   /* filtered throttle cap (rate at +0x74c) */
};

static float ps2_sqrt(float x) { return x == 0 ? x : sqrtf(fabsf(x)); }
static float clamp01(float x) { return gt4_select(x < 0, 0.0f, gt4_select(1.0f < x, 1.0f, x)); }

/* func_00359778: table lookup searched from the end; 0 once a value is not
   positive. */
static float curve_desc(uint32_t c, float x)
{
    int n = (int16_t)GT4_U16(c), k;
    uint32_t xs = GT4_U32(c + 8), ys = GT4_U32(c + 4);
    float y = GT4_F32(ys + (uint32_t)(n - 1) * 4);
    if (x <= GT4_F32(xs + (uint32_t)(n - 1) * 4)) return y;
    for (k = n - 1; k > 0; k--) {
        float yk = GT4_F32(ys + (uint32_t)k * 4), xa = GT4_F32(xs + (uint32_t)(k - 1) * 4);
        if (yk <= 0) return 0;
        if (!(xa < x)) {
            float xb = GT4_F32(xs + (uint32_t)k * 4), t = (x - xb) / (xa - xb);
            return GT4_F32(ys + (uint32_t)(k - 1) * 4) * t + yk * (1.0f - t);
        }
        y = GT4_F32(ys + (uint32_t)(k - 1) * 4);
    }
    return y;
}

/* func_003596C0: the same from the start; 0 once a value is not negative. */
static float curve_asc(uint32_t c, float x)
{
    int n = (int16_t)GT4_U16(c);
    uint32_t xs = GT4_U32(c + 8), ys = GT4_U32(c + 4);
    float y = GT4_F32(ys);
    if (x <= GT4_F32(xs)) return y;
    for (int i = 0; i < n - 1; i++) {
        float yi = GT4_F32(ys + (uint32_t)i * 4), xb = GT4_F32(xs + (uint32_t)(i + 1) * 4);
        if (0 <= yi) return 0;
        if (!(xb < x)) {
            float xa = GT4_F32(xs + (uint32_t)i * 4), t = (x - xa) / (xb - xa);
            return GT4_F32(ys + (uint32_t)(i + 1) * 4) * t + yi * (1.0f - t);
        }
        y = GT4_F32(ys + (uint32_t)(i + 1) * 4);
    }
    return y;
}

/* func_003F71E8: understeer (yaw rate short of what the steering asks) and
   oversteer (rear slip past a speed-dependent angle), filtered. */
static void balance(uint32_t car)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), layout = gt4_car_layout(car);
    float over = 0, under = 0, grip = 0, speed = F(CAR_SPEED);
    int n = GT4_U8(layout + LAYOUT_WHEELS);
    uint8_t mode;

    if (!(speed <= 0)) {
        float steer, yaw, yacc;
        for (int i = 0; i < n; i++)
            grip = grip + GT4_F32(gt4_tyre_params(params, i, GT4_U8(gt4_car_wheel(car, i) + WHEEL_SURFACE)));
        steer = F(0x56c);
        yaw = F(CAR_YAW_RATE);
        yacc = F(CAR_YAW_ACCEL);
        if (0 <= steer * yaw) {
            float want;
            if (steer < 0) { steer = -steer; yaw = -yaw; yacc = -yacc; }
            want = (speed * gt4_cosf(steer)) / GT4_F32(params + 0x18);
            if (0 < want && K(0x3e4ccccc) < fabsf(F(CAR_LAT_ACCEL)) / grip) {
                if (0 < yacc * yaw) yaw = yaw + yacc * 0.5f;
                if (yaw < want) under = 1.0f - yaw / want;
            }
            mode = GT4_U8(car + CAR_56A);
            if (!((uint8_t)(mode - 1) < 2) && !GT4_U8(car + CAR_56B)
                && (int8_t)GT4_U8(car + 0x55) && GT4_U8(AID_TABLE + 0x134)) {
                under = under * GT4_F32(0x006202a4u);
                if (1.0f < under) under = 1.0f;
            }
        }
        mode = GT4_U8(car + CAR_56A);
        if (!((uint8_t)(mode - 1) < 2) && !GT4_U8(car + CAR_56B)) {
            float v, lo, hi, limit;
            for (int i = 0; i < n; i++)
                if ((int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i) < 0) {
                    float s = GT4_F32(gt4_car_wheel(car, i) + WHEEL_SLIP);
                    if (s < 0) s = -s;
                    if (over < s) over = s;
                }
            v = ps2_sqrt(speed * speed + F(CAR_LAT_SPEED) * F(CAR_LAT_SPEED)) * K(0x3c9374bdu);
            if (1.0f < v) v = 1.0f;
            lo = (float)(int32_t)GT4_U8(AID_TABLE + 0xdd) * K(0x3c8efa34u);
            hi = (float)(int32_t)GT4_U8(AID_TABLE + 0xde) * K(0x3c8efa34u);
            limit = v * (hi - lo) + lo;
            if (limit <= over) {
                float a = (float)(int32_t)GT4_U8(AID_TABLE + 0xdf) * K(0x3dccccccu);
                float b = (float)(int32_t)GT4_U8(AID_TABLE + 0xe0) * K(0x3dccccccu);
                over = ((v * (b - a) + a) * (over - limit)) / (K(0x3fc90fd9u) - limit);
                if ((int8_t)GT4_U8(car + 0x55) && GT4_U8(AID_TABLE + 0x135)) over = over * GT4_F32(0x006202a8u);
                if (1.0f < over) over = 1.0f;
            }
        } else {
            float v = ps2_sqrt(speed * speed + F(CAR_LAT_SPEED) * F(CAR_LAT_SPEED));
            if (!(v < K(0x40de38e2u))) {
                float a = v * F(CAR_YAW_RATE), b = F(CAR_LAT_ACCEL);
                if (!(0 <= a * b)) {
                    if (a < 0) a = -a;
                    if (b < 0) b = -b;
                    a = gt4_select(a - b < 0, 0.0f, a - b);
                    over = (a + a) / grip;
                }
            }
        }
    }
    {
        float dt = F(CAR_DT);
        if (GT4_U8(AID_TABLE + 0x136)) {
            float r = F(CAR_UNDERSTEER + 4);
            r = r + (((under - F(CAR_UNDERSTEER)) * 500.0f) - r * 20.0f) * dt;
            F(CAR_UNDERSTEER + 4) = r;
            under = F(CAR_UNDERSTEER) + r * dt;
        }
        if (GT4_U8(AID_TABLE + 0x137)) {
            float r = F(CAR_OVERSTEER + 4);
            r = r + (((over - F(CAR_OVERSTEER)) * 500.0f) - r * 20.0f) * dt;
            F(CAR_OVERSTEER + 4) = r;
            over = F(CAR_OVERSTEER) + r * dt;
        }
        F(CAR_OVERSTEER) = over;
        F(CAR_UNDERSTEER) = under;
    }
}

/* func_003F9CF0 */
static void tcs_off(uint32_t car)
{
    F(CAR_TCS_THROTTLE) = F(CAR_THROTTLE);
    GT4_U32(car + CAR_TCS_THROTTLE + 4) = 0;
}

/* func_003F9C80: 1 below `from` of |grip use|, falling at `rate`, 0 past. */
static float grip_fade(float g, float rate, float from)
{
    if (g < 0) g = -g;
    g = g - from;
    if (g < 0) return 1.0f;
    g = g * rate;
    if (1.0f < g) return 0.0f;
    return 1.0f - g;
}

/* func_003F9D00: traction control: the throttle eased back when the driven
   wheels spin past the tyre's best slip. */
static void traction_control(uint32_t car, uint32_t s)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), layout;
    float fade = 1.0f, slip = 1.0f, best = 1.0f, factor = 1.0f, cut, speed;
    int n, drive;

    GT4_U8(car + CAR_TCS_ON) = GT4_F32(s + 0x20) != 0;
    if (!GT4_U8(car + CAR_TCS_ON)) { tcs_off(car); return; }
    if (!GT4_U8(AID_TABLE + 0x117)) fade = grip_fade(F(CAR_GRIP_USE), GT4_F32(s + 0x24), GT4_F32(s + 0x28));
    if (F(CAR_ENGINE_TORQUE) <= 0) return;
    if (!GT4_U8(car + CAR_CLUTCH_STATE)) { tcs_off(car); return; }
    if (!(0 < F(CAR_THROTTLE))) return;
    layout = gt4_car_layout(car);
    n = GT4_U8(layout + LAYOUT_WHEELS);
    drive = GT4_U8(params + PARAMS_DRIVE);
    for (int i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i);
        int8_t pos = (int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i);
        int driven = drive == 1 ? pos > 0 : (drive == 0 || (drive >= 5 && drive < 8)) ? pos < 0 : 1;
        if (!driven || !(0 < GT4_F32(w + WHEEL_LOAD))) continue;
        if (!(GT4_F32(w + WHEEL_38) < slip)) continue;
        {
            uint32_t tp;
            slip = GT4_F32(w + WHEEL_38);
            tp = gt4_tyre_params(params, i, GT4_U8(w + WHEEL_SURFACE));
            factor = GT4_F32(w + WHEEL_3C);
            best = GT4_F32(tp + 0x164);
            if (GT4_U8(AID_TABLE + 0x117) == 1) {
                float b = curve_asc(tp + 0x170, GT4_F32(w + 0x5c));
                if (best < b) best = b;
            }
        }
    }
    if (!GT4_U8(AID_TABLE + 0x117)) best = best * fade;
    best = best * GT4_F32(s + 0x2c);
    if ((GT4_U32(car + 0x568) & 0xffff0000u) == 0x10000u)
        best = best * GT4_F32(0x006220e0u + GT4_U8(car + 0x81) * 4u);   /* func_003F1E40 */
    speed = F(CAR_SPEED);
    if (GT4_U8(AID_TABLE + 0x146)) {
        float from = (float)(int32_t)GT4_U8(AID_TABLE + 0x147) * K(0x4031c71bu);
        if (from < speed) {
            float to = (float)(int32_t)GT4_U8(AID_TABLE + 0x148) * K(0x4031c71bu);
            float k = (float)(int32_t)GT4_U8(AID_TABLE + 0x149) * K(0x3c23d70au) + 1.0f;
            if (speed < to) k = k * ((speed - from) / (to - from));
            best = best * k;
        }
    }
    cut = clamp01((slip - best) * (GT4_F32(s + 0x20) * factor) + 1.0f);
    if (!(speed < K(0x41de38e2u))) {
        /* above 100 km/h the cap follows on a spring */
        float target = (F(CAR_THROTTLE) * cut) * K(0x3f8ccccc), v = F(CAR_TCS_THROTTLE + 4), c;
        v = v + (((target - F(CAR_TCS_THROTTLE)) * 500.0f) - v * 50.0f) * K(0x3c888888u);
        F(CAR_TCS_THROTTLE + 4) = v;
        c = F(CAR_TCS_THROTTLE) + v * K(0x3c888888u);
        F(CAR_TCS_THROTTLE) = c;
        if (F(CAR_THROTTLE) < c) { F(CAR_TCS_THROTTLE) = F(CAR_THROTTLE); GT4_U32(car + CAR_TCS_THROTTLE + 4) = 0; }
        else if (c < 0) { GT4_U32(car + CAR_TCS_THROTTLE) = 0; GT4_U32(car + CAR_TCS_THROTTLE + 4) = 0; }
        if (F(CAR_TCS_THROTTLE) < F(CAR_THROTTLE)) {
            GT4_U8(car + CAR_TCS_ON) = 2;
            F(CAR_THROTTLE) = F(CAR_TCS_THROTTLE);
        }
        return;
    }
    if (cut < 1.0f) GT4_U8(car + CAR_TCS_ON) = 2;
    F(CAR_THROTTLE) = F(CAR_THROTTLE) * cut;
    tcs_off(car);
}

/* func_003F9470: whether the ABS or stability control is available. */
static int abs_available(uint32_t s)
{
    if (GT4_U16(s + 0x30)) return 1;
    if (!GT4_U8(0x006224a6u)) return 0;
    if (GT4_U8(s + 0x32)) return 1;
    if (GT4_U8(0x006224a8u)) return 0;
    return GT4_U8(s + 0x33) != 0;
}

/* The ABS work area (func_003F92D0 sets it up): [0..1] brake per axle,
   [2..3] slip limit per axle, then per axle and side: [4..7] slip factor,
   [8..11] brake factor, [12..15] brake added. */
typedef struct { float axle[2], slip[2], side_slip[4], side_brake[4], side_add[4]; } abs_work;

/* func_003F94C0: stability control: under- or oversteer brakes one side
   (added brake when coasting, reduced brake when braking). */
static void stability(uint32_t car, uint32_t s, abs_work *a, int on)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS);
    int flags = 0, inside, outside;
    float under, over;
    for (int k = 0; k < 2; k++) a->slip[k] = a->slip[k] * GT4_F32(params + 0x102c + (uint32_t)k * 4);
    if (!on) return;
    inside = 0 < F(CAR_YAW_RATE) ? 0 : 1;
    outside = 1 - inside;
    under = F(CAR_UNDERSTEER);
    over = F(CAR_OVERSTEER);
    if (F(CAR_BRAKE_PEDAL) == 0) {
        if (GT4_U8(s + 0x30)) {
            float b = (over - GT4_F32(s + 4)) * GT4_F32(s);
            if (0 < b) {
                flags = 1;
                if (GT4_U8(s + 0xc)) a->side_add[outside] = a->side_add[outside] + b;
                if (GT4_U8(s + 0xd)) a->side_add[2 + outside] = a->side_add[2 + outside] + b;
            }
        }
        if ((GT4_U8(0x006224a7u) || F(CAR_THROTTLE_REQ) == 0) && GT4_U8(s + 0x32)) {
            float b = (under - GT4_F32(s + 0x14)) * GT4_F32(s + 0x10);
            if (0 < b) {
                if (GT4_U8(0x006224a6u)) flags |= 2;
                if (GT4_U8(s + 0x1c)) a->side_add[inside] = a->side_add[inside] + b;
                if (GT4_U8(s + 0x1d)) a->side_add[2 + inside] = a->side_add[2 + inside] + b;
            }
        }
    } else {
        if (GT4_U8(s + 0x31)) {
            float f = gt4_select(1.0f - (over - K(0x3dccccccu)) * GT4_F32(s + 8) < 0, 0.0f,
                                 1.0f - (over - K(0x3dccccccu)) * GT4_F32(s + 8));
            if (f < 1.0f) {
                flags = 1;
                if (GT4_U8(s + 0xe)) { a->side_slip[inside] = a->side_slip[inside] * f; a->side_brake[inside] = a->side_brake[inside] * f; }
                if (GT4_U8(s + 0xf)) { a->side_slip[2 + inside] = a->side_slip[2 + inside] * f; a->side_brake[2 + inside] = a->side_brake[2 + inside] * f; }
            }
        }
        if (GT4_U8(s + 0x33)) {
            float f = gt4_select(1.0f - (under - K(0x3dccccccu)) * GT4_F32(s + 0x18) < 0, 0.0f,
                                 1.0f - (under - K(0x3dccccccu)) * GT4_F32(s + 0x18));
            if (f < 1.0f) {
                if (GT4_U8(0x006224a6u) && !GT4_U8(0x006224a8u)) flags |= 2;
                if (GT4_U8(s + 0x1e)) { a->side_slip[outside] = a->side_slip[outside] * f; a->side_brake[outside] = a->side_brake[outside] * f; }
                if (GT4_U8(s + 0x1f)) { a->side_slip[2 + outside] = a->side_slip[2 + outside] * f; a->side_brake[2 + outside] = a->side_brake[2 + outside] * f; }
            }
        }
    }
    if ((flags & 2) && GT4_U8(0x00620e10u) && !((uint8_t)(GT4_U8(car + CAR_56A) - 1) < 2) && !GT4_U8(car + CAR_56B)) {
        float k = gt4_select(1.0f - under * GT4_F32(0x006202a0u) < 0, 0.0f, 1.0f - under * GT4_F32(0x006202a0u));
        F(CAR_THROTTLE) = F(CAR_THROTTLE) * k;
    }
    if (flags) GT4_U8(car + CAR_ABS_ON) = 2;
    else if (abs_available(s)) GT4_U8(car + CAR_ABS_ON) = 1;
}

/* func_003F9340: the axle and side factors onto each wheel's brake and slip
   limit. */
static void apply_abs(uint32_t car, abs_work *a, float *limit)
{
    uint32_t layout = gt4_car_layout(car);
    for (int i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        uint32_t w = gt4_car_wheel(car, i);
        uint8_t axle = GT4_U8(layout + 0x14 + i), side, k;
        if (axle >= 2) continue;
        GT4_F32(w + 0x64) = GT4_F32(w + 0x64) * a->axle[axle];
        limit[i] = limit[i] * a->slip[axle];
        side = GT4_U8(layout + LAYOUT_SIDE + i);
        if (side >= 2) continue;
        k = (uint8_t)(side | axle * 2);
        limit[i] = limit[i] * a->side_slip[k];
        GT4_F32(w + 0x64) = GT4_F32(w + 0x64) * a->side_brake[k];
        GT4_F32(w + 0x64) = GT4_F32(w + 0x64) + a->side_add[k];
        if (1.0f < GT4_F32(w + 0x64)) GT4_F32(w + 0x64) = 1.0f;
    }
}

/* func_003F9870: ABS: each wheel's brake eased as its slip passes the
   tyre's limit (lowered with steering), after the stability control. */
static void anti_lock(uint32_t car, uint32_t s, int on)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), layout = gt4_car_layout(car);
    int n = GT4_U8(layout + LAYOUT_WHEELS);
    abs_work a;
    float limit[4] = { 0, 0, 0, 0 };
    a.axle[0] = a.axle[1] = a.slip[0] = a.slip[1] = 1.0f;
    for (int k = 0; k < 4; k++) { a.side_slip[k] = 1.0f; a.side_brake[k] = 1.0f; a.side_add[k] = 0; }
    for (int i = 0; i < n; i++)
        limit[i] = GT4_F32(gt4_tyre_params(params, i, GT4_U8(gt4_car_wheel(car, i) + WHEEL_SURFACE)) + 0x168);
    for (int i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i), tp;
        uint8_t mix = GT4_U8(AID_TABLE + 0x115 + GT4_U8(layout + 0x14 + i));
        if (!mix) continue;
        tp = gt4_tyre_params(params, i, GT4_U8(w + WHEEL_SURFACE));
        if (GT4_U8(tp + 0x1d8) != 1) continue;
        {
            float c = curve_desc(tp + 0x170, GT4_F32(w + 0x5c)), l = limit[i];
            if (c < l) {
                float f = (float)(int32_t)mix / 100.0f;
                limit[i] = f * c + (1.0f - f) * l;
            }
        }
    }
    {
        int setting = GT4_U8(GT4_U32(car + CAR_SESSION) + 0xcbdd);
        setting = setting == 0xff ? 0 : setting;
        if ((setting == 2 || setting == 3) && GT4_U8(AID_TABLE + 0xe5)) {   /* func_0034C998 */
            float st = F(0x56c), full = (float)(int32_t)GT4_U8(AID_TABLE + 0xe5) * K(0x3c8efa34u), f;
            if (st < 0) st = -st;
            f = full <= st ? 1.0f : st / full;
            for (int k = 0; k < 2; k++)
                a.axle[k] = a.axle[k] * (1.0f - f * ((float)(int32_t)GT4_U8(AID_TABLE + 0xe6 + k) * K(0x3c23d70au)));
        }
    }
    stability(car, s, &a, on);
    if (GT4_U8(params + 0x32) == 2)
        for (int k = 0; k < 4; k++) {
            uint32_t w = car + CAR_WHEELS + (uint32_t)k * WHEEL_SIZE;
            GT4_U8(w + 0x88) = 0;
            if (!(a.side_slip[k] == 1.0f) || !(a.side_brake[k] == 1.0f) || !(a.side_add[k] == 0)) GT4_U8(w + 0x88) = 1;
        }
    apply_abs(car, &a, limit);
    for (int i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i);
        int axle = i < n ? GT4_U8(layout + 0x14 + i) : 0;
        float brake = GT4_F32(w + 0x64), f;
        if (brake == 0) continue;
        f = (GT4_F32(w + 0x4c) * GT4_F32(params + 0x1034 + (uint32_t)axle * 4))
            * (1.0f - (GT4_F32(w + WHEEL_38) - limit[i]) * (GT4_F32(params + 0x1024 + (uint32_t)axle * 4) * GT4_F32(w + WHEEL_3C)));
        f = clamp01(f) * brake;
        if (f == 0) GT4_F32(w + 0x64) = K(0x3a83126eu);
        else GT4_F32(w + 0x64) = f;
    }
}

/* func_003F76E8: throttle that holds the speed at `limit` (m/s). */
static float speed_limit(uint32_t car, float limit)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS);
    float v = F(CAR_SPEED), tq = F(CAR_ENGINE_TORQUE), eb = F(CAR_ENGINE_BRAKE), sum = tq + eb, r, t;
    int gear = GT4_U8(car + CAR_GEAR);
    limit = limit + K(0x3e0e38e3u);
    if (!gear) v = -v;
    if (sum <= 0) return 1.0f;
    r = GT4_F32(params + 0x29c + (uint32_t)gear * 4);
    t = (F(0x6c0) + eb * r) / (sum * r) - (v - limit) / K(0x3fb1c71bu);
    if (1.0f < t) return 1.0f;
    return gt4_select(t < 0, 0.0f, t);
}

/* func_003F7880: the pit lane limit ahead (km/h in *out); 1 inside, 2 before. */
static int pit_limit(uint32_t car, uint32_t *out)
{
    uint32_t session = GT4_U32(car + CAR_SESSION), block = session + 0xf818u, limit;
    if ((int16_t)GT4_U16(car + 0x5b0) > 0 || GT4_U8(car + CAR_56B) != 1) return 0;
    limit = GT4_U8(block + 0x6e);
    if (!limit) {
        limit = gt4_vcall(session, 0x10140u, 0x30, session, 0, 0) ? GT4_U8(block + 0x4e)
                : GT4_U8(session + GT4_U8(car + CAR_PLAYER) + 0xf866u);
        if (!limit) return 0;
    }
    if (out) *out = limit;
    {
        float left = GT4_F32(GT4_U32(GT4_U32(GT4_U32(session + 0xcbd8u) + 4) + 0xdc) + 0x14) - F(0x5a8);
        if ((float)(int32_t)(limit * 3) * K(0x3e8e38e3u) < left) return 1;
    }
    if (out) *out = 0;
    return 2;
}

void gt4_car_aids(uint32_t car)
{
    int8_t mode;
    uint32_t block, limit;
    balance(car);
    mode = (int8_t)GT4_U8(car + CAR_56A);
    GT4_U8(car + CAR_TCS_ON) = 0;
    GT4_U8(car + CAR_ABS_ON) = 0;
    if (mode < 3) {
        if (mode > 0) {
            uint32_t s = GT4_U32(GT4_U32(car + CAR_SESSION) + GT4_U8(car + CAR_PLAYER) * 4u + 0x10024u) + 0x24;   /* func_00463BE0 */
            traction_control(car, s);
            anti_lock(car, s, GT4_U8(car + CAR_CLUTCH_STATE) != 2);
            GT4_U8(car + CAR_TCS_ON) = 0;
            GT4_U8(car + CAR_ABS_ON) = 0;
        } else if (mode == 0) {
            if (!GT4_U8(car + CAR_56B) || GT4_U16(car + CAR_5BE)) {
                uint32_t s = GT4_U32(car + CAR_PARAMS) + 0x1098;
                traction_control(car, s);
                anti_lock(car, s, F(CAR_HANDBRAKE) < K(0x3dccccccu));
            } else {
                uint32_t s = GT4_U32(GT4_U32(car + CAR_SESSION) + GT4_U8(car + CAR_PLAYER) * 4u + 0x10024u) + 0x24;
                uint32_t own = GT4_U32(car + CAR_PARAMS) + 0x1098;
                traction_control(car, s);
                anti_lock(car, s, GT4_U8(car + CAR_CLUTCH_STATE) != 2);
                GT4_U8(car + CAR_TCS_ON) = GT4_F32(own + 0x20) != 0;
                GT4_U8(car + CAR_ABS_ON) = (uint8_t)abs_available(own);
            }
        }
    }
    block = GT4_U32(car + CAR_SESSION) + 0xf818u;
    limit = GT4_U8(block + ((int8_t)GT4_U8(car + CAR_56A) == 2 ? 0x43 : 0x42));
    {
        uint16_t t = GT4_U16(car + 0x712);                     /* func_0035FC68 */
        GT4_U16(car + 0x712) = (uint16_t)(t - (t != 0));
        if (t) {
            uint32_t v = GT4_U8(block + 0x4b);
            if (!limit || (int32_t)v < (int32_t)limit) limit = v;
        }
    }
    if (0 < F(0x5d0) && F(0x5d4) <= 0) {                        /* func_00354E68: out of fuel */
        uint32_t v = GT4_U8(block + 0x4c);
        if (!limit || (int32_t)v < (int32_t)limit) limit = v;
    }
    if ((int8_t)GT4_U8(car + 0x6c7)) limit = 10;
    {
        uint32_t pit = 0;
        if (pit_limit(car, &pit) == 1) limit = pit;
    }
    if (limit) {
        float t = speed_limit(car, (float)(int32_t)limit * K(0x3e8e38e3u));
        if (t < F(CAR_THROTTLE)) F(CAR_THROTTLE) = t;
    }
    if (GT4_U8(car + 0x5e9) & 2) {
        int8_t m = (int8_t)GT4_U8(car + CAR_56A);
        if ((m == 0 || m == 3) && !GT4_U8(car + CAR_56B)) {
            float t = speed_limit(car, F(CAR_SPEED)), g = F(CAR_GRIP_USE);
            g = clamp01((g * g) * 1.5f - K(0x3e199999u));
            t = t * g;
            if (F(CAR_THROTTLE_REQ) < t) F(CAR_THROTTLE) = t;
        }
    }
}
