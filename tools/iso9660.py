#!/usr/bin/env python3
"""Minimal ISO 9660 reader for the user's own PS2 disc image.

Usage:
  python tools/iso9660.py path/to/your.iso ls [DIR]
  python tools/iso9660.py path/to/your.iso cat FILE
  python tools/iso9660.py path/to/your.iso extract [OUT_DIR]   (default: extracted/disc)

Only reads the image. Extracted files stay on your machine (extracted/ is git-ignored).
"""
import struct
import sys
from pathlib import Path

SECTOR = 2048
ROOT = Path(__file__).resolve().parent.parent


class Entry:
    def __init__(self, name, lba, size, is_dir):
        self.name, self.lba, self.size, self.is_dir = name, lba, size, is_dir


class Iso:
    def __init__(self, path):
        self.f = open(path, "rb")
        pvd = self.read(16, SECTOR)
        if pvd[1:6] != b"CD001":
            raise ValueError("not an ISO 9660 image")
        self.root = self._record(pvd[156:156 + 34])

    def read(self, lba, size):
        self.f.seek(lba * SECTOR)
        return self.f.read(size)

    @staticmethod
    def _record(rec):
        lba = struct.unpack_from("<I", rec, 2)[0]
        size = struct.unpack_from("<I", rec, 10)[0]
        flags = rec[25]
        name_len = rec[32]
        name = rec[33:33 + name_len].decode("ascii", "replace").split(";")[0]
        return Entry(name, lba, size, bool(flags & 2))

    def listdir(self, d):
        data = self.read(d.lba, d.size)
        out, pos = [], 0
        while pos < len(data):
            n = data[pos]
            if n == 0:  # records never cross sectors; skip to next one
                pos = (pos // SECTOR + 1) * SECTOR
                continue
            e = self._record(data[pos:pos + n])
            if e.name not in ("\x00", "\x01"):
                out.append(e)
            pos += n
        return out

    def lookup(self, path):
        cur = self.root
        for part in [p for p in path.replace("\\", "/").split("/") if p]:
            match = [e for e in self.listdir(cur) if e.name.upper() == part.upper()]
            if not match:
                raise FileNotFoundError(path)
            cur = match[0]
        return cur

    def walk(self, d, prefix=""):
        for e in self.listdir(d):
            p = f"{prefix}/{e.name}" if prefix else e.name
            yield p, e
            if e.is_dir:
                yield from self.walk(e, p)

    def copy_to(self, e, dest):
        dest.parent.mkdir(parents=True, exist_ok=True)
        self.f.seek(e.lba * SECTOR)
        left = e.size
        with open(dest, "wb") as out:
            while left:
                chunk = self.f.read(min(left, 1 << 22))
                out.write(chunk)
                left -= len(chunk)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    iso, cmd, args = Iso(sys.argv[1]), sys.argv[2], sys.argv[3:]

    if cmd == "ls":
        for e in iso.listdir(iso.lookup(args[0] if args else "/")):
            print(f"{'d' if e.is_dir else '-'} {e.size:>12} lba={e.lba:<8} {e.name}")
    elif cmd == "cat":
        e = iso.lookup(args[0])
        sys.stdout.buffer.write(iso.read(e.lba, e.size))
    elif cmd == "extract":
        out = Path(args[0]) if args else ROOT / "extracted" / "disc"
        for p, e in iso.walk(iso.root):
            if not e.is_dir:
                print(p)
                iso.copy_to(e, out / p)
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
