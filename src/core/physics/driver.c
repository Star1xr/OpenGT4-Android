/* The driver's controls on the car: steering (limits by speed, the
   counter-steer allowance, the steering's own inertia), throttle, brake and
   handbrake from analog or digital inputs, and the clutch request
   (func_00349F48 and what it calls).

   Input is the 32-byte driver command the controller read returns: [0]
   flags (bit 0 steering analog, 1 throttle, 2 brake, 3 the second
   throttle [0xc], 4 handbrake), [1] device (2 pad, 4 GT Force, 6 another
   wheel), [2] a button copied to car+0x567, [4] steering s16 -4096..4096
   (+ = left), [8] throttle, [0xa] brake, [0xc], [0xe] handbrake 0..4096
   (digital: 0 or 1). */
#include "car.h"

#include <math.h>
#include <string.h>

#define K(bits) gt4_f32(bits)
#define F(o) GT4_F32(car + (o))
#define U8(o) GT4_U8(car + (o))

enum {
    CAR_STEER_RAW      = 0x558,  /* s16 [2]: steering input, this and the last step */
    CAR_STEER_RAW_PREV = 0x55c,  /* s16 [2]: the same one step earlier */
    CAR_STEER_SHOWN    = 0x560,  /* steering for the display, -1..1 */
    CAR_REVERSE_ON     = 0x566,  /* u8 */
    CAR_BUTTON         = 0x567,  /* u8: command [2] */
    CAR_THROTTLE_MODE  = 0x568,  /* u8: digital throttle held: 1 forward, 2 the other one */
    CAR_STEER_MODE     = 0x569,  /* u8: 0 none, 1 digital, 2 pad, 3 wheel */
    CAR_STEER          = 0x56c,  /* front wheel angle asked for, rad */
    CAR_STEER_RATE     = 0x588,
    CAR_STEER_RIGHT    = 0x59c,  /* steering limits this step */
    CAR_STEER_LEFT     = 0x5a0,
    CAR_STEER_ASSIST   = 0x5a4,
    CAR_STEER_NO_AID   = 0x5b5,  /* u8 bit 0 */
    CAR_ASSISTS        = 0x5e9,  /* u8: 8, 0x10 yaw assist on the steering */
    CAR_WHEEL_TYPE     = 0x547,  /* s8: GT Force steering response 0..4 */
    CAR_730            = 0x730,

    PARAMS_STEER_CURVE_A = 0x58, /* max steering against speed */
    PARAMS_STEER_CURVE_B = 0x94, /* the same for the GT Force */
    PARAMS_STEER_CURVE_C = 0xd0, /* the same, other setting */
    PARAMS_WHEEL_SCALE   = 0x10d3,
    WHEEL_BRAKE_IN_      = 0x64,
};

#define DRIVE_SETTINGS  0x00622110u  /* +0x11 analog pedal ramps, +0x22, +0x26 counter-steer modes */
#define SURFACE_STEER   0x00620288u  /* [surface]: 0/1 per surface */
#define STEER_TABLES    0x00843598u
#define ASSIST_LINE     0x0084357cu
#define ASSIST_HOLD     0x0084358cu

static float ps2_sqrt(float x) { return x == 0 ? x : sqrtf(fabsf(x)); }

/* func_00348588: moves *p toward target by at most rate. */
static void ramp(uint32_t p, float target, float rate)
{
    float v = GT4_F32(p);
    if (v < target) { v = v + rate; if (target < v) v = target; }
    else if (target < v) { v = v - rate; if (v < target) v = target; }
    GT4_F32(p) = v;
}

/* func_00348800 */
static float steer_shown(float angle)
{
    float v = angle * K(0x40747648u);
    if (1.0f < v) return 1.0f;
    return v < -1.0f ? -1.0f : v;
}

/* Table value i of a steering table (func_003485D0's data). */
static float tab(uint32_t off, int i) { return GT4_F32(STEER_TABLES + off + (uint32_t)i * 4); }

/* func_003485D0: the steering follows the asked angle as a damped spring
   (rate at CAR_STEER_RATE), with its own gains when turning in, back to
   centre or against the car's motion (front wheels' direction `wheels`). */
static void steer_follow(uint32_t car, int set, int kind, float target, float wheels)
{
    float cur = F(CAR_STEER), rate = F(CAR_STEER_RATE), dt = F(CAR_DT);
    float k_turn = tab(0x10, set), k_center = tab(0x08, set);
    int v = set * 2 + kind;
    float cap = tab(0x20, v), cap_fast = tab(0x68, v), a;
    a = (target - cur) * tab(0x00, set) - rate * tab(0x18, set);
    if (k_turn != 0 && ((cur < target && cur < wheels) || (target < cur && wheels < cur))) {
        cap_fast = tab(0x88, v);
        cap = tab(0x40, v);
        a = a + (wheels - cur) * k_turn;
        a = a * dt;
    } else if ((0 < cur && target < cur) || (cur < 0 && cur < target)) {
        cap_fast = tab(0x78, v);
        cap = tab(0x30, v);
        a = (a - cur * k_center) * dt;
    } else {
        a = a * dt;
    }
    rate = rate + a;
    if (GT4_U32(STEER_TABLES + 0x50 + (uint32_t)set * 4)) {
        float speed = F(CAR_SPEED), lo = tab(0x58, set);
        if (lo < speed) {
            float hi = tab(0x60, set);
            if (hi <= speed) cap = cap_fast;
            else cap = cap + ((cap_fast - cap) * (speed - lo)) / (hi - lo);
        }
    }
    if (cap < rate) rate = cap;
    else if (rate < -cap) rate = -cap;
    a = cur + rate * dt;
    if ((cur <= target && target <= a) || (target <= cur && a <= target)) {
        F(CAR_STEER) = target;
        GT4_U32(car + CAR_STEER_RATE) = 0;
        return;
    }
    F(CAR_STEER_RATE) = rate;
    F(CAR_STEER) = a;
}

/* The yaw assist on the steering (car+0x5e9 bits 8/0x10): the angle the
   car's motion asks for, faded in above a speed. */
static float yaw_assist(uint32_t car, uint32_t params, float wheels)
{
    float speed = F(CAR_SPEED), f = 1.0f, d;
    float y = F(CAR_YAW_RATE) + F(CAR_YAW_FILTER) * GT4_F32(0x00843570u);
    y = -(F(CAR_LAT_SPEED) - y * GT4_F32(params + PARAMS_AXLE_DIST));
    d = gt4_atan2f(y, speed) - wheels;
    if (speed < GT4_F32(0x00843578u)) f = speed / (GT4_F32(0x00843578u) - GT4_F32(0x00843574u));
    return d * f;
}

/* The counter-steer allowance: the angle the car's slip asks for. */
static float slip_angle(uint32_t car, uint32_t params, uint32_t k)
{
    float y = (F(CAR_YAW_RATE) + F(CAR_YAW_FILTER) * GT4_F32(k)) * GT4_F32(params + PARAMS_AXLE_DIST);
    return gt4_atan2f(-(F(CAR_LAT_SPEED) - y), F(CAR_SPEED));
}

/* func_00348850: front wheel angle from the steering input. 1 when it ran,
   0 when a case not rewritten yet was handed to the original. */
