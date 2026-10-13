"""Differential test of the physics rewrite (src/core/physics) against the
original step, recompiled from the user's disc (physics_run).

For each snapshot in captures/physics/*.eeMemory.bin (extracted from PCSX2
savestates, local only) the step runs N frames twice: original code, then
with every rewritten function swapped in. The car arrays must be identical,
bit for bit. Snapshots missing = skipped (they are not in the repository).

  .venv/Scripts/python tests/physics/run_diff.py [frames]
"""
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EXE = ROOT / "build" / "Release" / "physics_run.exe"
CONDUCTOR = "0x1bf7860"   # Time Trial snapshots; see docs/analysis/PHYSICS.md
STACK = (0x01e18000, 0x01ef0000)   # zero in the snapshots; physics_run's stack


def run(snap, frames, out, native=None):
    spr = str(snap).replace(".eeMemory.bin", ".Scratchpad.bin")
    args = [str(EXE), str(snap), spr, CONDUCTOR, str(frames), str(out)] + ([native] if native else [])
    r = subprocess.run(args, capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"{snap.name}: {r.stderr.strip() or r.stdout.strip()}")
    return r.stdout


def main():
    frames = int(sys.argv[1]) if len(sys.argv) > 1 else 600
    snaps = sorted((ROOT / "captures" / "physics").glob("*.eeMemory.bin"))
    if not snaps or not EXE.exists():
        print("skipped: no snapshots or physics_run not built")
        return 0
    bad = 0
    for snap in snaps:
        with tempfile.TemporaryDirectory() as t:
            o, n = Path(t, "o"), Path(t, "n")
            o.mkdir(); n.mkdir()
            run(snap, frames, o)
            log = run(snap, frames, n, "all")
            same = (o / "cars.bin").read_bytes() == (n / "cars.bin").read_bytes()
            # the rest of the RAM too, except the harness's own stack
            ro, rn = (o / "ram.bin").read_bytes(), (n / "ram.bin").read_bytes()
            lo, hi = STACK
            ram_same = ro[:lo] == rn[:lo] and ro[hi:] == rn[hi:]
            same = same and ram_same
            calls = [l.split(": ")[1] for l in log.splitlines() if l.startswith("native")]
            print(f"{snap.name.split('.')[0]:10} {'identical' if same else 'DIFFERENT'}"
                  f"{'' if ram_same else ' (RAM)'}")
            bad += not same
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
