/* The race's end for one car (func_00353218): laps or time done, fuel for
   the rest of the distance, worn-out or punctured tyres; sets the car's
   retire flags (car+0x5b5 bits 4 and 8, car+0x6c6). */
#include "car.h"

#define K(bits) gt4_f32(bits)

/* func_003C0FF8: the offered A-spec points. Entry+0x18 is its car,
   car+0x10 the parameters, +0xFE0 the rating. func_00447550 reads the
   course kind (settings+0xA0, COURSE row+0x10); func_00445E20 reads the
   drivetrain kind (garage+0x40, its row+0x10). Arithmetic stays in the
   original order and rounding mode, including the final +0.5 truncate. */
uint32_t gt4_race_aspec_points(uint32_t race, int32_t i, const gt4_specdb_src *db)
{
    uint32_t entries = GT4_U32(race + 0x60), list = GT4_U32(entries + 8);
    int32_t n = (int32_t)GT4_U32(entries), mode;
    uint32_t entry, world, fn;
    float player_rating, sum = 0, adjustment = K(0x3cf5c28fu), delta, points;
    uint8_t row[0x100] = { 0 };
    if (i < 0 || i >= n) return 0;
    entry = GT4_U32(list + (uint32_t)i * 4);
    player_rating = GT4_F32(GT4_U32(GT4_U32(entry + 0x18) + 0x10) + 0xfe0);
    if (n == 1) return 0;
    world = GT4_U32(race + 0x84);
    fn = GT4_U32(GT4_U32(world + 0x64) + 0x2fc);
    mode = fn == 0x005f6980u ? (int32_t)GT4_U32(world + 0xd68)
                            : (int32_t)gt4_vcall(world, 0x64, 0x2f8, 0, 0, 0);
    if (mode == 10) return 250;
    if (db && db->row(db->ctx, 0x22, GT4_U32(GT4_U32(race + 0x70) + 0xa0), 0, row, sizeof row) > 0 && row[0x10] == 2) {
        uint32_t gar = entry + 0x20;
        db->row(db->ctx, GT4_U32(gar + 0x44), GT4_U32(gar + 0x40), 0, row, sizeof row);
        if (row[0x10] != 2) adjustment = K(0x3d8f5c28u);
    }
    for (int32_t k = 0; k < n; k++)
        if (k != i) {
            uint32_t e = GT4_U32(list + (uint32_t)k * 4);
            sum += GT4_F32(GT4_U32(GT4_U32(e + 0x18) + 0x10) + 0xfe0);
        }
    delta = sum / (float)(n - 1);
    delta += adjustment;
    delta -= player_rating;
    if (delta < 0) {
        points = delta * K(0x41200000u);
        points += K(0x3f800000u);
        if (points < K(0x3dcccccCu)) points = K(0x3dcccccCu);
    } else {
        points = delta * K(0x43480000u);
        if (points < 1) points = 1;
    }
    points *= K(0x41200000u);
    if (points > 200) points = 200;
    return (uint32_t)(points + K(0x3f000000u)) & 0xffu;
}

/* func_0034C230 (func_00395400): the course's length. */
static float course_length(uint32_t session)
{
    return GT4_F32(GT4_U32(GT4_U32(GT4_U32(session + 0xcbd8u) + 4) + 0xdc) + 0x14);
}

/* func_00343C20: 0 racing, 1 the race is over for this car, 2 over and
   past the half lap. */
static int race_over(uint32_t car)
{
    uint32_t session = GT4_U32(car + CAR_SESSION), time, limit, laps;
    int32_t lap;
    if (GT4_U8(car + 0x5b4)) return 0;
    time = GT4_U32(session + 0xf898u) / 3u;                         /* func_00358440 */
    limit = GT4_U16(session + 0xf850u) * 60000u;                    /* func_0034C140 */
    if (limit && !(time < limit)) return 1;                         /* func_0034C158 */
    laps = gt4_vcall(session, 0x10140u, 0x128, time, 0, 0);
    if (!laps) return 0;
    lap = (int16_t)GT4_U16(car + 0x5b0);
    if (lap < (int32_t)laps) return 0;
    if (GT4_F32(car + 0x5a8) < course_length(session) * 0.5f) laps++;
    return lap < (int32_t)laps ? 2 : 1;
}

