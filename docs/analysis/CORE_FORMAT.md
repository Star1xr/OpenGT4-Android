# Disc and CORE.GT4 format

Notes from analysing one disc: NTSC-U SCUS-97328 v2.00. Other versions and regions are not checked yet.

Each value is marked:

- **confirmed**: checked two independent ways, or by the game's own code.
- **observed**: seen in this dump, meaning not proven.
- **unknown**: no idea yet.

## Disc

| Item | Value | Status |
|------|-------|--------|
| Serial | `SCUS-97328` (from `SYSTEM.CNF` `BOOT2`) | confirmed |
| Version | 2.00 (`SYSTEM.CNF` `VER`) | confirmed, also matches the CRC32 below |
| Size | 5314478080 bytes | matches GT Modding Hub (4.94 GB) |
| CRC32 | `831f9f84` | matches the GT Modding Hub build list for US 2.00 |
| SHA-1 | `7da11cc5c18d5ed4a7e882c6d1168d3afa6940c3` | **computed locally, not yet checked against Redump** (redump.org was unreachable) |

Root files: `SYSTEM.CNF`, `SCUS_973.28` (boot ELF), `CORE.GT4` (main executable), `IOPRP300.IMG` (IOP reboot image), `GT4.VOL` (game data, 2.4 GB), plus `IRX/`, `NET/`, `EPSON/`.

## SCUS_973.28

A normal EE ELF, but small (0x2dc54 bytes of code) and loaded at `0x01000000`, above where games usually run. Its strings include `core.gt4`, `ImageLoader`, `InflateBase` and the IOP module list. **Observed**: it is a bootstrap that loads CORE.GT4. The exact loading code has not been disassembled yet.

## CORE.GT4 container

| Offset | Size | Value | Meaning | Status |
|--------|------|-------|---------|--------|
| 0x0 | 2 | `01 01` | format tag | observed |
| 0x2 | 4 | 0x005d5ecc | unpacked size | confirmed: output length matches exactly |
| 0x6 | rest | | raw deflate stream (no zlib header) | confirmed: decodes to the end with no leftover bytes |

## Unpacked layout

| Offset | Value | Meaning | Status |
|--------|-------|---------|--------|
| 0x000–0x0ff | high-entropy bytes | signature or hash | unknown purpose; the bytes are nearly random, unlike the code that follows |
| 0x100 | 0x176265b3 | magic | observed; used by `unpack_core.py` as a sanity check |
| 0x104 | 3 | | unknown |
| 0x108 | 0x00100008 | entry point | confirmed, see crt0 below |
| 0x10c | 0x006179fc | end of `.dtors` | observed: equals the address right after the `.dtors` zero terminator (0x006179f8). Purpose for the loader unknown |
| 0x110 | 0x18 | | unknown |
| 0x114–0x127 | 0 | | unknown |
| 0x128 | 0x006dddf0 | `_gp` | confirmed: crt0 loads the same value into `gp` |
| 0x12c | | first load record | confirmed, see below |

### Load records

From 0x12c to the end of the file, the data is a list of records:

```
u32 vaddr
u32 size
u8  data[size]
```

| # | File offset of data | vaddr | size | Contents |
|---|---------------------|-------|------|----------|
| 1 | 0x134 | 0x00100000 | 0x517a14 | text (code) |
| 2 | 0x517b50 | 0x00617a80 | 0xbe37c | data (.data, .rodata and others, see below) |

**Confirmed** in three ways:

1. The two records end exactly at the end of the file (0x517b50 + 0xbe37c = 0x5d5ecc, the unpacked size).
2. The text address: sampled `jal` targets against likely function starts (after `jr ra`, or `addiu sp,sp,-N`). The best offset, 0x100000 − 0x134, scored 230 of 300 samples; the next best (4 bytes off) scored 129.
3. The data address: of 12474 `lui`/`addiu` pairs in the code that point into the data range, 5664 land on the start of a string with the record layout, against 7 if the file were loaded as one contiguous block.

The first version of `unpack_core.py` (commit b78877b) loaded everything contiguously. That was right for the text but 0x64 bytes off for all data. Fixed.

## Memory layout

| Item | Address | How it was confirmed |
|------|---------|----------------------|
| text | 0x00100000 to 0x00617a14 | record 1 |
| Entry | 0x00100008 | The header word at 0x108. The code there is the usual PS2 crt0: a run of `padduw rN, zero, zero` clearing the 128-bit registers. |
| data | 0x00617a80 to 0x006d5dfc | record 2 |
| bss | 0x006d5e00 to 0x008a215c | crt0 clears this range byte by byte, then in 16-byte steps |
| `_gp` | 0x006dddf0 | crt0 passes it to `SetupThread` (syscall 0x3c) and moves it into `gp` |
| Stack | 0x01ff8000, size 0x8000 | arguments to `SetupThread` |
| Heap `_end` | 0x008a215c | argument to `SetupHeap` (syscall 0x3d) |

## Section layout inside the records

The container only has two records. Inside them, the sections below were
found from the code's own behaviour. `splat/gt4.yaml` follows this table.

