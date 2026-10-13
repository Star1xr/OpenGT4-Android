# Guest threads

Runtime: `src/ee/thread.c`, `src/ee/thread.h`. Tests: `tests/ee_unit/test_sched.c`
(no game data), `tests/initsys/run_test.py` (the game's own thread).

## Model

- Each PS2 thread runs on its own host fiber (Windows fibers; `ucontext`
  elsewhere, not yet built). A thread can block in the middle of a
  recompiled call chain and resume later.
- Registers (GPRs, HI/LO, HI1/LO1, SA, FPU) are saved per thread at each
  switch. COP0 Status is shared, not per thread (limitation).
- Priorities 0–127, lower wins, FIFO within a priority. A preempted thread
  goes back to the head of its queue; `RotateThreadReadyQueue` moves the
  caller to the back.
- Switches happen only where the kernel would switch: `StartThread`,
  `ChangeThreadPriority`, `WaitSema`, `SignalSema`, `SleepThread`,
  `WakeupThread`, `RotateThreadReadyQueue`, thread exit, and the return from
  an interrupt handler that readied a better thread. The `i` variants never
  switch. There is no time slicing, as on the PS2.
- When no thread can run, pending SIF interrupts are delivered (the kernel's
  idle loop takes interrupts regardless of the blocked thread's EIE). If
  nothing can make progress, the run ends as a deadlock.
- `CreateThread` writes the initial context at `stack + size - 0x2a0`, in the
  same layout as `SetupThread`: one 16-byte slot per GPR, `gp` at `+0x1c0`,
  `sp` and `fp` at `+0x1d0`/`+0x1e0` (top − 0x20), `ra` at `+0x1f0`.
  `StartThread` loads the thread's registers from that context, so changes
  the game makes to it before starting (as `0x005aede0` does) take effect.
- A new thread's return address is the root routine crt0 gave
  `SetupThread` (`0x00100228` here). The PCSX2 capture shows this value on
  the created thread's stack. A thread whose entry returns exits (dormant),
  and can be started again from its entry.
- Main thread: ID 1, initial priority 0 (**test input**).

## Checked against the PCSX2 capture

The `_InitSys` run now does what the capture showed
(`docs/analysis/THREAD_REFERENCE.md`): the created thread is dormant, is started, runs
when main lowers its priority to 1, blocks in `WaitSema`, and main resumes.

Memory after `_InitSys`, BSS `0x006d5e00–0x008a215c`, against the capture:
75 differing bytes, all classified:

| Bytes | Where | Why |
|-------|-------|-----|
| 28 | boot arguments `0x006d6380` | argc=2, `cdrom0:\CORE.GT4;1`, `hot` on PCSX2; the harness passes none (test input) |
| 46 | `0x00885bc0–0x00885e40` | the real kernel saves a blocked thread's registers on its own stack, below its SP (the thread descriptor's saved SP is `0x00885bc0`). Our scheduler keeps them outside guest memory. Kernel-private state |
| 1 | `0x00885ee0` | semaphore ID, 2 on PCSX2, 3 here. Compare by role |

The thread's own stack frames, which were missing before, now match.

## Not modelled

- Saving thread contexts into guest memory on a switch (see above).
- `SuspendThread`/`ResumeThread`, `TerminateThread`, `CancelWakeupThread`,
  event flags, alarms from the kernel side, and releasing waiters when a
  semaphore is deleted: each ends the run if reached.
- Error return values for invalid IDs: the run ends instead of returning a
  code whose exact value has not been checked. `PollSema` on zero returns -1
  (negative, exact EE value not verified).
- Timer interrupts: still never raised.
