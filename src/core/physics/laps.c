/* Laps: each car's crossing of the start line, the sector lines and the
   course's timed points, with the time of the crossing to 1/50 of a frame
   (func_00358608), then the car's signal lights and the pit lane
   (func_00359008). The times go to the session (virtual +0x80..+0xB8) and
   to the lap records of the replay (func_00359B60 / func_00359C10). */
#include "car.h"

#include <math.h>

#define K(bits) gt4_f32(bits)

static float ps2_sqrt(float x) { return x == 0 ? x : sqrtf(fabsf(x)); }

static uint32_t car_at(uint32_t cond, uint32_t i) { return cond + 0x6698u + i * CAR_SIZE; }   /* func_0034C190 */

/* func_0034C230 */
static float course_length(uint32_t cond)
{
    return GT4_F32(GT4_U32(GT4_U32(GT4_U32(cond + 0xcbd8u) + 4) + 0xdc) + 0x14);
}

/* func_0034C338 / func_0034C470: the session value +0x130 (+0x138) is at
   least one and less than the course's length less one */
static int value_on_course(uint32_t cond, uint32_t off)
{
    float f, n;
    gt4_session(cond, off, 0, 0, 0, 0, 0, &f);
    if (f < 1.0f) return 0;
    gt4_session(cond, off, 0, 0, 0, 0, 0, &n);
    return !(course_length(cond) - 1.0f < n);
}

/* func_0035ACD8: the car's signal (D_006207E4: 8 bytes per kind; kinds
   with a zero there only while no other is shown) */
void gt4_car_signal(uint32_t car, uint32_t kind)
{
    uint32_t b = car + 0x104, t = GT4_U32(0x006207e4u + kind * 8);
    if (t || !GT4_U8(b + 0x5fb)) {
        GT4_U8(b + 0x5fb) = (uint8_t)t;
        GT4_U8(b + 0x5fa) = (uint8_t)kind;
    }
}

/* func_00359AF0: the car's times are recorded (a race kind where the
   session says so, and the car not driven by the model) */
int gt4_car_records_times(uint32_t car)
{
    if (!gt4_session(GT4_U32(car + 4), 0x30, 0, 0, 0, 0, 0, NULL)) return 0;
    return (GT4_U32(car + 0x104 + 0x464) & 0xffff0000u) == 0;
}

/* func_00359B60 / func_00359C10: the lap records of the replay */
static void record_split(uint32_t car, uint32_t k, uint32_t frac, float t, float a, float b)
{
    gt4_orig_a2 = frac;
    gt4_orig_f14 = b;
    gt4_orig_call(0x00359b60u, car, k, t, a);
}
static void record_time(uint32_t car, uint32_t k, uint32_t time, uint32_t on)
{
    gt4_orig_a2 = time;
    gt4_orig_a3 = on;
    gt4_orig_call(0x00359c10u, car, k, 0, 0);
}

/* The time (in thirds of a frame) of a crossing a fraction `frac` of the
   way through the frame: the race clock (+0xF898) plus 50 * frac plus the
   session's own count (+0x80). */
static uint32_t crossing_time(uint32_t cond, uint32_t idx, float frac, int32_t *fifty)
{
    int32_t f = (int32_t)(frac * 50.0f);
    uint32_t t = GT4_U32(cond + 0xf898u) + (uint32_t)f;
    t = t + gt4_session(cond, 0x80, idx, 0, 0, 0, 0, NULL);
    if (fifty) *fifty = f;
    return t;
}

/* func_00358608: car idx moved from distance prev to its current one
   (car+0x4A4 of the block at +0x104). 1 when it went backwards over the
   line or backwards along the course. */
