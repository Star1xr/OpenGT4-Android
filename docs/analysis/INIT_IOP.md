# _init and the IOP services it needs

Disc: NTSC-U SCUS-97328 v2.00. Addresses are EE addresses in `CORE.GT4`
unless marked as IOP (FILEIO module offsets).

Status marks, as in `docs/analysis/SYMBOLS.md`:

- **confirmed (EE)**: read from the game's own EE code.
- **confirmed (IOP)**: read from the IOP module the game itself loads
  (`FILEIO` from `IOPRP300.IMG`, see below).
- **provisional**: a name or reading that fits, but has not been checked
  independently.
- **test input**: a value the HLE has to choose. It depends on the console,
  the BIOS or the boot path, and comes from the reference capture.

PCSX2 validation of all of this is a separate, pending item
(`docs/analysis/REFERENCE_CAPTURE.md`).

## Summary

`_init` (0x0048ef90) does two things:

1. Heap setup (`0x00575c78`): clears the region between the pointers at
   `0x0065586c` and `0x00655870` and initializes an allocator at
   `0x008744d0` with it. **No IOP.** (provisional reading)
2. Timezone (`0x005524b8`): converts two reference dates to time values
   and stores their difference at `0x0064c8b0` (provisional reading). On the way, libscf
   (`0x0058cf88`) needs to know whether the console is a development unit.
   It finds out by reading **`rom0:ROMVER`** through the IOP fileio service.
   That is the only IOP use in `_init`.

Static check over the 91 functions reachable from `_init`: no indirect
calls; one RPC server bound (`0x80000001`); four RPC calls (fileio
functions 0xFF, 0, 2, 1). Everything below follows from that path:

```
_init
 └ 0x005524b8  timezone
    └ 0x0058d480 → 0x0058cf88 → 0x0058ce48  "is ROMVER[4] == 'T'?"
       └ 0x0058cdd8  read rom0:ROMVER once into 0x00657ad0 (14 bytes)
          ├ sceOpen("rom0:ROMVER", 1)   0x005b2df0
          │  └ fileio init (first use) 0x005b28f0
          │     ├ sceSifInitRpc          0x005b1028
          │     │  └ sceSifInitCmd       0x005b08c8
          │     ├ bind server 0x80000001 0x005b17d0
          │     └ RPC fno 0xFF           0x005b2258 → 0x005b19b0
          ├ sceRead(fd, 0x00657ad0, 14)  0x005b3438   RPC fno 2
          └ sceClose(fd)                 0x005b3080   RPC fno 1
```

## The IOP side is known

The bootstrap (`SCUS_973.28`) reboots the IOP with `cdrom0:\IOPRP300.IMG`
(string in the bootstrap). That image is on the disc and holds the IOP
modules, among them `SIFCMD`, `FILEIO`, `IOMAN` and `ROMDRV`. So the server
that answers `_init` is the **`FILEIO_service` v2.12** module from that
image, and its code can be read. `tools/irx_dis.py` disassembles it with
import calls named.

`FILEIO` registers three RPC servers: `0x80000001` (used here), `0x80000003`
and `0x80000007`. Handler for `0x80000001`: IOP `0x1e68`.

## Layers

| Layer | EE side | IOP side | Carried by |
|-------|---------|----------|------------|
| EE kernel | `sceSifGetReg`/`SetReg` (0x7a/0x79), `sceSifSetDma` (0x77), `sceSifSetDChain` (0x78), `AddDmacHandler` (0x12), `_EnableDmac` (0x16), semaphores | - | syscalls: HLE |
| SIF DMA | channel 5 (IOP→EE) handler `0x005b0e30` | - | DMA + DMAC interrupt |
| SIF commands | `sceSifSendCmd` `0x005b0db0`, handler table at `0x00886840` | `SIFCMD` | 16-byte header + payload |
| SIF RPC | bind `0x005b17d0`, call `0x005b19b0` | `SIFCMD` RPC | commands `0x80000008`–`0x8000000c` |
| fileio | `0x005b28f0`, open/read/close | `FILEIO` | RPC `0x80000001` + command `0x80000011` |

### SIF command header

As built by `0x005b0c78` (**confirmed (EE)**):

| Offset | Content |
|--------|---------|
| +0x0 | byte 0: packet size (`psize`); bits 8–31: extra data size (`dsize`) |
| +0x4 | IOP destination of the extra data, or 0 |
| +0x8 | command ID |
| +0xc | option (set by the caller) |
| +0x10 | payload, up to `psize` |

`psize` must be 0x10–0x70, or the send returns 0 without sending anything.
The EE sends one DMA list with `sceSifSetDma`: the extra data first (if any,
to the given IOP address), then the packet to the IOP command buffer (the
address kept at `0x00886820`), with attribute `0x44`. The send returns the
DMA ID; **0 means failure**, and every caller checks it.

