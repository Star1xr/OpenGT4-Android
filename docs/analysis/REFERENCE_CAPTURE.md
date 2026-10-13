# Reference capture with PCSX2

Goal: record, on PCSX2, the same state our recompiled crt0 + `_InitSys` run
produces, so the two can be compared. Status: **preliminary entry and exit
snapshots captured on 2026-09-26 through PINE.** See
[comparison results and limitations](REFERENCE_COMPARISON.md). Full tracing
and equivalence validation remain pending. The inventory below describes
the earlier setup; the selected BIOS and actual capture settings are recorded
in the comparison report.

Capture correction: old reads below `0x80000` returned zeros because they
used low virtual addresses. They cannot validate kernel patches or the
syscall table. Format-2 captures read RAM via `0x80000000 + physical_offset`
and retain physical filenames. This path was validated on PCSX2 2.5.165 in
the corrected two-point recapture: all three kernel blocks match the native
baseline. Full tracing and CPU equivalence are still pending.

## What is needed

- The same disc image: NTSC-U SCUS-97328 v2.00, CRC32 `831f9f84`
  (`python tools/verify_iso.py <iso>` must say OK).
- A PS2 BIOS dumped from your own console, kept outside this repository.
  None is included here. The captures so far used a USA 2.30 BIOS
  (identified from its ROMDIR entries).
- PCSX2. The captures so far used version **2.5.165.0**. Record the actual
  version and configuration (a copy of `PCSX2.ini`) with every capture.

## Settings

Correction (2026-09-26): do not disable the EE recompiler in the installed
standard v2.5.165 build when relying on execution breakpoints. In that tag's
`pcsx2/Interpreter.cpp`, `execI()` only checks breakpoints under `EXTRA_DEBUG`
or `PCSX2_DEVBUILD`. The interpreter experiment ran past the configured
breakpoints. The interpreter recommendations below require a suitable
development build; the standard recompiler already stopped correctly in the
preliminary capture. Trace macros also require `PCSX2_DEVBUILD`; changing
INI switches alone cannot activate them in the standard build. Stable
2.8.2 retains both restrictions. First recapture with the corrected tool
and original recompiler configuration. The logging settings below are for
a development build only if a full trace subsequently proves necessary.
Source: https://github.com/PCSX2/pcsx2/blob/v2.5.165/pcsx2/Interpreter.cpp

Record the exact settings used. The easiest way: copy `Documents/PCSX2/inis/PCSX2.ini`
into the capture folder as `settings.ini` after the capture.

Recommended for the capture (change back afterwards if you like):

| Setting | Value | Why |
|---------|-------|-----|
| Boot | normal boot of the disc through the BIOS | same path as a console. Note whether Fast Boot was on (`EnableFastBoot`); both boot through the BIOS kernel, but Fast Boot skips the BIOS menu |
| `EmuCore/CPU/Recompiler` `EnableEE`, `EnableIOP` | Keep `true` on the installed standard build | Interpreter breakpoints require development/debug support in v2.5.165; verify tracing separately |
| `EnableFastmem` | `false` | every memory access goes through the logged path |
| `EmuCore/TraceLog` `Enabled` | `true` | |
| `EE.bios` | `true` | kernel calls (syscalls) |
| `EE.counters` | `true` | timer activity |
| `EE.knownhw` | `true` | reads and writes to known hardware registers, timers included |
| `Logging` `EnableFileLogging` | `true` | trace goes to `logs/emulog.txt` |
| PINE (`EnablePINE`) | `true`, slot 28011 | memory capture tool |
| Game fixes, speed hacks | defaults; record them | |

The trace log format has not been parsed by our tools yet. The parser gets
written from the first real log, not guessed.

## Capture points

Set execution breakpoints in the PCSX2 debugger:

1. **Entry**: `0x00100008` (`_start`). The bootstrap at `0x01000000` loads
   `CORE.GT4` first, so the breakpoint only hits after the load.
2. **After `_InitSys`**: `0x001001f0` (the `jal FlushCache` right after
   `_InitSys` returns). This is where our run stops.

At each point, while paused:

- Memory: `python tools/pcsx2_capture.py --out captures/<run>/entry` (or
  `.../initsys_exit`). It reads `0x00000000Ã¢â‚¬â€œ0x00100000`,
  `0x00100000Ã¢â‚¬â€œ0x008a215c` and `0x01ff8000Ã¢â‚¬â€œ0x02000000` (same names as our
  dumps), the timer registers, and what PCSX2 reports about itself and the
  game.
- Registers: PINE cannot read them. Copy them from the debugger's register
  view into `regs.txt`, one per line: `name = 0x...` for the 32 GPRs (all 128
  bits), `hi`, `lo`, `hi1`, `lo1`, `sa`, and COP0 `Status`, `EPC`,
  `ErrorEPC`.
- After both points: copy `logs/emulog.txt` and `inis/PCSX2.ini` into
  `captures/<run>/`.

`captures/` is git-ignored: it holds game memory.

## Comparing: what will differ by design

Do not fix these as constants. Compare them by role.

| Item | Our test input | On PCSX2 | Compare as |
|------|------|----------|------------|
| Thread, semaphore, handler IDs | 1, 2, ... (our allocator) | kernel's | same number of calls, IDs consistent within each run |
| `SetupThread` result (`sp`) | `stack + size` = `0x02000000` (chosen, not a PS2 value) | kernel's | record the real value; then decide whether the HLE should match it |
| `SetSyscall(0x12c, ...)` | not modelled, reported as a limitation | unknown | see what the real kernel does with it: whether the table holds it, and whether the game ever calls 0x12c |
| Kernel syscall table base | `0x80070000` (HLE); the game stores it at `0x00658360` | real kernel table | the game must store *a* base whose entries hold the handlers it installed |
| Timer `COUNT` | +1 per read | real time | ignore values, compare accesses (address, read/write, order) |
| `GetOsdConfigParam` value | `0x00010000` (English) | console/BIOS settings | record; not a behaviour difference |
| Boot arguments (`argc` at `0x006d6380`) | 0 | whatever the bootstrap passed | record the real value |
| COP0 Status at entry | `0x70030c13` (chosen, not a PS2 value) | real | take the real value from the entry capture and use it as the input |

## Where threads and interrupts block a direct comparison

Our run never runs a thread and never raises an interrupt. On PCSX2 both can
happen between the entry and the stop point.

- **Timer 2 interrupt.** `_InitSys` registers handler `0x005b8158` for cause 11
  (timer 2), enables it with `_EnableIntc`, and programs timer 2
  (`COMP = 0xffff`, `MODE = 0xf82`). If it fires before `0x001001f0`, then on
  PCSX2 and not in our run:
  - `0x006592f0` and `0x00659308` are written (direct stores in the handler);
  - memory written through pointers by the handler and its 6 functions (one
    indirect call among them) changes; those addresses are not known statically;
  - `T0_COUNT`, `T2_COUNT` and `T2_MODE` are read or written.

  Treat those as "possibly different" until the capture shows whether the
  interrupt fired (the trace log with `EE.counters` should show it).
- **Thread.** The reference starts the created thread and it blocks in
  WaitSema. The native run skips StartThread because its ReferThreadStatus
  stub does not fill created-thread status. See [thread audit](THREAD_REFERENCE.md).
  The earlier claim that the thread did not run on PCSX2 was incorrect.
- **Kernel memory in general.** `0x00000000Ã¢â‚¬â€œ0x00080000` holds the real
  kernel on PCSX2 and is almost empty in our run. Only the ranges the game
  itself writes there are comparable: the patch copies at `0x80075000`
  (0x330 bytes), `0x80076000` (0x740 bytes) and `0x00082000` (0x28 bytes).

## Directly comparable

- Everything in `0x00100000Ã¢â‚¬â€œ0x008a215c` except the items above: program
  image, data written by `_InitSys`, bss.
- The order and arguments of kernel calls that do not involve IDs or kernel
  addresses, e.g. `SetSyscall(0x5b, 0x80076000)`, `FlushCache(0)`, `FlushCache(2)`.
- The contents of the three kernel patch copies.
- `gp`, and every register the game sets from its own constants.

Local BIOS inventory: the SCPH-39001 image is 4 MiB and its ROMVER entry is
`0160AC20020207` followed by a newline (14 bytes total). This was read from
the local image, not inferred from its filename. No firmware was copied into
the repository. Locating a BIOS does not mean a reference capture was made.
