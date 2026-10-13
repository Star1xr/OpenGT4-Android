/* A car at the race start: func_00363B10, called by the race init
   (func_0034D490) for each car after the record. It builds the
   parameters, hooks the car to its race entry and course, sets its driving
   aids and driver model, finds where it starts (grid slot, rolling start
   along a line, or the start distance) and places it there. The parts not
   rewritten yet call the original. */
#include "car.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define K(bits) gt4_f32(bits)
#define GAME_SETTINGS 0x00620cd8u
#define SIDES         0x00620ad8u   /* D_00620AD8: [6] which side of the grid slot */
#define SESSION_VT    0x10140u      /* conductor: the session's virtual table */

/* Still the original. */
static uint32_t orig(uint32_t addr, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    gt4_orig_a2 = a2;
    gt4_orig_a3 = a3;
    gt4_orig_call(addr, a0, a1, 0, 0);
    return gt4_orig_v0;
}
static float orig_f(uint32_t addr, uint32_t a0, uint32_t a1)
{
    return gt4_orig_call(addr, a0, a1, 0, 0);
}
/* session virtual calls, entry off: v0 and f0 */
static uint32_t session_call(uint32_t cond, uint32_t off, uint32_t a1, float *f0)
{
    return gt4_session(cond, off, a1, 0, 0, 0, 0, f0);
}

/* func_00362E58: a fresh tyre on wheel i: no wear or heat, or the race's
   starting ones when it has them. */
void gt4_wheel_fresh(uint32_t car, int i)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), cond = GT4_U32(car + 4);
    uint32_t w = gt4_car_wheel(car, i), d = cond + 4;
    GT4_F32(w + 0x50) = 1.0f;
    GT4_U32(w + 0xac) = 0;
    GT4_U8(w + 0xb1) = 0;
    GT4_U8(w + 0xb0) = 0;
    GT4_U32(w + 0xa4) = 0;
    GT4_U32(w + 0xa8) = 0;
    if (GT4_U8(d + 0x68)) {
        GT4_F32(w + 0xac) = GT4_F32(d);
        if (!GT4_U8(d + 0x69)) GT4_U8(w + 0xb1) = GT4_F32(w + 0xac) < 0 ? 0xc1 : 0;
        GT4_F32(w + 0x50) = 1.0f - GT4_F32(d + 4);
    }
    if (GT4_U8(d + 0x69)) {
        GT4_F32(w + 0xa8) = GT4_F32(d + 0x50);
        GT4_U8(w + 0xb1) = GT4_F32(d + 0x50) < 0 ? 0xc1 : 0;
        GT4_F32(w + 0x50) = GT4_F32(w + 0x50) * (1.0f - GT4_F32(d + 0x54));
    }
    if (GT4_U8(params + 0x32) == 4) {
        GT4_U32(w + 0xa8) = 0;
        GT4_F32(w + 0x50) = 1.0f;
        GT4_U32(w + 0xac) = 0;
        GT4_U8(w + 0xb1) = 0;
    }
}

/* func_0034C968 / func_0034C998: the race mode (conductor+0xCBDD, 0xFF as
   0), and 1 for mode 2, 2 for mode 3, else 0. */
static uint32_t race_mode(uint32_t cond)
{
    uint32_t m = GT4_U8(cond + 0xcbddu);
    return m == 0xff ? 0 : m;
}
static uint32_t mode_class(uint32_t cond)
{
    uint32_t m = race_mode(cond);
    return m == 2 ? 1 : m == 3 ? 2 : 0;
}
/* func_0034C230: the course's length */
static float course_length(uint32_t cond)
{
    return GT4_F32(GT4_U32(GT4_U32(GT4_U32(cond + 0xcbd8u) + 4) + 0xdc) + 0x14);
}
/* func_0034C3E0 */
static int32_t session_laps(uint32_t cond) { return gt4_session_laps(cond); }

static uint32_t entry_of(uint32_t race, uint32_t idx)
{
    return GT4_U32(GT4_U32(GT4_U32(race + 0x60) + 8) + idx * 4);
}

/* func_003952C0 / func_00395228: a grid slot of the course (x, y, z,
   heading; the variant at course+0xB0 when set), and its heading turned
   half round. */
static uint32_t grid_slot(uint32_t course, uint32_t slot)
{
    uint32_t rw = GT4_U32(GT4_U32(course + 4) + 0xdc);
    int32_t v = (int32_t)GT4_U32(course + 0xb0);
    uint32_t base = v < 0 ? GT4_U32(rw + 0x5c) : GT4_U32(GT4_U32(rw + 0x9c) + (uint32_t)v * 32 + 0x18);
    return base + slot * 16;
}

/* func_00359510: the car's drive layout (D_00620478) */
static uint32_t layout_of(uint32_t params)
{
    return GT4_LAYOUTS + GT4_U8(params + PARAMS_LAYOUT) * 84u;
}

/* func_00363538: the speed the car runs at (+0x6C8), its peak (+0x6CC) */
static void set_speed(uint32_t car, float v)
{
    GT4_F32(car + 0x6c8) = v;
    GT4_F32(car + 0x6d0) = v;
    if (GT4_F32(car + 0x6cc) < v) GT4_F32(car + 0x6cc) = v;
}

/* func_0036BEB8: the gear; a CVT (params+0x32 == 4) keeps its gear when
   given neutral. */
static void set_gear(uint32_t car, uint32_t gear)
{
    if (GT4_U8(GT4_U32(car + CAR_PARAMS) + 0x32) != 4 || gear) GT4_U8(car + 0x624) = (uint8_t)gear;
}

/* func_00363018: the wheels' per-step state */
static void wheels_clear(uint32_t car)
{
    uint32_t lay = layout_of(GT4_U32(car + CAR_PARAMS));
    for (uint32_t i = 0; i < GT4_U8(lay + LAYOUT_WHEELS); i++) {
        uint32_t w = car + 0x164 + i * 0xec;
        GT4_U32(w + 0xbc) = 0;
        GT4_U8(w + 0xb2) = 0xff;
        GT4_U32(w + 0xb4) = 0;
        GT4_F32(w + 0xb8) = 1.0f;
        GT4_U32(w + 0x24) = 0;
        GT4_U32(w + 0x40) = 0;
        GT4_U32(w + 0x38) = 0;
        GT4_U32(w + 0x58) = 0;
        GT4_U32(w + 0x90) = 0;
        GT4_U32(w + 0x98) = 0;
        GT4_U32(w + 0x9c) = 0;
        GT4_U32(w + 0xa0) = 0;
        GT4_U32(w + 0xc0) = 0;
        GT4_U32(w + 0xc4) = 0;
        GT4_U32(w + 0x5c) = 0;
        GT4_U32(w + 0xcc) = 0;
        GT4_U32(w + 0xd0) = 0;
        GT4_U32(w + 0xd4) = 0;
    }
}

