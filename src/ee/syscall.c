/*
 * HLE of the EE kernel calls needed so far (crt0 + _InitSys).
 *
 * Each call is traced with its number, name, arguments, return value and
 * status (implemented / stubbed). Any other syscall ends the run, so a missing
 * service shows up as an error rather than a silent zero.
 *
 * Structure layouts follow ps2sdk's kernel.h (ee_thread_t, ee_thread_status_t,
 * ee_sema_t). IDs are allocated here and will not match a real console, so a
 * comparison against a reference must treat them as variables.
 *
 * Threads are created and recorded but never run. INTC handlers are not run;
 * the optional SIF HLE dispatches the registered DMAC channel 5 handler. docs/analysis/EXECUTION_START.md explains what that
 * leaves out of a comparison.
 */
#include "ee.h"
#include "thread.h"
#include "sif.h"

#include <string.h>

#define A0 ((uint32_t)c->r[4].uw[0])
#define A1 ((uint32_t)c->r[5].uw[0])
#define A2 ((uint32_t)c->r[6].uw[0])
#define A3 ((uint32_t)c->r[7].uw[0])
#define T0 ((uint32_t)c->r[8].uw[0])

static void set_v0(ee_ctx *c, int32_t v)
{
    c->r[2].sd[0] = (int64_t)v;
}

typedef struct {
    int         num;
    const char *name;
    const char *status;
    /* Returns the value for v0. `ret_set` = 0 means the call returns nothing. */
    int32_t   (*fn)(ee_ctx *c, int *ret_set, char *note, size_t note_len);
} syscall_def;

/* ---- threads ---- */

static int32_t k_setup_thread(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    /* SetupThread(gp, stack, stack_size, args, root).
       The supported explicit-RAM stack reserves a 0x2a0-byte context.
       Verified against the captured USA 2.30 handler and crt0 result; see
       docs/analysis/THREAD_REFERENCE.md. Automatic stack allocation is not modeled.
       Boot arguments remain a synthetic argc=0; no scheduler is implied. */
    if (A1 >= EE_RAM_SIZE || A2 < 0x2a0 || A2 > EE_RAM_SIZE - A1)
        ee_unsupported(c, 0, "SetupThread requires an explicit RAM stack of at least 0x2a0 bytes");
    uint32_t top = A1 + A2;
    uint32_t sp = top - 0x2a0;
    c->k.main_gp = A0;
    c->k.main_stack = A1;
    c->k.main_stack_size = A2;
    c->k.main_args = A3;
    c->k.main_root = T0;
    c->k.cur_thread_id = 1;
    c->k.cur_priority = 0;
    ee_sched_setup_main(c, A0, A1, A2);
    /* Only the four initial context words observed in SetupThread are set.
       Other saved registers are not zeroed or invented here. */
    ee_w32(c, sp + 0x1c0, A0);
    ee_w32(c, sp + 0x1d0, top - 0x20);
    ee_w32(c, sp + 0x1e0, top - 0x20);
    ee_w32(c, sp + 0x1f0, T0);
    if (A3) {
        /* Argument block, layout seen in the PCSX2 capture at 0x006d6380:
           argc at +0, argv[16] at +4, strings from +0x44 (256 bytes). */
        uint32_t str = A3 + 0x44, end = A3 + 0x44 + 0x100;
        int argc = 0;
        for (int i = 0; i < c->k.boot_argc && i < 16; i++) {
            size_t len = strlen(c->k.boot_argv[i]) + 1;
            if (str + len > end)
                ee_unsupported(c, 0, "boot arguments longer than the 256-byte area");
            ee_w32(c, A3 + 4 + 4u * (uint32_t)i, str);
            for (size_t k = 0; k < len; k++)
                ee_w8(c, str + (uint32_t)k, (uint8_t)c->k.boot_argv[i][k]);
            str += (uint32_t)len;
            argc++;
        }
        ee_w32(c, A3, (uint32_t)argc);
    }
    snprintf(note, n, "root=0x%08x; sp=stack+size-0x2a0 (USA 2.30 context); boot args are a test input (argc=%d)", T0, c->k.boot_argc);
    *ret_set = 1;
    return (int32_t)sp;
}

static int32_t k_setup_heap(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    c->k.heap_start = A0;
    c->k.heap_size = (int32_t)A1;
    snprintf(note, n, "returns heap_start");
    *ret_set = 1;
    return (int32_t)A0;
}






/* ---- semaphores ---- */





/* ---- interrupts ---- */

static int32_t k_add_intc_handler2(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    int id = ++c->k.next_handler_id;
    snprintf(note, n, "cause=%d handler=0x%08x next=%d arg=0x%08x id=%d (never called)",
             (int)A0, A1, (int)A2, A3, id);
    *ret_set = 1;
    return id;
}

