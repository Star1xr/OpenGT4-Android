/* Tyre grip per wheel (func_00357418): the force limit of each tyre from its
   load, camber and the surface. */
#include "car.h"

#define K(bits) gt4_f32(bits)

#include <math.h>

/* Axle of wheel i (0 front, 1 rear) from the drive layout. */
int gt4_wheel_axle(uint32_t layout, int i)
{
    return i < GT4_U8(layout + LAYOUT_WHEELS) ? GT4_U8(layout + LAYOUT_AXLE + i) : 0;
}

/* func_00360F80: tyre parameters for wheel i on a surface kind. */
uint32_t gt4_tyre_params(uint32_t params, int i, uint8_t surface)
{
    uint32_t layout = GT4_LAYOUTS + GT4_U8(params + PARAMS_LAYOUT) * 84u;
    uint8_t tyre = GT4_U8(params + PARAMS_TYRE_INDEX + gt4_wheel_axle(layout, i) * 24 + surface);
    return params + PARAMS_TYRES + tyre * TYRE_SIZE;
}

/* func_00356EF0: tyre wear and heat into WHEEL_GRIP_FACTOR, when the
   session enables them (flags 0x68 wear, 0x69 heat of the block s at
   *(car+4) + 4). Wear (wheel+0xAC) goes through the marks s+0, s+8, s+0x10,
   s+0x18, each band losing grip linearly (s+4, s+0xC, s+0x14, s+0x1C at its
   end); heat (wheel+0xA8) the same about s+0x58 (below: s+0x50, s+0x54;
   above: up to s+0x60, s+0x5C to s+0x64). wheel+0xB1 is the gauge,
   -128..127. The factor never drops below the tyre's floor (+0x68).
   Returns whether the factors apply. */
static int tyre_condition(uint32_t car, uint32_t s)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), layout = gt4_car_layout(car);
    const float one = K(0x3f800000u), band_hi = K(0x427f9999u), band_lo = K(0x41ff3333u);
    int wear = GT4_U8(s + 0x68) != 0, heat = GT4_U8(s + 0x69) != 0, n;
    if (!wear && !heat) return 0;
    if (GT4_U8(car + 0x104 + 0x4b1) & 0x10) return 0;
    n = GT4_U8(layout + LAYOUT_WHEELS);
    if (wear) {
        for (int i = 0; i < n; i++) {
            uint32_t w = gt4_car_wheel(car, i);
            float x = GT4_F32(w + 0xac), f;
            int gauge;
            if (GT4_F32(s + 0x18) <= x) {
                GT4_F32(w + 0xac) = GT4_F32(s + 0x18);
                GT4_F32(w + WHEEL_GRIP_FACTOR) = one - GT4_F32(s + 0x1c);
                gauge = 0x7f;
            } else if (GT4_F32(s + 0x10) <= x) {
                f = (x - GT4_F32(s + 0x10)) / (GT4_F32(s + 0x18) - GT4_F32(s + 0x10));
                GT4_F32(w + WHEEL_GRIP_FACTOR) = (one - GT4_F32(s + 0x14)) - f * (GT4_F32(s + 0x1c) - GT4_F32(s + 0x14));
                gauge = one <= f ? 0x7f : (int32_t)(f * band_hi) + 0x40;
            } else if (GT4_F32(s + 8) <= x) {
                f = (x - GT4_F32(s + 8)) / (GT4_F32(s + 0x10) - GT4_F32(s + 8));
                GT4_F32(w + WHEEL_GRIP_FACTOR) = (one - GT4_F32(s + 0xc)) - f * (GT4_F32(s + 0x14) - GT4_F32(s + 0xc));
                gauge = one <= f ? 0x40 : (int32_t)(f * band_lo) + 0x20;
            } else if (0 <= x) {
                f = x / GT4_F32(s + 8);
                GT4_F32(w + WHEEL_GRIP_FACTOR) = one - GT4_F32(s + 0xc) * f;
                gauge = one <= f ? 0x20 : (int32_t)(f * band_lo);
            } else if (GT4_F32(s + 0) <= x) {
                if (heat) {
                    GT4_F32(w + WHEEL_GRIP_FACTOR) = one;
                    continue;
                }
                f = GT4_F32(w + 0xac) / GT4_F32(s + 0);
                GT4_F32(w + WHEEL_GRIP_FACTOR) = one - GT4_F32(s + 4) * f;
                gauge = one <= f ? -0x3f : -(int32_t)(f * band_hi);
            } else {
                GT4_F32(w + WHEEL_GRIP_FACTOR) = one;
                gauge = -0x80;
            }
            if (!heat) GT4_U8(w + 0xb1) = (uint8_t)gauge;
        }
    }
    if (heat) {
        for (int i = 0; i < n; i++) {
            uint32_t w = gt4_car_wheel(car, i);
            float t = GT4_F32(w + 0xa8), mid = GT4_F32(s + 0x58), g = one, f;
            int gauge;
            if (mid < t) {
                f = (t - mid) / (GT4_F32(s + 0x60) - mid);
                if (one < f) f = one;
                gauge = (int32_t)(f * band_hi) + 0x40;
                g = (one - GT4_F32(s + 0x5c)) - (GT4_F32(s + 0x64) - GT4_F32(s + 0x5c)) * f;
            } else if (0 <= t) {
                f = t / mid;
                gauge = (int32_t)(f * band_hi);
                g = one - GT4_F32(s + 0x5c) * f;
            } else if (GT4_F32(s + 0x50) <= t) {
                f = t / GT4_F32(s + 0x50);
                gauge = -(int32_t)(f * band_hi);
                g = one - GT4_F32(s + 0x54) * f;
            } else {
                gauge = -0x80;
            }
            GT4_U8(w + 0xb1) = (uint8_t)gauge;
            if (wear) GT4_F32(w + WHEEL_GRIP_FACTOR) = GT4_F32(w + WHEEL_GRIP_FACTOR) * g;
            else GT4_F32(w + WHEEL_GRIP_FACTOR) = g;
        }
    }
    for (int i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i), tyre = gt4_tyre_params(params, i, GT4_U8(w + 0x89));
        if (GT4_F32(w + WHEEL_GRIP_FACTOR) < GT4_F32(tyre + 0x68)) GT4_F32(w + WHEEL_GRIP_FACTOR) = GT4_F32(tyre + 0x68);
    }
    return 1;
}

