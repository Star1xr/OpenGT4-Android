/* Suspension: wheel travel against the ground, springs, dampers, bump stops,
   anti-roll bars and the load each wheel asks for (func_0036A490,
   func_0036A758 and their helpers). */
#include "car.h"

#include <math.h>

#define K(bits) gt4_f32(bits)
#define ONE K(0x3f800000u)

#define DAMPER_LINEAR 0x00620dd9u   /* byte */
#define DAMPER_BLEND  0x00620cb0u   /* word: blend the dampers towards a square law */
#define DAMPER_BUMP_V 0x00620cb8u   /* [axle] */
#define DAMPER_BUMP_K 0x00620cc0u   /* [axle] */
#define DAMPER_REB_V  0x00620cc8u   /* [axle] */
#define DAMPER_REB_K  0x00620cd0u   /* [axle] */
#define SIMPLE_TRANSFER 0x00620e11u /* byte: no lateral load transfer term */
#define SETTINGS      0x00620cd8u

/* PS2 sqrt: of the magnitude, keeping a zero's sign. */
static float ps2_sqrt(float x) { return x == 0 ? x : sqrtf(fabsf(x)); }

static uint32_t axle_block(uint32_t params, int axle) { return params + PARAMS_AXLES + axle * AXLE_SIZE; }

/* func_003696C8: travel of wheel i where the geometry puts it. */
static float wheel_rest(uint32_t car, int i)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS);
    int axle = gt4_wheel_axle(gt4_car_layout(car), i);
    uint32_t b = axle_block(params, axle);
    float d = GT4_F32(params + PARAMS_2C4 + axle * 4) - GT4_F32(params + PARAMS_2DC + axle * 4);
    return (-GT4_F32(b + AXLE_10) - d) + GT4_F32(b + AXLE_0C);
}

/* func_00369760: coordinate k of wheel i's mount, moved along the body's
   third axis by `along`. */
static float wheel_mount(uint32_t car, int i, int k, float along)
{
    return (GT4_F32(car + CAR_POSITION + 4 * k) + GT4_F32(gt4_car_wheel(car, i) + WHEEL_MOUNT + 4 * k))
           + GT4_F32(car + CAR_MATRIX + 0x18 + 4 * k) * along;
}

/* func_0036A080: suspension travel each wheel needs to reach the ground. */
void gt4_car_suspension_update_reach(uint32_t car)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), layout = gt4_car_layout(car);
    for (int i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        uint32_t w = gt4_car_wheel(car, i);
        float rest = wheel_rest(car, i), z = wheel_mount(car, i, 2, rest), d;
        int axle = gt4_wheel_axle(layout, i);
        d = (z - (GT4_F32(w + WHEEL_8C) + GT4_F32(w + WHEEL_90))) * GT4_F32(car + CAR_160) / GT4_F32(w + WHEEL_C8);
        GT4_F32(car + CAR_SUSP_REACH + 4 * i) = ((d - GT4_F32(params + PARAMS_2C4 + axle * 4))
            + GT4_F32(axle_block(params, axle) + AXLE_0C)) - rest;
    }
}

/* func_0036A1D8: adds a bit per wheel whose ground is beyond its travel. */
uint8_t gt4_car_suspension_short(uint32_t car, uint8_t flags)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), layout = gt4_car_layout(car);
    int n = GT4_U8(layout + LAYOUT_WHEELS);
    for (int i = 0; i < n; i++) {
        int axle = gt4_wheel_axle(layout, i);
        if (GT4_F32(car + CAR_SUSP_REACH + 4 * i) < GT4_F32(axle_block(params, axle) + AXLE_MIN)) flags |= (uint8_t)(1 << i);
    }
    return flags;
}

/* func_0036A490: ground normal against the body, then the reach and the
   short flags (CAR_SUSP_SHORT). */
