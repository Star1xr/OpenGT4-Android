/*
 * EE FPU semantics, no game data. Expected values are worked out by hand from
 * the PS2 rules in src/ee/fpu.h. Several cases are chosen so that IEEE
 * round-to-nearest (the host default) would give a different answer.
 */
#include "ee.h"
#include "fpu.h"

#include <stdio.h>

static int fails;
#define EXPECT(cond, ...)                                  \
    do {                                                   \
        if (!(cond)) {                                     \
            fails++;                                       \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);    \
            printf(__VA_ARGS__);                           \
            printf("\n");                                  \
        }                                                  \
    } while (0)

#define ONE   0x3f800000u
#define TWO   0x40000000u
#define THREE 0x40400000u
#define FMAX  0x7f7fffffu
#define NFMAX 0xff7fffffu

int main(void)
{
    static ee_ctx c;
    uint32_t r;
    if (ee_init(&c) != 0)
        return 2;

    /* Plain arithmetic. */
    EXPECT(ee_fpu_add(&c, ONE, TWO) == THREE, "1+2");
    EXPECT(ee_fpu_mul(&c, TWO, THREE) == 0x40c00000u, "2*3");
    EXPECT(ee_fpu_sub(&c, ONE, TWO) == 0xbf800000u, "1-2");

    /* Rounding toward zero. 1 + 0.75 ulp: nearest gives 0x3f800001, PS2 0x3f800000. */
    r = ee_fpu_add(&c, ONE, 0x33c00000u);
    EXPECT(r == ONE, "1 + 0.75ulp rounds toward zero, got %08x", r);
    /* 2^24 + 3 does not fit: nearest gives 16777220, toward zero 16777218. */
    r = ee_fpu_cvt_s_w(&c, 16777219u);
    EXPECT(r == 0x4b800001u, "cvt.s.w 16777219, got %08x", r);

    /* No infinities: Inf input reads as FLT_MAX, overflow clamps and sets O. */
    c.fcr31 = 0;
    EXPECT(ee_fpu_add(&c, 0x7f800000u, 0) == FMAX, "Inf input reads as FLT_MAX");
    EXPECT(!(c.fcr31 & EE_FPU_O), "FLT_MAX + 0 is not an overflow");
    r = ee_fpu_mul(&c, FMAX, TWO);
    EXPECT(r == FMAX && (c.fcr31 & EE_FPU_O) && (c.fcr31 & EE_FPU_SO), "overflow, got %08x fcr %08x", r, c.fcr31);
    r = ee_fpu_mul(&c, NFMAX, TWO);
    EXPECT(r == NFMAX, "negative overflow, got %08x", r);
    /* O is cleared by the next operation, SO stays. */
    ee_fpu_add(&c, ONE, ONE);
    EXPECT(!(c.fcr31 & EE_FPU_O) && (c.fcr31 & EE_FPU_SO), "O cleared, SO sticky");

    /* Denormals: input reads as zero; a denormal result becomes zero with U. */
    EXPECT(ee_fpu_add(&c, 0x00000001u, 0) == 0, "denormal input is zero");
    c.fcr31 = 0;
    r = ee_fpu_mul(&c, 0x00800000u, 0x3f000000u);    /* 2^-126 * 0.5 */
    EXPECT(r == 0 && (c.fcr31 & EE_FPU_U) && (c.fcr31 & EE_FPU_SU), "underflow, got %08x", r);

    /* Division. */
    EXPECT(ee_fpu_div(&c, 0x40c00000u, TWO) == THREE, "6/2");
    c.fcr31 = 0;
    r = ee_fpu_div(&c, ONE, 0);
    EXPECT(r == FMAX && (c.fcr31 & EE_FPU_D), "1/0 gives FLT_MAX with D, got %08x", r);
    r = ee_fpu_div(&c, 0xbf800000u, 0);
    EXPECT(r == NFMAX, "-1/0 gives -FLT_MAX, got %08x", r);
    c.fcr31 = 0;
    ee_fpu_div(&c, 0, 0);
    EXPECT((c.fcr31 & EE_FPU_I) && !(c.fcr31 & EE_FPU_D), "0/0 sets I, not D");

    /* Square roots. */
    EXPECT(ee_fpu_sqrt(&c, 0x40800000u) == TWO, "sqrt 4");
    c.fcr31 = 0;
    r = ee_fpu_sqrt(&c, 0xc0800000u);
    EXPECT(r == TWO && (c.fcr31 & EE_FPU_I), "sqrt -4 = 2 with I, got %08x", r);
    EXPECT(ee_fpu_rsqrt(&c, 0x40c00000u, 0x40800000u) == THREE, "6/sqrt(4)");

    /* Float to int: toward zero, saturating. */
    EXPECT(ee_fpu_cvt_w_s(&c, 0x4079999au) == 3, "3.9 -> 3");
    EXPECT(ee_fpu_cvt_w_s(&c, 0xc079999au) == (uint32_t)-3, "-3.9 -> -3");
    EXPECT(ee_fpu_cvt_w_s(&c, 0x4f32d05eu) == 0x7fffffffu, "3e9 saturates");
    EXPECT(ee_fpu_cvt_w_s(&c, 0xcf32d05eu) == 0x80000000u, "-3e9 saturates");
    EXPECT(ee_fpu_cvt_w_s(&c, 0x7f800000u) == 0x7fffffffu, "Inf pattern saturates");

    /* Sign operations and min/max. */
    EXPECT(ee_fpu_neg(&c, ONE) == 0xbf800000u && ee_fpu_abs(&c, 0xbf800000u) == ONE, "neg/abs");
    EXPECT(ee_fpu_max(&c, ONE, TWO) == TWO && ee_fpu_min(&c, ONE, TWO) == ONE, "max/min");

    /* Accumulator: ACC = 1; ACC + 2*3 = 7; ACC - 2*3 = -5. */
    ee_fpu_mula(&c, ONE, ONE);
    EXPECT(ee_fpu_madd(&c, TWO, THREE) == 0x40e00000u, "madd 1+2*3");
    EXPECT(ee_fpu_msub(&c, TWO, THREE) == 0xc0a00000u, "msub 1-2*3");
    ee_fpu_madda(&c, TWO, THREE);
    EXPECT(ee_fpu_add(&c, 0, 0) == 0 && c.acc == 7.0f, "madda leaves ACC = 7");

    /* Compares set C. */
    ee_fpu_c_lt(&c, ONE, TWO);
    EXPECT(c.fcr31 & EE_FPU_C, "1 < 2");
    ee_fpu_c_le(&c, TWO, ONE);
    EXPECT(!(c.fcr31 & EE_FPU_C), "not 2 <= 1");
    ee_fpu_c_eq(&c, 0x00000001u, 0);
    EXPECT(c.fcr31 & EE_FPU_C, "denormal equals zero");
    ee_fpu_c_f(&c);
    EXPECT(!(c.fcr31 & EE_FPU_C), "c.f clears C");

    ee_free(&c);
    printf(fails ? "%d failure(s)\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
