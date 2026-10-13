# Running into main

Target `main_run` (built with `-DOPENGT4_RECOMP_TESTS=ON`) recompiles from
crt0 through `main` and runs with no stop address, until the first thing the
runtime does not provide. It is a probe, not a test with pass criteria.

```
cmake --build build --config Release --target main_run
build/Release/main_run.exe extracted/core.bin <out_dir> 0 retail 0
```

Arguments after `<out_dir>` are the same SIF test inputs as `init_run`
(fresh SIF: 0/1; ROMVER: retail/tool/missing; IRQ delay points).

## What the build includes

- Closure from crt0 over direct calls and tail calls, plus every function
  whose address appears in the data record `0x00616f24–0x006d5dfc`
  (`--table-roots`): constructor lists, vtables, callback tables. 25,137
  functions, written in 16 parts compiled in parallel (about 3 minutes).
- Boot arguments as a test input: `cdrom0:\CORE.GT4;1` and `hot`, the values
  the bootstrap passed on PCSX2.

## Where it stops (2026-09-27)

After `_init`, `__main` runs the 370 constructors and `main` starts. About
3,000 more trace events later (mostly `GetThreadId` and interrupt
enable/disable pairs around locks), the game binds **IOP RPC server
`0x80000592`**, which the HLE does not provide:

```
end reason 3: SIF: unknown bind server   (bind packet +0x20 = 0x80000592)
```

`CDVDFSV` in `IOPRP300.IMG` registers servers `0x80000592`, `0x80000593`,
`0x80000595`, `0x80000597`, `0x8000059a` and `0x8000059c`. This is the disc
access service (`libcdvd`), which the game needs to read `GT4.VOL`.

## Added for this run

| Piece | Where | Checked by |
|-------|-------|-----------|
| Guest threads on fibers | `src/ee/thread.c`, `docs/analysis/THREADS.md` | `ee_sched` unit test; the `_InitSys` thread matches the PCSX2 capture |
| FPU with PS2 semantics (no Inf/NaN, clamping, flags, round toward zero) | `src/ee/fpu.c` | `ee_fpu` unit test, recompiler test (`c.lt.s` + `bc1tl`, `sqrt.s`) |
| MMI (128-bit parallel) | `src/ee/mmi.c` | `ee_mmi` unit test |
| `SA` in bytes (`mtsah` was storing bits) | `mmi.c` | `ee_mmi` unit test |
| Boot arguments written by `SetupThread` | `src/ee/syscall.c` | layout from the PCSX2 capture |
| Table roots and split output | `tools/recomp/ee_recomp.py` | builds and runs |

## Still not translated

2,158 sites in the 25,137 functions, mainly the VU0 macro instructions
(`lqc2`, `sqc2`, `qmtc2`, `qmfc2`, `v*`, and `vcallms`, which starts a VU0
microprogram), `break` (compiler traps, left as an explicit stop) and
`bc0f`. Each ends the run if reached.

## Next

1. HLE of the CDVD servers, from the `CDVDFSV` module the game loads, as was
   done for `FILEIO` (`docs/analysis/INIT_IOP.md`): which calls `main` makes, the data
   sent, the replies expected.
2. A reader for the user's disc image behind it (sectors of the ISO), then
   the `GT4.VOL` format.
