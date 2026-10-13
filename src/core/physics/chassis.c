/* Chassis step (func_00344530): suspension, body attitude, steering,
   engine vibration, wheel rotation. Parts not rewritten yet run the
   original. */
#include "car.h"

#include <math.h>

/* func_0036C9F8: body rotation rows from the three angles. */
void gt4_rotation_from_angles(float *a, float *b, float *c, float x, float y, float z)
{
    float cx = gt4_cosf(x), sx = gt4_sinf(x), cy = gt4_cosf(y), sy = gt4_sinf(y), cz = gt4_cosf(z), sz = gt4_sinf(z);
    float szcx = sz * cx, czcx = cz * cx;
    b[2] = -sx * cy;
    b[0] = sz * sy - czcx * cy;
    b[1] = cz * sy + szcx * cy;
    a[2] = cx;
    a[0] = -cz * sx;
    a[1] = sz * sx;
    c[2] = sx * sy;
    c[0] = sz * cy + czcx * sy;
    c[1] = cz * cy - szcx * sy;
}

void gt4_car_chassis(uint32_t car)
{
    gt4_car_suspension_reach(car);
    gt4_car_suspension(car);
    gt4_car_attitude(car);
    gt4_car_ground_fix(car);
    gt4_car_vibration_wheels(car);
    gt4_car_force_feedback(car);
    gt4_car_surface_feel(car);
    gt4_car_vibration_engine(car);
    gt4_car_vibration_reset(car);
    gt4_car_view_follow(car);
    gt4_car_mounts(car);
    gt4_rotation_from_angles(gt4_ptr(car + CAR_MATRIX), gt4_ptr(car + CAR_MATRIX + 0xc), gt4_ptr(car + CAR_MATRIX + 0x18),
                             GT4_F32(car + CAR_ANGLES), GT4_F32(car + CAR_ANGLES + 4), GT4_F32(car + CAR_ANGLES + 8));
    gt4_car_ground_frame(car);
    gt4_car_ground_speeds(car);
    gt4_car_wheel_rotation(car);
    gt4_car_wheel_effects(car);
    gt4_car_view_sway(car);
    gt4_car_view_bob(car);
}

#define K(bits) gt4_f32(bits)
#define VERTICAL_MODE 0x00620decu   /* byte: 2 = vertical forces along each wheel's ground normal */

/* func_0036AF30: pitch moment and vertical force of the drive and brake
   forces acting at the wheels. */
static void drive_reaction(uint32_t car, float *moment, float *lift)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), layout = gt4_car_layout(car);
    float m = 0, l = 0;
    for (int i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        uint32_t w = gt4_car_wheel(car, i);
        int n = GT4_U8(layout + LAYOUT_WHEELS);
        int axle = i < n ? GT4_U8(layout + LAYOUT_AXLE + i) : 0;
        int pos = GT4_U8(layout + LAYOUT_POSITION + i);
        float f = gt4_sinf(GT4_F32(w + WHEEL_STEER)) * (GT4_F32(w + WHEEL_70) + GT4_F32(w + WHEEL_74));
        float h = f * GT4_F32(params + PARAMS_2C4 + axle * 4);
        m = m + h * GT4_F32(params + PARAMS_12C + pos * 4);
        n = GT4_U8(layout + LAYOUT_WHEELS);
        if (i < n && (int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i) > 0) l = l + h / GT4_F32(params + PARAMS_AXLE_DIST + pos * 4);
        else if (i < n && (int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i) < 0) l = l - h / GT4_F32(params + PARAMS_AXLE_DIST + pos * 4);
    }
    *moment = m;
    *lift = l;
}

static float damper_on_ground(uint32_t w)
{
    return GT4_F32(w + WHEEL_LOAD) == 0 ? 0 : GT4_F32(w + WHEEL_DAMPER);
}

/* func_0036A298: the pit jack's lift at timer t (steps, counting while
   CAR_5BE is set): nothing up to 60, x until 660, lowering to 720. */
static float jack_lift(int t, float x, int *stage)
{
    if (!(t < 720)) { *stage = 0; return 0; }
    if (!(t < 660)) { *stage = 1; return x * (float)(720 - t) / K(0x42700000u); }
    if (!(t < 60)) { *stage = 2; return x; }
    *stage = 0;
    return 0;
}

/* func_0036A318: the car held up on the pit jacks, front and rear (the
   rear 15 steps ahead) above the ground: its height and pitch from them,
   no roll, no vertical motion; 0 while neither lifts it. */
