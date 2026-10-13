# Summary of MIPS functions in the local disassembly (splat/asm/core/text.s):
# size, FPU ops, sqrt/div, float constants, backward branches (loops), callees
# and the base-register offsets of float loads/stores. Run from the repo root:
#   python tools/analysis/funcinfo.py 0x34db30 0x344530 ...
import re, sys, struct, collections, json
want = {int(a, 16) for a in sys.argv[1:]}
fops = set('add.s sub.s mul.s div.s sqrt.s madd.s msub.s adda.s mula.s madda.s rsqrt.s neg.s abs.s max.s min.s c.lt.s c.le.s c.eq.s cvt.s.w cvt.w.s'.split())
body = collections.defaultdict(list); cur = None
for l in open('splat/asm/core/text.s', encoding='utf-8', errors='replace'):
    if l.startswith('glabel '):
        m = re.match(r'glabel func_([0-9A-F]{8})', l)
        cur = int(m.group(1), 16) if m else None
        continue
    if cur in want:
        m = re.search(r'/\*\s*\w+\s+([0-9A-F]{8})\s+\w+\s*\*/\s+([\w.]+)\s*(.*)', l)
        if m: body[cur].append((int(m.group(1), 16), m.group(2), m.group(3).strip()))
cg = json.load(open('build/crs/callgraph.json'))
def reach_fpu(f):
    seen, st = set(), [hex(f)]
    while st:
        x = st.pop()
        if x in seen: continue
        seen.add(x); st += cg['calls'].get(x, [])
    return sum(cg['fpu'].get(x, 0) for x in seen), len(seen)
for f in sorted(want, key=lambda a: sys.argv.index(hex(a)) if hex(a) in sys.argv else 0):
    ins = body[f]; ops = collections.Counter(o for _, o, _ in ins)
    consts = []; hi = {}
    for a, o, arg in ins:
        m = re.match(r'\$(\w+), \((0x[0-9A-F]+) >> 16\)', arg)
        if o == 'lui' and m:
            hi[m.group(1)] = int(m.group(2), 16)
        m = re.match(r'\$(\w+), \$(f\d+)', arg)
        if o == 'mtc1' and m and m.group(1) in hi:
            v = struct.unpack('<f', struct.pack('<I', hi[m.group(1)]))[0]
            if f'{v:g}' not in consts: consts.append(f'{v:g}')
    loops = sum(1 for a, o, arg in ins if o.startswith('b') and (t := re.search(r'\.L([0-9A-F]{8})', arg)) and int(t.group(1), 16) < a)
    offs = collections.Counter()
    for a, o, arg in ins:
        m = re.match(r'\$f\d+, (-?0x[0-9A-F]+|\d+)\(\$(\w+)\)', arg)
        if o in ('lwc1', 'swc1') and m and m.group(2) != 'sp':
            offs[(m.group(2), int(m.group(1), 16) if 'x' in m.group(1) else int(m.group(1)), o[0])] += 1
    calls = [arg.replace('func_', '') for a, o, arg in ins if o == 'jal']
    rf, rn = reach_fpu(f)
    print(f'== {f:#x}: {len(ins)} ins, fpu {sum(ops[x] for x in fops)} (reach {rf} in {rn} funcs), '
          f'sqrt {ops["sqrt.s"]} rsqrt {ops["rsqrt.s"]} div {ops["div.s"]} loops {loops} jalr {ops["jalr"]}')
    print('   consts:', ' '.join(consts[:30]))
    print('   calls:', ' '.join(dict.fromkeys(calls)))
    rd = sorted(k for k in offs if k[2] == 'l'); wr = sorted(k for k in offs if k[2] == 's')
    print('   loads:', ' '.join(f'{r}+{o:#x}' for r, o, _ in rd[:40]))
    print('   stores:', ' '.join(f'{r}+{o:#x}' for r, o, _ in wr[:40]))