static int steering(uint32_t car, uint32_t cmd)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), layout = gt4_car_layout(car);
    int n = GT4_U8(layout + LAYOUT_WHEELS), count = 0, surf = 2, table_set, kind, limited, analog_steer, line = 0;
    float wheels = 0, speed, max, left, right, dt = F(CAR_DT);
    uint8_t setting;

    for (int i = 0; i < n; i++)
        if ((int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i) > 0) {
            wheels = wheels + GT4_F32(gt4_car_wheel(car, i) + WHEEL_VEL_ANGLE);
            count++;
        }
    wheels = wheels / (float)count;
    speed = F(CAR_SPEED);
    for (int i = 0; i < n; i++)
        if ((int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i) > 0) {
            int s = GT4_U8(SURFACE_STEER + GT4_U8(gt4_car_wheel(car, i) + WHEEL_SURFACE));
            if (s < surf) surf = s;
        }
    if (surf >= 2) surf = 0;
    setting = GT4_U8(GT4_U32(car + 4) + 0xcbdd);                  /* func_0034C968 */
    if (setting == 0xff) setting = 0;
    table_set = setting == 2 || setting == 3;
    speed = gt4_select(speed < 0, 0.0f, speed);

    kind = GT4_U8(cmd + 1);
    if (kind == 6) max = GT4_F32(params + PARAMS_STEER_MAX);
    else if (kind == 4) max = gt4_curve_eval(gt4_ptr(params + (table_set == 1 ? PARAMS_STEER_CURVE_C : PARAMS_STEER_CURVE_B)), speed);
    else {
        table_set = surf;
        max = gt4_curve_eval(gt4_ptr(params + (table_set == 1 ? PARAMS_STEER_CURVE_C : PARAMS_STEER_CURVE_A)), speed);
    }

    limited = 1;
    if (kind == 4) {
        int w = (int8_t)U8(CAR_WHEEL_TYPE);
        float least = 0;
        int has_least = 0;
        if (w == 1 || w == 3) {
            if (w == 3) limited = 0;
            least = GT4_F32(table_set == 1 ? 0x00620250u : 0x0062024cu);
            has_least = 1;
        } else if (w == 2) {
            least = GT4_F32(table_set == 1 ? 0x00620258u : 0x00620254u);
            has_least = 1;
        } else if (w == 4) limited = 0;
        if (has_least && max < least) max = least;
    } else limited = kind != 6;
    left = max;
    right = -max;

    if (limited) {
        uint32_t d = DRIVE_SETTINGS;
        float yf = F(CAR_YAW_FILTER), l2 = left, r2 = right, c = wheels, zero = 0;
        if (GT4_U8(d + 0x26) && K(0x4031c71bu) < F(CAR_SPEED) && 0 < yf * F(CAR_YAW_RATE)) {
            float a = slip_angle(car, params, 0x00843568u) - wheels;
            if (0 < F(CAR_YAW_RATE)) {
                l2 = left - a;
                if (GT4_U8(d + 0x26) < 2) l2 = gt4_select(l2 < zero, zero, l2);
            } else if (F(CAR_YAW_RATE) < 0) {
                r2 = right - a;
                if (GT4_U8(d + 0x26) < 2) r2 = gt4_select(zero < r2, zero, r2);
            }
        }
        if (GT4_U8(d + 0x22)) {
            float a = slip_angle(car, params, 0x0084356cu) - wheels;
            c = wheels - a;
        }
        if (right <= wheels && wheels <= max) {
            if (GT4_U8(d + 0x26)) { left = l2; right = r2; }
        } else if (0 < wheels) {
            uint8_t m = GT4_U8(d + 0x22);
            left = wheels;
            if (m && 0 < yf && F(CAR_YAW_RATE) < 0) {
                if (m == 2) left = gt4_select(c < 0, 0.0f, c);
                else if (max < c) left = c;
            }
            if (GT4_U8(d + 0x26)) right = r2;
        } else if (wheels < 0) {
            uint8_t m;
            if (GT4_U8(d + 0x26)) left = l2;
            m = GT4_U8(d + 0x22);
            right = wheels;
            if (m && yf < 0 && 0 < F(CAR_YAW_RATE)) {
                if (m == 2) right = gt4_select(0 < c, 0.0f, c);
                else if (c < -max) right = c;
            }
        }
    }
    {
        float cap = GT4_F32(params + PARAMS_STEER_MAX);
        if (cap < left) left = cap;
        if (right < -cap) right = -cap;
    }

    analog_steer = !(GT4_U8(cmd) & 1) || kind == 2 || kind == 3;
    if (analog_steer && !(U8(CAR_STEER_NO_AID) & 1) && K(0x41055554u) <= F(CAR_SPEED) && !U8(CAR_730)) {
        if (!(GT4_U32(ASSIST_HOLD) == 1 && (int16_t)GT4_U16(cmd + 4) != 0)) {
            uint32_t a = GT4_U32(ASSIST_LINE);
            if (a == 2 || (a == 1 && !GT4_U8(car + 0x32))) line = 1;
        }
    }

    if (kind == 4) {
        float in = (float)(int16_t)GT4_U16(cmd + 4) * K(0x39800000u), out = 0;
        int w;
        U8(CAR_STEER_MODE) = 0;
        F(CAR_STEER_SHOWN) = in * K(0x3f8e38e3u);
        w = (int8_t)U8(CAR_WHEEL_TYPE);
        if (w >= 0 && w < 4) {
            ramp(car + CAR_STEER_LEFT, left, GT4_F32(0x00620248u) * dt);
            ramp(car + CAR_STEER_RIGHT, right, GT4_F32(0x00620248u) * dt);
            out = 0 <= in ? F(CAR_STEER_LEFT) * in : -F(CAR_STEER_RIGHT) * in;
        } else if (w == 4) out = GT4_F32(params + PARAMS_STEER_MAX) * in;
        F(CAR_STEER) = out;
    } else if (kind == 6) {
        float in = (float)(int16_t)GT4_U16(cmd + 4), g;
        int curve = 0, s;
        uint8_t scale;
        U8(CAR_STEER_MODE) = 3;
        F(CAR_STEER_SHOWN) = in * 5.0f * K(0x39800000u);
        s = GT4_U8(GT4_U32(car + 4) + 0xcbdd);
        if (s == 0xff) s = 0;
        scale = GT4_U8(params + PARAMS_WHEEL_SCALE);
        if ((unsigned)(s - 2) < 2) g = GT4_F32(0x00620264u);
        else { curve = 1; g = GT4_F32(scale ? 0x00620260u : 0x0062025cu); }
        in = in * g;
        if (1.0f < in) in = 1.0f;
        else if (in < -1.0f) in = -1.0f;
        if (scale && curve) in = gt4_input_curve(0x00843538u, NULL, in);
        F(CAR_STEER_LEFT) = left;
        F(CAR_STEER_RIGHT) = right;
        F(CAR_STEER) = GT4_F32(params + PARAMS_STEER_MAX) * in;
    } else if (analog_steer) {
        int16_t raw = (int16_t)GT4_U16(cmd + 4);
        float target;
        F(CAR_STEER_LEFT) = left;
        F(CAR_STEER_RIGHT) = right;
        if (!(GT4_U8(cmd) & 1)) target = raw > 0 ? left : raw < 0 ? right : 0.0f;
        else {
            float in = (float)raw * K(0x39800000u);
            target = 0 <= in ? left * in : -right * in;
        }
        if (raw) U8(CAR_STEER_MODE) = kind == 2 ? 2 : 1;
        if ((U8(CAR_ASSISTS) & 0x10) && GT4_F32(0x00843574u) < F(CAR_SPEED))
            target = target - yaw_assist(car, params, wheels);
        if (line) {
            /* the steering assist along the course line: the car's angle to
               the line's heading, while small (and the driver does not
               steer against it), corrected with its rate of change */
            uint32_t obj = car + 0x14;
            float a = gt4_line_heading(GT4_U32(car + 4) + 0xda50u, GT4_U8(obj + 0x1c), GT4_U8(obj + 0x1d), F(0x5a8));
            float d = gt4_angle_diff(F(0x674), a), lim = GT4_F32(0x00843590u);
            int skip = d < -lim || lim < d;
            if (!skip && GT4_U32(ASSIST_HOLD) == 2)
                skip = (0 < d && (int16_t)GT4_U16(cmd + 4) > 0) || (d < 0 && (int16_t)GT4_U16(cmd + 4) < 0);
            if (!skip) {
                float prev = F(CAR_STEER_ASSIST), k, cap = GT4_F32(0x00843588u);
                F(CAR_STEER_ASSIST) = d;
                k = -d * GT4_F32(0x00843580u) - ((d - prev) * F(0x550)) * GT4_F32(0x00843584u);
                if (cap < k) k = cap;
                else if (k < -cap) k = -cap;
                target = target + k;
            }
        }
        steer_follow(car, table_set, U8(CAR_STEER_MODE) == 1 ? 0 : 1, target, wheels);
    }
    if ((kind == 4 || kind == 6) && (U8(CAR_ASSISTS) & 8) && GT4_F32(0x00843574u) < F(CAR_SPEED)) {
        float cap = GT4_F32(params + PARAMS_STEER_MAX), v = F(CAR_STEER) - yaw_assist(car, params, wheels);
        F(CAR_STEER) = v;
        if (cap < v) F(CAR_STEER) = cap;
        else if (v < -cap) F(CAR_STEER) = -cap;
    }
    if (!line) GT4_U32(car + CAR_STEER_ASSIST) = 0;
    if (U8(CAR_STEER_MODE) < 3 && U8(CAR_STEER_MODE)) F(CAR_STEER_SHOWN) = steer_shown(F(CAR_STEER));
    return 1;
}

/* func_0035A9D0: an input response curve (the steering wheel's, the
   pedals'): by type +0, 1 a parabola k(x-x0)^2 + c, 2 a root
   k sqrt(x-x0) + c, 3 k x^8 + c x, else linear; odd in x. The slope goes
   to *slope when asked (for the force feedback). */
#define ONE_F K(0x3f800000u)

float gt4_input_curve(uint32_t t, float *slope, float x)
{
    float a = x < 0 ? -x : x, y = a, d = ONE_F;
    switch ((int32_t)GT4_U32(t)) {
    case 1: {
        float k = GT4_F32(t + 0xc);
        a = a - GT4_F32(t + 4);
        y = (k * a) * a + GT4_F32(t + 8);
        d = (k + k) * a;
        break;
    }
    case 2: {
        float k = GT4_F32(t + 0xc), r;
        a = a - GT4_F32(t + 4);
        r = ps2_sqrt(a);
        y = k * r + GT4_F32(t + 8);
        if (r < K(0x3c23d70au)) r = K(0x3c23d70au);
        d = k / (r + r);
        break;
    }
    case 3: {
        float k = GT4_F32(t + 0xc), c = GT4_F32(t + 8), a2 = a * a, a4 = a2 * a2, a3 = a * a2;
        y = (k * a4) * a4 + c * a;
        d = ((k * K(0x41000000u)) * a4) * a3 + c;
        break;
    }
    default:
        break;
    }
    if (slope) *slope = d;
    if (x < 0) y = -y;
    return y;
}

