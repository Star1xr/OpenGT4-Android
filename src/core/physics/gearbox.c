/* Gearbox: engine speed against the driven wheels, clutch engagement and
   gear selection (func_003519B8, func_00351DC8, func_00350F48). */
#include "car.h"

#define K(bits) gt4_f32(bits)
#define RPM_TO_RAD K(0x3dd6774eu)   /* 2 pi / 60 */

static uint32_t drive_of(uint32_t car) { return GT4_U32(car + CAR_PARAMS) + PARAMS_DRIVE; }
static uint32_t engine_of(uint32_t car) { return GT4_U32(car + CAR_PARAMS) + PARAMS_ENGINE; }

/* func_0035A5F8 */
static void gear_request(uint32_t car, int slot, int value)
{
    if (slot < 4) GT4_U8(car + CAR_GEAR_REQUEST + slot) = (uint8_t)(int8_t)value;
}

/* func_00350F48: engine speed as whole rpm, kept at least idle - 1 (gearbox
   kind 4: at least 0). */
static void engine_rpm(uint32_t car)
{
    int rpm = (int)(GT4_F32(car + CAR_ENGINE_SPEED) * K(0x42700000u) / K(0x40c90fdau));
    int idle = GT4_U16(engine_of(car) + ENGINE_IDLE) - 1;
    if (GT4_U8(drive_of(car) + DRIVE_GEARBOX) == 4) { if (rpm < 0) rpm = 0; }
    else if (rpm < idle) rpm = idle;
    GT4_U16(car + CAR_ENGINE_RPM) = (uint16_t)rpm;
}

static int driver_kind(uint32_t car)
{
    int8_t mode = (int8_t)GT4_U8(car + CAR_56A);
    return (mode != 1 && !GT4_U8(car + CAR_56B)) ? GT4_U8(car + CAR_546) : 0;
}

