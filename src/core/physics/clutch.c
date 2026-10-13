/* Engine output, clutch and drive torque to the wheels (func_0036D818 and
   what it calls): every drive kind including the hybrid, turbo or not,
   manual gearboxes, the torque-converter automatics (1, 2) and the
   power split (3). */
#include "car.h"

#define K(bits) gt4_f32(bits)
#define ONE K(0x3f800000u)

static uint32_t drive_of(uint32_t car) { return GT4_U32(car + CAR_PARAMS) + PARAMS_DRIVE; }
static uint32_t engine_of(uint32_t car) { return GT4_U32(car + CAR_PARAMS) + PARAMS_ENGINE; }

/* func_0034EA68: optional cap of the engine power (engine flag 0xdb). */
float gt4_engine_power_cap(uint32_t engine, float torque, float speed)
{
    float power, knee, soft, r;
    if (!GT4_U8(engine + ENGINE_CAP)) return torque;
    power = torque * speed;
    knee = GT4_F32(engine + ENGINE_CAP_POWER);
    soft = GT4_F32(engine + ENGINE_CAP_SOFT);
    if (power <= knee * (ONE - soft)) return torque;
    r = knee * (ONE - soft * K(0x3f000000u));
    if (!(knee <= power)) {
        float d = knee - power;
        r = r - (d * d) / ((soft + soft) * knee);
    }
    r = r / speed;
    return r < torque ? r : torque;
}

/* func_00355410: rev limiter, then the torque delivered for the throttle
   (engine braking when closed). */
static float engine_output(uint32_t car, float throttle)
{
    uint32_t engine = engine_of(car);
    int rpm = (int16_t)GT4_U16(car + CAR_ENGINE_RPM);
    int redline = GT4_U16(engine + ENGINE_REDLINE);
    float brake, t = 0;

    if (!GT4_U8(car + CAR_LIMITER)) {
        if (!(rpm < redline)) GT4_U8(car + CAR_LIMITER) = 1;
    } else if (rpm < redline - 500) {
        GT4_U8(car + CAR_LIMITER) = 0;
    }
    brake = GT4_F32(car + CAR_ENGINE_BRAKE);
    if (!GT4_U8(car + CAR_LIMITER)) t = throttle * (GT4_F32(car + CAR_ENGINE_TORQUE) + brake);
    return gt4_engine_power_cap(engine, t - brake, GT4_F32(car + CAR_ENGINE_SPEED));
}

/* func_00354B78: fuel used for the delivered torque. */
static void fuel(uint32_t car, float torque)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS);
    float power, use;
    int burst = 0;

    if (GT4_U8(drive_of(car) + DRIVE_GEARBOX) == 3) return;   /* func_003683A8 */
    GT4_F32(car + CAR_FUEL_BURST) = 0;
    if (GT4_U8(car + CAR_FUEL_BURST_N)) {
        GT4_U8(car + CAR_FUEL_BURST_N) = GT4_U8(car + CAR_FUEL_BURST_N) - 1;
        burst = 1;
    }
    if (torque <= 0) return;
    power = torque * (GT4_F32(car + CAR_ENGINE_SPEED) * K(0x42700000u) / K(0x40c90fdau))
            / K(0x45db5612u);   /* torque * rpm / 7018.76 */
    if (burst) {
        float k = GT4_F32(params + PARAMS_FUEL_BURST);
        float f = power * k / (k + ONE), left;
        GT4_F32(car + CAR_FUEL_BURST) = f;
        left = GT4_F32(car + CAR_FUEL_BURST_LEFT) - f * GT4_F32(car + CAR_DT);
        GT4_F32(car + CAR_FUEL_BURST_LEFT) = left;
        if (left <= 0) {
            GT4_F32(car + CAR_FUEL_BURST_LEFT) = 0;
            GT4_U8(car + CAR_FUEL_BURST_N) = 0;
        }
    }
    use = GT4_F32(params + PARAMS_FUEL_RATE) / K(0x4a229900u) * power * GT4_F32(car + CAR_DT);
    GT4_F32(car + CAR_FUEL_USED) = GT4_F32(car + CAR_FUEL_USED) + use;
    if (0 < GT4_F32(car + CAR_FUEL_TANK) && !GT4_U16(car + CAR_5BE)) {
        float left = GT4_F32(car + CAR_EXTRA_MASS) - use;
        GT4_F32(car + CAR_EXTRA_MASS) = left;
        if (left < 0) GT4_F32(car + CAR_EXTRA_MASS) = 0;
    }
}

/* Turbo state, from car+0x104: the two stages' spooled rpm (+0x4F4,
   +0x4F6, the larger at +0x4F8), a blow-off after lifting (+0x504, armed
   by +0x607) and a smoothed boost (+0x508). */
#define SPOOL(car, j) ((car) + 0x5f8 + (uint32_t)(j) * 2)
#define SPOOL_MAX     0x5fc
#define BLOW_OFF      0x608
#define BOOST_SMOOTH  0x60c
#define BLOW_ARMED    0x70b

/* func_003554A0: engine torque for the throttle. A turbo engine (0xfc)
   first spools each stage toward the rpm the last boost would give,
   then scales the throttle by the boost it makes. */
float gt4_car_engine_drive(uint32_t car, float throttle)
{
    uint32_t engine = engine_of(car);
    float t, dt = GT4_F32(car + CAR_DT);

    if (GT4_F32(engine + ENGINE_TURBO) == 0) {
        GT4_F32(car + CAR_THROTTLE_IN) = throttle;
        GT4_U8(car + CAR_70A) = 0;
        GT4_F32(car + CAR_THROTTLE_OUT) = throttle;
    } else {
        int rpm = (int16_t)GT4_U16(car + CAR_ENGINE_RPM);
        float full = gt4_engine_boost(engine, rpm, rpm), last = GT4_F32(car + CAR_THROTTLE_IN);
        float target, boosted, lag;
        GT4_F32(car + CAR_THROTTLE_OUT) = last;
        target = (last * (float)rpm) / full;
        for (int j = 0; j < 2; j++) {
            uint16_t s;
            if (GT4_F32(engine + 0xe4 + j * 4) == 0) continue;
            s = (uint16_t)(GT4_U16(SPOOL(car, j)) + (int32_t)(((target - (float)(int16_t)GT4_U16(SPOOL(car, j)))
                                                                * GT4_F32(engine + 0xec + j * 4)) * dt));
            GT4_U16(SPOOL(car, j)) = (int16_t)s < 0 ? 0 : s;
        }
        boosted = throttle * gt4_engine_boost(engine, (int16_t)GT4_U16(SPOOL(car, 0)), (int16_t)GT4_U16(SPOOL(car, 1)));
        lag = K(0x3f000000u) - ((float)rpm * K(0x39800000u)) * K(0x3e800000u);   /* 0.5 - rpm/16384 */
        if (lag < K(0x3d800000u)) lag = K(0x3d800000u);
        GT4_F32(car + BLOW_OFF) = 0;
        GT4_F32(car + BOOST_SMOOTH) = (GT4_F32(car + BOOST_SMOOTH) + (boosted + (ONE - throttle) * lag)) * K(0x3f000000u);
        if (GT4_U8(car + BLOW_ARMED) == 0) {
            if (throttle < K(0x3f400000u)) {                   /* lifted below 0.75 */
                float v = last - K(0x3f4ccccc);
                if (v < GT4_F32(car + BLOW_OFF)) v = GT4_F32(car + BLOW_OFF);
                GT4_U8(car + BLOW_ARMED) = 1;
                GT4_F32(car + BLOW_OFF) = v * K(0x3b800000u);
            }
        } else if (GT4_U8(car + BLOW_ARMED) == 1) {
            if (K(0x3f666666u) < throttle) GT4_U8(car + BLOW_ARMED) = 0;
        }
        GT4_F32(car + CAR_THROTTLE_IN) = boosted;
        throttle = full <= boosted ? ONE : boosted / full;
        GT4_U16(car + SPOOL_MAX) = GT4_U16(SPOOL(car, 0));
        if ((int32_t)GT4_U16(SPOOL(car, 0)) < (int16_t)GT4_U16(SPOOL(car, 1)))
            GT4_U16(car + SPOOL_MAX) = GT4_U16(SPOOL(car, 1));
    }
    t = engine_output(car, throttle);
    if (0 < GT4_F32(engine + ENGINE_TURBO)) {
        /* the turbo's vibration, from the boost left over 0.75 */
        float over = GT4_F32(car + CAR_THROTTLE_OUT) - K(0x3f400000u);
        uint8_t v = 1;
        if (!(over <= 0) && !(t <= 0)) {
            float f = over;
            int n;
            if (!(K(0x4412ffffu) <= t)) f = (over * t) * K(0x3adee95du);
            n = (int32_t)((f * f) * K(0x43000000u));
            if (!(n < 0x80)) n = 0x7f;
            if (!(0 < n)) n = 1;
            v = (uint8_t)n;
        }
        GT4_U8(car + CAR_70A) = v;
    }
    fuel(car, t);
    return t;
}