/* func_00363560: the car at rest: wheels, engine, gear 1, controls. */
void gt4_car_rest(uint32_t car)
{
    uint32_t b = car + 0x104, p = GT4_U32(car + CAR_PARAMS), lay = layout_of(p), v;
    for (uint32_t i = 0; i < GT4_U8(lay + LAYOUT_WHEELS); i++) {
        uint32_t w = car + 0x164 + i * 0xec;
        GT4_U32(w + 0x30) = 0;
        GT4_U32(w + 0x60) = 0;
        GT4_U32(w + 0x6c) = 0;
        GT4_U32(w + 0x68) = 0;
        GT4_U32(w + 0x70) = 0;
        GT4_U32(w + 0x74) = 0;
        GT4_U32(w + 0x28) = 0;
        GT4_U32(w + 0x2c) = 0;
        GT4_U32(w + 0x20) = 0;
        GT4_U8(w + 0x46) = 0;
        GT4_F32(w + 0x3c) = 1.0f;
        GT4_U32(car + 0x534 + i * 4) = 0;
    }
    for (uint32_t k = 0; k < 2; k++) {
        GT4_U8(b + 0x524 + k) = (uint8_t)(GT4_U8(p + 0x244 + k) - 3) < 2;
        GT4_U32(b + 0x52c + k * 4) = 0;
    }
    v = GT4_U8(p + PARAMS_DRIVE);
    GT4_U8(b + 0x523) = (uint8_t)(v - 4) < 3 || v == 9;
    GT4_U32(b + 0x51c) = 0;
    GT4_F32(b + 0x444) = 1.0f;
    GT4_U32(b + 0x480) = 0;
    GT4_U32(b + 0x468) = 0;
    GT4_F32(b + 0x498) = -GT4_F32(p + 0x54);
    GT4_F32(b + 0x49c) = GT4_F32(p + 0x54);
    GT4_U32(b + 0x57c) = 0;
    GT4_U32(b + 0x484) = 0;
    GT4_U32(b + 0x4a0) = 0;
    GT4_U32(b + 0x46c) = 0;
    GT4_U32(b + 0x59c) = 0;
    GT4_U32(b + 0x5a0) = 0;
    GT4_U32(b + 0x544) = 0;
    GT4_U32(b + 0x548) = 0;
    GT4_U32(b + 0x54c) = 0;
    GT4_U32(b + 0x45c) = 0;
    GT4_U8(b + 0x460) = 0;
    GT4_U32(b + 0x488) = 0;
    GT4_U32(b + 0x494) = 0;
    GT4_U32(b + 0x48c) = 0;
    for (uint32_t i = 0; i < GT4_U8(lay + LAYOUT_WHEELS); i++) GT4_U32(car + 0x1c8 + i * 0xec) = 0;
    GT4_U32(b + 0x508) = 0;
    GT4_U8(b + 0x461) = 0;
    GT4_U16(b + 0x4f6) = 0;
    GT4_U32(b + 0x500) = 0;
    GT4_U32(b + 0x504) = 0;
    GT4_U32(b + 0x4e8) = 0;
    GT4_U32(b + 0x4ec) = 0;
    GT4_U32(b + 0x4f0) = 0;
    GT4_U16(b + 0x4f4) = 0;
    GT4_U16(b + 0x4f8) = 0;
    GT4_U8(b + 0x607) = 0;
    GT4_U8(b + 0x521) = 0;
    GT4_U32(car + 0x6cc) = 0;
    set_speed(car, 0);
    set_gear(car, 1);
    GT4_U16(b + 0x4fa) = 0;
    GT4_U8(b + 0x5f8) = 0;
    GT4_U8(b + 0x5f9) = 0;
    GT4_U32(b + 0x610) = 0;
    {                                                       /* func_00353EB8 / func_00353EA0 */
        uint32_t c = GT4_U32(car + 4) + GT4_U8(car + 0x544) * 16u + 0xca58u;
        GT4_U8(c + 3) = 0;
        GT4_U8(c + 2) = 0x80;
        GT4_U8(c + 1) = 0;
        GT4_U8(c) = 0;
    }
    GT4_U16(b + 0x454) = 0;
    GT4_U16(b + 0x456) = 0;
    GT4_U16(b + 0x458) = 0;
    GT4_U16(b + 0x45a) = 0;
    GT4_U8(b + 0x633) = 30;                                 /* func_00353ED8 */
    GT4_U8(b + 0x605) = 0x7f;
    GT4_U8(b + 0x604) = 0x7f;
    GT4_U8(b + 0x606) = 0;
    wheels_clear(car);
    GT4_U32(b + 0x53c) = 0;
    GT4_U32(b + 0x584) = 0;
    GT4_U32(b + 0x540) = 0;
    GT4_U8(b + 0x522) = 0;
    GT4_U32(b + 0x514) = 0;
    GT4_U32(b + 0x4fc) = 0;
    GT4_U32(b + 0x580) = 0;
    GT4_U8(b + 0x4b5) = 0;
    GT4_U8(b + 0x526) = 0;
    if ((int8_t)GT4_U8(b + 0x466) == 1 || GT4_U8(b + 0x467) || GT4_U8(b + 0x442) == 0 || GT4_U8(b + 0x442) == 2)
        GT4_U8(b + 0x526) = 12;
    GT4_U32(b + 0x660) = 0;
    GT4_U16(b + 0x52a) = 0;
    GT4_U8(b + 0x527) = 0;
    GT4_U8(b + 0x528) = 0;
    GT4_U8(b + 0x529) = 0;
    GT4_U32(b + 0x5a8) = 0;
    GT4_U32(b + 0x5ac) = 0;
    GT4_U32(b + 0x550) = 0;
    GT4_U32(b + 0x554) = 0;
    GT4_U32(b + 0x558) = 0;
    GT4_U32(b + 0x58c) = 0;
    GT4_U32(b + 0x588) = 0;
    GT4_U32(b + 0x564) = 0;
    GT4_U32(b + 0x5bc) = 0;
    GT4_U32(b + 0x470) = 0;
    GT4_U32(b + 0x478) = 0;
    GT4_U32(b + 0x474) = 0;
    GT4_U32(b + 0x47c) = 0;
    GT4_U32(b + 0x65c) = 0;
    memset(gt4_ptr(car + 0x774), 0, 4);                     /* func_0035A638 */
    memset(gt4_ptr(car + 0x778), 0, 16);
}

/* func_00363868: the car rolling at speed v (m/s, capped at the top
   speed): body and wheels, gear for the speed, engine rpm, turbo. */
static void car_rolling(uint32_t car, float v)
{
    uint32_t b = car + 0x104, p = GT4_U32(car + CAR_PARAMS), d = p + PARAMS_DRIVE, lay = layout_of(p);
    float lo, hi;
    int32_t rpm;
    if (GT4_F32(p + 0xfd8) < v) v = GT4_F32(p + 0xfd8);
    GT4_U32(b + 0x57c) = 0;
    for (uint32_t i = 0; i < 3; i++) GT4_F32(car + 0x648 + i * 4) = GT4_F32(car + 0x110 + i * 4) * v;
    set_speed(car, v);
    set_gear(car, (uint32_t)gt4_gear_for_speed(car, v));
    GT4_F32(b + 0x518) = 1.0f;
    GT4_F32(b + 0x4fc) = v * GT4_F32(p + 0x29c + GT4_U8(b + 0x520) * 4u);
    gt4_converter(d, (float *)gt4_ptr(b + 0x4fc), (float *)gt4_ptr(b + 0x518));
    lo = (float)(int32_t)GT4_U16(p + 0x202) * K(0x3dd6774eu);
    if (GT4_F32(b + 0x4fc) < lo) GT4_F32(b + 0x4fc) = lo;
    else {
        hi = (float)(int32_t)GT4_U16(p + 0x200) * K(0x3dd6774eu);
        if (hi < GT4_F32(b + 0x4fc)) GT4_F32(b + 0x4fc) = hi;
    }
    rpm = (int32_t)(GT4_F32(b + 0x4fc) * 60.0f / K(0x40c90fdau));
    GT4_U16(b + 0x4fa) = (uint16_t)rpm;
    if (GT4_F32(p + 0x230) != 0) {
        GT4_U16(b + 0x4f4) = (uint16_t)rpm;
        if (0 < GT4_F32(p + 0x21c)) GT4_U16(b + 0x4f6) = (uint16_t)rpm;
        GT4_F32(b + 0x508) = gt4_engine_boost(p + PARAMS_ENGINE, (int16_t)rpm, (int16_t)rpm);
        GT4_F32(b + 0x4f0) = 1.0f;
        GT4_U16(b + 0x4f8) = GT4_U16(b + 0x4fa);
        GT4_U32(b + 0x504) = 0;
        GT4_F32(b + 0x500) = 1.0f;
        GT4_U8(b + 0x607) = 0;
    }
    if (!GT4_U8(car + 0x14 + 0x1e)) {                      /* func_00463B98 */
        GT4_F32(b + 0x494) = 1.0f;
        GT4_F32(b + 0x488) = 1.0f;
    }
    GT4_U8(b + 0x522) = 1;
    GT4_F32(b + 0x514) = 1.0f;
    GT4_F32(b + 0x53c) = v;
    GT4_U32(b + 0x540) = 0;
    for (uint32_t i = 0; i < GT4_U8(lay + LAYOUT_WHEELS); i++) {
        GT4_F32(car + 0x164 + i * 0xec + 0x20) = v;
        GT4_F32(car + 0x164 + i * 0xec + 0x28) = v;
    }
    for (uint32_t k = 0; k < 2; k++) {
        float r = GT4_F32(p + 0x2cc + k * 4);
        if (GT4_U8(d + 1)) r = GT4_F32(p + 0x23c + 0x90 + GT4_U8(d + 3) * 4u);
        GT4_F32(car + 0x630 + k * 4) = r * v;
    }
}

/* func_003683C8 / func_00368260: a CVT (drivetrain kind 3) starts at its
   lowest ratio; the first such car also fills the gearbox's rpm and
   torque tables (D_00844068, D_00844098; count at D_00620CA0). */
