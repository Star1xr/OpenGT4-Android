/* Smaller per-car pieces of the step: the snapshot of the previous state
   (func_00343ED0), brake temperature (func_003F38D8) and the wheels' random
   vibration (func_0035A678). */
#include "car.h"

#include <math.h>

#define K(bits) gt4_f32(bits)

static float ps2_sqrt(float x) { return x == 0 ? x : sqrtf(fabsf(x)); }

/* func_00343ED0: what the view and replay read of the last step. */
void gt4_car_keep_state(uint32_t car)
{
    int n = GT4_U8(gt4_car_layout(car) + LAYOUT_WHEELS);
    for (int k = 0; k < 3; k++) GT4_F32(car + 0x8bc + k * 4) = GT4_F32(car + CAR_ANGLES + k * 4);
    for (int k = 0; k < 2; k++) GT4_F32(car + 0x8c8 + k * 4) = GT4_F32(car + CAR_GROUND_PITCH + k * 4);
    for (int i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i), g = car + CAR_GROUND + (uint32_t)i * GROUND_SIZE;
        uint32_t a = car + 0x980 + (uint32_t)i * 4, b = car + 0x958 + (uint32_t)i * 0xc;
        GT4_F32(a - 0xb0) = GT4_F32(w + WHEEL_TRAVEL);
        GT4_F32(a - 0xa0) = GT4_F32(w + 0);
        GT4_F32(a - 0x90) = GT4_F32(w + 4);
        GT4_F32(a - 0x80) = GT4_F32(w + 8);
        GT4_F32(a - 0x70) = GT4_F32(w + WHEEL_ROTATION);
        GT4_F32(a - 0x60) = GT4_F32(w + 0xa4);
        GT4_F32(a - 0x50) = GT4_F32(g + GROUND_U);
        GT4_F32(a - 0x40) = GT4_F32(g + GROUND_V);
        GT4_F32(b - 8) = GT4_F32(g + GROUND_X);
        GT4_F32(b - 4) = GT4_F32(w + 0x8c);
        GT4_F32(b) = GT4_F32(g + GROUND_Z);
        GT4_F32(a) = GT4_F32(w + 0xc0);
    }
    GT4_F32(car + 0x994) = GT4_F32(car + CAR_VIEW_YAW_LAG);
    GT4_F32(car + 0x998) = GT4_F32(car + CAR_VIEW_PITCH_LAG);
    GT4_F32(car + 0x99c) = GT4_F32(car + 0x5a8);
}

#define BRAKE_HEAT 0x00845a00u   /* gain, cooling, cooling speed offset, level offset */

/* func_003F38D8: brake temperature (WHEEL + 0xa4, 0..1): heating from the
   braking torque at the wheel's speed, cooling with speed. */
void gt4_car_brake_heat(uint32_t car)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), layout = gt4_car_layout(car);
    float dt = GT4_F32(car + CAR_DT);
    int n = GT4_U8(layout + LAYOUT_WHEELS);
    for (int i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i);
        int axle = GT4_U8(layout + 0x14 + i);
        float spin = GT4_F32(w + WHEEL_SPIN), t = GT4_F32(w + 0xa4), heat, cool;
        float k = GT4_F32(params + PARAMS_BRAKE + (uint32_t)axle * 4) * GT4_F32(params + 0x2c4 + (uint32_t)axle * 4);
        if (spin < 0) spin = -spin;
        heat = ((k * GT4_F32(w + 0x64)) * spin) * GT4_F32(BRAKE_HEAT);
        if (GT4_F32(car + CAR_BRAKE_PEDAL) == 0) heat = 0;
        cool = ((spin + GT4_F32(BRAKE_HEAT + 8)) * GT4_F32(BRAKE_HEAT + 4)) * (t + GT4_F32(BRAKE_HEAT + 0xc));
        t = t + (heat - cool) * dt;
        if (1.0f < t) t = 1.0f;
        else t = gt4_select(t < 0, 0.0f, t);
        GT4_F32(w + 0xa4) = t;
    }
}

/* func_00579330 with func_00578598: the game's random generator (state at
   D_00874D50) and its conversion to a float in [0, 1) done on the bits (the
   R5900's leading-sign count, PLZCW). */