/* A pedal from an analog value 0..4096: a dead zone lifted to 0.1. */
static float pedal_curve(float v)
{
    return v == 0 ? v : v + GT4_F32(0x00620268u) * (1.0f - v);
}

/* Rate limit toward target (analog pedals with the ramp setting on). */
static float pedal_ramp(float cur, float target, float up, float down)
{
    if (cur < target) { cur = cur + up; if (target < cur) cur = target; }
    else if (target < cur) { cur = cur - down; if (cur < target) cur = target; }
    return cur;
}

/* func_003481D8: clutch request (1 engaged) for gearboxes 0-2. */
static void clutch_request(uint32_t car, uint32_t cmd, int driver)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS);
    if (U8(CAR_5C0) || U8(CAR_56B) == 8) { GT4_U32(car + CAR_CLUTCH_REQ) = 0; return; }
    if (GT4_U16(car + CAR_5BE)) { GT4_U32(car + CAR_CLUTCH_REQ) = 0; return; }
    if (driver == 3) {
        float b = F(CAR_BRAKE_PEDAL);
        GT4_U32(car + CAR_BRAKE_PEDAL) = 0;
        F(CAR_CLUTCH_REQ) = 1.0f - b;
        if (!(1.0f - b == F(CAR_BRAKE_PEDAL))) { U8(CAR_REVERSE_ON) = 0; U8(CAR_BUTTON) = 0; }
        return;
    }
    if (driver == 6 && (GT4_U8(cmd + 2) || GT4_U16(cmd + 0xc))) { GT4_U32(car + CAR_CLUTCH_REQ) = 0; return; }
    if (GT4_U8(params + PARAMS_DRIVE + 3) == 1 && K(0x3f4ccccc) < F(CAR_HANDBRAKE)) { GT4_U32(car + CAR_CLUTCH_REQ) = 0; return; }
    if ((uint8_t)(GT4_U8(params + PARAMS_DRIVE + DRIVE_GEARBOX) - 1) < 2 && K(0x3f4ccccc) < F(CAR_HANDBRAKE)) {
        float v = F(CAR_SPEED) * F(CAR_SPEED) + F(CAR_LAT_SPEED) * F(CAR_LAT_SPEED);
        if (ps2_sqrt(v) < K(0x3e8e38e3)) { GT4_U32(car + CAR_CLUTCH_REQ) = 0; return; }
    }
    if (!U8(CAR_CLUTCH_STATE)) {
        GT4_U32(car + CAR_CLUTCH_REQ) = 0;
        if (F(CAR_BRAKE_PEDAL) < 0.25f) F(CAR_CLUTCH_REQ) = 1.0f;
        return;
    }
    F(CAR_CLUTCH_REQ) = 1.0f;
    if (!((int16_t)GT4_U16(car + CAR_ENGINE_RPM) < (int)GT4_U16(params + PARAMS_ENGINE + ENGINE_IDLE)))
        return;
    GT4_U32(car + CAR_CLUTCH_REQ) = 0;
}

/* func_00348368: the power split's "clutch": open when stopped with the
   brake down, and when off (car+0x5C0, car+0x5BE, driver 8). */
void gt4_split_clutch(uint32_t car)
{
    if (U8(0x5c0) || GT4_U16(car + CAR_5BE) || U8(0x56b) == 8) {
        GT4_U32(car + CAR_CLUTCH_REQ) = 0;
        return;
    }
    if (!U8(CAR_CLUTCH_STATE)) {
        GT4_U32(car + CAR_CLUTCH_REQ) = 0;
        if (F(CAR_BRAKE_PEDAL) < K(0x3dccccccu)) F(CAR_CLUTCH_REQ) = 1.0f;
        return;
    }
    F(CAR_CLUTCH_REQ) = 1.0f;
    if (K(0x3dccccccu) < F(CAR_BRAKE_PEDAL) && F(CAR_SPEED) < K(0x3e8e38e3u) && K(0xbe8e38e3u) < F(CAR_SPEED))
        GT4_U32(car + CAR_CLUTCH_REQ) = 0;
}

/* func_00348428 and func_00368548: creep, the throttle raised toward 0.3
   below 15 km/h; reverse when stopped with no pedals. */
static void split_creep(uint32_t car, uint32_t cmd)
{
    float v, r;
    if (F(CAR_CLUTCH_REQ) == 0) return;
    v = F(CAR_SPEED);
    if (!U8(CAR_GEAR)) v = -v;
    if (K(0xbe8e38e3u) <= v) {
        r = (K(0x40855554u) - v) / K(0x40855554u);
        if (r < 0) r = 0;
        r = r * K(0x3e999999u);
        F(CAR_THROTTLE_REQ) = r + (1.0f - r) * F(CAR_THROTTLE_REQ);
    }
    if (!(int16_t)GT4_U16(cmd + 8) && !(int16_t)GT4_U16(cmd + 0xc) && F(CAR_SPEED) < K(0xbe8e38e3u))
        U8(CAR_REVERSE_ON) = 1;
}


/* func_00349418: steering, then the pedals and the clutch. */
static void controls(uint32_t car, uint32_t cmd)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS);
    int kind = GT4_U8(cmd + 1), wheel = kind == 4 || kind == 6, driver = 0;
    int thr_analog = 1, rev_analog = 1;
    uint32_t thr = GT4_U16(cmd + 8), rev = GT4_U16(cmd + 0xc), v1;
    uint8_t flags = GT4_U8(cmd);
    float t = 0;

    GT4_U16(car + CAR_STEER_RAW_PREV) = GT4_U16(car + CAR_STEER_RAW);
    GT4_U16(car + CAR_STEER_RAW_PREV + 2) = GT4_U16(car + CAR_STEER_RAW + 2);
    steering(car, cmd);

    if ((int8_t)U8(CAR_56A) != 1 && !U8(CAR_56B)) driver = U8(CAR_546);
    if (!(flags & 2)) { thr = (thr << 12) & 0xffff; thr_analog = 0; }
    if (!(flags & 8)) { rev = (rev << 12) & 0xffff; rev_analog = 0; }
    if (GT4_U16(car + CAR_5BE) || K(0x40b1c71bu) < F(CAR_SPEED)) rev = 0;
    if (GT4_U8(params + 0x32) == 4) {
        rev = 0;
        if (0 < (int16_t)GT4_U16(cmd + 0xa)) thr = 0;
    }
    if (driver < 7) {
        if (driver < 3) {
            if (rev < thr) rev = 0;
            else { thr_analog = rev_analog; thr = rev; }
        } else if (driver == 5) rev = 0;
    }
    U8(CAR_REVERSE_ON) = rev != 0;

    /* throttle */
    if (!thr_analog) {
        if (thr) {
            t = F(CAR_THROTTLE_REQ) + GT4_F32(0x00620270u);
            if (1.0f < t) t = 1.0f;
            v1 = rev ? 2 : 1;
            U8(CAR_THROTTLE_MODE) = (uint8_t)v1;
        } else {
            t = F(CAR_THROTTLE_REQ) - GT4_F32(0x00620274u);
            if (t < 0) { U8(CAR_THROTTLE_MODE) = 0; t = 0; }
            v1 = U8(CAR_THROTTLE_MODE);
            if (v1 == 2) U8(CAR_REVERSE_ON) = 1;
        }
    } else if (thr) {
        t = pedal_curve((float)(int32_t)thr * K(0x39800000u));
        if (!wheel && GT4_U8(DRIVE_SETTINGS + 0x11))
            t = pedal_ramp(F(CAR_THROTTLE_REQ), t, GT4_F32(0x00620270u), GT4_F32(0x00620274u));
        U8(CAR_THROTTLE_MODE) = 0;
        v1 = 0;
    } else {
        v1 = U8(CAR_THROTTLE_MODE);
        if (v1) {
            t = F(CAR_THROTTLE_REQ) - GT4_F32(0x00620274u);
            if (t <= 0) { U8(CAR_THROTTLE_MODE) = 0; t = 0; v1 = 0; }
            if (v1 == 2) U8(CAR_REVERSE_ON) = 1;
        }
    }
    if (!v1 && wheel && !(GT4_U8(params + PARAMS_DRIVE + DRIVE_GEARBOX) == 3))   /* func_003683A8 */
        t = gt4_input_curve(0x00843548u, NULL, t);
    F(CAR_THROTTLE_REQ) = t;

    /* brake */
    if (!(flags & 4)) {
        float b;
        if ((int16_t)GT4_U16(cmd + 0xa)) {
            b = F(CAR_BRAKE_PEDAL) + GT4_F32(0x00620278u);
            F(CAR_BRAKE_PEDAL) = b;
            if (1.0f < b) F(CAR_BRAKE_PEDAL) = 1.0f;
        } else {
            b = F(CAR_BRAKE_PEDAL) - GT4_F32(0x0062027cu);
            F(CAR_BRAKE_PEDAL) = b;
            if (b < 0) GT4_U32(car + CAR_BRAKE_PEDAL) = 0;
        }
    } else {
        float b = (float)(int32_t)GT4_U16(cmd + 0xa) * K(0x39800000u);
        b = pedal_curve(b);
        if (wheel) b = gt4_input_curve(0x00843558u, NULL, b);
        else if (GT4_U8(DRIVE_SETTINGS + 0x11))
            b = pedal_ramp(F(CAR_BRAKE_PEDAL), b, GT4_F32(0x00620278u), GT4_F32(0x0062027cu));
        F(CAR_BRAKE_PEDAL) = b;
    }
    U8(CAR_BUTTON) = GT4_U16(car + CAR_5BE) ? 0 : GT4_U8(cmd + 2);

    /* handbrake */
    if (!(flags & 0x10)) {
        float h;
        if ((int16_t)GT4_U16(cmd + 0xe)) {
            h = F(CAR_HANDBRAKE) + GT4_F32(0x00620280u);
            F(CAR_HANDBRAKE) = h;
            if (1.0f < h) F(CAR_HANDBRAKE) = 1.0f;
        } else {
            h = F(CAR_HANDBRAKE) - GT4_F32(0x00620284u);
            F(CAR_HANDBRAKE) = h;
            if (h < 0) GT4_U32(car + CAR_HANDBRAKE) = 0;
        }
    } else F(CAR_HANDBRAKE) = pedal_curve((float)(int16_t)GT4_U16(cmd + 0xe) * K(0x39800000u));

    /* clutch */
    if (GT4_U8(params + PARAMS_DRIVE + DRIVE_GEARBOX) == 4) {
        if (U8(CAR_5C0)) GT4_U32(car + CAR_THROTTLE_REQ) = 0;
        F(CAR_CLUTCH_REQ) = 1.0f;
        return;
    }
    if (GT4_U8(params + PARAMS_DRIVE + DRIVE_GEARBOX) == 3) {
        gt4_split_clutch(car);
        split_creep(car, cmd);
        return;
    }
    clutch_request(car, cmd, driver);
    if ((uint8_t)(GT4_U8(params + PARAMS_DRIVE + DRIVE_GEARBOX) - 1) >= 2) return;   /* func_003F1F98 */
    if (!(F(CAR_HANDBRAKE) < K(0x3f4ccccc))) {
        float v = F(CAR_SPEED) * F(CAR_SPEED) + F(CAR_LAT_SPEED) * F(CAR_LAT_SPEED);
        if (!(K(0x3e8e38e3) <= ps2_sqrt(v))) return;
    }
    {
        /* throttle kept up to hold the engine above idle */
        float tq = F(CAR_ENGINE_BRAKE), m;
        m = ((float)(int32_t)GT4_U16(params + PARAMS_ENGINE + ENGINE_IDLE) + 500.0f
             - (float)(int16_t)GT4_U16(car + CAR_ENGINE_RPM)) * K(0x3a03126eu)
            + tq / (F(CAR_ENGINE_TORQUE) + tq);
        m = gt4_select(m < 0, 0.0f, gt4_select(1.0f < m, 1.0f, m));
        if (F(CAR_THROTTLE_REQ) < m) F(CAR_THROTTLE_REQ) = m;
    }
    if (!(int16_t)GT4_U16(cmd + 8) && !(int16_t)GT4_U16(cmd + 0xc) && !U8(CAR_GEAR)) U8(CAR_REVERSE_ON) = 1;
}