void gt4_car_suspension_reach(uint32_t car)
{
    uint32_t layout = gt4_car_layout(car);

    GT4_F32(car + CAR_6B8) = 0;
    for (int k = 0; k < 3; k++)
        GT4_F32(car + CAR_6B8) = GT4_F32(car + CAR_6B8) + GT4_F32(car + CAR_158 + 4 * k) * GT4_F32(car + CAR_MATRIX + 0x18 + 4 * k);
    for (int i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        uint32_t g = car + CAR_GROUND + i * GROUND_SIZE;
        float c = GT4_F32(g + 0x1c) * GT4_F32(car + CAR_MATRIX + 0x18);
        c = c - GT4_F32(g + 0x24) * GT4_F32(car + CAR_MATRIX + 0x1c);
        c = c + GT4_F32(g + 0x20) * GT4_F32(car + CAR_MATRIX + 0x20);
        GT4_F32(gt4_car_wheel(car, i) + WHEEL_C8) = c;
    }
    gt4_car_suspension_update_reach(car);
    GT4_U8(car + CAR_SUSP_SHORT) = gt4_car_suspension_short(car, 0);
}

/* func_00369B48: damper force for the suspension speed v (clamped to
   +-1 m/s): square-root law from the axle block, optionally blended
   towards a square law below a speed per axle. */
static float damper(uint32_t b, int axle, float v)
{
    float f, g, t;
    if (GT4_U8(DAMPER_LINEAR)) {
        /* linear: the V0/C pairs a knee, K the speed of the maximum
           force (+0x48 compression, +0x4C extension) */
        if (v < 0) {
            float top = GT4_F32(b + AXLE_BUMP_K), knee = GT4_F32(b + AXLE_BUMP_V0), c = GT4_F32(b + AXLE_BUMP_C);
            if (v <= -top) return GT4_F32(b + 0x48);
            if (-knee <= v) return (-v * c) / knee;
            return c + ((-v - knee) * (GT4_F32(b + 0x48) - c)) / (top - knee);
        }
        if (0 < v) {
            float top = GT4_F32(b + AXLE_REB_K), knee = GT4_F32(b + AXLE_REB_V0), c = GT4_F32(b + AXLE_REB_C);
            if (top <= v) return -GT4_F32(b + 0x4c);
            if (v <= knee) return (-v * c) / knee;
            return -(c + ((v - knee) * (GT4_F32(b + 0x4c) - c)) / (top - knee));
        }
        return 0;
    }
    if (v < 0) {
        if (v < -ONE) v = -ONE;
        f = GT4_F32(b + AXLE_BUMP_C) + GT4_F32(b + AXLE_BUMP_K) * ps2_sqrt(-v - GT4_F32(b + AXLE_BUMP_V0));
        if (GT4_U32(DAMPER_BLEND)) {
            t = GT4_F32(DAMPER_BUMP_V + axle * 4);
            if (-t < v) {
                float k = GT4_F32(DAMPER_BUMP_K + axle * 4);
                g = GT4_F32(b + AXLE_BUMP_C) + GT4_F32(b + AXLE_BUMP_K) * ps2_sqrt(t - GT4_F32(b + AXLE_BUMP_V0));
                f = (ONE - k) * f + k * (g * (v * v) / ((-t) * (-t)));
            }
        }
        return f;
    }
    if (ONE < v) v = ONE;
    {
        float c = -GT4_F32(b + AXLE_REB_C);
        f = c - GT4_F32(b + AXLE_REB_K) * ps2_sqrt(v - GT4_F32(b + AXLE_REB_V0));
        if (GT4_U32(DAMPER_BLEND)) {
            t = GT4_F32(DAMPER_REB_V + axle * 4);
            if (v < t) {
                float k = GT4_F32(DAMPER_REB_K + axle * 4);
                g = c - GT4_F32(b + AXLE_REB_K) * ps2_sqrt(t - GT4_F32(b + AXLE_REB_V0));
                f = (ONE - k) * f + k * (g * (v * v) / (t * t));
            }
        }
    }
    return f;
}

/* func_0035D248: spring rate of wheel i's axle. */
static float spring_rate(uint32_t car, int i)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS);
    float k = GT4_F32(axle_block(params, gt4_wheel_axle(gt4_car_layout(car), i)) + AXLE_SPRING);
    if (GT4_U8(params + 0x32) == 1) {
        /* the cars with damage lights: a spring gives way with the damage
           count at car+0x788, one per 7, the wheels in the car's order */
        int n = GT4_U8(car + 0x788), at, q;
        if (!n) return k;
        at = i + GT4_U8(car + 0x104 + 0x440);
        q = at % 4;
        if (!((GT4_U8(car + 0x104 + 0x440) & 7) < 4)) q = 3 - q;
        if (n < q * 7) return k;
        n = 1 - (n - q * 7);
        if (n < 0) n = 0;
        return k * (float)n;
    }
    return k;
}

