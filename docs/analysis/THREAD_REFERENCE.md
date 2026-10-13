# Thread and boot-state audit, 2026-09-26

Evidence: the corrected, ignored `captures/alias_20260926-190228` snapshots,
their debugger screenshots, the native trace, and local disassembly of the
user's game and captured USA 2.30 kernel. No BIOS code or captured bytes are
included here. Addresses below identify observations, not portable BIOS APIs.

## Corrected: explicit SetupThread stack reservation

The captured syscall table points SetupThread (0x3c) to `0x80005190`.
For an explicit stack, it computes the top from base plus size, reserves
`0x2a0` bytes, and returns the reserved context address. The relevant
arithmetic is at `0x800051e0..0x800051f4`; the return is at `0x80005270`.
Four word stores initialize the context:

| Offset from returned context | Value |
| --- | --- |
| `0x1c0` | supplied GP |
| `0x1d0` | stack top minus `0x20` |
| `0x1e0` | stack top minus `0x20` |
| `0x1f0` | supplied root routine |

The runtime now applies this bounded behavior for explicit 32 MiB RAM
stacks, rejecting insufficient sizes, overflow and automatic allocation.
It leaves other context bytes alone. Synthetic argc=0, main ID/priority and
missing scheduling remain limitations, so SetupThread is still marked
stubbed rather than fully implemented. The behavior is supported by the
captured USA 2.30 kernel; other BIOS profiles have not been validated.

For this game SP now equals the reference `0x01fffd60`, rather than
`0x02000000`. All four context words also match the reference at exit.
Raw stack differing bytes decrease from 7,008 to 6,951; matching SP alone
does not make stack contents equivalent. The initial bootstrap stack,
kernel scratch state and additional execution differ.

## Confirmed control-flow defect: created thread status is not supplied

The old claim that no thread starts before the boundary was based on the
synthetic run and was incorrect for the reference.

1. Game routine `0x005aea78` creates a thread starting at `0x005ae9a0`,
   with stack `0x00885ae0`, size `0x400`, GP `0x006dddf0` and priority 0.
2. It calls helper `0x005aede0`. That helper requests ReferThreadStatus at
   `0x005aee18`, then tests the returned status against `0x10` (dormant)
   at `0x005aee40`.
3. For a dormant thread it prepares the saved context, restores interrupts,
   and calls StartThread at `0x005aef38` with argument `0x00885ee8`.
4. The native `k_refer_thread_status` only supports the main thread. For
   the newly created thread it leaves the output buffer untouched but
   returns success. In the current zero-initialized harness this bypasses
   the dormant branch, so StartThread is absent from the native trace.

The corrected exit kernel snapshot contains thread ID 4's descriptor at
`0x8001ad30` (table base `0x8001ac00`, stride `0x4c`). It has status 4,
saved SP `0x00885bc0`, wait-related values of 2, thread function
`0x005ae9a0`, and argument `0x00885ee8`. The saved context/stack includes
return address `0x005ae9e8`, the instruction immediately after the thread's
WaitSema call at `0x005ae9e0`. This supports that the thread ran and blocked,
rather than only having an initial dormant context prepared.

The 34 differing bytes in this thread's stack cannot be dismissed as unused
kernel initialization. Do not copy the observed final stack into the HLE or
claim the native path matches just because both runs reach the same stop PC.

Fixing this requires a bounded thread model as a unit: retain created-thread
metadata, report dormant state, build context, start the recompiled entry,
block on an empty semaphore, and resume the caller while preserving CPU
state. Later signaling/resumption must either work or fail explicitly.
The existing StartThread stub also does not execute a thread. Merely fixing
ReferThreadStatus would expose that next omission without resolving it.
This audit fixes SetupThread; scheduler implementation remains outstanding.

## Remaining differences and their interpretation

| Area | Evidence and classification |
| --- | --- |
| 28 BSS bytes in argument area | Reference argc=2 and bootstrap strings; native fixture argc=0. Host boot inputs, not constants to copy globally. |
| 34 BSS bytes in created-thread stack | Missing native thread execution, described above. |
| 1 BSS byte at `0x00885ee0` | Semaphore ID: reference 2, synthetic allocator 3. Compare by role. |
| 5 pre-BSS bytes | Thread ID, two semaphore IDs, and kernel table pointer; allocator/address choices. |
| SP | Corrected using stack bounds and reserved context size. |
| `at`, `t0..t8`, `k0`, LO | Kernel scratch/initial-state effects are plausible, but exact producers have not all been traced. Do not force captured values. |

Future boot-argument support should accept bootstrap inputs and construct
argc/argv in guest memory, with bounds and pointer validation. It must not
import final captured RAM as if the runtime produced it.

## Verification

Four CTest suites pass, including new synthetic SetupThread tests for two
stack locations, the minimum size, context stores, untouched bytes, undersized
stacks, RAM overrun, integer wraparound and automatic-allocation rejection.
The `_InitSys` reproducibility checks and all 12 `_init` scenarios (twice
each) pass. These remain internal regressions, not scheduler validation.