static int jack_hold(uint32_t car, float x)
{
    uint32_t b = car + 0x104, params = GT4_U32(car + CAR_PARAMS);
    int stage = 0, up = 0, t = GT4_U16(b + 0x4ba);
    float lf = jack_lift(t, x, &stage), lr = jack_lift(t + 15, x, &stage), z = GT4_F32(b + 8), g = GT4_F32(b + 0x38);
    float front = z + GT4_F32(b + 0x14) * GT4_F32(params + 0x10), rear = z - GT4_F32(b + 0x14) * GT4_F32(params + 0x14);
    if (front < g + lf) { front = g + lf; up = 1; }
    if (rear < g + lr) { rear = g + lr; up = 1; }
    if (!up) return 0;
    GT4_F32(b + 8) = front * GT4_F32(params + 0x24) + rear * GT4_F32(params + 0x20);
    GT4_F32(b + 0x568) = gt4_atan2f(front - rear, GT4_F32(params + 0x18));
    GT4_F32(b + 0x558) = (GT4_F32(b + 8) - z) * GT4_F32(b + 0x44c);
    GT4_U32(b + 0x56c) = 0;
    GT4_U32(b + 0x54c) = 0;
    GT4_U32(b + 0x584) = 0;
    GT4_U32(b + 0x580) = 0;
    return 1;
}

/* func_0036AE20: in the pit (CAR_5BE counting), once on the jacks the car
   turns toward its pit box's heading at 30 deg/s instead of moving. */
static int pit_held(uint32_t car)
{
    uint32_t b = car + 0x104, params = GT4_U32(car + CAR_PARAMS);
    float target, d;
    if (!GT4_U16(b + 0x4ba)) return 0;
    if (!jack_hold(car, K(0x3eeb851du) - (GT4_F32(params + PARAMS_AXLES + AXLE_0C) - GT4_F32(params + PARAMS_AXLES + AXLE_MAX_ALT))))
        return 0;
    target = gt4_line_point_value(GT4_U32(car + 4) + 0xda50u, (int8_t)GT4_U8(b + 0x4b7), 0x24);   /* func_0034C7C0 */
    d = gt4_angle_diff(target, GT4_F32(b + 0x570));
    if (d != 0) {
        float step = GT4_F32(car + CAR_DT) * K(0x3f060a90u);
        if (step < d) GT4_F32(b + 0x570) = GT4_F32(b + 0x570) + step;
        else if (d < -step) GT4_F32(b + 0x570) = GT4_F32(b + 0x570) - step;
        else GT4_F32(b + 0x570) = target;
    }
    gt4_rotation_from_angles(gt4_ptr(car + CAR_MATRIX), gt4_ptr(car + CAR_MATRIX + 0xc), gt4_ptr(car + CAR_MATRIX + 0x18),
                             GT4_F32(b + 0x568), GT4_F32(b + 0x56c), GT4_F32(b + 0x570));
    return 1;
}

/* func_0036B0C8: pitch and roll from the wheel loads, then the vertical
   motion of the body with gravity: by vertical mode (D_00620DEC) 0 a
   single sum along the body, 1 each wheel's load along the ground frame,
   2 along each wheel's own ground normal; the fan car (aero kind 7) sucks
   itself down near the ground. Layout kind 1 (two wheels) runs the
   original. */