static void cvt_init(uint32_t car)
{
    uint32_t p = GT4_U32(car + CAR_PARAMS), c = car + 0x788;
    int32_t n;
    if (GT4_U8(p + PARAMS_DRIVE + 0x12) != 3) return;
    GT4_F32(c + 4) = 1.0f;
    GT4_U8(c + 9) = 0;
    GT4_U32(c) = 0;
    GT4_U8(c + 8) = 0;
    if (GT4_U32(0x00620cacu) || GT4_U8(p + 0x247) != 3) return;
    n = (int16_t)GT4_U16(p + 0x134) + 1;
    if (n >= 13) n = 12;
    GT4_U16(0x00620ca0u) = (uint16_t)n;
    GT4_U32(0x00844068u) = 0;
    GT4_U32(0x00844098u) = 0;
    for (int32_t i = 1; i < n; i++) {
        float rpm = GT4_F32(p + 0x140 + (uint32_t)(i - 1) * 4), t = GT4_F32(p + 0x1a0 + (uint32_t)(i - 1) * 4);
        float w = rpm * 60.0f;
        t = t / K(0x411cccccu);
        GT4_F32(0x00844098u + (uint32_t)i * 4) = rpm;
        w = w / K(0x40c90fdau);
        t = t * w;
        t = t / K(0x44330cccu);
        GT4_F32(0x00844068u + (uint32_t)i * 4) = t * K(0x3f3c49bau);
    }
    GT4_U32(0x00620cacu) = 1;
}

/* func_003F1BD8 with 2 / func_003F1890: the driver's aid levels from the
   game options (D_00622080 for a player on the pad, D_00622081 else; the
   tables D_006220A8/B0/C8 by level), +0x6E cleared in sessions where
   virtual +0x30 says so, +0x6F = 0x80 while car+0x5D0 is not positive. */
static void driver_aids(uint32_t cond, uint32_t idx)
{
    uint32_t car = cond + 0x6698u + idx * CAR_SIZE, m = car + 0x14, src = GT4_U8(car + 0x56a) < 2 ? 0x00622080u : 0x00622081u;
    uint32_t lv = GT4_U8(src);
    GT4_U8(m + 0x6d) = (uint8_t)lv;
    GT4_F32(m + 0x74) = GT4_F32(0x006220c8u + lv * 4);
    GT4_F32(m + 0x70) = GT4_F32(0x006220b0u + lv * 4);
    GT4_U8(m + 0x6e) = src == 0x00622081u;
    GT4_U8(m + 0x6f) = GT4_U8(0x006220a8u + lv);
    if (session_call(cond, 0x30, 0, NULL)) GT4_U8(m + 0x6e) = 0;
    if (!(0 < GT4_F32(car + 0x5d0))) GT4_U8(m + 0x6f) = 0x80;   /* func_00354E38 */
    GT4_U8(m + 0x6c) = 0;
}

/* func_0034C5E0: a rolling start `start` metres along the course (kept
   20 m inside a lap; with the session's rolling line, before its end) on
   line `line` (-1: 6). 0 when there is none, else the sign of start. */
static int rolling_start(uint32_t cond, uint32_t ground, uint32_t px, uint32_t py, uint32_t pz, uint32_t heading,
                         int32_t start, uint32_t line)
{
    float d = (float)start, lim = course_length(cond) - 20.0f;
    int flag = GT4_U8(cond + 0xcbdeu) == 1;
    if (d < -lim) d = -lim;
    if (flag) {
        int32_t n = (int32_t)session_call(cond, 0x150, 0, NULL);
        session_call(cond, 0x158, (uint32_t)(n - 2), &lim);
        lim = lim - 20.0f;
    }
    if (lim < d) d = lim;
    if (!gt4_line_rolling_start(cond + 0xda50u, line != 0xffffffffu ? line : 6, ground, px, py, pz, heading, flag, d))
        return 0;
    return start < 0 ? -1 : 1;
}

/* func_003630C0: the car placed at (x, y) on the ground below height h,
   facing heading: body at rest on its wheels, each wheel's ground, then
   the attitude of the ground under it. id: the ground id to start from.
   When there is no ground under (x, y) the original goes on with a stale
   word of its stack as the height (after a rolling start, a register the
   line search saved: -1, a huge negative number to the PS2 FPU); here it
   is -FLT_MAX, so the car ends far below the course either way. */
void gt4_car_place(uint32_t car, uint32_t id, float x, float y, float h, float heading)
{
    uint32_t b = car + 0x104, p = GT4_U32(car + CAR_PARAMS), course = GT4_U32(car + CAR_COURSE);
    uint32_t lay = GT4_LAYOUTS + GT4_U8(p + PARAMS_LAYOUT) * 84u, cond;
    uint32_t marked = 0, off = 0;
    float ground = K(0xff7fffffu);
    {                                                       /* func_00395760 */
        gt4_ray_hit hit;
        float q[3] = { x, h + 5.0f, -y };
        if (gt4_course_ray_down(GT4_U32(GT4_U32(course + 4) + 0xdc), &hit, q)) {
            ground = hit.pos[1];
            id = (uint32_t)(uint16_t)hit.tri | (uint32_t)(int32_t)hit.leaf << 16;
        }
    }
    GT4_F32(b) = x;
    GT4_U32(b + 0x4a8) = id;
    GT4_F32(b + 4) = y;
    GT4_U32(b + 0x568) = 0;
    GT4_U32(b + 0x56c) = 0;
    GT4_F32(b + 0x570) = heading;
    GT4_F32(b + 8) = ground + GT4_F32(p + 0x3c);
    gt4_rotation_from_angles(gt4_ptr(car + 0x110), gt4_ptr(car + 0x11c), gt4_ptr(car + 0x128), 0, 0, heading);
    gt4_car_mounts(car);
    for (uint32_t i = 0; i < GT4_U8(lay + LAYOUT_WHEELS); i++) {
        uint32_t w = car + 0x164 + i * 0xec, g = car + CAR_GROUND + i * GROUND_SIZE;
        GT4_F32(g + 4) = GT4_F32(b + 8) + GT4_F32(w + 0x14);
        GT4_F32(g + 8) = -(GT4_F32(b + 4) + GT4_F32(w + 0x10));
        GT4_F32(g) = GT4_F32(b) + GT4_F32(w + 0xc);
        GT4_U32(g + 0xc) = GT4_U32(b + 0x4a8);
        gt4_ground_query(course, g);
        if (!GT4_U8(g + GROUND_MISS)) {
            GT4_F32(w + 0x8c) = GT4_F32(g + GROUND_HIT_Y);
            GT4_U8(w + 0x89) = gt4_ground_surface(g);
            GT4_U8(w + 0x8a) = GT4_U8(g + 0x32);
            GT4_U8(w + 0x8b) = GT4_U8(g + 0x33);
            marked += GT4_U8(g + 0x33) & 1;
            if (gt4_ground_off_road(g)) off++;
        } else {
            GT4_U8(w + 0x89) = 0;
            GT4_U8(w + 0x8a) = 0;
            GT4_U8(w + 0x8b) = 0;
            GT4_F32(w + 0x8c) = ground + GT4_F32(w + 0x14);
        }
    }
    cond = GT4_U32(car + 4);
    GT4_U8(b + 0x4b1) &= 0xef;
    if (marked && session_call(cond, 0x180, cond, NULL)) GT4_U8(b + 0x4b1) |= 0x10;
    GT4_U8(b + 0x62c) = (uint8_t)off;
    GT4_U8(b + 0x62d) = (uint8_t)marked;
    for (uint32_t i = 0; i < GT4_U8(lay + LAYOUT_WHEELS); i++) {
        uint32_t w = car + 0x164 + i * 0xec, axle = GT4_U8(lay + 0x14 + i);
        float f = GT4_F32(p + 0x1040 + axle * 4) / (float)(int32_t)GT4_U8(lay + 4 + axle);
        f = f + GT4_F32(p + 0x14 + 0x330 + axle * 0x58);
        GT4_U8(w + 0xb3) = 0;
        GT4_F32(w + 0x4c) = f;
        GT4_F32(w + 0x48) = f;
        GT4_F32(w + 0x94) = GT4_F32(p + 8 + 0x310 + axle * 0x58);
        GT4_U32(w + 0x1c) = 0;
        GT4_F32(w) = gt4_wheel_angle(p, (int)i, 0);
        GT4_U32(w + 0x18) = 0;
        if (GT4_U32(0x00845bf0u) == 2) {                    /* func_003F7C80 */
            GT4_U32(w + 0x18) = 0;
            GT4_F32(w + 0x1c) = GT4_F32(w);
        }
        GT4_F32(w + 4) = gt4_wheel_camber(p, (int)i, 0, 0);
        GT4_F32(w + 8) = gt4_wheel_contact_y(p, (int)i, 0, 0);
    }
    gt4_car_ground_frame(car);
    GT4_F32(b + 0x568) = GT4_F32(b + 0x574);
    GT4_F32(b + 0x56c) = GT4_F32(b + 0x578);
    GT4_F32(b + 0x568) = GT4_F32(b + 0x568) + gt4_atan2f(GT4_F32(p + 0x40) - GT4_F32(p + 0x44), GT4_F32(p + 0x18));
    gt4_rotation_from_angles(gt4_ptr(b + 0xc), gt4_ptr(b + 0x18), gt4_ptr(b + 0x24),
                             GT4_F32(b + 0x568), GT4_F32(b + 0x56c), GT4_F32(b + 0x570));
    GT4_F32(b + 8) = GT4_F32(b + 0x38) + GT4_F32(b + 0x2c) * GT4_F32(p + 0x3c);
    gt4_car_mounts(car);
    if (GT4_U8(cond + 0xcbdeu) == 2) GT4_U32(b + 0x4a4) = 0;
    else session_call(cond, 0x108, car, (float *)gt4_ptr(b + 0x4a4));
    GT4_U32(b + 0x628) = GT4_U32(b + 0x4a8);
    GT4_F32(b + 0x624) = GT4_F32(b + 0x38);
    GT4_F32(b + 0x61c) = GT4_F32(b + 0x30);
    GT4_F32(b + 0x620) = GT4_F32(b + 0x34);
    GT4_U32(b + 0x650) = 0;
}