#define RANDOM_STATE 0x00874d50u
static float random01(void)
{
    uint32_t x = GT4_U32(RANDOM_STATE) * 17u + 0x11u, r, lz, e, m;
    int32_t s;
    GT4_U32(RANDOM_STATE) = x;
    r = x ^ ((x << 16) | (x >> 16));
    if (!r) return 0.0f;
    s = (int32_t)r >> 31;
    {
        uint32_t y = r ^ (uint32_t)s;   /* PLZCW: leading bits equal to the sign, less one */
        lz = 0;
        while (lz < 32 && !(y & (0x80000000u >> lz))) lz++;
        lz = lz - 1;
    }
    lz = (lz & 0x1f) | (uint32_t)s;
    e = 125u - lz;
    m = (r << ((0u - e) & 31)) >> 9;
    return gt4_f32((e << 23) | m);
}

/* func_0035A678: each wheel's random vertical shake (WHEEL + 0xc0, rate at
   + 0xc4), stronger with speed up to 20 m/s. */
void gt4_car_wheel_noise(uint32_t car)
{
    int n = GT4_U8(gt4_car_layout(car) + LAYOUT_WHEELS);
    for (int i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i);
        float a = GT4_F32(w + WHEEL_VEL_X) * K(0x3dccccccu), pos = GT4_F32(w + 0xc0), rate = GT4_F32(w + 0xc4), r, f;
        if (a < 0) a = -a;
        if (2.0f < a) a = 2.0f;
        a = a + -1.0f;
        r = random01() * 1.0f;
        f = (a - -1.0f) * r;
        f = f + f;
        f = f + -1.0f;
        f = ((((f - pos) * 500.0f) - rate * 10.0f) * K(0x3c888888u));
        rate = rate + f;
        pos = pos + rate * K(0x3c888888u);
        if (1.0f < pos) { pos = 1.0f; rate = 0; }
        else if (pos < -1.0f) { pos = -1.0f; rate = 0; }
        GT4_F32(w + 0xc0) = pos;
        GT4_F32(w + 0xc4) = rate;
    }
}

/* func_003F2798: 0 below params+0x1078 km.. 1 from params+0x107c, linear. */
static float aero_by_speed(uint32_t params, float v)
{
    float lo = GT4_F32(params + 0x1078), hi = GT4_F32(params + 0x107c);
    if (v <= lo) return 0;
    if (hi <= v) return 1.0f;
    return (v - lo) / (hi - lo);
}

/* func_003F28B8: moving aero parts (params+0x1077: 1 wing up above a
   speed, 2 wing by speed, 4 air brake plus a side flap by lateral g, 5 air
   brake, 6 wing and air brake); car+0x698.. follow their targets at
   params+0x1080 per second. State in car+0x696 (and +0x697 for the flap). */