void gt4_car_attitude(uint32_t car)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), layout = gt4_car_layout(car);
    const float quarter = K(0x3f490fd9u);
    float dt = GT4_F32(car + CAR_DT), moment, lift, axle[2], rate, ang, acc[3], down;
    int i, k;

    if (GT4_U8(layout + LAYOUT_KIND) == 1) {
        GT4_ORIG(0x0036b0c8u, car);
        return;
    }
    drive_reaction(car, &moment, &lift);
    if (pit_held(car)) return;

    /* pitch: front against rear loads */
    axle[0] = axle[1] = 0;
    for (i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        int a = GT4_U8(layout + LAYOUT_AXLE + i);
        axle[a] = axle[a] + GT4_F32(car + CAR_LOAD_TARGET + 4 * i);
    }
    for (i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        int a = GT4_U8(layout + LAYOUT_AXLE + i);
        axle[a] = axle[a] + damper_on_ground(gt4_car_wheel(car, i));
    }
    rate = GT4_F32(car + CAR_PITCH_RATE)
         + ((axle[0] * GT4_F32(params + PARAMS_PITCH_ARM) - axle[1] * GT4_F32(params + PARAMS_PITCH_ARM + 4)) + moment) * dt;
    if (K(0x3fc90fdau) < rate) rate = K(0x3fc90fdau);
    else if (rate < -K(0x3fc90fdau)) rate = -K(0x3fc90fdau);
    ang = GT4_F32(car + CAR_ANGLES) + rate * dt;
    GT4_F32(car + CAR_ANGLES) = ang;
    if (ang < -quarter) { GT4_F32(car + CAR_ANGLES) = -quarter; rate = 0; }
    else if (quarter < ang) { GT4_F32(car + CAR_ANGLES) = quarter; rate = 0; }
    GT4_F32(car + CAR_PITCH_RATE) = rate;

    /* roll: left against right */
    axle[0] = axle[1] = 0;
    for (i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        uint8_t side = GT4_U8(layout + LAYOUT_SIDE + i);
        int a = GT4_U8(layout + LAYOUT_AXLE + i);
        if (side == 0) axle[a] = axle[a] + GT4_F32(car + CAR_LOAD_TARGET + 4 * i);
        else if (side == 1) axle[a] = axle[a] - GT4_F32(car + CAR_LOAD_TARGET + 4 * i);
    }
    for (i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        uint8_t side = GT4_U8(layout + LAYOUT_SIDE + i);
        int a = GT4_U8(layout + LAYOUT_AXLE + i);
        float d = damper_on_ground(gt4_car_wheel(car, i));
        if (side == 0) axle[a] = axle[a] + d;
        else if (side == 1) axle[a] = axle[a] - d;
    }
    rate = GT4_F32(car + CAR_ROLL_RATE)
         + (axle[0] * GT4_F32(params + PARAMS_ROLL_ARM) + axle[1] * GT4_F32(params + PARAMS_ROLL_ARM + 4)) * dt;
    if (K(0x41490fdau) < rate) rate = K(0x41490fdau);   /* 4 pi */
    else if (rate < -K(0x41490fdau)) rate = -K(0x41490fdau);
    ang = GT4_F32(car + CAR_ANGLES + 4) + rate * dt;
    GT4_F32(car + CAR_ANGLES + 4) = ang;
    if (ang < -quarter) { GT4_F32(car + CAR_ANGLES + 4) = -quarter; rate = 0; }
    else if (quarter < ang) { GT4_F32(car + CAR_ANGLES + 4) = quarter; rate = 0; }
    GT4_F32(car + CAR_ROLL_RATE) = rate;

    gt4_rotation_from_angles(gt4_ptr(car + CAR_MATRIX), gt4_ptr(car + CAR_MATRIX + 0xc), gt4_ptr(car + CAR_MATRIX + 0x18),
                             GT4_F32(car + CAR_ANGLES), GT4_F32(car + CAR_ANGLES + 4), GT4_F32(car + CAR_ANGLES + 8));

    if (GT4_U8(VERTICAL_MODE) == 0) {
        /* vertical mode 0: the loads straight along the body */
        float sum = 0, a;
        for (i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) sum = sum + GT4_F32(car + CAR_LOAD_TARGET + 4 * i);
        for (i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) sum = sum + damper_on_ground(gt4_car_wheel(car, i));
        sum = sum + lift;
        a = sum / GT4_F32(car + CAR_MASS_B);
        GT4_F32(car + CAR_VERT_ACCEL) = a - GT4_F32(car + CAR_MATRIX + 0x20) * K(0x411ccccc);
        a = a * dt;
        for (k = 0; k < 3; k++)
            GT4_F32(car + CAR_VELOCITY + 4 * k) = GT4_F32(car + CAR_VELOCITY + 4 * k) + GT4_F32(car + CAR_MATRIX + 0x18 + 4 * k) * a;
        GT4_F32(car + CAR_VELOCITY + 8) = GT4_F32(car + CAR_VELOCITY + 8) - dt * K(0x411ccccc);
        return;
    }

    /* vertical: each wheel's load along its ground normal (mode 2) or
       the ground frame's (other modes) */
    acc[0] = acc[1] = acc[2] = 0;
    for (i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        uint32_t g = car + CAR_GROUND + i * GROUND_SIZE, w = gt4_car_wheel(car, i);
        int n = GT4_U8(layout + LAYOUT_WHEELS);
        int a = i < n ? GT4_U8(layout + LAYOUT_AXLE + i) : 0;
        float nrm[3], dot = 0, load;
        if (GT4_U8(VERTICAL_MODE) == 2) {
            nrm[0] = GT4_F32(g + 0x1c);
            nrm[2] = GT4_F32(g + 0x20);
            nrm[1] = -GT4_F32(g + 0x24);
        } else {
            for (k = 0; k < 3; k++) nrm[k] = GT4_F32(car + CAR_140 + 0x18 + 4 * k);
        }
        for (k = 0; k < 3; k++) dot = dot + nrm[k] * GT4_F32(car + CAR_MATRIX + 0x18 + 4 * k);
        load = GT4_F32(w + WHEEL_LOAD) - GT4_F32(params + PARAMS_AXLES + a * AXLE_SIZE + AXLE_UNSPRUNG);
        load = gt4_select(load < 0, 0.0f, load);
        dot = dot * load;
        for (k = 0; k < 3; k++) acc[k] = acc[k] + dot * nrm[k];
    }
    down = lift - (GT4_F32(car + CAR_DOWNFORCE) + GT4_F32(car + CAR_DOWNFORCE + 4));
    if (GT4_U8(params + PARAMS_AERO_KIND) == 7) {
        /* the fan car: full suction within the skirt's height (+0x3C, plus
           0.1 m) of the ground, fading over the next 0.1 m */
        float gap = (GT4_F32(car + CAR_POSITION + 8) - GT4_F32(car + CAR_POSITION + 0x38))
                    - (GT4_F32(params + 0x3c) + K(0x3dccccccu));
        float suck = GT4_F32(params + 0x1084);
        if (gap <= 0) down = down - suck;
        else if (!(K(0x3dccccccu) < gap)) down = down - suck * (K(0x3f800000u) - gap * K(0x41200000u));
    }
    for (k = 0; k < 3; k++) acc[k] = acc[k] + down * GT4_F32(car + CAR_MATRIX + 0x18 + 4 * k);
    for (k = 0; k < 3; k++) acc[k] = acc[k] / GT4_F32(car + CAR_MASS_B);
    acc[2] = acc[2] - K(0x411ccccc);   /* gravity */
    {
        float along = 0;
        for (k = 0; k < 3; k++) {
            GT4_F32(car + CAR_VELOCITY + 4 * k) = GT4_F32(car + CAR_VELOCITY + 4 * k) + acc[k] * dt;
            along = along + acc[k] * GT4_F32(car + CAR_MATRIX + 0x18 + 4 * k);
        }
        GT4_F32(car + CAR_VERT_ACCEL) = along;
    }
}

