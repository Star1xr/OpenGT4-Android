/*
 * EE memory map, MMIO and run control.
 *
 * Mapped: RDRAM (32 MB) through kuseg, kseg0, kseg1 and the 0x2/0x3 uncached
 * mirrors; scratchpad at 0x70000000; the four EE timers as MMIO. Any other
 * address ends the run with EE_END_BAD_ADDRESS or EE_END_UNSUPPORTED, so an
 * unhandled access is reported instead of silently returning a value.
 */
#include "ee.h"
#include "thread.h"
#include "fpu.h"
#include "sif.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* ---- run control and trace ---- */

void ee_trace(ee_ctx *c, const char *fmt, ...)
{
    va_list ap;
    if (!c->trace)
        return;
    fprintf(c->trace, "{\"seq\":%llu,", (unsigned long long)c->trace_seq++);
    va_start(ap, fmt);
    vfprintf(c->trace, fmt, ap);
    va_end(ap);
    fputs("}\n", c->trace);
}

void ee_end(ee_ctx *c, int reason, uint32_t pc, const char *fmt, ...)
{
    va_list ap;
    c->end_reason = reason;
    c->end_pc = pc;
    va_start(ap, fmt);
    vsnprintf(c->end_detail, sizeof c->end_detail, fmt, ap);
    va_end(ap);
    ee_trace(c, "\"ev\":\"end\",\"reason\":%d,\"pc\":\"0x%08x\",\"detail\":\"%s\"",
             reason, pc, c->end_detail);
    if (ee_sched_active(c))
        ee_sched_exit_to_host(c);         /* never returns */
    if (c->exit_jmp)
        longjmp(*c->exit_jmp, 1);
    abort();
}

void ee_unrecompiled(ee_ctx *c, uint32_t target, uint32_t from_pc)
{
    ee_end(c, EE_END_UNRECOMPILED, from_pc, "call to 0x%08x, not recompiled", target);
}

void ee_unsupported(ee_ctx *c, uint32_t pc, const char *what)
{
    ee_end(c, EE_END_UNSUPPORTED, pc, "%s", what);
}

int ee_init(ee_ctx *c)
{
    memset(c, 0, sizeof *c);
    ee_fpu_enter();                       /* PS2 FPU rounding (toward zero) */
    c->ram = calloc(1, EE_RAM_SIZE);
    c->spr = calloc(1, EE_SPR_SIZE);
    return (c->ram && c->spr) ? 0 : -1;
}

void ee_free(ee_ctx *c)
{
    ee_sif_free(c);
    free(c->ram);
    free(c->spr);
    c->ram = c->spr = NULL;
    ee_fpu_leave();
}

long ee_load_image(ee_ctx *c, const char *path, uint32_t vaddr)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0 || (uint64_t)vaddr + (uint64_t)n > EE_RAM_SIZE
        || fread(c->ram + vaddr, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        return -2;
    }
    fclose(f);
    return n;
}

/* ---- timers (MMIO) ----
 * EE timers 0-3 at 0x10000000 + n*0x800: COUNT +0x00, MODE +0x10, COMP +0x20,
 * HOLD +0x30 (timers 0 and 1 only).
 * Stubbed: writes are stored and read back. COUNT does not run with time; it
 * advances by 1 on each read, so runs are reproducible. Mode flag bits are
 * not cleared by writing 1. Interrupts are never raised.
 */
static int timer_decode(uint32_t phys, int *n, int *reg)
{
    uint32_t off;
    if (phys < 0x10000000u || phys >= 0x10002000u)
        return 0;
    off = phys - 0x10000000u;
    *n = (int)(off >> 11);
    if ((off & 0x7ff) > 0x30 || (off & 0xf))
        return 0;
    *reg = (int)((off & 0x7ff) >> 4);
    return 1;
}

static const char *timer_reg_name[4] = { "COUNT", "MODE", "COMP", "HOLD" };

