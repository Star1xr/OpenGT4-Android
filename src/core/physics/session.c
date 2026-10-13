/* The race session: the object the conductor is (its virtual table at
   conductor+0x10140), which says how the race runs: laps, checkpoints,
   where and how each car starts, the rules. Each kind of race is a class
   of its own; the Time Trial's methods are rewritten here, the other
   classes (and the Time Trial methods not rewritten yet) still run the
   original through the table. */
#include <fenv.h>
#include "car.h"
#include "license_session.h"

#include <string.h>

#define K(bits) gt4_f32(bits)
#define SESSION_VT       0x10140u
#define TIME_TRIAL_VT    0x00685890u   /* the Time Trial session's table */
#define RACE_VT          0x00679ce8u   /* a race against other cars (Arcade) */
#define PRACTICE_VT      0x006856e0u   /* DynamicsConductorFreePractice (ctor 003F4EB8): GT Mode's Practice */

int gt4_session_orig;
gt4_tt_ghost_fn gt4_tt_ghost;
void *gt4_tt_ghost_ctx;
static uint32_t event_rest;   /* the start/lap events' t2 and f12 (laps.c) */
static float event_frac;

static uint32_t course_of(uint32_t cond) { return GT4_U32(cond + 0xcbd8u); }
static uint32_t runway_of(uint32_t course) { return GT4_U32(GT4_U32(course + 4) + 0xdc); }   /* func_00396800 */

/* The course's checkpoint set: the runway's own (v < 0, course+0xB0) or
   one of its variants (32 bytes each at runway+0x9C). */
static int variant(uint32_t course) { return (int32_t)GT4_U32(course + 0xb0); }
static uint32_t variant_at(uint32_t course) { return GT4_U32(runway_of(course) + 0x9c) + (uint32_t)variant(course) * 32; }

/* func_00395528 / func_00395588: two values of the set (runway +0x18/+0x1C,
   variant +0/+4) */
static float course_value(uint32_t course, uint32_t k)
{
    if (variant(course) < 0) return GT4_F32(runway_of(course) + 0x18 + k * 4);
    return GT4_F32(variant_at(course) + k * 4);
}

/* func_00395420 (func_003E6BF0 / func_003E6DB8): entry j's first word of
   the set's table (count u16, table of 8-byte entries), 0 past the end */
static uint32_t course_entry(uint32_t course, uint32_t j)
{
    uint32_t rw = runway_of(course), n, t;
    if (variant(course) < 0) {
        n = GT4_U16(rw + 0x38);
        t = GT4_U32(rw + 0x58);
    } else {
        uint32_t v = variant_at(course);
        n = GT4_U16(v + 8);
        t = GT4_U32(v + 0x14);
    }
    return (int32_t)j < (int32_t)n ? GT4_U32(t + j * 8) : 0;
}

/* func_00395498 (func_003E6C20 / func_003E6DE8): float k of entry j's list */
static float course_entry_value(uint32_t course, uint32_t k, uint32_t j)
{
    uint32_t rw = runway_of(course), n, t;
    if (variant(course) < 0) {
        n = GT4_U16(rw + 0x38);
        t = GT4_U32(rw + 0x58);
    } else {
        uint32_t v = variant_at(course);
        n = GT4_U16(v + 8);
        t = GT4_U32(v + 0x14);
    }
    if (!((int32_t)j < (int32_t)n)) return 0;
    return GT4_F32(GT4_U32(t + j * 8 + 4) + k * 4);
}

/* func_0034C230: the course's length */
static float course_length(uint32_t cond) { return GT4_F32(runway_of(course_of(cond)) + 0x14); }

/* func_0034C968: the race mode (0xFF as 0) */
static uint32_t race_mode(uint32_t cond)
{
    uint32_t m = GT4_U8(cond + 0xcbddu);
    return m == 0xff ? 0 : m;
}

/* func_00466B90 */
static uint32_t course_ai_settings(uint32_t cond, uint32_t mode)
{
    uint32_t v = GT4_U32(course_of(cond) + 0xa0);
    if (v) return v;
    return 0x00623dc0u + (mode == 2 || mode == 3 ? 64u : 0u);
}

/* func_0034C338: the laps (virtual +0x130) are at least one and less than
   the course's length less one */
static int laps_counted(uint32_t cond)
{
    float f, n;
    gt4_session(cond, 0x130, 0, 0, 0, 0, 0, &f);
    if (f < 1.0f) return 0;
    gt4_session(cond, 0x130, 0, 0, 0, 0, 0, &n);
    return !(course_length(cond) - 1.0f < n);
}

/* func_0034C3E0: the laps of the session, 0 when it counts none */
int32_t gt4_session_laps(uint32_t cond)
{
    float f;
    if ((int8_t)GT4_U8(cond + 0xcbe2u) == -1) return 0;
    if (!laps_counted(cond)) return 0;
    gt4_session(cond, 0x130, 0, 0, 0, 0, 0, &f);
    return (int32_t)f;
}

/* func_0034CF30: car i's start: out a2 0, a3 its entry's +0x3448, t0 how
   it is held (2 when its profile says so, 1 in some race kinds, else 0),
   t1 1 for car 1 */
static void start_hold(uint32_t cond, uint32_t i, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1)
{
    uint32_t race = GT4_U32(cond), entry = GT4_U32(GT4_U32(GT4_U32(race + 0x60) + 8) + i * 4), p;
    GT4_U32(a2) = 0;
    GT4_U32(a3) = GT4_U32(entry + 0x3448);
    p = GT4_U32(entry + 0x343c);
    if (!GT4_U32(p + 0x1cc)) GT4_U32(t0) = 2;
    else if (GT4_U32(race + 0xe0) && GT4_U32(p + 0x1d0)) GT4_U32(t0) = 2;
    else {
        uint32_t k = GT4_U32(GT4_U32(race + 0x84) + 0xcc8);   /* func_003F1760 */
        GT4_U32(t0) = 0;
        if (k == 1 || k == 3) GT4_U32(t0) = 1;
        else if (GT4_U8(cond + 0xcbdcu) == 4) GT4_U32(t0) = 2;
    }
    GT4_U32(t1) = 0;
    if (i == 1) GT4_U32(t1) = i;
}

/* func_0034D288: a rolling start from the course's AI settings (+0x19 tens
   of metres back, +0x1A speed in 5 km/h): kind 3 */
static int start_rolling(uint32_t cond, uint32_t a2, uint32_t a3, uint32_t t0)
{
    uint32_t v = course_ai_settings(cond, race_mode(cond));
    int32_t d = -(int32_t)(GT4_U8(v + 0x19) * 10), k = (int32_t)GT4_U8(v + 0x1a) * 5;
    if (d >= 0 || k <= 0) return 0;
    GT4_U32(a2) = 3;
    GT4_U32(a3) = (uint32_t)d;
    GT4_U32(t0) = (uint32_t)k;
    return 1;
}

/* func_0034D168: a start the session gives by its own (virtual +0x118 the
   distance, +0x120 the speed): kind 2, the distance a lap on when it
   counts laps */
static int start_given(uint32_t cond, uint32_t i, uint32_t a2, uint32_t a3, uint32_t t0)
{
    if ((int32_t)gt4_session(cond, 0x118, i, 0, 0, 0, 0, NULL) >= 0) return 0;
    if ((int32_t)gt4_session(cond, 0x120, i, 0, 0, 0, 0, NULL) <= 0) return 0;
    GT4_U32(a2) = 2;
    GT4_U32(a3) = gt4_session(cond, 0x118, i, 0, 0, 0, 0, NULL);
    if (GT4_U32(a3)) GT4_U32(a3) = GT4_U32(a3) + (uint32_t)gt4_session_laps(cond);
    GT4_U32(t0) = gt4_session(cond, 0x120, i, 0, 0, 0, 0, NULL);
    return 1;
}

/* func_0035FA98: a car off the course: with all its wheels marked (a2)
   the session may flag it (virtual +0x170, 1 in a Time Trial), with all
   off the road (a3) too (+0x178); +0x4B6 counts the frames, +0x4B3 says
   which (4 marked, 2 off). */
static void off_course(uint32_t cond, uint32_t car, uint32_t marked, uint32_t off)
{
    uint32_t b = car + 0x104, n = GT4_U8(gt4_car_layout(car) + LAYOUT_WHEELS);
    int32_t s = 0, v;
    int by_marks = 0;
    if (GT4_U8(b + 0x4b3) || (int8_t)GT4_U8(b + 0x466)) return;
    if (marked == n) {
        by_marks = 1;
        v = (int32_t)gt4_session(cond, 0x170, 0, 0, 0, 0, 0, NULL);
        s = v > 0 ? v : 0;
    }
    if (off == n) {
        v = (int32_t)gt4_session(cond, 0x178, 0, 0, 0, 0, 0, NULL);
        if (s < v) s = v;
    }
    if (s == 1) GT4_U8(b + 0x4b6) = (uint8_t)(GT4_U8(b + 0x4b6) + 1);
    else if (s == 2) GT4_U8(b + 0x4b6) = 0xff;
    else GT4_U8(b + 0x4b6) = 0;
    if (GT4_U8(b + 0x4b6)) GT4_U8(b + 0x4b3) = by_marks ? 4 : 2;
}

/* func_00358558: the car's distance along the course (a stand-in point
   from D_00622020 for a car in state 4) */
static float car_distance(uint32_t cond, uint32_t car)
{
    uint32_t b = car + 0x104, id = GT4_U32(b + 0x4a8);
    float p[3];
    if ((int8_t)GT4_U8(b + 0x466) == 4) {
        uint32_t t = 0x00622020u + GT4_U8(b + 0x440) * 12u;
        p[0] = GT4_F32(t);
        p[1] = GT4_F32(t + 8);
        p[2] = -GT4_F32(t + 4);
    } else {
        p[0] = GT4_F32(b);
        p[1] = GT4_F32(b + 8);
        p[2] = -GT4_F32(b + 4);
    }
    return gt4_course_distance_p(course_of(cond), p, &id);
}

static uint32_t car_at(uint32_t cond, uint32_t i) { return cond + 0x6698u + i * CAR_SIZE; }   /* func_0034C190 */

/* func_003447C8: the car's distance along the course t of the way between
   the frame's start (+0x99C) and end (+0x5A8), t = f + car+0x554 */
static float distance_between(uint32_t car, float f)
{
    float t = f + GT4_F32(car + 0x554);
    return GT4_F32(car + 0x99c) * t + GT4_F32(car + 0x5a8) * (1.0f - t);
}

/* func_003658F0: laps done (less those counted down, while racing) */
static int32_t laps_done(uint32_t b)
{
    int32_t l = (int16_t)GT4_U16(b + 0x4ac);
    if (!GT4_U8(b + 0x4b2)) l = l - GT4_U8(b + 0x4b0);
    return l;
}

/* func_003659E8: the order of two cars (their blocks at +0x104): 1 when b
   is ahead, -1 when a is */
static int position_order(uint32_t cond, uint32_t a, uint32_t b)
{
    uint32_t ka = GT4_U8(a + 0x441), kb;
    float fa, fb;
    int32_t la, lb;
    uint8_t da, db;
    if (ka && ((uint8_t)(GT4_U8(a + 0x466) - 1) < 2 || GT4_U8(a + 0x467))) return -1;
    kb = GT4_U8(b + 0x441);
    if (kb && ((uint8_t)(GT4_U8(b + 0x466) - 1) < 2 || GT4_U8(b + 0x467))) return 1;
    fa = GT4_F32(a + 0x4a4);
    fb = GT4_F32(b + 0x4a4);
    if (ka == 2) {
        fa = distance_between(car_at(cond, GT4_U8(a + 0x440)), 0);
        kb = GT4_U8(b + 0x441);
    }
    if (kb == 2) fb = distance_between(car_at(cond, GT4_U8(b + 0x440)), 0);
    la = laps_done(a);
    lb = laps_done(b);
    if (lb < la) return 1;
    if (la < lb) return -1;
    da = GT4_U8(b + 0x4b2);
    db = GT4_U8(a + 0x4b2);
    if (!(da | db)) {
        if (fb < fa) return 1;
        if (fa < fb) return -1;
        return 0;
    }
    if (da && db) {
        int32_t d = (int32_t)(GT4_U32(b + 0x614) - GT4_U32(a + 0x614));
        return d < 0 ? -1 : d > 0 ? 1 : 0;
    }
    if (GT4_U8(cond + 0xf81au) == 2 || GT4_U8(cond + 0xcbdeu) == 1) return da ? -1 : 1;
    return da ? 1 : -1;
}

/* func_00365BC0: the race order (conductor+0xF8A4, car indices) by
   insertion, then each car's position (+0x5B3, from 1) */
static void race_positions(uint32_t cond)
{
    uint32_t order = cond + 0xf8a4u;
    for (int32_t k = 1; k < (int32_t)GT4_U32(cond + 0xf8a0u); k++) {
        uint32_t cur = GT4_U32(order + (uint32_t)k * 4);
        int32_t j = k - 1;
        for (; j >= 0; j--) {
            uint32_t other = GT4_U32(order + (uint32_t)j * 4);
            gt4_session(cond, 0x128, 0, 0, 0, 0, 0, NULL);
            if (position_order(cond, car_at(cond, other) + 0x104, car_at(cond, cur) + 0x104) >= 0) break;
            GT4_U32(order + (uint32_t)(j + 1) * 4) = other;
        }
        GT4_U32(order + (uint32_t)(j + 1) * 4) = cur;
    }
    for (uint32_t k = 0; (int32_t)k < (int32_t)GT4_U32(cond + 0xf8a0u); k++)
        GT4_U8(car_at(cond, GT4_U32(order + k * 4)) + 0x5b3) = (uint8_t)(k + 1);
}

/* func_00357E80: the race clocks (+0xF89C always, +0xF898 unless stopped
   at +0xCBE4; 50 a frame, up to 0x405F7DFD), and the lap records at the
   start of a held race */
static void race_clock(uint32_t cond)
{
    uint32_t race = GT4_U32(cond), cap = 0x405f7dfdu, v;
    if (GT4_U32(race + 0xbc) == 1 && gt4_session(cond, 0x38, 0, 0, 0, 0, 0, NULL) && GT4_U32(race + 0xe4)) {
        for (uint32_t i = 0; (int32_t)i < (int32_t)GT4_U32(cond + 0xf8a0u); i++) {
            uint32_t car = car_at(cond, i);
            if (!gt4_phys_headless && gt4_car_records_times(car)) {
                gt4_orig_a2 = 0;
                gt4_orig_f14 = GT4_F32(car + 0x5a8);
                gt4_orig_call(0x00359b60u, car, 0, 1.0f, 0);
            }
        }
    }
    if ((int32_t)GT4_U32(race + 0xbc) > 0) return;          /* func_0034C210 */
    v = GT4_U32(cond + 0xf89cu) + 50;
    GT4_U32(cond + 0xf89cu) = cap < v ? cap : v;
    if ((int8_t)GT4_U8(cond + 0xcbe4u)) return;
    v = GT4_U32(cond + 0xf898u) + 50;
    GT4_U32(cond + 0xf898u) = cap < v ? cap : v;
}

