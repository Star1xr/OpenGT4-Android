#include "car.h"
#include "engine.h"

#include <math.h>
#include <string.h>

#define K(bits) gt4_f32(bits)

/* func_0035ACB0 */
static void countdown(uint32_t car)
{
    uint8_t *n = gt4_ptr(car + CAR_COUNTDOWN);
    if (*n != 0 && *n != 0xff) (*n)--;
}

/* func_003658A0: step length from the time scale (1 = real time at 60 Hz). */
static void time_step(uint32_t car)
{
    float scale = GT4_F32(car + CAR_TIME_SCALE);
    GT4_F32(car + CAR_RATE) = K(0x42700000u) / scale;   /* 60 */
    GT4_F32(car + CAR_DT) = scale * K(0x3c888888u);     /* 1/60 */
}

/* func_00354AC0 */
static void masses(uint32_t car)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS);
    float extra = GT4_F32(car + CAR_EXTRA_MASS) * K(0x3f3d70a3u);   /* 0.74 */
    GT4_F32(car + CAR_MASS_A) = GT4_F32(params + PARAMS_MASS) + extra;
    GT4_F32(car + CAR_MASS_B) = GT4_F32(params + PARAMS_MASS_B) + extra;
}

/* func_0035C6D8: the key of tyre compound c for player p's axle (0 front,
   1 rear) from the race's table (cond+0xF928 range, keys at +0xF940),
   -1 without one. */
static uint64_t pit_tyre_key(uint32_t cond, int p, int axle, int c)
{
    uint32_t e = cond + 0xf928u + (uint32_t)p * 4 + (uint32_t)axle * 2;
    uint64_t key;
    if (p < 0 || !(p < (int32_t)GT4_U32(cond + 0xf8a0u))) return ~(uint64_t)0;
    if (c < GT4_U8(e) || GT4_U8(e + 1) < c) return ~(uint64_t)0;
    memcpy(&key, gt4_ptr(cond + 0xf940u + ((uint32_t)p * 10 + (uint32_t)axle * 5 + (uint32_t)(c - GT4_U8(e))) * 8), 8);
    return key;
}

/* func_004468F8 for the keys of the pit table (func_0035C4D8 fills it
   with tyres only, category 25 front, 26 rear): into the garage entry. */
static void garage_fit(uint32_t gar, uint64_t key)
{
    uint32_t cat = (uint32_t)(key >> 32);
    if (cat == 0x19) memcpy(gt4_ptr(gar + 0x60), &key, 8);
    else if (cat == 0x1a) memcpy(gt4_ptr(gar + 0x68), &key, 8);
}

/* func_0035C0B8: the pit stop (CAR_5BE counting down). At 645 the tyres
   asked for (car+0x752/0x753, 0xFE the same, 0xFF none) go into a copy of
   the car's garage entry, its record is rebuilt and the tyre blocks of the
   axles changed (car+0x5BC bits 0x40 front, 0x80 rear) made from it; at
   644 their load scaling; at 60 the wheels' wear and heat start afresh; at
   59 bit 0x200. Held at 721 until the request's countdown (car+0x750)
   runs out; stopped at 90 while refuelling (0x100). */
