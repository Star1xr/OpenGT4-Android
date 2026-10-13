/* Wheel spin, differentials and the car's speed along its heading. */
#include "car.h"

#define K(bits) gt4_f32(bits)

static uint32_t drive_of(uint32_t car) { return GT4_U32(car + CAR_PARAMS) + PARAMS_DRIVE; }

/* func_00351FA8: forces along the heading -> acceleration -> speed. The
   effective mass adds the rotating inertia of the wheels that are free
   (not braking, on the ground). */
void gt4_car_longitudinal(uint32_t car)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), layout = gt4_car_layout(car);
    float dt = GT4_F32(car + CAR_DT), drive = 0, inertia = 0, inv_mass, v, lat, dv;
    int n = GT4_U8(layout + LAYOUT_WHEELS), i;

    for (i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i);
        drive = drive + gt4_sinf(GT4_F32(w + WHEEL_STEER)) * GT4_F32(w + WHEEL_70);
    }
    GT4_F32(car + CAR_668) = (GT4_F32(car + CAR_668) + drive) + GT4_F32(car + CAR_ROLLING);
    for (i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i);
        if (GT4_U8(w + WHEEL_FLAGS)) continue;
        inertia = inertia + GT4_F32(params + PARAMS_WHEEL_INERTIA + gt4_wheel_axle(layout, i) * 4);
    }
    inv_mass = K(0x3f800000u) / (inertia + GT4_F32(car + CAR_MASS_A));
    GT4_F32(car + CAR_654) = GT4_F32(car + CAR_654) + (GT4_F32(car + CAR_668) + GT4_F32(car + CAR_DRAG)) * inv_mass;
    v = GT4_F32(car + CAR_SPEED) + GT4_F32(car + CAR_654) * dt;
    GT4_F32(car + CAR_SPEED) = v;
    if (v == 0) return;

    lat = 0;
    for (i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        uint32_t w = gt4_car_wheel(car, i);
        lat = lat + gt4_sinf(GT4_F32(w + WHEEL_STEER)) * GT4_F32(w + WHEEL_74);
        if (GT4_U8(w + WHEEL_DD) == 1) {
            lat = lat - gt4_cosf(GT4_F32(w + WHEEL_STEER)) * GT4_F32(w + WHEEL_FORCE_LAT);
        } else {
            float a = GT4_F32(w + WHEEL_VEL_ANGLE);
            lat = lat - gt4_cosf(a) * GT4_F32(w + WHEEL_FORCE_LAT);
            lat = lat - gt4_sinf(a) * GT4_F32(w + WHEEL_FORCE_6C);
        }
    }
    dv = lat * inv_mass;
    v = GT4_F32(car + CAR_SPEED);
    {
        float next = v + dv * dt;
        if (0 < v * next) {
            GT4_F32(car + CAR_668) = GT4_F32(car + CAR_668) + lat;
            GT4_F32(car + CAR_654) = GT4_F32(car + CAR_654) + dv;
            GT4_F32(car + CAR_SPEED) = next;
        } else {
            /* the force would reverse the motion: stop instead */
            GT4_F32(car + CAR_654) = GT4_F32(car + CAR_654) + GT4_F32(car + CAR_RATE) * -v;
            GT4_F32(car + CAR_SPEED) = 0;
        }
    }
}

/* func_0036D230: torque on each wheel from its tyre forces and the brake. */
void gt4_car_wheel_torque(uint32_t car)
{
    uint32_t drive = drive_of(car), layout = gt4_car_layout(car);
    for (int i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        uint32_t w = gt4_car_wheel(car, i);
        float t = -GT4_F32(drive + DRIVE_RADIUS + gt4_wheel_axle(layout, i) * 4)
                  * (GT4_F32(w + WHEEL_7C) + GT4_F32(w + WHEEL_80));
        float spin = GT4_F32(w + WHEEL_SPIN);
        GT4_F32(w + WHEEL_TORQUE) = t;
        if (spin == 0) continue;
        GT4_F32(w + WHEEL_TORQUE) = 0 < spin ? t - GT4_F32(w + WHEEL_BRAKE) : t + GT4_F32(w + WHEEL_BRAKE);
    }
}

/* func_0036D2F8: the hybrid's front motor (drive kind 7), and the active
   yaw control of differentials 6 and 7 between 10 and 180 km/h: with the
   steering (3.8 per radian, at most 1) times the strength at +0x14, the
   torque moved to the wheel that should turn faster in the corner (the
   average spin 1/8 above on the outside), 7/16 added and 9/16 taken. */