/* The lap events of the Time Trial without what they hand to the
   presentation (the race's event queue, the ghost, lap records and the
   player's records): headless only. func_003F59F8 (+0xA0, the start),
   func_003F5B80 (+0xA8, a lap), func_00357D00 (+0xB8, the finish). */
static void lap_start_common(uint32_t cond, uint32_t car, uint32_t time)
{
    uint32_t b = car + 0x104;
    if (GT4_U8(b + 0x441) || GT4_U32(GT4_U32(cond) + 0xe4)) gt4_car_take_over(car, 6);
    GT4_U8(b + 0x4b6) = 0;                                   /* func_00363528 */
    GT4_U8(b + 0x4b3) = 0;
    GT4_U32(b + 0x614) = time;
}
static void tt_start(uint32_t cond, uint32_t idx, uint32_t time)
{
    uint32_t car = car_at(cond, idx), b = car + 0x104;
    if (!idx && GT4_U8(b + 0x467) == 1) {
        gt4_car_pit_leave(car);
        GT4_U16(car + 0x694) = 0;                            /* func_003F37B8 */
    }
    /* func_003F59F8 +0x80..+0x130: the ghost and the recording */
    if (!idx && gt4_tt_ghost) gt4_tt_ghost(gt4_tt_ghost_ctx, cond, 0, 0, event_rest, event_frac);
    lap_start_common(cond, car, time);
}
/* func_0035E3C8: the car's best lap */
static void best_lap(uint32_t car, uint32_t lap)
{
    if (lap < GT4_U32(car + 0x104 + 0x618)) GT4_U32(car + 0x104 + 0x618) = lap;
}
static void tt_lap(uint32_t cond, uint32_t idx, uint32_t time, uint32_t lap)
{
    uint32_t car = car_at(cond, idx), world = GT4_U32(GT4_U32(cond) + 0x84);
    uint32_t b = car + 0x104;
    best_lap(car, lap);
    /* func_003F5B80 +0x8C..+0x28C: the ghost and the recording */
    if (!idx && gt4_tt_ghost) gt4_tt_ghost(gt4_tt_ghost_ctx, cond, (uint32_t)(int16_t)GT4_U16(b + 0x4ac) - 1, lap, event_rest, event_frac);
    if (GT4_U8(b + 0x441) || GT4_U32(GT4_U32(cond) + 0xe4)) gt4_car_take_over(car, 6);
    if (GT4_U32(world + 0xd54)) gt4_car_take_over(car, 5);
    GT4_U8(b + 0x4b6) = 0;
    GT4_U8(b + 0x4b3) = 0;
    GT4_U32(b + 0x614) = time;
}
static uint32_t tt_finish(uint32_t cond, uint32_t idx, uint32_t time, uint32_t lap)
{
    uint32_t car = car_at(cond, idx);
    if (GT4_U8(cond + 0xcbdeu) != 1 && GT4_U8(cond + 0xf81au) != 2) return 0;
    best_lap(car, lap);
    gt4_car_finish_drive(car, 3);
    if (!idx && GT4_U8(cond + 0xcbdcu) == 4) GT4_U16(cond + 0xcbf4u) = 3 * 60 + 1;   /* func_0034C0A0 */
    GT4_U8(car + 0x104 + 0x4b6) = 0;
    GT4_U8(car + 0x104 + 0x4b3) = 0;
    GT4_U32(car + 0x104 + 0x614) = time;
    return 1;
}

/* func_00365D80 (method 0x10): the race order reset: no one placed
   (conductor+0xF8A4.. -1), then car i in place i+1. */
static void race_order_reset(uint32_t cond)
{
    for (uint32_t k = 0; k < 6; k++) GT4_U32(cond + 0xf8a4u + k * 4) = 0xffffffffu;
    for (uint32_t i = 0; (int32_t)i < (int32_t)GT4_U32(cond + 0xf8a0u); i++) {
        GT4_U8(car_at(cond, i) + 0x5b3) = (uint8_t)(i + 1);
        GT4_U32(cond + 0xf8a4u + i * 4) = i;
    }
}

#define SET_B(o) ((float)(int32_t)GT4_U8(0x00622110u + (o)))

/* func_0035E968: a few driving settings as floats (D_006208B8). */
static void drive_tables_a(void)
{
    const float ten = K(0x41200000u), deg = K(0x3c8efa34u);
    GT4_F32(0x006208b8u + 0x14) = SET_B(0xe9) * deg;
    GT4_F32(0x006208b8u + 0x08) = SET_B(0xe6) * ten;
    GT4_F32(0x006208b8u + 0x00) = SET_B(0xe4) * ten;
    GT4_F32(0x006208b8u + 0x04) = SET_B(0xe5) * ten;
    GT4_F32(0x006208b8u + 0x0c) = SET_B(0xe7) * deg;
    GT4_F32(0x006208b8u + 0x10) = SET_B(0xe8) * deg;
}

/* func_0035EA08: D_00843988 and D_008439A0 (three each of two bytes). */
static void drive_tables_b(void)
{
    float a = SET_B(0xdc), b = SET_B(0xdd), c = SET_B(0xde), d = SET_B(0xdf);
    for (uint32_t k = 0; k < 3; k++) {
        GT4_F32(0x00843988u + k * 4) = a;
        GT4_F32(0x00843994u + k * 4) = b;
        GT4_F32(0x008439a0u + k * 4) = c;
        GT4_F32(0x008439acu + k * 4) = d;
    }
    drive_tables_a();
}

/* func_0035EA90: the driving settings of the race's level (the rows of
   D_0062211C, 0x20 apart: 0 below level 2 or from 4 on, else 1) into
   D_00843928 / D_00843958 and D_006208D0. */
static void drive_tables(uint32_t cond)
{
    uint32_t lvl = GT4_U8(cond + 0xcbddu), r;                /* func_0034C968 */
    int row = 0;
    const uint32_t A = 0x00843928u, C = 0x00843958u, D = 0x006208d0u;
    drive_tables_b();
    if (lvl == 0xff) lvl = 0;
    if ((int32_t)lvl < 4) row = !((int32_t)lvl < 2);
    r = 0xc + (uint32_t)row * 0x20;
#define RB(o) SET_B(r + (o))
    GT4_F32(A + 0x00) = RB(0xe0);
    GT4_F32(A + 0x04) = RB(0xe1);
    GT4_F32(A + 0x08) = RB(0xe2);
    GT4_F32(C + 0x00) = RB(0xe6);
    GT4_F32(C + 0x04) = RB(0xe7);
    GT4_F32(C + 0x08) = RB(0xe8);
    GT4_F32(A + 0x0c) = RB(0xe3);
    GT4_F32(A + 0x10) = RB(0xe4);
    GT4_F32(A + 0x14) = RB(0xe5);
    GT4_F32(C + 0x0c) = RB(0xe9);
    GT4_F32(C + 0x10) = RB(0xea);
    GT4_F32(C + 0x14) = RB(0xeb);
    GT4_F32(A + 0x18) = RB(0xec);
    GT4_F32(A + 0x1c) = RB(0xed);
    GT4_F32(C + 0x18) = RB(0xf2);
    GT4_F32(A + 0x20) = RB(0xee);
    GT4_F32(C + 0x1c) = RB(0xf3);
    GT4_F32(A + 0x24) = RB(0xef);
    GT4_F32(A + 0x28) = RB(0xf0);
    GT4_F32(A + 0x2c) = RB(0xf1);
    GT4_F32(C + 0x24) = RB(0xf5);
    GT4_F32(C + 0x28) = RB(0xf6);
    GT4_F32(C + 0x2c) = RB(0xf7);
    GT4_F32(C + 0x20) = RB(0xf4);
    if (!GT4_U8(0x00622110u + r + 0xee)) {
        GT4_F32(A + 0x20) = RB(0xee) + K(0x41f00000u);
        GT4_F32(C + 0x20) = RB(0xf4) + K(0x41200000u);
    }
    GT4_F32(D + 0x08) = RB(0xfa) * K(0x41200000u);
    GT4_F32(D + 0x00) = RB(0xf8) * K(0x41200000u);
    GT4_F32(D + 0x10) = RB(0xfc) * K(0x3c8efa34u);
    GT4_F32(D + 0x14) = RB(0xfd) * K(0x3c23d70au);
    GT4_F32(D + 0x04) = RB(0xf9) * K(0x41200000u);
    GT4_F32(D + 0x0c) = RB(0xfb) * K(0x3c8efa34u);
#undef RB
    drive_tables_a();
}
#undef SET_B

/* func_003F5270 (Practice's method 0x10): no one placed (conductor+0xF8A4..
   -1), then each car in its grid slot's place (entry +0x3444) */
static void practice_order_reset(uint32_t cond)
{
    uint32_t race = GT4_U32(cond);
    for (uint32_t k = 0; k < 6; k++) GT4_U32(cond + 0xf8a4u + k * 4) = 0xffffffffu;
    for (uint32_t i = 0; (int32_t)i < (int32_t)GT4_U32(cond + 0xf8a0u); i++) {
        uint32_t slot = GT4_U32(GT4_U32(GT4_U32(GT4_U32(race + 0x60) + 8) + i * 4) + 0x3444);
        GT4_U8(car_at(cond, i) + 0x5b3) = (uint8_t)(slot + 1);
        GT4_U32(cond + 0xf8a4u + slot * 4) = i;
    }
}

/* func_003F5350 (Practice's method 0x18): the order by insertion on each
   car's best lap (car +0x71C, func_0035E3F0), the faster first, then each
   car's position (+0x5B3, from 1) */
static void practice_positions(uint32_t cond)
{
    uint32_t order = cond + 0xf8a4u;
    for (int32_t k = 1; k < (int32_t)GT4_U32(cond + 0xf8a0u); k++) {
        uint32_t cur = GT4_U32(order + (uint32_t)k * 4);
        int32_t j = k - 1;
        for (; j >= 0; j--) {
            uint32_t other = GT4_U32(order + (uint32_t)j * 4);
            if (!(GT4_U32(car_at(cond, cur) + 0x71c) < GT4_U32(car_at(cond, other) + 0x71c))) break;
            GT4_U32(order + (uint32_t)(j + 1) * 4) = other;
        }
        GT4_U32(order + (uint32_t)(j + 1) * 4) = cur;
    }
    for (uint32_t k = 0; (int32_t)k < (int32_t)GT4_U32(cond + 0xf8a0u); k++)
        GT4_U8(car_at(cond, GT4_U32(order + k * 4)) + 0x5b3) = (uint8_t)(k + 1);
}

/* func_00414660: the lap and position tables at conductor+0xCD7C (n x n
   u16 and u8) cleared. */
static void pairs_clear(uint32_t t)
{
    int n = (int)GT4_U32(GT4_U32(t) + 0xf8a0u);
    for (int a = 0; a < n; a++)
        for (int k = 0; k < n; k++) {
            GT4_U16(t + 4 + (uint32_t)(a * 12 + k * 2)) = 0;
            GT4_U8(t + 0x4c + (uint32_t)(a * 6 + k)) = 0;
        }
}

/* func_0034CD60: after the cars are set up (both tables' method 0xD8
   start with it): the pit tyres, the race order and positions, the tables
   of the cars' relations cleared, the gaps, the solver, the race's
   records, and (when no car is followed by the view) the driving
   settings. */
static void after_cars(uint32_t cond, const gt4_specdb_src *db)
{
    uint32_t race = GT4_U32(cond), world = GT4_U32(race + 0x84);
    gt4_pit_tyres(cond, db);                                  /* func_0035C4D8 */
    if (GT4_U32(cond + SESSION_VT) == PRACTICE_VT) {          /* the table's 0x10 and 0x18 */
        practice_order_reset(cond);
        practice_positions(cond);
    } else {
        race_order_reset(cond);                               /* method 0x10 */
        race_positions(cond);                                 /* method 0x18 */
    }
    memset(gt4_ptr(cond + 0xcc14u), 0, 0x90);                 /* func_003F4478 */
    memset(gt4_ptr(cond + 0xcca4u), 0, 0x24);                 /* func_003F40D0 */
    memset(gt4_ptr(cond + 0xccc8u), 0, 0x24);                 /* func_00365E38 */
    memset(gt4_ptr(cond + 0xccecu), 0, 0x90);
    gt4_race_gaps(cond);                                      /* func_00365E80 */
    pairs_clear(cond + 0xcd7cu);                              /* func_00414660 */
    gt4_walls_in(cond);                                       /* func_003F4B20 */
    gt4_walls_setup(cond);                                    /* func_0045B8C0 */
    GT4_U32(race + 0x88) = 0xfffffffeu;                       /* func_003C12B0 */
    GT4_U32(race + 0x8c) = 0x157529ffu;
    GT4_U32(race + 0x88 + 0x24) = 0;
    GT4_U32(race + 0x88 + 8) = 0;
    GT4_U32(race + 0x88 + 0xc) = 0;
    GT4_U32(race + 0x88 + 0x10) = 0;
    GT4_U8(race + 0x88 + 0x14) = 0;
    if (!GT4_U32(world + 0xcc8)) {                            /* func_0035F630 */
        drive_tables(cond);
        GT4_U32(0x006208b0u) = 0;
    }
}

/* func_00353570: the lap limits at 20 where unset */
static void lap_limits(uint32_t cond)
{
    for (uint32_t k = 0; k < 4; k++)
        if (!(int8_t)GT4_U8(cond + 0xf830u + k)) GT4_U8(cond + 0xf830u + k) = 0x14;
}

/* func_003F5580 (the Time Trial's method 0xD8): after_cars, the lap
   limits, the records object's best time, and with two cars the second
   one handed to the model. */
void gt4_tt_after_cars(uint32_t cond, const gt4_specdb_src *db)
{
    uint32_t race = GT4_U32(cond), world = GT4_U32(race + 0x84);
    after_cars(cond, db);
    lap_limits(cond);
    if (!GT4_U8(cond + 0xcbdcu)) {
        uint32_t t = GT4_U32(world + 0x64) + 0x240, rec, best;
        if (GT4_U32(t + 4) == 0x003ea970u) {                  /* func_003EA8C8: the records object */
            rec = GT4_U32(world + 0x10000u - 0xfacu) || GT4_U32(GT4_U32(world + 0x6c) + 0xe0) ? world + 0xe84cu : world + 0xe48cu;
        } else {
            rec = gt4_vcall(world, 0x64, 0x240, 0x100, 0, 0);
        }
        rec = GT4_U32(rec + 0x144);
        best = GT4_U32(rec + 4) ? GT4_U32(rec + 0x60) : 0;    /* func_00346A28 */
        if (!GT4_U32(best + 0x10)) GT4_U32(best + 0x10) = 0x157529ffu;
    }
    if (GT4_U32(cond + 0xf8a0u) == 2) gt4_car_take_over(car_at(cond, 1), 6);   /* func_00345520 */
}