void gt4_car_race_end(uint32_t car)
{
    uint32_t session = GT4_U32(car + CAR_SESSION), s = session + 4, layout = gt4_car_layout(car);
    int n = GT4_U8(layout + LAYOUT_WHEELS), out = 0;

    GT4_U8(car + 0x6c6) = 0;
    if (!gt4_vcall(session, 0x10140u, 0x180, session, 0, 0)) return;
    if ((GT4_U8(car + 0x5b5) & 8) || GT4_U8(car + 0x5b6) || race_over(car)) return;
    if (0 < GT4_F32(car + 0x5d0) && 500.0f < GT4_F32(car + 0x5cc)) {
        /* fuel for the distance left */
        float end = gt4_line_point_distance(session + 0xda50u, 1);
        if (GT4_F32(car + 0x5a8) < end) {
            float len = course_length(session), need;
            need = (((end - GT4_F32(car + 0x5a8)) + len) / (GT4_F32(car + 0x5cc) / GT4_F32(car + 0x5c8))) * K(0x3f866666u);
            if (GT4_F32(car + 0x5d4) < need) out = 1;
        }
    }
    if (GT4_U8(s + 0x68)) {
        for (int i = 0; i < n; i++) {
            uint32_t w = gt4_car_wheel(car, i);
            if (GT4_F32(w + 0xac) < 0) GT4_U8(car + 0x6c6) = 1;
            else if (GT4_F32(w + 0x50) < GT4_F32(s + 0x20)) {
                out |= 1;
                GT4_U8(car + 0x6c6) = 1;
                break;
            }
        }
    }
    if (!(int8_t)GT4_U8(car + CAR_56A) && gt4_vcall(session, 0x10140u, 0x160, session, 0, 0) == 4) {   /* func_0035FA50 */
        for (int i = 0; i < n; i++)
            if (GT4_U8(gt4_car_wheel(car, i) + 0x47)) { out |= 2; break; }
    }
    if (out) {
        uint8_t f = GT4_U8(car + 0x5b5) | 4;
        int8_t mode = (int8_t)GT4_U8(car + CAR_56A);
        GT4_U8(car + 0x5b5) = f;
        if (mode == 2 || (mode == 1 && !GT4_U8(car + 0x6c5))) GT4_U8(car + 0x5b5) = f | 8;
    }
}

/* func_003658F0: laps done (less the one in progress at the start). */
static int32_t laps_done(uint32_t car)
{
    int32_t lap = (int16_t)GT4_U16(car + 0x5b0);
    return GT4_U8(car + 0x5b6) ? lap : lap - GT4_U8(car + 0x5b4);
}

/* func_00365E80: how far each car is ahead of each other one, in laps
   (s8 at conductor + 0xccc8 + a * 6 + b) and metres (conductor + 0xccec +
   a * 0x18 + b * 4) (func_00365910). */
void gt4_race_gaps(uint32_t conductor)
{
    int n = (int)GT4_U32(conductor + 0xf8a0u);
    for (int a = 0; a < n; a++)
        for (int b = 0; b < n; b++) {
            uint32_t ca = conductor + 0x6698u + (uint32_t)a * CAR_SIZE, cb = conductor + 0x6698u + (uint32_t)b * CAR_SIZE;
            int32_t lap;
            float d;
            if (a == b) continue;
            lap = laps_done(cb) - laps_done(ca);
            d = GT4_F32(cb + 0x5a8) - GT4_F32(ca + 0x5a8);
            if (lap > 0 && d < 0) { lap--; d = d + course_length(conductor); }
            else if (lap < 0 && 0 < d) { lap++; d = d - course_length(conductor); }
            if (lap < -127) lap = -127;
            if (!(lap < 128)) lap = 127;
            GT4_U8(conductor + 0xccc8u + (uint32_t)a * 6 + (uint32_t)b) = (uint8_t)lap;
            GT4_F32(conductor + 0xccecu + (uint32_t)a * 0x18 + (uint32_t)b * 4) = d;
        }
}

/* func_00366588: at frame 179 of the finish sequence, cars still driven by
   their players are handed to the driver model. */
void gt4_race_finish_handover(uint32_t conductor)
{
    uint32_t race = GT4_U32(conductor);
    if (!GT4_U32(race + 0xc4) || (int32_t)GT4_F32(race + 0xc8) != 0xb3) return;
    for (int i = 0; i < (int)GT4_U32(conductor + 0xf8a0u); i++) {
        uint32_t car = conductor + 0x6698u + (uint32_t)i * CAR_SIZE;
        if (GT4_U8(car + 0x5b6)) continue;
        GT4_U8(car + 0x736) = 1;
        if ((int8_t)GT4_U8(car + CAR_56A)) continue;
        GT4_U8(car + CAR_56A) = 2;
        if ((uint8_t)(GT4_U8(car + CAR_56B) - 2) < 3) continue;
        gt4_ai_line_switch(car + 0x14, 6, 1);
    }
}

/* func_00365688: the catch-up of a car behind (t = self+0x7C: +0xC the
   most, full from +0x14 metres back, nothing within +0x10; none when the
   car holds, flag 0x10 of +0x4B1). */