/* func_00349F48 */
void gt4_car_controls(uint32_t car, uint32_t cmd)
{
    if (GT4_U8(gt4_car_layout(car) + LAYOUT_KIND) == 1)
        gt4_orig_call(0x00349418u, car, cmd, 0, 0);   /* layout kind 1 steers otherwise */
    else
        controls(car, cmd);
    F(CAR_THROTTLE) = F(CAR_THROTTLE_REQ);
    {
        /* func_00343CF8: brake pedal to every wheel */
        int n = GT4_U8(gt4_car_layout(car) + LAYOUT_WHEELS);
        float b = F(CAR_BRAKE_PEDAL);
        for (int i = 0; i < n; i++) GT4_F32(gt4_car_wheel(car, i) + WHEEL_BRAKE_IN_) = b;
    }
}

/* func_003484D0: steering for the display (-100..100) and whether the
   throttle is fully down. */
static void input_display(uint32_t car, uint32_t cmd)
{
    int16_t steer = (int16_t)GT4_U16(cmd + 4), thr = (int16_t)GT4_U16(cmd + 8);
    if (!(GT4_U8(cmd) & 1)) U8(0x564) = (uint8_t)(steer == 0 ? 0 : steer > 0 ? 100 : -100);
    else {
        int32_t v = steer * 100;
        U8(0x564) = (uint8_t)((v < 0 ? v + 0xfff : v) >> 12);
    }
    if (GT4_U8(cmd) & 2) { if (thr != 0x1000) U8(0x565) = 0; }
    else if (!thr) U8(0x565) = 0;
}

/* func_00350C20: height of the ground under the reference point
   (car+0x720), kept at car+0x728 with its triangle at car+0x72c. */
static void reference_ground(uint32_t car)
{
    uint32_t course = GT4_U32(GT4_U32(car + 4) + 0xcbd8u), mesh = GT4_U32(GT4_U32(course + 4) + 0xdc);
    gt4_ray_hit hit;
    float p[3];
    F(0x728) = F(CAR_POSITION + 8);
    p[0] = F(0x720);
    p[1] = F(0x728) + 5.0f;   /* func_00395760: from 5 m above */
    p[2] = -F(0x724);
    if (gt4_course_ray_down(mesh, &hit, p) || gt4_course_ray_down(mesh, &hit, p)) {
        F(0x728) = hit.pos[1];
        GT4_U32(car + 0x72c) = (uint32_t)(uint16_t)hit.tri | (uint32_t)(int32_t)hit.leaf << 16;
    } else GT4_U32(car + 0x72c) = 0u - GT4_U32(car + 0x72c);
}

/* A virtual call: the table at [obj + table], entry +off (s16 this offset,
   function address). */
static uint32_t vcall(uint32_t obj, uint32_t table, uint32_t off, uint32_t a1, uint32_t a2)
{
    uint32_t e;
    if (table == 0x10140u) return gt4_session(obj, off, a1, a2, 0, 0, 0, NULL);
    e = GT4_U32(obj + table) + off;
    if (gt4_returns_zero(GT4_U32(e + 4))) return 0;   /* the race worlds' answer to 0x290 and 0x318 */
    gt4_orig_a2 = a2;
    gt4_orig_call(GT4_U32(e + 4), obj + (uint32_t)(int32_t)(int16_t)GT4_U16(e), a1, 0, 0);
    return gt4_orig_v0;
}

/* func_00349F80 */
static void replay_controls(uint32_t car)
{
    gt4_ai_apply(car + 0x14);
    if (U8(CAR_56B) == 8) F(CAR_HANDBRAKE) = 1.0f;
}

/* func_0035BFE8: the pit crew's orders from the controller (e[0] 1 set,
   2 cancel): car+0x5BC flags (0x10 in the box, 4 tyres with the
   compounds e[1], e[2] into 0x40/0x80 and car+0x752/0x753, 8 fuel e[3]
   into 0x100); the stop's timer car+0x5BE from 90 steps. */
void gt4_pit_orders(uint32_t car, uint32_t e)
{
    uint32_t b = car + 0x104;
    uint16_t f = GT4_U16(b + 0x4b8), v;
    if (GT4_U8(e) == 1) {
        if (!(f & 0x10)) return;
        if (f & 4) {
            if (GT4_U8(e + 1) != 0xff) { f = (uint16_t)(f | 0x40); GT4_U16(b + 0x4b8) = f; }
            if (GT4_U8(e + 2) != 0xff) { f = (uint16_t)(f | 0x80); GT4_U16(b + 0x4b8) = f; }
            GT4_U8(b + 0x64e) = GT4_U8(e + 1);
            GT4_U8(b + 0x64f) = GT4_U8(e + 2);
        }
        if ((f & 8) && GT4_U8(e + 3)) { f = (uint16_t)(f | 0x100); GT4_U16(b + 0x4b8) = f; }
        v = (uint16_t)(f & 0xffe3);
        GT4_U16(b + 0x4b8) = v;
        if (!v) GT4_U16(b + 0x4ba) = 0;
    } else if (GT4_U8(e) == 2) {
        if (!(f & 0x100)) return;
        v = (uint16_t)(f ^ 0x100);
        GT4_U16(b + 0x4b8) = v;
        if (!(v & 0xc0)) GT4_U16(b + 0x4ba) = 90;
    }
}

/* func_00387D38: the view's mode (world+0xCCC) for the pit-lane camera,
   and world+0xCC8 from it by the view's kind (world+0xCC0), then the
   presentation's own update. */
static void world_view(uint32_t world, uint32_t on)
{
    int32_t kind;
    if (GT4_U32(world + 0xccc) == on) return;
    GT4_U32(world + 0xccc) = on;
    kind = (int32_t)GT4_U32(world + 0xcc0);
    if (0 <= kind) {
        if (kind < 2) {
            if (on == 0) GT4_U32(world + 0xcc8) = 0;
            else if (on == 1) GT4_U32(world + 0xcc8) = 1;
        } else if (kind < 4) {
            if (on == 0) GT4_U32(world + 0xcc8) = 2;
            else if (on == 1) GT4_U32(world + 0xcc8) = 3;
        }
    }
    if (!gt4_phys_headless) vcall(world, 0x64, 0x98, 0, 0);
}