/* func_003457B0: height of each wheel mount from the body's tilt. */
static void mount_heights(uint32_t car)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), layout = gt4_car_layout(car);
    for (int i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        uint32_t w = gt4_car_wheel(car, i);
        int pos = GT4_U8(layout + LAYOUT_POSITION + i), n;
        uint8_t side = GT4_U8(layout + LAYOUT_SIDE + i);
        float y = GT4_F32(car + CAR_MATRIX + 0x14) * GT4_F32(params + PARAMS_HALF_TRACK + pos * 4);
        float x = GT4_F32(car + CAR_MATRIX + 0x08) * GT4_F32(params + PARAMS_AXLE_DIST + pos * 4);
        if (side == 0) GT4_F32(w + WHEEL_MOUNT + 8) = -y;
        else if (side == 1) GT4_F32(w + WHEEL_MOUNT + 8) = y;
        else GT4_F32(w + WHEEL_MOUNT + 8) = 0;
        n = GT4_U8(layout + LAYOUT_WHEELS);
        if (i < n && (int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i) > 0)
            GT4_F32(w + WHEEL_MOUNT + 8) = GT4_F32(w + WHEEL_MOUNT + 8) + x;
        else if (i < n && (int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i) < 0)
            GT4_F32(w + WHEEL_MOUNT + 8) = GT4_F32(w + WHEEL_MOUNT + 8) - x;
    }
}

/* func_0036BA88: when wheels cannot reach the ground within their travel,
   lift the body and tilt it by the shortfall of each corner. */