void gt4_car_diff_assist(uint32_t car)
{
    uint32_t drive = drive_of(car), layout = gt4_car_layout(car), b = car + 0x104;
    if (GT4_U8(drive + DRIVE_KIND) == 7) gt4_hybrid_front(car);
    for (int k = 0; k < GT4_U8(layout + LAYOUT_DIFFS); k++) {
        uint8_t a = GT4_U8(layout + LAYOUT_DIFF_WHEEL_A + k), c = GT4_U8(layout + LAYOUT_DIFF_WHEEL_B + a), type;
        int idx;
        float v, steer, s, f, avg, step, give, take;
        uint32_t wa, wb;
        if (c == 0x7f) continue;
        idx = k < GT4_U8(layout + LAYOUT_DIFFS) ? GT4_U8(layout + LAYOUT_DIFF_INDEX + k) : 0;
        type = GT4_U8(drive + DRIVE_DIFF_TYPE + idx);
        if (type < 6 || !(type < 8)) continue;
        v = GT4_F32(b + 0x53c);
        if (v < K(0x4031c71bu) || K(0x4247ffffu) < v) continue;
        steer = GT4_F32(b + 0x468);
        s = steer < 0 ? -steer : steer;
        s = s * K(0x40747648u);
        if (K(0x3f800000u) < s) s = K(0x3f800000u);
        f = s * GT4_F32(drive + 0x14 + (uint32_t)idx * 4);
        if (f == 0) continue;
        wa = gt4_car_wheel(car, a);
        wb = gt4_car_wheel(car, c);
        avg = (GT4_F32(wa + WHEEL_SPIN) + GT4_F32(wb + WHEEL_SPIN)) * K(0x3f000000u);
        step = avg * K(0x3e000000u);
        if (0 < steer) avg = avg + step;
        else if (steer < 0) avg = avg - step;
        take = f * K(0x3f100000u);
        give = f * K(0x3ee00000u);
        if (GT4_F32(wb + WHEEL_SPIN) < avg) {
            GT4_F32(wb + WHEEL_TORQUE) = GT4_F32(wb + WHEEL_TORQUE) + give;
            GT4_F32(wa + WHEEL_TORQUE) = GT4_F32(wa + WHEEL_TORQUE) - take;
        } else if (avg < GT4_F32(wb + WHEEL_SPIN)) {
            GT4_F32(wb + WHEEL_TORQUE) = GT4_F32(wb + WHEEL_TORQUE) - take;
            GT4_F32(wa + WHEEL_TORQUE) = GT4_F32(wa + WHEEL_TORQUE) + give;
        }
    }
}

/* func_0036E4B8: wheel spin through each differential (type
   drive+0x10). Open (0): each wheel integrates its own torque. Locked
   (1): both at the driven speed. The rest split the driven speed by the
   wheels' spin and torque difference; before that, torque sensing (3, 4)
   moves torque across up to the drive torque times a ratio (+0x1C on
   power, +0x24 off, at least the preload +0x14), locking (car+0x628) when
   the wheels' torques are within twice that, and viscous (5) moves
   torque with the square of the spin difference (capped at +0x14,
   times +0x1C). */
