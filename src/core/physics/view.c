/* Effects of the body motion on the player's view (camera springs) and the
   reset of the vibration outputs. Part of the simulation step. */
#include "car.h"

#define K(bits) gt4_f32(bits)
#define STEP K(0x3c888888u)   /* 1/60, fixed */

#define VIEW_YAW_K     0x0069f4c8u
#define VIEW_YAW_DAMP  0x0069f4d0u
#define VIEW_SWAY_ON   0x006202b0u   /* byte */
#define VIEW_BOB_ON    0x006202b5u   /* byte */
#define VIEW_SPRINGS   0x006202c8u   /* limit, gain, stiffness, damping; twice */

/* Per-player view block: session + 0xcab8 + player * 0x30. */
static uint32_t view_block(uint32_t car)
{
    return GT4_U32(car + CAR_SESSION) + 0xcab8 + GT4_U8(car + CAR_PLAYER) * 0x30;
}

/* func_00345658: the view follows yaw and pitch through two springs; the
   lag is kept in CAR_VIEW_YAW_LAG and CAR_VIEW_PITCH_LAG. */
void gt4_car_view_follow(uint32_t car)
{
    uint32_t b = view_block(car);
    float d, rate, a;

    d = gt4_angle_diff(GT4_F32(car + CAR_ANGLES + 8), GT4_F32(b + 0));
    d = d * GT4_F32(VIEW_YAW_K) - GT4_F32(b + 4) * GT4_F32(VIEW_YAW_DAMP);
    rate = GT4_F32(b + 4) + (GT4_F32(b + 8) + d) * K(0x3f000000u) * STEP;
    GT4_F32(b + 8) = d;
    GT4_F32(b + 4) = rate;
    a = GT4_F32(b + 0) + rate * STEP;
    GT4_F32(b + 0) = a;
    GT4_F32(b + 0) = gt4_angle_wrap(a);
    GT4_F32(car + CAR_VIEW_YAW_LAG) = gt4_angle_diff(GT4_F32(car + CAR_ANGLES + 8), GT4_F32(b + 0));

    d = gt4_angle_diff(GT4_F32(car + CAR_ANGLES), GT4_F32(b + 0xc));
    rate = GT4_F32(b + 0x10) + (d * K(0x41a00000u) - GT4_F32(b + 0x10) * K(0x41200000u)) * STEP;   /* 20, 10 */
    GT4_F32(b + 0x10) = rate;
    a = GT4_F32(b + 0xc) + rate * STEP;
    GT4_F32(b + 0xc) = a;
    GT4_F32(b + 0xc) = gt4_angle_wrap(a);
    GT4_F32(car + CAR_VIEW_PITCH_LAG) = gt4_angle_diff(GT4_F32(car + CAR_ANGLES), GT4_F32(b + 0xc));
}

/* func_00344858 and relatives: a stored value blended with a body angle by
   CAR_VIEW_BLEND. */
static float blend(uint32_t car, uint32_t stored, uint32_t angle)
{
    float t = 0 + GT4_F32(car + CAR_VIEW_BLEND);
    return GT4_F32(car + stored) * t + GT4_F32(car + angle) * (K(0x3f800000u) - t);
}

static void view_spring(uint32_t b, uint32_t spring, float x)
{
    float lim = GT4_F32(spring), pos = GT4_F32(b + 4), vel = GT4_F32(b + 0);
    if (lim < x) x = lim;
    else if (x < -lim) x = -lim;
    vel = vel + (GT4_F32(spring + 8) * (x * GT4_F32(spring + 4) - pos) - GT4_F32(spring + 0xc) * vel) * STEP;
    GT4_F32(b + 0) = vel;
    GT4_F32(b + 4) = pos + vel * STEP;
}

/* func_0034A668: view sway from a body angle difference. */
void gt4_car_view_sway(uint32_t car)
{
    if (!GT4_U8(VIEW_SWAY_ON)) return;
    view_spring(view_block(car) + 0x14, VIEW_SPRINGS,
                blend(car, CAR_8BC, CAR_ANGLES) - blend(car, CAR_8BC + 0xc, CAR_67C - 4));
}

/* func_0034A788: the same for the other axis. */
void gt4_car_view_bob(uint32_t car)
{
    if (!GT4_U8(VIEW_BOB_ON)) return;
    view_spring(view_block(car) + 0x28, VIEW_SPRINGS + 0x10,
                blend(car, CAR_8BC + 4, CAR_ANGLES + 4) - blend(car, CAR_8BC + 0x10, CAR_67C));
}

