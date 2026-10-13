/*
 * EE FPU arithmetic with PS2 semantics. See fpu.h.
 *
 * Every operation here relies on the host rounding mode set by
 * ee_fpu_enter() (toward zero). Floating-point environment access is turned
 * on for this file so the compiler does not assume round-to-nearest.
 */
#include "fpu.h"

#include <fenv.h>
#include <math.h>
#include <string.h>

#ifdef _MSC_VER
#pragma fenv_access (on)
#else
#pragma STDC FENV_ACCESS ON
#endif

#define SIGN 0x80000000u
#define EXP  0x7f800000u
#define MANT 0x007fffffu
#define FMAX 0x7f7fffffu

static int saved_round = -1;

void ee_fpu_enter(void)
{
    if (saved_round < 0)
        saved_round = fegetround();
    fesetround(FE_TOWARDZERO);
}

void ee_fpu_leave(void)
{
    if (saved_round >= 0)
        fesetround(saved_round);
    saved_round = -1;
}

static float f_of(uint32_t v) { float f; memcpy(&f, &v, 4); return f; }
static uint32_t u_of(float f) { uint32_t v; memcpy(&v, &f, 4); return v; }

/* PS2 reading of an operand: denormal -> +/-0, Inf/NaN -> +/-FLT_MAX. */
static float in(uint32_t v)
{
    uint32_t e = v & EXP;
    if (e == 0)
        return f_of(v & SIGN);
    if (e == EXP)
        return f_of((v & SIGN) | FMAX);
    return f_of(v);
}

/* PS2 writing of a result: overflow -> +/-FLT_MAX with O, denormal -> +/-0 with U.
   With rounding toward zero an overflow does not produce infinity (it rounds
   down to FLT_MAX), so overflow is detected from `exact`, the result computed
   in double precision. */
static uint32_t out2(ee_ctx *c, float f, double exact)
{
    uint32_t r = u_of(f), e = r & EXP;
    if (e == EXP || fabs(exact) > 3.4028234663852886e38) {
        c->fcr31 |= EE_FPU_O | EE_FPU_SO;
        return (r & SIGN) | FMAX;
    }
    if (e == 0 && (r & MANT)) {
        c->fcr31 |= EE_FPU_U | EE_FPU_SU;
        return r & SIGN;
    }
    return r;
}

static uint32_t out(ee_ctx *c, float f) { return out2(c, f, f); }

static void clear_ou(ee_ctx *c) { c->fcr31 &= ~(EE_FPU_O | EE_FPU_U); }

uint32_t ee_fpu_add(ee_ctx *c, uint32_t a, uint32_t b) { clear_ou(c); return out2(c, in(a) + in(b), (double)in(a) + in(b)); }
uint32_t ee_fpu_sub(ee_ctx *c, uint32_t a, uint32_t b) { clear_ou(c); return out2(c, in(a) - in(b), (double)in(a) - in(b)); }
uint32_t ee_fpu_mul(ee_ctx *c, uint32_t a, uint32_t b) { clear_ou(c); return out2(c, in(a) * in(b), (double)in(a) * in(b)); }

uint32_t ee_fpu_div(ee_ctx *c, uint32_t a, uint32_t b)
{
    float x = in(a), y = in(b);
    c->fcr31 &= ~(EE_FPU_I | EE_FPU_D);
    if (y == 0.0f) {
        if (x == 0.0f)
            c->fcr31 |= EE_FPU_I | EE_FPU_SI;
        else
            c->fcr31 |= EE_FPU_D | EE_FPU_SD;
        return ((a ^ b) & SIGN) | FMAX;
    }
    return out2(c, x / y, (double)x / y);
}

uint32_t ee_fpu_sqrt(ee_ctx *c, uint32_t b)
{
    float y = in(b);
    c->fcr31 &= ~(EE_FPU_I | EE_FPU_D);
    if (y == 0.0f)
        return b & SIGN;                  /* +/-0, as the input's sign */
    if (y < 0.0f)
        c->fcr31 |= EE_FPU_I | EE_FPU_SI;
    return u_of(sqrtf(fabsf(y)));
}