static float catch_up(uint32_t t, uint32_t b, int32_t lap, float x)
{
    float lo, hi;
    if (GT4_U8(b + 0x4b1) & 0x10) return 1.0f;
    if (0 < lap) return GT4_F32(t + 0xc) + 1.0f;
    hi = GT4_F32(t + 0x14);
    if (hi <= x) return GT4_F32(t + 0xc) + 1.0f;
    lo = GT4_F32(t + 0x10);
    if (lo < x) return GT4_F32(t + 0xc) * ((x - lo) / (hi - lo)) + 1.0f;
    return 1.0f;
}

/* func_00365718: the holding back of a car ahead (t: +0 the most, full
   from +8 metres ahead, nothing within +4). */
static float hold_back(uint32_t t, int32_t lap, float x)
{
    float lo, hi;
    if (0 < lap) return 1.0f - GT4_F32(t);
    hi = GT4_F32(t + 8);
    if (hi <= x) return 1.0f - GT4_F32(t);
    lo = GT4_F32(t + 4);
    if (lo < x) return 1.0f - GT4_F32(t) * ((x - lo) / (hi - lo));
    return 1.0f;
}

/* func_00365790 (the race sessions' method 0x20): car b's speed factor
   (b+0x444, cars at +0x104) against car a, times k (the chain so far),
   at most 1.3, times the session's +0x94; b racing on only. Returns the
   factor before +0x94. */
float gt4_race_rubber_band(uint32_t self, uint32_t ba, uint32_t bb, float k)
{
    float f = 1.0f, d;
    int32_t lap;
    if (!GT4_U8(bb + 0x4b2)) {
        /* func_00365910: a ahead of b in laps and metres */
        lap = laps_done(ba - 0x104) - laps_done(bb - 0x104);
        d = GT4_F32(ba + 0x4a4) - GT4_F32(bb + 0x4a4);
        if (0 < lap && d < 0) { lap--; d = d + course_length(self); }
        else if (lap < 0 && 0 < d) { lap++; d = d - course_length(self); }
        if (0 < lap || (lap == 0 && 0 < d)) {
            if (GT4_F32(self + 0x88) != 0) f = catch_up(self + 0x7c, bb, lap, d);
        } else if (GT4_F32(self + 0x7c) != 0) {
            f = hold_back(self + 0x7c, -lap, -d);
        }
    }
    f = f * k;
    if (K(0x3fa66666u) < f) f = K(0x3fa66666u);
    GT4_F32(bb + 0x444) = f * GT4_F32(self + 0x94);
    return f;
}

/* ---- The race object (race = the conductor's +0): its event queue, the
   start countdown, and what it does before and after each step. ---- */

/* The event queue at race+0xF4 (func_003B7470..003B77E0), read by the
   presentation: 256 events {s16 kind, s16 extra, u32 value} from +8, a
   count per kind at +0x808 (0x1C of them), the number at +0x824. The
   original guards the removal with a semaphore (another thread reads). */
#define Q_EVENT(q, i) ((q) + 8u + (uint32_t)(i) * 8u)
#define Q_KINDS 0x808u
#define Q_COUNT 0x824u

void gt4_race_queue_reset(uint32_t q)                   /* func_003B7470 */
{
    GT4_U32(q + Q_COUNT) = 0;
    memset(gt4_ptr(q + Q_KINDS), 0, 0x1c);
}

/* func_003B7498: event i removed, the later ones moved up */
static void queue_remove(uint32_t q, int32_t i)
{
    uint32_t kind = (uint32_t)(int32_t)(int16_t)GT4_U16(Q_EVENT(q, i));
    GT4_U8(q + kind + Q_KINDS) = (uint8_t)(GT4_U8(q + kind + Q_KINDS) - 1);
    GT4_U32(q + Q_COUNT) = GT4_U32(q + Q_COUNT) - 1;
    for (; i < (int32_t)GT4_U32(q + Q_COUNT); i++)
        memcpy(gt4_ptr(Q_EVENT(q, i)), gt4_ptr(Q_EVENT(q, i + 1)), 8);
}

/* func_003B76F0: an event added; with the queue full, the one of the
   highest kind goes to make room, unless the new one's kind is higher */
void gt4_race_queue_push(uint32_t q, int32_t kind, uint32_t value, int32_t extra)
{
    uint32_t at;
    if (GT4_U32(q + Q_COUNT) == 0x100) {
        int32_t top = -1, i, n = 0;
        for (i = 0; i < 0x100; i++) {
            int32_t k = (int16_t)GT4_U16(Q_EVENT(q, i));
            if (top < k) {
                top = k;
                n = i;
            }
        }
        if (top < kind) return;
        queue_remove(q, n);
    }
    GT4_U8(q + (uint32_t)kind + Q_KINDS) = (uint8_t)(GT4_U8(q + (uint32_t)kind + Q_KINDS) + 1);
    at = Q_EVENT(q, GT4_U32(q + Q_COUNT));
    GT4_U16(at) = (uint16_t)kind;
    GT4_U16(at + 2) = (uint16_t)extra;
    GT4_U32(at + 4) = value;
    GT4_U32(q + Q_COUNT) = GT4_U32(q + Q_COUNT) + 1;
}