void gt4_car_gearbox(uint32_t car)
{
    uint32_t drive = drive_of(car), engine = engine_of(car);
    uint8_t state = GT4_U8(car + CAR_CLUTCH_STATE), gearbox = GT4_U8(drive + DRIVE_GEARBOX);
    float dt = GT4_F32(car + CAR_DT), speed;
    int s;

    if (state == 0) {
        /* clutch open: the engine spins on its own, not below idle (the
           hybrid charging its battery) */
        float idle = (float)GT4_U16(engine + ENGINE_IDLE) * RPM_TO_RAD;
        float t = GT4_U8(drive + DRIVE_KIND) == 7 ? gt4_hybrid_idle(car)
                                                   : gt4_car_engine_drive(car, GT4_F32(car + CAR_THROTTLE));
        float v = GT4_F32(car + CAR_ENGINE_SPEED) + t * GT4_F32(drive + DRIVE_ENGINE_INERTIA) * dt;
        GT4_F32(car + CAR_ENGINE_SPEED) = v;
        if (v < idle) GT4_F32(car + CAR_ENGINE_SPEED) = idle;
        s = GT4_U8(car + CAR_CLUTCH_STATE);
    } else {
        float wheel = gt4_wheel_engine_speed(car);
        s = state;
        if (state == 1) {
            /* engaged: the engine turns with the wheels */
            if ((uint8_t)(gearbox - 1) < 2) speed = GT4_F32(car + CAR_61C) * wheel;
            else speed = wheel;
            GT4_F32(car + CAR_ENGINE_SPEED) = speed;
            s = GT4_U8(car + CAR_CLUTCH_STATE);
        } else if ((state == 2 || state == 3) && (driver_kind(car) == 3 || driver_kind(car) == 6)
                   && GT4_F32(car + CAR_CLUTCH_REQ) == K(0x3f800000u)) {
            /* the clutch let in at once: the wheels take the engine's
               speed, each differential through its ratio */
            uint32_t layout = gt4_car_layout(car);
            int gear = GT4_U8(car + CAR_GEAR);
            float v;
            if (gearbox == 2 && gear >= 2) gear = 1;
            v = GT4_F32(car + CAR_ENGINE_SPEED) / GT4_F32(drive + DRIVE_RATIOS + (uint32_t)gear * 4);
            for (int k = 0; k < GT4_U8(layout + LAYOUT_DIFFS); k++) {
                uint8_t a = GT4_U8(layout + LAYOUT_DIFF_WHEEL_A + k), b;
                GT4_F32(car + 0x630 + (uint32_t)k * 4) = v;              /* the axle's speed */
                v = v * GT4_F32(drive + DRIVE_RADIUS + (uint32_t)GT4_U8(layout + LAYOUT_DIFF_INDEX + k) * 4);
                GT4_F32(gt4_car_wheel(car, a) + 0x20) = v;
                b = GT4_U8(layout + LAYOUT_DIFF_WHEEL_B + a);
                if (b != 0x7f) GT4_F32(gt4_car_wheel(car, b) + 0x20) = v;
            }
            GT4_U8(car + CAR_CLUTCH_STATE) = 1;
            s = 1;
            speed = GT4_F32(car + CAR_ENGINE_SPEED);
        } else if (state == 2 || state == 3) {
            float in = GT4_F32(car + CAR_ENGINE_SPEED_IN), at = GT4_F32(car + CAR_WHEEL_ENGINE_SPEED);
            speed = GT4_F32(car + CAR_ENGINE_SPEED);
            if ((in <= at && wheel <= speed) || (at <= in && speed <= wheel)) {
                /* engine and wheels meet: engage */
                GT4_U8(car + CAR_CLUTCH_STATE) = 1;
                speed = wheel;
                GT4_U8(car + CAR_62A) = 12;
                s = 1;
                GT4_F32(car + CAR_ENGINE_SPEED) = wheel;
            }
            if (s == 3 && GT4_F32(car + CAR_WHEEL_ENGINE_SPEED)
                              <= (float)GT4_U16(engine + ENGINE_IDLE) * RPM_TO_RAD) {
                GT4_U8(car + CAR_CLUTCH_STATE) = 0;
                s = 0;
            }
            if (s == 2 && K(0x3f400000u) < GT4_F32(car + CAR_BRAKE_PEDAL)) {
                float v = GT4_F32(car + CAR_SPEED);
                if (speed < (float)(GT4_U16(engine + ENGINE_IDLE) + 500) * RPM_TO_RAD
                    || (v < K(0x4031c71bu) && K(0xc031c71bu) < v)) {
                    GT4_U8(car + CAR_CLUTCH_STATE) = 0;
                    s = 0;
                }
            }
        } else {
            speed = GT4_F32(car + CAR_ENGINE_SPEED);
        }
        if (speed < 0) {
            GT4_F32(car + CAR_ENGINE_SPEED) = 0;
        } else {
            float top = (float)(GT4_U16(engine + ENGINE_REDLINE) + 10) * RPM_TO_RAD;
            if (top <= speed) GT4_F32(car + CAR_ENGINE_SPEED) = top;
        }
    }
    if (s != 3) gear_request(car, 0, 0);
    engine_rpm(car);
}

/* func_0036C098: the model's shift offset from the pedals (car+0x62b,
   held 60 steps after a change, counting in car+0x62d). */
static void shift_bias(uint32_t car)
{
    int32_t want = -1, cur;
    float t = GT4_F32(car + CAR_THROTTLE_REQ);
    if (!(t <= 0)) want = (int32_t)((1.0f - t) * 3.0f);
    if (GT4_F32(car + CAR_BRAKE_PEDAL) == 1.0f) want = 0;
    if (GT4_U8(car + 0x62c)) GT4_U8(car + 0x62c) = (uint8_t)(GT4_U8(car + 0x62c) - 1);
    cur = GT4_U8(car + 0x62b);
    if (want == cur || want == -1) { GT4_U8(car + 0x62d) = 0; return; }
    {
        uint8_t n = (uint8_t)(GT4_U8(car + 0x62d) + 1);
        GT4_U8(car + 0x62d) = n;
        if (want < cur ? n < 13 : n < 61) return;
        GT4_U8(car + 0x62c) = 60;
        GT4_U8(car + 0x62b) = (uint8_t)(want < cur ? cur - 1 : cur + 1);
        GT4_U8(car + 0x62d) = 0;
    }
}