void gt4_car_ground_fix(uint32_t car)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), layout = gt4_car_layout(car);
    const float half = K(0x3f000000u);
    float d[4] = { 0, 0, 0, 0 }, fl = 0, fr = 0, rl = 0, rr = 0, lift = 0, a, b, c;
    uint8_t flags;

    if (!GT4_U8(car + CAR_SUSP_SHORT)) return;
    mount_heights(car);
    gt4_car_suspension_update_reach(car);
    flags = gt4_car_suspension_short(car, GT4_U8(car + CAR_SUSP_SHORT) << 4);
    GT4_U8(car + CAR_SUSP_SHORT) = flags;
    if (flags & 0xf) {
        for (int i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
            int n = GT4_U8(layout + LAYOUT_WHEELS);
            int axle = i < n ? GT4_U8(layout + LAYOUT_AXLE + i) : 0;
            d[i] = GT4_F32(params + PARAMS_AXLES + axle * AXLE_SIZE + AXLE_MIN) - GT4_F32(car + CAR_SUSP_REACH + 4 * i);
            if (d[i] < 0) d[i] = 0;
        }
        /* rise at the front (fl), rear (rl) and per side (fr, rr) */
        switch (GT4_U8(layout + LAYOUT_KIND)) {
        case 0:
            fl = (d[0] + d[1]) * half;
            rl = (d[2] + d[3]) * half;
            fr = d[0] + (d[2] - d[0]) * GT4_F32(params + 0x20);
            rr = d[1] + (d[3] - d[1]) * GT4_F32(params + 0x20);
            break;
        case 1:
            fl = d[0];
            rl = d[1];
            break;
        case 2:
            fl = d[0];
            rl = (d[1] + d[2]) * half;
            fr = d[0] + (d[1] - d[0]) * GT4_F32(params + 0x20);
            rr = d[0] + (d[2] - d[0]) * GT4_F32(params + 0x20);
            break;
        case 3:
            rl = d[2];
            fl = (d[0] + d[1]) * half;
            fr = rl + (d[0] - rl) * GT4_F32(params + 0x24);
            rr = rl + (d[1] - rl) * GT4_F32(params + 0x24);
            break;
        default:
            break;
        }
        if (!(fr == 0 && rr == 0)) {
            if (rr < fr) {
                GT4_F32(car + CAR_ANGLES + 4) = GT4_F32(car + CAR_ANGLES + 4) + gt4_atan2f(fr - rr, GT4_F32(params + 0x1c));
                lift = rr * half;
            } else if (fr < rr) {
                GT4_F32(car + CAR_ANGLES + 4) = GT4_F32(car + CAR_ANGLES + 4) + gt4_atan2f(fr - rr, GT4_F32(params + 0x1c));
                lift = fr * half;
            } else {
                lift = fr * half;
            }
        }
        if (!(fl == 0 && rl == 0)) {
            float l;
            if (rl < fl) {
                GT4_F32(car + CAR_ANGLES) = GT4_F32(car + CAR_ANGLES) + gt4_atan2f(fl - rl, GT4_F32(params + 0x18));
                l = rl * half;
            } else if (fl < rl) {
                GT4_F32(car + CAR_ANGLES) = GT4_F32(car + CAR_ANGLES) + gt4_atan2f(fl - rl, GT4_F32(params + 0x18));
                l = fl * half;
            } else {
                l = fl * half;
            }
            lift = lift + l;
        }
        GT4_F32(car + CAR_POSITION + 8) = GT4_F32(car + CAR_POSITION + 8) + lift;
    }
    gt4_rotation_from_angles(gt4_ptr(car + CAR_MATRIX), gt4_ptr(car + CAR_MATRIX + 0xc), gt4_ptr(car + CAR_MATRIX + 0x18),
                             GT4_F32(car + CAR_ANGLES), GT4_F32(car + CAR_ANGLES + 4), GT4_F32(car + CAR_ANGLES + 8));
    /* the vertical speed may not point into the ground */
    a = 0;
    b = 0;
    for (int k = 0; k < 3; k++) {
        a = a + GT4_F32(car + CAR_140 + 4 * k) * GT4_F32(car + CAR_VELOCITY + 4 * k);
        b = b + GT4_F32(car + CAR_140 + 0xc + 4 * k) * GT4_F32(car + CAR_VELOCITY + 4 * k);
    }
    c = a * GT4_F32(car + CAR_140 + 8) + b * GT4_F32(car + CAR_140 + 0x14);
    c = gt4_select(0 < c, 0.0f, c);
    if (GT4_F32(car + CAR_VELOCITY + 8) < c) GT4_F32(car + CAR_VELOCITY + 8) = c;
}

/* func_003458D0: horizontal position of each wheel mount (half track along
   the body's second row, axle distance along the first). */