/* func_003F1BD8, kinds 4..9 (the B-Spec monitor's orders and questions):
   for car idx of the race, 4 the pace (0..4; nothing when it is already
   so), 5 the overtake and 6 the pit (both only when the session's +0x30
   says the car may; the pit only in a race with pits, cond+0xCBE0 1, and
   nothing when its bit 8 of +0x4B1 already says so), written into the
   race's event block (cond+0xF8F8 + idx * 4: kind, value) the driver
   reads next (func_00349FD0); 7, 8, 9 answer the pace, the overtake and
   the pit. Returns 1 for an order given, 0 for none needed, -1 refused. */
/* func_0035BC70: the pit menu asked by the stop (car+0x5BC bit 0, taken
   once, 0x10 set): 1, with 2|4 the tyres (flag 4), 8 the fuel (flag 8),
   0x10 a car not in B-Spec (+0x466 not 1); 0 none */
int gt4_pit_menu_ask(uint32_t car)
{
    uint32_t f = GT4_U16(car + 0x5bc) ^ 1u, a = f | 0x10, v;
    if (f & 1) return 0;
    GT4_U16(car + 0x5bc) = (uint16_t)a;
    v = (a & 4) ? 7 : 1;
    v |= a & 8;
    if ((int8_t)GT4_U8(car + 0x104 + 0x466) != 1) v |= 0x10;
    return (int)v;
}

/* func_0035BCC8: the player touched the menu: the stop's countdown
   (car+0x750, 300 in B-Spec) no longer runs out on its own */
void gt4_pit_menu_touch(uint32_t car)
{
    uint32_t b = car + 0x104;
    GT4_U16(b + 0x64c) = (int16_t)GT4_U16(b + 0x64c) > 0 ? 0 : 0xffff;
}

/* func_0035BCE8: the menu is over without the player (the car out of
   the box's flag 0x10, or its countdown at its end) */
int gt4_pit_menu_gone(uint32_t car)
{
    uint32_t b = car + 0x104;
    if (!(GT4_U8(b + 0x4b1) & 0x10)) return 1;
    return (int16_t)GT4_U16(b + 0x64c) == 1;
}

/* func_0035BF08: the menu's choice into the car's pit orders (cond+0xF910:
   1, the front and rear compounds, the fuel), unless a choice is there
   already and this one keeps the tyres (0xFE); the driving mode (0 A-Spec,
   1 B-Spec, 0xFF kept) a controller event 10 when it changes */
void gt4_pit_menu_choose(uint32_t car, int front, int rear, int feed, int mode)
{
    uint32_t b = car + 0x104, cond = GT4_U32(car + 4), e = cond + 0xf910u + GT4_U8(b + 0x440) * 4u;
    int now;
    GT4_U16(b + 0x64c) = 0;
    if (GT4_U8(e) == 1 && (front & 0xff) == 0xfe) return;
    GT4_U8(e + 3) = (uint8_t)feed;
    GT4_U8(e + 1) = (uint8_t)front;
    GT4_U8(e + 2) = (uint8_t)rear;
    GT4_U8(e) = 1;
    now = (int8_t)GT4_U8(b + 0x466);
    if ((now == 0 && (mode & 0xff) == 1) || (now == 1 && !(mode & 0xff)))
        gt4_bspec_order(cond, GT4_U8(b + 0x440), 10, mode & 0xff);
}

int gt4_bspec_order(uint32_t cond, int idx, int kind, int value)
{
    uint32_t car, obj, b, blk;
    if (idx < 0 || idx >= (int)GT4_U32(cond + 0xf8a0u)) return -1;
    car = cond + 0x6698u + (uint32_t)idx * CAR_SIZE;
    obj = car + 0x14;
    b = car + 0x104;
    switch (kind) {
    case 4:
        if (value < 0 || value >= 5) return -1;
        if (GT4_U8(obj + 0x6d) == (uint32_t)value) return 0;
        break;
    case 5:
        if (gt4_session(cond, 0x30, 0, 0, 0, 0, 0, NULL)) return -1;
        break;
    case 6:
        if (GT4_U8(cond + 0xcbe0u) != 1 || gt4_session(cond, 0x30, 0, 0, 0, 0, 0, NULL)) return -1;
        if (value ? (GT4_U8(b + 0x4b1) & 8) : !(GT4_U8(b + 0x4b1) & 8)) return 0;
        break;
    case 7: return GT4_U8(obj + 0x6d);
    case 8: return GT4_U8(obj + 0x6e);
    case 9: return (GT4_U8(b + 0x4b1) & 8) != 0;
    case 10: break;                                  /* the driving mode */
    default: return -1;
    }
    blk = cond + 0xf8f8u + (uint32_t)idx * 4;
    GT4_U8(blk) = (uint8_t)kind;
    GT4_U8(blk + 1) = (uint8_t)value;
    return 1;
}

/* func_003F19E0: an event from the controller (kind, value): 4 the
   gearbox the player picks (its ratios from D_006220B0/C8, the fuel mode
   from D_006220A8 when there is fuel), 5 the model's hold (released back
   onto the middle line), 6 the pit request, 10 the pit-lane view. */
void gt4_controller_event(uint32_t car, int kind, uint32_t v)
{
    uint32_t b = car + 0x104, obj = car + 0x14;
#define OU8(o) GT4_U8(obj + (o))
#define OF(o) GT4_F32(obj + (o))
    switch (kind) {
    case 4:
        if (OU8(0x6d) == v) return;
        OU8(0x6d) = (uint8_t)v;
        OF(0x74) = GT4_F32(0x006220c8u + v * 4);
        OF(0x70) = GT4_F32(0x006220b0u + v * 4);
        if (0 < GT4_F32(car + 0x5d0)) OU8(0x6f) = GT4_U8(0x006220a8u + v);   /* func_00354E38 */
        return;
    case 5:
        if (OU8(0x6e) == v) return;
        OU8(0x6e) = (uint8_t)v;
        if (v || GT4_U8(b + 0x467)) return;
        if (OU8(0x1c) == 1 || OU8(0x1c) == 2) gt4_ai_line_switch(obj, 6, 0);
        return;
    case 6: {
        uint8_t f = GT4_U8(b + 0x4b1);
        if (!(int16_t)GT4_U16(b + 0x4ac) || gt4_car_race_over(car)) return;
        if (v) {
            if (f & 8) return;
            GT4_U8(b + 0x5c1) = 0;
            GT4_U8(b + 0x4b1) = (uint8_t)(f | 8);
        } else if (f & 8) {
            GT4_U8(b + 0x4b1) = (uint8_t)(f & 0xf7);
            if (f & 4) GT4_U8(b + 0x5c1) = 1;
        }
        return;
    }
    case 10:
        GT4_U8(b + 0x466) = v ? 1 : 0;
        world_view(GT4_U32(GT4_U32(car) + 0x84), v ? 1 : 0);
        if (v) {                                                   /* func_00469E48 */
            OU8(0x41) = 0;
            OU8(0x40) = 0xff;
        }
        return;
    }
#undef OU8
#undef OF
}

gt4_phys_pads gt4_phys_pad;

/* The game's controller class (its table entries func_00426CE0,
   func_00426D50, func_00426EA0) does nothing past the command's clearing
   while it has no device (+0x1CC) or is held (+0x1C8). */
int gt4_pad_idle(uint32_t car, uint32_t off, uint32_t fn)
{
    uint32_t obj = GT4_U32(car + 0xc), e = GT4_U32(obj + 0xd0) + off;
    if (GT4_U32(e + 4) != fn) return 0;
    if (off == 0x28) return !GT4_U32(obj + 0x1cc);
    return GT4_U32(obj + 0x1c8) || !GT4_U32(obj + 0x1cc);
}

/* cvt.w.s: truncated, saturating as the EE does. */
static int32_t ffb_cvtw(float x)
{
    if (!(x < 2147483648.0f)) return 0x7fffffff;
    if (!(-2147483648.0f < x)) return (int32_t)0x80000000u;
    return (int32_t)x;
}

/* func_00353EE8: the steering wheel's force (the pad outputs of the
   car's player slot, out[2]: 0x80 centred) and its settings, by the
   controller kind car+0x569 (3 a force wheel: row 1, or 2 with
   params+0x10D3) in the rows of D_008436B0 (0xB0 each): the front tyres'
   aligning torque (+0x68, scaled by their slip and forces when the row
   asks), through a response curve (another when the race's AI level is
   set), limited per the row's mode; the wheels' own motion when the
   centring setting D_00622173 asks, the steering's speed and angle, the
   front tyres' bumps along the ground; at a standstill only the angle;
   a force wheel's end stops; fading in after a reset (car+0x737). Off
   while pitting or the model drives: centred by the steering input. */