int gt4_tyre_condition(uint32_t car, uint32_t s) { return tyre_condition(car, s); }

void gt4_car_tyre_grip(uint32_t car)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS);
    uint32_t layout = gt4_car_layout(car);
    int condition = tyre_condition(car, GT4_U32(car + CAR_SESSION) + 4);
    float roll = GT4_F32(car + CAR_ANGLES + 4) - GT4_F32(car + CAR_67C);

    for (int i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        uint32_t w = gt4_car_wheel(car, i);
        uint8_t surface = GT4_U8(w + WHEEL_SURFACE);
        uint32_t tyre = gt4_tyre_params(params, i, surface);
        int axle = gt4_wheel_axle(layout, i);
        float load, grip, camber;

        GT4_U8(w + WHEEL_DD) = GT4_U8(tyre + TYRE_1DA);
        load = GT4_F32(w + WHEEL_LOAD);
        /* func_003573A8: load relative to the axle's reference */
        grip = load * gt4_curve_eval(gt4_ptr(tyre + TYRE_LOAD_CURVE),
                                     load / GT4_F32(params + PARAMS_LOAD_REF + axle * 4));
        if (condition) grip = grip * GT4_F32(w + WHEEL_GRIP_FACTOR);
        grip = grip * GT4_F32(params + PARAMS_AXLE_GRIP + axle * 4);

        if (GT4_U8(layout + LAYOUT_KIND) == 1) camber = 0;
        else {
            uint8_t side = GT4_U8(layout + LAYOUT_SIDE + i);
            camber = side == 0 ? -roll : side == 1 ? roll : 0;
            camber = camber + GT4_F32(w + WHEEL_CAMBER);
        }
        camber = gt4_curve_eval(gt4_ptr(params + PARAMS_CAMBER_CURVE + axle * 12), camber);
        GT4_F32(w + WHEEL_GRIP) = grip * GT4_F32(tyre + TYRE_MU + surface * 4) * camber;
        GT4_F32(w + WHEEL_GRIP) = GT4_F32(w + WHEEL_GRIP) * gt4_surface_factor(car, surface);
    }
}

/* func_00352218: folds a slip angle into [0, pi/2] and gives the side. */
float gt4_slip_fold(float a, float *side)
{
    const float half_pi = gt4_f32(0x3fc90fdau), pi = gt4_f32(0x40490fdau);
    if (a < -half_pi) { *side = 1; return a + pi; }
    if (a < 0) { *side = -1; return -a; }
    if (a < half_pi) { *side = 1; return a; }
    *side = -1;
    return pi - a;
}

#define WORLD_MODE  0x00845bf0u   /* func_003F7C80 */
#define CAMBER_RANGE 0x00620764u  /* degrees */
#define CAMBER_FORCE 0x00620768u
#define AVERAGE_SLIP 0x00623e52u  /* byte: store the average slip of the front wheels */

/* func_00352408: side force of each tyre from its slip (the angle between
   where the wheel points and where it moves), limited by the grip. */