static uint32_t mmio_read(ee_ctx *c, uint32_t vaddr, uint32_t phys, int size)
{
    int n, reg;
    uint32_t v;
    if (ee_sif_mmio_read(c, phys, &v)) return v;
    if (timer_decode(phys, &n, &reg)) {
        v = c->timer[n][reg];
        if (reg == 0)
            c->timer[n][0] = (c->timer[n][0] + 1) & 0xffff;
        ee_trace(c, "\"ev\":\"mmio\",\"rw\":\"r\",\"addr\":\"0x%08x\",\"size\":%d,"
                    "\"val\":\"0x%08x\",\"dev\":\"T%d_%s\",\"status\":\"%s\"",
                 vaddr, size, v, n, timer_reg_name[reg], EE_STUBBED);
        return v;
    }
    ee_end(c, EE_END_UNSUPPORTED, 0, "MMIO read %d at 0x%08x", size, vaddr);
    return 0;
}

static void mmio_write(ee_ctx *c, uint32_t vaddr, uint32_t phys, int size, uint64_t v)
{
    int n, reg;
    if (ee_sif_mmio_write(c, phys, (uint32_t)v)) return;
    if (timer_decode(phys, &n, &reg)) {
        c->timer[n][reg] = (uint32_t)v & 0xffff;
        ee_trace(c, "\"ev\":\"mmio\",\"rw\":\"w\",\"addr\":\"0x%08x\",\"size\":%d,"
                    "\"val\":\"0x%08llx\",\"dev\":\"T%d_%s\",\"status\":\"%s\"",
                 vaddr, size, (unsigned long long)v, n, timer_reg_name[reg], EE_STUBBED);
        return;
    }
    ee_end(c, EE_END_UNSUPPORTED, 0, "MMIO write %d at 0x%08x", size, vaddr);
}

/* ---- address translation ---- */

enum { MAP_RAM, MAP_SPR, MAP_IO };

static uint8_t *map(ee_ctx *c, uint32_t a, int size, int *kind, uint32_t *phys)
{
    uint32_t p;
    if (a >= 0x70000000u && a < 0x70000000u + EE_SPR_SIZE) {
        *kind = MAP_SPR;
        return c->spr + (a - 0x70000000u);
    }
    if ((a >> 28) == 2 || (a >> 28) == 3)
        p = a & 0x01ffffffu;           /* uncached / uncached-accelerated mirrors */
    else if (a >= 0x80000000u && a < 0xc0000000u)
        p = a & 0x1fffffffu;           /* kseg0 / kseg1 */
    else if (a < 0x20000000u)
        p = a;                          /* kuseg as mapped by the kernel */
    else {
        ee_end(c, EE_END_BAD_ADDRESS, 0, "access %d at 0x%08x", size, a);
        return NULL;
    }
    *phys = p;
    if (p + (uint32_t)size <= EE_RAM_SIZE) {
        *kind = MAP_RAM;
        return c->ram + p;
    }
    if (p >= 0x10000000u && p < 0x10010000u) {
        *kind = MAP_IO;
        return NULL;
    }
    ee_end(c, EE_END_BAD_ADDRESS, 0, "access %d at 0x%08x (phys 0x%08x)", size, a, p);
    return NULL;
}

#define DEF_READ(bits, type)                                              \
    type ee_r##bits(ee_ctx *c, uint32_t a)                                \
    {                                                                     \
        int kind; uint32_t phys = 0; type v;                              \
        uint8_t *p = map(c, a, bits / 8, &kind, &phys);                   \
        if (kind == MAP_IO)                                               \
            return (type)mmio_read(c, a, phys, bits / 8);                 \
        memcpy(&v, p, sizeof v);                                          \
        return v;                                                         \
    }

#define DEF_WRITE(bits, type)                                             \
    void ee_w##bits(ee_ctx *c, uint32_t a, type v)                        \
    {                                                                     \
        int kind; uint32_t phys = 0;                                      \
        uint8_t *p = map(c, a, bits / 8, &kind, &phys);                   \
        if (kind == MAP_IO) {                                             \
            mmio_write(c, a, phys, bits / 8, (uint64_t)v);                \
            return;                                                       \
        }                                                                 \
        memcpy(p, &v, sizeof v);                                          \
    }

DEF_READ(8, uint8_t)
DEF_READ(16, uint16_t)
DEF_READ(32, uint32_t)
DEF_READ(64, uint64_t)
DEF_WRITE(8, uint8_t)
DEF_WRITE(16, uint16_t)
DEF_WRITE(32, uint32_t)
DEF_WRITE(64, uint64_t)

