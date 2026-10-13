/*
 * Guest threads and semaphores. See thread.h and docs/analysis/THREADS.md.
 *
 * Without a scheduler (ee_sched_run not called, as in the unit tests and the
 * older single-thread harness), WaitSema keeps its previous behaviour: it can
 * only be satisfied by a queued IOP reply, and otherwise ends the run.
 */
#include "thread.h"
#include "sif.h"

#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <ucontext.h>
#endif

#define A0 ((uint32_t)c->r[4].uw[0])
#define A1 ((uint32_t)c->r[5].uw[0])

#define FIBER_STACK (4u * 1024 * 1024)
#define CTX_SIZE    0x2a0u   /* saved context at the top of a thread stack */

typedef struct {
    ee_u128  r[32];
    uint64_t hi, lo, hi1, lo1;
    uint32_t sa;
    union { float f; uint32_t u; } fpr[32];
    float    acc;
    uint32_t fcr31;
} regs_t;

typedef struct {
    int      used, status, prio, init_prio;
    int      wait_type, wait_id, wakeup_count;
    int64_t  seq;             /* ready-queue order within a priority */
    uint64_t wait_seq;        /* FIFO order among waiters */
    uint32_t entry, stack, stack_size, gp, attr, option, arg;
    int      is_main;
    ee_fn    host_entry;      /* main thread only */
    void    *fiber;
#ifndef _WIN32
    ucontext_t uc;
    void    *uc_stack;
#endif
    jmp_buf  top;             /* restart point after exit, inside the fiber */
    regs_t   regs;
} thr_t;

struct ee_sched {
    ee_ctx  *c;
    thr_t    t[EE_MAX_THREADS];
    int      cur;
    int64_t  seq, front_seq;
    uint64_t wait_seq;
    int      in_guest, in_idle, deferred;
    uint64_t switches;
#ifdef _WIN32
    void    *host_fiber;
    int      converted;
#else
    ucontext_t host_uc;
#endif
};

/* ---- register save/restore ---- */

static void save_regs(const ee_ctx *c, regs_t *r)
{
    memcpy(r->r, c->r, sizeof r->r);
    r->hi = c->hi; r->lo = c->lo; r->hi1 = c->hi1; r->lo1 = c->lo1; r->sa = c->sa;
    memcpy(r->fpr, c->fpr, sizeof r->fpr);
    r->acc = c->acc; r->fcr31 = c->fcr31;
}

static void load_regs(ee_ctx *c, const regs_t *r)
{
    memcpy(c->r, r->r, sizeof c->r);
    c->hi = r->hi; c->lo = r->lo; c->hi1 = r->hi1; c->lo1 = r->lo1; c->sa = r->sa;
    memcpy(c->fpr, r->fpr, sizeof c->fpr);
    c->acc = r->acc; c->fcr31 = r->fcr31;
}

/* ---- fibers ---- */

static void thread_body(struct ee_sched *s, int id);

#ifdef _WIN32
typedef struct { struct ee_sched *s; int id; } fiber_arg;
static fiber_arg fiber_args[EE_MAX_THREADS];

static VOID CALLBACK fiber_proc(LPVOID p)
{
    fiber_arg *a = p;
    thread_body(a->s, a->id);
}

static int make_fiber(struct ee_sched *s, int id)
{
    fiber_args[id].s = s;
    fiber_args[id].id = id;
    s->t[id].fiber = CreateFiber(FIBER_STACK, fiber_proc, &fiber_args[id]);
    return s->t[id].fiber ? 0 : -1;
}

static void jump_to(struct ee_sched *s, int id)
{
    SwitchToFiber(id < 0 ? s->host_fiber : s->t[id].fiber);
}
#else
static struct ee_sched *uc_sched;
static void uc_proc(int id) { thread_body(uc_sched, id); }