void gt4_car_force_feedback(uint32_t car)
{
    uint32_t b = car + 0x104, params = GT4_U32(car + CAR_PARAMS), lay = gt4_car_layout(car);
    uint32_t out = GT4_U32(car + 4) + 0xca58u + GT4_U8(car + 0x544) * 16u;
    uint32_t row, lvl;
    int kind = 0, ai, n, still, own = 0;
    int16_t state[2] = { 1, 1 };                               /* func_003F7170 */
    float f = 0, g, lim, scale = ONE_F, speed;
    if (!gt4_phys_scratch) {
        GT4_ORIG(0x00353ee8u, car);
        return;
    }
    GT4_U16(out + 8) = 0;
    GT4_U8(out + 0xa) = 0;
    GT4_U8(out + 0xb) = 0;
    GT4_U8(out + 0xd) = 0;
    GT4_U8(out + 0xe) = 0;
    GT4_U8(out + 0xf) = 0;
    if ((!gt4_phys_pad.wheel_state || !gt4_phys_pad.wheel_state(gt4_phys_pad.ctx, car, state))
        && !gt4_pad_idle(car, 0x20, 0x00426ce0u)) {                               /* the wheel's state */
        uint32_t buf = gt4_phys_scratch + 0x780;
        GT4_U16(buf) = 1;
        GT4_U16(buf + 2) = 1;
        vcall(GT4_U32(car + 0xc), 0xd0, 0x20, buf, GT4_U32(car + 0xc));
        state[0] = (int16_t)GT4_U16(buf);
        state[1] = (int16_t)GT4_U16(buf + 2);
    }
    if (GT4_U8(b + 0x465) == 3) kind = GT4_U8(params + 0x10d3) ? 2 : 1;
    lvl = GT4_U8(GT4_U32(car + 4) + 0xcbddu);                 /* func_0034C998 */
    ai = lvl == 2 || lvl == 3;
    if ((uint8_t)(GT4_U8(b + 0x466) - 1) < 2 || GT4_U8(b + 0x467)) {
        int32_t v = (int16_t)GT4_U16(b + 0x454), c;
        if (GT4_U8(b + 0x465) == 3) v = v * 2;
        c = 0x80 - v / 32;
        GT4_U8(out + 2) = (uint8_t)(c < 0 ? 0 : c < 0x100 ? c : 0xff);
        return;
    }
    row = 0x008436b0u + (uint32_t)kind * 0xb0u;
    n = GT4_U8(lay + LAYOUT_WHEELS);
    for (int i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i);
        float t;
        if (!(0 < (int8_t)GT4_U8(lay + LAYOUT_HANDBRAKE + i))) continue;
        t = GT4_F32(w + 0x68);
        if (GT4_U8(row + 0x50)) {
            float sl = GT4_F32(w + 0x34), k;
            if (sl < 0) sl = -sl;
            k = GT4_F32(row + 0x54) + (ONE_F - sl * GT4_F32(row + 0x5c)) * GT4_F32(row + 0x58);
            k = k + ((GT4_F32(w + 0x70) + GT4_F32(w + 0x74)) / (GT4_F32(params + 0x1040) * K(0x3f000000u))) * GT4_F32(row + 0x60);
            if (K(0x40000000u) < k) t = t * K(0x40000000u);
            else if (k < K(0xbf800000u)) t = t * K(0xbf800000u);
            else t = t * k;
        }
        f = f + t;
    }
    f = f / GT4_F32(params + 0x1040);
    if (ONE_F < f) f = ONE_F;
    else if (f < K(0xbf800000u)) f = K(0xbf800000u);
    f = gt4_input_curve(row + (ai ? 0x1c : 0xc), NULL, f);
    if (GT4_U8(b + 0x465) == 3) scale = GT4_F32(row + 0x80 + (uint32_t)(int32_t)state[0] * 4);   /* func_003F7158 */
    lim = GT4_F32(row + (ai ? 8 : 4));
    switch (GT4_U8(row + 0x7c)) {
    case 1:
        if (lim < f) f = lim;
        else if (f < -lim) f = -lim;
        f = f * scale;
        break;
    case 2:
        if (scale < f) f = scale;
        else if (f < -scale) f = -scale;
        f = f * lim;
        break;
    default:
        f = f * lim * scale;
        break;
    }
    speed = GT4_F32(b + 0x544) * GT4_F32(b + 0x544) + GT4_F32(b + 0x548) * GT4_F32(b + 0x548);
    speed = speed == 0 ? speed : sqrtf(fabsf(speed));
    still = speed < K(0x3f0e38e3u);
    if (GT4_U32(0x00845bf0u)) {                                /* func_003F7C80 */
        uint8_t centre = GT4_U8(0x00622173u);
        if (centre == 2 || (centre == 1 && still)) own = 1;
    }
    if (own) {
        for (int i = 0; i < n; i++) {
            uint32_t w = gt4_car_wheel(car, i);
            uint32_t m = GT4_U32(0x00845bf0u);
            float x = 0, y;
            uint8_t pos;
            if (!(0 < (int8_t)GT4_U8(lay + LAYOUT_HANDBRAKE + i))) continue;
            pos = GT4_U8(lay + 0x2c + i);
            if (m == 1) x = -GT4_F32(w + 0x1c);
            else if (m == 2) x = -(GT4_F32(w + 0x1c) - GT4_F32(w));
            y = gt4_input_curve(row + 0x40, NULL, x / GT4_F32(0x00845bf8u + pos * 4u));   /* func_003F7C90 */
            f = (f + GT4_F32(row + 0x38) * y) - GT4_F32(w + 0x18) * GT4_F32(row + 0x3c);
        }
        if (ONE_F < f) f = ONE_F;
        else if (f < K(0xbf800000u)) f = K(0xbf800000u);
    }
    f = f + (float)(((int32_t)(int16_t)GT4_U16(b + 0x454) - (int32_t)(int16_t)GT4_U16(b + 0x456)) * 60) * GT4_F32(row + 0x2c);
    {
        float a = GT4_F32(b + 0x468), c = GT4_F32(row + 0x34);
        if (c < a) a = c;
        else if (a < -c) a = -c;
        f = f + GT4_F32(row + 0x30) * a;
    }
    g = 0;
    for (int i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i), gr = car + CAR_GROUND + (uint32_t)i * GROUND_SIZE;
        float sn, cs, t[3], u[3], d = 0;
        if (!(0 < (int8_t)GT4_U8(lay + LAYOUT_HANDBRAKE + i))) continue;
        sn = gt4_sinf(GT4_F32(w));
        cs = gt4_cosf(GT4_F32(w));
        for (int k = 0; k < 3; k++) t[k] = sn * GT4_F32(b + 0x18 + 4 * k) - cs * GT4_F32(b + 0xc + 4 * k);
        u[0] = GT4_F32(gr + 0x1c);
        u[1] = -GT4_F32(gr + 0x24);
        u[2] = GT4_F32(gr + 0x20);
        for (int k = 0; k < 3; k++) d = d + t[k] * u[k];
        g = g + d * GT4_F32(w + 0x48);
    }
    g = g * ((GT4_F32(params + 0x31c) + K(0x473f67ffu)) / K(0x483f67ffu));
    f = f + (g * GT4_F32(row)) / GT4_F32(params + 0x1040);
    if (still && !own) {
        float a = GT4_F32(b + 0x468);
        if (K(0x3db2b8c1u) < a) a = K(0x3db2b8c1u);
        else if (a < K(0xbdb2b8c1u)) a = K(0xbdb2b8c1u);
        f = a * K(0x40b758b5u);
    }
    if (GT4_U8(b + 0x465) == 3 && !ai) {
        GT4_U8(out + 0xd) = GT4_U8(row + 0x79);
        GT4_U8(out + 0xe) = GT4_U8(row + 0x7a);
        GT4_U8(out + 0xf) = GT4_U8(row + 0x7b);
    }
    if (ONE_F < f) f = ONE_F;
    else if (f < K(0xbf800000u)) f = K(0xbf800000u);
    if (GT4_U8(b + 0x465) == 3 && state[1]) {   /* func_003F7168: the end stops */
        int32_t p = (int16_t)GT4_U16(b + 0x454), s0 = p - (int16_t)GT4_U16(b + 0x456);
        uint32_t world = GT4_U32(GT4_U32(car) + 0x84);
        float y, x;
        if ((int32_t)vcall(world, 0x64, 0x318, (uint32_t)p, world) > 0)
            s0 = p - (p + (int16_t)GT4_U16(b + 0x45a)) / 2;
        x = (float)s0;
        if (x < 0) x = -x;
        y = gt4_curve_eval(gt4_ptr(row + 0xa4), x);             /* D_00843754 */
        if (0 < s0) {
            if (0 < f) f = f * y;
        } else if (s0 < 0 && f < 0) {
            f = f * y;
        }
    }
    {
        uint8_t c = GT4_U8(b + 0x633);
        if (c) {
            GT4_U8(b + 0x633) = (uint8_t)(c - 1);
            f = f * ((float)(30 - (int32_t)c) * K(0x3d088888u));
        }
    }
    GT4_U8(out + 2) = (uint8_t)(-0x80 - ffb_cvtw(f * K(0x42fe0000u)));
}

/* func_00349FD0: one step of the driver: read the controller (through its
   virtual table), the steering input extrapolated for wheels, then the
   controls by mode (0 player, 1/2 replay). `buf` is 0x40 bytes of guest
   memory for the command and the event block. */