/* The Time Trial's methods; 0 for the ones not rewritten. */
static int time_trial(uint32_t cond, uint32_t off, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1,
                      uint32_t *v0, float *f0)
{
    switch (off) {
    case 0x50: *v0 = 2; return 1;                                    /* func_003F5528 */
    case 0x30: case 0x170: case 0x180: case 0x1a0: *v0 = 1; return 1;   /* func_003F5518 ... */
    case 0x128: case 0x160: case 0x178: *v0 = 0; return 1;
    case 0x08: case 0xc0: case 0x90: case 0xb0: return 1;                /* func_00359230, func_0034E968, func_00357B08, func_00357CF8 */
    case 0x38: case 0x40: *v0 = 0; return 1;                             /* func_00358250, func_00358258 */
    case 0x48: *v0 = GT4_U8(a1 + 0x5b8); return 1;                       /* func_00359B58 */
    case 0x80: *v0 = GT4_U32(cond + 0x10148u + a1 * 4); return 1;        /* func_003F5750 */
    case 0x18: race_positions(cond); return 1;                           /* func_00365BC0 */
    case 0x10: race_order_reset(cond); return 1;                         /* func_00365D80 */
    case 0x58: case 0x60: *v0 = 0; return 1;                             /* func_0034D488, func_00365470 */
    case 0x78: race_clock(cond); return 1;                               /* func_00357E80 */
    case 0xc8: return gt4_phys_headless;                                 /* func_003F59A0: the command into the replay */
    /* headless: without the presentation's part (events, ghost, records) */
    case 0x98: return gt4_phys_headless;                                 /* func_003F5780 */
    case 0xa0: if (!gt4_phys_headless) return 0; tt_start(cond, a1, a2); return 1;
    case 0xa8: if (!gt4_phys_headless) return 0; tt_lap(cond, a1, t0, t1); return 1;
    case 0xb8: if (!gt4_phys_headless) return 0; *v0 = tt_finish(cond, a1, t0, t1); return 1;
    case 0x70:                                                           /* func_003F5EB0 */
        if (!gt4_phys_headless) return 0;
        {
            uint32_t car = car_at(cond, 0);
            int16_t n = (int16_t)GT4_U16(car + 0x694);        /* func_003F37C0 */
            if (n > 0) {
                GT4_U16(car + 0x694) = (uint16_t)(n - 1);
                if (n - 1 == 0) gt4_car_pit_leave(car);
            }
        }
        return 1;
    case 0x140: *v0 = 0xff; return 1;                                    /* func_00365478: no car against car */
    case 0x68: gt4_walls(cond, 0); return 1;                             /* func_0034DB10 */
    case 0x108: *f0 = car_distance(cond, a1); return 1;                  /* func_003585F0 */
    case 0x118: *v0 = (uint32_t)(int32_t)(int16_t)GT4_U16(cond + 0xf86cu + a1 * 2); return 1;   /* func_0034D030 */
    case 0x120:                                                          /* func_0034D048 */
        *v0 = GT4_U8(cond + a1 + 0xf866u);
        if (!*v0) *v0 = GT4_U8(cond + 0xf818u + 0x4e);
        return 1;
    case 0x130: *f0 = course_value(course_of(cond), 0); return 1;       /* func_0034C318 */
    case 0x138: *f0 = course_value(course_of(cond), 1); return 1;       /* func_0034C450 */
    case 0x150: {                                                        /* func_0034C250 */
        int32_t n = (int32_t)course_entry(course_of(cond), 0) + 1;
        *v0 = (uint32_t)(n < 17 ? n : 16);
        return 1;
    }
    case 0x158: {                                                        /* func_0034C288 */
        int32_t n = (int32_t)gt4_session(cond, 0x150, 0, 0, 0, 0, 0, NULL);
        *f0 = n - 2 < (int32_t)a1 ? -1.0f : course_entry_value(course_of(cond), a1, 0);
        return 1;
    }
    case 0x188: off_course(cond, a1, a2, a3); return 1;                  /* func_0035FA98 */
    case 0xe8:                                                           /* func_003F5638 */
        start_hold(cond, a1, a2, a3, t0, t1);
        if (GT4_U32(t1)) GT4_U32(t1) = 2;
        return 1;
    case 0xf0:                                                           /* func_003F5670 */
        if (!start_rolling(cond, a2, a3, t0)) gt4_session(cond, 0xf8, a1, a2, a3, t0, 0, NULL);
        return 1;
    case 0xf8: {                                                         /* func_0034D328 */
        float back = 400.0f, tight = 350.0f;
        uint32_t m;
        if (start_given(cond, a1, a2, a3, t0) || start_rolling(cond, a2, a3, t0)) return 1;
        GT4_U32(a2) = 4;
        m = race_mode(cond);
        if (m >= 2 && m < 4) {
            back = 300.0f;
            tight = 200.0f;
        }
        GT4_U32(a3) = (uint32_t)((int32_t)gt4_line_tight_corner(cond + 0xda50u, -back, tight) - 5);
        return 1;
    }
    }
    return 0;
}

/* func_003578B0: car idx passed timing point `point` on lap `lap` at race
   time `time`. Its split since the last point, and its best there (with
   the lap), are kept; each point keeps the times the cars passed it on
   the lap the leader is on, in order (for the gaps). A car laps behind
   is a race event (func_0035E8E8, skipped headless). self: the session object
   the method gets. */
#define SPLIT_B 0xcu                  /* the tables below are from self+0xC */
#define LAST_TIME(i)    (self + SPLIT_B + 0xd600u + (uint32_t)(i) * 4)
#define POINT_LAP(p)    (self + SPLIT_B + 0xd3e0u + (uint32_t)(p) * 2)
#define POINT_COUNT(p)  (self + SPLIT_B + 0xd400u + (uint32_t)(p))
#define POINT_FIRST(p)  (self + SPLIT_B + 0xd440u + (uint32_t)(p))
#define POINT_TIME(p, k) (self + SPLIT_B + 0xcde0u + (uint32_t)(p) * 24 + (uint32_t)(k) * 4)
#define POINT_CAR(p, k) (self + SPLIT_B + 0xd480u + (uint32_t)(p) * 6 + (uint32_t)(k))
#define SPLIT(i, p)     (self + 4 + 0xd620u + ((uint32_t)(i) * 16 + (uint32_t)(p)) * 4)
#define BEST_SPLIT(i, p) (self + 4 + 0xd7a0u + ((uint32_t)(i) * 16 + (uint32_t)(p)) * 4)
#define BEST_LAP(i, p)  (self + 4 + 0xd920u + ((uint32_t)(i) * 16 + (uint32_t)(p)) * 2)
static void race_split(uint32_t self, uint32_t idx, uint32_t lap, uint32_t point, uint32_t time)
{
    uint32_t d = time - GT4_U32(LAST_TIME(idx));
    int32_t lead = (int16_t)GT4_U16(POINT_LAP(point)), at;
    GT4_U32(SPLIT(idx, point)) = d;
    GT4_U32(LAST_TIME(idx)) = time;
    if (d < GT4_U32(BEST_SPLIT(idx, point))) {
        GT4_U32(BEST_SPLIT(idx, point)) = d;
        GT4_U16(BEST_LAP(idx, point)) = (uint16_t)lap;
    }
    if (lead < (int32_t)lap) {                      /* the first car on this lap here */
        GT4_U16(POINT_LAP(point)) = (uint16_t)lap;
        GT4_U8(POINT_COUNT(point)) = 1;
        GT4_U8(POINT_FIRST(point)) = 0;
        GT4_U32(POINT_TIME(point, 0)) = time;
        GT4_U8(POINT_CAR(point, 0)) = (uint8_t)idx;
        return;
    }
    if ((int32_t)lap != lead) {                     /* laps behind: a race event */
        gt4_race_behind(self, idx, (uint32_t)(lead - (int32_t)lap));   /* func_0035E8E8 */
        return;
    }
    {
        int32_t n = (int8_t)GT4_U8(POINT_COUNT(point)), first = (int8_t)GT4_U8(POINT_FIRST(point));
        at = n;
        if (n != first) {
            if (first < n && time < GT4_U32(POINT_TIME(point, n - 1))) {
                at = n - 1;
                while (first < at && time < GT4_U32(POINT_TIME(point, at - 1))) at--;
            }
            /* the later times move up one (the cars' numbers stay) */
            for (int32_t k = (int8_t)GT4_U8(POINT_COUNT(point)); at < k; k--)
                GT4_U32(POINT_TIME(point, k)) = GT4_U32(POINT_TIME(point, k - 1));
        }
        GT4_U32(POINT_TIME(point, at)) = time;
        GT4_U8(POINT_CAR(point, at)) = (uint8_t)idx;
        GT4_U8(POINT_COUNT(point)) = (uint8_t)(GT4_U8(POINT_COUNT(point)) + 1);
    }
}

/* A gap for the screen (func_0035E8D0, func_0035E8B8: func_0035E078) */
static void gap_event(uint32_t cond, uint32_t car, uint32_t gap)
{
    gt4_race_gap(GT4_U32(cond), car, gap);
}

/* func_00359238: at each timing point, the cars that passed it since the
   last frame get their gaps: to the leader, once the car has done enough
   laps (func_0035E8D0), and the second car's gap back to the leader
   (func_0035E8B8). The other hooks there (func_0035E898 ...) do nothing. */
static void race_gaps(uint32_t self)
{
    for (uint32_t p = 0; (int32_t)p < (int32_t)gt4_session(self, 0x150, 0, 0, 0, 0, 0, NULL); p++) {
        int32_t first = (int8_t)GT4_U8(POINT_FIRST(p)), n = (int8_t)GT4_U8(POINT_COUNT(p));
        if (first == n) continue;
        if (first < n) {
            for (int32_t k = first; k < (int8_t)GT4_U8(POINT_COUNT(p)); k++) {
                uint32_t car = (uint32_t)(int32_t)(int8_t)GT4_U8(POINT_CAR(p, k)), t;
                if (k == 0) continue;
                t = GT4_U32(POINT_TIME(p, k));
                if (!(k + 1 < (int8_t)GT4_U8(car_at(self, car) + 0x5b3)))
                    gap_event(self, car, t - GT4_U32(POINT_TIME(p, 0)));
                if (k == 1 && (int8_t)GT4_U8(car_at(self, car) + 0x5b3) == 2)
                    gap_event(self, (uint32_t)(int32_t)(int8_t)GT4_U8(POINT_CAR(p, 0)),
                              GT4_U32(POINT_TIME(p, 0)) - t);
            }
        }
        GT4_U8(POINT_FIRST(p)) = GT4_U8(POINT_COUNT(p));
    }
}

/* func_00357B98: car idx finished lap `lap` at race time `time` (t: the
   extra word the lap code passes). The lap's race event (func_0035E550)
   and the finish event (func_0035DF80) are the presentation's, skipped
   headless. The car is through when the leader already is, when this was
   the last lap, or, past the time limit (minutes at +0xF850), when it
   leads; then it drives on as finished (func_00343D78). */
static void race_lap(uint32_t cond, uint32_t idx, uint32_t lap, uint32_t time, uint32_t t)
{
    uint32_t car = car_at(cond, idx), laps, limit;
    int done;
    gt4_race_lap_event(cond, idx, lap, t, GT4_U8(car + 0x5b7));        /* func_0035E550 */
    laps = gt4_session(cond, 0x128, 0, 0, 0, 0, 0, NULL);
    done = GT4_U8(car_at(cond, GT4_U32(cond + 0xf8a4u)) + 0x5b6) != 0;   /* func_003658D8: the leader */
    if (laps && lap == laps) done = 1;
    limit = (uint32_t)GT4_U16(cond + 0xf850u) * 60000u;                 /* func_0034C158, func_0034C140 */
    if (limit && !(time < limit) && GT4_U32(cond + 0xf8a4u) == idx) done = 1;
    if (done) {
        gt4_race_event(GT4_U32(cond), 5, idx, 2);                         /* func_0035DF80 */
        gt4_car_finish_drive(car_at(cond, idx), 3);
        if (!idx && GT4_U8(cond + 0xcbdcu) == 4) GT4_U16(cond + 0xcbf4u) = 3 * 60 + 1;   /* func_0034C0A0 */
    }
    GT4_U8(car + 0x104 + 0x4b6) = 0;                                    /* func_00363528 */
    GT4_U8(car + 0x104 + 0x4b3) = 0;
    GT4_U32(car + 0x104 + 0x614) = time;
}

/* func_00347858 (the Arcade's method 0x58): in an arcade race, the
   catch-up settings in block b (cond+0xF818) by the race's level
   (race+8, func_00354F30): 1 or 2 set them, others clear them; returns
   whether there is a level (1 outside arcade races). */
static int world_is_arcade(uint32_t race);
static uint32_t catch_up_setting(uint32_t b, uint32_t level)
{
    static const uint8_t set[2][9] = {   /* +0x2C..+0x34 */
        { 8, 0x28, 0xc, 1, 5, 4, 0, 5, 5 },
        { 6, 0x28, 0xc, 1, 5, 3, 0, 4, 5 },
    };
    if (level == 1 || level == 2) {
        for (uint32_t k = 0; k < 9; k++) GT4_U8(b + 0x2c + k) = set[level - 1][k];
    } else {
        for (uint32_t k = 0; k < 9; k++)
            if (k != 6) GT4_U8(b + 0x2c + k) = 0;   /* +0x32 kept */
    }
    return level != 0;
}
static uint32_t race_catch_up(uint32_t cond,uint32_t b)
{
    uint32_t r=GT4_U32(cond);
    return world_is_arcade(r)?catch_up_setting(b,GT4_U32(r+8)):1;
}

/* func_003C0E78: whether the race is a replay (the world's virtual
   +0x298; 0 for the race worlds seen so far) */
static int race_replay(uint32_t race)
{
    uint32_t world = GT4_U32(race + 0x84);
    if (!world) return 0;
    return gt4_vcall(world, 0x64, 0x298, 0, 0, 0) != 0;
}

/* func_0034CE48 (the Arcade's method 0xE8, through func_00347838): how
   car i is held at the start (t0): a player's car 0, or 2 in race mode 4,
   or 1 when the world says so (func_003F1760); another's 2, or 4 in a
   replay. Only a car at 0 or 1 gets its entry's +0x3448 (a3). */