static int32_t k_enable_intc(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    uint32_t bit = 1u << (A0 & 31);
    int was = (c->k.intc_enabled & bit) != 0;
    c->k.intc_enabled |= bit;
    snprintf(note, n, "cause=%d, returns 1 if it was disabled", (int)A0);
    *ret_set = 1;
    return was ? 0 : 1;
}

/* ---- OSD config ---- */

static int32_t k_get_osd_config(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    /* Stubbed value: language English (1 << 16), everything else 0. */
    if (!c->k.osd_config)
        c->k.osd_config = 1u << 16;
    ee_w32(c, A0, c->k.osd_config);
    snprintf(note, n, "wrote 0x%08x (TEST INPUT, not the console setting)", c->k.osd_config);
    *ret_set = 0;
    return 0;
}

static int32_t k_set_osd_config(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    c->k.osd_config = ee_r32(c, A0);
    snprintf(note, n, "stored 0x%08x", c->k.osd_config);
    *ret_set = 0;
    return 0;
}

/* ---- syscall table ----
 * SetSyscall(num, handler): implemented for positive numbers. The handler is
 * stored in a table at EE_HLE_SYSCALL_TABLE (kernel memory, our address) and
 * later calls to that number run the recompiled handler. libkernl searches
 * kernel memory for these entries to find the table base, so the entry has to
 * really be there. */

static int32_t k_set_syscall(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    int num = (int)A0;
    if (num > 0 && num < 256) {
        c->k.user_syscall[num] = A1;
        ee_w32(c, EE_HLE_SYSCALL_TABLE + (uint32_t)num * 4, A1);
        snprintf(note, n, "syscall 0x%x -> 0x%08x, table entry at 0x%08x (HLE address)",
                 num, A1, EE_HLE_SYSCALL_TABLE + (uint32_t)num * 4);
    } else {
        /* LIMITATION: the real kernel's handling of numbers outside 1..255 is
           unknown (table size, masking, error). Nothing is written, the number
           is not truncated, and the registration is kept so that a later call
           to it ends the run instead of doing something made up. */
        if (c->k.n_rejected < (int)(sizeof c->k.rejected / sizeof c->k.rejected[0])) {
            c->k.rejected[c->k.n_rejected].num = num;
            c->k.rejected[c->k.n_rejected].handler = A1;
            c->k.n_rejected++;
        }
        c->k.status_override = EE_UNHANDLED;
        snprintf(note, n, "LIMITATION: number 0x%x (%d) outside the 1..255 table this HLE models, "
                          "real kernel behaviour unknown. Not stored; a call to it ends the run",
                 (unsigned)num, num);
    }
    *ret_set = 0;
    return 0;
}

/* ---- cache ---- */

static int32_t k_flush_cache(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    /* Sufficient because (1) the runtime has no caches, so writes are seen by
       later reads at once, and (2) copied code is never run from its bytes:
       it runs as code recompiled ahead of time, and ee_check_code verifies the
       copied bytes match before each dispatch into it. */
    (void)c;
    snprintf(note, n, "no caches modelled; copied code is checked before it runs");
    *ret_set = 0;
    return 0;
}

static const syscall_def table[] = {
    { 0x3c, "SetupThread",          EE_STUBBED,     k_setup_thread },
    { 0x3d, "SetupHeap",            EE_STUBBED,     k_setup_heap },
    { 0x20, "CreateThread",         EE_IMPLEMENTED, ee_k_create_thread },
    { 0x21, "DeleteThread",         EE_IMPLEMENTED, ee_k_delete_thread },
    { 0x22, "StartThread",          EE_IMPLEMENTED, ee_k_start_thread },
    { 0x23, "ExitThread",           EE_IMPLEMENTED, ee_k_exit_thread },
    { 0x24, "ExitDeleteThread",     EE_IMPLEMENTED, ee_k_exit_delete_thread },
    { 0x29, "ChangeThreadPriority", EE_IMPLEMENTED, ee_k_change_thread_priority },
    { -0x2a, "iChangeThreadPriority", EE_IMPLEMENTED, ee_k_change_thread_priority },
    { 0x2b, "RotateThreadReadyQueue", EE_IMPLEMENTED, ee_k_rotate_ready_queue },
    { 0x2f, "GetThreadId",          EE_IMPLEMENTED, ee_k_get_thread_id },
    { 0x30, "ReferThreadStatus",    EE_IMPLEMENTED, ee_k_refer_thread_status },
    { -0x31, "iReferThreadStatus",  EE_IMPLEMENTED, ee_k_refer_thread_status },
    { 0x32, "SleepThread",          EE_IMPLEMENTED, ee_k_sleep_thread },
    { 0x33, "WakeupThread",         EE_IMPLEMENTED, ee_k_wakeup_thread },
    { -0x34, "iWakeupThread",       EE_IMPLEMENTED, ee_k_wakeup_thread },
    { 0x40, "CreateSema",           EE_IMPLEMENTED, ee_k_create_sema },
    { 0x41, "DeleteSema",           EE_IMPLEMENTED, ee_k_delete_sema },
    { -0x49, "iDeleteSema",         EE_IMPLEMENTED, ee_k_delete_sema },
    { 0x42, "SignalSema",           EE_IMPLEMENTED, ee_k_signal_sema },
    { -0x43, "iSignalSema",         EE_IMPLEMENTED, ee_k_signal_sema },
    { 0x44, "WaitSema",             EE_IMPLEMENTED, ee_k_wait_sema },
    { 0x45, "PollSema",             EE_IMPLEMENTED, ee_k_poll_sema },
    { -0x46, "iPollSema",           EE_IMPLEMENTED, ee_k_poll_sema },
    { 0x47, "ReferSemaStatus",      EE_IMPLEMENTED, ee_k_refer_sema_status },
    { -0x48, "iReferSemaStatus",    EE_IMPLEMENTED, ee_k_refer_sema_status },
    { 0x10, "AddIntcHandler2",      EE_STUBBED,     k_add_intc_handler2 },
    { 0x14, "_EnableIntc",          EE_STUBBED,     k_enable_intc },
    { 0x4b, "GetOsdConfigParam",    EE_STUBBED,     k_get_osd_config },
    { 0x4a, "SetOsdConfigParam",    EE_STUBBED,     k_set_osd_config },
    { 0x64, "FlushCache",           EE_IMPLEMENTED, k_flush_cache },
    { 0x74, "SetSyscall",           EE_IMPLEMENTED, k_set_syscall },
};