/* func_0036BF28: rpm below which gear g shifts down. */
static uint32_t downshift_rpm(uint32_t car, int g)
{
    uint32_t rpm = GT4_U16(GT4_U32(car + CAR_PARAMS) + 0x260 + (uint32_t)g * 2);
    if ((int8_t)GT4_U8(car + CAR_56A) == 1) rpm = (rpm * GT4_U8(car + 0x83)) >> 7;
    return rpm;
}

#define SHIFT_SETTINGS 0x00622110u   /* +8 shift mode (1 hold, 2 sequential), +9..+0xb rpm offsets / 100 */

/* func_0036C160: the automatic gearbox (model: the gear for the speed plus
   its offset). The gear wanted, or -1 to keep this one. */
static int auto_gear(uint32_t car, int model)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), drive = params + PARAMS_DRIVE;
    int gear = GT4_U8(car + CAR_GEAR), count = GT4_U8(drive + 2);
    float v = GT4_F32(car + CAR_SPEED);
    int32_t up, down;
    int front = 0, rear = 0;

    if (GT4_U16(car + 0x62e)) GT4_U16(car + 0x62e) = (uint16_t)(GT4_U16(car + 0x62e) - 1);
    if (model) shift_bias(car);
    if (GT4_U8(car + 0x62a)) {
        uint8_t t = (uint8_t)(GT4_U8(car + 0x62a) - 1);
        GT4_U8(car + 0x62a) = t;
        if (GT4_U8(SHIFT_SETTINGS + 8) == 2 && !t && (int8_t)GT4_U8(car + 0x567)) GT4_U8(car + 0x62a) = 1;
        return -1;
    }
    if ((int8_t)GT4_U8(car + 0x566)) return gear ? 0 : -1;
    if (K(0x3e8e38e3u) < v && !gear) return 1;
    if (0 < GT4_F32(car + CAR_THROTTLE)) {
        if (v < K(0x3e8e38e3u)) return gear == 1 ? -1 : 1;
    } else if (v < K(0xbe8e38e3u)) {
        if (gear) return 0;
        goto mode;
    }
    if (gear > 0 && gear < count) {
        float top = ((float)(int32_t)(GT4_U16(params + PARAMS_ENGINE + ENGINE_REDLINE) + 500) * K(0x3dd6774eu))
                    / GT4_F32(drive + 0x68 + (uint32_t)gear * 4);
        if (top < v) return gear + 1;
    }
mode:
    {
        uint8_t m = GT4_U8(SHIFT_SETTINGS + 8);
        if (m == 1) { if ((int8_t)GT4_U8(car + 0x567)) return -1; }
        else if (m == 2) {
            int8_t b = (int8_t)GT4_U8(car + 0x567);
            if (b > 0 && gear < GT4_U8(params + 0x236)) {
                GT4_U8(car + 0x62a) = 1;
                GT4_U16(car + 0x62e) = 240;
                return gear + 1;
            }
            if (b < 0 && gear >= 2) {
                GT4_U16(car + 0x62e) = 240;
                GT4_U8(car + 0x62a) = 1;
                return gear - 1;
            }
            if (GT4_U16(car + 0x62e)) return -1;
        }
    }
    if (GT4_U8(car + CAR_CLUTCH_STATE) != 1) return -1;
    if (model) {
        int g, top;
        if (!gear) return -1;
        g = gt4_gear_for_speed(car, v) + GT4_U8(car + 0x62b);
        top = GT4_U8(params + 0x236);
        if (top < g) g = top;
        return gear == g ? -1 : g;
    }
    if (!gear) return -1;
    if (gear == 1) {
        if (count == 1) return -1;
        up = (int32_t)gt4_shift_rpm(car);
        down = -20000;
    } else {
        float t = GT4_F32(car + CAR_THROTTLE_REQ);
        up = gear < count ? (int32_t)gt4_shift_rpm(car) : 20000;
        down = (int32_t)downshift_rpm(car, gear);
        if (t < K(0x3d4ccccc)) down = down + GT4_U8(SHIFT_SETTINGS + 0xa) * 100;
        if (K(0x3f4ccccc) < GT4_F32(car + CAR_BRAKE_PEDAL)) down = down + GT4_U8(SHIFT_SETTINGS + 0xb) * 100;
        if (0.25f < t) down = down - GT4_U8(SHIFT_SETTINGS + 9) * 100;
    }
    switch (GT4_U8(drive + DRIVE_KIND)) {
    case 1: case 2: case 9: front = 1; break;
    case 3: case 8: case 10: front = rear = 1; break;
    case 0: case 4: case 5: case 6: case 7: rear = 1; break;
    default: break;
    }
    {
        int32_t rpm = (int16_t)GT4_U16(car + CAR_ENGINE_RPM);
        uint32_t layout = gt4_car_layout(car);
        int n = GT4_U8(layout + LAYOUT_WHEELS);
        if (rpm < down) return gear - 1;
        if (!(up < rpm)) return -1;
        for (int i = 0; i < n; i++) {
            int8_t pos = (int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i);
            uint32_t w = gt4_car_wheel(car, i), tp;
            if (!((front && pos > 0) || (rear && pos < 0))) continue;
            if (GT4_F32(w + WHEEL_LOAD) == 0) return -1;
            tp = gt4_tyre_params(params, i, GT4_U8(w + WHEEL_SURFACE));
            if (GT4_F32(w + WHEEL_38) < GT4_F32(tp + 0x164) * GT4_F32(w + WHEEL_3C)) return -1;
        }
        return gear + 1;
    }
}

