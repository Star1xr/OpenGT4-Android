/*
 * Runs recompiled crt0 + _InitSys from the user's core.bin and writes what
 * happened, for tests/initsys/run_test.py to check.
 *
 * Usage: initsys_run <core.bin> <out_dir>
 *
 * Output in out_dir:
 *   trace.jsonl   kernel calls, timer accesses, COP0 changes, end event
 *   regs.json     registers where the run ended
 *   ram_00000000_00100000.bin   kernel area (libkernl patches, HLE syscall table)
 *   ram_00100000_008a215c.bin   program image, data and bss after the run
 *   ram_01ff8000_02000000.bin   stack given to SetupThread
 *   summary.json  end reason, end pc, initial state that was assumed
 *
 * Exit code 0 only if the run ended at the stop address.
 */
#include "ee.h"
#include "thread.h"
#include "sif.h"

#include <stdlib.h>
#include <string.h>

void recomp_entry(ee_ctx *c);
ee_fn recomp_lookup(uint32_t addr);
extern const ee_code_range recomp_code_ranges[];
extern const int recomp_n_code_ranges;

#define LOAD_ADDR   0x00100000u
#if defined(OPENGT4_MAIN_CHUNK)
#define STOP_PC 0u                  /* no stop: run until something is missing */
#elif defined(OPENGT4_INIT_CHUNK)
#define STOP_PC 0x00100204u
#else
#define STOP_PC     0x001001f0u   /* jal FlushCache in crt0, right after _InitSys returns */
#endif
/* TEST INPUT, not a PS2 value: COP0 Status handed to crt0. The real value
   left by the kernel and bootstrap is unknown until the reference capture. */
#define INIT_STATUS 0x70030c13u

static int dump(const ee_ctx *c, const char *dir, uint32_t from, uint32_t to)
{
    char path[512];
    FILE *f;
    snprintf(path, sizeof path, "%s/ram_%08x_%08x.bin", dir, from, to);
    f = fopen(path, "wb");
    if (!f)
        return -1;
    fwrite(c->ram + from, 1, to - from, f);
    fclose(f);
    return 0;
}

static void write_regs(const ee_ctx *c, const char *dir)
{
    static const char *names[32] = {
        "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
        "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra" };
    char path[512];
    FILE *f;
    snprintf(path, sizeof path, "%s/regs.json", dir);
    f = fopen(path, "w");
    if (!f)
        return;
    fputs("{\n  \"gpr\": {\n", f);
    for (int i = 0; i < 32; i++)
        fprintf(f, "    \"%s\": \"0x%016llx%016llx\"%s\n", names[i],
                (unsigned long long)c->r[i].ud[1], (unsigned long long)c->r[i].ud[0], i < 31 ? "," : "");
    fprintf(f, "  },\n  \"hi\": \"0x%016llx\", \"lo\": \"0x%016llx\",\n"
               "  \"hi1\": \"0x%016llx\", \"lo1\": \"0x%016llx\",\n"
               "  \"sa\": \"0x%08x\", \"cop0_status\": \"0x%08x\", \"fcr31\": \"0x%08x\"\n}\n",
            (unsigned long long)c->hi, (unsigned long long)c->lo,
            (unsigned long long)c->hi1, (unsigned long long)c->lo1,
            c->sa, c->cop0[EE_COP0_STATUS], c->fcr31);
    fclose(f);
}

