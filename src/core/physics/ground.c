/* Speeds in the ground frame, wheel speeds, distance, gauges and free
   wheels (func_0035DE20 and what it calls). */
#include "car.h"

#define K(bits) gt4_f32(bits)

#define LAT_ROLL_TERM 0x00620784u   /* word: roll rate adds to the lateral wheel speed */

/* func_0035D6B0: velocity of each wheel along (WHEEL_VEL_X) and across
   (WHEEL_VEL_Y) the body, from the body velocity and the yaw rate. */
static void wheel_velocities(uint32_t car)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), layout = gt4_car_layout(car);
    float yaw = GT4_F32(car + CAR_YAW_RATE);
    for (int i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        uint32_t w = gt4_car_wheel(car, i);
        int pos = GT4_U8(layout + LAYOUT_POSITION + i), n = GT4_U8(layout + LAYOUT_WHEELS);
        int8_t fr = (int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i);
        uint8_t side = GT4_U8(layout + LAYOUT_SIDE + i);
        float ax = GT4_F32(params + PARAMS_AXLE_DIST + pos * 4), lat = 0, lon = 0, t;
        if (i < n && fr > 0) lat = -ax * yaw;
        else if (i < n && fr < 0) lat = ax * yaw;
        if (GT4_U32(LAT_ROLL_TERM))
            lat = lat - GT4_F32(car + CAR_ROLL_RATE) * GT4_F32(params + PARAMS_ROLL_HEIGHT + pos * 4)
                        * GT4_F32(car + CAR_MATRIX + 0x20);
        GT4_F32(w + WHEEL_VEL_Y) = GT4_F32(car + CAR_LAT_SPEED) + lat;
        t = GT4_F32(params + PARAMS_HALF_TRACK + pos * 4);
        if (side == 0) lon = -t * yaw;
        else if (side == 1) lon = t * yaw;
        GT4_F32(w + WHEEL_VEL_X) = GT4_F32(car + CAR_SPEED) + lon;
    }
}

/* func_00343518: whole metres into the odometer (capped at 1.6e9),
   returns the fraction left. */
static float odometer_add(uint32_t counter, float metres)
{
    int32_t whole = (int32_t)metres;
    uint32_t total;
    if (metres <= 0) return 0;
    total = GT4_U32(counter) + (uint32_t)whole;
    GT4_U32(counter) = total;
    if (0x5f5e0fffu < total) {
        GT4_U32(counter) = 0x5f5e1000u;
        return 0;
    }
    return metres - (float)whole;
}

/* func_0035DD00: distance travelled (forward only). */
static void distance(uint32_t car)
{
    const float wrap = K(0x4c989680u);   /* 1e7 m */
    float v = GT4_F32(car + CAR_ABS_SPEED), d, total;
    if (v < 0) return;
    d = v * GT4_F32(car + CAR_DT);
    GT4_F32(car + CAR_ODO_FRACTION) = odometer_add(car + CAR_ENGINE_WEAR, GT4_F32(car + CAR_ODO_FRACTION) + d);
    total = GT4_F32(car + CAR_DISTANCE) + d;
    GT4_F32(car + CAR_DISTANCE) = total;
    if (wrap <= total) GT4_F32(car + CAR_DISTANCE) = total - wrap;
    if ((int8_t)GT4_U8(car + CAR_56A) == 1) {
        total = GT4_F32(car + CAR_7B0) + d;
        GT4_F32(car + CAR_7B0) = total;
        if (wrap <= total) GT4_F32(car + CAR_7B0) = total - wrap;
    }
}

/* func_0035DC08: speedometer (moves at most 400 km/h per second) and a
   filtered lateral acceleration. */