static void race_hold(uint32_t cond, uint32_t i, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1)
{
    uint32_t race = GT4_U32(cond), entry = GT4_U32(GT4_U32(GT4_U32(race + 0x60) + 8) + i * 4);
    if (GT4_U32(GT4_U32(entry + 0x343c) + 0x1cc)) {
        GT4_U32(t0) = 0;
        if (GT4_U8(cond + 0xcbdcu) == 4) GT4_U32(t0) = 2;
        else {
            uint32_t k = GT4_U32(GT4_U32(race + 0x84) + 0xcc8);   /* func_003F1760 */
            if (k == 1 || k == 3) GT4_U32(t0) = 1;
        }
    } else {
        GT4_U32(t0) = 2;
        if (race_replay(race)) GT4_U32(t0) = 4;
    }
    GT4_U32(a2) = 0;
    GT4_U32(a3) = GT4_U32(t0) < 2 ? GT4_U32(entry + 0x3448) : 0;
    GT4_U32(t1) = 0;
}

/* func_00347618 (the Arcade's method 0xF0): where car i starts. A start
   the session gives (+0x118 distance, +0x120 speed: kind 1 ahead of the
   line, 2 behind, the distance a lap on when laps count); else the parts
   of it there are (func_0034D070, func_0034D0F8); else, with the course's
   rolling start (cond+0xF884 metres back, +0xF886 speed, +0xF887 metres
   between cars), the grid rolling in slot order. */
static void race_start_at(uint32_t cond, uint32_t i, uint32_t a2, uint32_t a3, uint32_t t0)
{
    int32_t d = (int32_t)gt4_session(cond, 0x118, i, 0, 0, 0, 0, NULL), v = (int32_t)gt4_session(cond, 0x120, i, 0, 0, 0, 0, NULL);
    int32_t back, speed, gap;
    uint32_t race = GT4_U32(cond), entry;
    if (d && v > 0) {
        GT4_U32(a2) = d > 0 ? 1 : 2;
        GT4_U32(a3) = (uint32_t)d;
        GT4_U32(a3) = GT4_U32(a3) + (uint32_t)gt4_session_laps(cond);
        GT4_U32(t0) = (uint32_t)v;
        return;
    }
    d = (int32_t)gt4_session(cond, 0x118, i, 0, 0, 0, 0, NULL);   /* func_0034D070 */
    if (d) {
        GT4_U32(a2) = 1;
        GT4_U32(a3) = (uint32_t)d;
        GT4_U32(a3) = GT4_U32(a3) + (uint32_t)gt4_session_laps(cond);
    }
    v = (int32_t)gt4_session(cond, 0x120, i, 0, 0, 0, 0, NULL);   /* func_0034D0F8 */
    if (v > 0) {
        GT4_U32(t0) = (uint32_t)v;
        return;
    }
    if (GT4_U32(a3)) return;
    back = (int16_t)GT4_U16(cond + 0xf884u);
    speed = GT4_U8(cond + 0xf886u);
    gap = GT4_U8(cond + 0xf887u);
    if (back >= 0 || speed <= 0) return;
    entry = GT4_U32(GT4_U32(GT4_U32(race + 0x60) + 8) + i * 4);
    back += (int32_t)(GT4_U32(cond + 0xf8a0u) - GT4_U32(entry + 0x3444) - 1u) * gap;
    GT4_U32(a2) = back >= 0 ? 1 : 2;
    GT4_U32(a3) = (uint32_t)back;
    GT4_U32(t0) = (uint32_t)speed;
}

/* A race against the other cars (the Arcade race's table); 0 for the
   methods not rewritten. Some are the Time Trial's own functions. */
/* func_00347928 (the Arcade's method 0xA0): car i crossed the start: a car
   still on the pit line (+0x56B 1) leaves it, its pit count cleared
   (func_003F37B8), and for car 0 race event 4; the rolling start's flag
   cleared (cond+0xCBE4) */
static void race_start_line(uint32_t cond, uint32_t i)
{
    uint32_t car = car_at(cond, i);
    if (GT4_U8(car + 0x56b) == 1) {
        gt4_car_pit_leave(car);                                          /* func_00345228 */
        GT4_U16(car + 0x694) = 0;
        if (!i) gt4_race_event(GT4_U32(cond), 4, 0, 0);
    }
    GT4_U8(cond + 0xcbe4u) = 0;
}

/* func_00357D00 (through func_003479B8, the Arcade's method 0xB8): car i
   crossed the course's finish line on lap `lap` at `time` (in frames/3),
   the lap in `laptime`. Only where the race ends there (cond+0xCBDE 1, or
   +0xF81A 2): the lap's events, the finish event, the car driven on as
   finished (func_00343D78), the last seconds' count for car 0
   (func_0034C0A0), its lap state cleared (func_00363528) and its lap start
   time. 1 when it counted. */
static uint32_t race_finish_line(uint32_t cond, uint32_t i, uint32_t lap, uint32_t time, uint32_t laptime)
{
    uint32_t car;
    if (GT4_U8(cond + 0xcbdeu) != 1 && GT4_U8(cond + 0xf81au) != 2) return 0;
    car = car_at(cond, i);
    gt4_race_lap_event(cond, i, lap, laptime, GT4_U8(car + 0x5b7));
    gt4_race_event(GT4_U32(cond), 5, i, 2);
    gt4_car_finish_drive(car, 3);
    if (!i && GT4_U8(cond + 0xcbdcu) == 4) GT4_U16(cond + 0xcbf4u) = 3 * 60 + 1;
    GT4_U8(car + 0x104 + 0x4b6) = 0;
    GT4_U8(car + 0x104 + 0x4b3) = 0;
    GT4_U32(car + 0x104 + 0x614) = time;
    return 1;
}

static int race(uint32_t cond, uint32_t off, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1,
                uint32_t t2, uint32_t *v0, float *f0)
{
    uint32_t e = GT4_U32(cond + SESSION_VT) + off, self = cond + (uint32_t)(int32_t)(int16_t)GT4_U16(e);
    switch (off) {
    case 0x30: case 0x80: case 0x160: *v0 = 0; return 1;                /* func_00358248, func_00358260, func_003479F0 */
    case 0x50:                                                       /* func_003475B8 +0x0c..0x4c */
        *v0 = GT4_U8(cond + 0xf866u) == 0;
        if (*v0 == 1 && (int16_t)GT4_U16(cond + 0xf884u) < 0)
            *v0 = GT4_U8(cond + 0xf886u) == 0;
        return 1;
    case 0x180: *v0 = 1; return 1;                                       /* func_0035FA48 */
    case 0x70: case 0xc8: case 0x188: return 1;                          /* func_00347908, func_00349FC8, func_003479F8 */
    case 0x78: race_clock(cond); return 1;                               /* func_00347910 */
    case 0x128: *v0 = GT4_U32(GT4_U32(self)); return 1;                  /* func_003577F0 */
    case 0x140: *v0 = GT4_U8(cond + 0xf83au); return 1;                  /* func_003478B8: car against car */
    case 0x68: gt4_walls_cars(cond); return 1;                           /* func_003478C8 */
    case 0x08: race_gaps(self); return 1;                                /* func_003479D0 */
    case 0xa8: race_lap(self, a1, a2, t0, t1); return 1;                 /* func_00357B98 */
    case 0x90: race_split(self, a1, a2, a3, t0); return 1;               /* func_003478E8 */
    case 0x98:                                                           /* func_00357B10 */
        gt4_race_split_event(self, a1, a2 - 1, a3, t2, GT4_U8(car_at(self, a1) + 0x5b7));
        return 1;
    case 0xa0: race_start_line(self, a1); return 1;                      /* func_00347928 */
    case 0xb8: *v0 = race_finish_line(self, a1, a2, t0, t1); return 1;   /* func_003479B8 */
    case 0x58: *v0 = race_catch_up(cond, a1); return 1;                 /* func_00347858 */
    case 0x60: *v0 = 1; return 1;                                        /* func_003478B0 */
    case 0xe8: race_hold(cond, a1, a2, a3, t0, t1); return 1;            /* func_00347838 */
    case 0xf0: race_start_at(cond, a1, a2, a3, t0); return 1;            /* func_00347618 */
    case 0x1a0: *v0 = 1; return 1;                                       /* func_003475B0 */
    case 0x10: case 0x18: case 0x38: case 0x40: case 0x48: case 0xb0: case 0xc0: case 0xf8: case 0x108:
    case 0x118: case 0x120: case 0x130: case 0x138: case 0x150: case 0x158:
        return time_trial(cond, off, a1, a2, a3, t0, t1, v0, f0);        /* the same functions */
    }
    return 0;
}

/* DynamicsConductorBattle2P (00679988, ctor 003470B0). This is
   session integration: existing dynamics and collision solvers are reused. */
static int battle(uint32_t c,uint32_t off,uint32_t a1,uint32_t a2,uint32_t a3,
                  uint32_t t0,uint32_t t1,uint32_t t2,uint32_t *v,float *f)
{
    uint32_t r=GT4_U32(c);
    switch(off){
    case 0x168:case 0x170:case 0x178:case 0x198:case 0x1a0:
        *v=0;return 1; /* 0035FA30/38/40, 0035AF10, 0034C098 */
    case 0x50:*v=GT4_U8(c+0xf866)==0;return 1; /* 0034C1B0 +8..10 */
    case 0x58:*v=catch_up_setting(a1,GT4_U32(r+0x14));return 1; /* 003471A8 */
    case 0x60:{ /* 00347188 -> 00354EB8 +0..70: original wear selector */
        uint32_t k=GT4_U32(r+0x1c);*v=k==1 || k==2;
        GT4_U8(a1+0x22)=*v?0:0xff;GT4_U8(a1+0x2a)=*v?20:0;
        GT4_U8(a1+0x28)=k==1?7:k==2?20:0;
        GT4_U8(a1+0x29)=k==1?20:k==2?10:0;return 1;
    }
    case 0x70:case 0xa0:return 1; /* 00357E00: no replay clock; 00357B90 noop */
    case 0xa8:{ /* 00347230 +20..F4: first finisher ends the battle */
        uint32_t car=car_at(c,a1),laps=GT4_U32(r);
        gt4_race_lap_event(c,a1,a2,t1,GT4_U8(car+0x5b7));
        if(GT4_U8(car_at(c,GT4_U32(c+0xf8a4))+0x5b6) || (laps && a2==laps)){
            gt4_race_event(r,5,a1,2);gt4_car_finish_drive(car,3);
        }
        GT4_U8(car+0x5ba)=GT4_U8(car+0x5b7)=0;GT4_U32(car+0x718)=t0;return 1;
    }
    case 0xf0:{ /* 00347108 +1C..60; tail 0034D0F8 only supplies speed */
        int32_t h=(int32_t)GT4_U32(r+0x18);
        if(h){int32_t one,two;
            if(h>0){one=-h;if(one>=-19)one=-20;two=-10;}
            else{one=-10;two=h;if(two>=-19)two=-20;}
            GT4_U32(a2)=1;GT4_U32(a3)=(uint32_t)(a1?one:two);
        }
        int32_t speed=(int32_t)gt4_session(c,0x120,a1,0,0,0,0,NULL);
        if(speed>0)GT4_U32(t0)=(uint32_t)speed;return 1;
    }
    default:return race(c,off,a1,a2,a3,t0,t1,t2,v,f);
    }
}

/* DynamicsConductorMachineTest (00685BF0, 003F6958): GT Mode's Power &
   Speed (world 00680EE0, 003BDD60, mode 15). The test is the race's laps
   word (003577F0 -> 003F69F8: below 4, else 0): 1 the 400 m, 2 the
   1000 m, 0 / 3 the top speed (003F6A38: its count of marks); its marks
   D_00622490: 400, 1000, 6845 m (-1 past them). No laps counted; at the
   end each car's top speed (car +0x6CC) is kept and shown. */
#define MACHINE_TEST_VT 0x00685bf0u
static int machine_kind(uint32_t c){int32_t l=(int32_t)GT4_U32(GT4_U32(c));return l<4?l:0;}
static int machine_marks(uint32_t c){int k=machine_kind(c);return k==1?1:k==2?2:k==0 || k==3?3:1;}
static float machine_mark(int i){static const uint32_t m[4]={0x43c80000u,0x447a0000u,0x45d5e800u,0xbf800000u};return K(m[i]);}
/* func_003584C8: the course distance of where the car will be (its place
   plus its velocity times the step at car +0x10) */
static float machine_distance(uint32_t cond,uint32_t car)
{
    uint32_t b=car+0x104,id=GT4_U32(b+0x4a8);float f=GT4_F32(GT4_U32(car+0x10)),p[3];
    float x=GT4_F32(b+0xc)*f,y=GT4_F32(b+0x10)*f,z=GT4_F32(b+0x14)*f;
    x=GT4_F32(b)+x;y=GT4_F32(b+4)+y;z=GT4_F32(b+8)+z;
    p[0]=x;p[1]=z;p[2]=-y;
    return gt4_course_distance_p(course_of(cond),p,&id);
}
/* func_0035E7C0: car i's top speed at its end: the entry's +0x3464 when
   the race keeps times, and message 0x13 (the car, the speed in the
   option's unit x10 up to 9999, 003A36E0) */