static int lap_progress(uint32_t cond, uint32_t idx, float prev)
{
    uint32_t car = car_at(cond, idx), b = car + 0x104, race = GT4_U32(cond);
    float len = course_length(cond), cur = GT4_F32(b + 0x4a4), step;
    int ret = 0;

    if (prev <= cur) {
        if (prev + len * 0.5f < cur) {                       /* backwards over the line */
            uint32_t n;
            GT4_F32(b + 0x650) = (cur - (prev + len)) * 60.0f;
            GT4_F32(car + 0x99c) = GT4_F32(car + 0x99c) + len;
            n = GT4_U8(b + 0x4b0) + 1u;
            GT4_U8(b + 0x4b0) = (uint8_t)((n & 0xff) < 0xfb ? n : 0xfa);
            return 1;
        }
        step = cur - prev;
        GT4_F32(b + 0x650) = step * 60.0f;
        if (GT4_U8(b + 0x4b0)) return 0;
        if (!(int16_t)GT4_U16(b + 0x4ac) || GT4_U8(b + 0x4b2)) return 0;
        {                                                    /* the next sector line */
            float c;
            gt4_session(cond, 0x158, GT4_U8(b + 0x4ae), 0, 0, 0, 0, &c);
            if (prev < c && c <= cur) {
                uint32_t t = crossing_time(cond, idx, (c - prev) / step, NULL), s1 = t / 3, s4 = s1 - GT4_U32(b + 0x614);
                if (!gt4_session(cond, 0x30, 0, 0, 0, 0, 0, NULL))
                    gt4_session(cond, 0x90, idx, (uint32_t)((int16_t)GT4_U16(b + 0x4ac) - 1), GT4_U8(b + 0x4ae), s1, s4, NULL);
                gt4_session_x(cond, 0x98, idx, (uint32_t)(int32_t)(int16_t)GT4_U16(b + 0x4ac), GT4_U8(b + 0x4ae), t, s1, s4, 0, NULL);
                GT4_U8(b + 0x4ae) = (uint8_t)(GT4_U8(b + 0x4ae) + 1);
            }
        }
        if (gt4_car_records_times(car) && GT4_U32(race + 0xe0)) {    /* the course's timed points */
            float c = gt4_course_checkpoint(GT4_U32(race + 0x80), GT4_U8(b + 0x4b4));   /* func_00359AD8 */
            if (prev < c && c <= cur) {
                float frac = (c - prev) / step;
                int32_t fifty;
                uint32_t t = crossing_time(cond, idx, frac, &fifty) / 3 - GT4_U32(b + 0x614), k;
                k = gt4_session(cond, 0x48, car, 1, 0, 0, 0, NULL);
                record_time(car, k, t, GT4_U32(race + 0xe4));
                GT4_U8(b + 0x4b4) = (uint8_t)(GT4_U8(b + 0x4b4) + 1);
                k = gt4_session(cond, 0x48, car, 0, 0, 0, 0, NULL);
                if (GT4_U32(race + 0xe4)) record_split(car, k, (uint32_t)(50 - fifty), frac, c, cur);
            }
        }
        if (GT4_U8(b + 0x65b) == 1 && value_on_course(cond, 0x130)) {   /* the timed start line */
            float c;
            gt4_session(cond, 0x130, 0, 0, 0, 0, 0, &c);
            if (prev < c && c <= cur) {
                float frac = (c - prev) / step;
                int32_t fifty;
                uint32_t t = crossing_time(cond, idx, frac, &fifty) / 3;
                gt4_session_x(cond, 0xb0, idx, t, (uint32_t)(50 - fifty), 0, 0, 0, frac, NULL);
                GT4_U8(b + 0x65b) = 2;
            }
        }
        if (value_on_course(cond, 0x138)) {                  /* the finish line */
            float c;
            gt4_session(cond, 0x138, 0, 0, 0, 0, 0, &c);
            if (prev < c && c <= cur) {
                uint32_t t = crossing_time(cond, idx, (c - prev) / step, NULL), s1 = t / 3, s4 = s1 - GT4_U32(b + 0x614), done;
                int rec = gt4_car_records_times(car);
                done = gt4_session(cond, 0xb8, idx, (uint32_t)(int32_t)(int16_t)GT4_U16(b + 0x4ac), t, s1, s4, NULL);
                if (done && !gt4_session(cond, 0x30, 0, 0, 0, 0, 0, NULL))
                    gt4_session(cond, 0x90, idx, (uint32_t)((int16_t)GT4_U16(b + 0x4ac) - 1), GT4_U8(b + 0x4ae), s1, s4, NULL);
                if (rec && done && GT4_U32(race + 0xe0))
                    record_time(car, gt4_session(cond, 0x48, car, 1, 0, 0, 0, NULL), s4, GT4_U32(race + 0xe4));
            }
        }
        return ret;
    }

    if (!(cur + len * 0.5f < prev)) {                        /* backwards along the course */
        GT4_F32(b + 0x650) = (cur - prev) * 60.0f;
        return 1;
    }
    /* over the line: a new lap */
    GT4_F32(b + 0x650) = ((cur + len) - prev) * 60.0f;
    GT4_F32(car + 0x99c) = GT4_F32(car + 0x99c) - len;
    if (GT4_U8(b + 0x4b0)) {
        GT4_U8(b + 0x4b0) = (uint8_t)(GT4_U8(b + 0x4b0) - 1);
        return 0;
    }
    if (GT4_U8(b + 0x4b2)) return 0;
    {
        float before = len - prev, frac;
        uint32_t sector = GT4_U8(b + 0x4ae), t, s4, s6, rest;
        int32_t lap = (int16_t)GT4_U16(b + 0x4ac), fifty;
        GT4_U8(b + 0x4ae) = 0;
        GT4_U16(b + 0x4ac) = (uint16_t)(GT4_U16(b + 0x4ac) + 1);
        frac = before / (before + cur);
        t = crossing_time(cond, idx, frac, &fifty);
        s4 = t / 3;
        rest = (uint32_t)(50 - fifty);
        GT4_U8(b + 0x5c1) = 0;
        s6 = s4 - GT4_U32(b + 0x614);
        if (!lap) {                                          /* the first crossing: the start */
            gt4_session_x(cond, 0xa0, idx, s4, rest, 0, 0, 0, frac, NULL);
            if (gt4_car_records_times(car) && !gt4_session(cond, 0x38, 0, 0, 0, 0, 0, NULL) && GT4_U32(race + 0xe4))
                record_split(car, 0, rest, frac, 0, cur);
        } else {
            int rec;
            if (!gt4_session(cond, 0x30, 0, 0, 0, 0, 0, NULL))
                gt4_session(cond, 0x90, idx, (uint32_t)(lap - 1), sector, s4, s6, NULL);
            rec = gt4_car_records_times(car);
            gt4_session_x(cond, 0xa8, idx, (uint32_t)lap, t, s4, s6, rest, frac, NULL);
            if (rec && GT4_U32(race + 0xe0)) {
                uint32_t k = gt4_session(cond, 0x48, car, 1, 0, 0, 0, NULL);
                record_time(car, k, s6, GT4_U32(race + 0xe4));
                if (gt4_session(cond, 0x40, 0, 0, 0, 0, 0, NULL) && !GT4_U8(b + 0x4b2))
                    record_split(car, k + 1, rest, frac, 0, cur);
            }
        }
        GT4_U8(b + 0x4b4) = 0;
    }
    return 0;
}

