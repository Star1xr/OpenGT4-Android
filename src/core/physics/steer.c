/* Steering geometry (func_003F7EA8): each wheel's angle from the steering
   (Ackermann factor per side, rear steer, toe), the steering column's own
   dynamics (settings at D_00845BF0) and the compliance steer the tyre's side
   force causes (D_00620AF0), plus the steering wheel shown in the cockpit
   (func_0040CC58). */
#include "car.h"

#define K(bits) gt4_f32(bits)

#define COLUMN     0x00845bf0u   /* u32 mode (0 off, 1, 2), u32 load scaling, [2] limit, [2] gain, [2] damping */
#define COMPLIANCE 0x00620af0u   /* u8 on; +0x10.. limit, gain, damping; curves at +0x34, +0x50, +0x64 */
#define SURFACE_STEER 0x00620288u

enum {
    WHEEL_COLUMN_RATE = 0x18,
    WHEEL_COLUMN      = 0x1c,
    WHEEL_TOE_OUT     = 0xcc,   /* compliance steer and its rate */
    WHEEL_TOE_RATE    = 0xd0,
};

/* func_003F7CA8: the angle wheel i gets from the steering angle. */
static float wheel_angle(uint32_t params, int i, float steer)
{
    uint32_t layout = GT4_LAYOUTS + GT4_U8(params + PARAMS_LAYOUT) * 84u;
    int n = GT4_U8(layout + LAYOUT_WHEELS);
    int8_t pos = i < n ? (int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i) : 0;
    uint8_t side = GT4_U8(layout + LAYOUT_SIDE + i);
    float a = 0;
    if (pos > 0) {
        a = steer;
        if (side < 2) a = a * GT4_F32(params + 0x10c + (0 <= steer ? side : 1u - side) * 4u);
    } else if (pos < 0 && GT4_U8(params + 0x10d0) == 1) {
        a = -steer * GT4_F32(params + 0x10cc);   /* rear steer */
        if (side < 2) a = a * GT4_F32(params + 0x10c + (0 <= steer ? side : 1u - side) * 4u);
    }
    {
        float toe = GT4_F32(params + 0x1008 + (i < n ? GT4_U8(layout + 0x14 + i) : 0) * 4u);
        if (side == 0) a = a - toe;
        else if (side == 1) a = a + toe;
    }
    return a;
}

/* func_0036CCD8: ka * a + kb * b of two angles, the shorter way round. */
static float angle_blend(float ka, float a, float kb, float b)
{
    const float pi = K(0x40490fdau), two_pi = K(0x40c90fdau);
    float x = gt4_angle_wrap(a), y = gt4_angle_wrap(b), d = x - y;
    if (pi <= d) y = y + two_pi;
    else if (d < K(0xc0490fdau)) x = x + two_pi;
    return gt4_angle_wrap(ka * x + kb * y);
}

/* func_0040CC58: the cockpit's steering wheel follows the steering on a
   spring (car+0x768 angle, +0x76c rate, +0x770 acceleration). */
static void steering_wheel(uint32_t car)
{
    float a = GT4_F32(car + 0x768), v = GT4_F32(car + 0x76c), acc, step = K(0x3c888888u);
    acc = ((GT4_F32(car + 0x56c) * 11.25f - a) * 200.0f) - v * 20.0f;
    GT4_F32(car + 0x770) = acc;
    v = v + acc * step;
    GT4_F32(car + 0x76c) = v;
    GT4_F32(car + 0x768) = a + v * step;
}

