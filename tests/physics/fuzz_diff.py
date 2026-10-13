"""Differential test on variants of the snapshots: the player's car is given
another velocity (direction and speed) and yaw rate, then the step runs N
frames with the original code and with the rewrite. Reaches paths the
recorded laps do not (walls hit from other angles, spins, higher speeds).
Same comparison as run_diff.py: car arrays and the rest of the RAM.

  .venv/Scripts/python tests/physics/fuzz_diff.py [frames] [variants per snapshot]

FUZZ_SEED picks another set of variants; FUZZ_ONLY=b_reta:17 runs one; FUZZ_NATIVES picks the rewritten
functions (default all, as physics_run takes them); FUZZ_KEEP=dir keeps the
variant's files there. FUZZ_INPUT=1 also replaces the recorded pad with
made-up commands (analog/digital, pad/wheel), a different sequence per
variant. FUZZ_SETTINGS=1 also draws the driving settings (counter-steer
modes, pedal ramps, steering-wheel response, assists, driver kind).
"""
import math
import os
import random
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EXE = ROOT / "build" / "Release" / "physics_run.exe"
CONDUCTOR = 0x1bf7860
CAR0 = CONDUCTOR + 0x6698
VELOCITY, YAW_RATE = 0x648, 0x680
STACK = (0x01e18000, 0x01ef0000)


def run(mem, spr, frames, out, native=None, env=None):
    args = [str(EXE), str(mem), str(spr), hex(CONDUCTOR), str(frames), str(out)] + ([native] if native else [])
    r = subprocess.run(args, capture_output=True, text=True, env=env)
    if r.returncode != 0:
        raise SystemExit(f"{mem.name}: {r.stderr.strip() or r.stdout.strip()}")
    return r.stderr