/* The pit lane: the marks are the points of line 4 (func_0034C780: its
   distance along the course, i < 0 counting from the end; the box of car
   k at point k, func_0034C7A0 / func_0034C7C0: its distance and heading).
   +0x467 of the car's block is where it is in the lane: 2 going in, 3 at
   its box, 4 going out. */
static float pit_mark(uint32_t cond, int i) { return gt4_line_point_distance(cond + 0xda50u, i); }
static float pit_box(uint32_t cond, int k) { return gt4_line_point_value(cond + 0xda50u, k, 0x14); }
static float pit_box_heading(uint32_t cond, int k) { return gt4_line_point_value(cond + 0xda50u, k, 0x24); }

/* func_00463C28: the driver model takes another line */
static uint32_t model_line(uint32_t car, uint32_t line, uint32_t now)
{
    return (uint32_t)gt4_ai_line_switch(car + 0x14, (int)line, (int)now);
}

/* func_00345450 / func_003453A0 / func_00345400: onto the pit line (3) or
   the box line (4); the car's lane state 4, 2 (and +0x4B1 bit 3) or 3 */
static int to_pit_line(uint32_t car, uint32_t line, uint8_t state)
{
    uint32_t b = car + 0x104;
    if ((int8_t)GT4_U8(b + 0x466) == 4) return 0;
    if (!model_line(car, line, 1)) return 0;
    GT4_U8(b + 0x467) = state;
    if (state == 2) GT4_U8(b + 0x4b1) |= 8;
    return 1;
}
static int pit_abandon(uint32_t car) { return to_pit_line(car, 3, 4); }   /* func_00345450 */
static int pit_enter(uint32_t car) { return to_pit_line(car, 3, 2); }     /* func_003453A0 */
static int pit_to_box(uint32_t car) { return to_pit_line(car, 4, 3); }    /* func_00345400 */
int gt4_car_pit_box(uint32_t car) { return pit_to_box(car); }