void ee_syscall(ee_ctx *c, uint32_t pc)
{
    int num = c->r[3].sw[0];
    uint32_t a0 = A0, a1 = A1, a2 = A2, a3 = A3;
    char note[200] = "";

    /* A number whose SetSyscall registration was not modelled. */
    for (int i = 0; i < c->k.n_rejected; i++)
        if (c->k.rejected[i].num == num)
            ee_end(c, EE_END_UNSUPPORTED, pc,
                   "syscall 0x%x registered by SetSyscall (handler 0x%08x) but not modelled",
                   (unsigned)num, c->k.rejected[i].handler);

    /* A number the game installed with SetSyscall runs the game's handler. */
    if (num > 0 && num < 256 && c->k.user_syscall[num]) {
        uint32_t h = c->k.user_syscall[num];
        ee_fn fn = c->lookup ? c->lookup(h) : NULL;
        if (!fn)
            ee_unrecompiled(c, h, pc);
        ee_check_code(c, h, pc);
        ee_trace(c, "\"ev\":\"syscall\",\"pc\":\"0x%08x\",\"num\":%d,\"name\":\"user_0x%x\","
                    "\"args\":[\"0x%08x\",\"0x%08x\",\"0x%08x\",\"0x%08x\"],\"handler\":\"0x%08x\","
                    "\"status\":\"%s\",\"note\":\"game handler installed by SetSyscall\"",
                 pc, num, num, a0, a1, a2, a3, h, EE_IMPLEMENTED);
        fn(c);
        ee_trace(c, "\"ev\":\"syscall_ret\",\"num\":%d,\"ret\":\"0x%08x\"", num, c->r[2].uw[0]);
        ee_sif_interrupt_point(c);
        return;
    }

    int32_t sif_ret;
    if (ee_sif_syscall(c, num, &sif_ret)) {
        set_v0(c, sif_ret);
        ee_sif_interrupt_point(c);
        return;
    }

    for (size_t i = 0; i < sizeof table / sizeof table[0]; i++) {
        if (table[i].num == num) {
            int ret_set = 0;
            const char *status;
            int32_t ret;
            c->k.status_override = NULL;
            ret = table[i].fn(c, &ret_set, note, sizeof note);
            status = c->k.status_override ? c->k.status_override : table[i].status;
            if (ret_set)
                set_v0(c, ret);
            char retbuf[16] = "null";
            if (ret_set)
                snprintf(retbuf, sizeof retbuf, "%d", ret);
            ee_trace(c, "\"ev\":\"syscall\",\"pc\":\"0x%08x\",\"num\":%d,\"name\":\"%s\","
                        "\"args\":[\"0x%08x\",\"0x%08x\",\"0x%08x\",\"0x%08x\"],"
                        "\"ret\":%s,\"status\":\"%s\",\"note\":\"%s\"",
                     pc, num, table[i].name, a0, a1, a2, a3, retbuf, status, note);
            ee_sif_interrupt_point(c);
            return;
        }
    }
    ee_end(c, EE_END_UNSUPPORTED, pc, "syscall %d (0x%x) not implemented", num, (unsigned)num & 0xffffffffu);
}