/* func_00472AE0: the fuel economy shown, in the units of the settings at
   D_006244D8 (+0x10: 1 and 2 miles per gallon, US and imperial, 3 litres
   per 100 km, else km per litre), 0..999. */
static uint32_t economy_value(float metres, float fuel)
{
    uint32_t unit = GT4_U32(0x006244d8u + 0x10);
    float d = metres, v;
    if (unit == 1) fuel = fuel * K(0x3e874251u);                   /* func_00472A78 */
    else if (unit == 2) fuel = fuel * K(0x3e61413bu);
    if (unit == 1 || unit == 2) d = metres * K(0x3f1f136au);
    else if (unit == 3) {
        if (d < K(0x3a83126eu)) return 999;
        v = (fuel * K(0x49742400u)) / d;
        return K(0x4479c000u) <= v ? 999 : (uint32_t)(int32_t)v;
    }
    if (d < K(0x3c23d70au)) return 0;
    if (fuel < K(0x358637bdu)) return 999;
    v = (d * K(0x3c23d70au)) / fuel;
    return K(0x4479c000u) < v ? 999 : (uint32_t)(int32_t)v;
}

/* func_00354CE8: fuel economy for the display, every 30 steps. */
static void economy(uint32_t car)
{
    uint8_t n = GT4_U8(car + CAR_ECONOMY_TIMER) - 1;
    GT4_U8(car + CAR_ECONOMY_TIMER) = n;
    if (n) return;
    GT4_U8(car + CAR_ECONOMY_TIMER) = 30;
    GT4_U16(car + CAR_ECONOMY) = (uint16_t)economy_value(GT4_F32(car + CAR_DISTANCE) - GT4_F32(car + CAR_ECONOMY_DIST),
                                                         GT4_F32(car + CAR_FUEL_USED) - GT4_F32(car + CAR_ECONOMY_FUEL));
    GT4_F32(car + CAR_ECONOMY_DIST) = GT4_F32(car + CAR_DISTANCE);
    GT4_F32(car + CAR_ECONOMY_FUEL) = GT4_F32(car + CAR_FUEL_USED);
}

/* Flags on the driven (8) and other axle's (4) wheels, per drive kind. */
static void drive_flags(uint32_t car, uint8_t kind)
{
    uint32_t layout = gt4_car_layout(car);
    for (int i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        uint32_t w = gt4_car_wheel(car, i);
        int8_t side = (int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i);
        int front = side > 0, rear = side < 0;
        uint8_t f = GT4_U8(w + WHEEL_FLAGS);
        switch (kind) {
        case 0: case 4: case 5: case 6: if (rear) f |= 8; break;
        case 1: case 9: if (front) f |= 8; break;
        case 2: if (front) f |= 8; if (rear) f |= 4; break;
        case 3: case 7: case 8: case 10: if (front) f |= 4; if (rear) f |= 8; break;
        default: break;
        }
        GT4_U8(w + WHEEL_FLAGS) = f;
    }
}

/* func_0036D7A8: any wheel braking. */
static int braking(uint32_t car)
{
    for (int i = 0; i < GT4_U8(gt4_car_layout(car) + LAYOUT_WHEELS); i++)
        if (GT4_F32(gt4_car_wheel(car, i) + WHEEL_BRAKE) != 0) return 1;
    return 0;
}

#define BIG      K(0x47bf67ffu)   /* 99999.99: the centre coupling locked */
#define LOCKED(b) (GT4_U8((b) + 0x523))

/* Driven speed per axle (+0x52C front, +0x530 rear) and the torque each
   is given (+0x534, +0x538), from car+0x104. */
#define AXLE_SPEED(b, i)  GT4_F32((b) + 0x52c + (uint32_t)(i) * 4)
#define AXLE_TORQUE(b, i) GT4_F32((b) + 0x534 + (uint32_t)(i) * 4)

/* func_0036D548: the limited-slip centre's coupling for the clutch
   torque and the axles' torques; eased off with the grip used (the
   globals at 0x62030C/0x620314), BIG when it locks. */
static float lsd_centre(uint32_t car, float clutch, float axle0, float axle1)
{
    uint32_t b = car + 0x104, params = GT4_U32(car + CAR_PARAMS);
    float grip = GT4_F32(car + CAR_GRIP_USE), low = GT4_F32(0x0062030cu), ease = ONE, c, lock, sum;
    uint8_t state;
    if (grip < 0) grip = -grip;
    if (!(grip <= low)) {
        if (ONE <= grip) ease = GT4_F32(0x00620314u);
        else ease = ONE - ((ONE - GT4_F32(0x00620314u)) * (grip - low)) / (ONE - low);
    }
    c = AXLE_SPEED(b, 1) - AXLE_SPEED(b, 0);
    if (c < 0) c = -c;
    c = c * GT4_F32(params + PARAMS_DRIVE + 4);
    if (GT4_U8(car + CAR_GEAR) == 0) clutch = -clutch;
    if (GT4_U8(0x0062030au) && clutch < 0) return -BIG;
    lock = GT4_F32(params + PARAMS_DRIVE + 8);
    if (lock != 0) {
        if ((GT4_U32(b + 0x520) & 0xff00ffu) == 0x10000u) return BIG;
        state = GT4_U8(car + CAR_CLUTCH_STATE);
        if (!state) return BIG;
        if (ease == ONE) {
            if (state == 2) return BIG;
            if (K(0x3f400000u) < GT4_F32(car + CAR_BRAKE_PEDAL)) return BIG;
            sum = (clutch + axle0) + axle1;
            if (0 < clutch && 0 < sum) return BIG;
            if (clutch < 0 && sum < 0) return BIG;
        }
    }
    c = c * ease;
    if (!(ONE <= c)) return c;
    return lock == 0 ? ONE : BIG;
}


/* func_0036D738: how hard a slipping clutch grips for the speed
   difference across it: 1 up to 52 rad/s, then rising to 1.125. */
static float clutch_slip(float d)
{
    if (d < 0) d = -d;
    d = d - K(0x42517082u);
    if (d < 0) d = 0;
    d = d / K(0x44517082u) + ONE;
    if (K(0x3f900000u) <= d) d = K(0x3f900000u);
    return d;
}

/* func_00356E58: the other engine model's (drive+0x13 = 4) extra torque,
   49 N*m falling off as 95428/rpm above 100 rpm. */
static float extra_torque(int rpm, float throttle)
{
    float t = K(0x42440000u);
    if (rpm >= 101) {
        t = K(0x47ba6233u) / (float)rpm;
        if (K(0x42440000u) < t) t = K(0x42440000u);
    }
    return t * throttle;
}

