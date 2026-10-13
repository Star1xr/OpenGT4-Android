/*
 * Unit tests for EE runtime helpers that do not need game data.
 * Expected values are worked out by hand from the little-endian MIPS rules.
 */
#include "ee.h"

#include <stdio.h>
#include <string.h>

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

int main(void)
{
    static ee_ctx c;
    jmp_buf j;
    const uint32_t A = 0x1000;
    /* bytes 00 11 22 33 44 55 66 77 88 99 aa bb cc dd ee ff at A */
    const uint64_t lo = 0x7766554433221100ull, hi = 0xffeeddccbbaa9988ull;

    if (ee_init(&c) != 0)
        return 2;
    c.exit_jmp = &j;
    if (setjmp(j)) {
        printf("FAIL unexpected end: %s\n", c.end_detail);
        return 1;
    }
    memcpy(c.ram + A, &lo, 8);
    memcpy(c.ram + A + 8, &hi, 8);

    /* Unaligned 64-bit load at A+3 as the compiler emits it: ldl A+10, ldr A+3. */
    {
        uint64_t r = 0;
        r = ee_ldl(&c, A + 3 + 7, r);
        r = ee_ldr(&c, A + 3, r);
        EXPECT(r == 0xaa99887766554433ull, "ldl/ldr pair at +3 gave %016llx", (unsigned long long)r);
    }
    /* Aligned cases: each half alone gives the full dword. */
    EXPECT(ee_ldl(&c, A + 7, 0) == lo, "ldl at +7");
    EXPECT(ee_ldr(&c, A + 0, 0) == lo, "ldr at +0");

    /* Unaligned 32-bit load at A+1: lwl A+4, lwr A+1 -> 0x44332211, sign 0. */
    {
        uint64_t r = 0;
        r = ee_lwl(&c, A + 1 + 3, r);
        r = ee_lwr(&c, A + 1, r);
        EXPECT((uint32_t)r == 0x44332211u, "lwl/lwr pair at +1 gave %08x", (uint32_t)r);
    }
    /* lwl result is sign-extended: word at A+8..A+11 is 0xbbaa9988 (negative). */
    EXPECT(ee_lwl(&c, A + 11, 0) == 0xffffffffbbaa9988ull, "lwl sign extension");

    /* Unaligned 64-bit store at A+5: sdl A+12, sdr A+5. */
    {
        const uint64_t v = 0x0123456789abcdefull;
        uint8_t expect[16];
        memcpy(expect, &lo, 8);
        memcpy(expect + 8, &hi, 8);
        memcpy(expect + 5, &v, 8);
        ee_sdl(&c, A + 5 + 7, v);
        ee_sdr(&c, A + 5, v);
        EXPECT(memcmp(c.ram + A, expect, 16) == 0, "sdl/sdr pair at +5");
    }
    /* Unaligned 32-bit store at A+2: swl A+5, swr A+2. */
    {
        uint8_t before[8], expect[8];
        const uint32_t v = 0xdeadbeefu;
        memcpy(before, c.ram + A, 8);
        memcpy(expect, before, 8);
        memcpy(expect + 2, &v, 4);
        ee_swl(&c, A + 2 + 3, v);
        ee_swr(&c, A + 2, v);
        EXPECT(memcmp(c.ram + A, expect, 8) == 0, "swl/swr pair at +2");
    }

    /* Address mirrors reach the same RAM. */
    ee_w32(&c, 0x00002000, 0x12345678u);
    EXPECT(ee_r32(&c, 0x80002000) == 0x12345678u, "kseg0 mirror");
    EXPECT(ee_r32(&c, 0xa0002000) == 0x12345678u, "kseg1 mirror");
    EXPECT(ee_r32(&c, 0x20002000) == 0x12345678u, "0x2 uncached mirror");

    /* Copied-code check: matching bytes pass, one changed byte ends the run. */
    {
        static const uint8_t code[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
        ee_code_range r = { 0x00003000u, 8, 0 };
        volatile int ended = 0;
        r.hash = ee_fnv1a(code, sizeof code);
        memcpy(c.ram + 0x3000, code, sizeof code);
        c.code_ranges = &r;
        c.n_code_ranges = 1;
        ee_check_code(&c, 0x80003004u, 0);   /* kseg0 alias of the range start: not in range */
        ee_check_code(&c, 0x00003004u, 0);   /* in range, bytes match: returns */
        c.ram[0x3005] ^= 0xff;
        if (setjmp(j) == 0)
            ee_check_code(&c, 0x00003000u, 0);
        else
            ended = 1;
        EXPECT(ended && c.end_reason == EE_END_CODE_MISMATCH, "changed copied code must end the run");
        c.code_ranges = NULL;
        c.n_code_ranges = 0;
    }

    ee_free(&c);
    printf(fails ? "%d failure(s)\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