/* func_003F24A0: the active suspension's lift for wheel i (the setting
   D_00622268 + 32 * params+0x1076: type 2 with the speed between two
   points, type 3 on the surfaces D_00620288 marks off). */
static float active_lift(uint32_t car, int i)
{
    uint32_t a = 0x00622268u + GT4_U8(GT4_U32(car + CAR_PARAMS) + 0x1076) * 32u;
    uint8_t type = GT4_U8(a);
    if (type == 2) {
        float v = GT4_F32(car + CAR_SPEED), hi = GT4_F32(a + 0x1c), lo;
        if (hi <= v) return GT4_F32(a + 0x14);
        lo = GT4_F32(a + 0x18);
        if (!(lo < v)) return 0;
        return (GT4_F32(a + 0x14) * (v - lo)) / (hi - lo);
    }
    if (type == 3) {
        if (!GT4_U8(0x00620288u + GT4_U8(gt4_car_wheel(car, i) + 0x89))) return GT4_F32(a + 0x14);
        return 0;
    }
    return 0;
}

/* func_003F2650: the active suspension's force for wheel i at travel
   `at`: a bump stop of its own below the axle's bump point, the lift
   against a spring toward the ride height. */
static float active_damp(uint32_t car, int i, float at)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), a = 0x00622268u + GT4_U8(params + 0x1076) * 32u, b;
    int axle = i >> 1;
    float bump = 0, f = 0, lift, x, d;
    if (!GT4_U8(a)) return 0;
    b = axle_block(params, axle);
    d = at - GT4_F32(b + AXLE_BUMP);
    if (d < 0) bump = -((d * d) * GT4_F32(b + AXLE_BUMP_RATE));
    x = at - GT4_F32(b + AXLE_10);
    lift = active_lift(car, i);
    x = x + lift;
    if (x < 0) {
        float lim = -GT4_F32(a + 4), g = GT4_F32(a + 8) / GT4_F32(a + 0xc + (uint32_t)axle * 4);
        if (lim < x) g = (g * x) / lim;
        f = (g * GT4_F32(params + 0x1040 + (uint32_t)axle * 4)) * K(0x3f000000u);
    }
    f = f - GT4_F32(b + AXLE_SPRING) * lift;
    return f + bump;
}

/* func_00369DE8: moves the wheel's unsprung mass, then spring, bump stop and
   damper against the ground at `reach`; the wheel load is what remains. */
static void wheel_spring(uint32_t car, uint32_t w, uint32_t b, int i, int axle, float reach, float k)
{
    float dt = GT4_F32(car + CAR_DT), top = GT4_F32(b + AXLE_MAX), old = GT4_F32(w + WHEEL_TRAVEL), pos;
    if (GT4_U16(car + CAR_5BE)) top = GT4_F32(b + AXLE_MAX_ALT);

    pos = old + (GT4_F32(w + WHEEL_TRAVEL_V) + (GT4_F32(w + WHEEL_SPRING) + GT4_F32(w + WHEEL_DAMPER))
                 / GT4_F32(b + AXLE_MASS) * K(0x3f000000u) * dt) * dt;
    if (top < pos) pos = top;
    else if (pos < GT4_F32(b + AXLE_MIN)) pos = GT4_F32(b + AXLE_MIN);

    if (pos < reach) {
        /* the wheel hangs above the ground */
        float v;
        GT4_F32(w + WHEEL_SPRING) = -pos * k;
        if (pos == top) {
            GT4_F32(w + WHEEL_TRAVEL_V) = 0;
        } else {
            v = GT4_F32(car + CAR_RATE) * (pos - old);
            GT4_F32(w + WHEEL_TRAVEL_V) = v;
            if (ONE < v) GT4_F32(w + WHEEL_TRAVEL_V) = ONE;
            else if (v < -ONE) GT4_F32(w + WHEEL_TRAVEL_V) = -ONE;
        }
        GT4_F32(w + WHEEL_DAMPER) = damper(b, axle, GT4_F32(w + WHEEL_TRAVEL_V));
        GT4_F32(w + WHEEL_TRAVEL) = pos;
        GT4_F32(w + WHEEL_LOAD) = 0;
        return;
    }
    {
        float at = GT4_F32(b + AXLE_MIN), bump, spring, v, damp, load;
        if (!(reach < at)) at = reach;
        bump = at - GT4_F32(b + AXLE_BUMP);
        spring = -at * k;
        if (bump < 0) spring = spring + bump * bump * GT4_F32(b + AXLE_BUMP_RATE);
        v = GT4_F32(car + CAR_RATE) * (reach - old);
        GT4_F32(w + WHEEL_TRAVEL_V) = v;
        damp = damper(b, axle, v);
        if (v < 0 && bump < 0) {
            float c = v < -ONE ? -ONE : v;
            damp = damp - GT4_F32(b + AXLE_BUMP_DAMP) * c;
        }
        if (GT4_U8(GT4_U32(car + CAR_PARAMS) + 0x1076))
            damp = damp + active_damp(car, i, at);
        GT4_F32(w + WHEEL_TRAVEL) = at;
        GT4_F32(w + WHEEL_SPRING) = spring;
        GT4_F32(w + WHEEL_DAMPER) = damp;
        load = (spring + damp) + GT4_F32(b + AXLE_UNSPRUNG);
        GT4_F32(w + WHEEL_LOAD) = load;
        if (load < 0) {
            GT4_F32(w + WHEEL_LOAD) = 0;
            GT4_F32(w + WHEEL_DAMPER) = 0;
            GT4_F32(w + WHEEL_SPRING) = -GT4_F32(b + AXLE_UNSPRUNG);
        }
    }
}