uint32_t ee_fpu_rsqrt(ee_ctx *c, uint32_t a, uint32_t b)
{
    float x = in(a), y = in(b);
    c->fcr31 &= ~(EE_FPU_I | EE_FPU_D);
    if (y == 0.0f) {
        c->fcr31 |= EE_FPU_D | EE_FPU_SD;
        return ((a ^ b) & SIGN) | FMAX;
    }
    if (y < 0.0f)
        c->fcr31 |= EE_FPU_I | EE_FPU_SI;
    return out2(c, x / sqrtf(fabsf(y)), (double)x / sqrt(fabs((double)y)));
}

uint32_t ee_fpu_max(ee_ctx *c, uint32_t a, uint32_t b)
{
    clear_ou(c);
    return in(a) >= in(b) ? a : b;        /* raw operand, as the PS2 moves it */
}

uint32_t ee_fpu_min(ee_ctx *c, uint32_t a, uint32_t b)
{
    clear_ou(c);
    return in(a) <= in(b) ? a : b;
}

uint32_t ee_fpu_abs(ee_ctx *c, uint32_t a) { clear_ou(c); return a & ~SIGN; }
uint32_t ee_fpu_neg(ee_ctx *c, uint32_t a) { clear_ou(c); return a ^ SIGN; }

uint32_t ee_fpu_cvt_s_w(ee_ctx *c, uint32_t a)
{
    (void)c;
    return u_of((float)(int32_t)a);       /* rounded toward zero by the host mode */
}

uint32_t ee_fpu_cvt_w_s(ee_ctx *c, uint32_t a)
{
    (void)c;
    /* |value| >= 2^31 (exponent >= 158) saturates by sign; NaN/Inf included. */
    if ((a & EXP) >= (158u << 23))
        return (a & SIGN) ? 0x80000000u : 0x7fffffffu;
    return (uint32_t)(int32_t)in(a);      /* C conversion truncates toward zero */
}

static uint32_t acc_bits(const ee_ctx *c) { return u_of(c->acc); }
static void set_acc(ee_ctx *c, uint32_t v) { c->acc = f_of(v); }

void ee_fpu_adda(ee_ctx *c, uint32_t a, uint32_t b) { clear_ou(c); set_acc(c, out2(c, in(a) + in(b), (double)in(a) + in(b))); }
void ee_fpu_suba(ee_ctx *c, uint32_t a, uint32_t b) { clear_ou(c); set_acc(c, out2(c, in(a) - in(b), (double)in(a) - in(b))); }
void ee_fpu_mula(ee_ctx *c, uint32_t a, uint32_t b) { clear_ou(c); set_acc(c, out2(c, in(a) * in(b), (double)in(a) * in(b))); }

/* Multiply-accumulate: the product is rounded, then added (two roundings). */
static float product(ee_ctx *c, uint32_t a, uint32_t b) { return in(out2(c, in(a) * in(b), (double)in(a) * in(b))); }

void ee_fpu_madda(ee_ctx *c, uint32_t a, uint32_t b)
{
    clear_ou(c);
    { float p = product(c, a, b); set_acc(c, out2(c, in(acc_bits(c)) + p, (double)in(acc_bits(c)) + p)); }
}

void ee_fpu_msuba(ee_ctx *c, uint32_t a, uint32_t b)
{
    clear_ou(c);
    { float p = product(c, a, b); set_acc(c, out2(c, in(acc_bits(c)) - p, (double)in(acc_bits(c)) - p)); }
}

uint32_t ee_fpu_madd(ee_ctx *c, uint32_t a, uint32_t b)
{
    clear_ou(c);
    float p = product(c, a, b);
    return out2(c, in(acc_bits(c)) + p, (double)in(acc_bits(c)) + p);
}

uint32_t ee_fpu_msub(ee_ctx *c, uint32_t a, uint32_t b)
{
    clear_ou(c);
    float p = product(c, a, b);
    return out2(c, in(acc_bits(c)) - p, (double)in(acc_bits(c)) - p);
}

static void set_c(ee_ctx *c, int v)
{
    if (v)
        c->fcr31 |= EE_FPU_C;
    else
        c->fcr31 &= ~EE_FPU_C;
}

void ee_fpu_c_f(ee_ctx *c) { set_c(c, 0); }
void ee_fpu_c_eq(ee_ctx *c, uint32_t a, uint32_t b) { set_c(c, in(a) == in(b)); }
void ee_fpu_c_lt(ee_ctx *c, uint32_t a, uint32_t b) { set_c(c, in(a) < in(b)); }
void ee_fpu_c_le(ee_ctx *c, uint32_t a, uint32_t b) { set_c(c, in(a) <= in(b)); }