/* func_00363B10. car: the car; rec: its record; ground: ground id of the
   start (-1); a3, t0, t1: from the session (+0xE8); idx: the car; kind:
   how it starts (0 grid, 1..5 rolling, 6/7 start line, 8...); pos: x, y,
   z, heading; start: the start distance; speed: km/h (sp+8); wear: its
   entry's engine state. buf: 0x60 bytes of guest memory. Returns what the
   parameters' build returns. */
uint32_t gt4_car_init(uint32_t car, uint32_t rec, uint32_t ground, uint32_t a3, uint32_t t0, uint32_t t1,
                      uint32_t idx, uint32_t kind, const float pos[4], int32_t start, int32_t speed,
                      uint32_t wear, uint32_t buf)
{
    uint32_t b = car + 0x104, cond = GT4_U32(car + 4), params = GT4_U32(car + CAR_PARAMS), race = GT4_U32(car);
    uint32_t entry, s0 = t1, s2 = a3, s5, s6, laps = 0, fp = 0xffffffffu, s7, ret, start_ok = 0;
    uint32_t start_flag = 0;
    int32_t s4 = start;
    uint32_t sp[5];
    uint8_t *aid;

    sp[0] = ground;
    memcpy(&sp[1], pos, 16);
    if (session_laps(cond) && s4 > 0) {
        start_flag = 1;
        if (!(s4 < session_laps(cond))) start_flag = 2;
    }
    if (!wear) {                                           /* func_003433E8: no wear */
        memset(gt4_ptr(car + 0x6dc), 0, 0x20);
        GT4_U8(car + 0x6dc + 0x14) = 1;
    } else {                                               /* func_0035DCA8: the entry's */
        memcpy(gt4_ptr(car + 0x6dc), gt4_ptr(wear), 0x20);
        GT4_U32(car + 0x6d8) = 0;
    }
    GT4_U32(car + 8) = GT4_U32(race + 0x80);
    entry = entry_of(race, idx);
    GT4_U32(car + 0xc) = GT4_U32(entry + 0x343c);
    {
        int32_t k = (int32_t)GT4_U32(entry + 0x344c), a = 1;
        if (k == 1) a = 2;
        else if (k == 2) a = 3;
        GT4_U8(car + 0x547) = (uint8_t)a;                   /* func_003487F0 */
    }
    GT4_U8(b + 0x441) = (uint8_t)s0;
    GT4_U8(b + 0x440) = (uint8_t)idx;
    GT4_U8(b + 0x466) = (uint8_t)s2;                        /* func_00362E00 */
    GT4_U8(b + 0x467) = 0;
    s5 = GT4_U32(entry + 0x3458);
    s2 = GT4_U32(entry + 0x3450);
    s0 = GT4_U32(entry + 0x3454);
    if (mode_class(cond)) { s2 = 0; s0 = 0; }
    if (!s2)                                                /* func_003F1700: stability aids off */
        GT4_U8(rec + 0x1b8) = GT4_U8(0x006224a8u) ? (uint8_t)(GT4_U8(rec + 0x1b8) & 0xc0) : 0;
    if (!s0) GT4_U8(rec + 0x1c2) = 0;                       /* func_003F1748: traction control off */
    {
        uint8_t f = GT4_U8(rec + 0x1b8);                    /* func_003F1728, func_003F1750 */
        if (GT4_U8(0x006224a8u)) f &= 0x3f;
        if (!f) s2 = 0;
        if (!GT4_U8(rec + 0x1c2)) s0 = 0;
    }
    /* which aids the driver may use (car+0x5E9) */
    aid = gt4_ptr(b + 0x4e5);
    *aid = 0;
    if (GT4_U8(b + 0x466) < 2 || (int8_t)GT4_U8(b + 0x466) == 3) {
        uint8_t c = GT4_U8(GAME_SETTINGS + 0x131);
        if (s0) *aid = 1;
        if (s2) *aid |= 4;
        if ((c == 1 && s0) || (c == 2 && s2) || c == 3) *aid |= 2;
        if (s5) *aid |= 8;
        c = GT4_U8(GAME_SETTINGS + 0x133);
        if ((c == 1 && s2) || c == 2) *aid |= 0x10;
    }
    /* the parameters, with the tyre grip of the surface kind the game
       uses for its setting 2 */
    {
        uint32_t k = GT4_U8(rec + 0x1ff), f = GT4_U8(rec + 0x1f5);
        uint32_t front = rec + 0x279 + k * 0xa8, rear = rec + 0x279 + (k + 3) * 0xa8;
        uint8_t kf = GT4_U8(front), kr = GT4_U8(rear);
        int two = mode_class(cond) == 2;
        if (two) {
            GT4_U8(rec + 0x1f5) = (uint8_t)k;
            GT4_U8(front) = GT4_U8(rec + 0x283 + k * 0xa8);
            GT4_U8(rear) = GT4_U8(rec + 0x283 + (k + 3) * 0xa8);
        }
        gt4_orig_t0 = cond + 0xf818u;
        gt4_orig_t1 = wear;
        gt4_car_params(params, rec, (int8_t)GT4_U8(b + 0x466), 0, cond + 0xf818u, wear, buf);   /* func_00361908 */
        ret = 0;
        if (mode_class(cond) == 2) {
            GT4_U8(rec + 0x1f5) = (uint8_t)f;
            GT4_U8(front) = kf;
            GT4_U8(rear) = kr;
        }
    }
    {
        uint32_t i = GT4_U8(b + 0x440);
        GT4_U32(cond + 0x10024u + i * 4) = i * 0xd4 + cond + 0xfb2cu;
    }
    gt4_ai_settings_load(0x00623e50u, 0x00623fb8u);        /* func_00469440 */
    s6 = car + 0x14;
    gt4_ai_profile_init(s6, GT4_U8(params + 0x31), race_mode(cond));   /* func_00466C20 */
    if (session_call(cond, 0x1a0, cond, NULL)) {            /* func_00469468 */
        GT4_U8(0x00623e50u + 0xb) = GT4_U8(0x00623fb8u + 0x3f);
        GT4_U8(0x00623e50u + 0xa) = GT4_U8(0x00623fb8u + 0x38);
    }
    if ((int8_t)GT4_U8(b + 0x466) == 2) gt4_ai_handicap(s6, GT4_U8(params + 0x31));   /* func_00467148 */
    {                                                       /* func_004673D8, func_00463BB8 */
        uint32_t prof = GT4_U32(GT4_U32(s6 + 0xe4) + GT4_U8(GT4_U32(s6 + 0xe8) + CAR_PLAYER) * 4u + 0x10024u);
        GT4_F32(prof + 0x70) = (GT4_F32(params + 0x106c) + GT4_F32(params + 0x1070)) / GT4_F32(params + 0x103c);
        GT4_U32(prof + 0x74) = 0;
        if (GT4_U8(params + 0x1077) == 7)
            GT4_F32(prof + 0x74) = GT4_F32(params + 0x1084) * K(0x3f4cccccu) / GT4_F32(params + 0x103c);
    }
    s5 = 0;
    {                                                       /* func_00362E10 */
        int8_t k = (int8_t)GT4_U8(b + 0x466);
        if (k == 2) GT4_U8(b + 0x442) = 0;
        else if ((k < 3 && k >= 0) || k == 4) GT4_U8(b + 0x442) = (uint8_t)t0;
    }
    GT4_U8(b + 0x4b1) = 0;
    GT4_U8(b + 0x5c2) = 0;
    GT4_U8(b + 0x64f) = 0xff;
    GT4_U8(b + 0x5c1) = 0;
    GT4_U16(b + 0x4b8) = 0;
    GT4_U8(b + 0x64e) = 0xff;
    GT4_U16(b + 0x64c) = 0;
    GT4_U8(b + 0x62c) = 0;
    GT4_U8(b + 0x62d) = 0;
    /* the start distance within one lap, and the laps it spans */
    {
        int32_t len = (int32_t)course_length(cond);
        if (s4 > 0) {
            laps = 1;
            while (!(s4 < len)) { s4 -= len; laps++; }
        } else if (s4 < 0) {
            while (!(-len < s4)) s4 += len;
        }
    }
    s5 = laps;
    s0 = 0;
    if (GT4_U8(cond + 0xcbdeu) != 2) {
        uint32_t g = grid_slot(GT4_U32(race + 0x80), GT4_U32(entry + 0x3444));
        fp = (uint32_t)gt4_line_nearest(cond + 0xda50u, GT4_F32(g), -GT4_F32(g + 8), 0);
    }
    /* where it starts */
    if (kind - 1 < 5) {
        uint32_t t3 = GT4_U8(cond + 0xcbdeu) == 1 ? fp : 0xffffffffu;
        int32_t r;
        memcpy(gt4_ptr(buf), sp, 20);
        r = rolling_start(cond, buf, buf + 4, buf + 8, buf + 0xc, buf + 0x10, s4, t3);
        memcpy(sp, gt4_ptr(buf), 20);
        if (r == 0) { s7 = (uint32_t)-6; kind = 0; }
        else if (r < 0) { s0 = 1; s7 = kind - 6; }
        else s7 = kind - 6;
        start_ok = 1;
    } else {
        s7 = kind - 6;
        if (s7 < 2) {
            gt4_line_start(cond + 0xda50u, (int)idx, buf + 0x30, buf + 0x40, buf);
            memcpy(&sp[1], gt4_ptr(buf + 0x30), 12);
            memcpy(&sp[4], gt4_ptr(buf + 0x48), 4);
            start_ok = 1;
        }
    }
    (void)start_ok;
    if (kind == 0 || kind == 8) {
        float back = GT4_F32(params), h, x, y;
        memcpy(&h, &sp[4], 4);
        memcpy(&x, &sp[1], 4);
        memcpy(&y, &sp[2], 4);
        x = x + back * gt4_cosf(h);
        y = y - back * gt4_sinf(h);
        memcpy(&sp[1], &x, 4);
        memcpy(&sp[2], &y, 4);
    }
    GT4_U8(b + 0x65b) = 0;
    if (start_flag) GT4_U8(b + 0x65b) = (uint8_t)(kind == 2 ? 1 : start_flag);
    {
        float p[4];
        memcpy(p, &sp[1], 16);
        gt4_car_place(car, sp[0], p[0], p[1], p[2], p[3]);
    }
    {                                                       /* func_003455F0 */
        uint32_t v = cond + GT4_U8(b + 0x440) * 48u + 0xcab8u;
        GT4_U32(b + 0x5fc) = 0;
        GT4_U32(b + 0x600) = 0;
        GT4_F32(v) = GT4_F32(b + 0x570);
        GT4_U32(v + 8) = 0;
        GT4_U32(v + 4) = 0;
        GT4_U32(v + 0x2c) = 0;
        GT4_F32(v + 0xc) = GT4_F32(b + 0x568);
        for (uint32_t o = 0x10; o <= 0x28; o += 4) GT4_U32(v + o) = 0;
    }
    gt4_car_rest(car);
    {                                                       /* func_00362FA8 */
        uint32_t lay = GT4_LAYOUTS + GT4_U8(params + PARAMS_LAYOUT) * 84u;
        for (uint32_t i = 0; i < GT4_U8(lay + LAYOUT_WHEELS); i++) gt4_wheel_fresh(car, (int)i);
    }
    GT4_U8(b + 0x4b6) = 0;                                  /* func_00363528 */
    GT4_U8(b + 0x4b3) = 0;
    GT4_U16(b + 0x60e) = 0;                                 /* func_0035FC48 */
    GT4_U8(b + 0x62e) = 0;
    GT4_U8(b + 0x5c3) = 0;
    GT4_U8(b + 0x62f) = 0;
    GT4_U8(b + 0x630) = 0;
    {                                                       /* func_00354AF8: fuel */
        uint32_t v = GT4_U8(cond + 0xf818u + 0x4c) ? GT4_U8(params + 0x104b) : 0;
        float f = (float)(int32_t)v;
        GT4_U8(b + 0x4bd) = 1;
        GT4_U32(b + 0x4c0) = 0;
        GT4_U16(b + 0x4be) = 0;
        GT4_U32(b + 0x4c4) = 0;
        GT4_U32(b + 0x4c8) = 0;
        GT4_F32(b + 0x4d0) = f;
        GT4_F32(b + 0x4cc) = f;
        if (v) {
            float g = f - (float)(int32_t)GT4_U8(0x00620e24u);
            GT4_F32(b + 0x4d0) = g;
            if (g < GT4_F32(b + 0x4c0)) GT4_F32(b + 0x4d0) = GT4_F32(b + 0x4c0);
        }
        {                                                   /* func_00354AC0 */
            float f2 = GT4_F32(b + 0x4d0) * K(0x3f3d70a3u);
            GT4_F32(b + 0x4d4) = GT4_F32(params + 0x48) + f2;
            GT4_F32(b + 0x4d8) = GT4_F32(params + 0x4c) + f2;
        }
    }
    GT4_U8(b + 0x631) = 0;
    if (0 < GT4_F32(params + 0x10d4)) GT4_F32(b + 0x4e0) = GT4_F32(params + 0x10d8);
    else GT4_U32(b + 0x4e0) = 0;
    GT4_U8(b + 0x4e4) = 0;
    GT4_U32(b + 0x4dc) = 0;
    GT4_U8(car + 0x6c4) = 0;                                /* func_003F3360 */
    if (GT4_U8(params + 0x32) == 3) {                       /* func_00355840 */
        GT4_U32(car + 0x78c) = 0;
        GT4_U32(car + 0x788) = 0;
    }
    if (GT4_U8(params + PARAMS_DRIVE) == 7) {               /* func_00355DC8 */
        GT4_F32(car + 0x788) = 1.0f * GT4_F32(0x008438dcu);
        GT4_U8(car + 0x78d) = 0;
        GT4_U8(car + 0x78c) = 0;
    }
    if (GT4_U8(params + 0x32) == 1) GT4_U8(car + 0x788) = 0; /* func_0035D108 */
    GT4_F32(b + 0x448) = GT4_F32(b + 0x444) * K(0x3c888888u); /* func_003658A0 */
    GT4_F32(b + 0x44c) = 60.0f / GT4_F32(b + 0x444);
    GT4_U8(b + 0x4b0) = 0;
    GT4_U8(b + 0x4b4) = 0;
    /* the lap sector it starts in (car+0x5B2) and the lap count (+0x5B0) */
    if (s0) {
        GT4_U8(b + 0x4ae) = 0;
        GT4_U16(b + 0x4ac) = 0;
    } else {
        uint32_t n = session_call(cond, 0x150, cond, NULL) - 1, i;
        float d = GT4_F32(b + 0x4a4), f;
        GT4_U8(b + 0x4ae) = (uint8_t)n;
        for (i = 0; (int32_t)i < (int32_t)n; i++) {
            session_call(cond, 0x158, i, &f);
            if (d < f) { GT4_U8(b + 0x4ae) = (uint8_t)i; break; }
        }
        if (kind == 6) {
            if (course_length(cond) * 0.5f < d) GT4_U8(b + 0x4b0) = 1;
            GT4_U8(b + 0x4ae) = 0;
            GT4_U16(b + 0x4ac) = 0;
        } else if ((int32_t)s5 > 0) {
            GT4_U16(b + 0x4ac) = (uint16_t)s5;
        } else if (GT4_U8(b + 0x4ae) != (uint8_t)n) {
            GT4_U16(b + 0x4ac) = 1;
        } else {
            int one = 0;
            if (session_call(cond, 0x138, 0, &f), !(f < 1.0f)      /* func_0034C470 */
                && (session_call(cond, 0x138, 0, &f), !(course_length(cond) - 1.0f < f))) {
                session_call(cond, 0x138, cond, &f);
                if (d < f) one = 1;
            }
            if (!one && d < course_length(cond) * 0.5f) one = 1;
            GT4_U8(b + 0x4ae) = 0;
            GT4_U16(b + 0x4ac) = (uint16_t)one;
        }
    }
    GT4_U32(b + 0x614) = 0;
    s0 = 0xffffffffu;
    s4 = (int32_t)kind - 2;
    GT4_U32(car + 0x71c) = 0x157529ffu;                     /* func_0035E3B8 */
    GT4_U8(b + 0x4bc) = 0;
    GT4_U16(b + 0x4ba) = 0;
    GT4_U8(b + 0x4b2) = 0;
    GT4_U8(b + 0x632) = 0;
    GT4_U8(b + 0x5fa) = 0;
    GT4_U8(b + 0x5fb) = 0;
    GT4_U8(b + 0x4b7) = (uint8_t)(GT4_U16(cond + 0xda50u + 0xc) ? 8 - idx : 0);   /* func_0034C760, func_0046BDA0 */
    GT4_U8(b + 0x4af) = (uint8_t)(idx + 1);
    /* the driver model's side of the road at the start */
    if ((uint32_t)s4 < 4) s0 = 6;
    else if (kind == 8) {}
    else if (GT4_U8(cond + 0xcbdeu) == 1) s0 = fp;
    else if (!(int8_t)GT4_U8(b + 0x466)) s0 = 7;
    else {                                                  /* func_00362DF8 is always 1 */
        uint32_t course = GT4_U32(race + 0x80), g0 = grid_slot(course, 0), g1 = grid_slot(course, 1);
        int32_t side = (int32_t)GT4_U32(entry + 0x3444) % 6;
        float dx = GT4_F32(g0) - GT4_F32(g1), dz = GT4_F32(g0 + 8) - GT4_F32(g1 + 8), h = GT4_F32(g0 + 0xc) + K(0x40490fdau);
        float c = -gt4_cosf(h), s = gt4_sinf(h), v;
        dz = -dz;
        v = dx * s - c * dz;
        s0 = GT4_U32(SIDES + (uint32_t)side * 4);
        if (!(v < 0)) {
            if (s0 == 2) s0 = 1;
            else if (s0 == 1) s0 = 2;
        }
    }
    driver_aids(cond, idx);
    gt4_ai_start(s6, s0);
    gt4_ai_gear(s6);
    if (s7 < 2) {
        gt4_car_pit_box(car);                               /* func_00345400 */
        GT4_U16(b + 0x4ba) = 0x78;
    }
    GT4_U16(b + 0x590) = 0;
    if ((uint32_t)s4 < 3 || kind == 8) {
        gt4_car_take_back(car);                             /* func_003452F0 */
        GT4_U8(b + 0x526) = 12;
        if (kind == 3) GT4_U16(b + 0x590) = 180;
        else if (kind == 8) GT4_U16(b + 0x590) = 300;
    }
    {
        float v = kind == 4 ? GT4_F32(s6 + 0x28) : (float)speed * K(0x3e8e38e3u);
        if (v != 0) car_rolling(car, v);
    }
    {                                                       /* func_00463AE0 */
        uint32_t m = car + 0x14, keep = 0;
        if ((uint8_t)(GT4_U8(b + 0x466) - 1) < 2 && !mode_class(cond) && !(K(0x41055554u) <= GT4_F32(b + 0x53c)))
            keep = GT4_U8(b + 0x467) ? 0 : kind == 0;
        if (!keep) GT4_U8(m + 0x1a) = 1;
        if (!GT4_U8(m + 0x1a)) GT4_U8(m + 0x1a) = GT4_U8(cond + 0xcbe9u);
    }
    GT4_U32(b + 0x648) = 0;                                 /* func_003F9CF0 */
    GT4_F32(b + 0x644) = GT4_F32(b + 0x494);
    {                                                       /* func_003F27E8: active aero */
        uint8_t k = GT4_U8(params + PARAMS_AERO_KIND);
        float v = GT4_F32(b + 0x53c);
        GT4_U32(b + 0x594) = 0;
        GT4_U32(b + 0x598) = 0;
        GT4_U8(b + 0x592) = 0;
        GT4_U8(b + 0x593) = 0;
        if (k == 2) {                                       /* func_003F2798 */
            float lo = GT4_F32(params + 0x1078), hi = GT4_F32(params + 0x107c), f = 0;
            if (!(v <= lo)) f = hi <= v ? 1.0f : (v - lo) / (hi - lo);
            GT4_F32(b + 0x594) = f;
        } else if ((k == 1 || k == 6) && GT4_F32(params + 0x107c) <= v) {
            GT4_U8(b + 0x592) = 1;
            GT4_F32(b + 0x594) = k == 1 ? 1.0f : K(0x3e1d89d8u);
        }
    }
    if (GT4_U8(params + 0x32) == 1) {                       /* func_0035C7E0 */
        GT4_U8(car + 0x78e) = 0;
        GT4_U8(car + 0x78a) = 0x3c;
        GT4_U8(car + 0x789) = 0;
        GT4_U8(car + 0x78b) = 0;
        GT4_U8(car + 0x78c) = 0;
        GT4_U8(car + 0x78d) = 0;
    }
    cvt_init(car);
    GT4_U32(b + 0x638) = 0;                                 /* func_00350EB0 */
    GT4_F32(b + 0x634) = (float)(int16_t)GT4_U16(b + 0x4fa);
    GT4_U32(b + 0x654) = 0;                                 /* func_00366388 */
    GT4_U8(b + 0x65a) = 0;
    GT4_U16(b + 0x658) = 0;
    return ret;
}

