/*
 * EE MMI semantics, no game data. Expected values worked out by hand from
 * the EE Core Instruction Set manual rules implemented in src/ee/mmi.c.
 */
#include "ee.h"
#include "mmi.h"

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

static void set(ee_ctx *c, int r, uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3)
{
    c->r[r].uw[0] = w0; c->r[r].uw[1] = w1; c->r[r].uw[2] = w2; c->r[r].uw[3] = w3;
}

static int is(const ee_ctx *c, int r, uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3)
{
    return c->r[r].uw[0] == w0 && c->r[r].uw[1] == w1 && c->r[r].uw[2] == w2 && c->r[r].uw[3] == w3;
}

#define SHOW(n) c.r[n].uw[0], c.r[n].uw[1], c.r[n].uw[2], c.r[n].uw[3]

int main(void)
{
    static ee_ctx c;
    if (ee_init(&c) != 0)
        return 2;

    /* pceqb: bytes equal -> 0xff. "abc\0" against zero finds the terminator. */
    set(&c, 1, 0x00636261u, 0x11111111u, 0, 0x00ff00ffu);
    set(&c, 2, 0, 0, 0, 0);
    ee_mmi_pceqb(&c, 3, 1, 2);
    EXPECT(is(&c, 3, 0xff000000u, 0, 0xffffffffu, 0xff00ff00u), "pceqb %08x %08x %08x %08x", SHOW(3));

    /* Interleave: pextlw rd = {rt.w0, rs.w0, rt.w1, rs.w1}. */
    set(&c, 1, 0xa0, 0xa1, 0xa2, 0xa3);
    set(&c, 2, 0xb0, 0xb1, 0xb2, 0xb3);
    ee_mmi_pextlw(&c, 3, 1, 2);
    EXPECT(is(&c, 3, 0xb0, 0xa0, 0xb1, 0xa1), "pextlw");
    ee_mmi_pextuw(&c, 3, 1, 2);
    EXPECT(is(&c, 3, 0xb2, 0xa2, 0xb3, 0xa3), "pextuw");
    /* rd may equal an operand. */
    ee_mmi_pextlw(&c, 1, 1, 2);
    EXPECT(is(&c, 1, 0xb0, 0xa0, 0xb1, 0xa1), "pextlw with rd == rs");

    /* ppacb: even bytes of rt to the low half, of rs to the high half. */
    set(&c, 1, 0x33221100u, 0x77665544u, 0xbbaa9988u, 0xffeeddccu);
    set(&c, 2, 0x03020100u, 0x07060504u, 0x0b0a0908u, 0x0f0e0d0cu);
    ee_mmi_ppacb(&c, 3, 1, 2);
    EXPECT(is(&c, 3, 0x06040200u, 0x0e0c0a08u, 0x66442200u, 0xeeccaa88u), "ppacb %08x %08x %08x %08x", SHOW(3));

    /* Saturation. */
    set(&c, 1, 0x7fffffffu, 0x80000000u, 5, 0);
    set(&c, 2, 1, 0xffffffffu, 3, 0);
    ee_mmi_paddsw(&c, 3, 1, 2);
    EXPECT(is(&c, 3, 0x7fffffffu, 0x80000000u, 8, 0), "paddsw");
    set(&c, 1, 0x0010ffffu, 0, 0, 0);
    set(&c, 2, 0x00200002u, 0, 0, 0);
    ee_mmi_padduh(&c, 3, 1, 2);
    EXPECT(c.r[3].uw[0] == 0x0030ffffu, "padduh %08x", c.r[3].uw[0]);

    /* Shifts. */
    set(&c, 2, 0x80000000u, 0x00000010u, 0xfffffff0u, 1);
    ee_mmi_psraw(&c, 3, 2, 4);
    EXPECT(is(&c, 3, 0xf8000000u, 0x00000001u, 0xffffffffu, 0), "psraw");
    ee_mmi_psrlh(&c, 3, 2, 4);
    EXPECT(c.r[3].uw[0] == 0x08000000u && c.r[3].uw[2] == 0x0fff0fffu, "psrlh %08x %08x", c.r[3].uw[0], c.r[3].uw[2]);

    /* qfsrv with SA set by mtsab (bytes): (rs:rt) >> 4 bytes. */
    set(&c, 1, 0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u);   /* rs, upper */
    set(&c, 2, 0xaaaaaaaau, 0xbbbbbbbbu, 0xccccccccu, 0xddddddddu);   /* rt, lower */
    set(&c, 4, 4, 0, 0, 0);
    ee_mmi_mtsab(&c, 4, 0);
    EXPECT(c.sa == 4, "mtsab gives SA = 4 bytes, got %u", c.sa);
    ee_mmi_qfsrv(&c, 3, 1, 2);
    EXPECT(is(&c, 3, 0xbbbbbbbbu, 0xccccccccu, 0xddddddddu, 0x11111111u), "qfsrv 4 bytes");
    set(&c, 4, 3, 0, 0, 0);
    ee_mmi_mtsah(&c, 4, 0);
    EXPECT(c.sa == 6, "mtsah 3 halfwords = 6 bytes, got %u", c.sa);

    /* pmulth: products 0..7; rd = {p0, p2, p4, p6}; LO = {p0, p1, p4, p5}; HI = {p2, p3, p6, p7}. */
    set(&c, 1, 0x00020001u, 0x00040003u, 0x00060005u, 0xffff0007u);   /* 1..7, -1 */
    set(&c, 2, 0x00020002u, 0x00020002u, 0x00020002u, 0x00020002u);   /* all 2 */
    ee_mmi_pmulth(&c, 3, 1, 2);
    EXPECT(is(&c, 3, 2, 6, 10, 14), "pmulth rd %u %u %u %u", SHOW(3));
    EXPECT((uint32_t)c.lo == 2 && (uint32_t)(c.lo >> 32) == 4 && (uint32_t)c.lo1 == 10 && (uint32_t)(c.lo1 >> 32) == 12, "pmulth LO");
    EXPECT((uint32_t)c.hi == 6 && (uint32_t)(c.hi >> 32) == 8 && (uint32_t)c.hi1 == 14 && (uint32_t)(c.hi1 >> 32) == 0xfffffffeu, "pmulth HI");
    ee_mmi_pmfhl_lw(&c, 5);
    EXPECT(is(&c, 5, 2, 6, 10, 14), "pmfhl.lw = {LO.w0, HI.w0, LO.w2, HI.w2}");

    /* madd: HI:LO += rs * rt (signed 32 x 32). */
    c.hi = 0; c.lo = 10;
    set(&c, 1, (uint32_t)-3, 0, 0, 0);
    set(&c, 2, 4, 0, 0, 0);
    ee_mmi_madd(&c, 3, 1, 2);
    EXPECT(c.lo == (uint64_t)(int64_t)-2 && c.hi == 0xffffffffffffffffull && c.r[3].sd[0] == -2, "madd 10 + -12");

    /* ppac5: 8:8:8:8 to 1:5:5:5. 0x80ff8040 -> A=1, B=0x1f, G=0x10, R=0x08. */
    set(&c, 2, 0x80ff8040u, 0, 0, 0);
    ee_mmi_ppac5(&c, 3, 2);
    EXPECT(c.r[3].uw[0] == 0xfe08u, "ppac5 %08x", c.r[3].uw[0]);

    /* r0 is never written. */
    set(&c, 1, 1, 1, 1, 1);
    ee_mmi_paddw(&c, 0, 1, 1);
    EXPECT(c.r[0].ud[0] == 0 && c.r[0].ud[1] == 0, "r0 stays zero");

    ee_free(&c);
    printf(fails ? "%d failure(s)\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
