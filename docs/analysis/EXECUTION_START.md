# Execution start

What runs from the entry point to `main`, and what a host has to provide for
the first recompiled code to run. Disc: NTSC-U SCUS-97328 v2.00.

Method: static call graph from each call in crt0, following direct `jal`
only. For each subtree, count syscalls (by stub table name), accesses to fixed
hardware addresses (`lui` + load/store), and COP0, FPU, VU0 and MMI
instructions. Names and statuses: `docs/analysis/SYMBOLS.md`.

**What this does not see**: indirect calls (`jalr`), and functions reached
only as data, such as the entry of a thread started with `CreateThread`, an
interrupt handler passed to `AddIntcHandler2`, or `AddDmacHandler`. The
subtrees below have 0 `jalr`, but they do register callbacks.

## Sequence

```
_start 0x00100008                 crt0
  clear 128-bit GPRs (padduw)
  clear bss 0x006d5e00-0x008a215c (sb, then sq)
  syscall 0x3c SetupThread(gp=0x006dddf0, stack=0x01ff8000, size=0x8000, args=0x006d6380, root=_root)
  syscall 0x3d SetupHeap(0x008a215c, -1)
  _InitSys        0x005b7560      45 functions
  FlushCache(0)
  ei
  _init           0x0048ef90      72 functions, starts SIF
  main(argc, argv) 0x00107f08     2110 functions reachable directly
    __main -> __do_global_ctors   370 constructors, via jalr
    func_00107E88(&argc, argv)
    func_00108040()
    func_00107ED8()
    return 0
  Exit(v0)        0x005a3140
```

`main` itself is 3 calls after `__main`. What those three do has not been
looked at yet.

## Needs per stage

### crt0 alone

| Need | Detail |
|------|--------|
| Memory | 0x00100000–0x008a215c (text, data, bss), stack 0x01ff8000–0x02000000 (`SetupThread` base + size) |
| CPU | 128-bit GPRs: `padduw` to clear registers, `sq` to clear bss |
| Syscalls | `SetupThread` (0x3c), `SetupHeap` (0x3d), both called directly from crt0, not through the stub table |

### _InitSys (0x005b7560)

| Need | Detail |
|------|--------|
| Syscalls (static list) | `CreateThread`, `StartThread`, `GetThreadId`, `ReferThreadStatus`, `ChangeThreadPriority`, `CreateSema`, `DeleteSema`, `AddIntcHandler2`, `_EnableIntc`, `GetOsdConfigParam`, `SetOsdConfigParam`, `FlushCache` |
| Syscalls missed by the static list | `SetSyscall` (0x74) and syscalls the game installs itself: 0x83, 0x5a, 0x5b (see "What running it showed") |
| Hardware | timer 0 COUNT/MODE (0x10000000, 0x10000010), timer 2 COUNT/MODE (0x10001000, 0x10001010, also through the uncached 0xb0001000–0xb0001020), timer 3 MODE (0x10001810) |
| COP0 | `mfc0 Status` (5), `di`, `ei` |
| Other | MMI `pcpyh`, `pcpyld`; 2 `sync` |
| Callbacks | a thread (`CreateThread` in 0x005aea78) and an interrupt handler (`AddIntcHandler2` in 0x005b7bb8, then `_EnableIntc`). Their bodies are not in this count |

No IOP, GS, FPU or VU0 use in the statically reachable code. The registered thread and interrupt handler are not included, so this still has to be confirmed at run time.

### _init (0x0048ef90)

Everything in `_InitSys` (it shares the timer setup at 0x005b7f08 and
0x005b8400), plus:

| Need | Detail |
|------|--------|
| Syscalls | `AddDmacHandler`, `_EnableDmac`, `SignalSema`, `WaitSema`, `GetOsdConfigParam2`, `sceSifSetReg`, `sceSifGetReg`, `sceSifSetDChain`, `sceSifSetDma`, `isceSifSetDma` |
| Hardware | DMA channel 5 SIF0 (0x1000c000), `D_STAT` (0x1000e010) |
| IOP | **yes**. 0x005b08c8 sets up SIF (registers, DMA chain, DMA handler). Above it, several functions block on `WaitSema` for replies from the IOP (RPC). Something has to answer them. Now analysed in `docs/analysis/INIT_IOP.md`: one service, fileio (`0x80000001`, functions 0xFF, 0, 2, 1) to read `rom0:ROMVER`, answered on a console by the `FILEIO` module in `IOPRP300.IMG`. The plan is HLE of that service |

### main (0x00107f08), for scale

Reachable directly: 2110 functions, 346 `jalr`, 1858 FPU instructions,
scratchpad (0x70000000), GS privileged registers, VU memory, IOP/SBUS
addresses, 23 syscalls including `SleepThread`/`WakeupThread` and
`Deci2Call`. The 370 constructors called from `__main` are not in this count.