/* func_0034C7E0: the course's settings from its COURSE row (the key at
   race+0x78): +0xCBDD race mode (row 0x10), +0xCBDE (0x12), +0xCBDF and
   +0xCBE1 (0x13, and whether it is 2 or more), +0xCBE0 (0x14 set), and the
   course code (UnistrDB string of row 0x08) at +0xCBE6: its first and
   fourth characters as decimal digits, the second and third as hex digits,
   the rest as they are. Without a row the fields keep what they had. */
static void course_settings(uint32_t cond, const gt4_specdb_src *db)
{
    uint32_t race = GT4_U32(cond);
    uint8_t row[0x100];
    const char *code = "";
    memset(row, 0, sizeof row);
    if (db->row(db->ctx, 0x22, GT4_U32(race + 0x78), 0, row, sizeof row) > 0) {    /* func_00447F48 */
        uint32_t s;
        GT4_U8(cond + 0xcbddu) = row[0x10];
        GT4_U8(cond + 0xcbe0u) = row[0x14] != 0;
        GT4_U8(cond + 0xcbdfu) = row[0x13];
        GT4_U8(cond + 0xcbdeu) = row[0x12];
        GT4_U8(cond + 0xcbe1u) = row[0x13] >= 2;
        memcpy(&s, row + 8, 4);
        if (db->str && db->str(db->ctx, s)) code = db->str(db->ctx, s);                /* func_00447F08 */
    }
    memset(gt4_ptr(cond + 0xcbe6u), 0, 8);
    for (uint32_t i = 0; i < 8 && code[i]; i++) {
        uint8_t c = (uint8_t)code[i], v = c;
        if (i == 0 || i == 3) {
            if ((uint8_t)(c - '0') < 10) v = (uint8_t)(c - '0');
        } else if (i < 3) {
            if ((uint8_t)(c - '0') < 10) v = (uint8_t)(c - '0');
            else if ((uint8_t)(c - 'a') < 6) v = (uint8_t)(c - 'a' + 10);
            else if ((uint8_t)(c - 'A') < 6) v = (uint8_t)(c - 'A' + 10);
        }
        GT4_U8(cond + 0xcbe6u + i) = v;
    }
}