static int make_fiber(struct ee_sched *s, int id)
{
    thr_t *t = &s->t[id];
    t->uc_stack = malloc(FIBER_STACK);
    if (!t->uc_stack || getcontext(&t->uc))
        return -1;
    t->uc.uc_stack.ss_sp = t->uc_stack;
    t->uc.uc_stack.ss_size = FIBER_STACK;
    t->uc.uc_link = NULL;
    uc_sched = s;
    makecontext(&t->uc, (void (*)(void))uc_proc, 1, id);
    t->fiber = t;
    return 0;
}

#endif

/* ---- scheduling ---- */

static int valid_id(const struct ee_sched *s, int id)
{
    return id > 0 && id < EE_MAX_THREADS && s->t[id].used;
}

/* Highest priority (lowest number) READY thread, FIFO within a priority. */
static int pick_ready(const struct ee_sched *s)
{
    int best = -1;
    for (int i = 1; i < EE_MAX_THREADS; i++) {
        const thr_t *t = &s->t[i];
        if (!t->used || t->status != EE_THS_READY)
            continue;
        if (best < 0 || t->prio < s->t[best].prio
            || (t->prio == s->t[best].prio && t->seq < s->t[best].seq))
            best = i;
    }
    return best;
}

static void make_ready(struct ee_sched *s, int id)
{
    s->t[id].status = EE_THS_READY;
    s->t[id].wait_type = EE_WAIT_NONE;
    s->t[id].wait_id = 0;
    s->t[id].seq = ++s->seq;
}

static void switch_to(struct ee_sched *s, int next, const char *why)
{
    ee_ctx *c = s->c;
    int prev = s->cur;
    ee_trace(c, "\"ev\":\"thread_switch\",\"from\":%d,\"to\":%d,\"reason\":\"%s\"", prev, next, why);
    save_regs(c, &s->t[prev].regs);
    load_regs(c, &s->t[next].regs);
    s->t[next].status = EE_THS_RUN;
    s->cur = next;
    s->switches++;
#ifndef _WIN32
    {
        ucontext_t *from = &s->t[prev].uc;
        swapcontext(from, &s->t[next].uc);
    }
#else
    jump_to(s, next);
#endif
}

/* Deliver one pending interrupt while no thread can run. Interrupts are taken
   by the kernel's idle loop regardless of the blocked thread's EIE. Returns 0
   if nothing can ever make progress. */
static int idle_interrupt(struct ee_sched *s)
{
    ee_ctx *c = s->c;
    ee_sif *f = c->sif;
    if (!f || !f->count || f->in_handler || !f->enabled || !f->chain)
        return 0;
    uint32_t st = c->cop0[EE_COP0_STATUS];
    c->cop0[EE_COP0_STATUS] |= EE_STATUS_EIE;
    s->in_idle = 1;
    ee_sif_interrupt_point(c);
    s->in_idle = 0;
    c->cop0[EE_COP0_STATUS] = st;
    return 1;
}

/* The current thread may have stopped running (blocked, dormant) or a better
   thread may be ready. Switch if needed. Returns when the current thread runs
   again. */
static void reschedule(struct ee_sched *s, const char *why)
{
    ee_ctx *c = s->c;
    thr_t *cur = &s->t[s->cur];
    if (c->sif && c->sif->in_handler) {   /* never switch inside a handler */
        s->deferred = 1;
        return;
    }
    if (cur->status == EE_THS_RUN) {
        int best = pick_ready(s);
        if (best < 0 || s->t[best].prio >= cur->prio)
            return;
        cur->status = EE_THS_READY;
        cur->seq = --s->front_seq;        /* preempted: head of its queue */
        switch_to(s, best, why);
        return;
    }
    for (;;) {
        int best = pick_ready(s);
        if (best == s->cur) {             /* woken while idle, no switch */
            cur->status = EE_THS_RUN;
            return;
        }
        if (best > 0) {
            switch_to(s, best, why);
            return;
        }
        if (!idle_interrupt(s))
            ee_end(c, EE_END_UNSUPPORTED, 0,
                   "deadlock: thread %d waits (%s) and no thread or interrupt can run", s->cur, why);
    }
}