/* func_00345228: out of the lane, back on the racing line */
int gt4_car_pit_leave(uint32_t car)
{
    uint32_t b = car + 0x104, obj = car + 0x14;
    GT4_U8(obj + 0x1b) = 0;                                  /* func_00463E90 */
    if (!GT4_U8(GT4_U32(car + 4) + 0xcbe1u)) {
        if ((int8_t)GT4_U8(b + 0x466) == 0) {
            if (!model_line(car, 7, 0)) return 0;
        } else {
            uint8_t s = GT4_U8(b + 0x467);
            if ((s == 2 || s == 4 || s == 3) && !model_line(car, 6, 0)) return 0;
        }
    }
    GT4_U8(b + 0x467) = 0;
    GT4_U8(b + 0x633) = 30;                                  /* func_00353ED8 */
    return 1;
}

/* func_00343C20: 1 when the race is over for the car (time limit, or the
   session's laps done: 2 on its last lap) */
int gt4_car_race_over(uint32_t car)
{
    uint32_t b = car + 0x104, cond = GT4_U32(car + 4), lim, t;
    int32_t n, lap;
    if (GT4_U8(b + 0x4b0)) return 0;
    t = GT4_U32(cond + 0xf898u) / 3;                         /* func_00358440 */
    lim = GT4_U16(cond + 0xf850u) * 60000u;                  /* func_0034C140 / func_0034C158 */
    if (lim && !(t < lim)) return 1;
    n = (int32_t)gt4_session(cond, 0x128, 0, 0, 0, 0, 0, NULL);
    if (!n) return 0;
    lap = (int16_t)GT4_U16(b + 0x4ac);
    if (lap < n) return 0;
    if (GT4_F32(b + 0x4a4) < course_length(cond) * 0.5f) n++;
    return lap < n ? 2 : 1;
}

/* func_0035E000: the pit message of car idx */
static void race_event(uint32_t race, uint32_t idx, uint32_t kind)
{
    gt4_race_pit_message(race, idx, kind);
}

/* func_0035AD10: car idx stops in the pits: counted, the history of its
   stops shifted, the lap of this one kept */
static void pit_stop_count(uint32_t cond, uint32_t idx)
{
    uint32_t base = cond + idx * 8, car = car_at(cond, idx), b = car + 0x104;
    int32_t lap;
    race_event(GT4_U32(cond), idx, 1);
    GT4_U16(cond + 0xd9e4u + idx * 2) = (uint16_t)(GT4_U16(cond + 0xd9e4u + idx * 2) + 1);
    for (uint32_t j = 0; j < 3; j++) {
        GT4_U16(base + 0xd9f6u - j * 2) = GT4_U16(base + 0xd9f4u - j * 2);
        GT4_U8(base + 0xda26u - j * 2) = GT4_U8(base + 0xda24u - j * 2);
        GT4_U8(base + 0xda27u - j * 2) = GT4_U8(base + 0xda25u - j * 2);
    }
    lap = (int16_t)GT4_U16(b + 0x4ac);                       /* func_00343B50 */
    gt4_session(cond, 0x128, 0, 0, 0, 0, 0, NULL);
    if (lap > 0) {
        if (GT4_U8(b + 0x4b2)) lap = lap - 1;
    } else lap = 1;
    GT4_U16(base + 0xd9f0u) = (uint16_t)lap;
    GT4_U8(base + 0xda20u) = 0xff;
    GT4_U8(base + 0xda21u) = 0xff;
}

/* func_0035ADF0 */
static void pit_exit_event(uint32_t cond, uint32_t idx)
{
    int8_t k = (int8_t)GT4_U8(car_at(cond, idx) + 0x56a);
    if ((uint32_t)(k - 1) < 2) return;
    race_event(GT4_U32(cond), idx, 6);
}

/* func_0035AF18: what the stop will do (+0x4B8 bits): 4 tyres, 8 or 0xC
   with fuel to add (func_00354E38), 2 when the session says so, 1 when the
   player chooses; as bits 0x40/0x80 and 0x100 once set going */