def main():
    frames = int(sys.argv[1]) if len(sys.argv) > 1 else 240
    per = int(sys.argv[2]) if len(sys.argv) > 2 else 8
    snaps = sorted((ROOT / "captures" / "physics").glob("*.eeMemory.bin"))
    if not snaps or not EXE.exists():
        print("skipped: no snapshots or physics_run not built")
        return 0
    rng = random.Random(int(os.environ.get("FUZZ_SEED", "4")))
    only = os.environ.get("FUZZ_ONLY")
    natives = os.environ.get("FUZZ_NATIVES", "all")
    keep = os.environ.get("FUZZ_KEEP")
    bad = 0
    extra = []
    for snap in snaps:
        spr = Path(str(snap).replace(".eeMemory.bin", ".Scratchpad.bin"))
        base = bytearray(snap.read_bytes())
        for v in range(per):
            ram = bytearray(base)
            ang = rng.uniform(-math.pi, math.pi)
            speed = rng.choice([8.0, 20.0, 40.0, 70.0])
            vz = struct.unpack_from("<f", ram, CAR0 + VELOCITY + 8)[0]
            struct.pack_into("<3f", ram, CAR0 + VELOCITY, speed * math.cos(ang), speed * math.sin(ang), vz)
            struct.pack_into("<f", ram, CAR0 + YAW_RATE, rng.uniform(-3.0, 3.0))
            input_seed = str(rng.randrange(1, 1 << 30))
            settings = [rng.randrange(3), rng.randrange(3), rng.randrange(2), rng.randrange(5),
                        rng.choice([0, 8, 0x10, 0x18]) | rng.choice([0, 2]), rng.randrange(7), rng.randrange(2), rng.randrange(4)]
            rows = [(rng.randrange(3), rng.randrange(4)) for _ in range(3)]
            model = [rng.randrange(2), rng.choice([0, 0, 1, 2]), rng.randrange(2), rng.choice([0, 0, 5]),
                     rng.randrange(2), rng.randrange(8), rng.randrange(2), rng.randrange(2), rng.randrange(2),
                     rng.choice([0, 0, 2, 3, 4])]
            blend, target = rng.choice([0.0, 0.0, 0.4, 1.0]), rng.uniform(5.0, 60.0)
            slip = [rng.randrange(2), rng.randrange(2), rng.randrange(101), rng.randrange(101),
                    rng.randrange(1, 40), rng.randrange(1, 40)]
            session_flags = [rng.randrange(2), rng.randrange(2)]   # tyre wear, heat
            wear_params = [rng.uniform(1, 5), rng.uniform(0, 1), rng.uniform(0.1, 1), rng.uniform(0.01, 1),
                           rng.uniform(0, 0.01), rng.uniform(0, 0.01), rng.uniform(0, 0.01), rng.uniform(0, 0.01),
                           rng.uniform(0, 10), rng.uniform(0, 10), rng.uniform(0, 1), 20.0, 0.0, 0.0, 0.0, 120.0]
            fuel = [rng.choice([0.0, 1.0]), rng.uniform(0, 50)]   # consumption on, fuel left
            time_limit, retire_wear = rng.choice([0, 0, 1, 2]), rng.uniform(0, 1)
            column = [rng.choice([0, 1, 1, 2]), rng.randrange(4), rng.randrange(2)]   # steering column mode, load scaling; compliance
            aids = [rng.choice([0.0, 0.0, rng.uniform(0.3, 1.5)]), rng.uniform(0, 2), rng.uniform(0, 0.5),
                    rng.uniform(0.7, 1.1)]   # traction control level, fade rate, fade start, scale
            aid_bytes = [rng.randrange(2) for _ in range(16)]   # switches at +0x30.., +0xc.., +0x1c..
            aid_table = [rng.randrange(2), rng.randrange(2), rng.randrange(2), rng.randrange(2), rng.randrange(2),
                         rng.randrange(2), rng.randrange(2), rng.randrange(2), rng.randrange(2)]
            aero = [rng.choice([0, 0, 1, 2, 3, 4, 5, 6]), rng.uniform(5, 20), rng.uniform(20, 40), rng.uniform(0.5, 4)]
            shift = [rng.randrange(3), rng.randrange(20), rng.randrange(20), rng.randrange(20)]   # mode, rpm offsets / 100
            timer, lead = rng.choice([rng.randrange(0, 20000), 200000]), rng.randrange(-1, 3)
            if os.environ.get("FUZZ_SETTINGS"):
                session = struct.unpack_from("<I", ram, CAR0 + 4)[0] & 0x1ffffff
                for addr, val in zip([0x622132, 0x622136, 0x622121, CAR0 + 0x547, CAR0 + 0x5e9,
                                      CAR0 + 0x546, CAR0 + 0x5b5, session + 0xcbdd], settings):
                    ram[addr] = val
                obj = CAR0 + 0x14   # the driver model's block
                for addr, val in zip([obj + 0x1a, obj + 0x1b, obj + 0x1f, obj + 0x52, 0x623e50 + 4, 0x623e50 + 5,
                                      0x623e50 + 8, 0x623e50 + 0xb, 0x623e54, obj + 0x1e], model):
                    ram[addr] = val
                struct.pack_into("<2f", ram, obj + 0x28, target, blend)
                for k, val in enumerate(slip):   # longitudinal slip factor table (func_00351358)
                    ram[0x620d8e + (k // 2) * 2 + k % 2] = val
                ram[CONDUCTOR + 4 + 0x68], ram[CONDUCTOR + 4 + 0x69] = session_flags
                if any(session_flags):   # the settings a race with wear fills in (0x24..0x60)
                    struct.pack_into("<16f", ram, CONDUCTOR + 4 + 0x24, *wear_params)
                struct.pack_into("<2f", ram, CAR0 + 0x5d0, *fuel)
                struct.pack_into("<H", ram, CONDUCTOR + 0xf850, time_limit)
                struct.pack_into("<f", ram, CONDUCTOR + 4 + 0x20, retire_wear)
                struct.pack_into("<2I", ram, 0x845bf0, column[0], column[1])
                ram[0x620af0] = column[2]
                params = struct.unpack_from("<I", ram, CAR0 + 0x10)[0] & 0x1ffffff
                struct.pack_into("<4f", ram, params + 0x1098 + 0x20, *aids)
                for k in range(4):
                    ram[params + 0x1098 + 0x30 + k] = aid_bytes[k]
                    ram[params + 0x1098 + 0xc + k] = aid_bytes[4 + k]
                    ram[params + 0x1098 + 0x1c + k] = aid_bytes[8 + k]
                for addr, val in zip([0x620cd8 + 0x117, 0x620cd8 + 0x146, 0x620cd8 + 0x134, 0x620cd8 + 0x135,
                                      0x620cd8 + 0x136, 0x620cd8 + 0x137, 0x6224a6, 0x6224a7, 0x6224a8], aid_table):
                    ram[addr] = val
                ram[params + 0x1077] = aero[0]   # moving aero parts
                struct.pack_into("<3f", ram, params + 0x1078, *aero[1:])
                for k in range(4):
                    ram[0x622110 + 8 + k] = shift[k]
                struct.pack_into("<I", ram, CONDUCTOR + 0xf89c, timer)   # session frames (the aim reaches further early on)
                struct.pack_into("<h", ram, CONDUCTOR + 0xf878, lead)
                for k, (damp, gain) in enumerate(rows):   # steering extrapolation per device
                    ram[0x622110 + 0x70 + k * 32 + 0x1a] = damp
                    ram[0x622110 + 0x70 + k * 32 + 0x1b] = gain
            if only and only != f"{snap.name.split('.')[0]}:{v}":
                continue
            with tempfile.TemporaryDirectory() as t:
                if keep:
                    t = keep
                    Path(t).mkdir(parents=True, exist_ok=True)
                mem = Path(t, "v.eeMemory.bin")
                mem.write_bytes(ram)
                o, n = Path(t, "o"), Path(t, "n")
                o.mkdir(exist_ok=True); n.mkdir(exist_ok=True)
                env = dict(os.environ)
                if os.environ.get("FUZZ_INPUT"):
                    env["PHYS_INPUT_SEED"] = input_seed
                run(mem, spr, frames, o, env=env)
                extra.append(run(mem, spr, frames, n, natives, env=env))
                same = (o / "cars.bin").read_bytes() == (n / "cars.bin").read_bytes()
                ro, rn = (o / "ram.bin").read_bytes(), (n / "ram.bin").read_bytes()
                lo, hi = STACK
                ram_same = ro[:lo] == rn[:lo] and ro[hi:] == rn[hi:]
                ok = same and ram_same
                print(f"{snap.name.split('.')[0]:10} v{v} ang {ang:+.2f} speed {speed:4.0f}  "
                      f"{'identical' if ok else 'DIFFERENT'}{'' if ram_same else ' (RAM)'}")
                bad += not ok
    Path(tempfile.gettempdir(), "fuzz_stderr.txt").write_text("".join(extra))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
