/* Body motion in the ground plane: yaw from the tyre forces
   (func_00352848) and the velocity update (func_00352C90). */
#include "car.h"

#define K(bits) gt4_f32(bits)

#define WORLD_MODE  0x00845bf0u   /* func_003F7C80 */

/* func_003522B0: at walking pace a tyre holds the car with a small force
   shaped by the wheel spin and the wheel-frame velocity. */
static float creep_force(uint32_t w)
{
    float grip = GT4_F32(w + WHEEL_GRIP), spin, rel, a, v;
    if (grip == 0) return 0;
    spin = GT4_F32(w + WHEEL_SPIN);
    rel = GT4_F32(w + WHEEL_VEL_X) - spin;
    if (spin < K(0xbfb1c71bu) || K(0x3fb1c71bu) < spin) return 0;   /* 5 km/h */
    if (rel < K(0xbf0e38e3u) || K(0x3f0e38e3u) < rel) return 0;     /* 2 km/h */
    a = GT4_F32(w + WHEEL_STEER);
    v = gt4_sinf(a) * GT4_F32(w + WHEEL_VEL_Y) + gt4_cosf(a) * GT4_F32(w + WHEEL_VEL_X);
    if (K(0x3f8e38e3u) < v || v < K(0xbf8e38e3u)) return 0;         /* 4 km/h */
    return gt4_cosf(v * K(0x4034f4abu)) * gt4_sinf(spin * K(0x3f90c3bcu)) * grip;
}

/* func_00350D40: smoothed yaw acceleration (fixed 1/60 step). */
static void yaw_filter(uint32_t car)
{
    const float step = K(0x3c888888u);
    float rate = GT4_F32(car + CAR_YAW_FILTER_RATE);
    float err = GT4_F32(car + CAR_YAW_ACCEL) - GT4_F32(car + CAR_YAW_FILTER);
    rate = rate + (err * K(0x43fa0000u) - rate * K(0x41a00000u)) * step;   /* 500, 20 */
    GT4_F32(car + CAR_YAW_FILTER_RATE) = rate;
    GT4_F32(car + CAR_YAW_FILTER) = GT4_F32(car + CAR_YAW_FILTER) + rate * step;
}

/* func_00467470 with func_00463BB8: grip factor of a surface in some
   driving modes (CAR_56A 1 or 2, or CAR_56B set). */
float gt4_surface_factor(uint32_t car, uint8_t surface)
{
    int8_t mode = (int8_t)GT4_U8(car + CAR_56A);
    uint32_t base, table;
    if (mode != 1 && mode != 2 && !GT4_U8(car + CAR_56B)) return K(0x3f800000u);
    base = car + 0x14;
    table = GT4_U32(GT4_U32(base + 0xe4) + GT4_U8(GT4_U32(base + 0xe8) + 0x544) * 4 + 0x10024);
    return GT4_F32(table + 0xa8 + GT4_U8(GT4_U32(car + CAR_PARAMS) + PARAMS_TYRE_INDEX + surface) * 4);
}