/* func_00466B90: the AI settings of the course (the .ad file's block at
   course+0xA0), else the defaults (D_00623DC0, the second set for race
   modes 2 and 3). */
static uint32_t course_ai_settings(uint32_t cond, uint32_t mode)
{
    uint32_t v = GT4_U32(GT4_U32(cond + 0xcbd8u) + 0xa0);
    if (v) return v;
    return 0x00623dc0u + (mode == 2 || mode == 3 ? 64u : 0u);
}

/* func_00360348: the course's flags (runway +0x94, one bit per feature;
   +0x98 bit 1) as the session's rule bits at +0xCBF0. */
static void course_rules(uint32_t cond)
{
    uint32_t rw = GT4_U32(GT4_U32(GT4_U32(cond + 0xcbd8u) + 4) + 0xdc), bits = GT4_U32(rw + 0x94), r = 0;
    int wide = (GT4_U32(rw + 0x98) & 2) != 0;
    for (uint32_t k = 0; k < 15; k++) {
        if (!(bits & 1u << k)) continue;
        switch (k) {
        case 0: case 7: case 8: case 9: case 12: r |= 3; if (wide) r |= 4; break;
        case 1: r |= 0x10; break;
        case 2: case 3: case 4: r |= 0x20; break;
        case 5: r |= 8; break;
        case 6: r |= 4; break;
        case 10: r |= 0x40; break;
        case 11: r |= 0x81; break;
        case 13: r |= 0x200; break;
        case 14: r |= 0x400; break;
        }
    }
    GT4_U32(cond + 0xcbf0u) = r;
}