/* The hybrid (drive kind 7): a battery (car+0x788, its meter at
   car+0x78D) and a motor sharing the load with the engine. The limits
   are globals at 0x8438C4.. and 0x62046B... */
#define BATTERY      0x788
#define ENGINE_POWER(car) (((GT4_F32((car) + CAR_ENGINE_TORQUE) + GT4_F32((car) + CAR_ENGINE_BRAKE))                             * (float)(int16_t)GT4_U16((car) + CAR_ENGINE_RPM)) * K(0x38dbc307u))

/* func_00355E40: the charge meter, -100..100. */
static void charge_meter(uint32_t car, float e)
{
    int v = (int8_t)GT4_U8(car + BATTERY + 5) + (int32_t)(e * GT4_F32(0x00843920u));
    if (v < -100) v = -100;
    if (!(v < 101)) v = 100;
    GT4_U8(car + BATTERY + 5) = (uint8_t)v;
}

/* func_00355E88: store energy, up to the capacity; returns what went in. */
static float battery_store(uint32_t car, float e)
{
    float sum = GT4_F32(car + BATTERY) + e, full = GT4_F32(0x008438dcu);
    charge_meter(car, e);
    if (full < sum) e = e - (sum - full);
    else full = sum;
    GT4_F32(car + BATTERY) = full;
    return e;
}

/* func_00355F08: take energy, as much as is left; returns what came out. */
static float battery_draw(uint32_t car, float e)
{
    float left = GT4_F32(car + BATTERY);
    charge_meter(car, -e);
    if (left < e) {
        e = e - (e - left);
        left = 0;
    } else {
        left = left - e;
    }
    GT4_F32(car + BATTERY) = left;
    return e;
}

/* func_00356168: the charge between the low and high marks, 0..1. */
static float charge_level(uint32_t car)
{
    float c = GT4_F32(car + BATTERY), hi = GT4_F32(0x008438fcu), lo;
    if (hi <= c) return ONE;
    lo = GT4_F32(0x008438f8u);
    if (c <= lo) return 0;
    return (c - lo) / (hi - lo);
}

/* func_003560F0: the throttle the engine gets, the pedal over the
   engine's share of the power. */
static float hybrid_share(uint32_t car)
{
    float p = ENGINE_POWER(car), c = GT4_F32(0x008438c4u), t;
    p = p / ((p + c) + c);
    t = GT4_F32(car + CAR_THROTTLE) / p;
    if (ONE < t) t = ONE;
    return t;
}

/* func_003561D0: the motor's load, -1 (charging) .. 1. */
static float motor_load(uint32_t car)
{
    float p = ENGINE_POWER(car), c = GT4_F32(0x008438c4u), want, l;
    uint8_t gear;
    p = p / ((p + c) + c);
    want = GT4_U8(car + CAR_CLUTCH_STATE) == 2 ? GT4_F32(car + CAR_THROTTLE_REQ) : GT4_F32(car + CAR_THROTTLE);
    if (want <= p) return K(0xbf800000u);
    l = ((want - p) + (want - p)) / (ONE - p);
    gear = GT4_U8(car + CAR_GEAR);
    l = l * charge_level(car);
    if (!((int8_t)GT4_U8(0x0062046du) < gear)) l = 0;
    else if (ONE < l && (gear < (int8_t)GT4_U8(0x0062046bu) || (int8_t)GT4_U8(0x0062046cu) < gear)) l = ONE;
    return l + K(0xbf800000u);
}

/* func_00356BB8: the motor's torque; braking on the rear wheels and a
   negative load charge the battery. */
static float hybrid_motor(uint32_t car)
{
    float load = motor_load(car), dt = GT4_F32(car + CAR_DT), regen = 0, rpm, t, e;
    const float power = K(0x46151b5cu);
    for (int i = 2; i < 4; i++) {
        uint32_t w = gt4_car_wheel(car, i);
        float f;
        if (GT4_F32(w + WHEEL_BRAKE_IN) == 0) continue;
        f = GT4_F32(w + 0x80) * GT4_F32(w + 0x20);
        if (f < 0) f = -f;
        regen = regen + f * K(0x3a832923u);
    }
    if (GT4_F32(0x008438e4u) < regen) regen = GT4_F32(0x008438e4u);
    battery_store(car, regen * dt);
    rpm = (float)(int16_t)GT4_U16(car + CAR_ENGINE_RPM);
    if (0 < load) {
        t = (GT4_F32(0x008438c4u) * power) / rpm;
        if (GT4_F32(0x008438ccu) < t) t = GT4_F32(0x008438ccu);
        e = (((t * rpm) / power) * load) * dt;
        t = t * load;
        return t * (battery_draw(car, e) / e);
    }
    t = (GT4_F32(0x008438c4u) * power) / rpm;
    if (GT4_F32(0x008438d4u) < t) t = GT4_F32(0x008438d4u);
    e = -(((t * rpm) / power) * load) * dt;
    battery_store(car, e);
    return t * load;
}

/* func_00355F80: the hybrid with the clutch open: below a full battery
   the engine is held at a throttle that charges it, the charging load
   taken from what it delivers. */
float gt4_hybrid_idle(uint32_t car)
{
    float rpm = (float)(int16_t)GT4_U16(car + CAR_ENGINE_RPM), throttle = GT4_F32(car + CAR_THROTTLE);
    float cap, load, floor, torque = GT4_F32(car + CAR_ENGINE_TORQUE), out;
    if (ONE <= GT4_F32(car + BATTERY) / GT4_F32(0x008438dcu)) return gt4_car_engine_drive(car, throttle);
    cap = (GT4_F32(0x008438f4u) * K(0x46151b5cu)) / rpm;
    load = torque * GT4_F32(0x008438f0u);
    if (cap < load) load = cap;
    floor = (GT4_F32(0x008438e8u) - rpm) * GT4_F32(0x008438ecu)
            + (load + GT4_F32(car + CAR_ENGINE_BRAKE)) / (torque + GT4_F32(car + CAR_ENGINE_BRAKE));
    if (floor < 0) floor = 0;
    if (throttle < floor) throttle = floor;
    if (ONE < throttle) throttle = ONE;
    GT4_F32(car + CAR_THROTTLE) = throttle;
    out = gt4_car_engine_drive(car, throttle) - load;
    battery_store(car, ((load * rpm) * K(0x38dbc307u)) * GT4_F32(car + CAR_DT));
    return out;
}

/* Gearbox 3: an engine and an electric motor sharing the drive through
   a power split, with a generator and a battery (func_00368628). State
   from car+0x788: +0 the speed the generator holds the engine toward,
   +4 the charge 0..1, +8 flags for the display, +9 the charge mode (0
   normal, 1 charging, 2 low). The engine model (drive+0x13) picks the
   motor curve and the limits. */
#define SPLIT         0x788
#define RAD_TO_RPM(w) (((w) * K(0x42700000u)) / K(0x40c90fdau))
#define KW            K(0x38dbc307u)   /* N*m at rpm to kW */

/* func_00367EB8 and func_00367DD0: the motor's torque for its rpm. */
static float motor_curve_a(float rpm)
{
    if (rpm < K(0x44960000u)) return K(0x43c7eb83u);
    if (rpm < K(0x44bb8000u)) return K(0x48e90b72u) / rpm;
    if (rpm < K(0x45b86000u))
        return (((K(0x42880000u) - (rpm - K(0x44bb8000u)) * K(0x3aca90dfu)) * K(0x44330cccu)) * K(0x411ccccc)) / rpm;
    if (rpm < K(0x45cb2000u)) return ((K(0x45cb2000u) - rpm) / K(0x44160000u)) * K(0x42919c07u);
    return 0;
}

static float motor_curve_b(float rpm)
{
    if (rpm < K(0x43c80000u)) return K(0x43aeee12u);
    if (rpm < K(0x44820000u)) return K(0x43aeee12u) - (rpm - K(0x43c80000u)) * K(0x3d93b49bu);
    if (rpm < K(0x45af0000u)) return K(0x489a3884u) / rpm;
    if (rpm < K(0x45bb8000u)) return ((K(0x45bb8000u) - rpm) / K(0x43c80000u)) * K(0x42619a5au);
    return 0;
}