static void thread_body(struct ee_sched *s, int id)
{
    ee_ctx *c = s->c;
    thr_t *t = &s->t[id];
    setjmp(t->top);                       /* a restarted thread comes back here */
    if (t->is_main) {
        t->host_entry(c);
        c->end_reason = EE_END_RETURNED;
        c->end_pc = 0;
        snprintf(c->end_detail, sizeof c->end_detail, "main thread entry returned");
        ee_trace(c, "\"ev\":\"end\",\"reason\":%d,\"pc\":null,\"detail\":\"%s\"", EE_END_RETURNED, c->end_detail);
        ee_sched_exit_to_host(c);
    }
    ee_call(c, t->entry, 0);
    /* Entry returned: the thread exits, as through the kernel's exit stub. */
    ee_trace(c, "\"ev\":\"thread_exit\",\"id\":%d,\"how\":\"return\"", id);
    t->status = EE_THS_DORMANT;
    reschedule(s, "exit");
    longjmp(t->top, 1);                   /* restarted by StartThread */
}

/* ---- public ---- */

int ee_sched_active(const ee_ctx *c)
{
    return c->sched && c->sched->in_guest;
}

int ee_sched_run(ee_ctx *c, ee_fn entry)
{
    struct ee_sched *s = calloc(1, sizeof *s);
    if (!s)
        return -1;
    s->c = c;
    c->sched = s;
#ifdef _WIN32
    if (IsThreadAFiber()) {
        s->host_fiber = GetCurrentFiber();
    } else {
        s->host_fiber = ConvertThreadToFiber(NULL);
        s->converted = 1;
    }
    if (!s->host_fiber)
        return -1;
#endif
    thr_t *m = &s->t[1];
    m->used = 1;
    m->is_main = 1;
    m->host_entry = entry;
    m->status = EE_THS_RUN;
    m->prio = m->init_prio = 0;           /* test input, see docs/analysis/THREADS.md */
    if (make_fiber(s, 1))
        return -1;
    s->cur = 1;
    s->in_guest = 1;
    c->k.cur_thread_id = 1;
#ifdef _WIN32
    jump_to(s, 1);
#else
    swapcontext(&s->host_uc, &m->uc);
#endif
    s->in_guest = 0;
    ee_trace(c, "\"ev\":\"sched_summary\",\"switches\":%llu", (unsigned long long)s->switches);
    return 0;
}

void ee_sched_exit_to_host(ee_ctx *c)
{
    struct ee_sched *s = c->sched;
    s->in_guest = 0;
#ifdef _WIN32
    jump_to(s, -1);
#else
    swapcontext(&s->t[s->cur].uc, &s->host_uc);
#endif
    abort();                              /* the host never switches back */
}

void ee_sched_free(ee_ctx *c)
{
    struct ee_sched *s = c->sched;
    if (!s)
        return;
    for (int i = 0; i < EE_MAX_THREADS; i++) {
#ifdef _WIN32
        if (s->t[i].fiber)
            DeleteFiber(s->t[i].fiber);
#else
        free(s->t[i].uc_stack);
#endif
    }
#ifdef _WIN32
    if (s->converted)
        ConvertFiberToThread();
#endif
    free(s);
    c->sched = NULL;
}

void ee_sched_after_interrupt(ee_ctx *c)
{
    struct ee_sched *s = c->sched;
    if (!s || !s->in_guest || s->in_idle)
        return;
    s->deferred = 0;
    reschedule(s, "interrupt");
}

void ee_sched_setup_main(ee_ctx *c, uint32_t gp, uint32_t stack, uint32_t size)
{
    struct ee_sched *s = c->sched;
    if (!s)
        return;
    s->t[1].gp = gp;
    s->t[1].stack = stack;
    s->t[1].stack_size = size;
}

/* ---- thread calls ---- */

static struct ee_sched *need(ee_ctx *c, const char *what)
{
    if (!c->sched || !c->sched->in_guest)
        ee_unsupported(c, 0, what);
    return c->sched;
}