## Calls, in order

### 1. SIF command init: `sceSifInitCmd` (0x005b08c8)

Sets up the EE command buffers, `FlushCache(0)`, then:

- reads `D_STAT` (0x1000e010); if bit 0x20 (channel 5 interrupt) is set,
  writes 0x20 to clear it;
- reads `D5_CHCR` (0x1000c000); if bit 0x100 (busy) is clear, calls
  `sceSifSetDChain` (start the IOP→EE receive chain);
- `AddDmacHandler(5, 0x005b0e30, 0)`, `_EnableDmac(5)`.

Then **two paths**, chosen by `sceSifGetReg(0x80000000)`:

| Path | Condition | EE does | EE expects |
|------|-----------|---------|------------|
| A: already initialized | `sceSifGetReg(0x80000000) != 0` | stores it as the IOP buffer, sends command **`0x80000000`**, 0x14 bytes, payload `+0x10` = EE receive buffer `0x00886740` | send succeeds (non-zero) |
| B: fresh | `sceSifGetReg(0x80000000) == 0` | loops until `sceSifGetReg(4) & 0x20000` (IOP ready), reads `sceSifGetReg(2)` (IOP command buffer), `sceSifSetReg(0x80000000, it)`, `sceSifSetReg(0x80000001, EE data)`, sends command **`0x80000002`** with option 0, payload `+0x10` = `0x00886740` | the `SIF_REG 4` bit to become set (spin loop, no syscall inside) |

`sceSifGetReg(0x80000000)` is a kernel-held register that survives from the
bootstrap, which already used SIF to reboot the IOP. So on a console **path A
is likely** (provisional; test input until the capture shows it). The HLE
should support both.

### 2. SIF RPC init: `sceSifInitRpc` (0x005b1028)

Registers EE handlers for commands `0x80000008` (RPC end, `0x005b1328`),
`0x80000009` (`0x005b1700`), `0x8000000a` (`0x005b1920`) and `0x8000000c`
(`0x005b1438`). Then:

| Condition | EE does | EE expects |
|-----------|---------|------------|
| `sceSifGetReg(0x80000002) != 0` | nothing more | - |
| `sceSifGetReg(0x80000002) == 0` | sends command **`0x80000002`**, 0x10 bytes, option 1; spins on software register 0 (`0x005b0880(0)`, array at `0x008869c0`) until non-zero; then `sceSifSetReg(0x80000002, 1)` | the IOP to set software register 0, which on the EE happens through command **`0x80000001`** (`{+0x10 index, +0x14 value}`, default handler `0x005b0850`) |

Same reasoning as above: after the bootstrap, register `0x80000002` is
probably already 1 (provisional, test input). The spin loop has no syscall
in it, so the IOP reply must be delivered by an interrupt, not by the loop.

### 3. Bind fileio: `sceSifBindRpc` (0x005b17d0), called from 0x005b28f0

EE sends command **`0x80000009`**, packet 0x40 bytes:

| Offset | Content |
|--------|---------|
| +0x14 | packet address (EE) |
| +0x1c | client structure (EE, `0x008899c0`) |
| +0x20 | server ID `0x80000001` |

Then `WaitSema` on a semaphore created for the call.

**Expected reply**: command **`0x80000008`** with `+0x1c` = the same client,
`+0x20` = `0x80000009` (the request type), `+0x24` = server pointer (IOP),
`+0x28` = server receive buffer (IOP). Handler `0x005b1328` stores `+0x24`
into `client+0x24` and `+0x28` into `client+0x14`, then `iSignalSema`.
**confirmed (EE)**

**Condition**: `client+0x24` must be non-zero. If it is 0 (server not
registered yet), `0x005b28f0` busy-waits and binds again, forever.

### 4. fileio init: RPC call fno 0xFF

Through `0x005b2258` (retries a failed send up to 100 times, sleeping with
`DelayThread` `0x005aed18` between attempts) and `sceSifCallRpc`
`0x005b19b0`.

`sceSifCallRpc` sends command **`0x8000000a`**, packet 0x40 bytes, and in
the same DMA list sends the call data to the server buffer (`client+0x14`):

| Offset | Content |
|--------|---------|
| +0x14 | packet address |
| +0x1c | client |
| +0x20 | function number |
| +0x24 | send size |
| +0x28 | receive buffer (EE) |
| +0x2c | receive size |
| +0x30 | mode (1 = the EE waits on a semaphore) |
| +0x34 | server pointer (from the bind) |