void gt4_car_driver(uint32_t car, uint32_t buf)
{
    uint32_t cmd = buf, ev = buf + 0x20, params = GT4_U32(car + CAR_PARAMS), blk;
    uint32_t session = GT4_U32(car + 4), player = U8(CAR_PLAYER) * 4u;
    int mode, replay = 0, row, damp, gain, d_now = 0, d_prev = 0;
    uint16_t steer;
    int16_t prev;

    memset(gt4_ptr(ev), 0, 0x20);                                  /* func_003557E8 */
    blk = session + player + 0xf910u;
    if (GT4_U8(blk)) {
        for (int k = 0; k < 4; k++) { GT4_U8(ev + 0x10 + k) = GT4_U8(blk + k); GT4_U8(blk + k) = 0; }
    }
    GT4_U8(ev + 3) = 4;
    if (U8(CAR_56A) < 2) {
        blk = session + player + 0xf8f8u;
        if (GT4_U8(blk)) {
            GT4_U8(ev + 0x14) = GT4_U8(blk);
            GT4_U8(ev + 0x15) = GT4_U8(blk + 1);
            GT4_U8(blk + 1) = 0;
            GT4_U8(blk) = 0;
        }
        GT4_U8(ev + 3) = 6;
    }
    memset(gt4_ptr(cmd), 0, 0x20);                                  /* controller read; func_003557E8 */
    if ((!gt4_phys_pad.read || !gt4_phys_pad.read(gt4_phys_pad.ctx, car, gt4_ptr(cmd), gt4_ptr(ev)))
        && !gt4_pad_idle(car, 0x28, 0x00426d50u))
        vcall(GT4_U32(car + 0xc), 0xd0, 0x28, cmd, ev);
    if (GT4_U8(cmd + 0x10)) gt4_pit_orders(car, cmd + 0x10);
    if (U8(CAR_56A) < 2 && GT4_U8(cmd + 0x14)) {
        gt4_controller_event(car, GT4_U8(cmd + 0x14), GT4_U8(cmd + 0x15));
    }
    if (!U8(CAR_VIBRATION_ON)) vcall(session, 0x10140u, 0xc8, GT4_U32(car + 0xc), cmd);
    if (GT4_U8(cmd + 7) && !U8(CAR_56B)) {
        float boost = GT4_F32(params + 0x10d4);
        if (0 < boost && 0 < F(0x5e4)) {
            U8(0x5e8) = 6;
            F(CAR_ENGINE_TORQUE) = F(CAR_ENGINE_TORQUE) * (boost + 1.0f);
        }
    }
    U8(0x565) = 1;

    /* steering extrapolation per device (row of D_00622110 + 0x70) */
    row = GT4_U8(cmd + 1) != 6 ? 0 : GT4_U8(params + PARAMS_WHEEL_SCALE) ? 2 : 1;
    {
        uint32_t r = DRIVE_SETTINGS + 0x70 + (uint32_t)row * 32, sys = GT4_U32(GT4_U32(car) + 0x84);
        damp = GT4_U8(r + 0x1a);
        gain = GT4_U8(r + 0x1b);
        if (vcall(sys, 0x64, 0x290, r, sys)) gain += 2;
        if (damp) {
            prev = (int16_t)GT4_U16(car + CAR_STEER_RAW);
            d_now = (int16_t)GT4_U16(cmd + 4) - prev;
            d_prev = prev - (int16_t)GT4_U16(car + CAR_STEER_RAW + 2);
            if ((int32_t)vcall(sys, 0x64, 0x318, (uint32_t)(int32_t)prev, sys) > 0) {
                int32_t a = (int16_t)GT4_U16(cmd + 4), b = (int16_t)GT4_U16(car + CAR_STEER_RAW + 2);
                int32_t mid = (a + b) / 2;
                d_prev = mid - b;
                d_now = a - mid;
            }
        }
    }
    steer = GT4_U16(cmd + 4);
    GT4_U16(car + CAR_STEER_RAW + 2) = GT4_U16(car + CAR_STEER_RAW);
    GT4_U16(car + CAR_STEER_RAW) = steer;
    if (damp) {
        int32_t s = (int16_t)steer + gain * d_now;
        if (damp >= 2) s = s + gain * gain * (d_now - d_prev);
        if (s < -0x1000) s = -0x1000;
        if (!(s < 0x1001)) s = 0x1000;
        GT4_U16(cmd + 4) = (uint16_t)s;
    }

    mode = (int8_t)U8(CAR_56A);
    if (mode == 1) {
        input_display(car, cmd);
        gt4_ai_drive(car, cmd);
        replay_controls(car);
        replay = 1;
    } else if (mode == 0) {
        input_display(car, cmd);
        if (U8(CAR_56B)) {
            gt4_ai_drive(car, cmd);
            gt4_ai_gear(car + 0x14);
            replay_controls(car);
            replay = 1;
        } else {
            gt4_ai_player_aim(car + 0x14);   /* the driver model's aim */
            gt4_car_controls(car, cmd);
            gt4_ai_gear(car + 0x14);
        }
    } else if (mode == 2) {
        gt4_ai_drive(car, cmd);
        replay_controls(car);
        replay = 1;
    }

    if (GT4_U8(params + 0x32) == 4 && U8(CAR_CLUTCH_STATE) == 3) {
        int n = GT4_U8(gt4_car_layout(car) + LAYOUT_WHEELS);
        GT4_U32(car + CAR_THROTTLE_REQ) = 0;
        GT4_U32(car + CAR_BRAKE_PEDAL) = 0;
        for (int i = 0; i < n; i++) GT4_F32(gt4_car_wheel(car, i) + WHEEL_BRAKE_IN_) = F(CAR_BRAKE_PEDAL);
    }
    reference_ground(car);
    if (GT4_U8(params + 0x32) == 6) {
        float k = GT4_F32(0x006207b4u);
        F(CAR_INPUTS + 4) = F(CAR_INPUTS + 4) * (1.0f - k) + F(CAR_STEER) * k;   /* func_0035A540/0035A618 */
    }
    if (replay) F(CAR_STEER_SHOWN) = steer_shown(F(CAR_STEER)) * K(0x3f2aacd9u);
}

/* The driving settings (D_00622110, bytes) expanded into the tables the
   controls, the assists and the force feedback read, at the race's
   start (func_00347A00 with func_00353938 and func_0035A818). */
#define G(o) ((float)(int32_t)GT4_U8(0x00622110u + (o)))
#define GS(o) ((float)(int32_t)(int8_t)GT4_U8(0x00622110u + (o)))

/* div.s as the EE does it: x/0 the largest float, signed as x^0 */
static float ds_sqrt(float x) { return x == 0 ? x : sqrtf(fabsf(x)); }

static float ds_div(float a, float b)
{
    uint32_t sa, sb;
    if (b != 0) return a / b;
    memcpy(&sa, &a, 4);
    memcpy(&sb, &b, 4);
    return ((sa ^ sb) >> 31) ? K(0xff7fffffu) : K(0x7f7fffffu);
}

static int32_t pct(int32_t v, int zero_ok) { return (zero_ok ? v < 0 : v <= 0) ? 0 : v < 101 ? v : 100; }

/* func_0035A818: a response curve (see gt4_input_curve) of type `type`
   and strength p percent: 1 a parabola, 2 a root, 3 a blend, else none. */
void gt4_input_curve_set(uint32_t t, uint32_t type, int32_t p)
{
    GT4_U32(t) = type;
    if (type == 2) {
        float c = (float)(100 - pct(p, 1)) * K(0x3c23d70au) + K(0x3f800000u), c2 = c * c, k;
        GT4_F32(t + 0xc) = c;
        k = -((c2 - K(0x3f800000u)) * (c2 - K(0x3f800000u)));
        k = ds_div(k, c2 * K(0x40800000u));
        GT4_F32(t + 4) = k;
        GT4_F32(t + 8) = -c * ds_sqrt(-k);
    } else if (type == 1) {
        float c = (float)pct(p, 0) * K(0x3c23d70au), k;
        GT4_F32(t + 0xc) = c < K(0x3a83126eu) ? K(0x3a83126eu) : c;
        c = GT4_F32(t + 0xc);
        k = ds_div(-(K(0x3f800000u) - c), c + c);
        GT4_F32(t + 4) = k;
        GT4_F32(t + 8) = (-c * k) * k;
    } else if (type == 3) {
        float c = (float)pct(p, 0) * K(0x3c23d70au);
        GT4_U32(t + 4) = 0;
        GT4_F32(t + 0xc) = c;
        GT4_F32(t + 8) = K(0x3f800000u) - c;
    } else {
        GT4_U32(t + 0xc) = 0;
        GT4_U32(t + 4) = 0;
        GT4_U32(t + 8) = 0;
    }
}

/* 900/(n*60*4096), or 1/4096 without n */
static float wheel_rate(uint8_t n)
{
    if (!n) return K(0x39800000u);
    return ds_div(K(0x44610000u), (float)(int32_t)(n * 60) * K(0x45800000u));
}

/* func_00353938: the force feedback's rows (D_008436B0, 0xB0 each: pad,
   and the two wheels) from the rows of D_0062218C (0x20 each). */