/* func_00366678: vibration mode (3 = none). */
static int vibration_mode(uint32_t car)
{
    int8_t mode = (int8_t)GT4_U8(car + CAR_56A);
    uint32_t c;
    uint8_t b;
    if (!GT4_U8(car + CAR_VIBRATION_ON)) return 3;
    if (mode == 4) return GT4_U8(car + CAR_5B6) ? 0 : 2;
    c = GT4_U32(car);
    if (!GT4_U32(c + 0xe0) && !GT4_U32(GT4_U32(c + 0x84) + 0x10000 - 0xfb0)) return 1;
    if (mode == 2) return 0;
    b = GT4_U8(car + CAR_56B);
    if (b == 6 || b == 8 || b == 1) return 0;
    return 2;
}

int gt4_car_presence(uint32_t car) { return vibration_mode(car); }

/* cvt.w.s: truncated, saturating as the EE does. */
static int32_t cvtw(float x)
{
    if (!(x < 2147483648.0f)) return 0x7fffffff;
    if (!(-2147483648.0f < x)) return (int32_t)0x80000000u;
    return (int32_t)x;
}

static int32_t clamp127(int32_t v) { return v < 128 ? v : 127; }

/* func_00353EB8: the car's pad outputs in the race (cond+0xCA58, 16 bytes
   per player slot car+0x544). */
static uint32_t pad_out(uint32_t car)
{
    return GT4_U32(car + 4) + 0xca58u + GT4_U8(car + 0x544) * 16u;
}

/* func_0036CEE8: the pad's small motor (out[1]) from the front wheels'
   forces and slip (+0x70, +0x74, +0x38), the kerbs under them (surfaces 1,
   10, 11, 21: bump +0x48 by speed), each wheel's jolt (+0xA0) and a hit
   (car+0x714); off (both motors) while pitting or the model drives. */
void gt4_car_vibration_wheels(uint32_t car)
{
    uint32_t b = car + 0x104, lay = gt4_car_layout(car);
    int n = GT4_U8(lay + LAYOUT_WHEELS), count = 0;
    float side = 0, drive = 0, slip = 0, jolt = 0, hit = 0, kerb = 0, v;
    if ((uint8_t)(GT4_U8(b + 0x466) - 1) < 2 || GT4_U8(b + 0x467)) {
        GT4_U8(pad_out(car) + 1) = 0;
        GT4_U8(pad_out(car)) = 0;
        return;
    }
    for (int i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i);
        if (!(0 < (int8_t)GT4_U8(lay + LAYOUT_HANDBRAKE + i))) continue;
        count++;
        slip = slip + GT4_F32(w + 0x38) * GT4_F32(w + 0x38);
        drive = drive + GT4_F32(w + 0x74);
        side = side + GT4_F32(w + 0x70);
    }
    if (count) side = side * (slip / (float)count + K(0x3dccccccu));
    if (side < 0) side = -side;
    side = side * K(0x3a2a7ef9u);
    if (K(0x3eaa7ef9u) < side) side = K(0x3eaa7ef9u);
    drive = drive / K(0x46991fffu);
    drive = drive * drive;
    if (K(0x3eaa7ef9u) < drive) drive = K(0x3eaa7ef9u);
    side = side + drive;
    if (K(0x3c23d70au) < GT4_F32(b + 0x610)) hit = GT4_F32(b + 0x610) * K(0x41000000u) + K(0x3e800000u);
    for (int i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i);
        uint8_t sf;
        float f;
        if (GT4_U8(b + 0x4bc)) continue;
        f = GT4_F32(w + 0xa0);
        if (0 < f) {
            f = f / K(0x46991fffu);
            jolt = jolt + (K(0x3f800000u) < f ? K(0x3f800000u) : f);
        }
        if (!(0 < (int8_t)GT4_U8(lay + LAYOUT_HANDBRAKE + i))) continue;
        sf = GT4_U8(w + 0x89);
        if (sf != 1 && sf != 10 && sf != 11 && sf != 21) continue;
        f = (GT4_F32(w + 0x48) / K(0x4537bfffu)) * (GT4_F32(b + 0x53c) * K(0x3c800000u));
        if (f < 0) f = -f;
        kerb = kerb + (K(0x3e800000u) < f ? K(0x3e800000u) : f);
    }
    v = jolt + kerb;
    if (v < side) v = side;
    if (v < hit) v = hit;
    GT4_U8(pad_out(car) + 1) = (uint8_t)clamp127(cvtw(v * K(0x43800000u)));
}