/* func_0036A690: lateral position of wheel i's contact (half track,
   pulled in by the suspension arm as it swings with the travel). */
static float contact_y(uint32_t params, int i, float angle, float travel)
{
    uint32_t layout = GT4_LAYOUTS + GT4_U8(params + PARAMS_LAYOUT) * 84u;
    uint8_t side = GT4_U8(layout + LAYOUT_SIDE + i);
    int pos;
    float arm, t;
    (void)angle;
    if (side == 2) return 0;
    pos = GT4_U8(layout + LAYOUT_POSITION + i);
    arm = GT4_F32(params + PARAMS_ARM + pos * 4);
    t = GT4_F32(params + PARAMS_HALF_TRACK + pos * 4);
    if (arm != 0) t = t - (ONE - gt4_sinf(gt4_atan2f(travel, arm))) * arm;
    if (side == 0) t = -t;
    return t;
}

/* func_0036A578: camber of wheel i against its travel (and steering, for
   front wheels when params 0x1004 is set). */
static float camber(uint32_t params, int i, float steer, float travel)
{
    uint32_t layout = GT4_LAYOUTS + GT4_U8(params + PARAMS_LAYOUT) * 84u;
    int n = GT4_U8(layout + LAYOUT_WHEELS), axle = gt4_wheel_axle(layout, i);
    float c = GT4_F32(params + PARAMS_CAMBER + axle * 4) + GT4_F32(params + PARAMS_CAMBER_GAIN + axle * 4) * travel, s;
    if (!(i < n && (int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i) > 0)) return c;
    if (GT4_F32(params + PARAMS_CASTER) == 0) return c;
    s = steer * GT4_F32(params + PARAMS_CASTER);
    if (GT4_U8(layout + LAYOUT_DIFF_WHEEL_B + i) != 0x7f) {
        if (0 <= s) s = i ? -(GT4_F32(params + 0x110) * s) : s * GT4_F32(params + 0x10c);
        else s = i ? -(GT4_F32(params + 0x10c) * s) : s * GT4_F32(params + 0x110);
    }
    return c + s;
}

/* func_0036A048: anti-roll bar force from the travel difference. */
static float anti_roll(uint32_t wa, uint32_t wb, uint32_t b)
{
    float d = GT4_F32(wa + WHEEL_TRAVEL) - GT4_F32(wb + WHEEL_TRAVEL), k = GT4_F32(b + AXLE_ARB);
    if (0 <= d) return k * d * d;
    return -k * d * d;
}