static void gauges(uint32_t car)
{
    const float step = K(0x3c888888u);
    float shown = GT4_F32(car + CAR_SPEEDO), v = GT4_F32(car + CAR_ABS_SPEED);
    float rate = GT4_F32(car + CAR_DT) * K(0x42de38e2u), s;
    if (shown < v) {
        s = shown + rate;
        GT4_F32(car + CAR_SPEEDO) = s;
        if (v < s) GT4_F32(car + CAR_SPEEDO) = v;
    } else {
        s = shown - rate;
        GT4_F32(car + CAR_SPEEDO) = s;
        if (s < v) GT4_F32(car + CAR_SPEEDO) = v;
    }
    rate = GT4_F32(car + CAR_LAT_G_RATE);
    rate = rate + ((GT4_F32(car + CAR_LAT_ACCEL) - GT4_F32(car + CAR_LAT_G)) * K(0x43480000u)
                   - rate * K(0x41a00000u)) * step;
    GT4_F32(car + CAR_LAT_G_RATE) = rate;
    GT4_F32(car + CAR_LAT_G) = GT4_F32(car + CAR_LAT_G) + rate * step;
}

/* func_0035D990: free wheels roll with the ground; the drive line speed of
   each differential follows its wheels when one of them rolls freely. */
static void free_wheels(uint32_t car)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), drive = params + PARAMS_DRIVE, layout = gt4_car_layout(car);
    int free_diff[4] = { 0, 0, 0, 0 };
    if (GT4_U8(drive + DRIVE_KIND) == 3 && GT4_F32(car + CAR_HANDBRAKE) != 0) {
        float v = GT4_F32(car + CAR_SPEED);
        if (K(0xbe8e38e3u) < v && v < K(0x3e8e38e3u)) return;
    }
    for (int i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        uint32_t w = gt4_car_wheel(car, i);
        float a;
        if (GT4_U8(w + WHEEL_FLAGS)) continue;
        a = GT4_F32(w + WHEEL_STEER);
        GT4_F32(w + WHEEL_SPIN) = gt4_sinf(a) * GT4_F32(w + WHEEL_VEL_X) - gt4_cosf(a) * GT4_F32(w + WHEEL_VEL_Y);
        free_diff[GT4_U8(layout + LAYOUT_POSITION + i)] = 1;
    }
    for (int k = 0; k < GT4_U8(layout + LAYOUT_DIFFS); k++) {
        int idx = k < GT4_U8(layout + LAYOUT_DIFFS) ? GT4_U8(layout + LAYOUT_DIFF_INDEX + k) : 0;
        uint8_t a, b;
        float r;
        if (!free_diff[k] && GT4_U8(drive + DRIVE_DIFF_TYPE + idx)) continue;
        a = GT4_U8(layout + LAYOUT_DIFF_WHEEL_A + k);
        b = GT4_U8(layout + LAYOUT_DIFF_WHEEL_B + a);
        if (b == 0x7f) b = a;
        r = GT4_F32(drive + DRIVE_RADIUS2 + idx * 4);
        if (GT4_U8(drive + 1)) r = GT4_F32(drive + DRIVE_RADIUS2 + GT4_U8(drive + 3) * 4);
        GT4_F32(car + CAR_FRONT_SPEED + 4 * k) = r
            * ((GT4_F32(gt4_car_wheel(car, a) + WHEEL_SPIN) + GT4_F32(gt4_car_wheel(car, b) + WHEEL_SPIN))
               * K(0x3f000000u));
    }
}

/* func_00350678: the engine wear limits (D_00620320), once, from bytes
   of the settings at D_00620CD8 (+0xD0.. in 100000 m, +0xD1/D4/D6 in
   thousandths). */
#define WEAR_LIMITS 0x00620320u
static void wear_limits(void)
{
    const uint32_t s = 0x00620cd8u, t = WEAR_LIMITS;
    if (GT4_U8(s + 0xcc)) return;
    GT4_F32(t + 4) = (float)(int32_t)GT4_U8(s + 0xd1) * K(0x3a83126eu);
    GT4_F32(t + 0) = -(float)(int32_t)(GT4_U8(s + 0xd0) * 100000);
    GT4_F32(t + 8) = (float)(int32_t)(GT4_U8(s + 0xd2) * 100000);
    GT4_F32(t + 0xc) = (float)(int32_t)(GT4_U8(s + 0xd3) * 100000) + GT4_F32(t + 8);
    GT4_F32(t + 0x10) = (float)(int32_t)GT4_U8(s + 0xd4) * K(0x3a83126eu);
    GT4_F32(t + 0x18) = (float)(int32_t)GT4_U8(s + 0xd6) * K(0x3a83126eu);
    GT4_F32(t + 0x14) = -(float)(int32_t)(GT4_U8(s + 0xd5) * 100000) * K(0x3f000000u);
    GT4_F32(t + 0x1c) = (float)(int32_t)(GT4_U8(s + 0xd7) * 100000);
    GT4_F32(t + 0x20) = (float)(int32_t)(GT4_U8(s + 0xd8) * 100000);
    GT4_U8(s + 0xcc) = 1;
}