/* func_00367FD8 */
static float motor_torque(uint32_t car, float w)
{
    float rpm = RAD_TO_RPM(w);
    if (rpm < 0) rpm = -rpm;
    return GT4_U8(drive_of(car) + 0x13) == 3 ? motor_curve_a(rpm) : motor_curve_b(rpm);
}

/* func_00368040, func_00368088, func_003680C0, func_003680F8: the
   engine's top speed, the generator's range and the battery's size. */
static float split_engine_top(uint32_t car)
{
    uint8_t m = GT4_U8(drive_of(car) + 0x13);
    return m == 3 ? K(0x4402e651u) : m == 2 ? K(0x43eb9e92u) : K(0x43d17082u);
}
static float split_gen_max(uint32_t car) { return GT4_U8(drive_of(car) + 0x13) == 3 ? K(0x4482e651u) : K(0x442a2b69u); }
static float split_gen_min(uint32_t car) { return GT4_U8(drive_of(car) + 0x13) == 3 ? K(0xc482e651u) : K(0xc42a2b69u); }
static float split_battery(uint32_t car) { return GT4_U8(drive_of(car) + 0x13) == 3 ? K(0x43fa0000u) : K(0x43890000u); }

/* func_00368128: fuel for the engine's power at its speed. */
static float split_fuel(uint32_t car, float w, float power)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS);
    if (GT4_F32(car + CAR_ENGINE_SPEED) < (float)(int32_t)GT4_U16(params + 0x202) * K(0x3dd6774eu)) return 0;
    if (power <= 0) return 0;
    return power * gt4_curve_eval(gt4_ptr(GT4_U8(params + 0x247) == 1 ? 0x00620ba8u : 0x00620bf8u), RAD_TO_RPM(w));
}

/* func_003681E0: the power asked for the speed and pedal. */
static float split_demand(float speed, float pedal, float lo)
{
    if (lo <= pedal)
        return (gt4_curve_eval(gt4_ptr(0x00620c80u), speed) * (pedal - lo)) / (ONE - lo);
    return (gt4_curve_eval(gt4_ptr(0x00620c90u), speed) * (pedal - lo)) / (0 - lo);
}

/* func_003685A0: the pedal, with creep below 15 km/h. */
static float split_pedal(uint32_t car, float pedal, float v)
{
    float r;
    if (GT4_U8(car + CAR_GEAR) == 0) v = -v;
    if (v <= K(0x40855554u)) return pedal;
    r = (K(0x40855554u) - v) / K(0x40855554u);
    if (0 < r) r = 0;
    if (r < K(0xbf800000u)) r = K(0xbf800000u);
    r = r * K(0x3f4ccccc);
    return r + (ONE - r) * pedal;
}