**Expected reply**: the receive data written to the EE receive buffer, then
command **`0x80000008`** with `+0x20` = `0x8000000a`; the handler signals the
waiting semaphore. **confirmed (EE)**

fno 0xFF specifically:

| | Content | Status |
|-|---------|--------|
| Sent (8 bytes, `0x00888280`) | `{0x00888f40, 0x00888f40 + 0x440}`: two EE buffers for completion records | confirmed (EE) |
| IOP stores them | as the two completion slots (IOP `0x32f0`) | confirmed (IOP) |
| Reply (8 bytes, to `0x00888f00`) | `{"3000", 2}`: word 0 is FILEIO's `.data` at IOP `0x323c` = ASCII `"3000"`; word 1 is 2 | confirmed (IOP) |
| EE uses | word 0 → `0x008899e8`; word 1 == 2 → flag `0x00658340` = 1 (use the slot index in completions) | confirmed (EE) |
| Condition | every later `sceOpen` checks word 0 against `"3000"` or `"...."` (`0x005b2af8`); anything else makes `sceOpen` return `0xfffefffc` | confirmed (EE) |

### 5. `sceOpen("rom0:ROMVER", 1)` (0x005b2df0): RPC fno 0

| | Content | Status |
|-|---------|--------|
| Sent (0x418 bytes, `0x008882c0`) | `+0x0` completion semaphore ID (EE); `+0x4` EE address for the result (on the caller's stack); `+0x8` 4 (result size); `+0xc` flags & 0x7fffffff (= 1, read); `+0x10` mode; `+0x14` path, NUL-terminated, up to 0x400; `+0x414` EE file slot index | confirmed (EE) |
| Immediate reply (4 bytes, `0x00888f00`, read uncached) | 1 = request accepted, 0 = refused (no request slot, or mailbox/thread creation failed) | confirmed (IOP `0x1e8c`, `0x2268`) |
| EE on 0 | deletes the semaphore, `sceOpen` returns -11 | confirmed (EE) |
| Completion | an IOP worker thread opens the file (through `IOMAN`/`ROMDRV`) and sends command **`0x80000011`** with option/`+0xc` = slot index 0/1 (alternating) and, in the same DMA, a record into the EE slot buffer: `{sema ID, type, dest, size, data...}` | confirmed (IOP `0x0528`–`0x05bc`, EE handler `0x005b23b8`) |
| EE handler | copies `size` bytes of data to `dest` (the 4-byte result: negative on error), does type-specific copies, `iSignalSema(sema)` | confirmed (EE) |
| EE after `WaitSema` | result < 0 → free the slot, return the error; else store the handle in the slot and return the slot index | confirmed (EE) |

### 6. `sceRead(fd, 0x00657ad0, 14)` (0x005b3438): RPC fno 2

Same shape. Sent 0x20 bytes; the IOP takes the EE file slot index from `+0x1c`
and uses it to find the worker's mailbox (IOP table at `0x4200`, filled at open
from `+0x414`).
Reply 4 bytes (1/0). Completion through `0x80000011` with the data: the EE
handler copies it to the destination (partly by type-specific copy). Reads
the 14 bytes of `ROMVER`. The request layout is now confirmed by EE `0x005b34b8` onward and IOP
`0x0828` onward: `+0x0` semaphore, `+0x4` result address, `+0x8` result size,
`+0xc` IOP fd, `+0x10` EE read destination, `+0x14` requested bytes,
`+0x1c` file slot. `+0x18` is not needed by this bounded HLE; no meaning is assumed.

The read completion has type 2 and is 0xa4 bytes (IOP `0x0ad8`). After the
16-byte completion header: result at `+0x10`, leading/trailing fragment lengths
at `+0x14/+0x18`, their EE destinations at `+0x1c/+0x20`, leading data at
`+0x24`, trailing data at `+0x64`. For reads below 64 bytes, IOP `0x0844` onward
places the whole result in the leading fragment. Open/close records are 0x20
bytes (IOP `0x0678`, `0x071c`). The prototype supports only ROMVER reads up to
14 bytes, not the general aligned bulk-DMA read path.

### 7. `sceClose(fd)` (0x005b3080): RPC fno 1

Sent 0x14 bytes; the IOP takes the EE file slot index from `+0x10`. Reply 4 bytes.
Completion through `0x80000011`.

### What the result is used for

`0x0058ce48` returns `ROMVER[4] == 'T'` (confirmed (EE)). `ROMVER` is 14
characters from the BIOS ROM. The usual reading, 4 digits of version, region
character, console type character, date, with `'T'` as the region of
development (TOOL) units, is general knowledge, not taken from this code:
**provisional**.