static void machine_top_speed(uint32_t c,uint32_t i,float speed)
{
    uint32_t car=car_at(c,i),b=car+0x104,r=GT4_U32(c);
    if(GT4_U8(b+0x441) || GT4_U8(b+0x632))return;
    if(!(GT4_U8(c+0xcbdcu) || GT4_U32(r+0xec) || GT4_U32(r+0xdc)))
        GT4_F32(GT4_U32(GT4_U32(GT4_U32(r+0x60)+8)+i*4)+0x3464)=speed;
    if(GT4_U32(r+0xdc))return;
    uint32_t unit=GT4_U32(0x006244d8u);float v=speed;   /* 00472940 */
    if(unit==0)v=speed*K(0x40666666u);
    else if(unit==1){v=speed*K(0x40666666u);v=v*K(0x3f1f136au);}
    v=v*10.f;int32_t n=(int32_t)v;if(n<0)n=0;if(n>9999)n=9999;
    uint32_t word=(i>15?15u:i)|((uint32_t)n&0x3fffu)<<4;
    gt4_race_queue_push(r+0xf4,0x13,word,2);
}
static int machine(uint32_t c,uint32_t off,uint32_t a1,uint32_t a2,uint32_t a3,
                   uint32_t t0,uint32_t t1,uint32_t t2,uint32_t *v,float *f)
{
    uint32_t r=GT4_U32(c),car;
    switch(off){
    case 0x08:case 0x70:case 0x90:case 0xa0:case 0xa8:return 1; /* 00359230, 00357E00, 00357B08, 00357B90, 003F6C28 */
    case 0x10:race_order_reset(c);return 1; /* 00365D80 */
    case 0x18:race_positions(c);return 1;   /* 00365BC0 */
    case 0x30:case 0x38:*v=1;return 1;      /* 003F6990, 003F6998 */
    case 0x50:*v=GT4_U8(c+0xf866)==0;return 1; /* 0034C1B0 */
    case 0x58:case 0x60:case 0x128:case 0x160:case 0x1a0:*v=0;return 1; /* 0034D488, 00365470, 003F69C0, 0035FA28, 0034C098 */
    case 0x68:gt4_walls(c,0);return 1;      /* 0034DB10 */
    case 0x78:race_clock(c);return 1;       /* 00357E80 */
    case 0x98: /* 003F6B68: outside a replay, the split's events */
        if(!GT4_U32(r+0xdc)){car=car_at(c,a1);gt4_race_split_event(c,a1,a2,a3,t2,GT4_U8(car+0x5b7));}
        return 1;
    case 0xb8: /* 003F6C30: past the last mark */
        car=car_at(c,a1);
        if(GT4_F32(car+0x6cc)<GT4_F32(car+0x6c8))GT4_F32(car+0x6cc)=GT4_F32(car+0x6c8);
        if(GT4_U32(r+0xdc)){*v=0;return 1;}
        gt4_car_finish_drive(car,1);
        {int k=machine_kind(c);
         if(!GT4_U8(car+0x104+0x441) && !GT4_U8(c+0xcbdcu) && k>0 && k<3)gt4_session_best(c,a1,t1);   /* 00358000 */
         if(k>0 && k<3)gt4_race_lap_event(c,a1,a2,t1,GT4_U8(car+0x5b7));   /* 0035E550 */
         else if(k==0 || k==3)machine_top_speed(c,a1,GT4_F32(car+0x6cc));}
        if(!a1)gt4_race_event(r,5,0,2);
        *v=1;return 1;
    case 0xe8:start_hold(c,a1,a2,a3,t0,t1);return 1; /* 003F69A0 -> 0034CF30 */
    case 0xf0:{ /* 0034D150 -> 0034D0F8: only the speed the session gives */
        int32_t speed=(int32_t)gt4_session(c,0x120,a1,0,0,0,0,NULL);
        if(speed>0)GT4_U32(t0)=(uint32_t)speed;return 1;
    }
    case 0x108:*f=machine_distance(c,a1);return 1; /* 003F6A20 -> 003584C8 */
    case 0x110: /* 003F69C8: through, its best; else this lap's time (00357800) */
        *v=GT4_U8(a1+0x5b6)?GT4_U32(a1+0x71c):GT4_U32(c+0xf898)/3-GT4_U32(a1+0x718);return 1;
    case 0x138:*f=machine_mark(machine_marks(c)-1);return 1; /* 003F6AA8 */
    case 0x140:*v=0xff;return 1;            /* 00365478 */
    case 0x150:*v=(uint32_t)machine_marks(c);return 1; /* 003F6A38 */
    case 0x158:{int n=machine_marks(c);*f=machine_mark((int32_t)a1<n-1?(int)a1:3);return 1;} /* 003F6B00 */
    case 0x188:off_course(c,a1,a2,a3);return 1; /* 0035FA98 */
    default:return race(c,off,a1,a2,a3,t0,t1,t2,v,f);
    }
}

/* DynamicsConductorFreePractice (006856E0): GT Mode's Practice, the
   other cars on the course with no laps counted and the order by best
   lap. Its own methods; those it shares with the Arcade's table go to
   race(). */
static int practice(uint32_t c,uint32_t off,uint32_t a1,uint32_t a2,uint32_t a3,
                    uint32_t t0,uint32_t t1,uint32_t t2,uint32_t *v,float *f)
{
    uint32_t r=GT4_U32(c),car;
    switch(off){
    case 0x08:case 0x70:case 0x90:case 0x188:return 1; /* 00359230, 00357E00 (a replay's clock), 00357B08, 003F54D8 */
    case 0x10:practice_order_reset(c);return 1; /* 003F5270 */
    case 0x18:practice_positions(c);return 1; /* 003F5350 */
    case 0x50:*v=3;return 1; /* 003F4EF0 */
    case 0x58:case 0x198:*v=1;return 1; /* 003F5240, 003F54D0 */
    case 0x60:case 0x128:case 0x160:case 0x1a0:*v=0;return 1; /* 003F5248, 003F5008 (no laps), 003F54C8, 0034C098 */
    case 0x68:gt4_walls_cars(c);return 1; /* 003F5250 -> 0034DA70 */
    case 0x78:race_clock(c);return 1; /* 00357E80 */
    case 0x98: /* 003F5028: the gap to the race's best split, then 0035E428 */
        car=car_at(c,a1);
        if(!GT4_U8(car+0x5b7) && GT4_U32(r+0x928+a3*4)!=0x157529ffu)gt4_race_gap(r,a1,t2-GT4_U32(r+0x928+a3*4));
        gt4_race_split_event(c,a1,a2,a3,t2,GT4_U8(car+0x5b7));return 1;
    case 0xa0: /* 003F50F0: car 0's message 3 at its first crossing, its lap start */
        if(!a1)gt4_race_pit_message(r,0,3);
        GT4_U32(car_at(c,a1)+0x104+0x614)=a2;return 1;
    case 0xa8:{ /* 003F5150: the gap to the race's best lap (race +0x968), message 2
                   when this lap beats it, the lap's events, its state and start */
        uint32_t best;car=car_at(c,a1);
        if(!GT4_U8(car+0x5b7)){
            best=GT4_U32(r+0x968);
            if(best!=0x157529ffu)gt4_race_gap(r,a1,t1-best);
            if(t1<best)gt4_race_pit_message(r,a1,2);
        }
        gt4_race_lap_event(c,a1,a2,t1,GT4_U8(car+0x5b7)); /* 0035E550 */
        GT4_U8(car+0x104+0x4b6)=0;GT4_U8(car+0x104+0x4b3)=0; /* 00363528 */
        GT4_U32(car+0x104+0x614)=t0;return 1;
    }
    case 0xb8:*v=race_finish_line(c,a1,a2,t0,t1);return 1; /* 00357D00 */
    case 0xe8:race_hold(c,a1,a2,a3,t0,t1);return 1; /* 003F4EF8 -> 0034CE48 */
    case 0xf0: /* 003F4F10: car 0 from the pit (6); the others 5, 300 m back and 200 m apart, at 120 */
        if(!a1){GT4_U32(a2)=6;GT4_U32(a3)=0;GT4_U32(t0)=0;}
        else{GT4_U32(a2)=5;GT4_U32(a3)=(uint32_t)-(int32_t)(a1*200+300);GT4_U32(t0)=0x78;}
        return 1;
    case 0x140:*v=0xff;return 1; /* 00365478: no car against car */
    default:return race(c,off,a1,a2,a3,t0,t1,t2,v,f);
    }
}

/* func_0035FCC8 (method 0x190 of both tables): the Arcade's collision
   penalty. A hard enough knock on a car (kind 1, force x 100) or a wall
   (kind 0, force over the car's mass), on contact detail of class 1..3
   (func_0035FBC8), against the thresholds at cond+0xF860/0xF861 (0 always,
   255 never), holds the car for the penalty's seconds (cond+0xF862:
   car+0x712 counts 60 per second; 255 a flag at car+0x732 instead). */
static int penalty_class(uint32_t detail)
{
    static const uint8_t cls[16] = { 2, 1, 1, 2, 3, 4, 4, 3, 1, 1, 1, 3, 4, 4, 4, 3 };
    return detail < 16 ? cls[detail] : 4;
}

static int penalty_hard(uint32_t threshold, float f)   /* func_0035FC10 */
{
    if (!threshold) return 1;
    if (threshold == 0xff) return 0;
    return (int32_t)f < (int32_t)threshold ? 0 : 1;
}

static void penalty(uint32_t cond, uint32_t car, uint32_t kind, uint32_t detail, float force)
{
    uint32_t b = car + 0x104;
    uint8_t secs;
    int hit = 0;
    if ((int8_t)GT4_U8(b + 0x466) || GT4_U8(b + 0x467) || GT4_U8(b + 0x4b2)) return;
    secs = GT4_U8(cond + 0x10000u - 0x79eu);
    if (!secs) return;
    if (secs == 0xff) {
        if (GT4_U8(b + 0x62e)) return;
    } else if (GT4_U16(b + 0x60e)) {
        return;
    }
    if (kind == 1) {
        if (penalty_class(detail) < 3) hit = penalty_hard(GT4_U8(cond + 0x10000u - 0x7a0u), force * K(0x42c80000u));
    } else if (kind == 0) {
        if (penalty_class(detail) < 3)
            hit = penalty_hard(GT4_U8(cond + 0x10000u - 0x79fu), force / GT4_F32(GT4_U32(car + CAR_PARAMS) + 0x48));
    }
    if (!hit) return;
    if (secs == 0xff) GT4_U8(b + 0x62e) = 1;
    else GT4_U16(b + 0x60e) = (uint16_t)(secs * 60);
}

/* License session, table 00685A40. Common methods below still use their
   existing native implementation; only this class's overrides live here. */
static int license(uint32_t c,uint32_t off,uint32_t a1,uint32_t a2,uint32_t a3,
                   uint32_t t0,uint32_t t1,uint32_t t2,uint32_t *v,float *f)
{
    uint32_t car,point,laps,n;int kind=GT4_U8(c+0xf81a);
    switch(off){
    case 0x30: case 0x170: *v=1;return 1; /* 003F5FA0, 003F6908 */
    case 0x38: case 0x50: *v=GT4_U8(c+0xf866)==0;return 1; /* 003F60F8/90 */
    case 0x40: *v=kind==3 && GT4_U8(c+0xf819)!=1;return 1; /* 003F6110 */
    case 0x48: /* 003F6140: timing-point index over the complete test */
        *v=GT4_U8(a1+0x5b8);
        if(gt4_session(c,0x40,0,0,0,0,0,NULL))
            *v+=((uint32_t)(int32_t)(int16_t)GT4_U16(a1+0x5b0)-1)*(course_entry(course_of(c),1)+1)-(a2!=0);
        return 1;
    case 0x70: case 0x88: case 0xc8: return 1; /* 003F6878,00358268,00349FC8 */
    case 0x78: /* 003F6880 */
        for(uint32_t i=0;(int32_t)i<(int32_t)GT4_U32(c+0xf8a0);i++)gt4_license_evaluate(c,car_at(c,i));
        race_clock(c);return 1;
    case 0x80: case 0x1a0: case 0x180: *v=0;return 1; /* 00358260,0034C098,003F6930 */
    case 0x118: *v=(uint32_t)(int32_t)(int16_t)GT4_U16(c+0xf86c);return 1; /* 003F6070 */
    case 0x120: *v=GT4_U8(c+0xf866);return 1; /* 003F6080 */
    case 0x128: *v=GT4_U32(GT4_U32(c));return 1; /* 003F60D0 */
    case 0x160: *v=3;return 1; /* 003F68F8 */
    case 0x168: *v=2;return 1; /* 003F6900 */
    case 0x178: *v=race_mode(c)!=2 && race_mode(c)!=3;return 1; /* 003F6910 */
    case 0xe8: start_hold(c,a1,a2,a3,t0,t1);return 1; /* 003F6050 */
    case 0xf0: /* 003F6278 -> 0034D168/0034D070/0034D0F8 */
        if(start_given(c,a1,a2,a3,t0))return 1;
        *v=gt4_session(c,0x118,a1,0,0,0,0,NULL);
        if(*v){GT4_U32(a2)=1;GT4_U32(a3)=*v+(uint32_t)gt4_session_laps(c);}
        *v=gt4_session(c,0x120,a1,0,0,0,0,NULL);
        if((int32_t)*v>0)GT4_U32(t0)=*v;
        return 1;
    case 0x110: /* 003F61F8 -> 00357800/30 */
        if(GT4_U8(a1+0x5b6))*v=GT4_U32(a1+0x71c);
        else if(GT4_U8(c+0xf866) && (!(int16_t)GT4_U16(a1+0x5b0) || GT4_U8(a1+0x75f)==1))*v=0;
        else *v=GT4_U32(c+0xf898)/3-GT4_U32(a1+0x718);
        return 1;
    case 0xa0: case 0xb0: /* 003F64D0/003F6760 */
        car=car_at(c,a1);
        if(off==0xb0)GT4_U8(car+0x75f)=0;
        if(!GT4_U8(car+0x75f) && GT4_U8(c+0xf866)){
            GT4_U32(car+0x718)=a2;gt4_car_pit_leave(car);
            if(!a1)gt4_race_event(GT4_U32(c),4,0,0);
        }return 1;
    case 0xb8: /* 003F67E8 */
        *v=kind==2;
        if(*v){car=car_at(c,a1);gt4_license_finish(c,car,t1);GT4_U32(car+0x718)=t0;}
        return 1;
    case 0x98: case 0xa8: /* 003F63A0 / 003F6588 */
        car=car_at(c,a1);laps=GT4_U32(GT4_U32(c));
        if(off==0xa8 && (kind==3 || kind==6) && laps==a2){
            gt4_license_finish(c,car,t1);GT4_U32(car+0x718)=t0;return 1;
        }
        if(off==0xa8 && kind==5){
            if(laps==a2){gt4_car_finish_drive(car,3);GT4_U32(car+0x718)=t0;}return 1;
        }
        n=gt4_session(c,0x150,0,0,0,0,0,NULL);
        int fits=(int32_t)((laps?laps:1)*n)<17; /* 003F6320 */
        if(off==0x98 && !fits)return 1;
        point=off==0x98?(a2-1)*n+a3:fits?a2*n-1:a2-1;
        if((int32_t)point>=16 || (GT4_U8(c+0xf858) && kind!=6))return 1;
        uint32_t elapsed=off==0x98?t2:t1,r=GT4_U32(c);
        /* 0035E6B0: gap against this point of the reference attempt. */
        if(!GT4_U8(c+0xcbdc) && !GT4_U8(car+0x5b7) && GT4_U32(r+0x928+point*4)!=0x157529ffu)
            gt4_race_gap(r,a1,elapsed-GT4_U32(r+0x928+point*4));
        gt4_race_split_event(c,a1,off==0x98?1:a2,point,elapsed,GT4_U8(car+0x5b7));return 1;
    case 0x08: case 0x10: case 0x18: case 0x58: case 0x60: case 0x68:
    case 0x90: case 0xc0: case 0xf8: case 0x108: case 0x130: case 0x138:
    case 0x140: case 0x150: case 0x158: case 0x188:
        return time_trial(c,off,a1,a2,a3,t0,t1,v,f);
    }
    return 0;
}

