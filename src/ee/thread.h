/*
 * Guest threads for recompiled code.
 *
 * Each PS2 thread runs on its own host fiber, so a thread can block in the
 * middle of a recompiled call chain and resume later. Scheduling follows the
 * EE kernel's rules as far as they are modelled: fixed priorities (lower
 * number wins), no time slicing, and a switch only where the kernel would
 * switch: StartThread, ChangeThreadPriority, WaitSema/SignalSema,
 * SleepThread/WakeupThread, RotateThreadReadyQueue, thread exit, and the
 * return from an interrupt handler that readied a higher-priority thread.
 * See docs/analysis/THREADS.md for what is and is not modelled.
 */
#ifndef OPENGT4_EE_THREAD_H
#define OPENGT4_EE_THREAD_H

#include "ee.h"

#define EE_MAX_THREADS 64

/* Thread status values, as in ee_thread_status_t. */
#define EE_THS_RUN     0x01
#define EE_THS_READY   0x02
#define EE_THS_WAIT    0x04
#define EE_THS_SUSPEND 0x08
#define EE_THS_DORMANT 0x10

/* Wait types. */
#define EE_WAIT_NONE  0
#define EE_WAIT_SLEEP 1
#define EE_WAIT_SEMA  2

/* Return address for new threads when SetupThread gave no root routine
   (our address). Normally the root routine is used, as the kernel does. */
#define EE_HLE_THREAD_EXIT 0x80001000u

/* Start the scheduler with the main thread (ID 1) running `entry`.
   Returns when the run ends (ee_end) or when the main thread's entry returns.
   Returns 0 on success, -1 if fibers could not be created. */
int  ee_sched_run(ee_ctx *c, ee_fn entry);
void ee_sched_free(ee_ctx *c);
int  ee_sched_active(const ee_ctx *c);
/* Called by ee_end from inside a guest thread: never returns. */
void ee_sched_exit_to_host(ee_ctx *c);
/* After an interrupt handler: switch if it readied a higher-priority thread. */
void ee_sched_after_interrupt(ee_ctx *c);
/* Main thread bookkeeping from SetupThread. */
void ee_sched_setup_main(ee_ctx *c, uint32_t gp, uint32_t stack, uint32_t size);

/* Kernel calls (same signature as the other HLE calls in syscall.c). */
typedef int32_t (*ee_kcall)(ee_ctx *c, int *ret_set, char *note, size_t n);
int32_t ee_k_create_thread(ee_ctx *c, int *ret_set, char *note, size_t n);
int32_t ee_k_delete_thread(ee_ctx *c, int *ret_set, char *note, size_t n);
int32_t ee_k_start_thread(ee_ctx *c, int *ret_set, char *note, size_t n);
int32_t ee_k_exit_thread(ee_ctx *c, int *ret_set, char *note, size_t n);
int32_t ee_k_exit_delete_thread(ee_ctx *c, int *ret_set, char *note, size_t n);
int32_t ee_k_get_thread_id(ee_ctx *c, int *ret_set, char *note, size_t n);
int32_t ee_k_refer_thread_status(ee_ctx *c, int *ret_set, char *note, size_t n);
int32_t ee_k_change_thread_priority(ee_ctx *c, int *ret_set, char *note, size_t n);
int32_t ee_k_rotate_ready_queue(ee_ctx *c, int *ret_set, char *note, size_t n);
int32_t ee_k_sleep_thread(ee_ctx *c, int *ret_set, char *note, size_t n);
int32_t ee_k_wakeup_thread(ee_ctx *c, int *ret_set, char *note, size_t n);
int32_t ee_k_create_sema(ee_ctx *c, int *ret_set, char *note, size_t n);
int32_t ee_k_delete_sema(ee_ctx *c, int *ret_set, char *note, size_t n);
int32_t ee_k_signal_sema(ee_ctx *c, int *ret_set, char *note, size_t n);
int32_t ee_k_wait_sema(ee_ctx *c, int *ret_set, char *note, size_t n);
int32_t ee_k_poll_sema(ee_ctx *c, int *ret_set, char *note, size_t n);
int32_t ee_k_refer_sema_status(ee_ctx *c, int *ret_set, char *note, size_t n);

#endif /* OPENGT4_EE_THREAD_H */
