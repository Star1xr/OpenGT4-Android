/*
 * EE FPU (COP1) arithmetic with PS2 semantics, for recompiled code.
 *
 * The PS2 FPU is not IEEE 754:
 *  - no infinities or NaNs: an input with exponent 255 is read as +/-FLT_MAX;
 *  - denormal inputs are read as +/-0;
 *  - a result that overflows becomes +/-FLT_MAX and sets O (and sticky SO);
 *  - a result that would be denormal becomes +/-0 and sets U (and SU);
 *  - division by zero gives +/-FLT_MAX and sets D (0/0 sets I instead);
 *  - rounding is toward zero.
 * This follows the behaviour of PCSX2's FPU interpreter with its default
 * settings (round toward zero). Rounding toward zero is set on the host by
 * ee_fpu_enter(); the operations live in fpu.c, compiled with floating-point
 * environment access on so the compiler keeps that rounding mode.
 *
 * Registers are passed and returned as raw 32-bit patterns.
 */
#ifndef OPENGT4_EE_FPU_H
#define OPENGT4_EE_FPU_H

#include "ee.h"

#define EE_FPU_C  0x00800000u   /* condition */
#define EE_FPU_I  0x00020000u   /* invalid */
#define EE_FPU_D  0x00010000u   /* divide by zero */
#define EE_FPU_O  0x00008000u   /* overflow */
#define EE_FPU_U  0x00004000u   /* underflow */
#define EE_FPU_SI 0x00000040u   /* sticky */
#define EE_FPU_SD 0x00000020u
#define EE_FPU_SO 0x00000010u
#define EE_FPU_SU 0x00000008u

/* Set / restore the host rounding mode (toward zero) for guest code. */
void ee_fpu_enter(void);
void ee_fpu_leave(void);

uint32_t ee_fpu_add(ee_ctx *c, uint32_t a, uint32_t b);
uint32_t ee_fpu_sub(ee_ctx *c, uint32_t a, uint32_t b);
uint32_t ee_fpu_mul(ee_ctx *c, uint32_t a, uint32_t b);
uint32_t ee_fpu_div(ee_ctx *c, uint32_t a, uint32_t b);
uint32_t ee_fpu_sqrt(ee_ctx *c, uint32_t b);
uint32_t ee_fpu_rsqrt(ee_ctx *c, uint32_t a, uint32_t b);
uint32_t ee_fpu_max(ee_ctx *c, uint32_t a, uint32_t b);
uint32_t ee_fpu_min(ee_ctx *c, uint32_t a, uint32_t b);
uint32_t ee_fpu_abs(ee_ctx *c, uint32_t a);
uint32_t ee_fpu_neg(ee_ctx *c, uint32_t a);
uint32_t ee_fpu_cvt_s_w(ee_ctx *c, uint32_t a);   /* int32 -> float */
uint32_t ee_fpu_cvt_w_s(ee_ctx *c, uint32_t a);   /* float -> int32, toward zero */
/* Accumulator forms (ACC held as raw bits in c->acc). */
void     ee_fpu_adda(ee_ctx *c, uint32_t a, uint32_t b);
void     ee_fpu_suba(ee_ctx *c, uint32_t a, uint32_t b);
void     ee_fpu_mula(ee_ctx *c, uint32_t a, uint32_t b);
void     ee_fpu_madda(ee_ctx *c, uint32_t a, uint32_t b);
void     ee_fpu_msuba(ee_ctx *c, uint32_t a, uint32_t b);
uint32_t ee_fpu_madd(ee_ctx *c, uint32_t a, uint32_t b);  /* ACC + a*b */
uint32_t ee_fpu_msub(ee_ctx *c, uint32_t a, uint32_t b);  /* ACC - a*b */
/* Compares set or clear the C bit. */
void     ee_fpu_c_f(ee_ctx *c);
void     ee_fpu_c_eq(ee_ctx *c, uint32_t a, uint32_t b);
void     ee_fpu_c_lt(ee_ctx *c, uint32_t a, uint32_t b);
void     ee_fpu_c_le(ee_ctx *c, uint32_t a, uint32_t b);

#endif /* OPENGT4_EE_FPU_H */