/* func_003B7668: the first event of a kind (its value to *value when
   asked), taken out when `take`; 0 when there is none */
int gt4_race_queue_take(uint32_t q, int32_t kind, uint32_t *value, int take)
{
    if (!GT4_U8(q + (uint32_t)kind + Q_KINDS)) return 0;
    for (int32_t i = 0; i < (int32_t)GT4_U32(q + Q_COUNT); i++) {
        if ((int16_t)GT4_U16(Q_EVENT(q, i)) != kind) continue;
        if (value) *value = GT4_U32(Q_EVENT(q, i) + 4);
        if (take) queue_remove(q, i);
        return 1;
    }
    return 0;
}

/* func_003B75D8, once a frame of the world (func_00389758): each event's
   count (+2, the `extra` it was posted with) goes down by one and the
   event leaves when it reaches 0; those posted with 0 stay until taken */
void gt4_race_queue_age(uint32_t q)
{
    for (int32_t i = 0; i < (int32_t)GT4_U32(q + Q_COUNT); i++) {
        uint16_t n = GT4_U16(Q_EVENT(q, i) + 2);
        if (!n) continue;
        GT4_U16(Q_EVENT(q, i) + 2) = (uint16_t)(n - 1);
        if (n == 1) queue_remove(q, i--);
    }
}

/* func_003B77E0: the n-th event of a kind, left in place */
int gt4_race_queue_peek(uint32_t q, int32_t kind, uint32_t *value, int32_t n)
{
    if (!(n < (int32_t)GT4_U8(q + (uint32_t)kind + Q_KINDS))) return 0;
    for (int32_t i = 0; i < (int32_t)GT4_U32(q + Q_COUNT); i++) {
        if ((int16_t)GT4_U16(Q_EVENT(q, i)) != kind) continue;
        if (n) {
            n--;
            continue;
        }
        if (value) *value = GT4_U32(Q_EVENT(q, i) + 4);
        return 1;
    }
    return 0;
}

/* The race's messages (the classes at D_0067E708.., func_003A3148 posts
   one as an event of extra 2): kinds 0x10 (a lap), 0x11 (a gap), 0x12
   (laps behind) pack the car (4 bits), a field (2 bits) and a time
   clamped to +-0x1FFFFFF (func_003A3520); kind 0xE the car, a field (4
   bits) and a number up to 999 (func_003A3408). */
static uint32_t clamp4(int32_t v) { return (uint32_t)(v < 0 ? 0 : v > 15 ? 15 : v); }

uint32_t gt4_race_message(int32_t kind, int32_t car, int32_t field, int32_t value)
{
    if (kind == 0xe) {
        int32_t n = value < 0 ? 0 : value > 999 ? 999 : value;
        return clamp4(car) | clamp4(field) << 4 | ((uint32_t)n & 0x3ffu) << 8;
    }
    if (value < -0x1ffffff) value = -0x1ffffff;
    else if (0x1ffffff < value) value = 0x1ffffff;
    return clamp4(car) | ((uint32_t)field & 3u) << 4 | (uint32_t)value << 6;
}

void gt4_race_post(uint32_t race, int32_t kind, int32_t car, int32_t field, int32_t value)
{
    gt4_race_queue_push(race + 0xf4, kind, gt4_race_message(kind, car, field, value), 2);
}

/* func_003C12B0: the race's records: none */
void gt4_race_records_reset(uint32_t r)
{
    GT4_U32(r) = 0xfffffffeu;
    GT4_U32(r + 4) = 0x157529ffu;
    GT4_U32(r + 0x24) = 0;
    GT4_U32(r + 8) = 0;
    GT4_U32(r + 0xc) = 0;
    GT4_U32(r + 0x10) = 0;
    GT4_U8(r + 0x14) = 0;
}

/* func_003C0190: the race begins: no records, no events, the countdown
   armed with the start's kind and seconds from the race's settings
   (race+0x70: +0x40, +0x41), the time limit (func_003C00B8: race+0xC4
   minutes, +0xC8 its seconds plus 3 as a float, +0xC0 whether there is
   one). */
