# GT4.VOL (RoFS volume)

Reader: `src/core/vol.c`. Tool: `gt4vol <disc.iso> ls|cat`.

Starting point: the GT Modding Hub description of the PS2 GT4 volume. Every
item below was checked against the user's NTSC-U v2.00 disc; differences from
that description are marked.

| Item | Value | Checked by |
|------|-------|------------|
| Location | `GT4.VOL` in the ISO root (LBA 105879 on this disc) | ISO 9660 directory |
| Decoy index | page 0 holds an old (GT3-style) index, version 2.2 | read |
| Real header | page `0x2159` (0x800-byte pages) | magic there |
| Magic | `0xACB990AD` = bitwise NOT of `"RoFS"` | read |
| Header | **not** XOR-encrypted (the Hub says it is when the magic is inverted; the plain values are the only ones that make sense) | read |
| Version | u16 1, u16 3 (Hub lists them the other way round) | read |
| `+0x08` | compressed index length (`0x59436`) | equals the last page offset |
| `+0x0c` | pages before the data area (`0xb3`) | data base = `0x2159 + 0xb3 = 0x220c` |
| `+0x10` | page size `0x800` | read |
| `+0x12` | index page count (407) | read |
| `+0x40` | 408 index page offsets, each `v ^ (i * 0x14ac327a + 0x14ac327a)` | decode to increasing offsets |
| Index pages | raw deflate, then XOR `0x55` on every byte | page headers make sense |
| Deflate stream | ends with a sync-flush marker, **no final block** | Python `eof` false; miniz needs `MZ_SYNC_FLUSH` |
| Page header | `s16` index page flag, `u16` record count, `s32` next, `s32` previous | |
| Record table | at the page end, read backwards: `u32` = offset (16 bits) \| length << 16 | records decode |
| Entry pages | records alternate (value, key) | |
| Key | parent node ID (big-endian u32) + name | |
| Value | type byte: 0 dir (`u32` node), 1 file (`u32` page, `u32` date, `u32` size), 2 compressed (`u32` page, `u32` date, `u32` compressed size, `u32` size), little-endian | all 18,562 entries parse |
| Root | node 1 (the `"."` entry under parent 0) | |
| Compressed file | magic `C5 EE F7 FF`, `s32` −size, raw deflate | size field equals −(index size) |

Result: 18,562 entries (14,260 compressed files, 4,092 plain files, 210
directories). Extracted files match the Python prototype byte for byte.