int32_t ee_k_create_thread(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    struct ee_sched *s = need(c, "CreateThread without scheduler");
    uint32_t p = A0;
    uint32_t entry = ee_r32(c, p + 4), stack = ee_r32(c, p + 8), size = ee_r32(c, p + 12);
    uint32_t gp = ee_r32(c, p + 16);
    int prio = (int)ee_r32(c, p + 20);
    int id;
    if (stack >= EE_RAM_SIZE || size < CTX_SIZE || size > EE_RAM_SIZE - stack || prio < 0 || prio > 127)
        ee_unsupported(c, 0, "CreateThread parameters outside the modelled range");
    for (id = 2; id < EE_MAX_THREADS && s->t[id].used; id++) {}
    if (id == EE_MAX_THREADS)
        ee_unsupported(c, 0, "thread capacity");
    thr_t *t = &s->t[id];
    void *keep = t->fiber;                /* fibers are reused, never deleted early */
    memset(&t->regs, 0, sizeof t->regs);
    t->used = 1;
    t->fiber = keep;
    t->status = EE_THS_DORMANT;
    t->entry = entry; t->stack = stack; t->stack_size = size; t->gp = gp;
    t->prio = t->init_prio = prio;
    t->attr = ee_r32(c, p + 28);
    t->option = ee_r32(c, p + 32);
    t->wait_type = EE_WAIT_NONE; t->wait_id = 0; t->wakeup_count = 0;
    t->is_main = 0;
    /* Initial context, same layout as SetupThread's: one 16-byte slot per GPR
       at stack + size - 0x2a0. The game reads and rewrites these before
       StartThread (0x005aede0). */
    uint32_t top = stack + size, ctx = top - CTX_SIZE;
    ee_w32(c, ctx + 0x1c0, gp);
    ee_w32(c, ctx + 0x1d0, top - 0x20);
    ee_w32(c, ctx + 0x1e0, top - 0x20);
    /* Return address: the root routine crt0 gave SetupThread (0x00100228 in
       this game), as the PCSX2 capture shows on this thread's stack. */
    ee_w32(c, ctx + 0x1f0, c->k.main_root ? c->k.main_root : EE_HLE_THREAD_EXIT);
    snprintf(note, n, "id=%d func=0x%08x stack=0x%08x size=0x%x gp=0x%08x prio=%d attr=0x%x, DORMANT",
             id, entry, stack, size, gp, prio, t->attr);
    *ret_set = 1;
    return id;
}

int32_t ee_k_start_thread(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    struct ee_sched *s = need(c, "StartThread without scheduler");
    int id = (int)A0;
    uint32_t arg = A1;
    if (!valid_id(s, id) || s->t[id].status != EE_THS_DORMANT || s->t[id].is_main)
        ee_unsupported(c, 0, "StartThread on a thread that is not a dormant created thread");
    thr_t *t = &s->t[id];
    /* Registers come from the saved context on the thread's stack. */
    uint32_t ctx = t->stack + t->stack_size - CTX_SIZE;
    memset(&t->regs, 0, sizeof t->regs);
    for (int r = 1; r < 32; r++)
        t->regs.r[r] = ee_r128(c, ctx + (uint32_t)r * 16);
    t->regs.r[4].sd[0] = (int64_t)(int32_t)arg;
    t->arg = arg;
    if (!t->fiber && make_fiber(s, id))
        ee_unsupported(c, 0, "cannot create a host fiber");
    make_ready(s, id);
    snprintf(note, n, "id=%d arg=0x%08x entry=0x%08x prio=%d sp=0x%08x, READY",
             id, arg, t->entry, t->prio, t->regs.r[29].uw[0]);
    *ret_set = 1;
    s->c->r[2].sd[0] = id;                /* v0 before a possible switch */
    reschedule(s, "StartThread");
    return id;
}