void gt4_race_begin(uint32_t race)
{
    uint32_t s = GT4_U32(race + 0x70), m;
    gt4_race_records_reset(race + 0x88);
    gt4_race_queue_reset(race + 0xf4);
    GT4_U32(race + 0xb4) = GT4_U8(s + 0x40);
    GT4_U32(race + 0xb0) = 1;
    GT4_U32(race + 0xb8) = GT4_U8(s + 0x41);
    m = GT4_U32(race + 0xc4);
    GT4_U32(race + 0xc8) = 0;
    GT4_U32(race + 0xcc) = 0;
    if (m) GT4_F32(race + 0xc8) = (float)(int32_t)(m * 60u + 0xb4u);
    GT4_U32(race + 0xc0) = 0.0f < GT4_F32(race + 0xc8);
    GT4_F32(race + 0xd4) = -1.0f;
    GT4_F32(race + 0xd8) = 1.0f;
    GT4_U8(race + 0x96c) = 0;
    GT4_U32(race + 0xe8) = 0;
}

/* func_00387FB0, 003883EC..0038842C: session virtual method +0x50
   selects the start. Types 1 and unrecognized types retain RACE +0x40;
   types 0/3 roll, type 2 (Time Trial) has no countdown. */
void gt4_race_select_start(uint32_t race, uint32_t session_kind)
{
    if (session_kind == 0 || session_kind == 3)
        GT4_U32(race + 0xb4) = UINT32_MAX;
    else if (session_kind == 2)
        GT4_U32(race + 0xb4) = 3;
}

/* func_003C0208: before each step. An armed countdown starts by its kind
   (race+0xB4, plus one: 0 a rolling start, event 0x18 of 12, no wait; 2
   eight seconds, 0x18 of 19; 3 ten seconds, 0x18 of 10; 4 none; else the
   seconds already at +0xB8), the course's start lights set to it
   (func_003973D0, the presentation's); then the frames left (+0xBC, every
   car held) count down, event 4 at zero: the start. */
void gt4_race_before_step(uint32_t race)
{
    uint32_t q = race + 0xf4;
    if (GT4_U32(race + 0xb0)) {
        uint32_t k = GT4_U32(race + 0xb4) + 1;
        GT4_U32(race + 0xb0) = 0;
        switch (k) {
        case 0: gt4_race_queue_push(q, 0x18, 0xc, 0); GT4_U32(race + 0xb8) = 0; break;
        case 2: GT4_U32(race + 0xb8) = 8; gt4_race_queue_push(q, 0x18, 0x13, 0); break;
        case 3: GT4_U32(race + 0xb8) = 10; gt4_race_queue_push(q, 0x18, 10, 0); break;
        case 4: GT4_U32(race + 0xb8) = 0; break;
        default: break;
        }
        GT4_U32(race + 0xbc) = GT4_U32(race + 0xb8) * 60u;
        if (!gt4_phys_headless)
            gt4_orig_call(0x003973d0u, GT4_U32(GT4_U32(GT4_U32(GT4_U32(race + 0x84) + 0x6c) + 0x80) + 4),
                          0, (float)(int32_t)GT4_U32(race + 0xb8), 0);
    }
    if (GT4_U32(race + 0xbc)) {
        GT4_U32(race + 0xbc) = GT4_U32(race + 0xbc) - 1;
        if (!GT4_U32(race + 0xbc)) gt4_race_queue_push(q, 4, 0xffffffffu, 0);
    }
}

/* func_003B6FD0: the entries linked in race order (the car's position,
   entry+0x18 then +0x5B3): head at +0x14, tail at +0x18 of the list, each
   entry's previous at +0xC and next at +0x10 */
static void entries_in_order(uint32_t list)
{
    uint32_t n = GT4_U32(list);
    GT4_U32(list + 0x14) = 0;
    GT4_U32(list + 0x18) = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t e = GT4_U32(GT4_U32(list + 8) + i * 4), at = GT4_U32(list + 0x14), prev = 0;
        int8_t pos = (int8_t)GT4_U8(GT4_U32(e + 0x18) + 0x5b3);
        while (at && !(pos < (int8_t)GT4_U8(GT4_U32(at + 0x18) + 0x5b3))) {
            prev = at;
            at = GT4_U32(at + 0x10);
        }
        if (!prev) GT4_U32(list + 0x14) = e;
        else GT4_U32(prev + 0x10) = e;
        if (!at) GT4_U32(list + 0x18) = e;
        else GT4_U32(at + 0xc) = e;
        GT4_U32(e + 0x10) = at;
        GT4_U32(e + 0xc) = prev;
    }
}

/* func_003C0310: after each step. The entries in race order; event 8
   ends the time limit (+0xC0); events 15 say whether the race may go on
   (+0xE8 1 when one of them is 0); each gap message (0x11) sets its car's
   entry's shown gap (+0x11E8, +0x11EC 0), each laps-behind one (0x12)
   the laps instead (+0x11E8 no time, +0x11EC the laps). */
