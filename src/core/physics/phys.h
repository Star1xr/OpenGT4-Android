/*
 * GT4 vehicle physics, rewritten from the original code (CORE.GT4, US v2.00)
 * and checked against it (tests/physics, docs/analysis/PHYSICS.md).
 *
 * Transitional layout: the physics state keeps the game's own memory layout
 * and its 32-bit addresses, resolved through gt4_phys_mem. That lets each
 * rewritten function be swapped into the original step and compared bit for
 * bit. Native pointers come once the whole step is rewritten.
 *
 * Arithmetic: the PS2 FPU rounds toward zero and has no denormals, infinities
 * or NaNs. The code is plain float C and expects the caller to have set the
 * host rounding to toward-zero (gt4_phys_fpu_enter); constants are written as
 * the exact bit patterns the game uses. A multiply-add on the PS2 rounds the
 * product and then the sum, which is what `acc + a * b` does in C without
 * contraction.
 */
#ifndef OPENGT4_PHYS_H
#define OPENGT4_PHYS_H

#include <stdint.h>
#include <string.h>

/* Base of the memory that holds the physics state (the guest RAM image). */
extern uint8_t *gt4_phys_mem;

static inline void *gt4_ptr(uint32_t addr) { return gt4_phys_mem + (addr & 0x01ffffffu); }
static inline float gt4_f32(uint32_t bits) { float f; memcpy(&f, &bits, 4); return f; }

/* c ? a : b, chosen on the bits. MSVC turns float selects such as
   `x <= 0 ? 0 : x` into maxss/minss even with /fp:strict and then returns
   the wrong zero on a tie (-0 against 0); the PS2 code keeps the sign, so
   clamps against zero go through this. */
static inline float gt4_select(int c, float a, float b)
{
    uint32_t x, y, m = 0u - (uint32_t)(c != 0);
    memcpy(&x, &a, 4);
    memcpy(&y, &b, 4);
    x = (x & m) | (y & ~m);
    memcpy(&a, &x, 4);
    return a;
}

/* Typed access to the state by address. */
#define GT4_F32(a) (*(float *)gt4_ptr(a))
#define GT4_U32(a) (*(uint32_t *)gt4_ptr(a))
#define GT4_U16(a) (*(uint16_t *)gt4_ptr(a))
#define GT4_U8(a)  (*(uint8_t *)gt4_ptr(a))

void gt4_phys_fpu_enter(void);

/* Transitional: runs an original function that has no rewrite yet (the test
   harness runs the recompiled code). Arguments a0, a1, f12, f13; returns f0,
   and leaves v0 in gt4_orig_v0. Also used for branches of a rewritten
   function that no snapshot exercises yet, so nothing untested replaces the
   original. */
extern float (*gt4_orig_call)(uint32_t addr, uint32_t a0, uint32_t a1, float f12, float f13);
extern uint32_t gt4_orig_v0;
/* a2/a3 for the next gt4_orig_call (reset to 0 by the call). */
extern uint32_t gt4_orig_a2, gt4_orig_a3, gt4_orig_t0, gt4_orig_t1, gt4_orig_t2, gt4_orig_t3;
extern float gt4_orig_f14, gt4_orig_f15;
/* Rewritten paths that no snapshot has exercised yet run the original
   unless this is set (for trying them out; the comparison cannot vouch for
   them). */
extern int gt4_phys_unverified;
/* Headless: the step leaves out what only feeds the presentation (replay
   recording, pad vibration and force feedback, the engine sound, the car
   the view follows); the physics is the same without it. */
extern int gt4_phys_headless;
#define GT4_ORIG(addr, a0) ((void)gt4_orig_call((addr), (a0), 0, 0.0f, 0.0f))

/* The race session's methods (session.c): the conductor's virtual table
   at +0x10140, entry `off`; the Time Trial's are rewritten, the rest run
   the original. v0 returned, f0 to *f0. gt4_session_orig: all original. */
uint32_t gt4_session(uint32_t cond, uint32_t off, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, float *f0);
/* the same with t2 and f12 */
uint32_t gt4_session_x(uint32_t cond, uint32_t off, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1,
                       uint32_t t2, float f12, float *f0);
extern int gt4_session_orig;
/* The Time Trial's ghost, the presentation's part of its start and lap
   events for the player's car (func_003F59F8 / func_003F5B80 through the
   world's ghost objects, vt+0x240): lap 0 at the first crossing, else the
   lap just ended and its time; rest (50 - the crossing's fiftieths) and
   the crossing's fraction of the step as the events receive them. */
typedef void (*gt4_tt_ghost_fn)(void *ctx, uint32_t cond, uint32_t lap, uint32_t lap_time, uint32_t rest, float frac);
extern gt4_tt_ghost_fn gt4_tt_ghost;
extern void *gt4_tt_ghost_ctx;

/* The race worlds' methods that only return 0 (func_00389DF0,
   func_00389DF8, func_0038A3B0) */
static inline int gt4_returns_zero(uint32_t fn)
{
    return fn == 0x00389df0u || fn == 0x00389df8u || fn == 0x0038a3b0u;
}

/* Transitional: a virtual call through the table at [obj + table], entry
   `off` (s16 this adjustment, function address); returns v0. */
static inline uint32_t gt4_vcall(uint32_t obj, uint32_t table, uint32_t off, uint32_t a1, uint32_t a2, uint32_t a3)
{
    uint32_t e;
    int16_t adj;
    if (table == 0x10140u) return gt4_session(obj, off, a1, a2, a3, 0, 0, NULL);
    e = *(uint32_t *)gt4_ptr(obj + table) + off;
    if (gt4_returns_zero(*(uint32_t *)gt4_ptr(e + 4))) return 0;
    memcpy(&adj, gt4_ptr(e), 2);
    gt4_orig_a2 = a2;
    gt4_orig_a3 = a3;
    gt4_orig_call(*(uint32_t *)gt4_ptr(e + 4), obj + (uint32_t)(int32_t)adj, a1, 0.0f, 0.0f);
    return gt4_orig_v0;
}

/* Piecewise linear table: n points (x ascending), clamped at both ends.
   `x` and `y` hold addresses of the arrays (in the engine they point to the
   inline arrays that follow). */
typedef struct {
    int16_t  n;
    int16_t  pad;
    uint32_t x, y;
} gt4_curve;

float gt4_curve_eval(const gt4_curve *c, float x);
void  gt4_curve_eval2(const gt4_curve *a, const gt4_curve *b, float x, float *ya, float *yb);

/* The game's own float math (polynomials, not the C library's). */
float gt4_atanf(float x);            /* func_0057D008 */
float gt4_atan2f(float y, float x);  /* func_0057D118 */
float gt4_sinf(float x);             /* func_0057D1F0 */
float gt4_cosf(float x);             /* func_0057D2B8 */
float gt4_tanf(float x);             /* func_0057D380 */
float gt4_angle_wrap(float x);       /* func_0036CB90: to [-pi, pi) */
float gt4_angle_diff(float a, float b); /* func_0036CC40 */
void  gt4_sincosf(float x, float *s, float *c); /* func_0057D8A0 */

#endif
