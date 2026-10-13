/* Drivetrain step (func_003535B0): clutch state, brakes, rolling resistance,
   tyre coefficients, then the wheel/engine coupling. */
#include "car.h"

#define K(bits) gt4_f32(bits)
#define ONE K(0x3f800000u)

static uint32_t params_of(uint32_t car) { return GT4_U32(car + CAR_PARAMS); }
static uint32_t drive_of(uint32_t car) { return params_of(car) + PARAMS_DRIVE; }
static uint8_t gearbox(uint32_t drive) { return GT4_U8(drive + DRIVE_GEARBOX); }

/* func_00350DA0: speed of the driven wheels (front, rear or a mix). */
static float driven_speed(uint32_t car)
{
    uint32_t drive = drive_of(car);
    float front = GT4_F32(car + CAR_FRONT_SPEED), rear;
    switch (GT4_U8(drive + DRIVE_KIND)) {
    case 0: case 4: case 5: case 6: case 7: return GT4_F32(car + CAR_REAR_SPEED);
    case 1: case 2: case 9: return front;
    case 3: case 10:
        rear = GT4_F32(car + CAR_REAR_SPEED);
        return (front + rear) * K(0x3f000000u);
    case 8:
        rear = GT4_F32(car + CAR_REAR_SPEED);
        return rear + GT4_F32(drive + DRIVE_SPLIT) * (front - rear);
    default: return 0;
    }
}

/* func_00350E28: engine speed the driven wheels imply in the current gear. */
float gt4_wheel_engine_speed(uint32_t car)
{
    uint32_t drive = drive_of(car);
    float w = driven_speed(car);
    int gear = GT4_U8(car + CAR_GEAR);
    if (gearbox(drive) == 2 && gear >= 2) gear = 1;   /* one forward ratio */
    w = w * GT4_F32(drive + DRIVE_RATIOS + gear * 4);
    return gt4_select(w <= 0, 0.0f, w);
}

/* func_00351178 (gearbox kind 3). */
static void clutch_state_kind3(uint32_t car)
{
    GT4_U8(car + CAR_CLUTCH_STATE) = GT4_F32(car + CAR_CLUTCH_REQ) == 0 ? 0 : 1;
}

/* func_00350FB8 */
static void clutch_state(uint32_t car)
{
    uint32_t drive = drive_of(car);
    uint8_t state = GT4_U8(car + CAR_CLUTCH_STATE);

    if (state == 1) {
        if (GT4_F32(car + CAR_CLUTCH_REQ) == 0) GT4_U8(car + CAR_CLUTCH_STATE) = 0;
        else if ((uint8_t)(gearbox(drive) - 1) < 2)   /* func_003F1F98: kinds 1, 2 */
            GT4_F32(car + CAR_WHEEL_ENGINE_SPEED) = gt4_wheel_engine_speed(car);
    } else if (state == 0 && GT4_F32(car + CAR_CLUTCH_REQ) != 0) {
        float v = GT4_F32(car + CAR_SPEED);
        int gear = GT4_U8(car + CAR_GEAR);
        int by_throttle;
        /* moving faster than 10 km/h in a forward gear, or backwards in reverse */
        if (K(0x4031c71bu) < v && gear != 0) by_throttle = 1;
        else by_throttle = v < K(0xc031c71bu) && gear == 0;
        if (by_throttle) {
            if (K(0x3e800000u) < GT4_F32(car + CAR_THROTTLE)) GT4_U8(car + CAR_CLUTCH_STATE) = 2;
        } else if (GT4_U16(drive + DRIVE_CLUTCH_RPM) < (int16_t)GT4_U16(car + CAR_ENGINE_RPM)) {
            GT4_U8(car + CAR_CLUTCH_STATE) = 2;
        } else {
            GT4_F32(car + CAR_CLUTCH_REQ) = 0;
        }
    }

    state = GT4_U8(car + CAR_CLUTCH_STATE);
    if ((uint8_t)(state - 2) < 2) {
        int8_t mode = (int8_t)GT4_U8(car + CAR_56A);
        uint8_t kind = (mode != 1 && !GT4_U8(car + CAR_56B)) ? GT4_U8(car + CAR_546) : 0;
        float wheel;
        GT4_F32(car + CAR_ENGINE_SPEED_IN) = GT4_F32(car + CAR_ENGINE_SPEED);
        wheel = gt4_wheel_engine_speed(car);
        GT4_F32(car + CAR_WHEEL_ENGINE_SPEED) = wheel;
        if (kind == 3 || kind == 6 || GT4_U8(car + CAR_CLUTCH_STATE) != 3) return;
        GT4_F32(car + CAR_THROTTLE) = wheel < GT4_F32(car + CAR_ENGINE_SPEED_IN) ? 0 : ONE;
    }
}