/* func_003435E8, func_00343708, func_003439E8: the engine's wear
   counters (car+0x6E0, +0x6E4, +0x6EC) run on with the distance, each up
   to its limit; the second starts negative and, once through zero,
   restarts from three times its own negative mark. */
static void engine_wear(uint32_t car, float d)
{
    uint32_t w = car + CAR_ENGINE_WEAR;
    if (GT4_U8(w + 0x14)) return;
    wear_limits();
    GT4_F32(w + 4) = GT4_F32(w + 4) + d;
    if (GT4_F32(WEAR_LIMITS + 0xc) < GT4_F32(w + 4)) GT4_F32(w + 4) = GT4_F32(WEAR_LIMITS + 0xc);
    {
        float old = GT4_F32(w + 8), v = old + d;
        GT4_F32(w + 8) = v;
        if (old < 0 && 0 <= v) GT4_F32(w + 8) = -(GT4_F32(WEAR_LIMITS + 0x14) * K(0x40400000u));
    }
    if (GT4_F32(WEAR_LIMITS + 0x1c) < GT4_F32(w + 8)) GT4_F32(w + 8) = GT4_F32(WEAR_LIMITS + 0x1c);
    GT4_F32(w + 0x10) = GT4_F32(w + 0x10) + d;
    if (GT4_F32(0x00620b18u) < GT4_F32(w + 0x10)) GT4_F32(w + 0x10) = GT4_F32(0x00620b18u);
}

/* func_0035DE20. Layout kind 1 (two wheels) runs the original. */
void gt4_car_ground_speeds(uint32_t car)
{
    float a = 0, b = 0;
    if (GT4_U8(gt4_car_layout(car) + LAYOUT_KIND) == 1) {
        GT4_ORIG(0x0035de20u, car);
        return;
    }
    /* func_0035D920 */
    for (int k = 0; k < 3; k++) {
        a = a + GT4_F32(car + CAR_140 + 4 * k) * GT4_F32(car + CAR_VELOCITY + 4 * k);
        b = b + GT4_F32(car + CAR_140 + 0xc + 4 * k) * GT4_F32(car + CAR_VELOCITY + 4 * k);
    }
    GT4_F32(car + CAR_LAT_SPEED) = b;
    GT4_F32(car + CAR_SPEED) = a;
    wheel_velocities(car);

    GT4_F32(car + CAR_ABS_SPEED) = GT4_F32(car + CAR_SPEED);
    distance(car);
    if (GT4_F32(car + CAR_ABS_SPEED) < 0) GT4_F32(car + CAR_ABS_SPEED) = -GT4_F32(car + CAR_ABS_SPEED);
    if (!GT4_U8(car + CAR_ENGINE_WEAR + 0x14))
        engine_wear(car, GT4_F32(car + CAR_ABS_SPEED) * GT4_F32(car + CAR_DT));   /* func_00350858 */
    if (!GT4_U8(car + CAR_5B6) && GT4_F32(car + CAR_TOP_SPEED) < GT4_F32(car + CAR_ABS_SPEED))
        GT4_F32(car + CAR_TOP_SPEED) = GT4_F32(car + CAR_ABS_SPEED);
    gauges(car);
    free_wheels(car);
}

/* func_003F4E40: surface of a ground block; marked blocks (bit 2) remap
   some surfaces to 0x14..0x16. */
static uint8_t ground_surface(uint32_t g)
{
    uint8_t s = GT4_U8(g + GROUND_SURFACE);
    if (!(GT4_U8(g + GROUND_KIND) & 2)) return s;
    switch (s) {
    case 0: case 7: return 0x14;
    case 1: case 10: case 11: case 12: return 0x15;
    case 9: return 0x16;
    default: return s;
    }
}

