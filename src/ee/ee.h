/*
 * EE runtime for statically recompiled code.
 *
 * Recompiled functions (generated from the user's own disc, never committed)
 * take an ee_ctx and use the helpers here for memory, kernel calls and
 * special instructions. Everything that stands in for hardware or the PS2
 * kernel records what it did in the trace, and says whether its behaviour is
 * implemented or only stubbed. See docs/analysis/EXECUTION_START.md.
 */
#ifndef OPENGT4_EE_H
#define OPENGT4_EE_H

#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef union {
    uint64_t ud[2];
    int64_t  sd[2];
    uint32_t uw[4];
    int32_t  sw[4];
    uint16_t uh[8];
    uint8_t  ub[16];
} ee_u128;

#define EE_RAM_SIZE (32u * 1024 * 1024)
#define EE_SPR_SIZE (16u * 1024)

/* COP0 register numbers used so far. */
#define EE_COP0_STATUS   12
#define EE_COP0_EPC      14
#define EE_COP0_ERROREPC 30
#define EE_STATUS_EXL    (1u << 1)
#define EE_STATUS_ERL    (1u << 2)
#define EE_STATUS_EIE    (1u << 16)

typedef struct ee_ctx ee_ctx;
typedef struct ee_sif ee_sif;
typedef void (*ee_fn)(ee_ctx *c);

/* Stand-in address of the kernel syscall table (kernel memory, kseg0).
   Our choice: the real kernel's table is elsewhere, so anything derived from
   it must be compared as a variable. */
#define EE_HLE_SYSCALL_TABLE 0x80070000u

/* Code that the game copies at run time and that was recompiled from its
   source bytes (--blob). Before running it, the runtime checks that guest
   memory at `addr` still holds exactly those bytes (FNV-1a 64 of `size` bytes). */
typedef struct {
    uint32_t addr, size;
    uint64_t hash;
} ee_code_range;

uint64_t ee_fnv1a(const uint8_t *p, size_t n);

struct ee_ctx {
    ee_u128  r[32];          /* GPRs, 128-bit. r[0] is kept zero. */
    uint64_t hi, lo;         /* HI/LO (lower 64 bits) */
    uint64_t hi1, lo1;       /* HI1/LO1 (upper 64 bits, MMI pipeline 1) */
    uint32_t sa;             /* shift amount register */
    uint32_t cop0[32];
    union { float f; uint32_t u; } fpr[32];
    float    acc;            /* FPU accumulator */
    uint32_t fcr31;

    uint8_t *ram;            /* EE_RAM_SIZE bytes */
    uint8_t *spr;            /* scratchpad, EE_SPR_SIZE bytes */

    /* Run control */
    jmp_buf  *exit_jmp;      /* longjmp target when the run ends */
    uint32_t stop_pc;        /* address where the harness wants to stop */
    uint32_t end_pc;         /* where the run actually ended */
    int      end_reason;     /* EE_END_* */
    char     end_detail[160];

    FILE    *trace;          /* JSON lines, may be NULL */
    uint64_t trace_seq;

    /* Hardware stand-ins */
    uint32_t timer[4][4];    /* COUNT, MODE, COMP, HOLD per timer */
    ee_sif *sif;             /* optional bounded SIF/fileio HLE */
    struct { int live, count, max, init; uint32_t attr, option; } sema[256];
    struct ee_sched *sched;  /* guest threads, NULL without a scheduler */

    /* HLE kernel state. IDs are ours and will not match a real console. */
    struct {
        int      next_thread_id;
        int      next_sema_id;
        int      next_handler_id;
        int      cur_thread_id;
        uint32_t cur_priority;
        uint32_t main_gp, main_stack, main_stack_size, main_args, main_root;
        /* Boot arguments SetupThread hands to crt0 (test input; none = argc 0). */
        const char *boot_argv[8];
        int      boot_argc;
        uint32_t heap_start;
        int32_t  heap_size;
        uint32_t osd_config;
        uint32_t intc_enabled;   /* bit per cause, from _EnableIntc */
        uint32_t user_syscall[256]; /* handlers installed by SetSyscall, 1..255 */
        /* SetSyscall numbers this HLE does not model (outside 1..255). Kept so
           a later call to one of them ends the run with a clear reason. */
        struct { int num; uint32_t handler; } rejected[16];
        int      n_rejected;
        const char *status_override;   /* set by a syscall to change its trace status */
    } k;