static void exit_current(struct ee_sched *s, int delete_it, const char *how)
{
    ee_ctx *c = s->c;
    thr_t *t = &s->t[s->cur];
    if (t->is_main)
        ee_unsupported(c, 0, "main thread exit not modelled");
    ee_trace(c, "\"ev\":\"thread_exit\",\"id\":%d,\"how\":\"%s\"", s->cur, how);
    t->status = EE_THS_DORMANT;
    if (delete_it)
        t->used = 0;
    reschedule(s, how);
    longjmp(t->top, 1);                   /* restarted later: begin at entry */
}

int32_t ee_k_exit_thread(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    (void)ret_set; (void)note; (void)n;
    exit_current(need(c, "ExitThread without scheduler"), 0, "ExitThread");
    return 0;
}

int32_t ee_k_exit_delete_thread(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    (void)ret_set; (void)note; (void)n;
    exit_current(need(c, "ExitDeleteThread without scheduler"), 1, "ExitDeleteThread");
    return 0;
}

int32_t ee_k_delete_thread(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    struct ee_sched *s = need(c, "DeleteThread without scheduler");
    int id = (int)A0;
    if (!valid_id(s, id) || s->t[id].status != EE_THS_DORMANT || s->t[id].is_main)
        ee_unsupported(c, 0, "DeleteThread on a thread that is not dormant");
    s->t[id].used = 0;
    snprintf(note, n, "id=%d", id);
    *ret_set = 1;
    return id;
}

int32_t ee_k_get_thread_id(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    (void)note; (void)n;
    *ret_set = 1;
    return c->sched ? c->sched->cur : c->k.cur_thread_id;
}

int32_t ee_k_refer_thread_status(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    struct ee_sched *s = need(c, "ReferThreadStatus without scheduler");
    int id = (int)A0 ? (int)A0 : s->cur;
    uint32_t p = A1;
    if (!valid_id(s, id))
        ee_unsupported(c, 0, "ReferThreadStatus of an unknown thread (error value not modelled)");
    thr_t *t = &s->t[id];
    if (p) {
        ee_w32(c, p + 0x00, (uint32_t)t->status);
        ee_w32(c, p + 0x04, t->entry);
        ee_w32(c, p + 0x08, t->stack);
        ee_w32(c, p + 0x0c, t->stack_size);
        ee_w32(c, p + 0x10, t->gp);
        ee_w32(c, p + 0x14, (uint32_t)t->init_prio);
        ee_w32(c, p + 0x18, (uint32_t)t->prio);
        ee_w32(c, p + 0x1c, t->attr);
        ee_w32(c, p + 0x20, t->option);
        ee_w32(c, p + 0x24, (uint32_t)t->wait_type);
        ee_w32(c, p + 0x28, (uint32_t)t->wait_id);
        ee_w32(c, p + 0x2c, (uint32_t)t->wakeup_count);
    }
    snprintf(note, n, "id=%d status=0x%x prio=%d stack=0x%08x size=0x%x; returns the status",
             id, t->status, t->prio, t->stack, t->stack_size);
    *ret_set = 1;
    return t->status;
}

int32_t ee_k_change_thread_priority(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    struct ee_sched *s = need(c, "ChangeThreadPriority without scheduler");
    int id = (int)A0 ? (int)A0 : s->cur;
    int prio = (int)A1;
    if (!valid_id(s, id) || prio < 0 || prio > 127)
        ee_unsupported(c, 0, "ChangeThreadPriority arguments outside the modelled range");
    int old = s->t[id].prio;
    s->t[id].prio = prio;
    c->k.cur_priority = (uint32_t)s->t[s->cur].prio;
    snprintf(note, n, "id=%d prio %d -> %d, returns old priority", id, old, prio);
    *ret_set = 1;
    c->r[2].sd[0] = old;
    if (c->r[3].sw[0] > 0)                /* i-variant never switches */
        reschedule(s, "ChangeThreadPriority");
    return old;
}