void gt4_car_active_aero(uint32_t car)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS);
    int kind = GT4_U8(params + 0x1077), parts = 1;
    float target[2] = { 0, 0 }, v = GT4_F32(car + CAR_SPEED), brake = GT4_F32(car + CAR_BRAKE_PEDAL);
    float lo = GT4_F32(params + 0x1078), hi = GT4_F32(params + 0x107c);
    uint32_t st = car + 0x696;
    if (!kind) return;
    if (kind == 1) {
        if (!GT4_U8(st)) { if (hi <= v) GT4_U8(st) = 1; }
        else if (v < lo) GT4_U8(st) = 0;
        if (GT4_U8(st) == 1) target[0] = 1.0f;
    } else if (kind == 2) target[0] = aero_by_speed(params, v);
    else if (kind == 4 || kind == 5) {
        if (!GT4_U8(st)) { if (hi <= v && 0.5f <= brake) GT4_U8(st) = 1; }
        else if (v < lo || brake < K(0x3dccccccu)) GT4_U8(st) = 0;
        if (GT4_U8(st) == 1) target[0] = 1.0f;
        if (kind == 4) {
            float g = GT4_F32(car + CAR_LAT_ACCEL) / K(0x411ccccc);
            uint8_t f = GT4_U8(car + 0x697);
            parts = 2;
            if (!f) {
                if (0.5f <= g) f = 1;
                else if (g <= -0.5f) f = 2;
            } else if (f == 1) { if (g < K(0x3e4ccccc)) f = 0; }
            else if (K(0xbe4ccccc) < g) f = 0;
            GT4_U8(car + 0x697) = f;
            if (f == 1) target[1] = 1.0f;
            else if (f == 2) target[1] = -1.0f;
        }
    } else if (kind == 6) {
        uint8_t s = GT4_U8(st);
        if (s == 1) {
            if (K(0x4031c71bu) <= v && 0.5f <= brake) s = 2;
            else if (v < lo) s = 0;
        } else if (s == 0) {
            if (K(0x4031c71bu) <= v && 0.5f <= brake) s = 2;
            else if (hi <= v) s = 1;
        } else if (s == 2) {
            if (brake < K(0x3dccccccu)) s = lo <= v ? 1 : 0;
            else if (v < K(0x3f0e38e3u)) s = 0;
        }
        GT4_U8(st) = s;
        if (s == 2) target[0] = 1.0f;
        else if (s == 1) target[0] = K(0x3e1d89d8u);
    }
    for (int k = 0; k < parts; k++) {
        uint32_t p = car + 0x698 + (uint32_t)k * 4;
        float rate = GT4_F32(params + 0x1080), cur = GT4_F32(p), t = target[k], dt = GT4_F32(car + CAR_DT), x;
        if (k == 1) rate = rate + rate;
        if (cur < t) {
            x = cur + rate * dt;
            GT4_F32(p) = x;
            if (t < x) GT4_F32(p) = t;
        } else if (t < cur) {
            x = cur - rate * dt;
            GT4_F32(p) = x;
            if (x < t) GT4_F32(p) = t;
        }
    }
}

/* Cars of kind 1 (params+0x32) keep a message state at car+0x788:
   +0 a counter while standing with the throttle down, +1 the message (low
   nibble) with the previous one above it, +2 how long it stays (0xFF:
   until replaced), +3 a pause between messages, +4 a 48-step cycle, +5 a
   ramp between -30 and 30, +6 the message set this frame. The tables
   D_00620880 (duration), D_00620890 (ramp direction; 2: cycling) and
   D_006208A0 (whether the message pauses) are per message. */
#define MSG 0x788u

static int msg_kind(uint32_t car) { return GT4_U8(GT4_U32(car + CAR_PARAMS) + 0x32) == 1; }   /* func_0035C7C8 */
static uint32_t msg_now(uint32_t car) { return GT4_U8(car + MSG + 1) & 0xf; }                /* func_0035C828 */

/* func_0035C848: show message `code` for `time` frames (-1: its own) */
static void msg_say(uint32_t car, uint32_t code, uint32_t time)
{
    uint32_t m = car + MSG, now, v;
    if (!msg_kind(car)) return;
    if ((int8_t)GT4_U8(m + 3) > 0 && time != 0xff) return;
    v = GT4_U8(m + 2);
    if (v == 0xff) return;
    if (!time && v) return;
    now = msg_now(car);
    if (now != code) {
        GT4_U8(m + 6) = (uint8_t)code;
        GT4_U8(m + 1) = (uint8_t)(now << 4 | code);
    }
    GT4_U8(m + 2) = (uint8_t)(time != 0xffffffffu ? time : GT4_U8(0x00620880u + code));
    if (GT4_U8(0x006208a0u + now) == GT4_U8(0x006208a0u + code)) return;
    GT4_U8(m + 3) = GT4_U8(0x006208a0u + now) ? 0x24 : 0x18;
}

/* func_0035C9B0 */
static int32_t msg_phase(int32_t x)
{
    if (!(x < 0x25)) return x - 0x30;
    if (x < 0xd) return x;
    return 0x18 - x;
}

/* func_00366018: laps and distance (to *d) car j is ahead of car i
   (conductor +0xCCC8 laps, +0xCCEC distances, 6 per car), the distance
   kept within half a lap */
