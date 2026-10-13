"""Compare the original 0034DB30 and native step from one Training capture.

Game RAM and generated original C stay local. Controller seed is test data.
The full car array is compared byte for byte, with a deliberate corruption
control. This proves neither initial assembly nor an entire teacher replay.
"""
import os
from pathlib import Path
import struct
import subprocess
import sys

exe, memory, scratch, destination = sys.argv[1:]
ram = Path(memory).read_bytes()
assert len(ram) == 0x2000000
matches = [p for p in range(0x100000, len(ram) - 0x10144, 4)
           if struct.unpack_from('<I', ram, p + 0x10140)[0] == 0x685f50]
assert len(matches) == 1, matches
cond = matches[0]
cars = struct.unpack_from('<I', ram, cond + 0xf8a0)[0]
assert 1 <= cars <= 6
# Longer original runs reach the unrecompiled VU0 audio path at frame 93.
# The bounded differential comparison uses no substitute for that path.
frames = 64
root = Path(destination)
root.mkdir(parents=True, exist_ok=True)
# Forward pad queries to the original controller on both paths. The
# native application's idle-pad boundary otherwise bypasses synthetic
# READ_PAD test data for the AI profile. No solver is substituted here.
env = dict(os.environ, PHYS_INPUT_SEED='854923', PHYS_INPUT_SHIFT='1', PHYS_PAD_HOST='1')
for key in ('PHYS_POKE', 'PHYS_POKE_CAR', 'PHYS_POKE_CARS', 'PHYS_POKE_COND',
            'PHYS_FRAME', 'PHYS_AB', 'PHYS_BUILD', 'PHYS_HEADLESS', 'PHYS_BEGIN',
            'PHYS_DISC', 'PHYS_PAD_FIXED'):
    env.pop(key, None)
for name, native in (('original', []), ('native', ['all'])):
    target = root / name
    target.mkdir(exist_ok=True)
    with (root / (name + '.log')).open('wb') as output:
        subprocess.run([exe, memory, scratch, hex(cond), str(frames), str(target),
                        *native], env=env, stdout=output,
                       stderr=subprocess.STDOUT, check=True, timeout=60)
original = (root / 'original/cars.bin').read_bytes()
native = bytearray((root / 'native/cars.bin').read_bytes())
assert len(original) == len(native) == (frames + 1) * cars * 0x10a0
if os.getenv('GT4_TRAINING_STEP_NEGATIVE'):
    native[cars * 0x10a0 + 0x104] ^= 1
if original != native:
    at = next(i for i, (a, b) in enumerate(zip(original, native)) if a != b)
    print('FAIL at frame', at // (cars * 0x10a0), 'car', at // 0x10a0 % cars,
          'offset', hex(at % 0x10a0), file=sys.stderr)
    sys.exit(1)
print('PASS', frames, 'Training steps,', cars, 'cars,', len(original),
      'bytes identical to original from the same capture; dynamics unchanged.')