## What running it showed

The first recompiled run found things the static analysis missed:

- **Lone syscall stubs.** `_InitSys` calls `SetSyscall` (0x74) through stubs
  outside the libkernl table, which the analysis only counted as unnamed.
- **libkernl patches the kernel.** Two routines, `0x005b7450`/`0x005b7a40` and
  `0x005b9928`, each:
  1. install their own handlers for syscalls 0x83 (`FindAddress`) and 0x5a
     (`Copy`) with `SetSyscall`;
  2. search kernel memory `0x80000000–0x80080000` for those entries to find
     the kernel syscall table (stored at `0x00658360`);
  3. copy code from `.data` into kernel memory: 0x740 bytes from
     `0x00658b48` to `0x80076000`, 0x330 bytes from `0x00659318` to
     `0x80075000`, 0x28 bytes from `0x00659288` to `0x00082000`;
  4. point syscall 0x5b at the copied code and use it to look up handlers
     for 0xfc, 0xfe, 0xfd, 0xff, 0x12c and 0x8 (the alarm calls), then
     install those with `SetSyscall`.
  The copy at `0x80076000` is gated on bit 0x100 of `T3_MODE`.
- **Kernel-mode stores.** `0x005b7b18` raises `ERL` in Status, stores a word
  (timer 2 registers through `0xb000xxxx`), sets `ErrorEPC = $ra` and
  executes `eret` to return.
- `StartThread` and `DeleteSema` are absent from the synthetic trace.
  This is not proof of absence on the reference: a missing created-thread
  status bypasses StartThread natively. See [thread audit](THREAD_REFERENCE.md).

How the runtime handles these:

| Item | How | Status |
|------|-----|--------|
| `SetSyscall` 1..255 | writes the handler into a table at `0x80070000` (kernel memory, our address) and dispatches that number to the recompiled handler | implemented; table address is ours |
| Game syscall handlers | recompiled; found as data references and from the patch tables (`--data-refs`, `--extra-roots`) | implemented |
| Code copied into kernel memory | recompiled from its source bytes for the address where it runs (`--blob SRC:DST:SIZE`); function extents by control flow | implemented |
| `eret` | allowed only as a return to `$ra`; anything else ends the run | implemented for this use |
| `SetSyscall` outside 1..255 (seen: `0x12c`, handler `0x80076488`) | **known limitation.** What the real kernel does with such a number is unknown (table size, masking, error). The HLE does not write anything, does not truncate the number, records the registration with trace status `unhandled` and in `summary.json` `limitations`, and ends the run if the game later calls that number | unhandled, reported |
| Copied code | before each call into code the game copied, the bytes in guest memory are hashed and compared with the bytes it was recompiled from; a mismatch ends the run | implemented, traced as `code_check` |

## First recompiled chunk

Proposal: crt0 + `_InitSys`, stopping before `_init`. It is the largest piece
that needs no IOP.

Host side for it:

1. Load `core.bin` at 0x00100000 and reserve memory up to 0x008a215c, plus
   the stack.
2. A 128-bit register file (for `padduw`/`sq` and the MMI ops used).
3. HLE for the 14 syscalls above (the 12 listed plus `SetupThread` and
   `SetupHeap`). The thread and semaphore calls need at least a thread
   table, since `_InitSys` creates and starts a thread.
4. Timer registers 0, 2 and 3 as memory-mapped I/O, reads and writes
   recorded, and the uncached mirror 0xb0001000 mapped to the same timer.
5. COP0 Status reads and `di`/`ei` (an interrupt-enable flag is enough at
   first).
6. A stop point: return to the host when crt0 reaches the `FlushCache(0)`
   call at 0x001001f0, or trap `_init`.

### Test for this chunk

Matching the order of kernel calls is only a first check, not full
validation. The test records and checks:

1. **Kernel calls**: order, arguments, and the value returned to the game for
   each one.
2. **Memory**: a diff of game memory after the run against a known reference
   (bss cleared, the variables `_InitSys` writes, heap and thread setup).
3. **End point**: the run stops at the chosen address (the `FlushCache(0)`
   call at 0x001001f0) with the expected registers, and did not stop
   anywhere else.
4. **Hardware**: every timer register read and write, with values.
5. **Threads and interrupts, defined explicitly**. The static analysis does
   not follow them, so the test must state what the host does:
   - `CreateThread` / `StartThread`: whether the new thread runs, when, and
     with what priority rules. First version: record it and do not run it,
     and say so in the output.
   - `AddIntcHandler2` / `_EnableIntc`: whether the handler is ever called.
     First version: record it, never raise the interrupt.
   - `di` / `ei` and COP0 Status: what value reads return.

