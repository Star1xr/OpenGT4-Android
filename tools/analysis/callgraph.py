# Call graph of the disassembly (splat/asm/core/text.s, local): callees, size and FPU
# instruction counts per function, written to build/crs/callgraph.json. Run from the repo root.
import re, collections, json
funcs={}; order=[]; cur=None
calls=collections.defaultdict(set); size=collections.Counter(); fpu=collections.Counter()
fops=set('add.s sub.s mul.s div.s sqrt.s madd.s msub.s adda.s mula.s madda.s rsqrt.s neg.s abs.s max.s min.s c.lt.s c.le.s c.eq.s cvt.s.w cvt.w.s'.split())
for l in open('splat/asm/core/text.s',encoding='utf-8',errors='replace'):
    if l.startswith('glabel '):
        m=re.match(r'glabel func_([0-9A-F]{8})',l); cur=int(m.group(1),16) if m else None; continue
    if cur is None: continue
    m=re.search(r'\*/\s+([\w.]+)\s*(\S*)',l)
    if not m: continue
    op,arg=m.group(1),m.group(2); size[cur]+=1
    if op in fops: fpu[cur]+=1
    if op=='jal':
        t=re.match(r'func_([0-9A-F]{8})',arg)
        if t: calls[cur].add(int(t.group(1),16))
json.dump({'calls':{hex(k):[hex(x) for x in v] for k,v in calls.items()},'size':{hex(k):v for k,v in size.items()},'fpu':{hex(k):v for k,v in fpu.items()}},open('build/crs/callgraph.json','w'))
print('functions',len(size))