| Range | Section | Status | Evidence |
|-------|---------|--------|----------|
| 0x00100000–0x00616f24 | .text | confirmed | last function returns at 0x00616f14, then padding |
| 0x00616f24–0x006174f4 | .ctors | confirmed | `-1`, 370 pointers into text, `0`; walked backwards by `__do_global_ctors` (0x005bc4c8) |
| 0x006174f4–0x006179fc | .dtors | observed | same layout right after: `-1`, 320 pointers, `0`. End equals header word 0x10c |
| 0x006179fc–0x00617a14 | ? | unknown | zeros, then the `_gp` value at 0x00617a10 |
| 0x00617a80–0x0068ba80 | .data | **hypothesis** | the writes we can identify (a store whose address comes from a `lui` + offset pair in the same function) all land in 0x00617a80–0x00659974. Writes through pointers, loops, DMA or library code are not seen by this method. Beyond 0x00659974, up to 0x0068ba1e: old-ABI g++ vtables (`{0, fn}` pairs, 0x659980–0x68a410) and 23 liberx export tables (`eRxLiB`, 0x68a418–0x68b920). No identified writes there, which does **not** make them read-only: liberx tables in particular are likely patched at run time. With gcc 2.9x vtables are normally in .data |
| 0x0064ae80–0x0064b438 | `rt_ac.irx` inside .data | confirmed | see below |
| 0x0068ba80–0x006d3e00 | .rodata | **hypothesis** | 0x61 zero bytes after the liberx tables end at 0x0068ba1e; first referenced string at 0x0068baa8; all 370 jump tables read by `lw` + `jr` are in 0x0068bb30–0x006d2f40. These fit a .data/.rodata boundary at 0x0068ba80, but no evidence rules out read-only data before it or writable data after it. The split is a working layout for splat, not a fact about the original link |
| 0x006d3e00–0x006d5bf0 | .eh_frame | confirmed | 191 + 67 CIE/FDE records parse to their zero terminators; 0x006d3e00 is passed to `__register_frame_info` |
| 0x006d5c00–0x006d5dfc | exception table | provisional | `{start, end, handler}` code-address triples ending in `-1`, the gcc 2.9x layout |
| — | .sdata / .sbss | **provisional: none identified** | observation: the code has no `$gp`-relative load, store or `addiu` at all, even though crt0 sets `_gp`. That fits a `-G0` build, but it does not prove the sections are absent: small data can exist without being reached through `$gp`, and the linker may still emit the sections |

A heuristic that was tried and dropped: "3+ consecutive words pointing into
text" as a jump table marker. It also matched vtables, liberx tables and
repeated handler pointers (for example 0x0068a438). Only tables actually read
by `lw` then `jr` count as jump tables.

## Embedded IOP module rt_ac

| Item | Value | Status |
|------|-------|--------|
| Location | 0x0064ae80–0x0064b435 (0x5b5 bytes) in .data | confirmed: ELF headers give this size, and the EE loader computes the same start and end |
| Format | IRX: ELF type 0xff80, `.iopmod`, relocatable (`.rel.text`, `.rel.rodata`, `.rel.data`) | confirmed |
| Name / version | `rt_ac`, 0x0105 (from `.iopmod`) | confirmed |
| Size | text 0x1e0, data 0x30, bss 0 | confirmed |
| Imports | sysmem 6 `QueryMemSize`, 8 `QueryTotalFreeMemSize`; modload 16 `GetModuleIdList`, 17 `ReferModuleStatus`, 18 `GetModuleIdListByName`; thbase 47 `GetThreadmanIdList` | numbers confirmed from the import stubs, names from ps2sdk `sysmem.h`, `xmodload.h`, `xthbase.h` |
| What it does | inspects IOP memory, loaded modules and threads, and returns a value through its start result | **provisional**, from imports only; its code has not been read yet |
| Loader | `LoadRtAcModule` 0x00520758: allocates IOP memory, `sceSifSetDma` + wait on `sceSifDmaStat`, loads and starts it from the buffer, frees the memory, stores `result >> 8` for the caller | provisional names. No direct `jal` or pointer to it was found; called indirectly |

## Generated files

`tools/unpack_core.py` builds these. The disc has no such files.

`extracted/core.elf`:

- `PT_LOAD` 1, R+X: record 1 at 0x00100000, memsz = filesz
- `PT_LOAD` 2, R+W: record 2 at 0x00617a80, memsz runs to 0x008a215c (covers the bss)
- entry from header 0x108
- `e_flags` 0x20924001, copied from the header of `SCUS_973.28`
- file offsets follow addresses (offset = 0x1000 + vaddr − 0x00100000), so `objcopy -O binary` of this ELF equals `core.bin`
- section headers `.text`, `.data`, `.bss`, `.shstrtab`. They only repeat the segments, so splat can read the file: `.text` = record 1, `.data` = all of record 2, `.bss` = 0x006d5e00–0x008a215c. The finer split (ctors, rodata, eh_frame, ...) is in "Section layout inside the records" above and in `splat/gt4.yaml`, not in this ELF.
- no symbols (those live in `splat/symbol_addrs.txt`, see `docs/analysis/SYMBOLS.md`)

`extracted/core.bin`: memory from 0x00100000 to 0x006d5dfc, with the 0x6c-byte gap between the records zero-filled. File offset = address − 0x00100000.

## Open questions

- What `rt_ac` computes and what the game does with `result >> 8`. Who calls `LoadRtAcModule`.
- Whether the vtables and liberx tables are .data or .rodata.
- What the 0x100-byte block is and whether the bootstrap checks it.
- Header words 0x104 and 0x110, and why the loader needs the end of .dtors (0x10c).