void gt4_race_after_step(uint32_t race)
{
    uint32_t q = race + 0xf4, v;
    entries_in_order(GT4_U32(race + 0x60));
    if (gt4_race_queue_take(q, 8, NULL, 1)) GT4_U32(race + 0xc0) = 0;
    GT4_U32(race + 0xe8) = 0;
    while (gt4_race_queue_take(q, 0xf, &v, 1))
        if (!v) GT4_U32(race + 0xe8) = 1;
    for (int32_t k = 0; k < (int32_t)GT4_U8(q + 0x11 + Q_KINDS); k++) {
        uint32_t e;
        int32_t t;
        if (!gt4_race_queue_peek(q, 0x11, &v, k)) continue;
        e = GT4_U32(GT4_U32(GT4_U32(race + 0x60) + 8) + (v & 0xf) * 4);
        if (!e) continue;
        t = (int32_t)v >> 6;                                   /* func_003A3620 */
        GT4_U32(e + 0x11ec) = 0;
        GT4_U32(e + 0x11e8) = 0x3fffffcu < (uint32_t)t + 0x1fffffeu ? 0x157529ffu : (uint32_t)t;
    }
    for (int32_t k = 0; k < (int32_t)GT4_U8(q + 0x12 + Q_KINDS); k++) {
        uint32_t e;
        if (!gt4_race_queue_peek(q, 0x12, &v, k)) continue;
        e = GT4_U32(GT4_U32(GT4_U32(race + 0x60) + 8) + (v & 0xf) * 4);
        if (!e) continue;
        GT4_U32(e + 0x11e8) = 0x157529ffu;
        GT4_U32(e + 0x11ec) = (uint32_t)((int32_t)v >> 6);
    }
}

/* ---- The race's events from the step (func_0035DF80..0035E8E8): what
   the presentation shows (the event queue) and what the results keep
   (each entry's lap and split times, the race's best). None when the race
   is a replay (race+0xDC) or the car is not shown (+0x632, +0x736). ---- */

static uint32_t cond_of(uint32_t race) { return GT4_U32(GT4_U32(race + 0x84) + 0x70); }
static uint32_t car_of(uint32_t cond, uint32_t i) { return cond + 0x6698u + i * CAR_SIZE; }   /* func_0034C190 */
static uint32_t entry_of(uint32_t race, uint32_t i) { return GT4_U32(GT4_U32(GT4_U32(race + 0x60) + 8) + i * 4); }

/* func_003C0E78: whether the race is a replay (the world's +0x298) */
int gt4_race_replay(uint32_t race)
{
    uint32_t world = GT4_U32(race + 0x84);
    if (!world) return 0;
    return gt4_vcall(world, 0x64, 0x298, 0, 0, 0) != 0;
}

/* func_0035DF80: race event `kind` for car i (the value), `extra` */
void gt4_race_event(uint32_t race, int32_t kind, uint32_t i, int32_t extra)
{
    uint32_t car = car_of(cond_of(race), i);
    if (GT4_U32(race + 0xdc) || GT4_U8(car + 0x104 + 0x632)) return;
    gt4_race_queue_push(race + 0xf4, kind, i, extra);
}

/* func_0035E078: the gap of car i (message 0x11; out of range, the most) */
void gt4_race_gap(uint32_t race, uint32_t i, uint32_t gap)
{
    uint32_t car = car_of(cond_of(race), i);
    if (GT4_U8(car + 0x736) || GT4_U32(race + 0xdc)) return;
    if (0x3fffffdu < gap + 0x1fffffeu) gap = 0x1ffffff;          /* func_003A35F0 */
    gt4_race_post(race, 0x11, (int32_t)i, 0, (int32_t)gap);
}

/* func_0035E8E8: car i laps behind (message 0x12) */
void gt4_race_behind(uint32_t cond, uint32_t i, uint32_t laps)
{
    uint32_t race = GT4_U32(cond);
    if (GT4_U8(car_of(cond, i) + 0x736) || GT4_U32(race + 0xdc)) return;
    gt4_race_post(race, 0x12, (int32_t)i, 0, (int32_t)laps);
}

/* func_0035E000: car i's pit message (0x14, func_003A3810: the car and 4
   bits of v; the rest of the word is what the original had in that
   register, the race's address) */
void gt4_race_pit_message(uint32_t race, uint32_t i, uint32_t v)
{
    uint32_t car = car_of(cond_of(race), i);
    if (GT4_U8(car + 0x736) || GT4_U32(race + 0xdc)) return;
    gt4_race_queue_push(race + 0xf4, 0x14, (race & 0xffffff00u) | (v & 0xfu) << 4 | clamp4((int32_t)i), 2);
}