int32_t ee_k_rotate_ready_queue(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    struct ee_sched *s = need(c, "RotateThreadReadyQueue without scheduler");
    int prio = (int)A0;
    thr_t *cur = &s->t[s->cur];
    snprintf(note, n, "prio=%d", prio);
    *ret_set = 1;
    c->r[2].sd[0] = prio;
    if (cur->prio == prio) {
        int best = pick_ready(s);
        if (best > 0 && s->t[best].prio == prio) {
            cur->status = EE_THS_READY;
            cur->seq = ++s->seq;          /* to the back of its queue */
            switch_to(s, best, "RotateThreadReadyQueue");
        }
    }
    return prio;
}

int32_t ee_k_sleep_thread(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    struct ee_sched *s = need(c, "SleepThread without scheduler");
    thr_t *t = &s->t[s->cur];
    *ret_set = 1;
    if (t->wakeup_count > 0) {
        t->wakeup_count--;
        snprintf(note, n, "id=%d consumed a pending wakeup", s->cur);
        return s->cur;
    }
    t->status = EE_THS_WAIT;
    t->wait_type = EE_WAIT_SLEEP;
    snprintf(note, n, "id=%d sleeps", s->cur);
    reschedule(s, "SleepThread");
    return s->cur;
}

int32_t ee_k_wakeup_thread(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    struct ee_sched *s = need(c, "WakeupThread without scheduler");
    int id = (int)A0;
    if (!valid_id(s, id))
        ee_unsupported(c, 0, "WakeupThread of an unknown thread");
    thr_t *t = &s->t[id];
    if (t->status == EE_THS_WAIT && t->wait_type == EE_WAIT_SLEEP) {
        make_ready(s, id);
        snprintf(note, n, "id=%d woken", id);
    } else {
        t->wakeup_count++;
        snprintf(note, n, "id=%d not sleeping, wakeup count %d", id, t->wakeup_count);
    }
    *ret_set = 1;
    c->r[2].sd[0] = id;
    if (c->r[3].sw[0] > 0)                /* i-variant (negative number) never switches */
        reschedule(s, "WakeupThread");
    return id;
}

/* ---- semaphores ---- */

static void check_sema(ee_ctx *c, uint32_t id, const char *what)
{
    if (!id || id >= 256 || !c->sema[id].live)
        ee_unsupported(c, 0, what);
}

int32_t ee_k_create_sema(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    uint32_t p = A0;
    int id;
    for (id = 1; id < 256 && c->sema[id].live; id++) {}
    if (id == 256)
        ee_unsupported(c, 0, "semaphore capacity");
    c->sema[id].live = 1;
    c->sema[id].count = (int)ee_r32(c, p + 8);
    c->sema[id].max = (int)ee_r32(c, p + 4);
    c->sema[id].init = c->sema[id].count;
    c->sema[id].attr = ee_r32(c, p + 16);
    c->sema[id].option = ee_r32(c, p + 20);
    if (c->sema[id].count < 0 || c->sema[id].max < c->sema[id].count)
        ee_unsupported(c, 0, "invalid semaphore counts");
    snprintf(note, n, "id=%d count=%d max=%d init=%d attr=0x%x",
             id, (int)ee_r32(c, p), c->sema[id].max, c->sema[id].init, c->sema[id].attr);
    *ret_set = 1;
    return id;
}

static int waiters(const ee_ctx *c, uint32_t id)
{
    int k = 0;
    if (!c->sched)
        return 0;
    for (int i = 1; i < EE_MAX_THREADS; i++) {
        const thr_t *t = &c->sched->t[i];
        k += t->used && t->status == EE_THS_WAIT && t->wait_type == EE_WAIT_SEMA && t->wait_id == (int)id;
    }
    return k;
}

int32_t ee_k_delete_sema(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    check_sema(c, A0, "delete invalid semaphore");
    if (waiters(c, A0))
        ee_unsupported(c, 0, "DeleteSema with waiting threads (release not modelled)");
    c->sema[A0].live = 0;
    snprintf(note, n, "id=%d", (int)A0);
    *ret_set = 1;
    return (int32_t)A0;
}