int32_t gt4_cars_gap(uint32_t cond, uint32_t i, uint32_t j, float *d)
{
    int32_t laps = (int8_t)GT4_U8(cond + 0xccc8u + i * 6 + j);
    float v = GT4_F32(cond + 0xccecu + (i * 6 + j) * 4), len = GT4_F32(GT4_U32(GT4_U32(GT4_U32(cond + 0xcbd8u) + 4) + 0xdc) + 0x14);
    float half = len * 0.5f;
    if (half < v) {
        v = v - len;
        laps++;
    } else if (v < -half) {
        v = v + len;
        laps--;
    }
    *d = v;
    return laps;
}

/* func_004149D8: of the other cars, the one car i stands to (the table at
   +0x4C of the object at conductor+0xCD7C: 6 by 6), the most pressing:
   5 one of them in state 3, 4 or 3 in state 2 (i's or the other's),
   1 ahead, 2 behind by laps; its index to *who (-1: none). */
static uint32_t msg_rival(uint32_t obj, uint32_t i, int32_t *who)
{
    uint32_t cond = GT4_U32(obj), n = GT4_U32(cond + 0xf8a0u), best = 0;
    *who = -1;
    for (uint32_t j = 0; (int32_t)j < (int32_t)n; j++) {
        uint32_t a, b, v;
        float d;
        int32_t r;
        if (j == i) continue;
        a = GT4_U8(obj + 0x4c + i * 6 + j);
        b = GT4_U8(obj + 0x4c + i + j * 6);
        if (a < 2 && b < 2) continue;
        r = gt4_cars_gap(cond, i, j, &d);
        if (r) v = r < 0 ? 2 : 1;
        else if (a == 3 || b == 3) v = 5;
        else if (a == 2) v = 4;
        else if (b == 2) v = 3;
        else v = 0;
        if ((int32_t)best < (int32_t)v) {
            *who = (int32_t)j;
            best = v & 0xff;
        }
    }
    return best;
}

/* func_0035CA60: this frame's message and the counters */
void gt4_car_messages(uint32_t car)
{
    uint32_t m = car + MSG, b = car + 0x104, v1;
    int32_t who, dir;
    if (!msg_kind(car)) return;
    GT4_U8(m + 6) = 0;
    {
        uint32_t rival = msg_rival(GT4_U32(car + 4) + 0xcd7cu, GT4_U8(b + 0x440), &who);
        int all_off = 1;
        if (GT4_U8(b + 0x4b2)) msg_say(car, (int8_t)GT4_U8(car + 0x5b3) == 1 ? 2 : 7, 0);
        else if (GT4_U8(car + 0x6fe) == 1) msg_say(car, 6, 0);
        else if (GT4_U8(b + 0x4b1) & 0x10) msg_say(car, 3, 0);
        else if (rival == 5) msg_say(car, 1, 0x78);
        else if (GT4_F32(b + 0x5a0) == 1.0f) msg_say(car, 2, 0x78);
        else {
            uint32_t lay = gt4_car_layout(car);                  /* func_0035C9E0: every wheel off the road */
            for (uint32_t k = 0; k < GT4_U8(lay + LAYOUT_WHEELS); k++)
                if (!gt4_ground_off_road(car + CAR_GROUND + k * GROUND_SIZE)) { all_off = 0; break; }
            if (all_off) msg_say(car, 10, 0);
            else if (!GT4_U8(m + 2)) msg_say(car, 0, 0xffffffffu);
        }
    }
    v1 = GT4_U8(m + 2);
    if (v1 && v1 != 0xff) {
        v1 = v1 - 1;
        GT4_U8(m + 2) = (uint8_t)v1;
        if (!(v1 & 0xff)) msg_say(car, 0, 0xffffffffu);
    }
    if ((int8_t)GT4_U8(m + 3) > 0) {
        GT4_U8(m + 3) = (uint8_t)(GT4_U8(m + 3) - 1);
        if (!GT4_U8(0x006208a0u + msg_now(car)) && (int8_t)GT4_U8(m + 3) < 0x19) GT4_U8(m + 3) = 0;
    }
    dir = (int8_t)GT4_U8(0x00620890u + msg_now(car));       /* func_0035C938 */
    if (dir != 1 && dir != -1) dir = 0;
    if (!msg_phase(GT4_U8(m + 4))) {
        int8_t r = (int8_t)GT4_U8(m + 5);
        if (dir == 1) {
            r = (int8_t)(r + 1);
            if (r >= 31) r = 30;
        } else if (dir == -1) {
            r = (int8_t)(r - 1);
            if (r < -30) r = -30;
        } else if (r > 0) r = (int8_t)(r - 1);
        else if (r < 0) r = (int8_t)(r + 1);
        GT4_U8(m + 5) = (uint8_t)r;
    }
    if (GT4_U8(m + 5)) return;
    if ((int8_t)GT4_U8(0x00620890u + msg_now(car)) != 2 && !msg_phase(GT4_U8(m + 4))) return;   /* func_0035C980 */
    {
        uint32_t c = GT4_U8(m + 4) + 1u;
        GT4_U8(m + 4) = (uint8_t)((c & 0xff) < 0x30 ? c : 0);
    }
}

