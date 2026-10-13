"""Compare a captured endurance stall with original 0034DB30.

The capture is native test RAM, not a PCSX2 trajectory. Only the camera
gate is supplied by the fixture; solver, AI, barriers and fuel are intact.
Game RAM and the recompiled oracle stay local, outside Git.
"""
import os
from pathlib import Path
import struct
import subprocess
import sys


exe, memory, destination = sys.argv[1:]
ram = bytearray(Path(memory).read_bytes())
assert len(ram) == 0x2000000
# series_playable's local GT40 trace: conductor/car addresses are fixture
# metadata, not original allocation addresses or a new game rule.
cond = 0x01cf8ed0
cars = struct.unpack_from('<I', ram, cond + 0xf8a0)[0]
assert cars == 6
player = cond + 0x6698
f32 = lambda offset: struct.unpack_from('<f', ram, player + offset)[0]
distance = f32(0x6d4)
assert f32(0x5d4) > 0 and ram[player + 0x5c0] == 0
assert ram[player + 0x56b] == 1 and f32(0x14 + 0x34) > 0
# 0035F658 +0..14 loads D_006208B0 and branches to its return when nonzero.
# This skips camera presentation/thread creation on the original path.
# It changes no car, course, AI, vehicle parameters or physics formula.
struct.pack_into('<I', ram, 0x006208b0, 1)
root = Path(destination)
root.mkdir(parents=True, exist_ok=True)
fixture = root / 'headless-eeMemory.bin'
fixture.write_bytes(ram)
# Match the native application's presentation boundary; no HLE expansion.
env = {key: value for key, value in os.environ.items()
       if not key.startswith('PHYS_') and key != 'OPENGT4_COVERAGE'}
env['PHYS_HEADLESS'] = '1'
frames = 600  # bounded regression length, not a game timing constant
for name, native in (('original', []), ('native', ['all'])):
    target = root / name
    target.mkdir(exist_ok=True)
    with (root / (name + '.log')).open('wb') as output:
        subprocess.run([exe, str(fixture), '-', hex(cond), str(frames),
                        str(target), *native], env=env, stdout=output,
                       stderr=subprocess.STDOUT, check=True, timeout=60)
original = (root / 'original/cars.bin').read_bytes()
native = bytearray((root / 'native/cars.bin').read_bytes())
stride = cars * 0x10a0
assert len(original) == len(native) == (frames + 1) * stride
if os.getenv('GT4_ENDURANCE_STALL_NEGATIVE'):
    native[stride + 0x104] ^= 1
if original != native:
    at = next(i for i, (a, b) in enumerate(zip(original, native)) if a != b)
    print('FAIL frame', at // stride, 'car', at // 0x10a0 % cars,
          'offset', hex(at % 0x10a0), file=sys.stderr)
    sys.exit(1)
# Both paths continue to press against the barrier without lap progress.
# This does not prove how either trajectory reached the captured state.
assert struct.unpack_from('<f', original, frames * stride + 0x6d4)[0] == distance
print('PASS', frames, 'endurance steps,', cars, 'cars,', len(original),
      'bytes identical to original; captured AI stall persists on both paths.')