void gt4_car_yaw(uint32_t car, int airborne)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), layout = gt4_car_layout(car);
    float dt = GT4_F32(car + CAR_DT), side = 0, moment = 0, yaw, lat_accel, grip;
    int n = GT4_U8(layout + LAYOUT_WHEELS), i;

    if (airborne) {
        GT4_F32(car + CAR_GRIP_USE) = 0;
        GT4_F32(car + CAR_LAT_ACCEL) = 0;
        for (i = 0; i < n; i++) GT4_F32(gt4_car_wheel(car, i) + WHEEL_SIDE_FORCE) = 0;
        return;
    }

    for (i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        uint32_t w = gt4_car_wheel(car, i);
        float steer = GT4_F32(w + WHEEL_STEER), drive, lat, fx, fy, camber, creep, a, arm, lx;
        uint32_t mode = GT4_U32(WORLD_MODE);
        int idx;

        if (mode == 1) GT4_F32(w + WHEEL_STEER) = GT4_F32(w + WHEEL_STEER) + GT4_F32(w + WHEEL_STEER_ADD);
        else if (mode == 2) GT4_F32(w + WHEEL_STEER) = GT4_F32(w + WHEEL_STEER_ADD);
        a = GT4_F32(w + WHEEL_STEER);
        drive = GT4_F32(w + WHEEL_70) + GT4_F32(w + WHEEL_74);
        lat = GT4_F32(w + WHEEL_FORCE_LAT);

        /* tyre forces into the body frame: fx sideways, fy along */
        if (GT4_U8(w + WHEEL_DD) == 1) {
            fx = -gt4_sinf(a) * lat;
            fy = -gt4_cosf(a) * lat;
        } else {
            float d = GT4_F32(w + WHEEL_VEL_ANGLE), s = gt4_sinf(d), c = gt4_cosf(d), f6c = GT4_F32(w + WHEEL_FORCE_6C);
            fx = -s * lat + c * f6c;
            fy = -c * lat + -s * f6c;
        }
        camber = GT4_F32(car + CAR_CAMBER_FORCE + 4 * i);
        fx = fx + -gt4_sinf(a) * camber;
        fy = fy + -gt4_cosf(a) * camber;
        creep = creep_force(w);
        fx = fx - gt4_sinf(a) * creep;
        fy = fy - gt4_cosf(a) * creep;
        fx = fx - gt4_cosf(a) * drive;
        fy = fy + gt4_sinf(a) * drive;
        GT4_F32(w + WHEEL_SIDE_FORCE) = fx;
        side = side + fx;

        /* moment about the centre of mass */
        idx = GT4_U8(layout + LAYOUT_POSITION + i);
        arm = GT4_F32(params + PARAMS_HALF_TRACK + idx * 4);
        switch (GT4_U8(layout + LAYOUT_SIDE + i)) {
        case 0: arm = -arm; break;
        case 1: break;
        default: arm = 0; break;
        }
        lx = GT4_F32(params + PARAMS_AXLE_DIST + idx * 4);
        if (i < n && (int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i) > 0) {}
        else if (i < n && (int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i) < 0) lx = -lx;
        else lx = 0;
        moment = moment + (arm * fy - lx * fx);
        GT4_F32(w + WHEEL_STEER) = steer;
    }

    GT4_F32(car + CAR_YAW_ACCEL) = moment * GT4_F32(params + PARAMS_YAW_INERTIA);
    yaw_filter(car);
    yaw = GT4_F32(car + CAR_YAW_RATE) + GT4_F32(car + CAR_YAW_ACCEL) * dt;
    GT4_F32(car + CAR_YAW_RATE) = yaw;
    if (K(0x40c90fdau) < yaw) GT4_F32(car + CAR_YAW_RATE) = K(0x40c90fdau);
    else if (yaw < K(0xc0c90fdau)) GT4_F32(car + CAR_YAW_RATE) = K(0xc0c90fdau);

    lat_accel = side / GT4_F32(car + CAR_MASS_A);
    GT4_F32(car + CAR_LAT_ACCEL) = lat_accel;
    grip = 0;
    for (i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        uint8_t surface = GT4_U8(gt4_car_wheel(car, i) + WHEEL_SURFACE);
        uint32_t tyre = gt4_tyre_params(params, i, surface);
        float mu = GT4_F32(tyre + TYRE_MU + surface * 4);
        mu = mu * gt4_surface_factor(car, surface);
        grip = grip + mu * GT4_F32(tyre + TYRE_MU_SCALE);
    }
    GT4_F32(car + CAR_GRIP_USE) = lat_accel * (float)GT4_U8(layout + LAYOUT_WHEELS) / grip;
}

/* func_00352C90: velocity from the accelerations; a car at rest with the
   brake held (or parked, CAR_56B 8) is held still. */
void gt4_car_velocity(uint32_t car, int airborne)
{
    float dt = GT4_F32(car + CAR_DT);
    int hold = 0;

    if (GT4_U16(car + CAR_5BE)) {
        GT4_F32(car + CAR_VELOCITY + 4) = 0;
        GT4_F32(car + CAR_668) = 0;
        GT4_F32(car + CAR_YAW_RATE) = 0;
        GT4_F32(car + CAR_VELOCITY) = 0;
        return;
    }
    if (airborne) return;

    if (GT4_U8(car + CAR_5C0)) {
        hold = 1;
    } else {
        uint8_t b = GT4_U8(car + CAR_56B);
        int8_t mode = (int8_t)GT4_U8(car + CAR_56A);
        if (b == 8) {
            hold = 1;
        } else if (!((uint8_t)(b - 1) < 2 || b == 4 || mode == 1 || mode == 2)) {
            float v = GT4_F32(car + CAR_SPEED);
            if (K(0xbe8e38e3u) < v && v < K(0x3e8e38e3u)) {   /* 1 km/h */
                int still = 1, n = gt4_car_wheels(car);
                for (int i = 0; i < n; i++)
                    if (GT4_F32(gt4_car_wheel(car, i) + WHEEL_VEL_WEIGHT) != 0) { still = 0; break; }
                if (still) hold = 0 < GT4_F32(car + CAR_BRAKE_PEDAL) || 0 < GT4_F32(car + CAR_HANDBRAKE);
            }
        }
    }

    if (hold) {
        GT4_F32(car + CAR_LAT_SPEED) = 0;
        GT4_F32(car + CAR_YAW_RATE) = 0;
        GT4_F32(car + CAR_VELOCITY) = 0;
        GT4_F32(car + CAR_VELOCITY + 4) = 0;
        GT4_F32(car + CAR_668) = 0;
        GT4_F32(car + CAR_654) = 0;
        GT4_F32(car + CAR_LAT_ACCEL) = 0;
        GT4_F32(car + CAR_GRIP_USE) = 0;
        GT4_F32(car + CAR_SPEED) = 0;
        return;
    }
    {
        float along = GT4_F32(car + CAR_654) * dt, across = GT4_F32(car + CAR_LAT_ACCEL) * dt;
        for (int k = 0; k < 2; k++)
            GT4_F32(car + CAR_VELOCITY + 4 * k) = GT4_F32(car + CAR_VELOCITY + 4 * k)
                + (along * GT4_F32(car + CAR_MATRIX + 4 * k) + across * GT4_F32(car + CAR_MATRIX + 0xc + 4 * k));
    }
}