uint32_t gt4_session_x(uint32_t cond, uint32_t off, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1,
                       uint32_t t2, float f12, float *f0)
{
    uint32_t e = GT4_U32(cond + SESSION_VT) + off, v0 = 0;
    float f = 0;
    /* 00685F50: RaceTraining inherits the licence conductor. Route its
       distinct virtual methods to the existing native implementations;
       no dynamics arithmetic or car parameters are changed. */
    /* 00685DA0: RaceMission's conductor, the training's but for these */
    if(!gt4_session_orig && GT4_U32(cond+SESSION_VT)==GT4_MISSION_SESSION_VT){
        int routed=1;
        switch(off){
        case 0x08: race_gaps(cond);break; /* 003F6E30 -> 00359238 */
        case 0xa0: /* 003F6E50: the licence's (003F64D0), then car +0x718 cleared */
            license(cond,off,a1,a2,a3,t0,t1,t2,&v0,&f);
            GT4_U32(car_at(cond,a1)+0x718)=0;break;
        case 0xc0: /* 003F6EE8: each car through the line, its finish event once */
            for(uint32_t i=0;(int32_t)i<(int32_t)GT4_U32(cond+0xf8a0);i++){
                uint8_t bit=(uint8_t)(1u<<i);
                if(GT4_U8(car_at(cond,i)+0x5b6) && !(GT4_U8(cond+0xcbe3)&bit)){
                    gt4_race_event(GT4_U32(cond),5,i,2);GT4_U8(cond+0xcbe3)|=bit;
                }
            }
            break;   /* its tail (kind 5, two cars, the first through) only reads */
        case 0x110: /* 003F6ED0 -> 00357800 */
            v0=GT4_U32(cond+0xf898)/3-GT4_U32(a1+0x718);break;
        case 0x160: case 0x168: case 0x170: case 0x178: v0=0;break; /* 003F7010..28 */
        case 0x180: v0=1;break; /* 003F7030 */
        case 0x190: penalty(cond,a1,a2,a3,f12);break; /* 003F7038 -> 0035FCC8 */
        default:routed=0;break;
        }
        if(routed){if(f0)*f0=f;return v0;}
    }
    if(!gt4_session_orig && (GT4_U32(cond+SESSION_VT)==GT4_TRAINING_SESSION_VT || GT4_U32(cond+SESSION_VT)==GT4_MISSION_SESSION_VT)){
        int routed=1;
        switch(off){
        case 0x08: case 0x30: v0=0;break; /* 003F7148,003F7050 */
        case 0x60: v0=1;break; /* 003F70D0 */
        case 0x68: gt4_walls_cars(cond);break; /* 003F7108 -> 0034DA70 */
        case 0x90: race_split(cond,a1,a2,a3,t0);break; /* 003F7128 -> 003578B0 */
        case 0x118: case 0x120:
            routed=time_trial(cond,off,a1,a2,a3,t0,t1,&v0,&f);break; /* 003F7090/A8 */
        case 0x140: v0=GT4_U8(cond+0xf83a);break; /* 003F70D8 */
        case 0xe8:
            race_hold(cond,a1,a2,a3,t0,t1); /* 003F7078 -> 0034CE48 */
            /* 0034CE48 +0x70..0x94 tests any nonzero world+CC8;
               training constructor normally leaves that word zero. */
            if(GT4_U32(t0)==0 && GT4_U32(GT4_U32(GT4_U32(cond)+0x84)+0xcc8))GT4_U32(t0)=1;
            break;
        default:routed=0;break;
        }
        if(routed){if(f0)*f0=f;return v0;}
    }
    if(!gt4_session_orig && (GT4_U32(cond+SESSION_VT)==GT4_LICENSE_SESSION_VT || GT4_U32(cond+SESSION_VT)==GT4_TRAINING_SESSION_VT || GT4_U32(cond+SESSION_VT)==GT4_MISSION_SESSION_VT)){
        if(off==0x190){gt4_license_collision(cond,a1,a2,a3,f12);if(f0)*f0=0;return 0;}
        if(license(cond,off,a1,a2,a3,t0,t1,t2,&v0,&f)){if(f0)*f0=f;return v0;}
    }
    if (!gt4_session_orig && off == 0x190
        && (GT4_U32(cond + SESSION_VT) == TIME_TRIAL_VT || GT4_U32(cond + SESSION_VT) == RACE_VT || GT4_U32(cond+SESSION_VT)==0x00679988u
            || GT4_U32(cond + SESSION_VT) == PRACTICE_VT || GT4_U32(cond+SESSION_VT)==MACHINE_TEST_VT)) {
        penalty(cond, a1, a2, a3, f12);
        if (f0) *f0 = 0;
        return 0;
    }
    if (off == 0xa0 || off == 0xa8) { event_rest = off == 0xa0 ? a3 : t2; event_frac = f12; }
    if (!gt4_session_orig && GT4_U32(cond + SESSION_VT) == TIME_TRIAL_VT && time_trial(cond, off, a1, a2, a3, t0, t1, &v0, &f)) {
        if (f0) *f0 = f;
        return v0;
    }
    if (!gt4_session_orig && GT4_U32(cond + SESSION_VT) == RACE_VT && race(cond, off, a1, a2, a3, t0, t1, t2, &v0, &f)) {
        if (f0) *f0 = f;
        return v0;
    }
    if(!gt4_session_orig && GT4_U32(cond+SESSION_VT)==0x00679988u && battle(cond,off,a1,a2,a3,t0,t1,t2,&v0,&f)){
        if(f0)*f0=f;return v0;
    }
    if(!gt4_session_orig && GT4_U32(cond+SESSION_VT)==PRACTICE_VT && practice(cond,off,a1,a2,a3,t0,t1,t2,&v0,&f)){
        if(f0)*f0=f;return v0;
    }
    if(!gt4_session_orig && GT4_U32(cond+SESSION_VT)==MACHINE_TEST_VT && machine(cond,off,a1,a2,a3,t0,t1,t2,&v0,&f)){
        if(f0)*f0=f;return v0;
    }
    gt4_orig_a2 = a2;
    gt4_orig_a3 = a3;
    gt4_orig_t0 = t0;
    gt4_orig_t1 = t1;
    gt4_orig_t2 = t2;
    f = gt4_orig_call(GT4_U32(e + 4), cond + (uint32_t)(int32_t)(int16_t)GT4_U16(e), a1, f12, 0);
    if (f0) *f0 = f;
    return gt4_orig_v0;
}

uint32_t gt4_session(uint32_t cond, uint32_t off, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, float *f0)
{
    return gt4_session_x(cond, off, a1, a2, a3, t0, t1, 0, 0, f0);
}

/* func_003C0098 (func_00395498 with 1): float k of the second entry of the
   course's checkpoint set */
float gt4_course_checkpoint(uint32_t course, uint32_t k) { return course_entry_value(course, k, 1); }

/* ---- the race's start (the Time Trial's method 0xD0) ---- */

#define PB(o) ((float)(int32_t)GT4_U8(0x00620cd8u + (o)))

static float d0_div(float a, float b)
{
    uint32_t sa, sb;
    if (b != 0) return a / b;
    memcpy(&sa, &a, 4);
    memcpy(&sb, &b, 4);
    return ((sa ^ sb) >> 31) ? K(0xff7fffffu) : K(0x7f7fffffu);
}

/* func_00359A38 */
static void curve_set(uint32_t c, uint32_t n, uint32_t xs, uint32_t ys)
{
    GT4_U16(c) = (uint16_t)n;
    GT4_U32(c + 4) = xs;
    GT4_U32(c + 8) = ys;
    GT4_U8(c + 2) = 0;
    GT4_U8(c + 3) = 0;
}

/* func_003F2F78: D_00622308..18 from the options (D_00620CD8). */
static void view_settings(void)
{
    float n = PB(0);
    GT4_F32(0x00622308u) = n * K(0x3dccccccu);
    if (GT4_U8(0x00620cd8u)) {
        GT4_F32(0x00622310u) = PB(2) / n;
        GT4_F32(0x0062230cu) = PB(1) / n;
    } else {
        GT4_U32(0x0062230cu) = 0;
        GT4_U32(0x00622310u) = 0;
    }
    GT4_F32(0x00622314u) = GT4_U8(0x00620cd8u + 5) ? K(0x41200000u) / PB(5) : K(0x3f800000u);
    GT4_F32(0x00622318u) = GT4_U8(0x00620cd8u + 6) ? K(0x41200000u) / PB(6) : K(0x3e800000u);
}

/* func_0035F808: the surfaces' bumps (D_00620938: the curve of speeds and
   heights, then per surface a wave length, height and two kinds). */
void gt4_surface_bumps(uint32_t cond)
{
    const uint32_t B = 0x00620938u, Q = 0x00620cdcu;
    uint32_t n = GT4_U8(0x00620cd8u + 0xb), lvl;
    view_settings();
    for (uint32_t k = 0; k < n; k++) {
        GT4_F32(B + 0xc + k * 4) = (float)(int32_t)(GT4_U8(0x00620cd8u + 0xc + k) * 2) * K(0x3e8e38e3u);
        GT4_F32(B + 0x2c + k * 4) = PB(0x14 + k) * K(0x3a83126eu);
    }
    curve_set(B, n, B + 0xc, B + 0x2c);
    lvl = GT4_U8(cond + 0xcbddu);
    for (uint32_t s = 0; s < 0x18; s++) {
        uint32_t i = s;
        if (lvl == 3) i = s == 5 ? 15 : s;                    /* func_0034C998: level 2 */
        GT4_F32(B + 0x4c + s * 4) = (float)(int32_t)(int16_t)GT4_U16(Q + 0x30 + i * 2) * K(0x3c23d70au);
        GT4_F32(B + 0xac + s * 4) = (float)(int32_t)GT4_U8(Q + 0x18 + i) * K(0x3a83126eu);
        GT4_F32(B + 0x10c + s * 4) = d0_div(K(0x40c90fdau), (float)(int32_t)GT4_U8(Q + 0x60 + i) * K(0x3dccccccu));
        GT4_U8(B + 0x16c + s) = GT4_U8(Q + 0x78 + i);
        GT4_U8(B + 0x184 + s) = GT4_U8(Q + 0x90 + i);
    }
}

/* func_003F7BE0: the wheel's centring (D_00845BF0). */
static void centring_settings(void)
{
    const uint32_t D = 0x00845bf0u;
    GT4_U32(D) = GT4_U8(0x00620cd8u + 0xed);
    GT4_U32(D + 4) = GT4_U8(0x00620cd8u + 0xee);
    for (uint32_t k = 0; k < 2; k++) {
        if (!GT4_U8(0x00620cd8u + 0xef + k)) GT4_U32(D) = 0;
        GT4_F32(D + 8 + k * 4) = PB(0xef + k) * K(0x3ae4c385u);
        GT4_F32(D + 0x18 + k * 4) = PB(0xf3 + k);
        GT4_F32(D + 0x10 + k * 4) = PB(0xf1 + k) * K(0x41200000u);
    }
}

/* func_00366100: the curves at D_00620B00 from D_00620AF0 (a, b: 4, 2). */
static void slip_curves(uint32_t t, uint32_t src, int32_t a, int32_t b)
{
    const float deg = K(0x3c8efa34u), hundred = K(0x42c80000u), fb = (float)b;
#define SB(o) ((float)(int32_t)GT4_U8(src + (o)))
    GT4_F32(t + 4) = (SB(1) * K(0x3c23d70au)) * deg;
    GT4_F32(t + 8) = SB(2) * hundred;
    GT4_F32(t + 0xc) = SB(3);
    GT4_U32(t + 0x1c) = 0;
    GT4_F32(t + 0x14) = (float)(int32_t)(int16_t)GT4_U16(src + 4) * K(0x447a0000u);
    GT4_F32(t + 0x20) = fb;
    GT4_F32(t + 0x18) = (float)(int32_t)(int16_t)GT4_U16(src + 6) * K(0x447a0000u);
    curve_set(t + 0x24, 2, t + 0x14, t + 0x1c);
    GT4_U32(t + 0x30) = 0;
    GT4_F32(t + 0x34) = (float)(a + b);
    GT4_F32(t + 0x10) = K(0x3f800000u) / fb;
    GT4_F32(t + 0x38) = SB(8) / hundred;
    GT4_F32(t + 0x3c) = SB(9) / hundred;
    curve_set(t + 0x40, 2, t + 0x30, t + 0x38);
    GT4_F32(t + 0x4c) = (SB(0xa) * deg) / hundred;
    GT4_F32(t + 0x50) = (SB(0xb) * deg) / hundred;
    curve_set(t + 0x54, 2, t + 0x30, t + 0x4c);
#undef SB
}

/* func_003F2390: five rows (D_00622240, 8 bytes) into D_00622268 (0x20). */
static void shift_rows(void)
{
    for (uint32_t r = 0; r < 5; r++) {
        uint32_t s = 0x00622240u + r * 8, d = 0x00622268u + r * 0x20;
        GT4_U8(d) = GT4_U8(s);
        GT4_F32(d + 4) = (float)(int32_t)GT4_U8(s + 1) * K(0x3a83126eu);
        GT4_F32(d + 8) = (float)(int32_t)GT4_U8(s + 2) / K(0x42c80000u);
        for (uint32_t k = 0; k < 2; k++) GT4_F32(d + 0xc + k * 4) = (float)(int32_t)GT4_U8(s + 3 + k) / K(0x42c80000u);
        GT4_F32(d + 0x1c) = (float)(int32_t)GT4_U8(s + 7) * K(0x3e8e38e3u);
        GT4_F32(d + 0x14) = (float)(int32_t)GT4_U8(s + 5) * K(0x3a83126eu);
        GT4_F32(d + 0x18) = (float)(int32_t)GT4_U8(s + 6) * K(0x3e8e38e3u);
    }
}

/* func_00350A40: the course's surfaces (course data +0x80, 0x44 each)
   from the rows of D_00620348 (8 bytes), some by the race's level. */
static void course_surfaces(uint32_t cond)
{
    uint32_t t0 = GT4_U32(GT4_U32(GT4_U32(cond + 0xcbd8u) + 4) + 0x80), lvl = GT4_U8(cond + 0xcbddu);
    for (uint32_t s = 0; s < 11; s++) {
        uint32_t i = s, row, e = t0 + s * 0x44;
        if (lvl == 3) i = s == 4 ? 11 : s;                    /* level 2 */
        if (lvl != 2 && lvl != 3 && lvl != 1 && s == 5) i = 12;   /* level 0, not mode 1 */
        row = 0x00620348u + i * 8;
        if (GT4_F32(e + 0x4dc) < K(0x3c23d70au)) GT4_F32(e + 0x4dc) = K(0x3f800000u);
        GT4_F32(e + 0x4e0) = (float)(int32_t)GT4_U8(row) * K(0x3c23d70au);
        GT4_F32(e + 0x4e4) = (float)(int32_t)GT4_U8(row + 1) * K(0x3dccccccu);
        GT4_F32(e + 0x4e8) = (float)(int32_t)GT4_U8(row + 2) * K(0x3c23d70au);
        GT4_F32(e + 0x4ec) = (float)(int32_t)GT4_U8(row + 3);
        GT4_F32(e + 0x4f0) = (float)(int32_t)GT4_U8(row + 4) * K(0x3c23d70au);
        GT4_F32(e + 0x4fc) = (float)(int32_t)GT4_U8(row + 5) * K(0x3c23d70au);
        GT4_F32(e + 0x500) = (float)(int32_t)GT4_U8(row + 6) * K(0x3c23d70au);
        GT4_U32(e + 0x510) = GT4_U8(row + 7);
    }
}