static void mount_xy(uint32_t car)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), layout = gt4_car_layout(car);
    for (int i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        uint32_t w = gt4_car_wheel(car, i);
        int pos = GT4_U8(layout + LAYOUT_POSITION + i), n;
        uint8_t side = GT4_U8(layout + LAYOUT_SIDE + i);
        float t = GT4_F32(params + PARAMS_HALF_TRACK + pos * 4), l = GT4_F32(params + PARAMS_AXLE_DIST + pos * 4);
        float y1 = GT4_F32(car + CAR_MATRIX + 0x10) * t, x1 = GT4_F32(car + CAR_MATRIX + 0x0c) * t;
        float x2 = GT4_F32(car + CAR_MATRIX) * l, y2 = GT4_F32(car + CAR_MATRIX + 4) * l;
        if (side == 0) { GT4_F32(w + WHEEL_MOUNT) = -x1; GT4_F32(w + WHEEL_MOUNT + 4) = -y1; }
        else if (side == 1) { GT4_F32(w + WHEEL_MOUNT) = x1; GT4_F32(w + WHEEL_MOUNT + 4) = y1; }
        else { GT4_F32(w + WHEEL_MOUNT) = 0; GT4_F32(w + WHEEL_MOUNT + 4) = 0; }
        n = GT4_U8(layout + LAYOUT_WHEELS);
        if (i < n && (int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i) > 0) {
            GT4_F32(w + WHEEL_MOUNT) = GT4_F32(w + WHEEL_MOUNT) + x2;
            GT4_F32(w + WHEEL_MOUNT + 4) = GT4_F32(w + WHEEL_MOUNT + 4) + y2;
        } else if (i < n && (int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i) < 0) {
            GT4_F32(w + WHEEL_MOUNT) = GT4_F32(w + WHEEL_MOUNT) - x2;
            GT4_F32(w + WHEEL_MOUNT + 4) = GT4_F32(w + WHEEL_MOUNT + 4) - y2;
        }
    }
}

/* func_00345A20: wheel mount positions from the body's orientation. */
void gt4_car_mounts(uint32_t car)
{
    mount_heights(car);
    mount_xy(car);
}

#define INPUT_FILTER_THROTTLE 0x0062075cu
#define INPUT_FILTER_BRAKE    0x00620760u

/* func_0035A540 / func_0035A618 for the first four slots. */
static float input_slot(uint32_t car, int k) { return GT4_F32(car + CAR_INPUTS + 4 * k); }
static void set_input_slot(uint32_t car, int k, float v) { GT4_F32(car + CAR_INPUTS + 4 * k) = v; }

/* func_00344040: wheel rotation angles (with whole turns counted apart),
   reference points of the body and filtered throttle and brake. */