- `'T'`: the timezone comes from the game's own data (`0x00657ac8`).
- anything else: from `GetOsdConfigParam` (bits 21 and up, signed); if the
  config version field (bits 13–15) is 0, it defaults to 540 minutes (+9 h).

If `sceOpen` fails, `0x00657ad0` stays zero and character 4 is not `'T'`, so
the OSD path is taken. The game does not stop on this failure.

## Off the normal path

- `DelayThread` (`0x005aed18`) runs only when an RPC send fails (retry in
  `0x005b2258`). It uses the alarm syscalls the game installed in kernel
  memory during `_InitSys`, and returns `0x80008008` if interrupts are
  disabled.
- Handler for command `0x80000013` (`0x005b28a8`): runs a callback
  `(func, arg)` sent by the IOP. `FILEIO` does not send it for these calls.
- `FILEIO` functions 0xFE (buffer config) and 0xFD (priority), and servers
  `0x80000003` and `0x80000007`: not used by `_init`.

## Test inputs this adds

| Input | Why it matters | Source for the real value |
|-------|----------------|---------------------------|
| `sceSifGetReg(0x80000000)` and `(0x80000002)` at CORE entry | pick path A/B in steps 1 and 2 | reference (trace log with `EE.bios`), or reasoning from the bootstrap |
| `SIF_REG 4`, `SIF_REG 2` (path B) | IOP ready bit, IOP command buffer address | reference; the address is the HLE's choice |
| Content of `rom0:ROMVER` | only `ROMVER[4] == 'T'` changes behaviour | the user's BIOS (reference capture reads `0x00657ad0` after `_init`) |
| IOP addresses returned by bind (server, buffer) | stored by the EE, used as DMA destinations | the HLE's choice; compare by role |

## Next executable chunk

**crt0 + `_InitSys` + `FlushCache(0)` + `_init`**, stopping on return from
`_init` at `0x00100204`.

Why this chunk:

- It is the next piece in execution order, and needs **one** IOP service,
  fully known from the game's own IOP module.
- It exercises the whole SIF stack (commands, RPC, DMA, DMAC interrupt)
  with the smallest possible server, which every later IOP service reuses.
- The result has one observable effect on game data (`0x00657ad0`, and the
  timezone at `0x0064c8b0`), easy to check.

What it needs, with how each would be treated:

| Service | Plan |
|---------|------|
| `sceSifGetReg`/`SetReg` | HLE register file; initial values are test inputs (paths A and B both tested) |
| `sceSifSetDma`, `isceSifSetDma` | HLE: copy EE data into an IOP memory model; a packet sent to the IOP command buffer goes to the IOP HLE |
| `sceSifSetDChain`, `AddDmacHandler`, `_EnableDmac`, `D_STAT`, `D5_CHCR` | HLE: record the channel 5 handler; registers modelled as far as step 1 reads them |
| Semaphores, incl. `iSignalSema`, `WaitSema` | implemented (counts); `WaitSema` on 0 with no pending IOP reply ends the run (deadlock, not silently skipped) |
| IOP SIF commands | HLE responder for `0x80000000`, `0x80000002` (option 0 and 1), `0x80000009`, `0x8000000a` |
| fileio server `0x80000001` | HLE of fno 0xFF, 0, 2, 1 with the replies above, completions through `0x80000011` in alternating slots |
| `rom0:ROMVER` | 14-byte test input |

**Delivery policy (must be explicit in the test).** The IOP HLE computes
its reply when the EE's DMA reaches the IOP command buffer. Delivery to the
EE (write into the EE buffer, then run the game's channel 5 handler
`0x005b0e30`) happens at a deterministic **interrupt point**: a syscall returning,
`ei`, a generated branch-target label, or a blocked `WaitSema`, with the EIE
bit set and no handler already running. Branch labels are required to support
delayed replies while the guest polls a software register without syscalls.
The prototype can delay each reply by a chosen number of eligible points;
this is a test schedule, not hardware timing. Real hardware
could deliver at other instants; the test states this, and the reference
capture shows the real order. Timer interrupts stay off, as in the first
chunk.

Pass criteria for that test, besides reproducibility: ends at `0x00100204`;
`0x00657ad0` holds the `ROMVER` input; `0x008899e8` holds `"3000"`;
`0x00658340` = 1; every SIF command and RPC call in the trace with its
payload; no `DelayThread` (no failed send); both SIF paths A and B end the
same way.

## Execution prototype

Implemented and tested: see `INIT_RUNTIME.md`. The missing-ROMVER scenario
reaches the stop point with RPC sequence `0xff, 0, 0`: the game attempts open
twice and does not issue read/close. Both attempts use asynchronous error
completions. The earlier static summary of four calls describes the successful
open path, not this failure path.