static void feedback_rows(void)
{
    const float deg = K(0x3c8efa34u), hundredth = K(0x3c23d70au);
    for (uint32_t s = 0; s < 3; s++) {
        uint32_t w = 0x008436b0u + s * 0xb0u, r = 0x0062218cu + s * 0x20u;
#define R(o) ((float)(int32_t)GT4_U8(r + (o)))
        float f2, span;
        GT4_F32(w + 4) = R(0) * K(0x3ca3d70au);
        GT4_F32(w + 0) = R(3) * K(0x3dccccccu);
        GT4_F32(w + 8) = R(0x14) * K(0x3ca3d70au);
        gt4_input_curve_set(w + 0xc, GT4_U8(r + 1), GT4_U8(r + 2));
        gt4_input_curve_set(w + 0x1c, GT4_U8(r + 0x15), GT4_U8(r + 0x16));
        f2 = R(7) * K(0x3ae4c385u);
        GT4_F32(w + 0x34) = f2;
        GT4_F32(w + 0x2c) = (R(5) * hundredth) * K(0x39800000u);
        if (!GT4_U8(r + 6)) GT4_U32(w + 0x30) = 0;
        else GT4_F32(w + 0x30) = ds_div(R(6) * hundredth, f2);
        GT4_F32(w + 0x38) = R(0x10) * hundredth;
        GT4_F32(w + 0x3c) = ((float)(int32_t)(int8_t)GT4_U8(r + 0x11) * hundredth) / deg;
        gt4_input_curve_set(w + 0x40, GT4_U8(r + 0x12), GT4_U8(r + 0x13));
        GT4_U8(w + 0x50) = 0;
        if (GT4_U8(r + 0xa) + GT4_U8(r + 0xb) && GT4_U8(r + 0xc)) {
            float a = R(0xa), b = R(0xb), c = R(0xc) * deg, d = R(0xd), sum = a + b;
            GT4_F32(w + 0x60) = ds_div(d, sum);
            GT4_F32(w + 0x54) = ds_div(a, sum);
            GT4_F32(w + 0x58) = ds_div(b, sum);
            GT4_F32(w + 0x5c) = ds_div(K(0x3f800000u), c);
            GT4_U8(w + 0x50) = 1;
        }
        GT4_F32(w + 0x6c) = ds_div(K(0x41000000u), GT4_F32(0x00620264u));
        GT4_F32(w + 0x68) = ds_div(K(0x41000000u), GT4_F32(0x0062025cu));
        GT4_F32(w + 0x64) = G(0x74) / K(0x42c80000u);
        if (!GT4_U8(0x00622110u + 0x7a)) {
            GT4_U32(w + 0x70) = 0;
            GT4_U8(w + 0x78) = GT4_U8(0x00622110u + 0x75);
        } else {
            GT4_U8(w + 0x78) = 0;
            GT4_F32(w + 0x70) = G(0x75) * K(0x40233333u);
        }
        GT4_U8(w + 0x79) = GT4_U8(0x00622110u + 0x77);
        GT4_U8(w + 0x7a) = GT4_U8(0x00622110u + 0x78);
        GT4_U8(w + 0x7b) = GT4_U8(0x00622110u + 0x79);
        GT4_U8(w + 0x7c) = GT4_U8(r + 0x17);
        GT4_F32(w + 0x74) = G(0x76) * K(0x40233333u);
        for (uint32_t k = 0; k < 3; k++) GT4_F32(w + 0x80 + k * 4) = R(0x18 + k) * hundredth;
        span = (s ? K(0x44610000u) : K(0x43480000u)) * K(0x42700000u);
        GT4_U32(w + 0x98) = 0;
        GT4_F32(w + 0x8c) = K(0x3f800000u);
        GT4_F32(w + 0x94) = R(0x1c) * hundredth;
        GT4_F32(w + 0x90) = R(0x1b) * hundredth;
        GT4_F32(w + 0x9c) = ds_div((float)(int32_t)(GT4_U8(r + 0x1d) * 10) * K(0x46000000u), span);
        GT4_F32(w + 0xa0) = ds_div((float)(int32_t)(GT4_U8(r + 0x1e) * 10) * K(0x46000000u), span);
        GT4_U16(w + 0xa4) = 3;                                   /* func_00359A38 */
        GT4_U32(w + 0xa8) = w + 0x98;
        GT4_U32(w + 0xac) = w + 0x8c;
        GT4_U8(w + 0xa6) = 0;
        GT4_U8(w + 0xa7) = 0;
#undef R
    }
}

/* func_00347A00: the settings of the controls (pedal ramps D_00620270,
   the assists at D_00843568.., the steering speeds and counter-steer
   tables D_00843598..D_00843620, the wheels' rates D_0062025C..64, the
   response curves D_00843538/48/58), then the force feedback's rows. */
void gt4_drive_settings(uint32_t cond)
{
    const uint32_t A = 0x00620270u;
    const float sixty = K(0x42700000u), hundred = K(0x42c80000u), one = K(0x3f800000u);
    const float hundredth = K(0x3c23d70au), deg = K(0x3c8efa34u), kmh = K(0x3e8e38e3u), lo = K(0x4031c71bu);
    float f;
    GT4_F32(A + 0x00) = ds_div(hundred, G(0x14) * sixty);
    GT4_F32(A + 0x14) = ds_div(hundred, G(0x19) * sixty);
    GT4_F32(A + 0x04) = ds_div(hundred, G(0x15) * sixty);
    GT4_F32(A + 0x08) = ds_div(hundred, G(0x16) * sixty);
    GT4_F32(A + 0x0c) = ds_div(hundred, G(0x17) * sixty);
    GT4_F32(A + 0x10) = ds_div(hundred, G(0x18) * sixty);
    for (uint32_t k = 0; k < 6; k++)
        if (one < GT4_F32(A + k * 4)) GT4_F32(A + k * 4) = one;

    GT4_F32(0x00620268u) = G(4) * hundredth;
    GT4_F32(0x00843568u) = G(0x27) * hundredth;
    GT4_F32(0x0084356cu) = G(0x23) * hundredth;
    f = (float)(int32_t)(GT4_U8(0x00622110u + 0x2a) * 10) * kmh;
    GT4_F32(0x00843574u) = f < lo ? lo : f;
    GT4_F32(0x00843570u) = G(0x29) * hundredth;
    f = (float)(int32_t)(GT4_U8(0x00622110u + 0x2b) * 10) * kmh;
    GT4_F32(0x00843578u) = f < lo ? lo : f;
    GT4_U32(0x0084357cu) = GT4_U8(0x00622110u + 0x2c);
    if (GT4_U8(cond + 0x10000u - 0x3422u) == 2) GT4_U32(0x0084357cu) = 0;
    GT4_F32(0x00843580u) = G(0x2d) * K(0x3dccccccu);
    GT4_F32(0x00843584u) = GS(0x2e) * hundredth;
    GT4_F32(0x00843588u) = G(0x2f) * K(0x3ae4c385u);
    GT4_U32(0x0084358cu) = GT4_U8(0x00622110u + 0x30);
    GT4_F32(0x00843590u) = GT4_U8(0x00622110u + 0x31) ? G(0x31) * deg : K(0x40c90fdau);

    for (uint32_t t = 0; t < 2; t++) {
        GT4_F32(0x00843598u + t * 4) = G(0x1c + t);
        GT4_F32(0x008435a0u + t * 4) = G(0x1e + t);
        GT4_F32(0x008435a8u + t * 4) = GT4_U8(0x00622110u + 0x10) ? G(0x20 + t) : 0.0f;
        GT4_F32(0x008435b0u + t * 4) = G(0x24 + t) * K(0x3dccccccu);
        for (uint32_t k = 0; k < 2; k++) {
            uint32_t i = t * 2 + k;
            GT4_F32(0x008435b8u + i * 4) = G(0x3c + i) * deg;
            GT4_F32(0x008435c8u + i * 4) = G(0x40 + i) * deg;
            GT4_F32(0x008435d8u + i * 4) = G(0x44 + i) * deg;
        }
        GT4_U32(0x008435e8u + t * 4) = GT4_U8(0x00622110u + 0x36 + t) < GT4_U8(0x00622110u + 0x38 + t);
        GT4_F32(0x008435f0u + t * 4) = G(0x36 + t) * kmh;
        GT4_F32(0x008435f8u + t * 4) = G(0x38 + t) * kmh;
        for (uint32_t k = 0; k < 2; k++) {
            uint32_t i = t * 2 + k;
            GT4_F32(0x00843600u + i * 4) = G(0x4c + i) * deg;
            GT4_F32(0x00843610u + i * 4) = G(0x50 + i) * deg;
            GT4_F32(0x00843620u + i * 4) = G(0x54 + i) * deg;
        }
    }
    GT4_F32(0x00620248u) = G(0x5c) * deg;
    GT4_F32(0x0062024cu) = G(0x5d) * deg;
    GT4_F32(0x00620250u) = G(0x5e) * deg;
    GT4_F32(0x0062025cu) = wheel_rate(GT4_U8(0x00622110u + 0x61));
    GT4_F32(0x00620260u) = wheel_rate(GT4_U8(0x00622110u + 0x64));
    GT4_F32(0x00620264u) = wheel_rate(GT4_U8(0x00622110u + 0x62));
    gt4_input_curve_set(0x00843538u, GT4_U8(0x00622110u + 0x65), GT4_U8(0x00622110u + 0x66));
    gt4_input_curve_set(0x00843548u, GT4_U8(0x00622110u + 0x6c), GT4_U8(0x00622110u + 0x6d));
    gt4_input_curve_set(0x00843558u, GT4_U8(0x00622110u + 0x70), GT4_U8(0x00622110u + 0x71));
    feedback_rows();
}

#undef G
#undef GS