int main(int argc, char **argv)
{
    static ee_ctx c;
    jmp_buf exit_jmp;
    char path[512];
    long loaded;
    FILE *f;

#ifdef OPENGT4_INIT_CHUNK
    if (argc != 6) {
        fprintf(stderr, "usage: %s <core.bin> <out_dir> <fresh:0|1> <retail|tool|missing> <delay:0..1000>\n", argv[0]);
        return 2;
    }
    if ((strcmp(argv[3], "0") && strcmp(argv[3], "1")) ||
        (strcmp(argv[4], "retail") && strcmp(argv[4], "tool") && strcmp(argv[4], "missing"))) return 2;
#endif
    if (argc < 3) {
        fprintf(stderr, "usage: %s <core.bin> <out_dir>\n", argv[0]);
        return 2;
    }
    if (ee_init(&c) != 0) {
        fprintf(stderr, "out of memory\n");
        return 2;
    }
    loaded = ee_load_image(&c, argv[1], LOAD_ADDR);
    if (loaded < 0) {
        fprintf(stderr, "cannot load %s (%ld)\n", argv[1], loaded);
        return 2;
    }

    snprintf(path, sizeof path, "%s/trace.jsonl", argv[2]);
    c.trace = fopen(path, "w");
    if (!c.trace) {
        fprintf(stderr, "cannot write %s\n", path);
        return 2;
    }

#ifdef OPENGT4_INIT_CHUNK
    /* Synthetic ROMVER fixtures, not copied from a BIOS. */
    const uint8_t *romver = (const uint8_t *)((argc > 4 && !strcmp(argv[4], "tool"))
                           ? "0000T000000000" : "0000A000000000");
    int fresh = argc > 3 ? atoi(argv[3]) : 0;
    int open_fail = argc > 4 && !strcmp(argv[4], "missing");
    unsigned delay = argc > 5 ? (unsigned)atoi(argv[5]) : 0;
    if (delay > 1000 || ee_sif_init(&c, fresh, romver, open_fail, delay)) return 2;
    ee_trace(&c, "\"ev\":\"sif_inputs\",\"fresh\":%d,\"romver\":\"%.14s\",\"open_fail\":%d,\"delay_points\":%u,"
                 "\"note\":\"synthetic test inputs; IRQ at syscall, ei, branch labels and WaitSema; threads scheduled at kernel switch points; timers not\"",
             fresh, romver, open_fail, delay);
#endif
#ifdef OPENGT4_MAIN_CHUNK
    /* TEST INPUT: the arguments the bootstrap passed on PCSX2 (capture). */
    c.k.boot_argv[0] = "cdrom0:\\CORE.GT4;1";
    c.k.boot_argv[1] = "hot";
    c.k.boot_argc = 2;
#endif
    /* Initial state: all registers zero, Status as below, run stops at STOP_PC. */
    c.cop0[EE_COP0_STATUS] = INIT_STATUS;
    c.stop_pc = STOP_PC;
    c.exit_jmp = &exit_jmp;
    c.lookup = recomp_lookup;
    c.code_ranges = recomp_code_ranges;
    c.n_code_ranges = recomp_n_code_ranges;
    ee_trace(&c, "\"ev\":\"start\",\"entry\":\"0x00100008\",\"loaded\":%ld,"
                 "\"cop0_status\":\"0x%08x\",\"status\":\"%s\"",
             loaded, INIT_STATUS, EE_STUBBED);

    /* Guest threads run on host fibers; the main thread (ID 1) runs the entry.
       The run returns here when it ends (stop address, error, or entry return). */
    if (setjmp(exit_jmp) == 0 && ee_sched_run(&c, recomp_entry) != 0) {
        fprintf(stderr, "cannot start the thread scheduler\n");
        return 2;
    }
    c.exit_jmp = NULL;
    fclose(c.trace);
    c.trace = NULL;

    write_regs(&c, argv[2]);
    dump(&c, argv[2], 0x00000000u, 0x00100000u);   /* kernel area: patches, HLE syscall table */
    dump(&c, argv[2], 0x00100000u, 0x008a215cu);
    dump(&c, argv[2], 0x01ff8000u, 0x02000000u);

    snprintf(path, sizeof path, "%s/summary.json", argv[2]);
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "{\"end_reason\": %d, \"end_pc\": \"0x%08x\", \"detail\": \"%s\", "
                   "\"stop_pc\": \"0x%08x\", \"loaded_bytes\": %ld,\n"
                   " \"test_inputs\": {\"_note\": \"chosen for this test, not PS2 values\", "
                   "\"cop0_status\": \"0x%08x\", \"gprs\": \"all zero\", "
                   "\"setup_thread_sp\": \"stack + size - 0x2a0 (USA 2.30 context)\", \"argc\": 0, "
                   "\"osd_config\": \"0x00010000\", \"hle_syscall_table\": \"0x%08x\"},\n"
                   " \"limitations\": [",
                c.end_reason, c.end_pc, c.end_detail, STOP_PC, loaded, INIT_STATUS,
                EE_HLE_SYSCALL_TABLE);
        for (int i = 0; i < c.k.n_rejected; i++)
            fprintf(f, "%s{\"what\": \"SetSyscall number outside 1..255, not modelled\", "
                       "\"num\": \"0x%x\", \"handler\": \"0x%08x\"}",
                    i ? ", " : "", (unsigned)c.k.rejected[i].num, c.k.rejected[i].handler);
        fprintf(f, "]");
#ifdef OPENGT4_INIT_CHUNK
        fprintf(f, ",\n \"sif_test_inputs\": {\"_note\":\"synthetic fixtures, not captured PS2 values\","
                   "\"fresh\":%d,\"romver\":\"%.14s\",\"open_fail\":%d,\"delay_points\":%u,"
                   "\"iop_command\":%u,\"iop_server\":%u,\"iop_receive\":%u},"
                   "\n \"sif_result\": {\"delivered\":%u,\"pending\":%u,\"file_open\":%d},"
                   "\n \"interrupt_policy\":\"syscall return, ei, branch labels, blocking WaitSema; EIE-gated, no nested IRQ\","
                   "\n \"reference_validation\":\"pending\",\"guest_threads_and_timer_interrupts\":\"not executed\"",
                fresh, romver, open_fail, delay, EE_SIF_IOP_CMD, EE_SIF_IOP_SERVER, EE_SIF_IOP_RECV,
                c.sif->delivered, c.sif->count, c.sif->opened);
#endif
        fprintf(f, "}\n");
        fclose(f);
    }
    printf("end reason %d at 0x%08x: %s\n", c.end_reason, c.end_pc, c.end_detail);
    ee_sched_free(&c);
    ee_free(&c);
    return c.end_reason == EE_END_STOP_PC ? 0 : 1;
}
