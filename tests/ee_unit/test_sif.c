/* Synthetic tests: no disc, BIOS or generated game code. */
#include "sif.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static int interrupts;
static void fake_handler(ee_ctx *c)
{
    interrupts++;
    /* Interrupt must preserve CPU registers but retain guest-memory effects. */
    c->r[2].ud[0] = 0xBAD;
    c->hi = 0xBAD; c->fpr[0].u = 0xBAD;
    ee_w32(c, 0x3000, ee_r32(c, c->sif->ee_recv + 8));
    c->sif->chain = 1;
    ee_sif_interrupt_point(c); /* must not recurse */
}
static ee_fn lookup(uint32_t a) { return a == 0x4000 ? fake_handler : NULL; }
static void sys(ee_ctx *c, int n, uint32_t a, uint32_t b)
{
    c->r[3].sw[0] = n; c->r[4].uw[0] = a; c->r[5].uw[0] = b;
    ee_syscall(c, 0x1000);
}

int main(void)
{
    static ee_ctx c;
    jmp_buf j;
    CHECK(ee_init(&c) == 0);
    c.exit_jmp = &j;
    if (setjmp(j)) { printf("unexpected stop: %s\n", c.end_detail); return 1; }
    CHECK(ee_sif_init(&c, 1, (const uint8_t *)"0000A000000000", 0, 2) == 0);
    c.lookup = lookup;
    c.sif->ee_recv = 0x2000; c.sif->handler = 0x4000;
    c.sif->enabled = c.sif->chain = 1;
    /* A synthetic RPC init command, sent using a guest DMA descriptor. */
    ee_w32(&c, 0x100, 0x200); ee_w32(&c, 0x104, EE_SIF_IOP_CMD);
    ee_w32(&c, 0x108, 16); ee_w32(&c, 0x10c, 0x44);
    ee_w32(&c, 0x200, 16); ee_w32(&c, 0x208, 0x80000002); ee_w32(&c, 0x20c, 1);
    sys(&c, 0x77, 0x100, 1);
    CHECK(c.sif->count == 1 && interrupts == 0);
    CHECK(ee_r32(&c, 0x2000) == 0); /* queued, not written while interrupts disabled */
    c.r[2].ud[0] = 0x1234; c.hi = 42; c.fpr[0].u = 43;
    ee_ei(&c); CHECK(interrupts == 0);
    ee_sif_interrupt_point(&c); CHECK(interrupts == 0);
    ee_sif_interrupt_point(&c);
    CHECK(interrupts == 1 && c.sif->count == 0);
    CHECK(c.r[2].ud[0] == 0x1234 && c.hi == 42 && c.fpr[0].u == 43);
    CHECK(ee_r32(&c, 0x3000) == 0x80000001);
    /* Semaphore starts with init_count (offset 8), not count (offset 0). */
    ee_w32(&c, 0x500, 99); ee_w32(&c, 0x504, 2); ee_w32(&c, 0x508, 1);
    sys(&c, 0x40, 0x500, 0);
    CHECK(c.r[2].uw[0] == 1 && c.sema[1].count == 1);
    sys(&c, 0x44, 1, 0); CHECK(c.sema[1].count == 0);
    sys(&c, -0x43, 1, 0); CHECK(c.sema[1].count == 1);
    sys(&c, 0x44, 1, 0); CHECK(c.sema[1].count == 0);
    if (setjmp(j) == 0) { sys(&c, 0x44, 1, 0); CHECK(0); }
    CHECK(strstr(c.end_detail, "deadlock") != NULL);
    if (setjmp(j) == 0) { sys(&c, 0x42, 255, 0); CHECK(0); }
    CHECK(strstr(c.end_detail, "invalid semaphore") != NULL);
    if (setjmp(j) == 0) {
        ee_w32(&c, 0x104, EE_IOP_SIZE - 4); sys(&c, 0x77, 0x100, 1); CHECK(0);
    }
    CHECK(strstr(c.end_detail, "outside IOP RAM") != NULL);
    c.sif->dstat = 0x00200020;
    ee_w32(&c, 0x1000e010, 0x00200020);
    CHECK(ee_r32(&c, 0x1000e010) == 0);
    ee_free(&c);
    puts("PASSED SIF IRQ gating/delay/context, semaphore counts/deadlock, DMA bounds, D_STAT");
    return 0;
}