int32_t ee_k_signal_sema(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    uint32_t id = A0;
    struct ee_sched *s = c->sched && c->sched->in_guest ? c->sched : NULL;
    check_sema(c, id, "signal invalid semaphore");
    *ret_set = 1;
    c->r[2].sd[0] = id;
    if (s) {
        int w = -1;
        for (int i = 1; i < EE_MAX_THREADS; i++) {
            thr_t *t = &s->t[i];
            if (t->used && t->status == EE_THS_WAIT && t->wait_type == EE_WAIT_SEMA
                && t->wait_id == (int)id && (w < 0 || t->wait_seq < s->t[w].wait_seq))
                w = i;
        }
        if (w > 0) {                      /* hand the count to the first waiter */
            make_ready(s, w);
            snprintf(note, n, "id=%u woke thread %d", id, w);
            if (c->r[3].sw[0] > 0)
                reschedule(s, "SignalSema");
            return (int32_t)id;
        }
    }
    if (c->sema[id].count == c->sema[id].max)
        ee_unsupported(c, 0, "semaphore overflow (error return not modeled)");
    c->sema[id].count++;
    snprintf(note, n, "id=%u count=%d", id, c->sema[id].count);
    return (int32_t)id;
}

int32_t ee_k_wait_sema(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    uint32_t id = A0;
    check_sema(c, id, "wait invalid semaphore");
    *ret_set = 1;
    if (c->sema[id].count > 0) {
        c->sema[id].count--;
        snprintf(note, n, "id=%u count=%d", id, c->sema[id].count);
        return (int32_t)id;
    }
    if (!c->sched || !c->sched->in_guest) {
        /* No scheduler: only a queued IOP reply can satisfy the wait. */
        while (!c->sema[id].count) {
            if (!c->sif || !c->sif->count || c->sif->in_handler ||
                !c->sif->enabled || !c->sif->chain ||
                !(c->cop0[EE_COP0_STATUS] & EE_STATUS_EIE))
                ee_unsupported(c, 0, "WaitSema deadlock: no deliverable IOP reply");
            ee_sif_interrupt_point(c);
        }
        c->sema[id].count--;
        snprintf(note, n, "id=%u count=%d (no scheduler)", id, c->sema[id].count);
        return (int32_t)id;
    }
    struct ee_sched *s = c->sched;
    thr_t *t = &s->t[s->cur];
    t->status = EE_THS_WAIT;
    t->wait_type = EE_WAIT_SEMA;
    t->wait_id = (int)id;
    t->wait_seq = ++s->wait_seq;
    ee_trace(c, "\"ev\":\"thread_wait\",\"id\":%d,\"sema\":%u", s->cur, id);
    reschedule(s, "WaitSema");
    /* Resumed: a SignalSema handed us the count. */
    c->r[2].sd[0] = id;
    snprintf(note, n, "id=%u blocked, then woken by SignalSema", id);
    return (int32_t)id;
}

int32_t ee_k_poll_sema(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    uint32_t id = A0;
    check_sema(c, id, "poll invalid semaphore");
    *ret_set = 1;
    if (c->sema[id].count > 0) {
        c->sema[id].count--;
        snprintf(note, n, "id=%u count=%d", id, c->sema[id].count);
        return (int32_t)id;
    }
    snprintf(note, n, "id=%u count=0; returns -1 (negative error, exact EE value not verified)", id);
    return -1;
}

int32_t ee_k_refer_sema_status(ee_ctx *c, int *ret_set, char *note, size_t n)
{
    uint32_t id = A0, p = A1;
    check_sema(c, id, "refer invalid semaphore");
    ee_w32(c, p + 0, (uint32_t)c->sema[id].count);
    ee_w32(c, p + 4, (uint32_t)c->sema[id].max);
    ee_w32(c, p + 8, (uint32_t)c->sema[id].init);
    ee_w32(c, p + 12, (uint32_t)waiters(c, id));
    ee_w32(c, p + 16, c->sema[id].attr);
    ee_w32(c, p + 20, c->sema[id].option);
    snprintf(note, n, "id=%u count=%d waiters=%d", id, c->sema[id].count, waiters(c, id));
    *ret_set = 1;
    return (int32_t)id;
}