ee_u128 ee_r128(ee_ctx *c, uint32_t a)
{
    int kind; uint32_t phys = 0; ee_u128 v;
    uint8_t *p = map(c, a & ~15u, 16, &kind, &phys);
    if (kind == MAP_IO)
        ee_end(c, EE_END_UNSUPPORTED, 0, "128-bit MMIO read at 0x%08x", a);
    memcpy(&v, p, sizeof v);
    return v;
}

void ee_w128(ee_ctx *c, uint32_t a, ee_u128 v)
{
    int kind; uint32_t phys = 0;
    uint8_t *p = map(c, a & ~15u, 16, &kind, &phys);
    if (kind == MAP_IO)
        ee_end(c, EE_END_UNSUPPORTED, 0, "128-bit MMIO write at 0x%08x", a);
    memcpy(p, &v, sizeof v);
}

/* ---- copied code check ---- */

uint64_t ee_fnv1a(const uint8_t *p, size_t n)
{
    uint64_t h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < n; i++)
        h = (h ^ p[i]) * 0x100000001b3ull;
    return h;
}

void ee_check_code(ee_ctx *c, uint32_t target, uint32_t pc)
{
    for (int i = 0; i < c->n_code_ranges; i++) {
        const ee_code_range *r = &c->code_ranges[i];
        uint8_t buf[4096];
        if (target < r->addr || target >= r->addr + r->size)
            continue;
        if (r->size > sizeof buf)
            ee_end(c, EE_END_UNSUPPORTED, pc, "code range 0x%08x too large to check", r->addr);
        for (uint32_t k = 0; k < r->size; k++)
            buf[k] = ee_r8(c, r->addr + k);
        if (ee_fnv1a(buf, r->size) != r->hash)
            ee_end(c, EE_END_CODE_MISMATCH, pc,
                   "code at 0x%08x (0x%x bytes) differs from the bytes it was recompiled from",
                   r->addr, r->size);
        ee_trace(c, "\"ev\":\"code_check\",\"pc\":\"0x%08x\",\"target\":\"0x%08x\","
                    "\"range\":\"0x%08x\",\"size\":%u,\"result\":\"match\"",
                 pc, target, r->addr, r->size);
        return;
    }
}

/* ---- COP0 interrupt enable ----
 * Simplified EIE gating. With SIF enabled, pending channel-5 replies can run
 * at an interrupt point. INTC/timer interrupts are still not scheduled. */

void ee_call(ee_ctx *c, uint32_t target, uint32_t pc)
{
    ee_fn fn = c->lookup ? c->lookup(target) : NULL;
    if (!fn) ee_unrecompiled(c, target, pc);
    ee_check_code(c, target, pc);
    fn(c);
}

void ee_di(ee_ctx *c)
{
    c->cop0[EE_COP0_STATUS] &= ~EE_STATUS_EIE;
    ee_trace(c, "\"ev\":\"cop0\",\"op\":\"di\",\"status_reg\":\"0x%08x\",\"status\":\"%s\"",
             c->cop0[EE_COP0_STATUS], EE_STUBBED);
}

void ee_ei(ee_ctx *c)
{
    c->cop0[EE_COP0_STATUS] |= EE_STATUS_EIE;
    ee_sif_interrupt_point(c);
    ee_trace(c, "\"ev\":\"cop0\",\"op\":\"ei\",\"status_reg\":\"0x%08x\",\"status\":\"%s\"",
             c->cop0[EE_COP0_STATUS], EE_STUBBED);
}

/* ---- unaligned loads and stores (little-endian semantics) ----
 * LDL/LDR and LWL/LWR merge part of an aligned word into the old register
 * value; SDL/SDR and SWL/SWR write part of the register into memory. */

uint64_t ee_ldl(ee_ctx *c, uint32_t a, uint64_t old)
{
    unsigned sh = (7 - (a & 7)) * 8;
    uint64_t m = ee_r64(c, a & ~7u);
    return sh ? (m << sh) | (old & ((1ull << sh) - 1)) : m;
}

uint64_t ee_ldr(ee_ctx *c, uint32_t a, uint64_t old)
{
    unsigned sh = (a & 7) * 8;
    uint64_t m = ee_r64(c, a & ~7u);
    return sh ? (m >> sh) | (old & ~(~0ull >> sh)) : m;
}