The reference values for 1–4 have to come from somewhere independent of
the recompiled code, for example a run of the same range in an existing
emulator or a hand-checked trace. Until then, the test only shows the run is
reproducible, not that it is correct.

Each service in the run is labelled as **implemented** (behaves like the
real one for this test) or **stubbed** (returns a fixed value or only
records the call). The test output lists both.

### Current state

`tests/initsys/run_test.py` runs the chunk twice and passes:

- reproducible: identical trace, registers and memory;
- ends at `0x001001f0`; `gp` and `sp` as crt0 set them; program text
  unchanged; the three kernel copies match their source bytes;
- 100 trace events: 54 syscalls (37 handled by the HLE, 18 of them
  `SetSyscall`; 17 dispatched to game handlers), 12 copied-code checks
  (one before each of the 12 calls into kernel-memory code, all matching),
  6 timer accesses, 9 COP0 events (`di`, `ei`, `eret`).
- one limitation recorded: `SetSyscall(0x12c)` (see above).

Services in this run:

| Implemented | Stubbed |
|-------------|---------|
| `FlushCache` (see below), `SetSyscall` 1..255, dispatch to game syscall handlers, copied-code check, `eret` as return, `di`/`ei` on the EIE bit, memory map (RAM mirrors, scratchpad), unaligned loads/stores (unit-tested) | `SetupThread`, `SetupHeap`, `CreateThread`, `ReferThreadStatus`, `GetThreadId`, `ChangeThreadPriority`, `CreateSema`, `AddIntcHandler2`, `_EnableIntc`, `GetOsdConfigParam`, `SetOsdConfigParam`, timers |

Unhandled and reported: `SetSyscall` numbers outside 1..255.

**FlushCache.** It does nothing in the runtime, and that is only correct
under two conditions:

1. The runtime has no caches: every write to guest memory is visible to the
   next read at once, so writing back or invalidating the data cache
   (`FlushCache(0)`) has nothing to do.
2. Copied code never runs from the bytes in guest memory. It runs as C
   recompiled ahead of time from the source bytes in `core.bin`. So an
   instruction-cache invalidate (`FlushCache(2)`) has nothing to invalidate,
   **as long as** the copied bytes are exactly the ones that were
   recompiled. The runtime checks this before every call into copied code
   (`code_check`), and ends the run on a mismatch (unit-tested).

What this does not cover: code that is modified or generated at run time
in a way that does not match a `--blob` given to the recompiler. Such code
has no recompiled version; a call into it ends the run as "not recompiled".
If the game ever relies on stale instruction-cache contents, this runtime
would not reproduce it. Nothing seen so far does.

### Test inputs

These values are chosen for the test. They are **not** claimed to be what a
PS2 has, and are listed in `summary.json` under `test_inputs`:

| Input | Value | Where it matters |
|-------|-------|------------------|
| COP0 Status at entry | `0x70030c13` | read by `_InitSys` (`mfc0 Status`), changed by `di`/`ei`/`eret` |
| GPRs at entry | all zero | crt0 clears them anyway |
| `SetupThread` return (`sp`) | `stack + size` = `0x02000000` | every stack address in the run |
| `argc` written by `SetupThread` | 0 | read by crt0 for `main`; not used before the stop |
| `GetOsdConfigParam` value | `0x00010000` | written to game memory by `_InitSys` |
| HLE kernel syscall table | `0x80070000` | the game stores it at `0x00658360` |

The reference capture gives the real values; each then gets compared by role
or adopted as the input, as `docs/analysis/REFERENCE_CAPTURE.md` explains.

Validation against a real system is **pending**: see `docs/analysis/REFERENCE_CAPTURE.md`
for the PCSX2 capture and what cannot be compared directly.

Build and run:

```
cmake -S . -B build -DOPENGT4_RECOMP_TESTS=ON
cmake --build build --config Release --target initsys_run ee_unit_tests
build/Release/ee_unit_tests.exe
.venv/Scripts/python tests/initsys/run_test.py
```

### After this chunk

crt0 + `_InitSys` + `_init`, stopping at `0x00100204`. The IOP services,
the data sent and the expected replies are in `docs/analysis/INIT_IOP.md`, which
also lists the delivery policy and pass criteria for that test.

## Executed follow-on chunk

The `crt0 + _InitSys + _init` prototype now reaches `0x00100204` with the
bounded SIF/fileio HLE. See `INIT_RUNTIME.md` for build commands, the 12-scenario
regression matrix, and scheduling/test-input limitations. Unlike the original
`_InitSys` harness, this runner executes channel-5 DMAC handlers; timer/INTC
handlers and guest threads remain unexecuted. Reference validation is pending.