/* func_00350080: the engine's vibration (car+0x708: the throttle against
   the engine's load, its revs near the limiter, a decaying term; scales at
   D_00843694..A0), halved with drive kind 3 (hybrid/electric: +0x12 of the
   drivetrain), silent past its top revs; cleared unless the vibration
   mode is 3. */
void gt4_car_vibration_engine(uint32_t car)
{
    uint32_t b = car + 0x104, params = GT4_U32(car + CAR_PARAMS), d = params + 0x234;
    int quiet = GT4_U8(d + 0x12) == 3;
    float lim, t, load, over, e, x;
    int32_t q;
    if (vibration_mode(car) != 3) {
        GT4_U32(b + 0x504) = 0;
        GT4_U8(b + 0x605) = 0;
        GT4_U8(b + 0x606) = 0;
        GT4_U8(b + 0x604) = 0;
        return;
    }
    lim = (float)(int32_t)GT4_U16(params + 0x202) * K(0x3dd6774eu);
    if (quiet && GT4_F32(b + 0x4fc) < lim) {
        GT4_U8(b + 0x604) = 0;
        GT4_U8(b + 0x605) = 0;
        return;
    }
    t = GT4_F32(b + 0x4f0);
    if (K(0x3f800000u) < t) t = K(0x3f800000u);
    load = GT4_F32(b + 0x4ec) / (GT4_F32(b + 0x4ec) + GT4_F32(b + 0x4e8));
    over = t - load;
    e = (over < 0 ? -over : over) + load;
    if (GT4_U8(b + 0x522) != 1) e = e * K(0x3f000000u);
    if (quiet) e = e * K(0x3f000000u);
    q = clamp127(cvtw((e * K(0x42fe0000u)) * GT4_F32(0x00843694u)));
    GT4_U8(b + 0x604) = (uint8_t)(((int32_t)GT4_U8(b + 0x604) + q) / 2);
    if (over < 0) over = 0;
    x = K(0x3f800000u) - over / (K(0x3f800000u) - load);
    x = x * x;
    x = x * x;
    x = x * x;
    x = x * x;
    x = (K(0x3f800000u) - x) * K(0x3f199999u) + K(0x3eccccccu);
    if (quiet) x = x * K(0x3f000000u);
    q = clamp127(cvtw((x * K(0x42fe0000u)) * GT4_F32(0x00843698u)));
    GT4_U8(b + 0x605) = (uint8_t)(((int32_t)GT4_U8(b + 0x605) + q) / 2);
    GT4_U8(b + 0x606) = (uint8_t)clamp127(cvtw((float)(int32_t)GT4_U8(b + 0x606) * GT4_F32(0x0084369cu)));
    GT4_F32(b + 0x504) = GT4_F32(b + 0x504) * GT4_F32(0x008436a0u);
}

void (*gt4_wheel_smoke)(void *user, uint32_t car, uint32_t e);
void *gt4_wheel_smoke_user;
void (*gt4_wheel_leaves)(void *user, uint32_t car, uint32_t e, uint32_t vel);

/* func_003667A8: what the effects need of wheel i (a block of 0x3C bytes:
   the wheel, its surface, the point, the heading, the ground's height,
   normal and the point below, and the wheel's slip and forces) handed to
   the car's effects (car+8): the smoke (func_00395C70) and, on surfaces
   marked 0x20, the marks (func_00395C98, with the car's velocity). */
