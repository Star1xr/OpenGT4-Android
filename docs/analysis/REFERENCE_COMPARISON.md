# Preliminary PCSX2 comparison, 2026-09-26

Latest result: the corrected format-2 recapture is complete at both verified
PCs. All three kernel blocks match the native baseline exactly. See the
corrected recapture section below; the original kernel mismatch is invalid.

Follow-up [thread/boot audit](THREAD_REFERENCE.md): the explicit SetupThread
stack reservation is now corrected, so SP matches the capture. The created
thread really executes on the reference; an incomplete ReferThreadStatus
stub bypasses StartThread natively. Historical counts below describe the
pre-fix baseline, not the current runtime. Full scheduling remains missing.

Two paused snapshots were captured from PCSX2 2.5.165 running SCUS-97328
v2.00 with the selected USA 2.30 BIOS. Debugger screenshots confirm the PCs:
`0x00100008` (entry) and `0x001001f0` (after `_InitSys`). The user reset the
game with both execution breakpoints enabled, then continued once between
the snapshots.

Local evidence is in ignored `captures/20260926-183123/`: RAM and timer
reads through PINE, configuration copies, debugger screenshots, transcribed
registers and `comparison.json`. The `entry_candidate` and
`initsys_exit_candidate` names were retained; their `validation.json` files
now confirm the PCs. No firmware or captured game bytes belong in Git.

## Method and limitations

Built `initsys_run` from the current workspace and ran a fresh baseline into
`native_baseline` within the capture directory. The local `compare.py`
compares that output with the snapshots and writes `comparison.json`.
The native harness retained its original synthetic inputs; it was not
initialized from the captured CPU or kernel state.

The captured settings have EE/IOP recompilers and fastmem enabled, and
EE BIOS/counter/hardware tracing disabled. This is a preliminary endpoint
comparison, not a syscall trace or a full equivalence test. `HI1`, `LO1`
and `SA` were not captured. CPU values were transcribed from screenshots;
the effect of debugger stops on exception state has not been established.
No conclusion about interrupt order or timing follows from these snapshots.

## Results

Both executions reach `0x001001f0`.

| Compared item | Result |
| --- | --- |
| Text, `0x00100000..0x00616f24` (exclusive end) | Byte-identical; also unchanged between PCSX2 entry and exit |
| Remaining pre-BSS image, `0x00616f24..0x006d5e00` | 5 differing bytes, in 4 contiguous ranges |
| BSS, `0x006d5e00..0x008a215c` | 63 differing bytes, in 23 contiguous ranges |
| Full 128-bit GPR values | 20 of 32 match |
| `gp` / `ra` | Match: `0x006dddf0` / `0x001001f0` |
| Exit COP0 Status | Matches: `0x70030c11` |
| `HI` / `LO` | HI matches zero; LO is `0x130` on PCSX2, zero natively |
| Stack snapshot | 7,008 differing bytes; raw address comparison only |

### Known variable inputs and IDs

| Value | PCSX2 | Native synthetic baseline |
| --- | --- | --- |
| Exit SP | `0x01fffd60` | `0x02000000` |
| argc at `0x006d6380` | 2 | 0 |
| Kernel table pointer at `0x00658360` | `0x80015500` | `0x80070000` |
| Created thread ID at `0x006582a8` | 4 | 2 |
| Semaphore IDs at `0x00658378`, `0x0065837c` | 0, 1 | 1, 2 |
| Semaphore ID at `0x00885ee0` | 2 | 3 |

The captured argument area contains `cdrom0:\CORE.GT4;1` and `hot`.
The SP difference is `0x2a0` bytes. This is one observed boot configuration,
not a justification for replacing the HLE defaults with these constants.
Thread/semaphore roles above are supported by stores following the respective
calls in the local disassembly.

The 12 differing GPRs are `at`, `t0` through `t8`, `k0` and `sp`.
Kernel scratch-register effects are a plausible contributor, but without a
trace their cause and significance remain unverified. Additional BSS
differences cluster near `0x00885bc4..0x00885ee1`; they need kernel/thread
service analysis before classification as harmless or incorrect.

### Kernel comparison withdrawn: capture addressing bug

Follow-up audit: both reference files contain only zeros below `0x00080000`,
including the purported syscall table. PINE reads guest virtual addresses
via `memRead64`; the old tool incorrectly treated low addresses as physical
RAM. The differing-byte counts below therefore compare native code against
zeros, **not a usable kernel reference**. They are retained as historical
diagnostics only and do not establish a kernel-copy or recompiler defect.