/* func_003F0100 / func_003EFF28: the race's shared tables for six slots
   cleared, and two per-slot values from D_00622010/D_00622018 as floats. */
static void race_tables_clear(void)
{
    memset(gt4_ptr(0x008456c0u), 0, 0x120);
    memset(gt4_ptr(0x008457f8u), 0, 0x120);
    for (uint32_t k = 0; k < 6; k++) {
        GT4_U32(0x008456c0u + k * 0x30) = 0xffffffffu;
        GT4_U32(0x008457e0u + k * 4) = 0xffffffffu;
        GT4_U32(0x008457f8u + k * 0x30) = 0xffffffffu;
        GT4_U32(0x00845918u + k * 4) = 0xffffffffu;
        GT4_U8(0x00845930u + k) = 1;
        GT4_U8(0x00845938u + k) = 0;
    }
    for (uint32_t k = 0; k < 6; k++) {
        uint32_t a = GT4_U8(0x00622010u + k), b = GT4_U8(0x00622018u + k);
        GT4_F32(0x00845958u + k * 4) = (float)(int32_t)b;
        GT4_F32(0x00845940u + k * 4) = (float)(int32_t)(a * 10);
    }
    for (uint32_t k = 0; k < 18; k++) {
        GT4_U32(0x00845970u + k * 4) = 0;
        GT4_U32(0x008459b8u + k * 4) = 0;
    }
}

/* func_00343AB8: a car and its block (0x1100 bytes) cleared, hooked to
   the race and session; its driver model to the session and lines
   (func_00463930). */
static void car_clear(uint32_t car, uint32_t cond, uint32_t race, uint32_t block)
{
    memset(gt4_ptr(car), 0, CAR_SIZE);
    memset(gt4_ptr(block), 0, 0x1100);
    GT4_U32(car + 4) = cond;
    GT4_U32(car) = race;
    GT4_U32(car + 0x10) = block;
    GT4_U32(car + 0x14 + 0xe8) = car;
    GT4_U32(car + 0x14 + 0xec) = cond + 0xda50u;
    GT4_U32(car + 0x14 + 0xe4) = cond;
}

/* func_003F24A0: how much wheel i is lowered by the car's lowering kind
   (params+0x1076, D_00622268: 32 bytes each): kind 2 with the speed
   between two limits, kind 3 on the surfaces D_00620288 does not mark. */
static float wheel_lowering(uint32_t car, uint32_t i)
{
    uint32_t t = 0x00622268u + GT4_U8(GT4_U32(car + CAR_PARAMS) + 0x1076) * 32u;
    uint8_t kind = GT4_U8(t);
    if (kind == 2) {
        float v = GT4_F32(car + 0x640), lo, hi = GT4_F32(t + 0x1c);
        if (hi <= v) return GT4_F32(t + 0x14);
        lo = GT4_F32(t + 0x18);
        if (!(lo < v)) return 0;
        return GT4_F32(t + 0x14) * (v - lo) / (hi - lo);
    }
    if (kind == 3 && !GT4_U8(0x00620288u + GT4_U8(car + 0x1ed + i * 0xec))) return GT4_F32(t + 0x14);
    return 0;
}

/* func_003F2560: a car with lowering starts lowered: the wheels' travel
   and the body's height. */
static void car_lowered(uint32_t car)
{
    uint32_t p = GT4_U32(car + CAR_PARAMS);
    float d = 0;
    if (!GT4_U8(p + 0x1076)) return;
    for (uint32_t i = 0; i < 4; i++) {
        uint32_t w = car + 0x164 + i * 0xec;
        float f = wheel_lowering(car, i);
        GT4_F32(w + 0x94) = GT4_F32(w + 0x94) - f;
        d = d - f * GT4_F32(p + 0x20 + (1 - (i >> 1)) * 4);
    }
    GT4_F32(car + 0x10c) = GT4_F32(car + 0x10c) + d * 0.5f;
    gt4_car_mounts(car);
}

/* func_004462A0 / func_00446178: bytes of the GEAR row of the garage entry
   (key at +0x48): 0x22, and the four the car keeps at +0x1080 (via the
   object func_00385410 builds). */
static int gear_row(const gt4_specdb_src *db, uint32_t gar, uint8_t *row, size_t cap)
{
    uint32_t lo = GT4_U32(gar + 0x48), hi = GT4_U32(gar + 0x4c);
    memset(row, 0, cap);
    if (lo == 0xffffffffu && hi == 0xffffffffu) return 0;
    return db->row(db->ctx, hi, lo, 0, row, cap) > 0;
}

/* session virtual call with up to five arguments after the object */
static uint32_t session_call5(uint32_t cond, uint32_t off, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1)
{
    return gt4_session(cond, off, a1, a2, a3, t0, t1, NULL);
}

/* func_00395228: the heading of grid slot `slot` (+pi), from the runway's
   own grid or the course's start set (+0xB0 when not negative). */
static float grid_heading(uint32_t course, uint32_t slot)
{
    uint32_t rw = GT4_U32(GT4_U32(course + 4) + 0xdc), list;   /* func_00396800 */
    int32_t set = (int32_t)GT4_U32(course + 0xb0);
    if (set < 0) list = GT4_U32(rw + 0x5c);
    else list = GT4_U32(GT4_U32(rw + 0x9c) + (uint32_t)set * 32 + 0x18);
    return GT4_F32(list + slot * 16 + 0xc) + K(0x40490fdau);
}

/* func_0034D490: the race start. mode: how the race runs (1 as 0, 3 as 2);
   then the course's settings, the lines, and for each car: its record from
   its garage entry, its start from the session (virtual +0xE8, +0xF0), the
   car itself (gt4_car_init) on its grid slot. db: where the SpecDB rows
   come from; scratch: 0x700 bytes of guest memory. */