static uint32_t pit_plan(uint32_t car)
{
    uint32_t b = car + 0x104, cond = GT4_U32(car + 4), s;
    if (GT4_U8(cond + 0xcbe0u) != 1) return 0;
    if (GT4_U8(b + 0x62e)) return 0;
    if (gt4_car_race_over(car) == 1) return 0;
    if (GT4_U8(GT4_U32(car + CAR_PARAMS) + 0x32) == 4) return 0;
    s = 0 < GT4_F32(car + 0x5d0) ? 12 : 4;
    if (gt4_session(cond, 0x198, 0, 0, 0, 0, 0, NULL)) s |= 2;
    if ((int8_t)GT4_U8(b + 0x466) != 2) {
        uint32_t view = GT4_U32(GT4_U32(car) + 0x84);
        if (gt4_vcall(view, 0x64, 0x120, view, 0, 0)) {
            if (s & 0xc) s = (s | 1) & 0xffff;
            return s;
        }
    }
    if (s & 4) s = (s | 0xc0) & 0xfffb;
    if (s & 8) s = (s | 0x100) & 0xfff7;
    return s;
}

/* func_00354D60: refuelling, at `rate` (0: D_00620E25 / 10) */
static void pit_refuel(uint32_t car, float rate)
{
    uint32_t b = car + 0x104, x;
    float full = GT4_F32(b + 0x4cc), v;
    if (!(GT4_U16(b + 0x4b8) & 0x100)) return;
    if (rate == 0) rate = (float)(int32_t)GT4_U8(0x00620e25u) * K(0x3dccccccu);
    v = GT4_F32(car + 0x5d4) + rate * GT4_F32(car + 0x54c);
    GT4_F32(car + 0x5d4) = v;
    if (!(full <= v)) return;
    GT4_F32(car + 0x5d4) = full;
    x = GT4_U16(b + 0x4b8) ^ 0x100;
    GT4_U16(b + 0x4b8) = (uint16_t)x;
    if (!(x & 0xc0)) GT4_U16(b + 0x4ba) = 0x5a;
}

static int crossed(float prev, float at, float cur) { return prev < at && at <= cur; }

/* func_0035B060: the pit lane for the car that moved from prev to cur
   along the course. */