void gt4_car_wheel_rotation(uint32_t car)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), layout = gt4_car_layout(car);
    const float two_pi = K(0x40c90fdau);
    float dt = GT4_F32(car + CAR_DT), yaw = GT4_F32(car + CAR_ANGLES + 8), r, s, t, old;

    /* func_0035A080: layout kind 1 (two wheels) runs the original */
    if (GT4_U8(layout + LAYOUT_KIND) == 1) {
        GT4_ORIG(0x00344040u, car);
        return;
    }
    if (two_pi <= yaw) {
        GT4_F32(car + CAR_ANGLES + 8) = yaw - two_pi;
        GT4_F32(car + CAR_YAW_TURNS) = GT4_F32(car + CAR_YAW_TURNS) - two_pi;
    } else if (yaw <= -two_pi) {
        GT4_F32(car + CAR_ANGLES + 8) = yaw + two_pi;
        GT4_F32(car + CAR_YAW_TURNS) = GT4_F32(car + CAR_YAW_TURNS) + two_pi;
    }

    for (int i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        uint32_t w = gt4_car_wheel(car, i);
        int n = GT4_U8(layout + LAYOUT_WHEELS);
        int axle = i < n ? GT4_U8(layout + LAYOUT_AXLE + i) : 0;
        float turn = GT4_F32(w + WHEEL_SPIN) * GT4_F32(params + PARAMS_INV_RADIUS + axle * 4) * dt;
        float a, turns;
        if (GT4_U8(car + CAR_5C0)) turn = 0;
        a = GT4_F32(w + WHEEL_ROTATION) + turn;
        if (two_pi <= a) {
            turns = GT4_F32(car + CAR_WHEEL_TURNS + 4 * i);
            do { a = a - two_pi; turns = turns - two_pi; } while (two_pi <= a);
            GT4_F32(car + CAR_WHEEL_TURNS + 4 * i) = turns;
        }
        if (a <= -two_pi) {
            turns = GT4_F32(car + CAR_WHEEL_TURNS + 4 * i);
            do { a = a + two_pi; turns = turns + two_pi; } while (a <= -two_pi);
            GT4_F32(car + CAR_WHEEL_TURNS + 4 * i) = turns;
        }
        GT4_F32(w + WHEEL_ROTATION) = a;
    }

    /* a point ahead of the centre, and its previous value */
    r = GT4_F32(params + PARAMS_50);
    for (int k = 0; k < 3; k++) GT4_F32(car + CAR_REF_A_PREV + 4 * k) = GT4_F32(car + CAR_REF_A + 4 * k);
    GT4_F32(car + CAR_REF_A) = GT4_F32(car + CAR_POSITION) - r * GT4_F32(car + CAR_MATRIX);
    GT4_F32(car + CAR_REF_A + 4) = GT4_F32(car + CAR_POSITION + 8) - r * GT4_F32(car + CAR_MATRIX + 8);
    GT4_F32(car + CAR_REF_A + 8) = -(GT4_F32(car + CAR_POSITION + 4) - r * GT4_F32(car + CAR_MATRIX + 4));
    if (GT4_U8(params + 0x32) == 6) {
        /* func_00359F50: raised along the body's up axis by params+0x2C8
           less the height of its class (D_006205C8, 0x78 per class) */
        uint8_t cls = GT4_U8(params + 0x33);
        uint32_t t = 0x006205c8u + (cls < 3 ? cls : 0) * 0x78u;
        float d = (GT4_F32(params + 0x2c8) - GT4_F32(t + 0x10)) - GT4_F32(params + 0x3c);
        GT4_F32(car + CAR_REF_A) = GT4_F32(car + CAR_REF_A) + d * GT4_F32(car + CAR_MATRIX + 0x18);
        GT4_F32(car + CAR_REF_A + 4) = GT4_F32(car + CAR_REF_A + 4) + d * GT4_F32(car + CAR_MATRIX + 0x20);
        GT4_F32(car + CAR_REF_A + 8) = GT4_F32(car + CAR_REF_A + 8) + d * GT4_F32(car + CAR_MATRIX + 0x1c);
    }
    for (int k = 0; k < 3; k++) GT4_F32(car + CAR_REF_B_PREV + 4 * k) = GT4_F32(car + CAR_REF_B + 4 * k);
    old = GT4_F32(car + CAR_STEER_SHARE);
    GT4_F32(car + CAR_REF_B) = GT4_F32(car + CAR_POSITION + 0x30) - r * GT4_F32(car + CAR_140);
    GT4_F32(car + CAR_REF_B + 4) = GT4_F32(car + CAR_POSITION + 0x38) - r * GT4_F32(car + CAR_140 + 8);
    GT4_F32(car + CAR_STEER_SHARE_PREV) = old;
    GT4_F32(car + CAR_REF_B + 8) = -(GT4_F32(car + CAR_POSITION + 0x34) - r * GT4_F32(car + CAR_140 + 4));

    /* steering share 0..1 (0.5 straight) */
    s = K(0x3f000000u) - GT4_F32(car + CAR_56C) / GT4_F32(params + PARAMS_STEER_MAX) * K(0x3f000000u);
    s = gt4_select(s < 0, 0.0f, gt4_select(K(0x3f800000u) < s, K(0x3f800000u), s));
    GT4_F32(car + CAR_STEER_SHARE) = s;

    t = GT4_F32(INPUT_FILTER_THROTTLE);
    set_input_slot(car, 2, GT4_F32(car + CAR_THROTTLE) * t + input_slot(car, 2) * (K(0x3f800000u) - t));
    t = GT4_F32(INPUT_FILTER_BRAKE);
    set_input_slot(car, 3, GT4_F32(car + CAR_BRAKE_PEDAL) * t + input_slot(car, 3) * (K(0x3f800000u) - t));
}

/* func_0035D350: the ground plane under the car from the four contact
   heights: its pitch (CAR_67C - 4), roll (CAR_67C), height at the centre
   and rotation rows at CAR_140. Layout kind 0 only; others run the
   original. */