void gt4_car_steer_geometry(uint32_t car)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), layout = gt4_car_layout(car);
    int n = GT4_U8(layout + LAYOUT_WHEELS);
    uint32_t mode;

    for (int i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i);
        if (GT4_U8(layout + LAYOUT_KIND) == 1) GT4_F32(w + WHEEL_STEER) = gt4_orig_call(0x003f7e20u, car, (uint32_t)i, 0, 0);
        else GT4_F32(w + WHEEL_STEER) = wheel_angle(params, i, GT4_F32(car + 0x56c));
    }
    steering_wheel(car);

    mode = GT4_U32(COLUMN);
    if (mode) {
        /* the column: each wheel's angle lags on a spring toward where the
           wheel is pointed (or where it rolls, on loose surfaces) */
        float dt = GT4_F32(car + CAR_DT);
        for (int i = 0; i < n; i++) {
            uint32_t w = gt4_car_wheel(car, i);
            int pos = GT4_U8(layout + 0x2c + i);
            int loose = GT4_U8(SURFACE_STEER + GT4_U8(w + WHEEL_SURFACE)) == 1;
            float target, cur = GT4_F32(w + WHEEL_COLUMN), rate = GT4_F32(w + WHEEL_COLUMN_RATE), acc, lim, f = 1.0f;
            uint32_t scale;
            if (mode == 2) {
                if (loose) target = GT4_F32(w + WHEEL_STEER);
                else if (GT4_F32(w + WHEEL_VEL_WEIGHT) == 0) target = cur;
                else if (GT4_F32(w + WHEEL_VEL_X) < 0)
                    target = angle_blend(0.5f, GT4_F32(w + WHEEL_VEL_ANGLE) - K(0x40490fdau), 0.5f, GT4_F32(w + WHEEL_STEER));
                else target = (GT4_F32(w + WHEEL_VEL_ANGLE) + GT4_F32(w + WHEEL_STEER)) * 0.5f;
            } else {
                if (loose) target = 0;
                else if (GT4_F32(w + WHEEL_VEL_WEIGHT) == 0) {
                    float l = GT4_F32(COLUMN + 8 + (uint32_t)pos * 4);
                    target = cur - (GT4_F32(w + WHEEL_STEER) - GT4_F32(car + 0x8e0 + (uint32_t)i * 4)) * 0.5f;
                    if (target < -l) target = -l;
                    else if (l < target) target = l;
                } else if (GT4_F32(w + WHEEL_VEL_X) < 0)
                    target = gt4_angle_diff(GT4_F32(w + WHEEL_VEL_ANGLE) - K(0x40490fdau), GT4_F32(w + WHEEL_STEER)) * 0.5f;
                else target = (GT4_F32(w + WHEEL_VEL_ANGLE) - GT4_F32(w + WHEEL_STEER)) * 0.5f;
            }
            acc = GT4_F32(COLUMN + 0x10 + (uint32_t)pos * 4) * (target - cur) - GT4_F32(COLUMN + 0x18 + (uint32_t)pos * 4) * rate;
            lim = GT4_F32(COLUMN + 8 + (uint32_t)pos * 4);
            scale = GT4_U32(COLUMN + 4);
            if (scale)
                f = (GT4_F32(w + 0x4c) * (float)(int32_t)GT4_U8(layout + 8 + pos))
                    / GT4_F32(params + 0x1040 + (uint32_t)pos * 4);
            if (scale == 2 || scale == 3) acc = acc * f;
            if (scale == 1 || scale == 3) lim = lim * f;
            rate = rate + acc * dt;
            GT4_F32(w + WHEEL_COLUMN_RATE) = rate;
            GT4_F32(w + WHEEL_COLUMN) = cur + rate * dt;
            if (mode == 2) {
                float c = GT4_F32(w + WHEEL_COLUMN), s = GT4_F32(w + WHEEL_STEER), d = c - s;
                if (lim < d) { GT4_U32(w + WHEEL_COLUMN_RATE) = 0; GT4_F32(w + WHEEL_COLUMN) = s + lim; }
                else if (d < -lim) { GT4_U32(w + WHEEL_COLUMN_RATE) = 0; GT4_F32(w + WHEEL_COLUMN) = s - lim; }
            } else {
                float c = GT4_F32(w + WHEEL_COLUMN);
                if (lim < c) { GT4_F32(w + WHEEL_COLUMN) = lim; GT4_U32(w + WHEEL_COLUMN_RATE) = 0; }
                else if (c < -lim) { GT4_F32(w + WHEEL_COLUMN) = -lim; GT4_U32(w + WHEEL_COLUMN_RATE) = 0; }
            }
        }
    }

    if (GT4_U8(COMPLIANCE)) {
        /* compliance steer: the side force turns the wheel, on a spring */
        float dt;
        for (int i = 0; i < n; i++) {
            uint32_t w = gt4_car_wheel(car, i);
            int pos = GT4_U8(layout + 0x2c + i);
            float force = GT4_F32(w + 0x68) * GT4_F32(params + 0x1058 + (uint32_t)pos * 4);
            float load = GT4_F32(params + 0x1050 + (uint32_t)pos * 4)
                         + gt4_curve_eval(gt4_ptr(COMPLIANCE + 0x34), GT4_F32(car + 0x6ec));
            float dead = gt4_curve_eval(gt4_ptr(COMPLIANCE + 0x50), load), toe, cur;
            if (0 <= force) {
                toe = force - dead;
                toe = gt4_select(toe < 0, 0.0f, toe);
            } else {
                toe = force + dead;
                toe = gt4_select(0 < toe, 0.0f, toe);
            }
            if (toe != 0) {
                float cap = GT4_F32(COMPLIANCE + 0x14);
                toe = toe * gt4_curve_eval(gt4_ptr(COMPLIANCE + 0x64), load);
                if (cap < toe) toe = cap;
                else if (toe < -cap) toe = -cap;
                if (pos == 1) toe = -toe;
            }
            cur = GT4_F32(w + WHEEL_TOE_OUT);
            dt = GT4_F32(car + CAR_DT);
            {
                float v = GT4_F32(w + WHEEL_TOE_RATE);
                float acc = GT4_F32(COMPLIANCE + 0x18) * (toe - cur) - GT4_F32(COMPLIANCE + 0x1c) * v;
                v = v + acc * dt;
                GT4_F32(w + WHEEL_TOE_RATE) = v;
                cur = cur + v * dt;
                GT4_F32(w + WHEEL_TOE_OUT) = cur;
                GT4_F32(w + WHEEL_STEER) = GT4_F32(w + WHEEL_STEER) + cur;
            }
        }
    }
}

float gt4_wheel_angle(uint32_t params, int i, float steer) { return wheel_angle(params, i, steer); }