    /* Recompiled function for a guest address, or NULL (set by the harness). */
    ee_fn (*lookup)(uint32_t addr);
    const ee_code_range *code_ranges;  /* copied code, set by the harness */
    int n_code_ranges;
    /* Code generated with --hooks asks this, on entry to each function, for
       a native replacement (NULL = run the recompiled one). */
    ee_fn (*hook)(uint32_t addr);
};

enum {
    EE_END_NONE = 0,
    EE_END_STOP_PC,          /* reached stop_pc: the expected end */
    EE_END_UNRECOMPILED,     /* call into code that was not recompiled */
    EE_END_UNSUPPORTED,      /* instruction, syscall or MMIO not handled */
    EE_END_BAD_ADDRESS,      /* memory access outside mapped ranges */
    EE_END_RETURNED,         /* the entry function returned */
    EE_END_CODE_MISMATCH,    /* copied code differs from what was recompiled */
};

/* Setup and teardown. */
int  ee_init(ee_ctx *c);
void ee_free(ee_ctx *c);
/* Load a flat image (extracted/core.bin) at vaddr. Returns bytes loaded, <0 on error. */
long ee_load_image(ee_ctx *c, const char *path, uint32_t vaddr);

/* Ends the run: records where and why, then longjmps to exit_jmp. */
void ee_end(ee_ctx *c, int reason, uint32_t pc, const char *fmt, ...);

/* Memory. Addresses are EE virtual addresses as the game uses them. */
uint8_t  ee_r8 (ee_ctx *c, uint32_t a);
uint16_t ee_r16(ee_ctx *c, uint32_t a);
uint32_t ee_r32(ee_ctx *c, uint32_t a);
uint64_t ee_r64(ee_ctx *c, uint32_t a);
ee_u128  ee_r128(ee_ctx *c, uint32_t a);
void ee_w8 (ee_ctx *c, uint32_t a, uint8_t  v);
void ee_w16(ee_ctx *c, uint32_t a, uint16_t v);
void ee_w32(ee_ctx *c, uint32_t a, uint32_t v);
void ee_w64(ee_ctx *c, uint32_t a, uint64_t v);
void ee_w128(ee_ctx *c, uint32_t a, ee_u128 v);
/* Unaligned (LDL/LDR, LWL/LWR, SDL/SDR, SWL/SWR). Loads return the merged
   register value; `old` is the register before the load. */
uint64_t ee_ldl(ee_ctx *c, uint32_t a, uint64_t old);
uint64_t ee_ldr(ee_ctx *c, uint32_t a, uint64_t old);
uint64_t ee_lwl(ee_ctx *c, uint32_t a, uint64_t old);
uint64_t ee_lwr(ee_ctx *c, uint32_t a, uint64_t old);
void ee_sdl(ee_ctx *c, uint32_t a, uint64_t v);
void ee_sdr(ee_ctx *c, uint32_t a, uint64_t v);
void ee_swl(ee_ctx *c, uint32_t a, uint32_t v);
void ee_swr(ee_ctx *c, uint32_t a, uint32_t v);

/* Kernel call (syscall instruction). Number is in v1. */
void ee_syscall(ee_ctx *c, uint32_t pc);

/* Special instructions. */
void ee_cache(ee_ctx *c, unsigned op, uint32_t addr, uint32_t pc);
void ee_di(ee_ctx *c);
void ee_ei(ee_ctx *c);
void ee_call(ee_ctx *c, uint32_t target, uint32_t pc);
void ee_sif_interrupt_point(ee_ctx *c);
/* ERET: only as a return to the caller, see mem.c. Returns normally if so. */
void ee_eret(ee_ctx *c, uint32_t pc);

/* Before running copied code: ends the run if guest memory at the code's
   range no longer matches what was recompiled. No-op outside code ranges. */
void ee_check_code(ee_ctx *c, uint32_t target, uint32_t pc);

/* Control transfers the recompiler could not resolve statically. */
void ee_unrecompiled(ee_ctx *c, uint32_t target, uint32_t from_pc);
void ee_unsupported(ee_ctx *c, uint32_t pc, const char *what);

/* Trace (JSON lines). */
void ee_trace(ee_ctx *c, const char *fmt, ...);

/* Implemented = behaves like the real service for what this test covers.
   Stubbed     = fixed or simplified result, or only recorded. */
#define EE_IMPLEMENTED "implemented"
#define EE_STUBBED     "stubbed"
/* Unhandled = the call happened, the HLE does not model it, and says so. */
#define EE_UNHANDLED   "unhandled"

#ifdef __cplusplus
}
#endif

#endif /* OPENGT4_EE_H */
