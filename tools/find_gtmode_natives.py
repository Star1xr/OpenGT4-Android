#!/usr/bin/env python3
"""Locate Adhoc native registrations in the user's local disassembly.
String references identify registration sites, not validated implementations.
Output contains addresses only; no original instructions or tables.
"""
import argparse
import bisect
import json
import re
from pathlib import Path
from find_named_strings import BASE, function_starts, references


def locate(core, lines, starts, refs, name):
    hits=[]
    key=name.encode('ascii')+b'\0'
    at=0
    while (pos:=core.find(key,at))>=0:
        at=pos+1
        if pos and core[pos-1]:
            continue
        address=BASE+pos
        for reference in sorted(refs.get(address,())):
            index=next((i for i,l in enumerate(lines) if f' {reference:08X} ' in l),None)
            if index is None:
                continue
            callback=None
            for line in lines[index:index+100]:
                match=re.search(r'addiu\s+\$a2,.*%lo\(func_([0-9A-F]{8})\)',line)
                if match:
                    callback=int(match[1],16)
                if 'jal        func_002F3818' in line:
                    break
            else:
                callback=None
            owner=starts[bisect.bisect_right(starts,reference)-1]
            hits.append({'name':name,'string':f'0x{address:08x}',
                         'reference':f'0x{reference:08x}',
                         'registration_function':f'0x{owner:08x}',
                         'callback':f'0x{callback:08x}' if callback else None,
                         'status':'registration candidate; inspect callback/callees'})
    return hits


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('names',nargs='+')
    parser.add_argument('--core',type=Path,default=Path('extracted/core.bin'))
    parser.add_argument('--asm',type=Path,default=Path('splat/asm/core/text.s'))
    args=parser.parse_args()
    core=args.core.read_bytes();lines=args.asm.read_text(encoding='utf-8').splitlines()
    starts=function_starts(args.asm);refs=references(core)
    print(json.dumps([hit for name in args.names for hit in locate(core,lines,starts,refs,name)],indent=2))


if __name__=='__main__':
    main()