void gt4_car_suspension(uint32_t car)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), layout = gt4_car_layout(car);
    float bar[4] = { 0, 0, 0, 0 }, lat, lon;
    int i, k;

    /* vertical speed of each wheel mount */
    for (i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        uint32_t w = gt4_car_wheel(car, i), g = car + CAR_GROUND + i * GROUND_SIZE;
        int pos = GT4_U8(layout + LAYOUT_POSITION + i);
        uint8_t side = GT4_U8(layout + LAYOUT_SIDE + i);
        float a = GT4_F32(params + PARAMS_HALF_TRACK + pos * 4) * GT4_F32(car + CAR_ROLL_RATE);
        float b = GT4_F32(params + PARAMS_AXLE_DIST + pos * 4) * GT4_F32(car + CAR_PITCH_RATE);
        float r = 0, vx, vy, vz;
        if (side == 0) r = a + r;
        else if (side == 1) r = r - a;
        if (i < GT4_U8(layout + LAYOUT_WHEELS) && (int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i) > 0) r = r + b;
        else if (i < GT4_U8(layout + LAYOUT_WHEELS) && (int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i) < 0) r = r - b;
        vx = GT4_F32(car + CAR_VELOCITY) + GT4_F32(car + CAR_MATRIX + 0x18) * r;
        vy = GT4_F32(car + CAR_VELOCITY + 4) + GT4_F32(car + CAR_MATRIX + 0x1c) * r;
        vz = GT4_F32(car + CAR_VELOCITY + 8) + GT4_F32(car + CAR_MATRIX + 0x20) * r;
        GT4_F32(w + WHEEL_MOUNT_V) = (vx * GT4_F32(g + 0x1c) + vy * -GT4_F32(g + 0x24)) + vz * GT4_F32(g + 0x20);
    }

    /* springs and dampers */
    for (i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        uint32_t w = gt4_car_wheel(car, i);
        int axle = i < GT4_U8(layout + LAYOUT_WHEELS) ? GT4_U8(layout + LAYOUT_AXLE + i) : 0;
        uint32_t b = axle_block(params, axle);
        float reach = GT4_F32(car + CAR_SUSP_REACH + 4 * i), travel;
        uint8_t contact = 0 < GT4_F32(w + WHEEL_LOAD);

        wheel_spring(car, w, b, i, axle, reach, spring_rate(car, i));
        if (!contact) {
            if (0 < GT4_F32(w + WHEEL_LOAD)) contact = 2;              /* touched down */
        } else if (GT4_F32(w + WHEEL_LOAD) == 0) {
            contact = 3;                                               /* lifted off */
        }
        GT4_U8(w + WHEEL_CONTACT) = contact;

        travel = GT4_F32(w + WHEEL_TRAVEL) - GT4_F32(b + AXLE_10);
        GT4_F32(w + WHEEL_CONTACT_Y) = contact_y(params, i, GT4_F32(car + CAR_56C), travel);
        if (GT4_U8(layout + LAYOUT_KIND) != 1) {
            GT4_F32(w + WHEEL_CAMBER) = camber(params, i, GT4_F32(car + CAR_56C), travel);
        } else {
            float s = GT4_F32(params + PARAMS_CASTER) * gt4_sinf(GT4_F32(car + CAR_ANGLES + 4) - GT4_F32(car + CAR_67C));
            GT4_F32(w + WHEEL_CAMBER) = GT4_F32(car + CAR_56C) * s;
        }
    }

    /* anti-roll bars */
    for (k = 0; k < GT4_U8(layout + LAYOUT_DIFFS); k++) {
        uint8_t a = GT4_U8(layout + LAYOUT_DIFF_WHEEL_A + k), bw = GT4_U8(layout + LAYOUT_DIFF_WHEEL_B + a);
        int idx;
        bar[k] = 0;
        if (bw == 0x7f) continue;
        idx = k < GT4_U8(layout + LAYOUT_DIFFS) ? GT4_U8(layout + LAYOUT_DIFF_INDEX + k) : 0;
        bar[k] = anti_roll(gt4_car_wheel(car, a), gt4_car_wheel(car, bw), axle_block(params, idx));
    }

    /* load each wheel asks for: spring, bar, downforce and load transfer */
    lat = 0;
    if (!GT4_U8(SIMPLE_TRANSFER)) lat = GT4_F32(params + PARAMS_ROLL_TRANSFER) * -GT4_F32(car + CAR_LAT_ACCEL);
    GT4_F32(car + CAR_6BC) = 0;
    if (GT4_U8(layout + LAYOUT_KIND) == 1)
        GT4_F32(car + CAR_6BC) = GT4_F32(car + CAR_LAT_ACCEL) * GT4_F32(car + CAR_MASS_A) * GT4_F32(car + CAR_MATRIX + 0x20);
    lon = GT4_F32(params + PARAMS_PITCH_TRANSFER) * GT4_F32(car + CAR_668);
    for (i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        uint32_t w = gt4_car_wheel(car, i);
        int n = GT4_U8(layout + LAYOUT_WHEELS);
        uint8_t side = GT4_U8(layout + LAYOUT_SIDE + i);
        float f = GT4_F32(w + WHEEL_SPRING), t = lon;

        if (GT4_F32(w + WHEEL_LOAD) == 0) {
            int axle = i < n ? GT4_U8(layout + LAYOUT_AXLE + i) : 0;
            f = -GT4_F32(axle_block(params, axle) + AXLE_UNSPRUNG);
        }
        if (GT4_U8(layout + LAYOUT_DIFF_WHEEL_B + i) != 0x7f) {
            float r = bar[GT4_U8(layout + LAYOUT_POSITION + i)];
            if (side == 0) f = f - r;
            else if (side == 1) f = f + r;
        }
        if (i < n && (int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i) > 0) {
            float c = (float)GT4_U8(layout + LAYOUT_FRONT_WHEELS);
            f = f - GT4_F32(car + CAR_DOWNFORCE) / c;
            if (GT4_U8(layout + LAYOUT_FRONT_WHEELS) != 2) t = lon * (K(0x40000000u) / c);
            f = f + t;
        } else if (i < n && (int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i) < 0) {
            float c = (float)GT4_U8(layout + LAYOUT_REAR_WHEELS);
            f = f - GT4_F32(car + CAR_DOWNFORCE + 4) / c;
            if (GT4_U8(layout + LAYOUT_REAR_WHEELS) != 2) t = lon * (K(0x40000000u) / c);
            f = f - t;
        }
        if (GT4_U8(layout + LAYOUT_DIFF_WHEEL_B + i) == 0x7f) {
            GT4_F32(car + CAR_LOAD_TARGET + 4 * i) = f;
            continue;
        }
        if (GT4_U8(SETTINGS + 0x139)) {
            /* lateral transfer from this tyre's side force and the roll centre */
            int axle = i < n ? GT4_U8(layout + LAYOUT_AXLE + i) : 0;
            float h = ((GT4_F32(car + CAR_SUSP_REACH + 4 * i) - GT4_F32(axle_block(params, axle) + AXLE_0C))
                       + GT4_F32(params + PARAMS_2C4 + axle * 4));
            float x = (GT4_F32(params + PARAMS_CG_HEIGHT) + h) / GT4_F32(params + PARAMS_HALF_TRACK + axle * 4)
                      * GT4_F32(w + WHEEL_SIDE_FORCE);
            if (side == 0) f = f - x;
            else if (side == 1) f = f + x;
        } else {
            if (side == 0) f = f + lat;
            else if (side == 1) f = f - lat;
        }
        GT4_F32(car + CAR_LOAD_TARGET + 4 * i) = f;
    }
}

/* func_003697A8: the point under each wheel mount (at rest travel) where
   the ground is looked for, in the course frame. */
void gt4_car_ground_points(uint32_t car)
{
    int n = GT4_U8(gt4_car_layout(car) + LAYOUT_WHEELS);
    for (int i = 0; i < n; i++) {
        float rest = wheel_rest(car, i), c[3];
        uint32_t g = car + CAR_GROUND + (uint32_t)i * GROUND_SIZE;
        for (int k = 0; k < 3; k++) c[k] = wheel_mount(car, i, k, rest);
        GT4_F32(g + GROUND_X) = c[0];
        GT4_F32(g + GROUND_Y) = c[2];
        GT4_F32(g + GROUND_Z) = -c[1];
    }
}

float gt4_wheel_contact_y(uint32_t params, int i, float angle, float travel) { return contact_y(params, i, angle, travel); }
float gt4_wheel_camber(uint32_t params, int i, float steer, float travel) { return camber(params, i, steer, travel); }