void gt4_car_tyre_forces(uint32_t car)
{
    const float one = gt4_f32(0x3f800000u);
    uint32_t params = GT4_U32(car + CAR_PARAMS);
    uint32_t layout = gt4_car_layout(car);
    float sum = 0;
    int count = 0;

    for (int i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        uint32_t w = gt4_car_wheel(car, i);
        uint32_t tyre = gt4_tyre_params(params, i, GT4_U8(w + WHEEL_SURFACE));
        float steer, heading, slip, side, lat, camber, curve;
        uint32_t mode;

        GT4_F32(w + WHEEL_SLIP) = 0;
        if (GT4_F32(w + WHEEL_VEL_WEIGHT) == 0) {
            GT4_F32(w + WHEEL_FORCE_LAT) = 0;
            GT4_F32(w + WHEEL_FORCE_6C) = 0;
            continue;
        }
        steer = GT4_F32(w + WHEEL_STEER);
        mode = GT4_U32(WORLD_MODE);
        if (mode == 1) GT4_F32(w + WHEEL_STEER) = GT4_F32(w + WHEEL_STEER) + GT4_F32(w + WHEEL_STEER_ADD);
        else if (mode == 2) GT4_F32(w + WHEEL_STEER) = GT4_F32(w + WHEEL_STEER_ADD);

        heading = GT4_F32(w + WHEEL_STEER);
        if (GT4_F32(w + WHEEL_VEL_X) < 0) heading = heading + gt4_f32(0x40490fdau);
        slip = gt4_angle_diff(heading, GT4_F32(w + WHEEL_VEL_ANGLE));
        GT4_F32(w + WHEEL_SLIP) = slip;

        if (GT4_U8(tyre + TYRE_BY_VELOCITY)) {
            /* lateral velocity in the wheel's frame instead of the angle */
            float a = GT4_F32(w + WHEEL_STEER);
            slip = gt4_sinf(a) * GT4_F32(w + WHEEL_VEL_Y);
            slip = slip + gt4_cosf(a) * GT4_F32(w + WHEEL_VEL_X);
            slip = slip * GT4_F32(params + PARAMS_SLIP_SCALE);
            side = one;
            if (slip < 0) { slip = -slip; side = -one; }
            if (!GT4_U8(tyre + TYRE_CIRCLE) && GT4_F32(w + WHEEL_SPIN) < 0) side = -side;
        } else {
            slip = gt4_slip_fold(slip, &side);
            if (GT4_U8(tyre + TYRE_CIRCLE) == 1 && GT4_F32(w + WHEEL_SPIN) < 0) side = -side;
        }

        if (i < GT4_U8(layout + LAYOUT_WHEELS) && (int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i) > 0) {
            sum = sum + slip;
            count++;
        }
        curve = gt4_curve_eval(gt4_ptr(tyre + TYRE_SIDE_CURVE), slip);
        GT4_F32(w + WHEEL_SIDE_CURVE) = curve;
        camber = 0;
        lat = side * curve;
        if ((int8_t)GT4_U8(layout + LAYOUT_44 + i) == 2) {
            float roll = GT4_F32(car + CAR_ANGLES + 4) - GT4_F32(car + CAR_67C);
            float c = GT4_U8(layout + LAYOUT_SIDE + i) == 1 ? roll + GT4_F32(w + WHEEL_CAMBER)
                                                            : roll - GT4_F32(w + WHEEL_CAMBER);
            c = c / (GT4_F32(CAMBER_RANGE) * gt4_f32(0x3c8efa34u));   /* degrees to radians */
            if (one < c) c = one;
            else if (c < -one) c = -one;
            camber = c * -GT4_F32(CAMBER_FORCE);
        }
        if (!GT4_U8(tyre + TYRE_BY_VELOCITY)) {
            camber = camber * GT4_F32(w + WHEEL_VEL_WEIGHT);
            lat = lat * GT4_F32(w + WHEEL_VEL_WEIGHT);
        }

        if (GT4_U8(tyre + TYRE_CIRCLE) == 1) {
            float limit = GT4_F32(w + WHEEL_COEF_B);
            if (limit < lat) lat = limit;
            else if (lat < -limit) lat = -limit;
            GT4_F32(w + WHEEL_FORCE_LAT) = lat * GT4_F32(w + WHEEL_GRIP);
            GT4_F32(car + CAR_CAMBER_FORCE + 4 * i) = camber * GT4_F32(w + WHEEL_GRIP);
            GT4_F32(w + WHEEL_FORCE_6C) = 0;
        } else {
            /* friction circle with the second curve */
            float lon = gt4_curve_eval(gt4_ptr(tyre + TYRE_6C_CURVE), slip), mag, limit;
            if (!GT4_U8(tyre + TYRE_BY_VELOCITY)) lon = lon * GT4_F32(w + WHEEL_VEL_WEIGHT);
            mag = sqrtf(lat * lat + lon * lon);
            limit = GT4_F32(w + WHEEL_COEF_B);
            if (limit < mag) {
                float k = limit / mag;
                lon = lon * k;
                lat = lat * k;
            }
            GT4_F32(w + WHEEL_FORCE_LAT) = lat * GT4_F32(w + WHEEL_GRIP);
            GT4_F32(car + CAR_CAMBER_FORCE + 4 * i) = camber * GT4_F32(w + WHEEL_GRIP);
            GT4_F32(w + WHEEL_FORCE_6C) = lon * GT4_F32(w + WHEEL_GRIP);
        }
        GT4_F32(w + WHEEL_STEER) = steer;
    }
    if (GT4_U8(AVERAGE_SLIP))
        GT4_F32(car + CAR_AVERAGE_SLIP) = count < 2 ? sum : sum / (float)count;
}