static void wheel_effects(uint32_t car, int i, const float *p, uint32_t vel)
{
    uint32_t w = gt4_car_wheel(car, i), g = car + CAR_GROUND + (uint32_t)i * GROUND_SIZE;
    uint32_t e = gt4_phys_scratch + 0x7a0;
    GT4_U32(e) = (uint32_t)i;
    GT4_U32(e + 4) = GT4_U8(w + 0x89);
    GT4_F32(e + 8) = p[0];
    GT4_F32(e + 0xc) = p[1];
    GT4_F32(e + 0x10) = p[2];
    GT4_F32(e + 0x14) = GT4_F32(car + 0x104 + 0x570) + GT4_F32(w);
    GT4_F32(e + 0x18) = GT4_F32(g + 0x14);
    for (int k = 0; k < 4; k++) GT4_F32(e + 0x1c + 4 * k) = GT4_F32(g + 0x1c + 4 * k);
    GT4_F32(e + 0x28) = GT4_F32(e + 0x28) - GT4_F32(e + 0x20) * K(0x3d4ccccc);
    GT4_F32(e + 0x2c) = GT4_F32(w + 0xbc);
    GT4_F32(e + 0x30) = GT4_F32(w + 0x38);
    GT4_F32(e + 0x34) = GT4_F32(w + 0x28);
    GT4_F32(e + 0x38) = GT4_F32(w + 0x20);
    if (gt4_wheel_smoke) gt4_wheel_smoke(gt4_wheel_smoke_user, car, e);
    if (gt4_wheel_leaves && (GT4_U8(g + 0x33) & 0x20)) gt4_wheel_leaves(gt4_wheel_smoke_user, car, e, vel);
    if (gt4_phys_headless) return;
    gt4_orig_a2 = GT4_U8(car + 0x104 + 0x440);
    gt4_orig_a3 = g + 0xc;
    gt4_orig_call(0x00395c70u, GT4_U32(car + 8), e, 0, 0);
    if (GT4_U8(g + 0x33) & 0x20) {
        gt4_orig_a2 = vel;
        gt4_orig_a3 = g + 0xc;
        gt4_orig_call(0x00395c98u, GT4_U32(car + 8), e, 0, 0);
    }
}

/* func_003668F0: the effects at each wheel's contact point; params+0x32 =
   6 cars at two points, each between a pair of wheels (layout +0xC and
   its partner), named by the one whose ground is lower. */
void gt4_car_wheel_effects(uint32_t car)
{
    uint32_t b = car + 0x104, lay = gt4_car_layout(car), vel;
    float p[3];
    if (!gt4_phys_scratch) {
        GT4_ORIG(0x003668f0u, car);
        return;
    }
    vel = gt4_phys_scratch + 0x7e0;
    GT4_F32(vel) = GT4_F32(car + 0x648);
    GT4_F32(vel + 4) = GT4_F32(car + 0x650);
    GT4_F32(vel + 8) = -GT4_F32(car + 0x64c);
    if (GT4_U8(GT4_U32(car + CAR_PARAMS) + 0x32) == 6) {
        for (int k = 0; k < 2; k++) {
            int a = GT4_U8(lay + 0xc + k), c = GT4_U8(lay + 0x24 + a), t;
            uint32_t wa, wc;
            if (GT4_F32(car + 0x7cc + (uint32_t)a * 0x34) < GT4_F32(car + 0x7cc + (uint32_t)c * 0x34)) {
                t = a;
                a = c;
                c = t;
            }
            wa = gt4_car_wheel(car, a);
            wc = gt4_car_wheel(car, c);
            p[0] = GT4_F32(b) + (GT4_F32(wa + 0xc) + GT4_F32(wc + 0xc)) * K(0x3f000000u);
            p[1] = (GT4_F32(wa + 0x8c) + GT4_F32(wc + 0x8c)) * K(0x3f000000u);
            p[2] = -(GT4_F32(b + 4) + (GT4_F32(wa + 0x10) + GT4_F32(wc + 0x10)) * K(0x3f000000u));
            wheel_effects(car, a, p, vel);
        }
        return;
    }
    for (int i = 0; i < GT4_U8(lay + LAYOUT_WHEELS); i++) {
        uint32_t w = gt4_car_wheel(car, i);
        p[0] = GT4_F32(b) + GT4_F32(w + 0xc);
        p[1] = GT4_F32(w + 0x8c);
        p[2] = -(GT4_F32(b + 4) + GT4_F32(w + 0x10));
        wheel_effects(car, i, p, vel);
    }
}

/* func_00366718: clears the per-wheel vibration terms and outputs. */
void gt4_car_vibration_reset(uint32_t car)
{
    if (vibration_mode(car) == 3) return;
    for (int i = 0; i < gt4_car_wheels(car); i++) {
        uint32_t w = gt4_car_wheel(car, i);
        GT4_F32(w + WHEEL_BC) = 0;
        GT4_F32(w + WHEEL_B4) = 0;
        GT4_F32(w + WHEEL_B8) = K(0x3f800000u);
    }
    GT4_U8(car + CAR_VIBRATION) = 0;
    GT4_U8(car + CAR_VIBRATION + 1) = 0;
    GT4_U8(car + CAR_VIBRATION + 2) = 0;
}
