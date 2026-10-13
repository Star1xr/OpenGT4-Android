"""Compare the GT40's approach to the barrier, not only its stalled state.

The input is a native checkpoint before the observed exit. The original
camera gate is the only changed RAM word; presentation is a boundary.
This proves this window, not the hours preceding the checkpoint.
"""
import argparse
import os
from pathlib import Path
import struct
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("exe")
    parser.add_argument("memory", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--frames", type=int, default=15000)
    parser.add_argument("--production", help="Native production-caller checkpoint executable")
    parser.add_argument("--disc", help="Required with --production")
    parser.add_argument("--timeout", type=int, default=600, help="Diagnostic wall-clock budget per replay")
    parser.add_argument("--digest", action="store_true", help="Windows: SHA256 of every full state plus the raw player odometer")
    args = parser.parse_args()
    assert bool(args.production) == bool(args.disc)
    ram = bytearray(args.memory.read_bytes())
    assert len(ram) == 0x2000000
    cond, player = 0x1cf8ed0, 0x1cff568  # local fixture metadata
    cars = struct.unpack_from("<I", ram, cond + 0xf8a0)[0]
    assert cars == 6 and ram[player + 0x56b] == 1
    initial_distance = struct.unpack_from("<f", ram, player + 0x6d4)[0]
    # Original 0035F658 +0..14: skip camera thread creation only.
    struct.pack_into("<I", ram, 0x6208b0, 1)
    args.destination.mkdir(parents=True, exist_ok=True)
    fixture = args.destination / "headless-eeMemory.bin"
    fixture.write_bytes(ram)
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(("PHYS_", "GT4_SERIES_")) and k != "OPENGT4_COVERAGE"}
    env["PHYS_HEADLESS"] = "1"
    env["PHYS_FRAME"] = "1"
    env["PHYS_SERIES_INPUT"] = "1"
    if args.digest:
        env["PHYS_STATE_DIGEST"] = "1"
    for name, native in (("original", []), ("native", ["all"])):
        folder = args.destination / name
        folder.mkdir(exist_ok=True)
        with (args.destination / (name + ".log")).open("wb") as output:
            subprocess.run([args.exe, str(fixture), "-", hex(cond), str(args.frames), str(folder), *native],
                           stdout=output, stderr=subprocess.STDOUT, env=env, check=True, timeout=args.timeout)
    stride = 36 if args.digest else cars * 0x10a0
    original = args.destination / "original/cars.bin"
    native = args.destination / "native/cars.bin"
    total = (args.frames + 1) * stride
    assert original.stat().st_size == native.stat().st_size == total
    if os.getenv("GT4_ENDURANCE_TRAJECTORY_NEGATIVE"):
        with native.open("r+b") as output:
            corruption = stride + (0 if args.digest else 0x104)
            output.seek(corruption)
            altered = output.read(1)[0] ^ 1
            output.seek(corruption)
            output.write(bytes([altered]))
    def compare(other):
        assert other.stat().st_size == total
        with original.open("rb") as reference, other.open("rb") as actual:
            offset = 0
            while block := reference.read(stride * 256):
                candidate = actual.read(len(block))
                if block != candidate:
                    at = offset + next(i for i, (a, b) in enumerate(zip(block, candidate)) if a != b)
                    raise AssertionError(f"{other.name} differs frame={at // stride} state offset={at % stride:#x} digest={args.digest}")
                offset += len(block)
    compare(native)
    if args.production:
        production = args.destination / "production-cars.bin"
        with (args.destination / "production.log").open("wb") as output:
            subprocess.run([args.production, args.disc, str(args.memory), str(production), str(args.frames)],
                           stdout=output, stderr=subprocess.STDOUT, env=env, check=True, timeout=args.timeout)
        compare(production)
    maximum, previous, last_progress = initial_distance, initial_distance, 0
    with original.open("rb") as reference:
        for frame in range(args.frames + 1):
            state = reference.read(stride)
            distance = struct.unpack_from("<f", state, 32 if args.digest else 0x6d4)[0]
            maximum = max(maximum, distance)
            if distance != previous:
                last_progress = frame
            previous = distance
    assert maximum > initial_distance + 100, "fixture starts after the exit"
    assert last_progress < args.frames - 600, "window did not reach the sustained stall"
    method = "SHA256 of every full state" if args.digest else "raw state bytes"
    print(f"PASS {args.frames} frames / {cars} cars / {method} / {total} compared bytes; "
          f"approach advances {maximum - initial_distance:.3f} m, last progress frame {last_progress}. "
          "The original also reaches the barrier from this checkpoint; earlier trajectory remains outside this proof.")


if __name__ == "__main__":
    main()