void ee_sdl(ee_ctx *c, uint32_t a, uint64_t v)
{
    unsigned sh = (7 - (a & 7)) * 8;
    uint64_t m = ee_r64(c, a & ~7u);
    ee_w64(c, a & ~7u, sh ? (m & ~(~0ull >> sh)) | (v >> sh) : v);
}

void ee_sdr(ee_ctx *c, uint32_t a, uint64_t v)
{
    unsigned sh = (a & 7) * 8;
    uint64_t m = ee_r64(c, a & ~7u);
    ee_w64(c, a & ~7u, sh ? (m & ((1ull << sh) - 1)) | (v << sh) : v);
}

uint64_t ee_lwl(ee_ctx *c, uint32_t a, uint64_t old)
{
    unsigned sh = (3 - (a & 3)) * 8;
    uint32_t m = ee_r32(c, a & ~3u);
    uint32_t r = sh ? (m << sh) | ((uint32_t)old & ((1u << sh) - 1)) : m;
    return (uint64_t)(int64_t)(int32_t)r;
}

uint64_t ee_lwr(ee_ctx *c, uint32_t a, uint64_t old)
{
    unsigned sh = (a & 3) * 8;
    uint32_t m = ee_r32(c, a & ~3u);
    uint32_t r = sh ? (m >> sh) | ((uint32_t)old & ~(0xffffffffu >> sh)) : m;
    /* Sign-extends only when the whole word was loaded (offset 0). */
    return sh ? (old & 0xffffffff00000000ull) | r : (uint64_t)(int64_t)(int32_t)r;
}

void ee_swl(ee_ctx *c, uint32_t a, uint32_t v)
{
    unsigned sh = (3 - (a & 3)) * 8;
    uint32_t m = ee_r32(c, a & ~3u);
    ee_w32(c, a & ~3u, sh ? (m & ~(0xffffffffu >> sh)) | (v >> sh) : v);
}

void ee_swr(ee_ctx *c, uint32_t a, uint32_t v)
{
    unsigned sh = (a & 3) * 8;
    uint32_t m = ee_r32(c, a & ~3u);
    ee_w32(c, a & ~3u, sh ? (m & ((1u << sh) - 1)) | (v << sh) : v);
}

/* ---- ERET ----
 * Implemented for the one use seen so far: code that raises ERL (or EXL) to
 * run a few instructions in kernel mode, puts its return address in ErrorEPC
 * (or EPC) and executes ERET to drop back. That only works as a plain return
 * here, so the target must equal the caller's return address ($ra); any other
 * ERET ends the run. */

void ee_eret(ee_ctx *c, uint32_t pc)
{
    uint32_t st = c->cop0[EE_COP0_STATUS];
    uint32_t target;
    const char *via;
    if (st & EE_STATUS_ERL) {
        target = c->cop0[EE_COP0_ERROREPC];
        c->cop0[EE_COP0_STATUS] = st & ~EE_STATUS_ERL;
        via = "ErrorEPC";
    } else {
        target = c->cop0[EE_COP0_EPC];
        c->cop0[EE_COP0_STATUS] = st & ~EE_STATUS_EXL;
        via = "EPC";
    }
    ee_trace(c, "\"ev\":\"cop0\",\"op\":\"eret\",\"pc\":\"0x%08x\",\"via\":\"%s\",\"target\":\"0x%08x\","
                "\"status_reg\":\"0x%08x\",\"status\":\"%s\"",
             pc, via, target, c->cop0[EE_COP0_STATUS], EE_IMPLEMENTED);
    if (target != c->r[31].uw[0])
        ee_end(c, EE_END_UNSUPPORTED, pc, "eret to 0x%08x, not the return address 0x%08x",
               target, c->r[31].uw[0]);
}

/* Only the data-cache maintenance operation used by SIF is modeled here. */
void ee_cache(ee_ctx *c, unsigned op, uint32_t addr, uint32_t pc)
{
    if (op != 0x18) ee_end(c, EE_END_UNSUPPORTED, pc, "cache op 0x%x not modeled", op);
    ee_trace(c, "\"ev\":\"cache\",\"op\":%u,\"addr\":%u,\"status\":\"implemented\","
                "\"note\":\"no data cache: writes already visible to DMA\"", op, addr);
}