static float split_drive(uint32_t car)
{
    uint32_t drive = drive_of(car), s = car + SPLIT;
    uint8_t gear = GT4_U8(car + CAR_GEAR), mode;
    float dt = GT4_F32(car + CAR_DT);
    float idle = (float)(int32_t)GT4_U16(GT4_U32(car + CAR_PARAMS) + 0x202) * K(0x3dd6774eu);
    int coast = GT4_F32(car + CAR_CLUTCH_REQ) == 0;      /* sp+0xC */
    int on = 0, slow = 0, launch = 0, electric = 0, low, charging, hold = 0, pushing = 0, regen = 0, busy = 0;
    float w, speed, demand = 0, want = 0, pedal, in, motor, epower, top, target, gen = 0, cut = K(0xc2d17082u);
    float eng, aim, out, power, share = 0, give = 0, lim;

    w = GT4_F32(car + 0x630 + (uint32_t)GT4_U8(drive + 3) * 4) * GT4_F32(drive + DRIVE_RATIOS + (uint32_t)gear * 4);
    if (!gear) w = -w;
    if (coast && K(0x3f4ccccc) < GT4_F32(car + CAR_BRAKE_PEDAL) && 0 < GT4_F32(car + CAR_THROTTLE_REQ)) launch = 1;

    /* the charge mode, from the charge in eighths with hysteresis */
    {
        float c = GT4_F32(s + 4) * K(0x41000000u) + K(0x3f000000u);
        mode = GT4_U8(s + 9);
        if (mode == 1) {
            if (K(0x40d99999u) <= c) GT4_U8(s + 9) = 0;
            else if (c < K(0x3fc00000u)) GT4_U8(s + 9) = 2;
        } else if (mode == 0) {
            if (c < K(0x40c66666u)) GT4_U8(s + 9) = 1;
        } else if (mode == 2) {
            if (K(0x40200000u) <= c) GT4_U8(s + 9) = 1;
        }
        mode = GT4_U8(s + 9);
    }
    charging = mode != 0;
    low = mode == 2;
    speed = GT4_F32(car + CAR_SPEED);
    if (GT4_U8(drive + 0x13) == 3 && K(0x3ce38e37u) <= speed) electric = 1;
    if (!gear) electric = 0;

    /* electric (model 3 moving): the power asked, less what the engine
       gives at low speed */
    if (electric) {
        demand = split_demand(speed, GT4_F32(car + CAR_THROTTLE), K(0x3dccccccu));
        if (!(demand <= want)) {
            float kw = (GT4_F32(car + CAR_ENGINE_TORQUE) * RAD_TO_RPM(GT4_F32(car + CAR_ENGINE_SPEED))) * KW;
            if (kw < demand) {
                float spare = 0;
                if (!(speed < spare)) spare = speed < K(0x40b1c71bu) ? speed * K(0x40900001u) : K(0x41c80000u);
                want = demand - spare;
                if (want < 0) want = 0;
            } else {
                want = demand;
            }
        }
    }
    pedal = split_pedal(car, GT4_F32(car + CAR_THROTTLE), speed);

    /* the motor: the pedal, or braking with the brake pedal */
    if (0 < GT4_F32(car + CAR_BRAKE_PEDAL) && !coast) {
        float a = w;
        in = -GT4_F32(car + CAR_BRAKE_PEDAL) * K(0x40800000u);
        if (w < 0) a = -w;
        a = a * K(0x3cc391d3u);
        if (ONE < a) a = ONE;
        in = in * a;
        if (in < K(0xbf800000u)) in = K(0xbf800000u);
    } else {
        in = pedal;
        if (w < 0) {
            float f = (ONE - w / K(0xc3517082u)) * K(0x40800000u);
            if (ONE < f) f = ONE;
            else if (f < 0) f = 0;
            in = pedal * f;
        }
    }
    if (in < 0 && w < K(0x3dd6774eu) && K(0xbdd6774eu) < w) motor = 0;
    else motor = motor_torque(car, w) * in;
    if (launch) motor = 0;
    if (GT4_U8(car + 0x5c0)) motor = 0;
    if (electric) motor = 0;
    epower = (motor * RAD_TO_RPM(w)) * KW;
    if (w < 0) epower = -epower;
    if (!launch && 0 < motor) pushing = 1;                 /* sp+0x10 */
    if (motor < 0) regen = !coast;                         /* fp */

    /* the engine speed wanted */
    top = split_engine_top(car);
    target = top;
    if (launch) {
        hold = 1;
    } else {
        target = 0;
        if (!(speed < 0)) {
            if (speed < K(0x419b8e38u) && !electric && epower < K(0x41200000u)) {
                /* slow and light: the engine stays off */
            } else if (electric) {
                target = gt4_curve_eval(gt4_ptr(0x00620ca0u), want);
                if (top < target) target = top;
            } else {
                target = top * (gear == 1 ? GT4_F32(car + CAR_THROTTLE) : 0);
            }
        }
    }
    if (K(0x419b8e38u) <= speed && target < K(0x42d17082u)) {
        target = K(0x42d17082u);
        if (!charging) cut = K(0x42d388acu);
        else { hold = 1; gen = target; }
    }
    if (target < idle && !low) {
        target = 0;
        if (GT4_F32(car + CAR_THROTTLE_REQ) == 0) cut = K(0x42d388acu);
    }
    if (GT4_F32(car + CAR_ENGINE_SPEED) < cut) slow = 1;   /* sp+8 */
    if (charging && !launch) {
        float before = target;
        gen = low ? K(0x43517082u) : K(0x42d17082u);
        if (target < idle) hold = 1;
        target = target + gen;
        if (top < target) {
            target = top;
            if (!low) gen = target - before;
        }
        slow = 0;
    }

    /* the generator holds the engine within its range of the motor */
    eng = GT4_F32(s + 0);
    aim = target + (target - w) * K(0x40200000u);
    {
        float hi = split_gen_max(car), lo = split_gen_min(car);
        int clamped = 0;
        if (hi < aim) { aim = hi; clamped = 1; }
        else if (aim < lo) { aim = lo; clamped = 1; }
        if (clamped) {
            target = w + (aim - w) / K(0x40600000u);
            if (target < idle) target = 0;
        }
    }
    {
        float cur = GT4_F32(car + CAR_ENGINE_SPEED), rate = 0;
        if (cur < idle && idle <= target) rate = K(0x45a39fe5u);
        else if (cur <= idle && target == 0) rate = K(0x45a39fe5u);
        if (eng < aim) {
            if (rate == 0) rate = K(0x4482e651u);
            eng = eng + rate * dt;
            if (aim < eng) eng = aim;
        } else if (aim < eng) {
            if (rate == 0) rate = K(0x4402e651u);
            eng = eng - rate * dt;
            if (eng < aim) eng = aim;
        }
    }
    GT4_F32(s + 0) = eng;
    GT4_F32(car + CAR_ENGINE_SPEED) = w + (eng - w) / K(0x40600000u);
    if (idle <= GT4_F32(car + CAR_ENGINE_SPEED)) on = 1;  /* sp+4 */

    /* the engine's torque and power */
    if (on && !slow) {
        float t = ONE;
        if (!electric) {
            t = target / K(0x43d17082u) + 0;
            if (ONE < t) t = ONE;
        }
        out = gt4_car_engine_drive(car, t);
        if (!electric && pedal * out < 0) out = 0;
        power = (out * RAD_TO_RPM(GT4_F32(car + CAR_ENGINE_SPEED))) * KW;
    } else {
        out = 0;
        power = 0;
    }
    if (electric) epower = demand - power;
    GT4_F32(car + CAR_FUEL_USED) = GT4_F32(car + CAR_FUEL_USED)
        + split_fuel(car, GT4_F32(car + CAR_ENGINE_SPEED), power) * dt;

    /* what of the engine's output charges the battery (share, kW) and
       what drives (give, torque) */
    if (0 < power) {
        if (hold) {
            if (!electric) {
                give = out;
                out = 0;
            } else {
                share = power;
                out = 0;
                power = give;
            }
        } else if (electric) {
            share = (power * gen) / GT4_F32(car + CAR_ENGINE_SPEED);
            if (power < share) share = power;
            else if (share < 0) share = 0;
            power = power - share;
        } else {
            give = (out * gen) / GT4_F32(car + CAR_ENGINE_SPEED);
            if (out < give) give = out;
            else if (give < 0) give = 0;
            out = out - give;
            give = give + out / K(0x40600000u);
        }
        if (electric) epower = epower + share;
        else if (0 < give) share = (give * RAD_TO_RPM(GT4_F32(car + CAR_ENGINE_SPEED))) * KW;
    }
    if (on && !hold) {
        if (electric) {
            float rpm = RAD_TO_RPM(w);
            out = rpm < K(0x42c80000u) ? 0 : power / (rpm * KW);
        } else {
            out = (out * K(0x40200000u)) / K(0x40600000u);
        }
    }
    if (electric) {
        if (0 < share) busy = 1;
        if (0 < demand && 0 < power) busy = 1;
    } else if (0 < give) {
        busy = 1;
    }
    if (low && share < epower) {
        float f = share / epower;
        if (f < 0) f = -f;
        motor = motor * f;
        epower = share;
    }
    if (electric) {
        /* the motor gives the power left, within its torque */
        float t = motor_torque(car, w), rpm = RAD_TO_RPM(w), k = rpm * KW, most = t * k;
        if (most < 0) most = -most;
        if (most < epower) {
            epower = most;
            motor = t;
        } else if (epower < -most) {
            epower = -most;
            motor = -t;
        } else if (K(0xbdd6774eu) < rpm && rpm < K(0x3dd6774eu)) {
            motor = 0 <= epower ? t : -t;
        } else {
            motor = epower / k;
        }
    }

    /* the battery */
    share = share - epower;
    share = (share * K(0x447a0000u)) / split_battery(car);
    share = (dt * share) / K(0x46b6d000u);
    lim = GT4_F32(s + 4) + share;
    GT4_F32(s + 4) = lim;
    if (ONE < lim) GT4_F32(s + 4) = ONE;
    else if (lim < 0) GT4_F32(s + 4) = 0;

    /* the display */
    if (electric) {
        if (!launch && 0 < epower) pushing = 1;
        if (epower < 0 && !coast) regen = 1;
    }
    if (slow) on = 0;
    GT4_U8(s + 8) = 0;
    if (launch) GT4_U8(s + 8) = 1;
    else if (on && !hold && 0 < out && 0 < demand) {
        pushing = 1;
        GT4_U8(s + 8) = 1;
    }
    if (launch || pushing) GT4_U8(s + 8) = (uint8_t)(GT4_U8(s + 8) | 4);
    else if (regen && demand < 0) GT4_U8(s + 8) = (uint8_t)(GT4_U8(s + 8) | 2);
    if (busy || GT4_U8(s + 9)) GT4_U8(s + 8) = (uint8_t)(GT4_U8(s + 8) | 8);
    if (0 < share) GT4_U8(s + 8) = (uint8_t)(GT4_U8(s + 8) | 0x10);
    else if (share < 0) GT4_U8(s + 8) = (uint8_t)(GT4_U8(s + 8) | 0x20);
    return out + motor;
}

/* func_00356300: the lateral acceleration the steering asks for (speed
   squared over the turning radius), no more than the inner front tyre's
   side grip allows. */
static float yaw_target(uint32_t car)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), b = car + 0x104, tyre;
    float steer = GT4_F32(b + 0x468), v = GT4_F32(b + 0x53c), a, g, s;
    int side;
    a = v * ((v * gt4_tanf(steer)) / GT4_F32(params + 0x18));
    if (a < 0) a = -a;
    side = 0 < GT4_F32(b + 0x468) ? 1 : 0;
    tyre = gt4_tyre_params(params, side, GT4_U8(gt4_car_wheel(car, side) + 0x89));
    s = GT4_F32(b + 0x468) * K(0x3fc00000u);
    if (s < 0) s = -s;
    g = gt4_curve_eval(gt4_ptr(tyre + 0x6c), s) * GT4_F32(tyre);
    g = g + g;
    if (g < 0) g = -g;
    if (g < a) a = g;
    if (GT4_F32(b + 0x468) < 0) a = -a;
    return a;
}

/* Splits `t` between the two sides by the yaw error e (> 0 more on the
   first): each side t(1 +- e/2), the excess over `lim` passed across. */
static void yaw_split(float t, float e, float lim, float *more, float *less)
{
    float hi, lo;
    e = e * K(0x3f000000u);
    hi = t * (e + ONE);
    lo = t * (ONE - e);
    *more = hi;
    *less = lo;
    if (lim < hi) {
        *more = lim;
        *less = lo - (hi - lim);
    }
}

