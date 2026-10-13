/* Aerodynamics: drag, downforce and the slipstream (func_003F3090). */
#include "car.h"

#define K(bits) gt4_f32(bits)

/* Slipstream constants (D_0062230C..D_00622318). */
#define SLIP_DRAG_MIN   0x0062230cu   /* drag factor in full slipstream (0.5) */
#define SLIP_DOWN_MIN   0x00622310u   /* downforce factor in full slipstream (1.0) */
#define SLIP_RISE       0x00622314u   /* per second, times the target (2.0) */
#define SLIP_FALL       0x00622318u   /* per second (0.5) */

/* func_003F2DB8: some aero kinds add a share of the downforce scale to the
   rear value, weighted by CAR_698. */
static void extra_downforce(uint32_t car, float scale)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS);
    uint8_t kind = GT4_U8(params + PARAMS_AERO_KIND);
    if (kind == 0 || (kind >= 3 && (kind >= 7 || kind < 5))) return;
    GT4_F32(car + CAR_DOWNFORCE + 4) += GT4_F32(params + PARAMS_AERO_EXTRA) * scale * GT4_F32(car + CAR_698);
}

void gt4_car_aero(uint32_t car)
{
    const float one = K(0x3f800000u);
    uint32_t params = GT4_U32(car + CAR_PARAMS);
    float v = GT4_F32(car + CAR_SPEED), v2 = v * v, dt = GT4_F32(car + CAR_DT);
    float target = GT4_F32(car + CAR_SLIP_TARGET), s = GT4_F32(car + CAR_SLIP);
    float yaw, drag, down, outside;

    /* Slipstream amount: rises while behind a car, falls otherwise; 0..1. */
    if (target != 0) {
        s = s + target * GT4_F32(SLIP_RISE) * dt;
        GT4_F32(car + CAR_SLIP) = s;
        if (one < s) GT4_F32(car + CAR_SLIP) = one;
    } else {
        s = s - GT4_F32(SLIP_FALL) * dt;
        GT4_F32(car + CAR_SLIP) = s;
        if (s < 0) GT4_F32(car + CAR_SLIP) = 0;
    }
    s = GT4_F32(car + CAR_SLIP);
    outside = one - s;

    /* Angle between the body's side axis and the velocity. */
    {
        float lat = (GT4_F32(car + CAR_VELOCITY) * GT4_F32(car + CAR_MATRIX + 0x18)
                   + GT4_F32(car + CAR_VELOCITY + 4) * GT4_F32(car + CAR_MATRIX + 0x1c))
                   + GT4_F32(car + CAR_VELOCITY + 8) * GT4_F32(car + CAR_MATRIX + 0x20);
        yaw = gt4_atan2f(lat, v);
    }

    drag = GT4_F32(SLIP_DRAG_MIN) + (one - GT4_F32(SLIP_DRAG_MIN)) * outside;
    if ((int8_t)GT4_U8(car + CAR_6C7) == 2) drag = K(0x41a00000u);   /* 20 */
    drag = GT4_F32(params + PARAMS_DRAG) * (drag * v2);
    GT4_F32(car + CAR_DRAG) = (0 <= v) ? -drag : drag;

    /* Downforce, lost between 5 and 10 degrees of yaw. */
    down = GT4_F32(SLIP_DOWN_MIN) + (one - GT4_F32(SLIP_DOWN_MIN)) * outside;
    if (yaw < 0) yaw = -yaw;
    if (K(0x3e32b8c1u) < yaw) down = 0;                              /* 10 degrees */
    else if (K(0x3db2b8c1u) < yaw)                                    /* 5 degrees */
        down = down * ((K(0x3e32b8c1u) - yaw) / K(0x3db2b8c1u));
    down = down * v2;
    for (int i = 0; i < 2; i++)
        GT4_F32(car + CAR_DOWNFORCE + 4 * i) = GT4_F32(params + PARAMS_DOWNFORCE + 4 * i) * down;
    extra_downforce(car, down);
}
