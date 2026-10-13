/* One physics step for every car (func_0034DB30): each subsystem runs over
   all cars before the next one starts, in the game's order. `replay` skips
   the first car and the session-wide parts. Functions not rewritten yet are
   called through the bridge; `buf` is 0x40 bytes of guest memory the
   driver step needs for the controller's command. */
#include "car.h"

#define K(bits) gt4_f32(bits)

#define NCARS   0xf8a0u   /* conductor: number of cars */
#define ORDER   0xf8a4u   /* conductor: car index by race position */

static uint32_t car_at(uint32_t conductor, uint32_t i) { return conductor + 0x6698u + i * CAR_SIZE; }   /* func_0034C190 */
static int ncars(uint32_t conductor) { return (int)GT4_U32(conductor + NCARS); }
static uint32_t order(uint32_t conductor, int k) { return GT4_U32(conductor + ORDER + (uint32_t)k * 4); }

static uint32_t orig1(uint32_t addr, uint32_t a0) { gt4_orig_call(addr, a0, 0, 0, 0); return gt4_orig_v0; }
static uint32_t orig2(uint32_t addr, uint32_t a0, uint32_t a1) { gt4_orig_call(addr, a0, a1, 0, 0); return gt4_orig_v0; }

/* The session's virtual table (conductor + 0x10140). */
static uint32_t session_call(uint32_t c, uint32_t off, uint32_t a1, uint32_t a2)
{
    return gt4_vcall(c, 0x10140u, off, a1, a2, 0);
}

/* Car against car (session entry 0x20) for cars a and b; returns f0.
   The race sessions' (func_00365790) is rewritten. */
static float cars_contact(uint32_t c, uint32_t a, uint32_t b, float k)
{
    uint32_t e = GT4_U32(c + 0x10140u) + 0x20;
    if (GT4_U32(e + 4) == 0x00365790u)
        return gt4_race_rubber_band(c + (uint32_t)(int32_t)(int16_t)GT4_U16(e), car_at(c, a) + 0x104, car_at(c, b) + 0x104, k);
    gt4_orig_a2 = car_at(c, b) + 0x104;
    return gt4_orig_call(GT4_U32(e + 4), c + (uint32_t)(int32_t)(int16_t)GT4_U16(e), car_at(c, a) + 0x104, k, 0);
}

#define EACH(...) for (int i = first; i < ncars(c); i++) { uint32_t car = car_at(c, (uint32_t)i); __VA_ARGS__; }