void gt4_car_ground_frame(uint32_t car)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), layout = gt4_car_layout(car);
    const float half = K(0x3f000000u);
    float h[4], front, a, b, s, pitch, roll;

    if (GT4_U8(layout + LAYOUT_KIND) == 1) {
        GT4_ORIG(0x0035d350u, car);
        return;
    }
    for (int i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) h[i] = GT4_F32(gt4_car_wheel(car, i) + WHEEL_8C);
    GT4_F32(car + CAR_POSITION + 0x30) = GT4_F32(car + CAR_POSITION);
    GT4_F32(car + CAR_POSITION + 0x34) = GT4_F32(car + CAR_POSITION + 4);
    if (GT4_U8(layout + LAYOUT_KIND) == 2 || GT4_U8(layout + LAYOUT_KIND) == 3) {
        /* three wheels: one alone at the front (2) or the rear (3), the
           pair's midpoint against it, the pair's difference for the roll */
        float one, mid, l, r, base;
        if (GT4_U8(layout + LAYOUT_KIND) == 2) {
            one = h[0]; l = h[1]; r = h[2];
            mid = (l + r) * half;
            base = one + (mid - one) * GT4_F32(params + 0x20);
            pitch = gt4_atan2f(one - mid, GT4_F32(params + 0x18));
            GT4_F32(car + CAR_GROUND_PITCH) = pitch;
            roll = gt4_atan2f(l - r, GT4_F32(params + 0x2c) + GT4_F32(params + 0x2c));
        } else {
            one = h[2]; l = h[0]; r = h[1];
            mid = (l + r) * half;
            base = mid + (one - mid) * GT4_F32(params + 0x20);
            pitch = gt4_atan2f(mid - one, GT4_F32(params + 0x18));
            GT4_F32(car + CAR_GROUND_PITCH) = pitch;
            roll = gt4_atan2f(l - r, GT4_F32(params + 0x28) + GT4_F32(params + 0x28));
        }
        GT4_F32(car + CAR_POSITION + 0x38) = base;
        GT4_F32(car + CAR_GROUND_ROLL) = roll;
    }
    if (GT4_U8(layout + LAYOUT_KIND) != 0) {
        gt4_rotation_from_angles(gt4_ptr(car + CAR_140), gt4_ptr(car + CAR_140 + 0xc), gt4_ptr(car + CAR_140 + 0x18),
                                 GT4_F32(car + CAR_GROUND_PITCH), GT4_F32(car + CAR_GROUND_ROLL), GT4_F32(car + CAR_ANGLES + 8));
        return;
    }

    front = (h[0] + h[1]) - (h[2] + h[3]);
    a = h[0] + (h[2] - h[0]) * GT4_F32(params + 0x20);
    b = h[1] + (h[3] - h[1]) * GT4_F32(params + 0x20);
    s = GT4_F32(params + 0x18) * gt4_sinf(GT4_F32(car + CAR_ANGLES));
    pitch = gt4_atan2f(front * half, s);
    s = GT4_F32(params + 0x1c) * gt4_sinf(GT4_F32(car + CAR_ANGLES + 4));
    GT4_F32(car + CAR_GROUND_PITCH) = pitch;
    roll = gt4_atan2f(a - b, s);
    GT4_F32(car + CAR_GROUND_ROLL) = roll;
    GT4_F32(car + CAR_POSITION + 0x38) = (a + b) * half;

    gt4_rotation_from_angles(gt4_ptr(car + CAR_140), gt4_ptr(car + CAR_140 + 0xc), gt4_ptr(car + CAR_140 + 0x18),
                             GT4_F32(car + CAR_GROUND_PITCH), GT4_F32(car + CAR_GROUND_ROLL), GT4_F32(car + CAR_ANGLES + 8));
}

/* func_00344420: mounts, the wheels' speed weights (func_00345A50: ground
   speed / 50, at most 1) and the ground points. */
void gt4_car_wheel_setup(uint32_t car)
{
    int n;
    mount_xy(car);
    n = GT4_U8(gt4_car_layout(car) + LAYOUT_WHEELS);
    for (int i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i);
        float v = GT4_F32(w + WHEEL_VEL_X) * GT4_F32(w + WHEEL_VEL_X) + GT4_F32(w + WHEEL_VEL_Y) * GT4_F32(w + WHEEL_VEL_Y);
        v = (v == 0 ? v : sqrtf(fabsf(v))) * gt4_f32(0x3ca3d70bu);
        GT4_F32(w + WHEEL_40) = 1.0f < v ? 1.0f : v;
    }
    gt4_car_ground_points(car);
}