/* func_0035E3F8: whether the race keeps times (not in a replay, not in
   the race kinds that do not, cond+0xCBDC, nor when race+0xEC says so) */
static int keeps_times(uint32_t race, uint32_t cond)
{
    return !(GT4_U8(cond + 0xcbdcu) || GT4_U32(race + 0xec) || GT4_U32(race + 0xdc));
}

/* A time in the race's messages (func_003A3660): the most past 0x3FFFFFE */
static int32_t message_time(uint32_t t) { return (int32_t)(0x3fffffeu < t ? 0x3ffffffu : t); }

/* func_003BB8C8: a set of splits (16, the lap at +0x40) copied when its
   lap is better or the other has none */
static void better_splits(uint32_t to, uint32_t from)
{
    uint32_t have = GT4_U32(to + 0x40);
    if (have != 0x157529ffu && !(GT4_U32(from + 0x40) < have)) return;
    for (uint32_t k = 0; k < 16; k++) GT4_U32(to + k * 4) = GT4_U32(from + k * 4);
    GT4_U32(to + 0x40) = GT4_U32(from + 0x40);
}

/* func_0035E550: car i finished lap `lap` in `time` (thirds of a frame).
   Its best lap (car+0x71C, func_0035E3C8); messages 0xD (the lap: the
   rest of the word is what the original had in that register, the car's
   address) and 0x10 (the time); when the race keeps times, the entry's
   laps (+0x1A0: a count, then up to 1000 times, the oldest dropped;
   func_003BBA80) and, unless `quiet`, its best (+0x11E0, func_003DD788),
   its splits of this lap against its best (+0x119C, +0x1158) and the
   race's (+0x928) (func_003C0CC8). */
void gt4_race_lap_event(uint32_t cond, uint32_t i, uint32_t lap, uint32_t time, int quiet)
{
    uint32_t car = car_of(cond, i), b = car + 0x104, race, e, l;
    int rec;
    if (time < GT4_U32(b + 0x618)) GT4_U32(b + 0x618) = time;
    if (GT4_U8(b + 0x441) || GT4_U8(b + 0x632)) return;
    race = GT4_U32(cond);
    rec = keeps_times(race, cond);
    if (!GT4_U32(race + 0xdc)) {
        int32_t n = (int32_t)lap < 0 ? 0 : (int32_t)lap > 999 ? 999 : (int32_t)lap;
        gt4_race_queue_push(race + 0xf4, 0xd, (car & 0xfffc00f0u) | clamp4((int32_t)i) | ((uint32_t)n & 0x3ffu) << 8, 2);
        gt4_race_post(race, 0x10, (int32_t)i, 0, message_time(time));
    }
    if (!rec) return;
    e = entry_of(race, i);
    l = e + 0x1a0;
    GT4_U32(l + 4) = GT4_U32(l + 4) + 1;
    if (!GT4_U32(l)) {
        GT4_U32(l + 0x10) = GT4_U32(l + 0x10) + time;
        if (GT4_U32(l + 0xc) == 1000) {
            GT4_U32(l + 8) = GT4_U32(l + 8) + 1;
            memmove(gt4_ptr(l + 0x14), gt4_ptr(l + 0x18), 0xf9c);
            GT4_U32(l + 0xc) = GT4_U32(l + 0xc) - 1;
        }
        GT4_U32(l + 0x14 + GT4_U32(l + 0xc) * 4) = time;
        GT4_U32(l + 0xc) = GT4_U32(l + 0xc) + 1;
    }
    if (quiet) return;
    {
        uint32_t best = GT4_U32(e + 0x11e0), t = 0x157529ffu < time ? 0x157529ffu : time;
        if (best == 0x157529ffu || time < best) GT4_U32(e + 0x11e0) = t;
    }
    GT4_U32(e + 0x119c + 0x40) = time;
    better_splits(e + 0x1158, e + 0x119c);
    better_splits(race + 0x928, e + 0x119c);
}

/* func_0035E428: car i passed split `split` of lap `lap` in `time`:
   messages 0xE (the lap and split: the rest of the word is the car's
   address, as the original's register had it) and 0x10 (the time); when
   the race keeps times and not `quiet`, the entry's split (+0x119C,
   func_003C0C60) */
