#!/usr/bin/env python3
"""Disassemble an IOP module (IRX) with import calls named.

Usage:
  python tools/irx_dis.py MODULE.irx [--headers DIR] [--from ADDR --to ADDR]

Reads the module's import tables (magic 0x41e00000: library name, then
`jr $ra; addiu $zero, $zero, N` stubs) and names each stub `lib:N`, or
`lib:Name` when a ps2sdk header in --headers declares `DECLARE_IMPORT(N, Name)`
for that library. Addresses are link addresses (the module is relocatable and
loads elsewhere). Relocations are not applied, so `lui`/`addiu` constants are
module-relative.

Input is extracted from the user's own disc (extracted/ is git-ignored);
the output is only printed.
"""
import argparse
import re
import struct
import sys
from pathlib import Path

import rabbitizer as R

R.config.pseudos_enablePseudos = False
R.config.regNames_gprAbiNames = R.Abi.O32


def load_header_names(folder: Path):
    """{ (header stem, number): name } from DECLARE_IMPORT lines."""
    names = {}
    if not folder or not folder.is_dir():
        return names
    for h in folder.glob("*.h"):
        for num, name in re.findall(r"DECLARE_IMPORT\((\d+),\s*(\w+)\)", h.read_text(errors="ignore")):
            names[(h.stem.lower(), int(num))] = name
    return names


def text_of(irx: bytes):
    phoff = struct.unpack_from("<I", irx, 28)[0]
    phn = struct.unpack_from("<H", irx, 44)[0]
    for i in range(phn):
        t, off, va, _, fs, ms, _, _ = struct.unpack_from("<8I", irx, phoff + 32 * i)
        if t == 1:
            return irx[off:off + fs], va
    raise ValueError("no PT_LOAD")


def imports(text: bytes, names):
    stubs = {}
    words = struct.unpack_from(f"<{len(text) // 4}I", text)
    i = 0
    while i < len(words):
        if words[i] == 0x41E00000:
            lib = text[i * 4 + 12:i * 4 + 20].rstrip(b"\0").decode(errors="replace")
            j = i + 5
            while j + 1 < len(words) and words[j] == 0x03E00008 and words[j + 1] >> 16 == 0x2400:
                n = words[j + 1] & 0xFFFF
                stubs[j * 4] = f"{lib}:{names.get((lib.lower(), n), n)}"
                j += 2
            i = j
        else:
            i += 1
    return stubs


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("irx")
    ap.add_argument("--headers", default=None)
    ap.add_argument("--from", dest="lo", type=lambda s: int(s, 0), default=None)
    ap.add_argument("--to", dest="hi", type=lambda s: int(s, 0), default=None)
    a = ap.parse_args()
    irx = Path(a.irx).read_bytes()
    text, base = text_of(irx)
    stubs = imports(text, load_header_names(Path(a.headers) if a.headers else None))
    lo = a.lo if a.lo is not None else base
    hi = a.hi if a.hi is not None else base + len(text)
    for pc in range(lo, hi, 4):
        w = struct.unpack_from("<I", text, pc - base)[0]
        ins = R.Instruction(w, vram=pc, category=R.InstrCategory.CPU)
        s = ins.disassemble()
        if ins.getOpcodeName() in ("jal", "j"):
            t = ((pc + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
            if t in stubs:
                s += f"    ; {stubs[t]}"
        if pc in stubs:
            s += f"    ; <stub {stubs[pc]}>"
        print(f"{pc:06x} {w:08x}  {s}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