/* func_00356418: the hybrid's front motor (drive kind 7): its share of
   the power, limited by the battery's charge; the front wheels regenerate
   through their brake force; the drive split left and right against the
   yaw error (lateral acceleration asked less measured, car+0x658), each
   side's torque capped and paid from the battery, regenerating the same
   way when the load is negative. */
void gt4_hybrid_front(uint32_t car)
{
    uint32_t b = car + 0x104, drive = drive_of(car);
    float dt = GT4_F32(car + CAR_DT), load, p, share, lim, cap, err, t[2], w[2];
    int still[2];

    GT4_U32(b + 0x51c) = 0;
    p = engine_output(car, GT4_U8(b + 0x522) == 3 ? GT4_F32(b + 0x488) : GT4_F32(b + 0x494));
    p = (p * (float)(int16_t)GT4_U16(b + 0x4fa)) * KW;
    load = motor_load(car);
    p = ((p + GT4_F32(0x008438c4u) * load) * GT4_F32(0x00843904u)) / (ONE - GT4_F32(0x00843904u));
    share = p / (GT4_F32(0x008438c0u) + GT4_F32(0x008438c0u));
    if (ONE < share) share = ONE;
    else if (share < K(0xbf800000u)) share = K(0xbf800000u);
    {
        float c = charge_level(car);
        c = c + GT4_F32(0x00843900u) * (ONE - c);
        if (c < share) share = c;
    }
    for (int i = 0; i < 2; i++) {
        uint32_t wh = gt4_car_wheel(car, i);
        float spin = GT4_F32(wh + WHEEL_SPIN), f;
        if (spin < 0) spin = -spin;
        still[i] = spin < K(0x3e8e38e3u);
        f = GT4_F32(wh + 0x80);
        if (f < 0) f = -f;
        f = (f * spin) * K(0x3a832923u);
        if (GT4_F32(0x008438e0u) < f) f = GT4_F32(0x008438e0u);
        battery_store(car, f * dt);
    }
    cap = ((GT4_F32(drive + DRIVE_RADIUS) * K(0x40c90fdau)) * K(0x411ccccc)) * K(0x44330cccu) / K(0x4230851eu);

    if (0 < share) {
        lim = GT4_F32(0x008438d8u) * GT4_F32(0x008438c8u);
        t[0] = t[1] = share;
        err = yaw_target(car) * GT4_F32(0x0084390cu) - GT4_F32(car + 0x658) * GT4_F32(0x00843908u);
        if (0 < err) {
            err = err - GT4_F32(0x00843910u);
            if (!(err < 0)) {
                yaw_split(share, err, ONE, &t[1], &t[0]);
                if (t[0] < K(0xbf800000u)) t[0] = K(0xbf800000u);
            }
        } else if (err < 0) {
            err = err + GT4_F32(0x00843910u);
            if (!(0 < err)) {
                yaw_split(share, -err, ONE, &t[0], &t[1]);
                if (t[1] < K(0xbf800000u)) t[1] = K(0xbf800000u);
            }
        }
        for (int i = 0; i < 2; i++) {
            float spin = GT4_F32(gt4_car_wheel(car, i) + WHEEL_SPIN);
            w[i] = 0;
            if (!still[i]) {
                float q = t[i] * ((GT4_F32(0x008438c0u) * cap) / spin);
                t[i] = spin < 0 ? -q : q;
                if (lim < t[i]) t[i] = lim;
                else if (t[i] < -lim) t[i] = -lim;
                q = (t[i] * spin) / cap;
                w[i] = spin < 0 ? -q : q;
            } else if (GT4_F32(b + 0x48c) == w[i]) {
                t[i] = lim;
                w[i] = 0;
            }
        }
        {
            float e = (w[0] + w[1]) * dt, sum = 0;
            if (0 < e) {
                float got = battery_draw(car, e) / e;
                t[1] = t[1] * got;
                w[0] = w[0] * got;
                t[0] = t[0] * got;
                w[1] = w[1] * got;
            }
            if (0 < w[0]) sum = w[0] + sum;
            if (0 < w[1]) sum = sum + w[1];
            GT4_F32(b + 0x51c) = sum / GT4_F32(0x008438c0u);
        }
        if (!GT4_U8(b + 0x520)) {
            t[0] = -t[0];
            t[1] = -t[1];
        }
        GT4_F32(gt4_car_wheel(car, 0) + WHEEL_TORQUE) = GT4_F32(gt4_car_wheel(car, 0) + WHEEL_TORQUE) + t[0];
        GT4_F32(gt4_car_wheel(car, 1) + WHEEL_TORQUE) = GT4_F32(gt4_car_wheel(car, 1) + WHEEL_TORQUE) + t[1];
        return;
    }

    lim = GT4_F32(0x008438d8u) * GT4_F32(0x008438d0u);
    t[0] = t[1] = share;
    err = yaw_target(car) * GT4_F32(0x00843918u) - GT4_F32(car + 0x658) * GT4_F32(0x00843914u);
    if (0 < err) {
        err = err - GT4_F32(0x0084391cu);
        if (!(err < 0)) {
            float e = err * K(0x3f000000u), hi = t[0] * (e + ONE), lo = t[1] * (ONE - e);
            t[0] = hi;
            t[1] = lo;
            if (hi < K(0xbf800000u)) {
                t[0] = K(0xbf800000u);
                t[1] = lo - (hi + ONE);
            }
            if (0 < t[1]) t[1] = 0;
        }
    } else if (err < 0) {
        err = err + GT4_F32(0x0084391cu);
        if (!(0 < err)) {
            float e = err * K(0x3f000000u), lo = t[1] * (ONE - e), hi = t[0] * (e + ONE);
            t[1] = lo;
            t[0] = hi;
            if (lo < K(0xbf800000u)) {
                t[1] = K(0xbf800000u);
                t[0] = hi - (lo + ONE);
            }
            if (0 < t[0]) t[0] = 0;
        }
    }
    for (int i = 0; i < 2; i++) {
        uint32_t wh = gt4_car_wheel(car, i);
        float q = t[i];
        if (!still[i]) {
            float spin = GT4_F32(wh + WHEEL_SPIN);
            q = q * ((GT4_F32(0x008438c0u) * cap) / spin);
            if (spin < 0) q = -q;
            if (q < -lim) q = -lim;
            else if (lim < q) q = lim;
        }
        if (!GT4_U8(b + 0x520)) q = -q;
        GT4_F32(wh + WHEEL_TORQUE) = GT4_F32(wh + WHEEL_TORQUE) + q;
    }
}

/* func_003F2070: the torque converter's rates, once, from bytes in
   hundredths. */
static void converter_rates(void)
{
    if (GT4_U8(0x0062210cu)) return;
    GT4_U8(0x0062210cu) = 1;
    GT4_F32(0x006220f8u) = (float)(int32_t)GT4_U8(0x006220f4u) * K(0x3c23d70au);
    GT4_F32(0x006220fcu) = (float)(int32_t)GT4_U8(0x006220f5u) * K(0x3c23d70au);
    GT4_F32(0x00622104u) = (float)(int32_t)GT4_U8(0x00622100u) * K(0x3c23d70au);
    GT4_F32(0x00622108u) = (float)(int32_t)GT4_U8(0x00622101u) * K(0x3c23d70au);
}

/* func_003F2110: the automatic gearboxes (1, 2): the engine torque
   multiplied by a torque converter ratio (car+0x61C) that moves toward
   the engine/wheel speed ratio, bounded by the top gear's ratio. */
