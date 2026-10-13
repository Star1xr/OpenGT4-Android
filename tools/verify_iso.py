#!/usr/bin/env python3
"""Check that a user-supplied disc image matches a supported version by SHA-1.

Usage: python tools/verify_iso.py path/to/your.iso

Reads config/supported_discs.json. Nothing is uploaded or copied anywhere.
"""
import hashlib
import json
import sys
from pathlib import Path

CONFIG = Path(__file__).resolve().parent.parent / "config" / "supported_discs.json"


def sha1_of(path: Path) -> str:
    h = hashlib.sha1()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2

    iso = Path(sys.argv[1])
    if not iso.is_file():
        print(f"file not found: {iso}")
        return 2

    known = [d for d in json.loads(CONFIG.read_text())["discs"] if d.get("sha1")]
    if not known:
        print("no hashes configured yet in config/supported_discs.json")
        return 2

    digest = sha1_of(iso)
    for d in known:
        if d["sha1"].lower() == digest:
            print(f"OK: {d['region']} ({d.get('serial', '')})")
            return 0

    print(f"unsupported disc image (sha1 {digest})")
    return 1


if __name__ == "__main__":
    sys.exit(main())
