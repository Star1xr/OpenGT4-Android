#!/usr/bin/env python3
"""Where the executable registers an Adhoc native by name: the function
given with the name (the code loads the name's address, then, a few
instructions on, the function's with lui/addiu).

  python tools/find_native.py <image.bin> <splat text.s> Name [Name ...]

image.bin: the executable's image (CORE.GT4 unpacked, 32 MB of RAM)."""
import re, sys

image = open(sys.argv[1], 'rb').read()
lines = open(sys.argv[2], encoding='utf-8', errors='replace').read().splitlines()
index = {}
for n, l in enumerate(lines):
    m = re.search(r'%lo\(D_([0-9A-F]{8})\)', l)
    if m: index.setdefault(int(m.group(1), 16), []).append(n)

for name in sys.argv[3:]:
    found = []
    at = image.find(name.encode() + b'\0')
    while at >= 0:
        if at == 0 or image[at - 1] == 0:
            for n in index.get(at, []):
                fn = None
                for k in range(n, min(n + 40, len(lines))):
                    m = re.search(r'%lo\((func_[0-9A-F]{8})\)', lines[k])
                    if m: fn = m.group(1); break
                owner = next((lines[k].split()[1] for k in range(n, 0, -1) if lines[k].startswith('glabel')), '?')
                found.append('%s (registered in %s)' % (fn, owner))
        at = image.find(name.encode() + b'\0', at + 1)
    print(name, ':', ', '.join(sorted(set(found))) or 'not found')