static float torque_converter(uint32_t car)
{
    uint32_t drive = drive_of(car);
    float dt = GT4_F32(car + CAR_DT), throttle = GT4_F32(car + CAR_THROTTLE), req = GT4_F32(car + CAR_THROTTLE_REQ);
    float r = ONE, want = ONE, rate;
    int flare = 0;

    converter_rates();
    if (GT4_U8(car + CAR_CLUTCH_STATE) == 1 && GT4_U8(car + CAR_GEAR) != 0) {
        float top = GT4_F32(drive + DRIVE_RATIOS + GT4_U8(drive + 2) * 4);
        r = GT4_F32(car + CAR_ENGINE_SPEED) / GT4_F32(car + CAR_WHEEL_ENGINE_SPEED);
        if (ONE < r) r = ONE;
        else if (r < top) r = top;
        if (GT4_U8(drive + DRIVE_GEARBOX) == 1) {
            /* stall speed (drive+0x30 rpm) scaled by the throttle asked */
            float stall = ((float)(int32_t)GT4_U16(drive + 0x30) * K(0x3dd6774eu)) * (req * K(0x3ecccccc) + K(0x3f199999));
            float wheel = GT4_F32(car + CAR_WHEEL_ENGINE_SPEED);
            if (stall < wheel) {
                want = stall / wheel;
                if (want < top) want = top;
                else if (ONE < want) want = ONE;
            }
            rate = GT4_F32(0x006220fcu) + req * (GT4_F32(0x006220f8u) - GT4_F32(0x006220fcu));
        } else {
            uint8_t gear = GT4_U8(car + CAR_GEAR);
            if (gear >= 2) want = GT4_F32(drive + DRIVE_RATIOS + gear * 4);
            rate = want < r ? GT4_F32(0x00622104u) : GT4_F32(0x00622108u);
            if ((int8_t)GT4_U8(car + CAR_56A) == 1 || GT4_U8(car + CAR_56B) || !GT4_U8(car + CAR_546)) flare = 1;
        }
        if (want == r) {
            GT4_F32(car + CAR_61C) = r;
            return r * gt4_car_engine_drive(car, throttle);
        }
        rate = (rate * dt) * GT4_F32(car + CAR_61C);
        if (r < want) {
            r = r + rate;
            if (want <= r) r = want;
            else if (flare) GT4_U8(car + CAR_62A) = 1;
        } else {
            r = r - rate;
            if (r <= want) r = want;
            else if (flare) GT4_U8(car + CAR_62A) = 1;
        }
    }
    GT4_F32(car + CAR_61C) = r;
    return r * gt4_car_engine_drive(car, throttle);
}