void gt4_step(uint32_t c, int replay, uint32_t buf)
{
    int first, n, all = GT4_U8(c + 0xcbdc) == 2;
    uint32_t race = GT4_U32(c);

    if (!gt4_phys_headless) {
        if (gt4_race_replay(race)) GT4_ORIG(0x003f1148u, c);   /* replay */
        for (int i = 0; i < ncars(c); i++) {
            uint32_t car = car_at(c, (uint32_t)i);
            if (all || GT4_U8(car + CAR_VIBRATION_ON)) GT4_ORIG(0x0035be60u, car);   /* pad vibration */
        }
    }

    if (GT4_U8(c + 0xcbdc)) {
        /* slowed time: step only when a car's time is due */
        int due = 0;
        float scale = GT4_F32(c + 0xcbf8);
        for (int i = 0; i < ncars(c); i++) {
            uint32_t car = car_at(c, (uint32_t)i);
            float t = GT4_F32(car + CAR_VIEW_BLEND) - scale;
            if (t < -scale * 0.5f) { t = 1.0f - scale; due = 1; }
            GT4_F32(car + CAR_VIEW_BLEND) = t;
        }
        if (!due) goto end;
    }

    n = ncars(c);
    first = replay != 0;
    EACH(gt4_car_keep_state(car))
    EACH(if (GT4_U8(GT4_U32(car + CAR_PARAMS) + PARAMS_DRIVE) == 7) GT4_U8(car + 0x78d) = 0)   /* func_00355E10 */
    EACH(gt4_car_messages(car))
    {
        int hold = (int32_t)GT4_U32(GT4_U32(c) + 0xbc) > 0;   /* func_0034C210: the race holds every car */
        EACH({
            uint32_t cnt = c + 0xcbfcu + (uint32_t)i * 4, v;
            if (hold) { GT4_U8(car + CAR_5C0) = 1; continue; }
            v = GT4_U32(cnt);
            if (!v) { GT4_U8(car + CAR_5C0) = 0; continue; }
            GT4_U32(cnt) = v - 1;
            if (v - 1) { GT4_U8(car + CAR_5C0) = 1; continue; }
            gt4_race_event(race, 4, GT4_U8(car + CAR_PLAYER), 0);   /* func_0035DF80 */
        })
    }
    EACH(gt4_car_engine_update(car))
    EACH(gt4_car_driver(car, buf))
    EACH(gt4_car_idle_count(car))
    EACH(gt4_car_steer_geometry(car))
    EACH(gt4_car_slip(car))
    EACH(gt4_car_aids(car))
    EACH(gt4_car_drivetrain(car))
    EACH(gt4_car_brake_heat(car))
    EACH(if ((int8_t)GT4_U8(car + 0x710) != 2) GT4_U32(car + 0x70c) = 0)   /* func_00350308 */
    if (!gt4_phys_headless && gt4_race_replay(race)) GT4_ORIG(0x003f12b0u, c);   /* replay */
    if (!gt4_phys_headless && gt4_race_replay(race)) GT4_ORIG(0x003f1368u, c);
    EACH({
        uint8_t t = GT4_U8(car + 0x735);
        if (t) {
            GT4_U8(car + 0x735) = --t;
            if (!t) GT4_U8(car + 0x734) ^= 2;
        }
    })
    if (!replay) session_call(c, 0x68, (uint32_t)ncars(c), 0);   /* walls */
    else gt4_walls(c, 1);
    if (!replay) gt4_cars_nearby(c);
    EACH(gt4_car_active_aero(car))
    EACH(gt4_car_wheel_setup(car))
    EACH(gt4_car_ground_query(car))
    EACH(gt4_car_ground_info(car))
    EACH(gt4_car_chassis(car))
    EACH(gt4_car_tyre_wear(car))
    EACH(gt4_car_race_end(car))
    EACH(gt4_car_wheel_noise(car))
    if (!replay) {
        gt4_race_finish_handover(c);
        session_call(c, 0x70, (uint32_t)ncars(c), 0);
    }
    EACH({
        float d;
        gt4_session(c, 0x108, car, 0, 0, 0, 0, &d);
        GT4_F32(car + 0x5a8) = d;   /* distance along the course */
    })
    if (!replay) {
        for (int i = first; i < ncars(c); i++) gt4_car_laps(c, GT4_U32(c + 0xf8a4u + (uint32_t)i * 4));   /* func_003658D8 */
        session_call(c, 0x08, 0, 0);
        session_call(c, 0x78, 0, 0);
        gt4_race_gaps(c);
        session_call(c, 0x18, 0, 0);
        session_call(c, 0xc0, (uint32_t)first, (uint32_t)ncars(c));
    } else gt4_car_laps(c, (uint32_t)first);

    {
        /* car against car, by race position */
        int lead = -1, last = -1, kind;
        for (int k = 0; k < ncars(c); k++) {
            uint32_t idx = order(c, k);
            if ((int8_t)GT4_U8(car_at(c, idx) + CAR_56A) == 2) continue;
            if (lead == -1) lead = (int)idx;
            last = (int)idx;
        }
        if (last == -1) lead = 0;
        kind = (int)session_call(c, 0x140, 0, 0);
        if (kind == 1) {
            for (int i = 0; i < ncars(c); i++)
                if (i != lead) cars_contact(c, (uint32_t)lead, (uint32_t)i, 1.0f);
        } else if (kind == 2) {
            int pos = (int8_t)GT4_U8(car_at(c, (uint32_t)lead) + 0x5b3);
            for (int k = pos - 2; k >= 0; k--) cars_contact(c, order(c, k + 1), order(c, k), 1.0f);
            for (int k = pos; k < ncars(c); k++) cars_contact(c, order(c, k - 1), order(c, k), 1.0f);
        } else if (kind == 0) {
            int pos = (int8_t)GT4_U8(car_at(c, (uint32_t)lead) + 0x5b3) - 1;
            float f = 1.0f;
            for (int k = pos; k > 0; k--) f = cars_contact(c, order(c, k), order(c, k - 1), f);
            f = 1.0f;
            for (int k = pos; k < ncars(c) - 1; k++) f = cars_contact(c, order(c, k), order(c, k + 1), f);
        }
    }

    for (int i = first; i < n; i++) {                       /* func_00354A78: the outputs to the pad */
        uint32_t car = car_at(c, (uint32_t)i);
        if (gt4_phys_pad.write && gt4_phys_pad.write(gt4_phys_pad.ctx, car, gt4_ptr(GT4_U32(car + 4) + 0xca58u + GT4_U8(car + 0x544) * 16u)))
            continue;
        if (!gt4_phys_headless && !gt4_pad_idle(car, 0x30, 0x00426ea0u)) GT4_ORIG(0x00354a78u, car);
    }
    for (int i = first; i < n; i++) {                       /* func_003F3368 */
        uint32_t car = car_at(c, (uint32_t)i);
        GT4_U8(car + 0x6c4) = K(0x3f666666u) < GT4_F32(car + 0x598) && GT4_U8(car + 0x62a) == 11;
    }
    if (!gt4_phys_headless && !GT4_U32(GT4_U32(race + 0x84) + 0xcc8)) GT4_ORIG(0x0035f658u, c);   /* the car the view follows */
    if (!gt4_phys_headless)
        for (int i = first; i < n; i++) GT4_ORIG(0x003f71c8u, car_at(c, (uint32_t)i));   /* engine sound (car+0x9A0) */
    for (int i = first; i < n; i++) {
        uint32_t car = car_at(c, (uint32_t)i);
        if (GT4_U8(GT4_U32(car + CAR_PARAMS) + 0x32) == 3) {
            /* func_00355868: a cycle at car+0x788 running with the speed
               (full at 240 km/h), each turn setting a pulse at car+0x78C */
            float v = GT4_F32(car + CAR_SPEED), f, ph;
            if (v < 0) v = -v;
            f = v / K(0x42855554u);
            if (K(0x3f800000u) < f) f = K(0x3f800000u);
            ph = GT4_F32(car + 0x788) + (f * K(0x400ccccc) + K(0x3f4ccccc)) * K(0x3c888888u);
            GT4_F32(car + 0x788) = ph;
            if (K(0x3f800000u) <= ph) {
                GT4_F32(car + 0x78c) = f * K(0x3ecccccc) + K(0x3f199999u);
                GT4_F32(car + 0x788) = GT4_F32(car + 0x788) - K(0x3f800000u);
            } else {
                GT4_U32(car + 0x78c) = 0;
            }
        }
    }
end:
    gt4_race_countdown(c);                                        /* func_0034C0E0 */
    gt4_cars_relations(c + 0xcd7cu);
}
