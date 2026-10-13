# Symbols and library identification

Disc: NTSC-U SCUS-97328 v2.00, `CORE.GT4`. Symbols live in `splat/symbol_addrs.txt`. Every line there ends with a status tag and the evidence, for example:

```
FlushCache = 0x005adf20; // type:func [verified] syscall 0x64
```

The evidence text must not contain `:` or `;`: splat reads `key:value` pairs from the comment and expects a single `;`.

The status tags:

- **verified**: fixed by something that cannot be a coincidence. Examples: the header entry field, a syscall number.
- **provisional**: fits the evidence, but another reading is still possible. The comment says what the evidence is.
- **unnamed**: we know it's a function, but not what it does. It gets a placeholder name (`syscall_XX`).

Rule: never promote a name to verified without writing down how in the comment.

## Sony library versions

The binary contains `PsII` version markers left by the Sony SDK libraries. They are plain strings, so the list below is **verified** for this disc. They show which libraries are linked and at which version. They do **not** say where each library's functions are.

| Address | Library | Version |
|---------|---------|---------|
| 0x00655e70 | libgraph | 3000 |
| 0x00655ec0 | libcdvd | 3000 |
| 0x00657a88 | libeyety (EyeToy) | 1300 |
| 0x00657ab8 | libscf | 3000 |
| 0x00657ae0 | libmc | 3020 |
| 0x00657b00 | libpad2 | 3020 |
| 0x00657b30 | libvib | 3000 |
| 0x00657b40 | libdbc | 3020 |
| 0x00657b58 | libmrpc | 3000 |
| 0x00658290 | libkernl | 3000 |
| 0x006ce4f0 | liberx | 3021 |

So the game was built against SDK 3.0.x. Other libraries (libdma, libgifpk, libvu0, libipu, libsdr and so on) leave no marker. They may be linked, but that isn't proven.

## EE kernel syscall stubs

`tools/find_syscall_stubs.py` scans the text for `addiu v1, zero, N; syscall; jr ra; nop`. The longest run of them 16 bytes apart is the libkernl stub table: 138 stubs at 0x005ad880–0x005ae120.

A syscall number fixes what the kernel does, so matching by number is reliable. The names come from ps2sdk's `ee/kernel/include/syscallnr.h`, which is not bundled here: pass its path to the script. Sony's own names are almost always the same, but where they differ the ps2sdk name is what we use.

| Status | Count | Why |
|--------|-------|-----|
| verified | 120 | number has exactly one name in ps2sdk |
| provisional | 10 | `AddIntcHandler`/`AddIntcHandler2` and `AddDmacHandler`/`AddDmacHandler2` share a number and the order of each pair wasn't checked; `SetupThread`/`SetupHeap` are ps2sdk aliases of RFU060/061; `RFU009`, `RFU059`, and two ps2sdk calls "arbitrarily named" |
| unnamed | 8 | numbers ps2sdk doesn't list: 0x0, 0x3, 0xfd, −0xfe, 0x3f, 0x49, −0x53, −0x5a |

23 more stubs sit outside the table, at 0x005b7380–0x005b9aa0, inside other functions. They are listed in the script output as comments and left unnamed for now.

## crt0

From the code at the entry point (0x00100008). Disassembled with rabbitizer.

| Address | Name | Status | Evidence |
|---------|------|--------|----------|
| 0x00100008 | `_start` | verified | header entry field; the code is a standard crt0 |
| 0x00100228 | `_root` | provisional | passed to `SetupThread` as the root routine; its body calls syscall 0x23 (`ExitThread`) |
| 0x005b7560 | `_InitSys` | provisional | first call after `SetupHeap`, where Sony's crt0 calls it |
| 0x005adf20 | `FlushCache` | verified | table stub for syscall 0x64; crt0 calls it with a0 = 0 |
| 0x0048ef90 | `_init` | provisional | called after `ei`, before `main`, where Sony's crt0 calls `_init`. It is **not** the constructor loop: 2 direct calls, no `jalr`. Constructors run from `__main` |
| 0x00107f08 | `main` | provisional | called with a0 = argc (`*0x006d6380`), a1 = argv (`0x006d6384`) |
| 0x005a3140 | `Exit` | provisional | crt0 jumps here with main's return value |