The corrected tool reads RAM via the direct kernel alias
`0x80000000 + physical_offset`, retains physical-offset filenames, records
format-2 addressing metadata, and rejects zero-filled kernel data. It also
requires paused state before each range and after reading, refuses to
overwrite captures, and no longer reads beyond the final word. Six synthetic
tests pass; live validation of the corrected path remains pending.

Sources: [PINE virtual reads](https://github.com/PCSX2/pcsx2/blob/v2.5.165/pcsx2/PINE.cpp#L537-L548),
[kernel direct mapping](https://github.com/PCSX2/pcsx2/blob/v2.5.165/pcsx2/Memory.cpp#L515-L523).

The original low-virtual-address reads differ from the native blocks by 509 bytes
at `0x00075000` (size `0x330`) and 1,347 bytes at `0x00076000` (size `0x740`).
Those PCSX2 ranges are unchanged between the entry and exit snapshots.
The 40-byte block at `0x00082000` matches the native output, but was already
present at entry: equality does not prove a copy occurred during this interval.

Do not classify these as recompiler defects or assume a particular BIOS
branch from endpoint data alone. Check kernel address/cache visibility and
trace the actual patch/copy path, including its timer-dependent condition.
The reference table pointer also differs by design from the synthetic HLE.

The 63 BSS differences partition into 28 bytes in boot arguments, 34 inside
the created thread's requested stack (`0x00885ae0..0x00885ee0`), and one byte
in the semaphore ID at `0x00885ee0`. The stack base/size are assembled before
`CreateThread` at `0x005aeaf8`. Kernel preparation of a thread context is a
plausible explanation for those 34 bytes; it does not prove thread execution.
The HLE records the created thread but does not prepare that guest stack.

## Validation and next work

`cmake --build build --config Release --target initsys_run` succeeded.
`tests/initsys/run_test.py` passed reproducibility and all internal checks.
This does not make the reference comparison a pass.

The corrected recapture below completes the kernel-addressing check.
Remaining work is reference-aware treatment of boot arguments, stack/thread
state and scratch registers. No runtime semantics were changed for this comparison.

The interpreter experiment did not stop at breakpoints. In standard 2.5.165,
interpreter breakpoint checks require `PCSX2_DEVBUILD`/`EXTRA_DEBUG`, and
trace macros require `PCSX2_DEVBUILD`. INI switches cannot enable code omitted
at build time. The previous configuration was restored and verified.

Stable 2.8.2 (the official latest-release redirect at audit time) retains
these gates: [interpreter](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/Interpreter.cpp#L143-L158),
[trace macros](https://github.com/PCSX2/pcsx2/blob/v2.8.2/pcsx2/DebugTools/Debug.h#L216-L222).
An ordinary update does not solve this specific limitation; keep 2.5.165
for the corrected recapture to avoid changing reference conditions.

## Corrected recapture: kernel endpoint comparison complete

Evidence: ignored `captures/alias_20260926-190228/`. Both snapshots use the
format-2 direct RAM alias with PCSX2 2.5.165, the original recompiler settings,
and SCUS-97328 v2.00. Screenshots confirm entry `0x00100008` and exit
`0x001001f0`; all GPRs, PC, HI and LO were transcribed for this run. CP0 and
HI1/LO1/SA were not recaptured and were not copied from the earlier run.

The first 512 KiB now contain 79,145 nonzero bytes at entry. At exit the
`0x330` bytes at `0x75000`, `0x740` bytes at `0x76000`, and `0x28` bytes at
`0x82000` all match the native baseline exactly. They also already match at
entry and remain unchanged; endpoint equality does not demonstrate which
copy operations occurred during the interval.

The kernel table at `0x80015500` is readable. Handler 0x5a changes from
`0x01028978` to `0x005b98d0`; handler 0x54 changes from `0x0102afc0` to
`0x005b9e40`. The checked final handlers (0x08, 0x54 through 0x59, 0x5a,
0x5b and 0xfc through 0xff) match the native baseline values at its own
synthetic table base. This is an endpoint check, not syscall-order validation.

Text remains byte-identical to the native output, data/BSS still differ by
68 bytes, and 20 of 32 GPRs match. The previous kernel discrepancy is resolved
as a capture-addressing error. Full CPU/kernel equivalence is not established.
