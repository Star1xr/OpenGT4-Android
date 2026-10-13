#include "phys.h"

#include <fenv.h>
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE__)
#include <xmmintrin.h>
#endif

uint8_t *gt4_phys_mem;
float (*gt4_orig_call)(uint32_t addr, uint32_t a0, uint32_t a1, float f12, float f13);
uint32_t gt4_orig_v0;
uint32_t gt4_orig_a2, gt4_orig_a3, gt4_orig_t0, gt4_orig_t1, gt4_orig_t2, gt4_orig_t3;
float gt4_orig_f14, gt4_orig_f15;
int gt4_phys_unverified;
int gt4_phys_headless;

/* PS2 FPU behaviour on the host: rounding toward zero, and no denormals
   (results flushed to zero, denormal inputs read as zero), which matters
   for the tiny terms of the sin/cos polynomials. */
void gt4_phys_fpu_enter(void)
{
    fesetround(FE_TOWARDZERO);
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE__)
    _mm_setcsr(_mm_getcsr() | 0x8040u);   /* FTZ | DAZ */
#endif
}

/* func_00359568 and func_00359830. The first narrows the interval by
   bisection while it is 4 points or wider and then scans, the second only
   scans; with ascending x both find the first point above x, as here. An
   exact hit on a bisection midpoint returns that point's y, which the
   interpolation also gives. */
static float eval(int n, const float *xs, const float *ys, float x)
{
    int last = n - 1, i;

    if (x <= xs[0]) return ys[0];
    if (xs[last] <= x) return ys[last];
    for (i = 1; i < last && !(x < xs[i]); i++) {}
    return ys[i - 1] + (ys[i] - ys[i - 1]) * (x - xs[i - 1]) / (xs[i] - xs[i - 1]);
}

float gt4_curve_eval(const gt4_curve *c, float x)
{
    return eval(c->n, gt4_ptr(c->x), gt4_ptr(c->y), x);
}

/* func_003598F8: two tables read at the same x (the first one's points). */
void gt4_curve_eval2(const gt4_curve *a, const gt4_curve *b, float x, float *ya, float *yb)
{
    *ya = eval(a->n, gt4_ptr(a->x), gt4_ptr(a->y), x);
    *yb = eval(a->n, gt4_ptr(a->x), gt4_ptr(b->y), x);
}