void gt4_car_clutch(uint32_t car)
{
    uint32_t drive = drive_of(car), layout = gt4_car_layout(car), b = car + 0x104;
    uint8_t kind = GT4_U8(drive + DRIVE_KIND), gearbox = GT4_U8(drive + DRIVE_GEARBOX);
    float dt = GT4_F32(car + CAR_DT), clutch = 0, engine, axle[4], inertia[2], ratio_inertia = 0, couple = 0, split = 0;
    int gear, n, k, centre = 0;

    if (kind > 10) {
        GT4_ORIG(0x0036d818u, car);
        return;
    }
    if (GT4_U8(car + CAR_CLUTCH_STATE)) drive_flags(car, kind);

    if (gearbox == 3) {
        clutch = split_drive(car);
        if (!GT4_U8(car + CAR_CLUTCH_STATE)) clutch = 0;
    } else if (GT4_U8(car + CAR_CLUTCH_STATE)) {
        uint8_t state;
        float throttle = GT4_F32(car + CAR_THROTTLE);
        if (gearbox == 1 || gearbox == 2) {
            engine = torque_converter(car);
        } else if (kind == 7) {
            /* the hybrid: the engine for its share, plus the motor */
            engine = gt4_car_engine_drive(car, hybrid_share(car));
            engine = engine + hybrid_motor(car);
        } else if (GT4_U8(drive + 0x13) == 4) {
            engine = gt4_car_engine_drive(car, throttle);
            engine = engine + extra_torque((int16_t)GT4_U16(car + CAR_ENGINE_RPM), throttle);
        } else {
            engine = gt4_car_engine_drive(car, throttle);
        }
        clutch = engine;
        state = GT4_U8(car + CAR_CLUTCH_STATE);
        if (state != 1) {
            float in, wheel;
            if (state == 3) {
                clutch = GT4_F32(drive + DRIVE_CLUTCH_B0);
            } else {
                clutch = GT4_F32(car + CAR_ENGINE_TORQUE) * K(0x3d4ccccc);   /* 0.05 */
                if (0 < engine) {
                    float slip = clutch_slip(GT4_F32(car + CAR_ENGINE_SPEED_IN) - GT4_F32(car + CAR_WHEEL_ENGINE_SPEED));
                    clutch = clutch + slip * engine;
                }
            }
            in = GT4_F32(car + CAR_ENGINE_SPEED_IN);
            wheel = GT4_F32(car + CAR_WHEEL_ENGINE_SPEED);
            if (in < wheel) clutch = -clutch;
            GT4_F32(car + CAR_ENGINE_SPEED) = GT4_F32(car + CAR_ENGINE_SPEED)
                + (engine - clutch) * GT4_F32(drive + DRIVE_ENGINE_INERTIA) * dt;
        }
    }
    economy(car);
    if (kind != 7) {
        GT4_F32(car + CAR_CLUTCH_TORQUE) = clutch;
        if (!(0 <= clutch)) GT4_F32(car + CAR_CLUTCH_TORQUE) = -clutch;
    }

    /* the ratio in gear; the second automatic box (2) keeps the first
       gear's ratio in every forward gear, its converter (car+0x61C)
       giving the rest */
    gear = GT4_U8(car + CAR_GEAR);
    if (gearbox == 2 && gear >= 2) gear = 1;
    clutch = clutch * GT4_F32(drive + DRIVE_RATIOS + gear * 4);

    /* torque the tyres return, per axle */
    n = GT4_U8(layout + LAYOUT_DIFFS);
    for (k = 0; k < n; k++) axle[k] = 0;
    for (int i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        int a = GT4_U8(layout + LAYOUT_AXLE + i);
        axle[a] = axle[a] + GT4_F32(gt4_car_wheel(car, i) + WHEEL_TORQUE);
    }
    if (GT4_U8(drive + 1)) {
        int j = GT4_U8(drive + 3), o = 1 - j;
        axle[o] = axle[o] * (GT4_F32(drive + DRIVE_RADIUS + o * 4) * GT4_F32(drive + DRIVE_RADIUS2 + j * 4));
    }

    /* the centre: how the two axles are coupled (func_0036D818's second
       table): 2 split by drive+4 of the speed difference, 3/10 a viscous
       coupling, 4/5/6/8/9 a coupling whose torque (couple) depends on the
       kind, locked (3) when it holds */
    if (kind == 2) centre = 1;
    else if (kind == 3 || kind == 10) centre = 2;
    else if (kind >= 4 && kind != 7) {
        float c = 0, f = GT4_F32(drive + 4);
        int lock = 0;
        switch (kind) {
        case 4:
            if (f == 1.0f) { c = BIG; goto decided; }
            if (K(0x3e4ccccc) < GT4_F32(b + 0x490)) goto decided;
            c = BIG;
            if (clutch <= 0) goto decided;
            if ((GT4_U32(b + 0x520) & 0xff00ffu) == 0x10000u) goto decided;
            c = f * clutch;
            goto chosen;
        case 5:
            c = lsd_centre(car, clutch, axle[0], axle[1]);
            goto half;
        case 6:
            if (K(0x3e4ccccc) < GT4_F32(b + 0x490)) goto half;
            c = BIG;
            if ((GT4_U32(b + 0x520) & 0xff00ffu) == 0x10000u) goto half;
            {
                float s = GT4_F32(b + 0x468), r;
                if (s < 0) s = -s;
                r = 1.0f - s * f;
                if (r < 0) r = 0;
                if (r < K(0x3f7d70a3)) c = r;
            }
            goto half;
        default:                                                /* 8, 9 */
            c = f * clutch;
            goto chosen;
        }
half:
        if (c != BIG && c != -BIG) c = (c * clutch) * 0.5f;
        else goto decided;
chosen:
decided:
        if (LOCKED(b)) {
            if (c == BIG) lock = 1;
            else if (c != -BIG) {
                int j = GT4_U8(drive + 3), o = 1 - j;
                float d = (axle[o] - axle[j]) - clutch, h = c;
                if (d < 0) d = -d;
                if (h < 0) h = -h;
                if (!(h < d)) lock = 1;
            }
            if (!lock) LOCKED(b) = 0;
        }
        if (lock) centre = 3;
        else {
            if (c == -BIG) couple = 0;
            else if (c == BIG) couple = GT4_F32(drive + 8);
            else {
                couple = c;                                     /* keeps the sign of a zero */
                if (couple < 0) couple = -couple;
            }
            if (couple != 0) centre = 1;
        }
        if (centre)
            for (int i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++)
                if (GT4_U8(layout + 0x2c + i) != GT4_U8(drive + 3))
                    GT4_U8(gt4_car_wheel(car, i) + WHEEL_FLAGS) = (uint8_t)(GT4_U8(gt4_car_wheel(car, i) + WHEEL_FLAGS) | 4);
    }

    /* inertia of the drive line in gear */
    if (GT4_U8(car + CAR_CLUTCH_STATE) == 1) {
        ratio_inertia = GT4_F32(drive + DRIVE_ENGINE_ROT) * GT4_F32(drive + DRIVE_RATIOS + gear * 4);
        if (GT4_U8(car + CAR_GEAR) == 0) ratio_inertia = -ratio_inertia;
        if (gearbox == 1 || gearbox == 2) ratio_inertia = ratio_inertia * GT4_F32(car + CAR_61C);
    }
    if (gearbox == 3) ratio_inertia = 0;

    if (centre == 3) {
        /* locked: both axles turn as one */
        float old = AXLE_SPEED(b, 0), v;
        AXLE_SPEED(b, 0) = old + ((clutch + axle[0]) + axle[1])
                           / ((GT4_F32(drive + DRIVE_INERTIA_F) + GT4_F32(drive + DRIVE_INERTIA_R)) + ratio_inertia) * dt;
        v = AXLE_SPEED(b, 0);
        if (braking(car)) {                   /* braking: stop at zero */
            if (v * old < 0 || old == 0) { AXLE_SPEED(b, 0) = 0; v = 0; }
        }
        AXLE_SPEED(b, 1) = v;
        AXLE_TORQUE(b, 1) = clutch * 0.5f;
        AXLE_TORQUE(b, 0) = clutch * 0.5f;
        return;
    }
    if (centre == 2) {
        /* viscous: the mean speed from the whole torque, the difference
           from the axles' torque difference less the coupling */
        float mean = (AXLE_SPEED(b, 0) + AXLE_SPEED(b, 1)) * 0.5f, m0 = mean, d, h;
        mean = mean + ((clutch + axle[0]) + axle[1])
                      / ((GT4_F32(drive + DRIVE_INERTIA_F) + GT4_F32(drive + DRIVE_INERTIA_R)) + ratio_inertia) * dt;
        if (braking(car)) {
            if (mean * m0 < 0 || m0 == 0) mean = 0;
        }
        d = AXLE_SPEED(b, 0) - AXLE_SPEED(b, 1);
        h = d * 0.5f;
        h = h + ((axle[0] - axle[1]) - GT4_F32(drive + 4) * d) * GT4_F32(drive + 0xc4) * dt;
        if (d * 0.5f * h < 0) h = 0;
        AXLE_TORQUE(b, 1) = clutch * 0.5f;
        AXLE_SPEED(b, 1) = mean - h;
        AXLE_SPEED(b, 0) = mean + h;
        AXLE_TORQUE(b, 0) = clutch * 0.5f;
        return;
    }

    /* the torque to each axle, and the inertia each turns */
    switch (kind) {
    case 1:
        AXLE_TORQUE(b, 0) = clutch;
        inertia[0] = GT4_F32(drive + DRIVE_INERTIA_F) + ratio_inertia;
        inertia[1] = 0;
        AXLE_TORQUE(b, 1) = 0;
        break;
    case 2:
        inertia[0] = GT4_F32(drive + DRIVE_INERTIA_F) + ratio_inertia;
        inertia[1] = GT4_F32(drive + DRIVE_INERTIA_R);
        split = GT4_F32(drive + 4) * (AXLE_SPEED(b, 0) - AXLE_SPEED(b, 1));
        AXLE_TORQUE(b, 1) = split;
        AXLE_TORQUE(b, 0) = clutch - split;
        break;
    case 9:
        inertia[0] = GT4_F32(drive + DRIVE_INERTIA_F) + ratio_inertia;
        inertia[1] = GT4_F32(drive + DRIVE_INERTIA_R);
        if (AXLE_SPEED(b, 1) < AXLE_SPEED(b, 0)) {
            AXLE_TORQUE(b, 1) = couple;
            AXLE_TORQUE(b, 0) = clutch - couple;
        } else {
            AXLE_TORQUE(b, 0) = clutch + couple;
            AXLE_TORQUE(b, 1) = -couple;
        }
        break;
    case 4: case 5: case 6: case 8:
        inertia[0] = GT4_F32(drive + DRIVE_INERTIA_F);
        inertia[1] = GT4_F32(drive + DRIVE_INERTIA_R) + ratio_inertia;
        if (AXLE_SPEED(b, 1) < AXLE_SPEED(b, 0)) {
            AXLE_TORQUE(b, 1) = clutch + couple;
            AXLE_TORQUE(b, 0) = -couple;
        } else {
            AXLE_TORQUE(b, 0) = couple;
            AXLE_TORQUE(b, 1) = clutch - couple;
        }
        break;
    default:                                                    /* 0: rear */
        inertia[0] = 0;
        AXLE_TORQUE(b, 1) = clutch;
        inertia[1] = GT4_F32(drive + DRIVE_INERTIA_R) + ratio_inertia;
        AXLE_TORQUE(b, 0) = 0;
        break;
    }
    split = centre == 1 ? AXLE_SPEED(b, 0) - AXLE_SPEED(b, 1) : 0;

    /* driven speed per differential */
    for (k = 0; k < GT4_U8(layout + LAYOUT_DIFFS); k++) {
        uint8_t a = GT4_U8(layout + LAYOUT_DIFF_WHEEL_A + k), w2;
        int idx = k < GT4_U8(layout + LAYOUT_DIFFS) ? GT4_U8(layout + LAYOUT_DIFF_INDEX + k) : 0;
        float old, next, d = 0;
        if (GT4_U8(drive + DRIVE_DIFF_TYPE + idx) == 0) continue;
        old = AXLE_SPEED(b, idx);
        if (inertia[idx] != 0) d = (AXLE_TORQUE(b, idx) + axle[idx]) / inertia[idx] * dt;
        next = old + d;
        AXLE_SPEED(b, idx) = next;
        w2 = GT4_U8(layout + LAYOUT_DIFF_WHEEL_B + a);
        if (w2 == 0x7f) w2 = a;
        if (GT4_F32(gt4_car_wheel(car, a) + WHEEL_BRAKE) == 0 && GT4_F32(gt4_car_wheel(car, w2) + WHEEL_BRAKE) == 0)
            continue;
        /* braking: stop at zero instead of reversing */
        if (next * old < 0 || old == 0) AXLE_SPEED(b, idx) = 0;
    }

    /* a coupled centre whose axles came to the same speed locks */
    if (centre == 1) {
        float d = AXLE_SPEED(b, 0) - AXLE_SPEED(b, 1);
        if ((0 < split && d <= 0) || (split < 0 && 0 <= d)) {
            int j = GT4_U8(drive + 3);
            AXLE_SPEED(b, 1 - j) = AXLE_SPEED(b, j);
            if ((uint8_t)(kind - 4) < 3 || kind == 9) LOCKED(b) = 1;
        }
    }
}