/* func_0036BF68: the gear to start off in for the speed: the first
   whose top speed (the shift rpm less a margin, over the per-gear speed
   ratio at params+0x2A0) is above it. */
static int start_gear(uint32_t car, float speed)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), drive = params + PARAMS_DRIVE;
    int n;
    if (speed < 0) return 0;
    if (GT4_U8(drive + DRIVE_GEARBOX) == 3 || GT4_U8(drive + DRIVE_GEARBOX) == 1) return 1;
    n = GT4_U8(drive + 2);
    for (int g = 1; g < n; g++) {
        uint32_t margin = g == 1 ? 2000u : 500u;
        float top = ((float)(int32_t)(gt4_shift_rpm(car) - margin) * RPM_TO_RAD)
                    / GT4_F32(params + 0x2a0 + (uint32_t)(g - 1) * 4);
        if (speed < top) return g;
    }
    return n;
}

/* The shift timer (car+0x62A) and the buttons: car+0x566 reverse,
   car+0x567 up (> 0) or down (< 0). */
static int shift_buttons(uint32_t car, int lowest)
{
    int gear = GT4_U8(car + CAR_GEAR), btn = (int8_t)GT4_U8(car + 0x567);
    if (btn > 0 && gear < GT4_U8(drive_of(car) + 2)) {
        GT4_U8(car + 0x62a) = 1;
        return gear + 1;
    }
    if (btn < 0 && gear > lowest) {
        GT4_U8(car + 0x62a) = 1;
        return gear - 1;
    }
    return -1;
}

/* func_0036C690: manual, starting off in gear by itself. */
static int manual_start(uint32_t car)
{
    int gear = GT4_U8(car + CAR_GEAR);
    if ((int8_t)GT4_U8(car + 0x566)) return gear ? 0 : -1;
    if (gear == 0 && 0 < GT4_F32(car + CAR_THROTTLE_REQ)) {
        int g = start_gear(car, GT4_F32(car + CAR_SPEED));
        return g ? g : 1;
    }
    if (GT4_U8(car + 0x62a)) {
        uint8_t t = (uint8_t)(GT4_U8(car + 0x62a) - 1);
        GT4_U8(car + 0x62a) = t;
        if (!t && (int8_t)GT4_U8(car + 0x567)) GT4_U8(car + 0x62a) = 1;
        return -1;
    }
    return shift_buttons(car, 1);
}

/* func_0036C780: manual (driver kinds 3, 4, 6). */
static int manual(uint32_t car)
{
    int gear = GT4_U8(car + CAR_GEAR);
    if (GT4_U8(car + 0x62a)) {
        uint8_t t = (uint8_t)(GT4_U8(car + 0x62a) - 1);
        GT4_U8(car + 0x62a) = t;
        if (!t && (GT4_U32(car + 0x564) & 0xffff0000u)) GT4_U8(car + 0x62a) = 1;
        return -1;
    }
    if ((int8_t)GT4_U8(car + 0x566)) {
        if (!gear) return -1;
        GT4_U8(car + 0x62a) = 1;
        return 0;
    }
    return shift_buttons(car, 1);
}

