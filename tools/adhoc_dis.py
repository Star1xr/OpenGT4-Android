#!/usr/bin/env python3
"""Disassemble GT4 compiled Adhoc scripts (ADCH version 5).

Usage: python tools/adhoc_dis.py SCRIPT.adc [> out.txt]

Format from the GT Modding Hub description and the GTAdhocToolchain (MIT)
instruction layouts; see docs/ADHOC.md. Input is extracted from the user's
own disc; nothing is written except the listing.
"""
import struct
import sys

OPS = {
    0: ("ARRAY_CONST_OLD", "i"), 1: ("ASSIGN_OLD", ""), 2: ("ATTRIBUTE_DEFINE", "s"),
    3: ("ATTRIBUTE_PUSH", "s"), 4: ("BINARY_ASSIGN_OPERATOR", "s"), 5: ("BINARY_OPERATOR", "s"),
    6: ("CALL", "i"), 7: ("CLASS_DEFINE", "sS"), 8: ("EVAL", ""), 9: ("FLOAT_CONST", "f"),
    10: ("FUNCTION_DEFINE", "sF"), 11: ("IMPORT", "Ss"), 12: ("INT_CONST", "i"), 13: ("JUMP", "i"),
    14: ("JUMP_IF_TRUE", "i"), 15: ("JUMP_IF_FALSE", "i"), 16: ("LIST_ASSIGN_OLD", "i"),
    17: ("LOCAL_DEFINE", "s"), 18: ("LOGICAL_AND_OLD", "i"), 19: ("LOGICAL_OR_OLD", "i"),
    20: ("METHOD_DEFINE", "sF"), 21: ("MODULE_DEFINE", "S"), 22: ("NIL_CONST", ""), 23: ("NOP", ""),
    24: ("POP_OLD", ""), 25: ("PRINT", "i"), 26: ("REQUIRE", ""), 27: ("SET_STATE_OLD", "b"),
    28: ("STATIC_DEFINE", "s"), 29: ("STRING_CONST", "s"), 30: ("STRING_PUSH", "i"), 31: ("THROW", ""),
    32: ("TRY_CATCH", "i"), 33: ("UNARY_ASSIGN_OPERATOR", "s"), 34: ("UNARY_OPERATOR", "s"),
    35: ("UNDEF", "S"), 36: ("VARIABLE_PUSH", "Si"), 37: ("ATTRIBUTE_EVAL", "S"),
    38: ("VARIABLE_EVAL", "Si"), 39: ("SOURCE_FILE", "s"), 40: ("FUNCTION_CONST", "F"),
    41: ("METHOD_CONST", "F"), 42: ("MAP_CONST_OLD", "i"), 43: ("LONG_CONST", "q"), 44: ("ASSIGN", ""),
    45: ("LIST_ASSIGN", "i"), 46: ("CALL_OLD", "i"), 47: ("OBJECT_SELECTOR", ""), 48: ("SYMBOL_CONST", "s"),
    49: ("LEAVE", "ii"), 50: ("ARRAY_CONST", "I"), 51: ("ARRAY_PUSH", ""), 52: ("MAP_CONST", ""),
    53: ("MAP_INSERT", ""), 54: ("POP", ""), 55: ("SET_STATE", "b"), 56: ("VOID_CONST", ""),
    57: ("ASSIGN_POP", ""), 58: ("U_INT_CONST", "I"), 59: ("U_LONG_CONST", "Q"), 60: ("DOUBLE_CONST", "d"),
    61: ("ELEMENT_PUSH", ""), 62: ("ELEMENT_EVAL", ""), 63: ("LOGICAL_AND", "i"), 64: ("LOGICAL_OR", "i"),
    65: ("BOOL_CONST", "b"), 66: ("MODULE_CONSTRUCTOR", ""),
}


class Reader:
    def __init__(self, data, pos=0):
        self.d, self.p = data, pos

    def take(self, fmt):
        v = struct.unpack_from("<" + fmt, self.d, self.p)[0]
        self.p += struct.calcsize(fmt)
        return v

    def sym(self):
        n = self.take("h")
        s = self.d[self.p:self.p + n].decode("euc_jp", errors="replace")
        self.p += n
        return s

    def syms(self):
        return [self.sym() for _ in range(self.take("I"))]


class Frame:
    def __init__(self, r):
        self.source = r.sym()
        self.params = [r.sym() for _ in range(r.take("I"))]
        self.stack, self.locals = r.take("i"), r.take("i")
        self.ins = []
        for _ in range(r.take("I")):
            line, op = r.take("I"), r.take("B")
            if op not in OPS:
                raise ValueError(f"unknown opcode {op} at {r.p - 1:#x}")
            name, fmt = OPS[op]
            args = []
            for f in fmt:
                args.append({"i": lambda: r.take("i"), "I": lambda: r.take("I"), "b": lambda: r.take("B"),
                             "f": lambda: r.take("f"), "d": lambda: r.take("d"), "q": lambda: r.take("q"),
                             "Q": lambda: r.take("Q"), "s": r.sym, "S": r.syms, "F": lambda: Frame(r)}[f]())
            self.ins.append((line, name, args))


def load(data):
    end = data.index(b"\0")
    magic = data[:end].decode()
    if not magic.startswith("ADCH") or int(magic[4:]) != 5:
        raise ValueError(f"not ADCH005: {magic}")
    return Frame(Reader(data, end + 1))


def dump(fr, indent=0, out=sys.stdout):
    pad = "  " * indent
    print(f"{pad}frame {fr.source!r} params={fr.params} stack={fr.stack} locals={fr.locals}", file=out)
    for i, (line, name, args) in enumerate(fr.ins):
        shown = []
        sub = None
        for a in args:
            if isinstance(a, Frame):
                sub = a
            elif isinstance(a, list):
                shown.append("::".join(a))
            else:
                shown.append(repr(a))
        print(f"{pad}{i:5} L{line:<5} {name} {' '.join(shown)}", file=out)
        if sub:
            dump(sub, indent + 1, out)


if __name__ == "__main__":
    dump(load(open(sys.argv[1], "rb").read()))
