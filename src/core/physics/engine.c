#include "engine.h"

#define ONE        gt4_f32(0x3f800000u)
#define HALF       gt4_f32(0x3f000000u)
#define MILLI      gt4_f32(0x3a83126eu)   /* 0.001 */
#define NINTH      gt4_f32(0x3de38e38u)   /* 1/9 */

/* func_00350678: thresholds from the settings bytes, once. */
static void wear_params_init(gt4_wear_params *p)
{
    uint8_t *s = gt4_ptr(GT4_GAME_SETTINGS);
    if (s[0xcc]) return;
    p->a_lo_loss = (float)s[0xd1] * MILLI;
    p->a_lo      = -(float)(s[0xd0] * 100000);
    p->a_hi0     = (float)(s[0xd2] * 100000);
    p->a_hi1     = (float)(s[0xd3] * 100000) + p->a_hi0;
    p->a_hi_loss = (float)s[0xd4] * MILLI;
    p->b_change  = (float)s[0xd6] * MILLI;
    p->b_lo      = -(float)(s[0xd5] * 100000) * HALF;
    p->b_hi1     = (float)(s[0xd7] * 100000);
    p->b_hi0     = (float)(s[0xd8] * 100000);
    s[0xcc] = 1;
}

float gt4_engine_wear_factor(gt4_wear_params *p, const gt4_engine_wear *w)
{
    float fa = ONE, fb = ONE;

    if (w->none) return ONE;
    wear_params_init(p);

    if (w->a < p->a_lo)       fa = ONE - p->a_lo_loss;
    else if (w->a < 0)        fa = ONE - w->a * p->a_lo_loss / p->a_lo;
    else if (w->a <= p->a_hi0) fa = ONE;
    else if (w->a < p->a_hi1) fa = ONE - (w->a - p->a_hi0) * p->a_hi_loss / (p->a_hi1 - p->a_hi0);
    else                      fa = ONE - p->a_hi_loss;

    if (w->b < p->b_lo)       fb = p->b_change + ONE;
    else if (w->b < 0)        fb = w->b * p->b_change / p->b_lo + ONE;
    else if (p->b_hi0 < w->b) {
        float t = (w->b - p->b_hi0) / (p->b_hi1 - p->b_hi0);
        if (ONE < t) t = ONE;
        fb = ONE - p->b_change * t;
    }
    return fa * fb;
}

float gt4_engine_torque(const gt4_engine *e, float speed, const gt4_engine_wear *wear)
{
    /* func_003433E8: a zeroed state with `none` set */
    static const gt4_engine_wear no_wear = { .none = 1 };
    float t = gt4_curve_eval(&e->torque, speed);
    return t * gt4_engine_wear_factor(gt4_ptr(GT4_WEAR_PARAMS), wear ? wear : &no_wear);
}

float gt4_engine_braking(const gt4_engine *e, float speed)
{
    if (*(uint8_t *)gt4_ptr(GT4_ENGINE_BRAKE_ALT)) {
        float top = e->speed[e->torque.n - 1];
        float top2 = top * top;
        return e->braking * (speed * speed + top2 * NINTH) / top2;
    }
    return e->braking * speed;
}