void gt4_car_differentials(uint32_t car)
{
    uint32_t drive = drive_of(car), layout = gt4_car_layout(car), b = car + 0x104;
    float dt = GT4_F32(car + CAR_DT);

    for (int k = 0; k < GT4_U8(layout + LAYOUT_DIFFS); k++) {
        uint8_t a = GT4_U8(layout + LAYOUT_DIFF_WHEEL_A + k), c = GT4_U8(layout + LAYOUT_DIFF_WHEEL_B + a);
        int idx = k < GT4_U8(layout + LAYOUT_DIFFS) ? GT4_U8(layout + LAYOUT_DIFF_INDEX + k) : 0;
        uint8_t type;
        uint32_t wa, wb;
        float diff, move = 0;
        int moved = 0;

        if (c == 0x7f) c = a;
        wa = gt4_car_wheel(car, a);
        wb = gt4_car_wheel(car, c);
        diff = GT4_F32(wa + WHEEL_SPIN) - GT4_F32(wb + WHEEL_SPIN);
        type = GT4_U8(drive + DRIVE_DIFF_TYPE + idx);

        if (type == 3 || type == 4) {
            float t = GT4_F32(b + 0x534 + (uint32_t)k * 4), pre = GT4_F32(drive + 0x14 + (uint32_t)idx * 4);
            if (0 <= t) move = t * GT4_F32(drive + 0x1c + (uint32_t)idx * 4);
            else move = GT4_F32(drive + 0x24 + (uint32_t)idx * 4) * -t;
            if (move < pre) move = pre;
            if (GT4_U8(b + 0x524 + (uint32_t)k)) {
                float twice = move + move, d = GT4_F32(wa + WHEEL_TORQUE) - GT4_F32(wb + WHEEL_TORQUE);
                if (twice < d || d < -twice) GT4_U8(b + 0x524 + (uint32_t)k) = 0;
                else type = 1;
            }
            if (type != 1) moved = 1;
        } else if (type == 5) {
            float sq = diff * diff, cap = GT4_F32(drive + 0x14 + (uint32_t)idx * 4);
            if (cap < sq) sq = cap;
            move = GT4_F32(drive + 0x1c + (uint32_t)idx * 4) * sq;
            moved = 1;
        }
        if (moved) {
            if (0 < diff) {
                GT4_F32(wa + WHEEL_TORQUE) = GT4_F32(wa + WHEEL_TORQUE) - move;
                GT4_F32(wb + WHEEL_TORQUE) = GT4_F32(wb + WHEEL_TORQUE) + move;
            } else if (diff < 0) {
                GT4_F32(wa + WHEEL_TORQUE) = GT4_F32(wa + WHEEL_TORQUE) + move;
                GT4_F32(wb + WHEEL_TORQUE) = GT4_F32(wb + WHEEL_TORQUE) - move;
            }
        }

        if (type == 0) {
            int count = GT4_U8(layout + LAYOUT_DIFF_WHEELS + k);
            float inertia = GT4_F32(drive + DRIVE_DIFF_INERTIA + idx * 4);
            for (int j = 0; j < count; j++) {
                uint32_t w = gt4_car_wheel(car, j ? c : a);
                float spin = GT4_F32(w + WHEEL_SPIN);
                float next = spin + GT4_F32(w + WHEEL_TORQUE) * inertia * dt;
                if (GT4_F32(w + WHEEL_BRAKE) != 0) {
                    /* a braked wheel stops instead of reversing */
                    if (0 < spin) next = gt4_select(next < 0, 0.0f, next);
                    else if (spin < 0) next = gt4_select(0 < next, 0.0f, next);
                    else next = 0;
                }
                GT4_F32(w + WHEEL_SPIN) = next;
            }
        } else {
            float half = K(0x3f000000u), split = 0, radius, base;
            if (type != 1) {
                split = diff * half + (GT4_F32(wa + WHEEL_TORQUE) - GT4_F32(wb + WHEEL_TORQUE))
                        * GT4_F32(drive + DRIVE_DIFF_INERTIA + idx * 4) * half * dt;
                if (diff * split < 0) {
                    split = 0;
                    if (type == 3 || type == 4) GT4_U8(b + 0x524 + (uint32_t)k) = 1;
                }
            }
            radius = GT4_F32(drive + DRIVE_RADIUS + idx * 4);
            if (GT4_U8(drive + 1)) radius = GT4_F32(drive + DRIVE_RADIUS + GT4_U8(drive + 3) * 4);
            base = radius * GT4_F32(car + CAR_FRONT_SPEED + k * 4);
            if (type == 1) {
                GT4_F32(wb + WHEEL_SPIN) = base;
                GT4_F32(wa + WHEEL_SPIN) = base;
            } else {
                GT4_F32(wa + WHEEL_SPIN) = base + split;
                GT4_F32(wb + WHEEL_SPIN) = base - split;
            }
        }
    }
}

/* func_00350ED0: tachometer needle, a spring-damper towards the rpm. */
void gt4_car_tachometer(uint32_t car)
{
    float dt = GT4_F32(car + CAR_DT);
    float rate = GT4_F32(car + CAR_TACHO_RATE);
    float err = (float)(int16_t)GT4_U16(car + CAR_ENGINE_RPM) - GT4_F32(car + CAR_TACHO);
    rate = rate + (err * K(0x447a0000u) - rate * K(0x42480000u)) * dt;   /* 1000, 50 */
    GT4_F32(car + CAR_TACHO_RATE) = rate;
    GT4_F32(car + CAR_TACHO) = GT4_F32(car + CAR_TACHO) + rate * dt;
}
