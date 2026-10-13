/* Engine: torque curve, engine braking and the power factor from the engine's
   wear state. */
#ifndef OPENGT4_PHYS_ENGINE_H
#define OPENGT4_PHYS_ENGINE_H

#include "phys.h"

/* Engine block of the car parameters (car params + 0x134). Engine speeds in
   rad/s, torque in N*m. */
typedef struct {
    gt4_curve torque;        /* 0x00: points at speed[] and torque_nm[] */
    float     speed[24];     /* 0x0c */
    float     torque_nm[24]; /* 0x6c */
    uint8_t   _0cc[4];
    float     braking;       /* 0xd0: engine braking torque per rad/s */
} gt4_engine;

/* Per-car state read by the power factor (car + 0x6dc). The meaning of the
   two values is not established yet; the thresholds suggest distances. */
typedef struct {
    uint32_t _00;
    float    a;              /* 0x04 */
    float    b;              /* 0x08 */
    uint8_t  _0c[8];
    uint8_t  none;           /* 0x14: nonzero = factor 1 */
    uint8_t  _15[11];
} gt4_engine_wear;

/* Thresholds of the power factor (D_00620320), built once from settings
   bytes at D_00620CD8 + 0xd0..0xd8 (func_00350678). */
typedef struct {
    float a_lo, a_lo_loss;           /* a < 0: loss up to a_lo_loss at a_lo */
    float a_hi0, a_hi1, a_hi_loss;   /* a > a_hi0: loss up to a_hi_loss at a_hi1 */
    float b_lo, b_change;            /* b < 0: gain up to b_change at b_lo */
    float b_hi1, b_hi0;              /* b > b_hi0: loss up to b_change at b_hi1 */
} gt4_wear_params;

#define GT4_WEAR_PARAMS      0x00620320u
#define GT4_GAME_SETTINGS    0x00620cd8u
#define GT4_ENGINE_BRAKE_ALT 0x00620e14u   /* byte: alternative engine braking */

/* func_0034E9A0: torque at `speed` (rad/s); wear NULL = no wear. */
float gt4_engine_torque(const gt4_engine *e, float speed, const gt4_engine_wear *wear);
/* func_0034EA08: engine braking torque at `speed`. */
float gt4_engine_braking(const gt4_engine *e, float speed);
/* func_00350868: power factor (1 = none). */
float gt4_engine_wear_factor(gt4_wear_params *p, const gt4_engine_wear *w);

#endif