void gt4_race_split_event(uint32_t cond, uint32_t i, uint32_t lap, uint32_t split, uint32_t time, int quiet)
{
    uint32_t car = car_of(cond, i), b = car + 0x104, race;
    int rec;
    if (GT4_U8(b + 0x441) || GT4_U8(b + 0x632)) return;
    race = GT4_U32(cond);
    rec = keeps_times(race, cond);
    if (!GT4_U32(race + 0xdc)) {
        int32_t n = (int32_t)lap < 0 ? 0 : (int32_t)lap > 999 ? 999 : (int32_t)lap;
        gt4_race_queue_push(race + 0xf4, 0xe,
                            (car & 0xfffc0000u) | clamp4((int32_t)split) << 4 | clamp4((int32_t)i) | ((uint32_t)n & 0x3ffu) << 8, 2);
        gt4_race_post(race, 0x10, (int32_t)i, 0, message_time(time));
    }
    if (quiet || !rec) return;
    GT4_U32(entry_of(race, i) + 0x119c + split * 4) = time;
}

/* func_0034C0E0: the count of the race's last seconds (cond+0xCBF4),
   event 1 when it ends */
void gt4_race_countdown(uint32_t cond)
{
    uint16_t n = GT4_U16(cond + 0xcbf4u);
    if (!n) return;
    GT4_U16(cond + 0xcbf4u) = (uint16_t)(n - 1);
    if (n - 1 == 0) gt4_race_queue_push(GT4_U32(cond) + 0xf4, 1, 0, 0);
}

/* ---- The entries' records at the race's start (the world's race start,
   func_00387FB0, and the sequence's func_003C0108) ---- */

/* func_003BB858: a set of splits (16, then the lap at +0x40): none */
static void splits_reset(uint32_t s)
{
    for (uint32_t k = 0; k <= 0x40; k += 4) GT4_U32(s + k) = 0x157529ffu;
}

/* func_003BBA30: an entry's lap list (+0 kept or not, +4 laps, +8 dropped,
   +0xC count, +0x10 total, 1000 times from +0x14): mode 0 keeps them */
static void laps_reset(uint32_t l, uint32_t mode)
{
    GT4_U32(l) = mode;
    GT4_U32(l + 4) = 0;
    if (mode) return;
    GT4_U32(l + 8) = 0;
    GT4_U32(l + 0xc) = 0;
    GT4_U32(l + 0x10) = 0;
    for (uint32_t k = 0x14; k <= 0xfb0; k += 4) GT4_U32(l + k) = 0x157529ffu;
}

/* func_003B4E88: an entry's records: its laps, its splits, the gap shown,
   and (keeping them) its best lap */
void gt4_race_entry_reset(uint32_t e, uint32_t mode)
{
    laps_reset(e + 0x1a0, mode);
    splits_reset(e + 0x119c);
    GT4_U32(e + 0x11ec) = 0;
    GT4_U32(e + 0x11e8) = 0x157529ffu;
    if (!mode) GT4_U32(e + 0x11e0) = 0x157529ffu;             /* func_003DD778 */
}

/* func_003C0108: the race's best splits and two counts, each entry's
   best splits */
void gt4_race_records_start(uint32_t race)
{
    uint32_t list = GT4_U32(race + 0x60);
    GT4_U32(race + 0x920) = 0;                                  /* func_003DD7D0 */
    GT4_U32(race + 0x924) = 0;
    splits_reset(race + 0x928);
    for (uint32_t i = 0; (int32_t)i < (int32_t)GT4_U32(list); i++)
        splits_reset(GT4_U32(GT4_U32(list + 8) + i * 4) + 0x1158);
}

/* The world's race start for the entries (func_00387FB0): each tied to its
   car (func_003B6AE8: +0x18, +0x2894, its presentation object's +4),
   marked (func_003B4D80: +0x3488, its object's +0 at +0x2890; the car's
   names for the screen at +0x348C, +0x350C are not copied yet) and its
   records reset, kept unless the world replays (+0xCC8 2 or more) or the
   race says not (+0xF0, +0xDC). */
void gt4_race_entries_begin(uint32_t race, uint32_t cond)
{
    uint32_t list = GT4_U32(race + 0x60), mode;
    mode = !(GT4_U32(GT4_U32(race + 0x84) + 0xcc8) < 2) || GT4_U32(race + 0xf0) || GT4_U32(race + 0xdc);
    for (uint32_t i = 0; (int32_t)i < (int32_t)GT4_U32(list); i++) {
        uint32_t e = GT4_U32(GT4_U32(list + 8) + i * 4), car = cond + 0x6698u + i * CAR_SIZE;
        GT4_U32(e + 0x18) = car;
        GT4_U32(e + 0x2894) = car;
        if (GT4_U32(e + 0x2880)) GT4_U32(GT4_U32(e + 0x2880) + 4) = car;
        if (GT4_U32(e + 0x2880)) GT4_U32(GT4_U32(e + 0x2880)) = e + 0x2890;
        GT4_U32(e + 0x3488) = 1;
        gt4_race_entry_reset(e, mode);
    }
}
