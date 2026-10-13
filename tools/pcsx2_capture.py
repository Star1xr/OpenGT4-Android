#!/usr/bin/env python3
"""Read EE memory from a running PCSX2 through PINE (its IPC interface).

Usage:
  python tools/pcsx2_capture.py --out captures/<run>/<point> [--slot 28011]

Run it while PCSX2 is paused at a debugger breakpoint (see
docs/analysis/REFERENCE_CAPTURE.md). It writes the same ranges the recompiled test
dumps, with the same file names, plus info.json with what PCSX2 reports about
itself and the game. Registers are not available through PINE: copy them from
the debugger into regs.txt by hand (format in the doc).

PINE must be enabled: Settings > Advanced > "Enable PINE", slot 28011 by
default. On Windows PINE listens on TCP 127.0.0.1:<slot>.

PINE reads guest virtual addresses, not physical offsets. RAM is read via
the direct kernel alias (physical address | 0x80000000), while filenames
retain physical offsets. Earlier low-address captures returned zero-filled
kernel data and must not be used to validate kernel patches.

The corrected alias path was exercised on PCSX2 2.5.165 at two verified
breakpoints: entry instructions and all three kernel blocks matched the
local game/native baseline, and the kernel syscall table was readable.
Synthetic protocol/capture tests are in tests/test_pcsx2_capture.py.

Captured memory is game data: captures/ is git-ignored. Never commit it.
"""
import argparse
import json
import socket
import struct
import sys
from pathlib import Path

# PINE opcodes
MSG_READ32 = 2
MSG_READ64 = 3
MSG_VERSION = 8
MSG_TITLE = 0x0B
MSG_ID = 0x0C
MSG_UUID = 0x0D
MSG_GAME_VERSION = 0x0E
MSG_STATUS = 0x0F
RESULT_OK = 0x00

RANGES = [(0x00000000, 0x00100000), (0x00100000, 0x008A215C), (0x01FF8000, 0x02000000)]
# Timer registers, read once each. Reading COUNT has no side effect on real
# hardware; MODE flags are only cleared by writes.
TIMER_REGS = [0x10000000 + t * 0x800 + r * 0x10 for t in range(4) for r in range(3)]
BATCH = 4096  # reads per message, well under PINE's 650000-byte request limit
RAM_SIZE = 0x02000000
RAM_ALIAS = 0x80000000


class Pine:
    def __init__(self, slot):
        self.s = socket.create_connection(("127.0.0.1", slot), timeout=10)

    def _recv(self, n):
        buf = b""
        while len(buf) < n:
            chunk = self.s.recv(n - len(buf))
            if not chunk:
                raise ConnectionError("PINE connection closed")
            buf += chunk
        return buf

    def call(self, commands: bytes) -> bytes:
        self.s.sendall(struct.pack("<I", 4 + len(commands)) + commands)
        size = struct.unpack("<I", self._recv(4))[0]
        if not 5 <= size <= 450000:
            raise RuntimeError(f"Invalid PINE response size: {size}")
        body = self._recv(size - 4)
        if body[0] != RESULT_OK:
            raise RuntimeError("PINE returned failure")
        return body[1:]

    def string(self, opcode):
        data = self.call(bytes([opcode]))
        n = struct.unpack_from("<I", data)[0]
        return data[4:4 + n].rstrip(b"\0").decode(errors="replace")

    def status(self):
        return {0: "running", 1: "paused", 2: "shutdown"}.get(
            struct.unpack("<I", self.call(bytes([MSG_STATUS])))[0], "unknown")

    def read_range(self, start, end):
        """Read aligned guest virtual memory, without reading past end."""
        if start < 0 or end > 0x100000000 or start > end or start % 8 or end % 4:
            raise ValueError("Range must start on 8 bytes and end on 4 bytes")
        out = bytearray()
        addr = start
        while end - addr >= 8:
            n = min(BATCH, (end - addr) // 8)
            cmds = b"".join(struct.pack("<BI", MSG_READ64, addr + i * 8) for i in range(n))
            data = self.call(cmds)
            if len(data) != n * 8:
                raise RuntimeError("PINE returned an incomplete RAM batch")
            out += data
            addr += n * 8
        if addr < end:
            out += struct.pack("<I", self.read32(addr))
        return bytes(out)

    def read_ram(self, start, end):
        """Read physical EE RAM through the kernel's direct virtual alias."""
        if not 0 <= start <= end <= RAM_SIZE:
            raise ValueError("Range is outside the 32 MiB EE RAM")
        return self.read_range(RAM_ALIAS + start, RAM_ALIAS + end)

    def read32(self, addr):
        return struct.unpack("<I", self.call(struct.pack("<BI", MSG_READ32, addr)))[0]

    def close(self):
        self.s.close()


def capture(p, out):
    """Publish files only after a complete read with paused endpoint checks."""
    if p.status() != "paused":
        raise RuntimeError("Pause PCSX2 at the intended breakpoint before capture")
    if out.exists() and any(out.iterdir()):
        raise RuntimeError("Use a new or empty output directory; existing captures are preserved")
    info = {"pcsx2_version": p.string(MSG_VERSION), "status": "paused",
            "capture_format": 2, "ram_addressing": "physical via 0x80000000 direct alias",
            "ranges": [{"physical_start": hex(s), "physical_end": hex(e),
                        "pine_start": hex(RAM_ALIAS + s)} for s, e in RANGES]}
    for key, op in (("game_id", MSG_ID), ("game_title", MSG_TITLE),
                    ("game_uuid", MSG_UUID), ("game_version", MSG_GAME_VERSION)):
        info[key] = p.string(op)
    dumps = {}
    for start, end in RANGES:
        if p.status() != "paused":
            raise RuntimeError("PCSX2 resumed during capture; no files saved")
        dumps[f"ram_{start:08x}_{end:08x}.bin"] = p.read_ram(start, end)
    if not any(dumps["ram_00000000_00100000.bin"][:0x80000]):
        raise RuntimeError("Kernel RAM is all zero; invalid reference capture")
    info["timers"] = {f"{r:#010x}": f"{p.read32(r):#010x}" for r in TIMER_REGS}
    info["status_after"] = p.status()
    if info["status_after"] != "paused":
        raise RuntimeError("PCSX2 resumed during capture; no files saved")
    # Endpoint checks cannot detect a resume-and-pause between queries.
    info["pause_check"] = "before each range and after reads; keep paused throughout"
    out.mkdir(parents=True, exist_ok=True)
    for name, data in dumps.items():
        (out / name).write_bytes(data)
    (out / "info.json").write_text(json.dumps(info, indent=2))
    return info


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", required=True)
    ap.add_argument("--slot", type=int, default=28011)
    a = ap.parse_args()
    out = Path(a.out)
    p = Pine(a.slot)
    try:
        info = capture(p, out)
    finally:
        p.close()
    print(json.dumps(info, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