static void pit_service(uint32_t car)
{
    uint32_t b = car + 0x104, cond = GT4_U32(car + 4), w = GT4_U32(b + 0x4b8);
    uint32_t flags, t, which, player, choose;
    if ((w & 0xffff0010u) == 0x02d10010u) {
        int16_t n = (int16_t)GT4_U16(b + 0x64c);
        if (0 < n) {
            GT4_U16(b + 0x64c) = (uint16_t)(n - 1);
            if (!(uint16_t)(n - 1)) {                             /* func_0035BF08 */
                uint32_t e = cond + 0xf910u + GT4_U8(b + 0x440) * 4u;
                GT4_U16(b + 0x64c) = 0;
                if (GT4_U8(e) != 1) {
                    GT4_U8(e + 3) = 0xfe;
                    GT4_U8(e + 1) = 0xfe;
                    GT4_U8(e + 2) = 0xfe;
                    GT4_U8(e) = 1;
                }
            }
        }
        return;
    }
    if ((w & 0xffff0100u) == 0x005a0100u) return;
    flags = GT4_U16(b + 0x4b8);
    t = (uint16_t)(GT4_U16(b + 0x4ba) - 1);
    GT4_U16(b + 0x4ba) = (uint16_t)t;
    which = (flags & 0x40) ? 1 : 0;
    if (flags & 0x80) which |= 2;
    if (which) {
        choose = GT4_U8(b + 0x64e) < 0xfe || GT4_U8(b + 0x64f) < 0xfe;
        player = GT4_U8(b + 0x440);
        if (t == 0x285) {
            uint32_t pc = cond + 0x10000u + player * 2, pk = cond + 0x10000u + player * 8;
            if (GT4_U8(b + 0x64e) == 0xfe) GT4_U8(pk - 0x25e0u) = GT4_U8(pc - 0x4e0u);
            if (GT4_U8(b + 0x64f) == 0xfe) GT4_U8(pk - 0x25dfu) = GT4_U8(pc - 0x4dfu);
            if (choose) {
                if (!gt4_phys_specdb) {
                    GT4_U16(b + 0x4ba) = (uint16_t)(t + 1);
                    GT4_ORIG(0x0035c0b8u, car);
                    return;
                }
                {
                    uint32_t rec = gt4_phys_scratch, gar = gt4_phys_scratch + 0x600;
                    uint32_t entry = GT4_U32(GT4_U32(GT4_U32(GT4_U32(car) + 0x60) + 8) + player * 4) + 0x20;
                    memcpy(gt4_ptr(gar), gt4_ptr(entry), 0x178);           /* func_004441B8 */
                    for (int axle = 0; axle < 2; axle++) {
                        uint32_t c = GT4_U8(b + 0x64e + axle);
                        uint64_t key;
                        if (c == 0xff) continue;
                        key = pit_tyre_key(cond, (int)player, axle, (int)c);
                        if (key == ~(uint64_t)0) continue;
                        garage_fit(gar, key);
                        GT4_U8(pk - 0x25e0u + axle) = (uint8_t)c;
                        GT4_U8(pc - 0x4e0u + axle) = (uint8_t)c;
                    }
                    {
                        uint32_t m = GT4_U8(cond + 0xcbddu);           /* func_0034C968 */
                        gt4_car_record(rec, gar, m == 0xff ? 0 : m, gt4_phys_specdb);
                    }
                    gt4_tyre_blocks(GT4_U32(car + CAR_PARAMS), rec, which);   /* func_00444210: nothing to free */
                }
            }
        }
        if (t == 0x284 && choose) {
            gt4_tyre_static_load(GT4_U32(car + CAR_PARAMS), which);
            GT4_U8(b + 0x64f) = 0xff;
            GT4_U8(b + 0x64e) = 0xff;
        }
        if (t == 0x3c) {
            uint32_t lay = gt4_car_layout(car);
            int n = GT4_U8(lay + LAYOUT_WHEELS);
            for (int i = 0; i < n; i++) {
                int8_t fr = (int8_t)GT4_U8(lay + LAYOUT_HANDBRAKE + i);
                uint32_t f = GT4_U16(b + 0x4b8);
                int fit = 0 < fr ? (f & 0x40) != 0 : fr < 0 ? (f & 0x80) != 0 : (f & 0xc0) != 0;
                if (fit) gt4_wheel_fresh(car, i);
            }
            GT4_U16(b + 0x4b8) = (uint16_t)(GT4_U16(b + 0x4b8) & 0xff3f);
        }
    }
    if (t == 0x3b) GT4_U16(b + 0x4b8) = (uint16_t)(GT4_U16(b + 0x4b8) | 0x200);
}

void gt4_car_engine_update(uint32_t car)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS);
    const gt4_engine *engine = gt4_ptr(params + PARAMS_ENGINE);
    float speed;

    if (GT4_U16(car + CAR_5BE)) pit_service(car);
    countdown(car);
    time_step(car);
    masses(car);
    gt4_car_aero(car);

    speed = GT4_F32(car + CAR_ENGINE_SPEED);
    GT4_F32(car + CAR_ENGINE_TORQUE) = gt4_engine_torque(engine, speed, gt4_ptr(car + CAR_ENGINE_WEAR));
    GT4_F32(car + CAR_ENGINE_BRAKE) = gt4_engine_braking(engine, GT4_F32(car + CAR_ENGINE_SPEED));

    {
        float weight = GT4_F32(car + CAR_148) * GT4_F32(params + PARAMS_MASS) * K(0x411ccccc);   /* 9.8 */
        GT4_F32(car + CAR_6C0) = -((GT4_F32(car + CAR_ROLLING) + GT4_F32(car + CAR_DRAG)) + -weight);
    }
    gt4_car_tyre_grip(car);
    gt4_car_wheel_directions(car);
}

float gt4_wheel_direction(float *weight, float x, float y)
{
    float speed = sqrtf(y * y + x * x), w;

    if (K(0x4031c71bu) <= speed) w = K(0x3f800000u);                /* 10 km/h */
    else if (speed <= K(0x3e8e38e3u)) w = 0;                        /* 1 km/h */
    else w = (speed - K(0x3e8e38e3u)) * K(0x3ecccccfu);             /* 0.4 */
    *weight = w;
    if (w == 0) return 0;
    return gt4_atan2f(-y, x);
}

void gt4_car_wheel_directions(uint32_t car)
{
    int n = gt4_car_wheels(car);
    for (int i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i);
        GT4_F32(w + WHEEL_VEL_ANGLE) = gt4_wheel_direction(gt4_ptr(w + WHEEL_VEL_WEIGHT),
            GT4_F32(w + WHEEL_VEL_X), GT4_F32(w + WHEEL_VEL_Y));
    }
}
