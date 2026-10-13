/* Longitudinal slip of each wheel against the ground: the input of the
   tyre coefficient tables (WHEEL_38, -1..1), a factor (WHEEL_3C) and the
   direction flags (WHEEL_44: 1 driving, -1 braking or locked; WHEEL_45)
   (func_00351778 and what it calls). */
#include "car.h"

#include <math.h>

#define K(bits) gt4_f32(bits)

#define SLIP_TABLE 0x00620cd8u   /* +0xb6.. per direction: on, start %, range (in 1/57.6) */
#define SLIP_FADE_BRAKE 0x00620d8cu
#define SLIP_FADE_DRIVE 0x00620d8du

/* func_00351358: factor from start% at rest to 1 at `range` km/h of wheel
   ground speed, for driving (1) or braking (0). */
static float slip_factor(int drive, float v)
{
    uint32_t t = SLIP_TABLE + 0xb0u + (uint32_t)drive;
    float start, range, f;
    if (!GT4_U8(t + 6)) return 1.0f;
    start = (float)(int32_t)GT4_U8(t + 8) * K(0x3c23d70au);
    range = (float)(int32_t)GT4_U8(t + 0xa) * K(0x3e8e38e3u);   /* km/h */
    f = 1.0f - start;
    if (v < 0) v = -v;
    if (v < range) return start + (f * v) / range;
    return 1.0f;
}

/* func_003513F8: slip of one wheel whose ground speed along it is v. */
static void wheel_slip(uint32_t w, uint32_t params, uint32_t tyre, float v)
{
    float spin, s = 0, f = 1.0f, small = K(0x3c8e38e3u);
    int8_t d, e;
    GT4_U8(w + WHEEL_44) = 0;
    spin = GT4_F32(w + WHEEL_SPIN);
    if (GT4_U8(tyre + 0x1d8)) {
        /* plain difference, scaled */
        s = v - spin;
        if (0 <= spin) {
            d = v <= spin ? 1 : -1;
            e = d;
        } else {
            s = -s;
            if (spin <= v) { e = -1; d = 1; } else { e = 1; d = -1; }
        }
        GT4_U8(w + WHEEL_44) = (uint8_t)d;
        GT4_U8(w + WHEEL_45) = (uint8_t)e;
        s = s * GT4_F32(params + 0xfc8);
        GT4_F32(w + WHEEL_3C) = 1.0f;
        if (1.0f < s) s = 1.0f;
        else if (s < -1.0f) s = -1.0f;
        GT4_F32(w + WHEEL_38) = s;
        return;
    }
    if (-small < spin && spin < small) {
        /* wheel at rest */
        f = slip_factor(0, GT4_F32(w + WHEEL_VEL_X));
        if (spin < v) { e = -1; s = 1.0f; }
        else if (v < spin) { e = 1; s = 1.0f; }
        else { e = 0; s = 0; }
        GT4_U8(w + WHEEL_45) = (uint8_t)e;
        d = -1;
    } else if (0 < spin) {
        if (v <= spin) {
            s = v - spin;
            f = slip_factor(1, GT4_F32(w + WHEEL_VEL_X));
            d = 1;
            s = (s * f) / spin;
            e = 1;
        } else {
            f = slip_factor(0, GT4_F32(w + WHEEL_VEL_X));
            if (!(v < small && -small < v)) s = ((v - spin) * f) / v;
            d = e = -1;
        }
        GT4_U8(w + WHEEL_45) = (uint8_t)e;
    } else {
        if (spin <= v) {
            s = v - spin;
            f = slip_factor(1, GT4_F32(w + WHEEL_VEL_X));
            s = (s * f) / spin;
            e = -1;
            d = 1;
        } else {
            f = slip_factor(0, GT4_F32(w + WHEEL_VEL_X));
            if (!(v < small && -small < v)) s = ((v - spin) * f) / v;
            e = 1;
            d = -1;
        }
        GT4_U8(w + WHEEL_45) = (uint8_t)e;
    }
    GT4_U8(w + WHEEL_44) = (uint8_t)d;
    GT4_F32(w + WHEEL_3C) = f;
    if (1.0f < s) s = 1.0f;
    else if (s < -1.0f) s = -1.0f;
    {
        /* faded in over the first km/h of the wheel's (driving) or the
           ground's (braking) speed */
        float x = d >= 0 ? spin : v, lim;
        uint8_t k = GT4_U8(d >= 0 ? SLIP_FADE_DRIVE : SLIP_FADE_BRAKE);
        if (k) {
            lim = (float)(int32_t)k * K(0x3e8e38e3u);
            if (x < 0) x = -x;
            if (x < lim) s = s * (x / lim);
        }
    }
    GT4_F32(w + WHEEL_38) = s;
}

/* func_00351778 */
void gt4_car_slip(uint32_t car)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS);
    int n = GT4_U8(gt4_car_layout(car) + LAYOUT_WHEELS);
    for (int i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i), tyre = gt4_tyre_params(params, i, GT4_U8(w + WHEEL_SURFACE));
        float st = GT4_F32(w + WHEEL_STEER);
        float a = gt4_sinf(st) * GT4_F32(w + WHEEL_VEL_X), b = gt4_cosf(st) * GT4_F32(w + WHEEL_VEL_Y);
        wheel_slip(w, params, tyre, a - b);
    }
}