/* func_0035D1D0 / func_0035D158: +0 counts up (to 34) while the car
   stands (under 1 km/h) with the throttle down, else down */
void gt4_car_idle_count(uint32_t car)
{
    uint32_t b = car + 0x104, m = car + MSG;
    int standing = 0;
    if (!msg_kind(car)) return;
    if (!(GT4_F32(b + 0x490) < 1.0f)) {
        float x = GT4_F32(b + 0x53c), y = GT4_F32(b + 0x540);
        standing = !(K(0x3e8e38e3u) < ps2_sqrt(x * x + y * y));
    }
    if (standing) {
        uint32_t v = GT4_U8(m) + 1u;
        GT4_U8(m) = (uint8_t)(v < 0x23 ? v : 0x22);
    } else if (GT4_U8(m)) GT4_U8(m) = (uint8_t)(GT4_U8(m) - 1);
}

/* func_004146E8: the relation of each pair of cars (the object at
   conductor+0xCD7C: +4 a counter per pair, up while car i gains on j
   (conductor+0xCC14 > 0) up to 300, down to 0 else; +0x4C a state per
   pair, 6 by 6), moved on by what the race saw between them
   (conductor+0xCCA4, low nibble; func_003F40A8 / func_003F4450). */
void gt4_cars_relations(uint32_t obj)
{
    uint32_t cond = GT4_U32(obj), n = GT4_U32(cond + 0xf8a0u);
    for (uint32_t i = 0; (int32_t)i < (int32_t)n; i++)
        for (uint32_t j = 0; (int32_t)j < (int32_t)n; j++) {
            uint32_t k = i * 6 + j, cnt = obj + 4 + k * 2, tij = obj + 0x4c + k, tji = obj + 0x4c + j * 6 + i, seen;
            if (j == i) continue;
            if (0 < GT4_F32(cond + 0xcc14u + k * 4)) {
                if ((int16_t)GT4_U16(cnt) < 300) GT4_U16(cnt) = (uint16_t)(GT4_U16(cnt) + 1);
            } else if ((int16_t)GT4_U16(cnt) > 0) GT4_U16(cnt) = (uint16_t)(GT4_U16(cnt) - 1);
            seen = GT4_U8(cond + 0xcca4u + k) & 0xf;
            switch (GT4_U8(tij)) {
            case 0:
                if (GT4_U8(tji)) break;
                if (seen == 4) GT4_U8(tij) = 3;
                else if (seen == 5) GT4_U8(tij) = 2;
                else if (!((int16_t)GT4_U16(cnt) < 0x3d)) GT4_U8(tij) = 1;
                break;
            case 1:
                if (!GT4_U8(tji) && seen == 4) GT4_U8(tij) = 3;
                else if (!GT4_U8(tji) && seen == 5) GT4_U8(tij) = 2;
                else if (!GT4_U16(cnt)) GT4_U8(tij) = 0;
                break;
            case 2:
                if ((uint8_t)(seen - 3) < 2) GT4_U8(tij) = 3;
                else if (seen == 0 || seen == 6) GT4_U8(tij) = 1;
                else if ((uint8_t)(seen - 1) < 2) {
                    GT4_U8(tji) = 2;
                    GT4_U8(tij) = 0;
                }
                break;
            case 3:
                if ((uint8_t)(seen - 1) < 2) {
                    GT4_U8(tji) = 2;
                    GT4_U8(tij) = 0;
                } else if ((uint8_t)(seen - 5) < 2) GT4_U8(tij) = 2;
                break;
            }
        }
}