## libgcc startup

| Address | Name | Status | Evidence |
|---------|------|--------|----------|
| 0x005bc588 | `__main` | provisional | first call in `main`; static once-flag, then tail call to `__do_global_ctors`. gcc 2.9x inserts `__main()` at the top of `main` |
| 0x005bc4c8 | `__do_global_ctors` | provisional | counts the `-1`-headed list at 0x00616f24 up to the `0`, then calls the entries backwards via `jalr` |
| 0x005bec48 | `__register_frame_info` | provisional | first call in `__do_global_ctors`, with the `.eh_frame` start 0x006d3e00 |
| 0x00616f24 | `__CTOR_LIST__` | verified | `-1`, 370 function pointers, `0`, read by the loop above |
| 0x006174f4 | `__DTOR_LIST__` | provisional | same layout right after, 320 entries. Its reader has not been found |
| 0x006d3e00 | `__EH_FRAME_BEGIN__` | verified | CIE/FDE chain parses to a zero terminator |

## Library functions from error strings

`tools/find_named_strings.py` looks in rodata for messages that start with a
Sony function name (`sceXxx:` or `sceXxx():`) and finds the function that
references each one. By the project rule, a name from a string alone is
**provisional**: the function could be a wrapper, or a caller printing
someone else's name.

| Result | Count |
|--------|-------|
| provisional names (one referencing function) | 18: `sceCdChgSys`, 10 `sceDbc*` (libdbc), 6 `sceErx*` (liberx), `sceLibnetInitialize` |
| ambiguous, not named | 1: `sceDbcGetDeviceStatus` (message used by 0x005900f0 and 0x005901a0) |
| library membership only (`libxxx:` prefix) | liberx 0x00583718, 0x00583958, 0x00585408; libmc 0x0058d4c8; libpad2 0x0058eda8; libdbc 0x0058f780, 0x00590520, 0x005906d0 |

Where the libraries sit in text, from these functions: liberx ≈ 0x00583000–0x00588500,
libmc ≈ 0x0058d400, libpad2 ≈ 0x0058ed00, libdbc ≈ 0x0058f700–0x00590900.
These are hints for later work only; no library start or end is established.

## Embedded IOP module

| Address | Name | Status | Evidence |
|---------|------|--------|----------|
| 0x0064ae80 | `rt_ac_irx` | verified | IRX ELF; the EE loader computes the same start and end (0x0064b435) |
| 0x00520758 | `LoadRtAcModule` | provisional | our own name; copies `rt_ac_irx` to the IOP and starts it |
| 0x005b5e10 | `sceSifAllocSysMemory` | provisional | called `(0, size, 0)`; its result is the DMA destination |
| 0x005b6a20 | `sceSifLoadStartModuleBuffer` | provisional | called `(iop_addr, arglen, args, &result)` after the DMA finishes |
| 0x005b5e90 | `sceSifFreeSysMemory` | provisional | called with the IOP address after the start |

Details of the module itself: `docs/analysis/CORE_FORMAT.md`, "Embedded IOP module rt_ac".

## SIF, RPC and fileio (used by `_init`)

24 names, all **provisional**, in `splat/symbol_addrs.txt` under
"SIF / RPC / fileio used by _init". They come from behaviour (command IDs
sent and handled, packet layouts, call order) and from reading the IOP
`FILEIO` module the game loads, not from a signature match. Names in Sony's
style (`sceSifCallRpc`, `sceOpen`, ...) follow the usual SDK names for that
behaviour; lower-case names (`fio_init`, `read_romver`, ...) are ours.
Details and evidence: `docs/analysis/INIT_IOP.md`.

## Next

- Name libkernl functions around the stub table by their calls to verified stubs.
- Upgrade string-named functions by a second, independent check (argument count and use at call sites, or a match against a known SDK 3.0 build).
- Check the order of the `AddIntcHandler`/`AddDmacHandler` pairs.
