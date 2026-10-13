#!/usr/bin/env python3
"""Name Sony library functions from their own error strings.

Usage: python tools/find_named_strings.py [path/to/splat/asm/core/text.s]

Needs extracted/core.bin and the splat output (for function starts).
Looks in rodata for strings that start with `sceXxx:` / `sceXxx(` (a function
name) or `libxxx:` (a library name), finds the functions that reference them
(lui + addiu pairs), and prints symbol_addrs.txt lines:

  sceXxx: one referencing function  -> [provisional] name from string only
  sceXxx: several functions         -> comment only, ambiguous
  libxxx:                           -> comment only, library membership

A name from a string alone is never [verified]: the function could be a
wrapper or a caller that prints another function's name.
"""
import bisect
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASE = 0x00100000
TEXT_END = 0x00617A14
RODATA = 0x0068BA80     # provisional start, see docs/analysis/CORE_FORMAT.md
DATA_END = 0x006D5DFC

FUNC_RE = re.compile(rb"\s*(sce[A-Z]\w+)\s*(?:\(\))?\s*:|\s*(sce[A-Z]\w+)\(\)")
LIB_RE = re.compile(rb"\s*(lib[a-z0-9]+)\s*:")


def function_starts(text_s: Path):
    starts, lines = [], text_s.read_text(encoding="utf-8").splitlines()
    for i, line in enumerate(lines):
        if line.startswith("glabel "):
            m = re.search(r"/\* [0-9A-F]+ ([0-9A-F]{8}) ", lines[i + 1])
            if m:
                starts.append(int(m.group(1), 16))
    return sorted(starts)


def references(core: bytes):
    words = struct.unpack_from(f"<{(TEXT_END - BASE) // 4}I", core)
    hi, refs = {}, defaultdict(set)
    for i, w in enumerate(words):
        op = w >> 26
        if op == 0x0F:                      # lui
            hi[(w >> 16) & 31] = ((w & 0xFFFF) << 16, i)
        elif op == 0x09 and (w >> 21) & 31 in hi:  # addiu rt, rs(lui), lo
            h, at = hi[(w >> 21) & 31]
            if i - at < 16:
                lo = w & 0xFFFF
                lo -= 0x10000 if lo & 0x8000 else 0
                refs[(h + lo) & 0xFFFFFFFF].add(BASE + i * 4)
    return refs


def main() -> int:
    text_s = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "splat" / "asm" / "core" / "text.s"
    core = (ROOT / "extracted" / "core.bin").read_bytes()
    starts = function_starts(text_s)
    refs = references(core)
    owner = lambda a: starts[bisect.bisect_right(starts, a) - 1]

    named, libs = defaultdict(set), defaultdict(set)
    for m in re.finditer(rb"[\x20-\x7e\t\n]{4,}\x00", core[RODATA - BASE:DATA_END - BASE]):
        addr = RODATA + m.start()
        if core[addr - BASE - 1] != 0 or addr not in refs:
            continue
        s = m.group()[:-1]
        fns = {owner(r) for r in refs[addr]}
        if f := FUNC_RE.match(s):
            named[(f.group(1) or f.group(2)).decode()].update((fn, addr) for fn in fns)
        elif (lib := LIB_RE.match(s)) and lib.group(1) != b"library":
            for fn in fns:
                libs[fn].add(lib.group(1).decode())

    print("// Sony library functions named from their own error strings. tools/find_named_strings.py")
    taken = set()
    for name in sorted(named):
        fns = sorted({fn for fn, _ in named[name]})
        strs = sorted({a for _, a in named[name]})
        if len(fns) == 1 and fns[0] not in taken:
            taken.add(fns[0])
            print(f"{name} = {fns[0]:#010x}; // type:func [provisional] string only, "
                  f"{len(strs)} message(s) at {', '.join(f'{a:#010x}' for a in strs)}")
        else:
            print(f"//   {name} ambiguous, string referenced by "
                  f"{', '.join(f'{f:#010x}' for f in fns)}")
    print("// Library membership only (libxxx prefix), no function name:")
    for fn in sorted(libs):
        print(f"//   {fn:#010x} {', '.join(sorted(libs[fn]))}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