/* func_0036C850: sequential down into reverse (driver kind 5). */
static int sequential(uint32_t car)
{
    if (GT4_U8(car + 0x62a)) {
        uint8_t t = (uint8_t)(GT4_U8(car + 0x62a) - 1);
        GT4_U8(car + 0x62a) = t;
        if (!t && (int8_t)GT4_U8(car + 0x567)) GT4_U8(car + 0x62a) = 1;
        return -1;
    }
    return shift_buttons(car, 0);
}

/* func_0036C8E8: the gear the controls ask for, by driver kind (-1 none). */
static int gear_controls(uint32_t car)
{
    int kind = 0;
    if (GT4_U16(car + CAR_5BE)) return GT4_U8(car + CAR_GEAR) == 1 ? -1 : 1;
    if ((int8_t)GT4_U8(car + CAR_56A) != 1 && !GT4_U8(car + CAR_56B)) kind = GT4_U8(car + CAR_546);
    switch (kind) {
    case 0: return auto_gear(car, 0);
    case 2: return auto_gear(car, 1);
    case 1: return manual_start(car);
    case 3: case 4: case 6: return manual(car);
    case 5: return sequential(car);
    default: return -1;
    }
}

/* func_00351DC8: the driver's gear choice and the shift (func_0036BEB8). */
void gt4_car_gear_select(uint32_t car)
{
    int want = gear_controls(car);
    if (want == -1) return;
    if (GT4_U8(car + CAR_CLUTCH_STATE)) {
        if (want == 0 || GT4_U8(car + CAR_GEAR) == 0) {
            GT4_U8(car + CAR_CLUTCH_STATE) = 2;
        } else {
            int gear;
            if (GT4_U8(drive_of(car) + DRIVE_GEARBOX) != 2) GT4_U8(car + CAR_CLUTCH_STATE) = 3;
            gear = GT4_U8(car + CAR_GEAR);
            if (gear < want) gear_request(car, 0, 1);
            else if (want < gear) gear_request(car, 0, -1);
        }
    }
    if (!(GT4_U8(GT4_U32(car + CAR_PARAMS) + 0x32) == 4 && want == 0)) GT4_U8(car + CAR_GEAR) = (uint8_t)want;
}

/* func_00351EB0: the forward/reverse selector (automatic 1, and cars
   with params+0x32 = 2): drive when moving forward or on the throttle,
   reverse when rolling back, neutral when off (car+0x5BE). */
void gt4_car_direction_select(uint32_t car)
{
    int gear = GT4_U8(car + CAR_GEAR), want = -1;
    float v = GT4_F32(car + CAR_SPEED);
    if (GT4_U16(car + CAR_5BE)) {
        if (gear == 1) return;
        want = 1;
    } else if ((int8_t)GT4_U8(car + 0x566)) {
        if (!gear) return;
        want = 0;
    } else if (K(0x3e8e38e3u) < v || 0 < GT4_F32(car + CAR_THROTTLE)) {
        if (gear) return;
        want = 1;
    } else if (v < K(0xbe8e38e3u)) {
        if (gear) want = 0;
    }
    if (want == -1) return;
    if (!(GT4_U8(GT4_U32(car + CAR_PARAMS) + 0x32) == 4 && want == 0)) GT4_U8(car + CAR_GEAR) = (uint8_t)want;
    if ((uint8_t)(GT4_U8(drive_of(car) + DRIVE_GEARBOX) - 1) < 2) GT4_U8(car + CAR_CLUTCH_STATE) = 2;
}

/* func_003694D0: the power split's tachometer, whole rpm. */
void gt4_split_rpm(uint32_t car)
{
    GT4_U16(car + CAR_ENGINE_RPM) = (uint16_t)(int32_t)((GT4_F32(car + CAR_ENGINE_SPEED) * K(0x42700000u)) / K(0x40c90fdau));
}
