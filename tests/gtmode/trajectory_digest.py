"""Check test-output fingerprints against every byte of a raw oracle run."""
import argparse
import hashlib
import os
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("raw", type=Path)
    parser.add_argument("digest", type=Path)
    parser.add_argument("--cars", type=int, default=6)
    args = parser.parse_args()
    size = args.cars * 0x10a0
    assert args.raw.stat().st_size % size == 0
    frames = args.raw.stat().st_size // size
    assert args.digest.stat().st_size == frames * 36
    with args.raw.open("rb") as raw, args.digest.open("rb") as fingerprint:
        for frame in range(frames):
            state = bytearray(raw.read(size))
            if os.getenv("GT4_STATE_DIGEST_NEGATIVE") and frame == 1:
                state[0x104] ^= 1
            expected = hashlib.sha256(state).digest() + state[0x6d4:0x6d8]
            assert fingerprint.read(36) == expected, f"fingerprint misses state bytes at frame {frame}"
    print(f"PASS {frames} full states / {args.cars} cars: CNG SHA256 and raw odometer match Python hashlib.")


if __name__ == "__main__":
    main()