/* func_003FA980 */
static void pad_row(uint32_t p, int full)
{
    GT4_U8(p + 3) = 4;
    GT4_U8(p + 1) = 4;
    GT4_U8(p + 2) = 4;
    if (full) {
        GT4_U8(p + 8) = 0;
        GT4_U8(p + 0x10) = 0;
        GT4_U8(p + 0x18) = 0;
    }
    if (!(GT4_U8(p + 0x21) < 0x33)) GT4_U8(p + 0x21) = 0x32;
}

/* func_003FA9C0: a range's ends kept in order */
static void range_order(uint32_t a, uint32_t b)
{
    uint8_t hi = GT4_U8(b), lo = GT4_U8(a);
    if (lo < hi) return;
    if (lo != 0xff) GT4_U8(b) = (uint8_t)(lo + 1);
    else GT4_U8(a) = hi ? (uint8_t)(hi - 1) : 0xff;
}

/* func_00366B28 */
static void tyre_curve_a(uint32_t t, uint32_t src, float kx, float ky)
{
    uint32_t n = GT4_U8(src + 1);
    for (uint32_t k = 0; k < n; k++) {
        GT4_F32(t + k * 4) = (float)(int32_t)GT4_U8(src + 4 + k) * kx;
        GT4_F32(t + 0x10 + k * 4) = (float)(int32_t)GT4_U8(src + 8 + k) * ky;
    }
    curve_set(t + 0x20, n, t, t + 0x10);
}

/* func_00366BC8 */
static void tyre_curve_b(uint32_t t, uint32_t src, float kx, float ky, float ky2)
{
    uint32_t n = GT4_U8(src + 2), m = GT4_U8(src + 3);
    for (uint32_t k = 0; k < n; k++) {
        GT4_F32(t + 0x2c + k * 4) = -(float)(int32_t)GT4_U8(src + 0xc + n - 1 - k) * kx;
        GT4_F32(t + 0x4c + k * 4) = (float)(int32_t)GT4_U8(src + 0x10 + n - 1 - k) * ky;
    }
    for (uint32_t k = 0; k < m; k++) {
        GT4_F32(t + 0x2c + (n + k) * 4) = (float)(int32_t)GT4_U8(src + 0x14 + k) * kx;
        GT4_F32(t + 0x4c + (n + k) * 4) = (float)(int32_t)GT4_U8(src + 0x18 + k) * ky2;
    }
    curve_set(t + 0x6c, n + m, t + 0x2c, t + 0x4c);
}

/* func_00366CD8 */
static void tyre_limits(uint32_t t, uint32_t src)
{
    float f = (float)(int32_t)GT4_U8(src + 0x21) * K(0x3e8e38e3u);
    GT4_F32(t + 0x78) = (float)(int32_t)GT4_U8(src + 0x20) * K(0x3c23d70au);
    GT4_F32(t + 0x7c) = f / K(0x4247ffffu);
    GT4_F32(t + 0x80) = ((float)(int32_t)GT4_U8(src + 0x22) * K(0x3c23d70au) * K(0x4247ffffu)) / (K(0x41de38e2u) - f);
}

/* func_0034F6A0: the assists' and vibration's settings (D_00843630) */
static void assist_settings(uint32_t p)
{
    const uint32_t D = 0x00843630u;
    const float ten = K(0x41200000u), hundred = K(0x42c80000u), kmh = K(0x4031c71bu);
#define P(o) ((float)(int32_t)GT4_U8(p + (o)))
    float a = P(0xe) / ten, b = (P(0xf) / ten) / hundred, c;
    GT4_F32(D) = (a * b) * b;
    GT4_F32(D + 4) = P(0x10) * kmh;
    GT4_F32(D + 8) = P(0x11) * kmh;
    GT4_F32(D + 0xc) = (float)(int32_t)(GT4_U8(p + 0x12) * 100);
    GT4_F32(D + 0x10) = (float)(int32_t)(GT4_U8(p + 0x13) * 100);
    GT4_F32(D + 0x14) = P(0x14) / ten;
    c = (P(0x16) / ten) / hundred;
    GT4_F32(D + 0x18) = (a * c) * c;
    GT4_F32(D + 0x1c) = P(0x17) * kmh;
    GT4_F32(D + 0x20) = P(0x18) * kmh;
    GT4_F32(D + 0x24) = P(0x19) / hundred;
    GT4_F32(D + 0x28) = P(0x1a) / hundred;
    GT4_F32(D + 0x2c) = P(0x1b) / ten;
    GT4_F32(D + 0x30) = P(6) / hundred;
    GT4_F32(D + 0x34) = P(7) / hundred;
    GT4_F32(D + 0x38) = P(8) / hundred;
    GT4_F32(D + 0x3c) = P(9) * kmh;
    GT4_F32(D + 0x40) = P(0xa) * kmh;
    GT4_F32(D + 0x44) = P(0xb) / hundred;
    GT4_F32(D + 0x48) = P(0xc) / hundred;
    GT4_F32(D + 0x4c) = P(0xd) / ten;
    GT4_F32(D + 0x50) = P(0) / hundred;
    GT4_F32(D + 0x54) = P(1) / hundred;
    GT4_F32(D + 0x58) = P(2) * K(0x411ccccc);
    {
        float lo = P(4) / ten, hi = P(3) / ten;
        GT4_F32(D + 0x60) = lo;
        GT4_F32(D + 0x5c) = (hi - lo) / K(0x4482e651u);
    }
    GT4_F32(D + 0x64) = P(0x1c) / hundred;
    GT4_F32(D + 0x68) = P(0x1d) / hundred;
    GT4_F32(D + 0x6c) = P(0x1e) / hundred;
    GT4_F32(D + 0x70) = P(0x1f) / ten;
    GT4_F32(D + 0x74) = P(0x26) / hundred;
    GT4_F32(D + 0x78) = P(0x28) / hundred;
    GT4_F32(D + 0x7c) = P(0x29) / hundred;
#undef P
}

/* func_00366D68: the tyres' surface curves (D_00843A20/C30/E40, 0x84 per
   surface class) from D_006224B0, the assists' settings and D_00844050. */
static void tyre_surface_tables(void)
{
    const uint32_t S = 0x006224b0u;
    const float div = K(0x451f6000u);
    /* func_003FAA10: the rows' limits */
    for (uint32_t k = 0; k < 4; k++) {
        uint32_t s0 = S + k * 0x28;
        uint8_t c;
        pad_row(s0, 1);
        pad_row(s0 + 0xa0, 0);
        c = GT4_U8(s0 + 0xc0);
        if (c < 0x32) GT4_U8(s0 + 0xc0) = 0x32;
        else if (!(c < 0xc9)) GT4_U8(s0 + 0xc0) = 0xc8;
        pad_row(S + 0x140 + k * 0x28, 0);
    }
    range_order(S + 0x1e9, S + 0x1ea);
    range_order(S + 0x1f0, S + 0x1f1);
    range_order(S + 0x1f7, S + 0x1f8);
    range_order(S + 0x201, S + 0x203);
    if (!GT4_U8(S + 0x1e2)) GT4_U8(S + 0x1e2) = 1;
    for (uint32_t k = 0; k < 4; k++) {
        uint32_t src[3] = { S + k * 0x28, S + 0xa0 + k * 0x28, S + 0x140 + k * 0x28 };
        uint32_t dst[3] = { 0x00843a20u + k * 0x84, 0x00843c30u + k * 0x84, 0x00843e40u + k * 0x84 };
        for (int j = 0; j < 3; j++) {
            uint32_t s = src[j], t = dst[j];
            memset(gt4_ptr(t), 0, 0x84);
            tyre_curve_a(t, s, GT4_U8(s) ? K(0x3de38e37u) : K(0x3b64c385u), (float)(int32_t)GT4_U8(s + 0x1c) / div);
            tyre_curve_b(t, s, GT4_U8(s + 0x1f) ? K(0x3d638e37u) : K(0x3b808080u),
                         (float)(int32_t)GT4_U8(s + 0x1d) / div, (float)(int32_t)GT4_U8(s + 0x1e) / div);
            tyre_limits(t, s);
        }
    }
    assist_settings(S + 0x1e0);
    {
        uint32_t p = S + 0x1e0, D = 0x00844050u;
        GT4_F32(D + 0x10) = (float)(int32_t)GT4_U8(p + 0x24) * K(0x3dccccccu);
        GT4_F32(D + 0x00) = (float)(int32_t)GT4_U8(p + 0x20) * K(0x3dccccccu);
        GT4_F32(D + 0x0c) = (float)(int32_t)GT4_U8(p + 0x23) * K(0x4031c71bu);
        GT4_F32(D + 0x04) = (float)(int32_t)GT4_U8(p + 0x21) * K(0x4031c71bu);
        GT4_F32(D + 0x08) = (float)(int32_t)GT4_U8(p + 0x22) * K(0x3dccccccu);
    }
}

/* func_003F3AB8: D_00845A10, 5 rows of 24 s16 hundredths (D_00622330). */
static void speed_rows(void)
{
    for (uint32_t r = 0; r < 5; r++)
        for (uint32_t k = 0; k < 24; k++)
            GT4_F32(0x00845a10u + (r * 24 + k) * 4) = (float)(int32_t)(int16_t)GT4_U16(0x00622330u + r * 48 + k * 2) * K(0x3c23d70au);
}

/* func_003F3808: D_00845A00 from the options */
static void sound_rates(void)
{
    const uint32_t D = 0x00845a00u;
    uint32_t a = GT4_U8(0x00620cd8u + 0xac), b = GT4_U8(0x00620cd8u + 0xad);
    if (!a) a = 1;
    if (!b) b = 1;
    GT4_F32(D + 8) = PB(0xae) * K(0x3e8e38e3u);
    GT4_F32(D + 0xc) = PB(0xaf) * K(0x3c23d70au);
    GT4_F32(D) = K(0x3f800000u) / ((float)(int32_t)a * K(0x46191fffu));
    GT4_F32(D + 4) = K(0x3f800000u) / (((float)(int32_t)b * K(0x3dccccccu)) * K(0x42c80000u));
}

/* func_0034A4B8: D_006202C8 from D_006202B0, some by the race's level */
static void pit_settings(uint32_t cond)
{
    const uint32_t S = 0x006202b0u, T = 0x006202c8u;
    uint32_t lvl = GT4_U8(cond + 0xcbddu);
    int32_t hi = GT4_U8(S + 0xc), lo = GT4_U8(S + 0xb);
    float a, b, c, d;
#define SB(o) ((float)(int32_t)GT4_U8(S + (o)))
    GT4_F32(T + 0x10) = SB(6) * K(0x3ae4c385u);
    GT4_F32(T + 0x00) = SB(1) * K(0x3ae4c385u);
    GT4_F32(T + 0x04) = SB(2) * K(0x3dccccccu);
    GT4_F32(T + 0x08) = SB(3);
    GT4_F32(T + 0x0c) = SB(4);
    GT4_F32(T + 0x1c) = SB(9);
    GT4_F32(T + 0x14) = SB(7) * K(0x3dccccccu);
    GT4_F32(T + 0x18) = SB(8);
    if (lo == hi) hi = lo + 1;
    if (lvl == 0xff) lvl = 0;
    if (lvl == 2) { lo += GT4_U8(S + 0xf); hi += GT4_U8(S + 0xf); }
    else if (lvl == 3) { lo += GT4_U8(S + 0x10); hi += GT4_U8(S + 0x10); }
    a = (float)lo * K(0x3e8e38e3u);
    b = (float)hi * K(0x3e8e38e3u);
    c = SB(0xe) * K(0x3e8e38e3u);
    d = SB(0xd) * K(0x3e8e38e3u);
    GT4_F32(T + 0x20) = a;
    GT4_F32(T + 0x24) = b;
    GT4_F32(T + 0x2c) = c;
    GT4_F32(T + 0x28) = d;
    GT4_F32(T + 0x30) = d0_div(c - d, b - a);
#undef SB
}

/* func_00358270: the race's times cleared (func_003F10E8 and the lap and
   split records to "no time") */
static void times_clear(uint32_t cond)
{
    const uint32_t none = 0x157529ffu;
    GT4_U32(cond + 0xf898u) = 0;
    GT4_U32(cond + 0xf89cu) = 0;
    GT4_U32(cond + 0xf8c0u) = 0;                              /* func_003F10E8 */
    GT4_U32(cond + 0xf8bcu) = 0;
    GT4_U32(cond + 0xf8f4u) = 0xfffffff5u;
    for (uint32_t k = 0; k < 12; k++) GT4_U32(cond + 0xf8f0u - k * 4) = 0;
    for (uint32_t r = 0; r < 4; r++)
        for (uint32_t a = 0; a < 16; a++) {
            uint32_t v = cond + 0xce00u + r * 384 + a * 24;
            for (uint32_t k = 0; k < 6; k++) GT4_U32(v - k * 4) = none;
            GT4_U16(cond + 0xd3ecu + a * 2) = 0xffff;
            GT4_U8(cond + 0xd40cu + r * 16 + a) = 0;
            GT4_U8(cond + 0xd44cu + r * 16 + a) = 0;
        }
    for (uint32_t c = 0; c < 6; c++) {
        GT4_U32(cond + 0xd60cu + c * 4) = 0;
        for (uint32_t k = 0; k < 16; k++) {
            GT4_U32(cond + 0xd624u + c * 0x40 + k * 4) = none;
            GT4_U32(cond + 0xd7a4u + c * 0x40 + k * 4) = none;
            GT4_U16(cond + 0xd924u + c * 0x20 + k * 2) = 0xffff;
        }
        GT4_U16(cond + 0xd9e4u + c * 2) = 0;
        for (uint32_t k = 0; k < 4; k++) {
            GT4_U16(cond + 0xd9f0u + c * 8 + k * 2) = 0xffff;
            GT4_U8(cond + 0xda20u + c * 8 + k * 2) = 0xff;
            GT4_U8(cond + 0xda21u + c * 8 + k * 2) = 0xff;
        }
    }
}

/* func_003F1790 (func_003F1BD8 with 1): the gearbox choices (D_006220A8
   bytes, D_006220B0/C8/E0) from D_00622088..A0. Its div.s round to the
   nearest: the B-Spec savestates hold 1.1 as 0x3F8CCCCD, 0.8 0x3F4CCCCD and
   1.2 0x3F99999A, one above what the race's toward-zero mode gives. */