/* func_003F4EA0: 1 when a ground block is off the road. */
static int off_road(uint32_t g) { return GT4_U32(0x00622430u + GT4_U8(g + GROUND_SURFACE) * 4u) != 0; }

#define SURFACE_BUMPS 0x00620938u   /* +0xac [surface] height, +0x10c [surface] wave number, +0x184 [surface] s8 mode */

/* func_00369640: the surface's bumps at (x, z): cos(k x) + sin(k z),
   signed square in mode 1. */
static float bumps(int mode, float x, float z, float k)
{
    float v = gt4_cosf(k * x) + gt4_sinf(k * z);
    if (mode == 1) {
        float s = v * v;
        return v < 0 ? -s : s;
    }
    return v;
}

/* func_00369888: what each wheel stands on (surface, height, bumps), how
   many wheels are off the road, and the calls that tell the course. */
void gt4_car_ground_info(uint32_t car)
{
    uint32_t layout = gt4_car_layout(car), session = GT4_U32(car + CAR_SESSION);
    int n = GT4_U8(layout + LAYOUT_WHEELS), off = 0, marked = 0, all_off;
    for (int i = 0; i < n; i++) {
        uint32_t g = car + CAR_GROUND + (uint32_t)i * GROUND_SIZE;
        if (GT4_U8(g + GROUND_MISS)) continue;
        marked += GT4_U8(g + GROUND_KIND) & 1;
        if (off_road(g)) off++;
    }
    all_off = n == off;
    for (int i = 0; i < n; i++) {
        uint32_t g = car + CAR_GROUND + (uint32_t)i * GROUND_SIZE, w = gt4_car_wheel(car, i);
        float h, b;
        uint8_t s;
        if (GT4_U8(g + GROUND_MISS)) { GT4_U32(w + 0x90) = 0; continue; }
        GT4_F32(w + 0x8c) = GT4_F32(g + GROUND_HIT_Y);
        s = ground_surface(g);
        GT4_U8(w + WHEEL_SURFACE) = s;
        GT4_U8(w + 0x8a) = GT4_U8(g + 0x32);
        GT4_U8(w + 0x8b) = GT4_U8(g + GROUND_KIND);
        h = GT4_F32(SURFACE_BUMPS + 0xac + s * 4u);
        if (h == 0) { GT4_U32(w + 0x90) = 0; continue; }
        b = bumps((int8_t)GT4_U8(SURFACE_BUMPS + 0x184 + s), GT4_F32(g + GROUND_X), GT4_F32(g + GROUND_Z),
                  GT4_F32(SURFACE_BUMPS + 0x10c + s * 4u));
        if (off_road(g) && !all_off) b = b * 0.5f;
        if ((int8_t)GT4_U8(0x00620944u + 0x160 + s)) b = b * GT4_F32(w + WHEEL_40);
        GT4_F32(w + 0x90) = b * h;
    }
    GT4_U32(car + 0x5ac) = GT4_U32(car + CAR_GROUND + GROUND_ID);
    GT4_U8(car + 0x730) = (uint8_t)off;
    GT4_U8(car + 0x731) = (uint8_t)marked;
    if (!GT4_U8(car + CAR_56B)) {
        uint8_t f = GT4_U8(car + 0x734);
        if (!(f & 1)) {
            if (off == n) {
                GT4_U8(car + 0x734) = f | 1;
                if (GT4_U8(car + 0x733) < 100) GT4_U8(car + 0x733) = (uint8_t)(GT4_U8(car + 0x733) + 1);
            }
        } else if (!off) GT4_U8(car + 0x734) = f ^ 1;
    }
    GT4_U8(car + 0x5b5) &= 0xef;
    if (marked && gt4_vcall(session, 0x10140u, 0x180, session, 0, 0)) GT4_U8(car + 0x5b5) |= 0x10;
    gt4_vcall(session, 0x10140u, 0x188, car, (uint32_t)marked, (uint32_t)off);
}

uint8_t gt4_ground_surface(uint32_t g) { return ground_surface(g); }
int gt4_ground_off_road(uint32_t g) { return off_road(g); }