/* func_003511A0: clutch state, then brake torque and wheel flags. */
static void brakes(uint32_t car)
{
    uint32_t params = params_of(car), layout = gt4_car_layout(car);
    int n, i;
    float handbrake;

    GT4_F32(car + CAR_ENGINE_SPEED_IN) = 0;
    GT4_F32(car + CAR_WHEEL_ENGINE_SPEED) = 0;
    if (gearbox(params + PARAMS_DRIVE) == 3) clutch_state_kind3(car);
    else clutch_state(car);
    if (GT4_U8(params + PARAMS_DRIVE + DRIVE_GEARBOX) == 4) GT4_U8(car + CAR_CLUTCH_STATE) = 1;

    n = GT4_U8(layout + LAYOUT_WHEELS);
    for (i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i);
        GT4_F32(w + WHEEL_BRAKE) = GT4_F32(w + WHEEL_BRAKE_IN)
            * GT4_F32(params + PARAMS_BRAKE + gt4_wheel_axle(layout, i) * 4);
    }
    handbrake = GT4_F32(car + CAR_HANDBRAKE) * GT4_F32(params + PARAMS_HANDBRAKE);
    for (i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i);
        if ((int8_t)GT4_U8(layout + LAYOUT_HANDBRAKE + i) < 0)
            GT4_F32(w + WHEEL_BRAKE) = GT4_F32(w + WHEEL_BRAKE) + handbrake;
    }
    for (i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i);
        uint8_t flags = GT4_F32(w + WHEEL_BRAKE) == 0 ? 0 : 2;
        if (GT4_F32(w + WHEEL_LOAD) == 0) flags |= 1;
        GT4_U8(w + WHEEL_FLAGS) = flags;
    }
}

/* func_003534A0: rolling resistance, sum of load * surface coefficient,
   scaled by a curve of the speed and opposed to the motion. */
#define ROLLING_CURVE   0x00620938u
#define ROLLING_SURFACE 0x00620984u   /* [surface] */
static void rolling_resistance(uint32_t car)
{
    int n = gt4_car_wheels(car);
    float sum, v, r;

    GT4_F32(car + CAR_668) = 0;
    GT4_F32(car + CAR_654) = 0;
    GT4_F32(car + CAR_ROLLING) = 0;
    sum = GT4_F32(car + CAR_668);
    for (int i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i);
        sum = sum + GT4_F32(w + WHEEL_LOAD) * GT4_F32(ROLLING_SURFACE + GT4_U8(w + WHEEL_SURFACE) * 4);
    }
    v = GT4_F32(car + CAR_SPEED);
    if (0 < v) r = -gt4_curve_eval(gt4_ptr(ROLLING_CURVE), v);
    else r = gt4_curve_eval(gt4_ptr(ROLLING_CURVE), -v);
    GT4_F32(car + CAR_ROLLING) = r * sum;
}

