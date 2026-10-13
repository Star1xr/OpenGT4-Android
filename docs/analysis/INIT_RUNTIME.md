# Second native execution prototype

The recompiled NTSC-U SCUS-97328 v2.00 code runs from `0x00100008` through
`crt0`, `_InitSys`, `FlushCache(0)` and `_init`, stopping at `0x00100204` before
`main`. This is an internal execution experiment, not a playable port or a
verified replacement for the PS2. Reference comparison is still pending.

## Build and run

Needs the local `extracted/core.bin`, splat's `splat/asm/core/text.s`, Python
with rabbitizer (the existing `.venv`), CMake and VS 2022 Build Tools:

```powershell
cmake -S . -B build -DOPENGT4_RECOMP_TESTS=ON
cmake --build build --config Release --target init_run initsys_run ee_unit_tests ee_sif_tests ee_recomp_tests
ctest --test-dir build -C Release --output-on-failure
.venv/Scripts/python tests/init/run_test.py
.venv/Scripts/python tests/initsys/run_test.py
```

The init runner accepts `<core.bin> <existing-output-folder> <fresh:0|1>
<retail|tool|missing> <delay-points:0..1000>`. All generated C, traces and guest
memory dumps stay in ignored `build/`. No BIOS is required by this harness.

## What is executed

The game's EE code does heap initialization, SIF initialization, RPC binding,
FILEIO negotiation and the open/read/close calls. Replies go through the
registered channel-5 handler and its command callbacks, all recompiled from
the local game image. The HLE does not directly fill `ROMVER`, client fields
or semaphore results to bypass those handlers.

`src/ee/sif.c` implements a bounded IOP memory/transport model and just the
FILEIO operations needed here. `docs/analysis/INIT_IOP.md` records the protocol evidence.
For short reads, the EE handler copies the leading fragment out of the
completion record. Open/close records have 0x20 bytes and read records 0xa4;
the two completion slots alternate. Unsupported requests and invalid DMA
bounds stop the run.

Semaphores now have lifetime, initial/max counts, signal and wait semantics.
They do not implement a general guest-thread scheduler. An empty wait can
only make progress through queued SIF replies; if no reply can be delivered,
it stops as a deadlock instead of pretending success. Invalid IDs and overflow
also stop rather than inventing a PS2 error code.

The recompiler adds indirect calls through the generated address lookup,
computed jumps restricted to the current function, and PLZCW. PLZCW counts
leading sign bits excluding the sign bit in the two low 32-bit lanes, leaving
the upper 64 bits unchanged. Semantics were cross-checked with the
[PCSX2 instruction implementation](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/MMI.cpp);
our emitted implementation uses an independent bit-count loop. Synthetic
instruction tests cover lane values, source/destination aliasing, jump/call
delay slots, and an invalid jump target.

The observed `cache` operation 0x18 has no data-cache work to do in this
cacheless memory model; each use is traced. Other cache operations stop.
Previously copied kernel code is still hash-checked before dispatch.

## Test inputs and scheduling limits

- `0000A000000000` and `0000T000000000` are artificial 14-byte ROMVER fixtures,
  not BIOS files or measured console values. The missing-file fixture returns
  an asynchronous negative open result. File handles, SIF addresses and IDs
  are chosen by the HLE.
- The original crt0 test assumptions still apply: initial CPU state,
  `SetupThread` result/argc, console settings and kernel-table address.
- The IOP-ready bit is assumed already set in the fresh initialization case.
  This does not model IOP boot latency.
- Each reply waits in a FIFO. Delivery occurs at a syscall return, `ei`,
  a generated branch-target label, or a blocked `WaitSema`. EIE, channel enable
  and receive-chain state gate delivery; nested dispatch is suppressed. The
  interrupted CPU registers are restored after the handler while its memory
  and service effects remain. This approximates kernel interrupt dispatch;
  it does not execute the BIOS exception entry or model full COP0 masking.
- Delays count eligible safe points, not CPU cycles. Branch-label polling
  allows late RPC-init replies to arrive during the guest's syscall-free loop.
- Timer interrupts and created guest threads still do not execute. Direct
  command/data DMA is modeled, not general EE/IOP DMA hardware or IOP execution.
- Only one ROMVER file at a time and reads up to 14 bytes are supported. No
  arbitrary filesystem access, bulk file DMA, server discovery retries or
  failed DMA submission/retry behavior is implemented.
- Syscall registration 0x12c remains a reported limitation. Calling it stops.

## Evidence from tests

`tests/init/run_test.py` runs 12 combinations twice: existing/fresh SIF state,
retail/TOOL/missing ROMVER, and reply delays of 0/7 points. Every combination
must reach `0x00100204` with reproducible trace, registers and memory. It checks
stack/gp restoration, unchanged program text and copied kernel blocks,
allocator initialization, guest-written ROMVER, FILEIO version/slot mode,
RPC sequence, alternating completion slots and sizes, interrupt delivery,
no pending responses/open file, and the documented unknown syscall.

The missing-file case issues `0xff, 0, 0`: two failed opens, no reads or closes.
It follows the retail OSD path. The TOOL fixture performs fewer OSD reads.
The final timezone variable is not used as an independent oracle: interpreting
it remains provisional and no hardware/reference value has been captured.

`ee_sif_tests` checks deferred delivery while EIE is off, configured delay,
non-reentrancy, preservation of CPU state, semaphore counts and deadlock,
invalid IDs, DMA bounds and D_STAT acknowledgement. `ee_recomp_tests` uses
synthetic instructions only. The previous `_InitSys` regression still passes.

These checks establish reproducibility and consistency with the inspected
protocol, not PS2-equivalent timing or behavior. PCSX2 capture remains a
separate item in `REFERENCE_CAPTURE.md`.

## Next boundary

Trace `main` and its constructor path before choosing the next executable
chunk. Inventory newly reached instructions, indirect calls and IOP services;
do not extend the HLE with arbitrary successful replies merely to get farther.