static void gearbox_choices(void)
{
    int mode = fegetround();
    fesetround(FE_TONEAREST);
    for (uint32_t k = 0; k < 5; k++) {
        int32_t a = (int8_t)GT4_U8(0x00622088u + k), b = (int8_t)GT4_U8(0x006220a0u + k);
        GT4_U8(0x006220a8u + k) = (uint8_t)((a * 128 + 0x3200) / 100);
        GT4_F32(0x006220b0u + k * 4) = (float)(int32_t)GT4_U8(0x00622090u + k) / K(0x42c80000u);
        GT4_F32(0x006220c8u + k * 4) = (float)(int32_t)GT4_U8(0x00622098u + k) / K(0x42c80000u);
        GT4_F32(0x006220e0u + k * 4) = (float)(b + 100) / K(0x42c80000u);
    }
    fesetround(mode);
}

/* func_003BFD00: whether the race's world is a RaceArcadeSingle (its
   typeid function, table entry 0); another class the original answers. */
static int world_is_arcade(uint32_t race)
{
    /* 003BFD00's RTTI target is RaceArcadeSingle. RaceSplitBattle
       (00330578, 00676C48) derives from RaceSplitBattleBase instead. */
    uint32_t world = GT4_U32(race + 0x84), fn;
    if (!world) return 0;
    if(GT4_U32(world+0x64)==0x00676c48u)return 0;
    fn = GT4_U32(GT4_U32(world + 0x64) + 4);
    if (fn == 0x005f64f0u) return 1;                          /* RaceArcadeSingle */
    if (fn == 0x005fb1b0u) return 0;                          /* RaceFreeRun */
    if (fn == 0x005fb110u) return 0;                          /* RaceFreePractice (on RaceWorld, 0088F840) */
    /* 005FEA58/005FEB48 register RaceGTmode/RaceChampionship with the
       RaceWorld base (0088F840); neither inherits RaceArcadeSingle. */
    if (fn == 0x005fea58u || fn == 0x005feb48u || fn == 0x005fb338u) return 0; /* RaceLicense */
    /* 005FE9B8/005FE958: RaceTraining -> RaceTrainingBase -> RaceWorld. */
    if (fn == 0x005fe9b8u) return 0;
    /* 005FB728: RaceMission, also on RaceTrainingBase (005FE958). */
    if (fn == 0x005fb728u) return 0;
    /* 005FB460 / 005FE4F0: RaceMachineTest on RaceSolitaire (0088F230). */
    if (fn == 0x005fb460u) return 0;
    gt4_orig_call(0x003bfd00u, race, 0, 0, 0);
    return gt4_orig_v0 != 0;
}

/* func_0034CA20: the race's start: the grid set (cond+0xF839, below the
   runway's count), the penalty thresholds for arcade races, the
   settings tables, the race order and the catch-up (func_00365480), the
   lap records, the driving settings (func_00347A00) and the times. */
/* func_00367AF8 +0x00..0x190: global wear modifiers, including the
   signed halfwords and two separate rounded multiplications at +0x118. */
void gt4_tyre_wear_global(uint32_t t)
{
    if (!GT4_U8(0x00620b60u)) return;
    for (unsigned i = 0; i < 8; i++)
        GT4_F32(t + 0x30 + 4*i) = (float)(int16_t)GT4_U16(0x00620b62u + 2*i) * K(0x3a83126eu);
    GT4_F32(t + 0x4c) *= K(0x38d5fec0u);
    GT4_F32(t + 0x50) = (float)GT4_U8(0x00620b72u) * K(0xbc23d70au);
    for (unsigned i = 0; i < 3; i++)
        GT4_F32(t + 0x54 + 4*i) = (float)GT4_U8(0x00620b73u + i) * K(0x3c23d70au);
    GT4_F32(t + 0x60) = K(0x3f800000u);
    GT4_F32(t + 0x64) = (float)GT4_U8(0x00620b76u) * K(0x3c23d70au);
    GT4_U8(t + 0x69) = 1;
}

/* func_00367918 +0x00..0x1d8: validate the ordered wear thresholds,
   then install event values and the original GAME_SETTINGS modifiers. */
void gt4_tyre_wear_rules(uint32_t t, uint32_t p)
{
    if (!GT4_U8(p+0x2c) || GT4_U8(p+0x33) >= GT4_U8(p+0x2c) ||
        GT4_U8(p+0x34) >= GT4_U8(p+0x2d) || GT4_U8(p+0x31) >= GT4_U8(p+0x33) ||
        GT4_U8(p+0x32) >= GT4_U8(p+0x34) || GT4_U8(p+0x2e) >= GT4_U8(p+0x2d)) return;
    GT4_F32(t+0x18) = (float)GT4_U8(p+0x2c) * K(0x461c4000u);
    GT4_F32(t+0x1c) = (float)GT4_U8(p+0x2d) * K(0x3c23d70au);
    GT4_F32(t+0x20) = (float)(100-(int)GT4_U8(p+0x2e)) * K(0x3c23d70au);
    if (GT4_U8(t+0x69)) { GT4_U32(t)=0; GT4_U32(t+4)=0; }
    else {
        GT4_F32(t) = -(float)GT4_U8(p+0x2f) * K(0x461c4000u);
        GT4_F32(t+4) = (float)GT4_U8(p+0x30) * K(0x3c23d70au);
    }
    GT4_F32(t+8) = (float)GT4_U8(p+0x31) * K(0x461c4000u);
    GT4_F32(t+0xc) = (float)GT4_U8(p+0x32) * K(0x3c23d70au);
    GT4_F32(t+0x10) = (float)GT4_U8(p+0x33) * K(0x461c4000u);
    GT4_F32(t+0x14) = (float)GT4_U8(p+0x34) * K(0x3c23d70au);
    GT4_F32(t+0x24) = (float)GT4_U8(0x00620cd8u+0xbe) / K(0x42c80000u);
    GT4_F32(t+0x28) = (float)GT4_U8(0x00620cd8u+0xc2) / K(0x42c80000u);
    GT4_F32(t+0x2c) = -(float)GT4_U8(0x00620cd8u+0xc3) / K(0x42c80000u);
    GT4_U8(t+0x68) = 1;
}

static void race_start(uint32_t cond, uint32_t a1)
{
    uint32_t course = GT4_U32(cond + 0xcbd8u), race = GT4_U32(cond), grid, s1, s0, lvl;
    int32_t set = (int8_t)GT4_U8(cond + 0xf839u);
    grid = GT4_U16(GT4_U32(GT4_U32(course + 4) + 0xdc) + 0x54);   /* func_00395200 */
    if (!(set < (int32_t)grid)) set = -1;
    GT4_U32(course + 0xb0) = (uint32_t)set;                      /* func_00395220 */
    GT4_U8(cond + 0xcbe2u) = (uint8_t)set;
    GT4_U8(cond + 0xcbe5u) = 0;
    if (world_is_arcade(race)) {
        uint32_t mode = gt4_session(cond, 0x30, 0, 0, 0, 0, 0, NULL) ? 0 : GT4_U32(race + 4);
        if (mode == 1) {
            GT4_U8(cond + 0xf861u) = 8;
            GT4_U8(cond + 0xf860u) = 0x28;
            GT4_U8(cond + 0xf862u) = 10;
        } else if (mode == 2) {
            GT4_U8(cond + 0xf861u) = 8;
            GT4_U8(cond + 0xf860u) = 0x28;
            GT4_U8(cond + 0xf862u) = 0xff;
        } else {
            GT4_U8(cond + 0xf862u) = 0;
            GT4_U8(cond + 0xf860u) = 0xff;
            GT4_U8(cond + 0xf861u) = 0xff;
        }
    }
    lvl = GT4_U8(cond + 0xcbddu);
    if (lvl == 0xff) lvl = 0;
    if (!(GT4_U8(cond + 0xcbe0u) == 1 && !(lvl == 2 || lvl == 3) && !gt4_session(cond, 0x30, 0, 0, 0, 0, 0, NULL))) {
        if (GT4_U8(cond + 0xf862u) == 0xff) GT4_U8(cond + 0xf862u) = 0;
        GT4_U8(cond + 0xf864u) = 0;
    }
    gt4_surface_bumps(cond);                                   /* func_0035F808 */
    centring_settings();                                       /* func_003F7BE0 */
    slip_curves(0x00620b00u, 0x00620af0u, 4, 2);               /* func_00366100 */
    shift_rows();                                              /* func_003F2390 */
    course_surfaces(cond);                                     /* func_00350A40 */
    tyre_surface_tables();                                     /* func_00366D68 */
    speed_rows();                                              /* func_003F3AB8 */
    sound_rates();                                             /* func_003F3808 */
    pit_settings(cond);                                        /* func_0034A4B8 */
    s1 = gt4_session(cond, 0x60, cond + 0xf818u, 0, 0, 0, 0, NULL);
    s0 = gt4_session(cond, 0x58, cond + 0xf818u, 0, 0, 0, 0, NULL);
    if (s1) {                                                  /* func_00365480 */
        uint32_t b = cond + 0xf818u;
        GT4_F32(cond + 0x80) = (float)(int32_t)GT4_U8(b + 0x25);
        GT4_F32(cond + 0x84) = (float)(int32_t)GT4_U8(b + 0x26) * K(0x41200000u);
        GT4_F32(cond + 0x7c) = (float)(int32_t)GT4_U8(b + 0x24) * K(0x3c23d70au);
        GT4_F32(cond + 0x8c) = (float)(int32_t)GT4_U8(b + 0x29);
        GT4_F32(cond + 0x90) = (float)(int32_t)GT4_U8(b + 0x2a) * K(0x41200000u);
        GT4_F32(cond + 0x88) = (float)(int32_t)GT4_U8(b + 0x28) * K(0x3c23d70au);
        GT4_F32(cond + 0x94) = (float)(int32_t)(GT4_U8(b + 0x20) ? GT4_U8(b + 0x20) : 100) * K(0x3c23d70au);
    } else {
        GT4_U32(cond + 0x80) = 0;
        GT4_F32(cond + 0x94) = K(0x3f800000u);
        GT4_U32(cond + 0x84) = 0;
        GT4_U32(cond + 0x7c) = 0;
        GT4_U32(cond + 0x8c) = 0;
        GT4_U32(cond + 0x90) = 0;
        GT4_U32(cond + 0x88) = 0;
    }
    if (lvl == 2 || lvl == 3) s0 = 0;
    {                                                          /* func_00367C90 */
        uint32_t t = cond + 4;
        uint32_t flag = GT4_U8(cond + 0xcbe0u) == 1 ? s0 : 0;
        for (uint32_t o = 0; o < 0x60; o += 4)                 /* func_003678A8 (not +0x40) */
            if (o != 0x40) GT4_U32(t + o) = 0;
        GT4_U32(t + 0x64) = 0;
        GT4_U8(t + 0x68) = 0;
        GT4_U8(t + 0x69) = 0;
        if (flag) {
            gt4_tyre_wear_global(t);
            gt4_tyre_wear_rules(t, cond + 0xf818u);
        }
    }
    GT4_U16(cond + 0x1003cu) = 0;                              /* func_0035DF10 */
    gt4_drive_settings(cond);                                  /* func_00347A00 */
    times_clear(cond);                                         /* func_00358270 */
    GT4_F32(cond + 0xcbf8u) = K(0x3f800000u);
    gearbox_choices();                                         /* func_003F1BD8 with 1 */
}
#undef PB

/* func_003F5530 (the Time Trial's method 0xD0): the race's start, then no
   split times (cond+0x10148..) and the run mode 1 turned into 2. */
static void tt_race_start(uint32_t cond, uint32_t a1)
{
    race_start(cond, a1);
    for (uint32_t k = 0; k < 6; k++) GT4_U32(cond + 0x10148u + k * 4) = 0;   /* func_003F5718 */
    if (GT4_U8(cond + 0xcbe0u)) GT4_U8(cond + 0xcbe0u) = 2;
}

/* func_003477C8 (the Arcade's method 0xD8): after_cars; with the course's
   rolling start (cond+0xF884..0xF887) the race is flagged as one
   (cond+0xCBE4); the lap limits. */
static void race_after_cars(uint32_t cond, const gt4_specdb_src *db)
{
    after_cars(cond, db);
    if (GT4_U8(cond + 0xf886u) && (int16_t)GT4_U16(cond + 0xf884u) < 0 && GT4_U8(cond + 0xf887u))
        GT4_U8(cond + 0xcbe4u) = 1;
    lap_limits(cond);
}

/* The session's method 0xD0 (the race's start; the Arcade's is
   func_0034CA20 itself) for the rewritten tables; 0 for the others. */
int gt4_session_start(uint32_t cond, uint32_t a1)
{
    uint32_t vt = GT4_U32(cond + SESSION_VT);
    if (gt4_session_orig) return 0;
    if (vt == TIME_TRIAL_VT) tt_race_start(cond, a1);
    else if (vt == RACE_VT || vt == PRACTICE_VT || vt == MACHINE_TEST_VT || vt == 0x00679988u || vt == GT4_LICENSE_SESSION_VT || vt == GT4_TRAINING_SESSION_VT || vt == GT4_MISSION_SESSION_VT) {
        if(vt==GT4_TRAINING_SESSION_VT || vt==GT4_MISSION_SESSION_VT)GT4_U8(cond+0xf864)=0; /* 003F7058 +0x10 */
        race_start(cond, a1);
    }
    else return 0;
    return 1;
}

/* The session's method 0xD8 (after the cars) for the rewritten tables; 0
   for the others. */
int gt4_session_after_cars(uint32_t cond, const gt4_specdb_src *db)
{
    uint32_t vt = GT4_U32(cond + SESSION_VT);
    if (gt4_session_orig) return 0;
    if (vt == TIME_TRIAL_VT) gt4_tt_after_cars(cond, db);
    else if (vt == RACE_VT) race_after_cars(cond, db);
    else if (vt == PRACTICE_VT) {                             /* func_003F4F60 */
        uint32_t race = GT4_U32(cond);
        after_cars(cond, db);
        for (uint32_t i = 0; (int32_t)i < (int32_t)GT4_U32(cond + 0xf8a0u); i++)   /* each entry's best lap kept (+0x1198) */
            best_lap(car_at(cond, i), GT4_U32(GT4_U32(GT4_U32(GT4_U32(race + 0x60) + 8) + i * 4) + 0x1198));
        GT4_U8(cond + 0xcbe5u) = 1;
    }
    else if (vt == 0x00679988u || vt == MACHINE_TEST_VT || vt == GT4_LICENSE_SESSION_VT || vt == GT4_TRAINING_SESSION_VT || vt == GT4_MISSION_SESSION_VT) after_cars(cond, db); /* 0034CD60 */
    else return 0;
    return 1;
}