static void pit_lane(uint32_t car, float cur, float prev)
{
    uint32_t b = car + 0x104, cond = GT4_U32(car + 4), plan;
    float len, half;
    int over;
    GT4_U8(b + 0x5c3) = 0;
    if (!(int16_t)GT4_U16(cond + 0xda5cu)) return;           /* func_0034C750: no pit lane */
    len = course_length(cond);
    half = len * 0.5f;
    if (cur < half) cur = cur + len;
    if (prev < half) prev = prev + len;
    if (GT4_U16(b + 0x4ba)) gt4_car_signal(car, 11);
    if (!GT4_U16(b + 0x4ba) && (GT4_U8(b + 0x4b1) & 1)) {
        /* going the wrong way: out of the lane where it runs backwards */
        uint8_t f = GT4_U8(b + 0x4b1);
        float v;
        if ((uint8_t)(GT4_U8(b + 0x466) - 1) < 2 || GT4_U8(b + 0x467) || !(f & 0x10)) return;
        if (!(pit_mark(cond, 3) < cur) || !(cur < pit_mark(cond, -2))) return;
        v = GT4_F32(b + 0x53c);
        if (K(0x4131c71bu) < v) GT4_U8(b + 0x5c3) = 2;
        else if (K(0x4031c71bu) < v) GT4_U8(b + 0x5c3) = 1;
        {
            float m = pit_mark(cond, -3);
            if (m < prev && cur <= m) {
                pit_abandon(car);
                gt4_car_signal(car, 12);
                return;
            }
        }
        {
            float x = GT4_F32(b + 0x53c), y, lim = K(0x3fb1c71bu), nlim = K(0xbfb1c71bu);
            if (!(lim < x || x < nlim)) {
                y = GT4_F32(b + 0x540);
                if (!(lim < y) && !(y < nlim)) return;
            }
        }
        {
            uint32_t ai = cond + 0xda78u + GT4_U8(b + 0x440) * 0x4f0u;
            if (GT4_U32(ai + 0x43c) || GT4_U32(ai + 0x440)) return;   /* func_0045CC08 */
        }
        gt4_car_signal(car, 2);
        return;
    }

    plan = GT4_U16(b + 0x4b8);
    over = gt4_car_race_over(car);
    if (over == 2) over = 0;
    switch (GT4_U8(b + 0x467)) {
    case 2: {                                                /* going in */
        float m1 = pit_mark(cond, 1), box;
        if (m1 <= cur && m1 < len && !(GT4_U8(b + 0x4b1) & 0x10)) {
            gt4_car_pit_leave(car);
            gt4_car_signal(car, 0);
            GT4_U16(b + 0x4b8) = 0;
            GT4_U8(b + 0x4b1) &= 0xf7;
            if (GT4_U8(b + 0x62e) == 2) GT4_U8(b + 0x62e) = 1;
            return;
        }
        if (crossed(prev, m1, cur)) {
            pit_stop_count(cond, GT4_U8(b + 0x440));
            GT4_U16(b + 0x4b8) = (uint16_t)pit_plan(car);
        }
        box = pit_box(cond, (int8_t)GT4_U8(b + 0x4b7)) - 25.0f;
        if (crossed(prev, box, cur)) {
            if (over) {
                pit_abandon(car);
                gt4_car_signal(car, 8);
            } else if (!plan) {
                pit_abandon(car);
                gt4_car_signal(car, 9);
            } else if (!(K(0x41de38e2u) <= GT4_F32(b + 0x53c))) pit_to_box(car);   /* under 100 km/h */
            else {
                pit_abandon(car);
                gt4_car_signal(car, 7);
            }
            return;
        }
        if (pit_mark(cond, 2) <= cur) gt4_car_signal(car, 10);
        return;
    }
    case 3: {                                                /* at the box */
        uint32_t t = GT4_U16(b + 0x4ba);
        if (!t) {
            float box = pit_box(cond, (int8_t)GT4_U8(b + 0x4b7));
            if (plan && crossed(prev, box, cur)) {
                gt4_car_rest(car);
                GT4_U16(b + 0x4ba) = 0x2d1;
                GT4_U16(b + 0x64c) = (int8_t)GT4_U8(b + 0x466) == 1 && (int16_t)GT4_U16(b + 0x64c) != -1 ? 300 : 0;
            } else if (box + 0.5f <= cur) pit_abandon(car);
        } else if (t == 0x2d0) {
            float h;
            GT4_U16(b + 0x64c) = 0;
            h = pit_box_heading(cond, (int8_t)GT4_U8(b + 0x4b7));
            GT4_F32(b + 0x570) = h;
            gt4_rotation_from_angles(gt4_ptr(b + 0xc), gt4_ptr(b + 0x18), gt4_ptr(b + 0x24),
                                     GT4_F32(b + 0x568), GT4_F32(b + 0x56c), h);
            gt4_car_signal(car, 11);
        } else if (t == 1) {
            GT4_U16(b + 0x4b8) = 0;
            GT4_U8(b + 0x4b1) &= 0xfb;
        }
        {
            uint32_t p = GT4_U16(b + 0x4b8);
            if (!(p & 0x100)) return;
            if (GT4_U16(b + 0x4ba) == 0x285) GT4_U16(b + 0x4b8) = (uint16_t)(p | 0x20);
            else if ((uint16_t)(GT4_U16(b + 0x4ba) - 1) < 0x22a) pit_refuel(car, 0);
        }
        return;
    }
    case 4: {                                                /* going out */
        float m1 = pit_mark(cond, 1), a, c;
        if (m1 <= cur && cur < len && !(GT4_U8(b + 0x4b1) & 0x10)) {
            gt4_car_pit_leave(car);
            gt4_car_signal(car, 0);
            if (GT4_U8(b + 0x62e) == 2) GT4_U8(b + 0x62e) = 1;
        }
        if ((int8_t)GT4_U8(b + 0x466) == 0 && !GT4_U8(b + 0x4b2)) {
            a = pit_mark(cond, -2);
            c = pit_mark(cond, -3);
        } else {
            a = pit_mark(cond, -1);
            c = pit_mark(cond, -2);
        }
        if (len + len * 0.5f <= a) a = a - len;
        if (crossed(prev, a, cur)) {
            if (GT4_U8(b + 0x4b2)) {
                gt4_car_finish_drive(car, 3);
            } else {
                gt4_car_pit_leave(car);
                if (!over) pit_exit_event(cond, GT4_U8(b + 0x440));
            }
            GT4_U8(b + 0x4b1) &= 0xf7;
            if (GT4_U8(b + 0x62e) == 2) GT4_U8(b + 0x62e) = 0;
            return;
        }
        if (c <= cur) {
            int8_t k = (int8_t)GT4_U8(b + 0x466);
            if (over || (k != 0 && k != 3)) return;
            gt4_car_signal(car, GT4_U8(b + 0x461) ? 5 : 4);
            return;
        }
        if (!GT4_U8(b + 0x4b2)) gt4_car_signal(car, 10);
        return;
    }
    }
    /* on the course */
    if ((int8_t)GT4_U8(b + 0x466) == 0 && GT4_U8(b + 0x467) != 1) {
        if (GT4_U8(b + 0x62e)) {
            float m0 = pit_mark(cond, 0);
            if (crossed(prev, m0, cur)) {
                pit_enter(car);
                if (!GT4_U8(b + 0x62e) && 0 < GT4_F32(b + 0x53c)) pit_stop_count(cond, GT4_U8(b + 0x440));
                GT4_U16(b + 0x4b8) = (uint16_t)pit_plan(car);
                if (GT4_U8(b + 0x62e)) GT4_U8(b + 0x62e) = 2;
                return;
            }
        }
        if (!(GT4_U8(b + 0x4b1) & 0x10)) return;
        {
            float m1 = pit_mark(cond, 1), m2;
            if (crossed(prev, m1, cur)) {
                if (GT4_F32(b + 0x53c) < 0) pit_abandon(car);
                else {
                    pit_enter(car);
                    pit_stop_count(cond, GT4_U8(b + 0x440));
                    GT4_U16(b + 0x4b8) = (uint16_t)pit_plan(car);
                }
                if (GT4_U8(b + 0x62e)) GT4_U8(b + 0x62e) = 2;
                return;
            }
            m2 = pit_mark(cond, 2);
            if (crossed(prev, m2, cur)) {
                pit_abandon(car);
                GT4_U16(b + 0x4b8) = 0;
                gt4_car_signal(car, 9);
            }
        }
        return;
    }
    {
        uint8_t f = GT4_U8(b + 0x4b1);
        if (f & 0x10) {
            uint32_t ai = cond + 0xda78u + GT4_U8(b + 0x440) * 0x4f0u;
            if (GT4_U32(ai + 0x43c) && cur < len) {             /* func_0045CC30 */
                pit_enter(car);
                GT4_U8(car + 0x14 + 0x1b) = 2;                   /* func_00463E80 */
                return;
            }
            f = GT4_U8(b + 0x4b1);
        }
        if (!(f & 8) || over) return;
        if (crossed(prev, pit_mark(cond, 0), cur)) pit_enter(car);
    }
}

