/*
 * Scheduler tests without game data. Host C functions stand in for recompiled
 * guest code: they make kernel calls through ee_syscall like generated code
 * does, and log what they do so the order of execution can be checked.
 */
#include "ee.h"
#include "thread.h"

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

static char order[128];
static void mark(char ch)
{
    size_t n = strlen(order);
    if (n + 1 < sizeof order) { order[n] = ch; order[n + 1] = 0; }
}

static int32_t sys(ee_ctx *c, int num, uint32_t a0, uint32_t a1)
{
    c->r[3].sd[0] = num;
    c->r[4].sd[0] = (int32_t)a0;
    c->r[5].sd[0] = (int32_t)a1;
    ee_syscall(c, 0);
    return c->r[2].sw[0];
}

#define PARAM 0x00010000u   /* guest scratch for ThreadParam / SemaParam */
#define STACK 0x00020000u

static int32_t create_thread(ee_ctx *c, uint32_t entry, int prio)
{
    ee_w32(c, PARAM + 4, entry);
    ee_w32(c, PARAM + 8, STACK + (uint32_t)prio * 0x1000);
    ee_w32(c, PARAM + 12, 0x800);
    ee_w32(c, PARAM + 16, 0x006dddf0);
    ee_w32(c, PARAM + 20, (uint32_t)prio);
    return sys(c, 0x20, PARAM, 0);
}

static int32_t create_sema(ee_ctx *c, int init, int max)
{
    ee_w32(c, PARAM + 4, (uint32_t)max);
    ee_w32(c, PARAM + 8, (uint32_t)init);
    return sys(c, 0x40, PARAM, 0);
}

/* ---- guest threads ---- */

static int sema;

static void worker(ee_ctx *c)          /* guest address 0x1000 */
{
    mark('A');
    EXPECT(c->r[4].uw[0] == 0x1234, "worker got arg 0x%x", c->r[4].uw[0]);
    sys(c, 0x44, (uint32_t)sema, 0);    /* WaitSema: blocks */
    mark('B');
}                                       /* returns: thread exits */

static void sleeper(ee_ctx *c)         /* guest address 0x2000 */
{
    mark('s');
    sys(c, 0x32, 0, 0);                 /* SleepThread */
    mark('w');
}

static void spinner(ee_ctx *c)         /* guest address 0x3000 */
{
    mark('r');
    sys(c, 0x2b, 5, 0);                 /* RotateThreadReadyQueue(5) */
    mark('R');
}

static ee_fn lookup(uint32_t a)
{
    return a == 0x1000 ? worker : a == 0x2000 ? sleeper : a == 0x3000 ? spinner : NULL;
}

static void main_basic(ee_ctx *c)
{
    int id;
    sema = create_sema(c, 0, 1);
    id = create_thread(c, 0x1000, 0);
    mark('1');
    sys(c, 0x22, (uint32_t)id, 0x1234);    /* StartThread: equal priority, no switch */
    mark('2');
    sys(c, 0x29, 0, 1);                    /* lower own priority: worker runs */
    mark('3');
    sys(c, 0x42, (uint32_t)sema, 0);       /* SignalSema: worker (prio 0) preempts */
    mark('4');
    /* Worker returned, so it is dormant; it can be started again. */
    ee_w32(c, 0x9000, 0);
    sys(c, 0x30, (uint32_t)id, 0x9000);
    EXPECT(ee_r32(c, 0x9000) == EE_THS_DORMANT, "worker dormant after return, status 0x%x", ee_r32(c, 0x9000));
}

static void main_sleep(ee_ctx *c)
{
    int id = create_thread(c, 0x2000, 0);
    sys(c, 0x22, (uint32_t)id, 0);
    sys(c, 0x29, 0, 1);                    /* sleeper runs, sleeps */
    mark('m');
    sys(c, 0x33, (uint32_t)id, 0);         /* WakeupThread: preempts */
    mark('M');
}

static void main_rotate(ee_ctx *c)
{
    int id = create_thread(c, 0x3000, 5);
    sys(c, 0x29, 0, 5);                    /* same priority as spinner */
    sys(c, 0x22, (uint32_t)id, 0);         /* ready, equal priority: no switch */
    mark('x');
    sys(c, 0x2b, 5, 0);                    /* rotate: spinner runs */
    mark('y');
    sys(c, 0x2b, 5, 0);                    /* rotate again: spinner finishes */
    mark('z');
}

static void main_deadlock(ee_ctx *c)
{
    int s = create_sema(c, 0, 1);
    mark('d');
    sys(c, 0x44, (uint32_t)s, 0);          /* nobody can ever signal */
    mark('X');                             /* must not happen */
}

static int run(ee_fn entry, const char *name, const char *want, int want_reason)
{
    static ee_ctx c;
    order[0] = 0;
    if (ee_init(&c) != 0)
        return 1;
    c.lookup = lookup;
    c.cop0[EE_COP0_STATUS] = EE_STATUS_EIE;
    if (ee_sched_run(&c, entry) != 0) {
        printf("FAIL %s: scheduler did not start\n", name);
        return 1;
    }
    EXPECT(strcmp(order, want) == 0, "%s: order %s, expected %s", name, order, want);
    EXPECT(c.end_reason == want_reason, "%s: end reason %d (%s), expected %d",
           name, c.end_reason, c.end_detail, want_reason);
    ee_sched_free(&c);
    ee_free(&c);
    return 0;
}

int main(void)
{
    /* 1: StartThread does not preempt an equal priority; lowering the caller's
       priority runs the new thread; WaitSema blocks it; SignalSema from a lower
       priority thread preempts; a returning thread exits. */
    run(main_basic, "basic", "12A3B4", EE_END_RETURNED);
    run(main_sleep, "sleep", "smwM", EE_END_RETURNED);
    run(main_rotate, "rotate", "xryRz", EE_END_RETURNED);
    /* A wait nothing can satisfy ends the run instead of hanging. */
    run(main_deadlock, "deadlock", "d", EE_END_UNSUPPORTED);
    printf(fails ? "%d failure(s)\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