void gt4_race_init(uint32_t cond, uint32_t mode, const gt4_specdb_src *db, uint32_t scratch)
{
    uint32_t race = GT4_U32(cond), rec = scratch, buf = scratch + 0x600, out = scratch + 0x680, n;
    GT4_U8(cond + 0xcbdcu) = (uint8_t)mode;
    GT4_U32(cond + 0xcbd8u) = GT4_U32(race + 0x80);
    if ((mode & 0xff) == 1) GT4_U8(cond + 0xcbdcu) = 0;
    else if ((mode & 0xff) == 3) GT4_U8(cond + 0xcbdcu) = 2;
    course_settings(cond, db);
    if (GT4_U8(cond + 0xcbe6u)) {
        uint32_t v = course_ai_settings(cond, race_mode(cond));
        int32_t d = -(int32_t)(GT4_U8(v + 0x23) * 10);
        GT4_U16(cond + 0xf884u) = (uint16_t)d;
        GT4_U8(cond + 0xf886u) = GT4_U8(v + 0x24);
        GT4_U8(cond + 0xf887u) = GT4_U8(v + 0x25);
        if (!d) GT4_U16(cond + 0xf884u) = (uint16_t)-400;
        if (!GT4_U8(cond + 0xf886u)) GT4_U8(cond + 0xf886u) = 0x50;
        if (!GT4_U8(cond + 0xf887u)) GT4_U8(cond + 0xf887u) = 0x14;
        for (uint32_t k = 0; k < GT4_U8(cond + 0xf858u) + 1u; k++)
            if (GT4_U16(cond + 0xf878u + k * 2) || GT4_U16(cond + 0xf86cu + k * 2) || GT4_U8(cond + 0xf866u + k))
                GT4_U8(cond + 0xf886u) = 0;
        if (session_call(cond, 0x30, 0, NULL)) GT4_U8(cond + 0xf886u) = 0;
    }
    GT4_U8(cond + 0xcbe4u) = 0;
    GT4_U8(cond + 0xcbe3u) = 0;
    GT4_U16(cond + 0xcbf4u) = 0;
    GT4_U32(cond + 0xcd7cu) = cond;                         /* func_00414658 */
    gt4_lines_setup(cond + 0xda50u, GT4_U32(cond + 0xcbd8u));
    gt4_lines_clear(cond + 0xda50u);
    course_rules(cond);
    race_tables_clear();
    if (!gt4_session_start(cond, 1)) session_call(cond, 0xd0, 1, NULL);
    n = GT4_U32(cond + 0xf8a0u);
    for (uint32_t i = 0; (int32_t)i < (int32_t)n; i = i + 1, n = GT4_U32(cond + 0xf8a0u)) {
        uint32_t entry = GT4_U32(GT4_U32(GT4_U32(race + 0x60) + 8) + i * 4), car = cond + 0x6698u + i * CAR_SIZE;
        uint32_t gar = entry + 0x20, slot, g, p;
        float pos[4];
        GT4_U32(cond + i * 4 + 0xf910u) = 0;
        GT4_U32(cond + i * 4 + 0xf8f8u) = 0;
        car_clear(car, cond, race, cond + 0x98 + i * 0x1100u);
        session_call5(cond, 0xe8, i, out + 0xc, out + 4, out, out + 8);
        gt4_car_record(rec, gar, race_mode(cond), db);
        slot = GT4_U32(entry + 0x3444);
        g = grid_slot(GT4_U32(race + 0x80), slot);
        pos[0] = GT4_F32(g);
        pos[1] = -GT4_F32(g + 8);
        pos[2] = GT4_F32(g + 4);
        pos[3] = grid_heading(GT4_U32(race + 0x80), slot);
        GT4_U32(out + 0x10) = 0;
        GT4_U32(out + 0x14) = 0;
        GT4_U32(out + 0x18) = 0;
        session_call5(cond, 0xf0, i, out + 0x10, out + 0x14, out + 0x18, 0);
        gt4_car_init(car, rec, 0xffffffffu, GT4_U32(out), GT4_U32(out + 4), GT4_U32(out + 8), i, GT4_U32(out + 0x10),
                     pos, (int32_t)GT4_U32(out + 0x14), (int32_t)GT4_U32(out + 0x18), entry + 0x3468, buf);
        GT4_U32(cond + 0xcbfcu + i * 4) = 0;
        if (!GT4_U32(out + 0x18))
            GT4_U32(cond + 0xcbfcu + i * 4) = (uint32_t)((int32_t)(int16_t)GT4_U16(cond + 0xf878u + i * 2) * 60);
        car_lowered(car);
        p = GT4_U32(car + CAR_PARAMS);
        GT4_U8(p + 0x10d1) = 0;
        {
            uint8_t row[0x100];
            int has = gear_row(db, gar, row, sizeof row);
            if (GT4_U32(gar + 0x48) == 0xffffffffu && GT4_U32(gar + 0x4c) == 0xffffffffu) GT4_U8(p + 0x10d1) = 1;
            else if (row[0x22] || GT4_U8(p + 0x32) == 2) GT4_U8(p + 0x10d1) = 1;
            if (has) {
                GT4_U32(car + 0x1080) = row[0x2c];
                GT4_U32(car + 0x1084) = (uint32_t)(int32_t)(int8_t)row[0x2a];
                GT4_U32(car + 0x1088) = (uint32_t)(int32_t)(int8_t)row[0x2b];
                GT4_U32(car + 0x108c) = row[0x24];
            }
        }
        GT4_U32(car + 0x7b0) = 0;                           /* func_0035DCF0 */
        GT4_U32(car + 0x6d4) = 0;
        gt4_car_keep_state(car);
        gt4_car_wheel_rotation(car);
    }
    gt4_race_player_styles(cond);                           /* func_003F1E60 */
    if (!gt4_session_after_cars(cond, db)) session_call(cond, 0xd8, 1, NULL);
    if (!gt4_phys_headless)                                 /* the engine sound's (func_003F71A0) */
        for (uint32_t i = 0; (int32_t)i < (int32_t)GT4_U32(cond + 0xf8a0u); i++) orig(0x003f71a0u, cond + 0x6698u + i * CAR_SIZE, 0, 0, 0);
}

/* func_0035C4D8: for each car, the tyres the pit can fit: the compound
   ranges of its front and rear tyres (D_00620860 / D_00620870 by the
   tyre's kind, FRONTTIRE row 0x3A / REARTIRE row 0x38) and, for each
   level in them, the part's key from the car's parts list
   (func_00449E10 loads it, func_0044A048 finds a table and level in it). */
static uint64_t part_key(const uint8_t *parts, size_t size, uint32_t table, uint32_t level)
{
    int32_t n;
    if (!parts || size < 8) return ~(uint64_t)0;
    memcpy(&n, parts, 4);
    for (int32_t i = 0; i < n && 8 + (size_t)i * 16 + 16 <= size; i++) {
        uint64_t key;
        uint32_t t;
        memcpy(&key, parts + 8 + (size_t)i * 16, 8);
        t = (uint32_t)(key >> 32);
        if (t == table && parts[8 + (size_t)i * 16 + 8] == level) return key;
        if (table < t) break;
    }
    return ~(uint64_t)0;
}

void gt4_pit_tyres(uint32_t cond, const gt4_specdb_src *db)
{
    uint32_t race = GT4_U32(cond);
    for (uint32_t i = 0; (int32_t)i < (int32_t)GT4_U32(cond + 0xf8a0u); i++) {
        uint32_t gar = GT4_U32(GT4_U32(GT4_U32(race + 0x60) + 8) + i * 4) + 0x20, f, r, lo, hi;
        uint64_t car = (uint64_t)GT4_U32(gar) | (uint64_t)GT4_U32(gar + 4) << 32;
        size_t size = 0;
        uint8_t *parts = NULL;
        if ((uint32_t)(car >> 32) != 0x26 && db->car_parts) parts = db->car_parts(db->ctx, car, &size);
        f = (uint8_t)gt4_tyre_byte(db, gar, 0x60, 0x3a);   /* func_004462E0 */
        r = (uint8_t)gt4_tyre_byte(db, gar, 0x68, 0x38);   /* func_00446320 */
        GT4_U8(cond + 0xfb20u + i * 2) = (uint8_t)f;
        GT4_U8(cond + 0xfb21u + i * 2) = (uint8_t)r;
        lo = GT4_U8(0x00620860u + f);
        hi = GT4_U8(0x00620870u + f);
        GT4_U8(cond + 0xf928u + i * 4) = (uint8_t)lo;
        GT4_U8(cond + 0xf929u + i * 4) = (uint8_t)hi;
        for (uint32_t k = lo; (int32_t)k <= (int32_t)hi; k++) {
            uint64_t key = part_key(parts, size, 0x19, k);
            memcpy(gt4_ptr(cond + 0xf940u + i * 0x50 + (k - lo) * 8), &key, 8);
        }
        lo = GT4_U8(0x00620860u + r);
        hi = GT4_U8(0x00620870u + r);
        GT4_U8(cond + 0xf92au + i * 4) = (uint8_t)lo;
        GT4_U8(cond + 0xf92bu + i * 4) = (uint8_t)hi;
        for (uint32_t k = lo; (int32_t)k <= (int32_t)hi; k++) {
            uint64_t key = part_key(parts, size, 0x1a, k);
            memcpy(gt4_ptr(cond + 0xf968u + i * 0x50 + (k - lo) * 8), &key, 8);
        }
        free(parts);
    }
}
