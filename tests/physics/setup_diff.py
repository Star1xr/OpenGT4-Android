"""Car setup: the original (recompiled) against the rewrite, per car.

For every car of GENERIC_CAR (ids read from the snapshot's tables), or the
ids given, setup_run builds a new garage entry (func_00444480), the record
(func_003F3B68) and the physics parameters (func_00361908), once with the
original code and the game's in-memory tables and once with the rewrite
reading the tables from the disc image, and compares the three.

  python tests/physics/setup_diff.py <disc image> [id ...]

Also: SETUP_SEED=n (made-up records) and SETUP_GARAGE=n (made-up garage
entries) in setup_run, see tests/physics/setup_run.c.
"""
import os
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RUN = os.path.join(ROOT, 'build', 'Release', 'setup_run.exe')
SNAP = os.path.join(ROOT, 'captures', 'physics', 'c_curva.eeMemory.bin')
CONDUCTOR = '0x1bf7860'
NATIVES = '0x444480,0x3f3b68,0x361908'


def car_ids():
    m = open(SNAP, 'rb').read()
    u = lambda a: struct.unpack_from('<I', m, a)[0]
    blob = u(u(0x6235a8 + 0x134))            # GENERIC_CAR
    return [u(blob + 16 + 8 * i) for i in range(u(blob + 8))]


def run(out, car, env_extra, natives=None):
    env = dict(os.environ, SETUP_CAR=hex(car), **env_extra)
    args = [RUN, SNAP, CONDUCTOR, out] + ([natives] if natives else [])
    return subprocess.run(args, env=env, capture_output=True, text=True).returncode


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    disc = sys.argv[1]
    ids = [int(x, 0) for x in sys.argv[2:]] or car_ids()
    same = differ = 0
    with tempfile.TemporaryDirectory() as tmp:
        a, b = os.path.join(tmp, 'orig'), os.path.join(tmp, 'native')
        os.makedirs(a)
        os.makedirs(b)
        for car in ids:
            ra = run(a, car, {})
            rb = run(b, car, {'SETUP_DISC': disc}, NATIVES)
            if ra != rb:
                print('%#x: original %d, rewrite %d' % (car, ra, rb))
                differ += 1
                continue
            if ra == 3:
                continue
            bad = [f for f in ('entry', 'record', 'params')
                   if open(os.path.join(a, f + '.bin'), 'rb').read() != open(os.path.join(b, f + '.bin'), 'rb').read()]
            if bad:
                print('%#x: %s differ' % (car, ', '.join(bad)))
                differ += 1
            else:
                same += 1
    print('%d cars identical, %d differ' % (same, differ))
    return 1 if differ else 0


if __name__ == '__main__':
    sys.exit(main())