/* func_00359008: the laps of car idx, its signal lights (moving: 1, slow:
   0; in the pits: 4 or 5), then the pit lane (func_0035B060). */
void gt4_car_laps(uint32_t cond, uint32_t idx)
{
    uint32_t car = car_at(cond, idx), b = car + 0x104;
    float prev;
    if (GT4_U8(cond + 0xcbdeu) == 2) return;
    prev = GT4_F32(car + 0x99c);
    if ((int32_t)GT4_U32(GT4_U32(cond) + 0xbc) > 0) return;             /* func_0034C210 */
    if (lap_progress(cond, idx, prev) == 1) {
        float v0 = GT4_F32(b + 0x53c), v1 = GT4_F32(b + 0x540), speed;
        GT4_U8(b + 0x4b1) |= 1;
        speed = ps2_sqrt(v0 * v0 + v1 * v1);
        if (K(0x3fb1c71bu) < speed) {                        /* 5 km/h */
            uint32_t ai = cond + 0xda78u + idx * 0x4f0u;
            int busy = GT4_U32(ai + 0x43c) || GT4_U32(ai + 0x440);   /* func_0045CC08 */
            if (!busy && !((uint8_t)(GT4_U8(b + 0x466) - 1) < 2) && !GT4_U8(b + 0x467)) gt4_car_signal(car, 1);
        } else if (speed < K(0x3f555554u)) gt4_car_signal(car, 0);   /* 3 km/h */
    } else {
        GT4_U8(b + 0x4b1) &= 0xfe;
        gt4_car_signal(car, 0);
    }
    if (GT4_U8(b + 0x467) == 1) {
        int8_t k = (int8_t)GT4_U8(b + 0x466);
        if ((k == 0 || k == 3) && !GT4_U8(b + 0x4b2)) gt4_car_signal(car, GT4_U8(b + 0x461) ? 5 : 4);
    }
    pit_lane(car, GT4_F32(b + 0x4a4), prev);
}