/* func_00351850: two tyre coefficients per wheel from WHEEL_38. */
static void tyre_coefficients(uint32_t car)
{
    uint32_t params = params_of(car);
    int n = gt4_car_wheels(car);
    for (int i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i);
        uint32_t tyre = gt4_tyre_params(params, i, GT4_U8(w + WHEEL_SURFACE));
        gt4_curve_eval2(gt4_ptr(tyre + TYRE_COEF_A), gt4_ptr(tyre + TYRE_COEF_B), GT4_F32(w + WHEEL_38),
                        gt4_ptr(w + WHEEL_COEF_A), gt4_ptr(w + WHEEL_COEF_B));
    }
}

/* func_003518E8 */
#define SETTINGS 0x00620cd8u
static void wheel_force_split(uint32_t car)
{
    int n = gt4_car_wheels(car);
    for (int i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i);
        float m = GT4_F32(w + WHEEL_COEF_A) * GT4_F32(w + WHEEL_GRIP), t;
        if ((int8_t)GT4_U8(w + WHEEL_45) < 0) m = -m;
        t = GT4_F32(w + WHEEL_3C) * m;
        if ((int8_t)GT4_U8(w + WHEEL_44) > 0) {
            GT4_F32(w + WHEEL_7C) = t;
            GT4_F32(w + WHEEL_80) = 0;
            GT4_F32(w + WHEEL_74) = 0;
            GT4_F32(w + WHEEL_70) = GT4_U8(SETTINGS + 0xb7) == 2 ? t : m;
        } else {
            GT4_F32(w + WHEEL_7C) = 0;
            GT4_F32(w + WHEEL_80) = t;
            GT4_F32(w + WHEEL_74) = GT4_U8(SETTINGS + 0xb6) == 2 ? t : m;
            GT4_F32(w + WHEEL_70) = 0;
        }
    }
}


void gt4_car_drivetrain(uint32_t car)
{
    uint32_t params = params_of(car), drive = params + PARAMS_DRIVE;
    int8_t mode = (int8_t)GT4_U8(car + CAR_56A);
    float load;
    int n, airborne;

    brakes(car);
    rolling_resistance(car);
    tyre_coefficients(car);

    /* Some modes scale the tyre coefficients by a per-car session value (percent). */
    if (mode == 1 || mode == 2 || GT4_U8(car + CAR_56B)) {
        int8_t pct = (int8_t)GT4_U8(GT4_U32(car + CAR_SESSION) + 0xf830 + GT4_U8(params + 0x31));
        if (pct != 0) {
            float scale = (float)pct * K(0x3c23d70au) + ONE;   /* 0.01 */
            n = gt4_car_wheels(car);
            for (int i = 0; i < n; i++) {
                uint32_t w = gt4_car_wheel(car, i);
                GT4_F32(w + WHEEL_VEL_WEIGHT) = GT4_F32(w + WHEEL_VEL_WEIGHT) * scale;
                GT4_F32(w + WHEEL_COEF_B) = GT4_F32(w + WHEEL_COEF_B) * scale;
            }
        }
    }

    wheel_force_split(car);
    gt4_car_tyre_forces(car);
    gt4_car_longitudinal(car);
    gt4_car_wheel_torque(car);
    gt4_car_diff_assist(car);
    gt4_car_clutch(car);
    gt4_car_differentials(car);
    if (gearbox(drive) == 3) {
        gt4_split_rpm(car);
        gt4_car_direction_select(car);
    } else if (gearbox(drive) == 1) {
        gt4_car_gearbox(car);
        gt4_car_direction_select(car);
    } else if (gearbox(drive) == 2) {
        gt4_car_gearbox(car);
        gt4_car_gear_select(car);
    } else if (GT4_U8(params + 0x32) == 2) {
        gt4_car_gearbox(car);
        gt4_car_direction_select(car);
    } else {
        gt4_car_gearbox(car);
        gt4_car_gear_select(car);
    }
    gt4_car_tachometer(car);

    load = 0;
    n = gt4_car_wheels(car);
    for (int i = 0; i < n; i++) load = load + GT4_F32(gt4_car_wheel(car, i) + WHEEL_LOAD);
    airborne = load == 0;
    gt4_car_yaw(car, airborne);
    gt4_car_velocity(car, airborne);
}
