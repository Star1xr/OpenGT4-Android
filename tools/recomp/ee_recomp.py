#!/usr/bin/env python3
"""Static recompiler: EE (R5900) code from extracted/core.bin to C.

Usage:
  python tools/recomp/ee_recomp.py --out build/gen/initsys.c \
      --root 0x00100008:0x00100220 --stop 0x001001f0 \
      --exclude 0x0048ef90,0x00107f08,0x005a3140,0x005b7960

The output is derived from the user's own game code: it is written under
build/ and must never be committed.

Function set: the root plus everything reachable through direct `jal`, and
through `j` to another function (tail call). Addresses in --exclude stop the run. Indirect calls use the generated lookup
table; computed local jumps dispatch only within the current function.
Function boundaries come from splat's text.s (glabel lines); --root may give
an explicit end.

Each function becomes `void f_XXXXXXXX(ee_ctx *c)`. Branch delay slots are
emitted in place. `jr $ra` returns from the C function, which assumes calls
and returns are paired, as they are in compiled code. An instruction the
translator does not know is emitted as ee_unsupported(), so the build still
works and the run stops if it is reached.
"""
import argparse
import bisect
import re
import struct
import sys
from pathlib import Path

import rabbitizer as R

ROOT = Path(__file__).resolve().parents[2]
BASE = 0x00100000
TEXT_END = 0x00616F24

R.config.pseudos_enablePseudos = False


def u32(x):
    return x & 0xFFFFFFFF


def simm16(w):
    v = w & 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


class Code:
    def __init__(self, core: bytes, starts, names):
        self.core, self.names = core, names
        self.blobs = []            # (src, dst, size): code copied at run time
        self.blob_ext = {}         # blob function start -> end (by control flow)
        self._set_starts(starts)

    def _set_starts(self, starts):
        self.starts = sorted(set(starts))
        self.start_set = set(self.starts)

    def add_blob(self, src, dst, size):
        """Code the game copies from src (in core.bin) to dst before running it.
        Function starts: the entry, jal targets, and words inside the blob that
        point into it (its own handler table). Each function's extent is found
        by following its control flow, since starts can fall inside another
        function (shared tails)."""
        self.blobs.append((src, dst, size))
        starts = {dst}
        for pc in range(dst, dst + size, 4):
            w, name = instr(self, pc)
            if name == "jal":
                starts.add(((pc + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2))
        for pc in range(dst, dst + size, 4):
            v = self.word(pc)
            if dst <= v < dst + size and v % 4 == 0 and v != pc:
                starts.add(v)
        for f in starts:
            self.blob_ext[f] = self._flow_end(f, dst, dst + size)
        self._set_starts(list(self.starts) + list(starts))
        return sorted(starts)

    def _flow_end(self, start, lo, hi):
        seen, todo, last = set(), [start], start
        while todo:
            pc = todo.pop()
            if pc in seen or not (lo <= pc < hi):
                continue
            seen.add(pc)
            last = max(last, pc)
            w, name = instr(self, pc)
            base = LIKELY.get(name, name)
            if base in BRANCH_COND:
                last = max(last, pc + 4)
                todo += [u32(pc + 4 + (simm16(w) << 2)), pc + 8]
            elif name == "j":
                last = max(last, pc + 4)
                t = ((pc + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
                todo.append(t)
            elif name in ("jr",):
                last = max(last, pc + 4)
            elif name in ("jal", "jalr"):
                last = max(last, pc + 4)
                todo.append(pc + 8)
            elif name in ("eret",):
                pass
            else:
                todo.append(pc + 4)
        return last + 4

    def word(self, pc):
        for src, dst, size in self.blobs:
            if dst <= pc < dst + size:
                return struct.unpack_from("<I", self.core, src - BASE + (pc - dst))[0]
        return struct.unpack_from("<I", self.core, pc - BASE)[0]

    def end_of(self, start):
        if start in self.blob_ext:
            return self.blob_ext[start]
        limit = TEXT_END
        for _, dst, size in self.blobs:
            if dst <= start < dst + size:
                limit = dst + size
        i = bisect.bisect_right(self.starts, start)
        return min(self.starts[i], limit) if i < len(self.starts) else limit

    def name(self, a):
        return self.names.get(a, f"func_{a:08X}")


# ---------------------------------------------------------------- operands

def gpr(n):
    return f"c->r[{n}]"


def rs64(w):
    return f"c->r[{(w >> 21) & 31}].ud[0]"


def rt64(w):
    return f"c->r[{(w >> 16) & 31}].ud[0]"


def rs32(w):
    return f"c->r[{(w >> 21) & 31}].uw[0]"


def rt32(w):
    return f"c->r[{(w >> 16) & 31}].uw[0]"


def set64(reg, expr):
    return "" if reg == 0 else f"c->r[{reg}].ud[0] = (uint64_t)({expr});"


def set32(reg, expr):
    return "" if reg == 0 else f"c->r[{reg}].sd[0] = (int64_t)(int32_t)(uint32_t)({expr});"


def addr(w):
    return f"(uint32_t)(c->r[{(w >> 21) & 31}].uw[0] + {simm16(w)})"


# ---------------------------------------------------------------- translation

def translate(pc, w, name):
    """C for one non-control-flow instruction. None if not handled."""
    rs, rt, rd, sa = (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31, (w >> 6) & 31
    imm, simm = w & 0xFFFF, simm16(w)
    s64 = lambda r: f"c->r[{r}].sd[0]"
    t = {
        "nop": "",
        "plzcw": "" if rd == 0 else
            f"{{ ee_u128 src = c->r[{rs}]; for (int lane = 0; lane < 2; lane++) {{ "
            f"uint32_t x = src.uw[lane]; unsigned count = 0, sign = x >> 31; "
            f"for (int bit = 30; bit >= 0 && ((x >> bit) & 1u) == sign; bit--) count++; "
            f"c->r[{rd}].uw[lane] = count; }} }}",
        "cache": f"ee_cache(c, {rt}, {addr(w)}, {pc:#010x}u);",
        "lui": set32(rt, f"{imm:#x}u << 16"),
        "addiu": set32(rt, f"{rs32(w)} + (uint32_t){simm}"),
        "daddiu": set64(rt, f"{rs64(w)} + (uint64_t)(int64_t){simm}"),
        "andi": set64(rt, f"{rs64(w)} & {imm:#x}u"),
        "ori": set64(rt, f"{rs64(w)} | {imm:#x}u"),
        "xori": set64(rt, f"{rs64(w)} ^ {imm:#x}u"),
        "slti": set64(rt, f"{s64(rs)} < (int64_t){simm}"),
        "sltiu": set64(rt, f"{rs64(w)} < (uint64_t)(int64_t){simm}"),
        "addu": set32(rd, f"{rs32(w)} + {rt32(w)}"),
        "subu": set32(rd, f"{rs32(w)} - {rt32(w)}"),
        "daddu": set64(rd, f"{rs64(w)} + {rt64(w)}"),
        "dsubu": set64(rd, f"{rs64(w)} - {rt64(w)}"),
        "and": set64(rd, f"{rs64(w)} & {rt64(w)}"),
        "or": set64(rd, f"{rs64(w)} | {rt64(w)}"),
        "xor": set64(rd, f"{rs64(w)} ^ {rt64(w)}"),
        "nor": set64(rd, f"~({rs64(w)} | {rt64(w)})"),
        "slt": set64(rd, f"{s64(rs)} < {s64(rt)}"),
        "sltu": set64(rd, f"{rs64(w)} < {rt64(w)}"),
        "sll": set32(rd, f"{rt32(w)} << {sa}"),
        "srl": set32(rd, f"{rt32(w)} >> {sa}"),
        "sra": set32(rd, f"(uint32_t)((int32_t){rt32(w)} >> {sa})"),
        "sllv": set32(rd, f"{rt32(w)} << ({rs32(w)} & 31)"),
        "srlv": set32(rd, f"{rt32(w)} >> ({rs32(w)} & 31)"),
        "srav": set32(rd, f"(uint32_t)((int32_t){rt32(w)} >> ({rs32(w)} & 31))"),
        "dsll": set64(rd, f"{rt64(w)} << {sa}"),
        "dsrl": set64(rd, f"{rt64(w)} >> {sa}"),
        "dsra": set64(rd, f"(uint64_t)({s64(rt)} >> {sa})"),
        "dsll32": set64(rd, f"{rt64(w)} << {sa + 32}"),
        "dsrl32": set64(rd, f"{rt64(w)} >> {sa + 32}"),
        "dsra32": set64(rd, f"(uint64_t)({s64(rt)} >> {sa + 32})"),
        "dsllv": set64(rd, f"{rt64(w)} << ({rs32(w)} & 63)"),
        "dsrlv": set64(rd, f"{rt64(w)} >> ({rs32(w)} & 63)"),
        "dsrav": set64(rd, f"(uint64_t)({s64(rt)} >> ({rs32(w)} & 63))"),
        "movz": f"if ({rt64(w)} == 0) {{ {set64(rd, rs64(w))} }}" if rd else "",
        "movn": f"if ({rt64(w)} != 0) {{ {set64(rd, rs64(w))} }}" if rd else "",
        # HI/LO
        "mfhi": set64(rd, "c->hi"),
        "mflo": set64(rd, "c->lo"),
        "mthi": f"c->hi = {rs64(w)};",
        "mtlo": f"c->lo = {rs64(w)};",
        "mfhi1": set64(rd, "c->hi1"),
        "mflo1": set64(rd, "c->lo1"),
        "mthi1": f"c->hi1 = {rs64(w)};",
        "mtlo1": f"c->lo1 = {rs64(w)};",
        "mult": f"{{ int64_t p = (int64_t)(int32_t){rs32(w)} * (int32_t){rt32(w)}; "
                f"c->lo = (uint64_t)(int64_t)(int32_t)p; c->hi = (uint64_t)(int64_t)(int32_t)(p >> 32); "
                f"{set64(rd, 'c->lo')} }}",
        "multu": f"{{ uint64_t p = (uint64_t){rs32(w)} * {rt32(w)}; "
                 f"c->lo = (uint64_t)(int64_t)(int32_t)p; c->hi = (uint64_t)(int64_t)(int32_t)(p >> 32); "
                 f"{set64(rd, 'c->lo')} }}",
        "multu1": f"{{ uint64_t p = (uint64_t){rs32(w)} * {rt32(w)}; "
                  f"c->lo1 = (uint64_t)(int64_t)(int32_t)p; c->hi1 = (uint64_t)(int64_t)(int32_t)(p >> 32); "
                  f"{set64(rd, 'c->lo1')} }}",
        # Trapping add/sub: the overflow exception is not modelled (never raised).
        "add": set32(rd, f"{rs32(w)} + {rt32(w)}"),
        "addi": set32(rt, f"{rs32(w)} + (uint32_t){simm}"),
        "sub": set32(rd, f"{rs32(w)} - {rt32(w)}"),
        "dadd": set64(rd, f"{rs64(w)} + {rt64(w)}"),
        "daddi": set64(rt, f"{rs64(w)} + (uint64_t)(int64_t){simm}"),
        "dsub": set64(rd, f"{rs64(w)} - {rt64(w)}"),
        # Division by zero and INT_MIN / -1 give fixed results instead of C UB.
        "div": f"{{ int32_t n = (int32_t){rs32(w)}, d = (int32_t){rt32(w)}; int32_t q, r; "
               f"if (d == 0) {{ q = n < 0 ? 1 : -1; r = n; }} else if (n == INT32_MIN && d == -1) {{ q = n; r = 0; }} "
               f"else {{ q = n / d; r = n % d; }} c->lo = (uint64_t)(int64_t)q; c->hi = (uint64_t)(int64_t)r; }}",
        "divu": f"{{ uint32_t n = {rs32(w)}, d = {rt32(w)}; uint32_t q = d ? n / d : 0xffffffffu, r = d ? n % d : n; "
                f"c->lo = (uint64_t)(int64_t)(int32_t)q; c->hi = (uint64_t)(int64_t)(int32_t)r; }}",
        "mtsah": f"ee_mmi_mtsah(c, {rs}, {imm:#x}u);",
        # loads / stores
        "lb": set64(rt, f"(int64_t)(int8_t)ee_r8(c, {addr(w)})"),
        "lbu": set64(rt, f"ee_r8(c, {addr(w)})"),
        "lh": set64(rt, f"(int64_t)(int16_t)ee_r16(c, {addr(w)})"),
        "lhu": set64(rt, f"ee_r16(c, {addr(w)})"),
        "lw": set64(rt, f"(int64_t)(int32_t)ee_r32(c, {addr(w)})"),
        "lwu": set64(rt, f"ee_r32(c, {addr(w)})"),
        "ld": set64(rt, f"ee_r64(c, {addr(w)})"),
        "lq": f"{{ ee_u128 v = ee_r128(c, {addr(w)}); if ({rt}) c->r[{rt}] = v; }}",
        "ldl": set64(rt, f"ee_ldl(c, {addr(w)}, {rt64(w)})"),
        "ldr": set64(rt, f"ee_ldr(c, {addr(w)}, {rt64(w)})"),
        "lwl": set64(rt, f"ee_lwl(c, {addr(w)}, {rt64(w)})"),
        "lwr": set64(rt, f"ee_lwr(c, {addr(w)}, {rt64(w)})"),
        "sdl": f"ee_sdl(c, {addr(w)}, {rt64(w)});",
        "sdr": f"ee_sdr(c, {addr(w)}, {rt64(w)});",
        "swl": f"ee_swl(c, {addr(w)}, {rt32(w)});",
        "swr": f"ee_swr(c, {addr(w)}, {rt32(w)});",
        "sb": f"ee_w8(c, {addr(w)}, (uint8_t){rt64(w)});",
        "sh": f"ee_w16(c, {addr(w)}, (uint16_t){rt64(w)});",
        "sw": f"ee_w32(c, {addr(w)}, {rt32(w)});",
        "sd": f"ee_w64(c, {addr(w)}, {rt64(w)});",
        "sq": f"ee_w128(c, {addr(w)}, {gpr(rt)});",
        # COP0
        "mfc0": set32(rt, f"c->cop0[{rd}]"),
        "mtc0": f"c->cop0[{rd}] = {rt32(w)};",
        "di": "ee_di(c);",
        "eret": f"ee_eret(c, {pc:#010x}u); return;",
        "ei": "ee_ei(c);",
        "sync": "", "sync.l": "", "sync.p": "",
        # FPU (no clamping yet)
        "mtc1": f"c->fpr[{rd}].u = {rt32(w)};",
        "mfc1": set32(rt, f"c->fpr[{rd}].u"),
        "ctc1": f"if ({rd} == 31) c->fcr31 = {rt32(w)};",
        "cfc1": set32(rt, "c->fcr31") if rd == 31 else set32(rt, "0x2e00"),
        "lwc1": f"c->fpr[{rt}].u = ee_r32(c, {addr(w)});",
        "swc1": f"ee_w32(c, {addr(w)}, c->fpr[{rt}].u);",
        # FPU arithmetic with PS2 semantics (src/ee/fpu.c). fs = rd, ft = rt, fd = sa fields.
        "add.s": f"c->fpr[{sa}].u = ee_fpu_add(c, c->fpr[{rd}].u, c->fpr[{rt}].u);",
        "sub.s": f"c->fpr[{sa}].u = ee_fpu_sub(c, c->fpr[{rd}].u, c->fpr[{rt}].u);",
        "mul.s": f"c->fpr[{sa}].u = ee_fpu_mul(c, c->fpr[{rd}].u, c->fpr[{rt}].u);",
        "div.s": f"c->fpr[{sa}].u = ee_fpu_div(c, c->fpr[{rd}].u, c->fpr[{rt}].u);",
        "sqrt.s": f"c->fpr[{sa}].u = ee_fpu_sqrt(c, c->fpr[{rt}].u);",
        "rsqrt.s": f"c->fpr[{sa}].u = ee_fpu_rsqrt(c, c->fpr[{rd}].u, c->fpr[{rt}].u);",
        "max.s": f"c->fpr[{sa}].u = ee_fpu_max(c, c->fpr[{rd}].u, c->fpr[{rt}].u);",
        "min.s": f"c->fpr[{sa}].u = ee_fpu_min(c, c->fpr[{rd}].u, c->fpr[{rt}].u);",
        "mov.s": f"c->fpr[{sa}].u = c->fpr[{rd}].u;",
        "abs.s": f"c->fpr[{sa}].u = ee_fpu_abs(c, c->fpr[{rd}].u);",
        "neg.s": f"c->fpr[{sa}].u = ee_fpu_neg(c, c->fpr[{rd}].u);",
        "cvt.s.w": f"c->fpr[{sa}].u = ee_fpu_cvt_s_w(c, c->fpr[{rd}].u);",
        "cvt.w.s": f"c->fpr[{sa}].u = ee_fpu_cvt_w_s(c, c->fpr[{rd}].u);",
        "adda.s": f"ee_fpu_adda(c, c->fpr[{rd}].u, c->fpr[{rt}].u);",
        "suba.s": f"ee_fpu_suba(c, c->fpr[{rd}].u, c->fpr[{rt}].u);",
        "mula.s": f"ee_fpu_mula(c, c->fpr[{rd}].u, c->fpr[{rt}].u);",
        "madda.s": f"ee_fpu_madda(c, c->fpr[{rd}].u, c->fpr[{rt}].u);",
        "msuba.s": f"ee_fpu_msuba(c, c->fpr[{rd}].u, c->fpr[{rt}].u);",
        "madd.s": f"c->fpr[{sa}].u = ee_fpu_madd(c, c->fpr[{rd}].u, c->fpr[{rt}].u);",
        "msub.s": f"c->fpr[{sa}].u = ee_fpu_msub(c, c->fpr[{rd}].u, c->fpr[{rt}].u);",
        "c.f.s": "ee_fpu_c_f(c);",
        "c.eq.s": f"ee_fpu_c_eq(c, c->fpr[{rd}].u, c->fpr[{rt}].u);",
        "c.lt.s": f"ee_fpu_c_lt(c, c->fpr[{rd}].u, c->fpr[{rt}].u);",
        "c.le.s": f"ee_fpu_c_le(c, c->fpr[{rd}].u, c->fpr[{rt}].u);",
        # MMI (128-bit)
        "padduw": "" if rd == 0 else
                  f"{{ ee_u128 a = {gpr(rs)}, b = {gpr(rt)}; for (int i = 0; i < 4; i++) "
                  f"{{ uint64_t s = (uint64_t)a.uw[i] + b.uw[i]; c->r[{rd}].uw[i] = s > 0xffffffffu ? 0xffffffffu : (uint32_t)s; }} }}",
        "pcpyld": "" if rd == 0 else
                  f"{{ uint64_t lo = {gpr(rt)}.ud[0], hi = {gpr(rs)}.ud[0]; c->r[{rd}].ud[0] = lo; c->r[{rd}].ud[1] = hi; }}",
        "pcpyud": "" if rd == 0 else
                  f"{{ uint64_t lo = {gpr(rs)}.ud[1], hi = {gpr(rt)}.ud[1]; c->r[{rd}].ud[0] = lo; c->r[{rd}].ud[1] = hi; }}",
        "pcpyh": "" if rd == 0 else
                 f"{{ ee_u128 b = {gpr(rt)}; for (int i = 0; i < 4; i++) {{ c->r[{rd}].uh[i] = b.uh[0]; c->r[{rd}].uh[4 + i] = b.uh[4]; }} }}",
        "por": "" if rd == 0 else
               f"{{ c->r[{rd}].ud[0] = {gpr(rs)}.ud[0] | {gpr(rt)}.ud[0]; c->r[{rd}].ud[1] = {gpr(rs)}.ud[1] | {gpr(rt)}.ud[1]; }}",
        "pand": "" if rd == 0 else
                f"{{ c->r[{rd}].ud[0] = {gpr(rs)}.ud[0] & {gpr(rt)}.ud[0]; c->r[{rd}].ud[1] = {gpr(rs)}.ud[1] & {gpr(rt)}.ud[1]; }}",
        "pxor": "" if rd == 0 else
                f"{{ c->r[{rd}].ud[0] = {gpr(rs)}.ud[0] ^ {gpr(rt)}.ud[0]; c->r[{rd}].ud[1] = {gpr(rs)}.ud[1] ^ {gpr(rt)}.ud[1]; }}",
        # MMI (src/ee/mmi.c)
        "paddb": f"ee_mmi_paddb(c, {rd}, {rs}, {rt});",
        "paddh": f"ee_mmi_paddh(c, {rd}, {rs}, {rt});",
        "paddw": f"ee_mmi_paddw(c, {rd}, {rs}, {rt});",
        "psubb": f"ee_mmi_psubb(c, {rd}, {rs}, {rt});",
        "psubh": f"ee_mmi_psubh(c, {rd}, {rs}, {rt});",
        "psubw": f"ee_mmi_psubw(c, {rd}, {rs}, {rt});",
        "paddsb": f"ee_mmi_paddsb(c, {rd}, {rs}, {rt});",
        "paddsh": f"ee_mmi_paddsh(c, {rd}, {rs}, {rt});",
        "paddsw": f"ee_mmi_paddsw(c, {rd}, {rs}, {rt});",
        "psubsb": f"ee_mmi_psubsb(c, {rd}, {rs}, {rt});",
        "psubsh": f"ee_mmi_psubsh(c, {rd}, {rs}, {rt});",
        "psubsw": f"ee_mmi_psubsw(c, {rd}, {rs}, {rt});",
        "paddub": f"ee_mmi_paddub(c, {rd}, {rs}, {rt});",
        "padduh": f"ee_mmi_padduh(c, {rd}, {rs}, {rt});",
        "psubub": f"ee_mmi_psubub(c, {rd}, {rs}, {rt});",
        "psubuh": f"ee_mmi_psubuh(c, {rd}, {rs}, {rt});",
        "pmaxh": f"ee_mmi_pmaxh(c, {rd}, {rs}, {rt});",
        "pminh": f"ee_mmi_pminh(c, {rd}, {rs}, {rt});",
        "pmaxw": f"ee_mmi_pmaxw(c, {rd}, {rs}, {rt});",
        "pminw": f"ee_mmi_pminw(c, {rd}, {rs}, {rt});",
        "pceqb": f"ee_mmi_pceqb(c, {rd}, {rs}, {rt});",
        "pceqh": f"ee_mmi_pceqh(c, {rd}, {rs}, {rt});",
        "pceqw": f"ee_mmi_pceqw(c, {rd}, {rs}, {rt});",
        "pcgtb": f"ee_mmi_pcgtb(c, {rd}, {rs}, {rt});",
        "pcgth": f"ee_mmi_pcgth(c, {rd}, {rs}, {rt});",
        "pcgtw": f"ee_mmi_pcgtw(c, {rd}, {rs}, {rt});",
        "pnor": f"ee_mmi_pnor(c, {rd}, {rs}, {rt});",
        "pextlw": f"ee_mmi_pextlw(c, {rd}, {rs}, {rt});",
        "pextuw": f"ee_mmi_pextuw(c, {rd}, {rs}, {rt});",
        "pextlh": f"ee_mmi_pextlh(c, {rd}, {rs}, {rt});",
        "pextuh": f"ee_mmi_pextuh(c, {rd}, {rs}, {rt});",
        "pextlb": f"ee_mmi_pextlb(c, {rd}, {rs}, {rt});",
        "pextub": f"ee_mmi_pextub(c, {rd}, {rs}, {rt});",
        "ppacb": f"ee_mmi_ppacb(c, {rd}, {rs}, {rt});",
        "ppach": f"ee_mmi_ppach(c, {rd}, {rs}, {rt});",
        "ppacw": f"ee_mmi_ppacw(c, {rd}, {rs}, {rt});",
        "pinteh": f"ee_mmi_pinteh(c, {rd}, {rs}, {rt});",
        "pmulth": f"ee_mmi_pmulth(c, {rd}, {rs}, {rt});",
        "pmaddh": f"ee_mmi_pmaddh(c, {rd}, {rs}, {rt});",
        "pmultw": f"ee_mmi_pmultw(c, {rd}, {rs}, {rt});",
        "pmultuw": f"ee_mmi_pmultuw(c, {rd}, {rs}, {rt});",
        "mult1": f"ee_mmi_mult1(c, {rd}, {rs}, {rt});",
        "madd": f"ee_mmi_madd(c, {rd}, {rs}, {rt});",
        "maddu": f"ee_mmi_maddu(c, {rd}, {rs}, {rt});",
        "madd1": f"ee_mmi_madd1(c, {rd}, {rs}, {rt});",
        "maddu1": f"ee_mmi_maddu1(c, {rd}, {rs}, {rt});",
        "qfsrv": f"ee_mmi_qfsrv(c, {rd}, {rs}, {rt});",
        "pabsw": f"ee_mmi_pabsw(c, {rd}, {rt});",
        "pabsh": f"ee_mmi_pabsh(c, {rd}, {rt});",
        "ppac5": f"ee_mmi_ppac5(c, {rd}, {rt});",
        "pext5": f"ee_mmi_pext5(c, {rd}, {rt});",
        "prevh": f"ee_mmi_prevh(c, {rd}, {rt});",
        "prot3w": f"ee_mmi_prot3w(c, {rd}, {rt});",
        "pexew": f"ee_mmi_pexew(c, {rd}, {rt});",
        "pexcw": f"ee_mmi_pexcw(c, {rd}, {rt});",
        "psllh": f"ee_mmi_psllh(c, {rd}, {rt}, {sa});",
        "psrlh": f"ee_mmi_psrlh(c, {rd}, {rt}, {sa});",
        "psrah": f"ee_mmi_psrah(c, {rd}, {rt}, {sa});",
        "psllw": f"ee_mmi_psllw(c, {rd}, {rt}, {sa});",
        "psrlw": f"ee_mmi_psrlw(c, {rd}, {rt}, {sa});",
        "psraw": f"ee_mmi_psraw(c, {rd}, {rt}, {sa});",
        "pmfhi": f"ee_mmi_pmfhi(c, {rd});",
        "pmflo": f"ee_mmi_pmflo(c, {rd});",
        "pmthi": f"ee_mmi_pmthi(c, {rs});",
        "pmtlo": f"ee_mmi_pmtlo(c, {rs});",
        "pmfhl.lw": f"ee_mmi_pmfhl_lw(c, {rd});",
        "pmfhl.uw": f"ee_mmi_pmfhl_uw(c, {rd});",
        "pmfhl.lh": f"ee_mmi_pmfhl_lh(c, {rd});",
        "div1": f"ee_mmi_div1(c, {rs}, {rt});",
        "divu1": f"ee_mmi_divu1(c, {rs}, {rt});",
        "mfsa": f"ee_mmi_mfsa(c, {rd});",
        "mtsa": f"ee_mmi_mtsa(c, {rs});",
        "mtsab": f"ee_mmi_mtsab(c, {rs}, {imm:#x}u);",
        "pref": "",
        "break": f"ee_unsupported(c, {pc:#010x}u, \"break (compiler trap, e.g. division by zero check)\");",
        "syscall": f"ee_syscall(c, {pc:#010x}u);",
    }
    return t.get(name)


BRANCH_COND = {
    "bc1f": lambda w: "!(c->fcr31 & 0x00800000u)",
    "bc1t": lambda w: "(c->fcr31 & 0x00800000u)",
    "beq": lambda w: f"{rs64(w)} == {rt64(w)}",
    "bne": lambda w: f"{rs64(w)} != {rt64(w)}",
    "blez": lambda w: f"(int64_t){rs64(w)} <= 0",
    "bgtz": lambda w: f"(int64_t){rs64(w)} > 0",
    "bltz": lambda w: f"(int64_t){rs64(w)} < 0",
    "bgez": lambda w: f"(int64_t){rs64(w)} >= 0",
}
LIKELY = {"beql": "beq", "bnel": "bne", "blezl": "blez", "bgtzl": "bgtz", "bltzl": "bltz", "bgezl": "bgez",
          "bc1fl": "bc1f", "bc1tl": "bc1t"}


def instr(code, pc):
    w = code.word(pc)
    name = R.Instruction(w, vram=pc, category=R.InstrCategory.R5900).getOpcodeName()
    # rabbitizer does not name the R5900 SQRT.S encoding (COP1, fmt S, funct 4).
    if name == "c1" and w >> 26 == 0x11 and (w >> 21) & 31 == 16 and w & 0x3F == 4:
        name = "sqrt.s"
    return w, name


def emit_simple(pc, w, name):
    c = translate(pc, w, name)
    if c is None:
        return f"ee_unsupported(c, {pc:#010x}u, \"instruction {name}\");"
    return c


def function_c(code, start, end, funcs, exclude, stop, hooks=False):
    """Returns (C source, callees set)."""
    lines, callees = [], set()
    targets = set()
    for pc in range(start, end, 4):
        w, name = instr(code, pc)
        base = LIKELY.get(name, name)
        if base in BRANCH_COND:
            targets.add(u32(pc + 4 + (simm16(w) << 2)))
        elif name == "j":
            t = ((pc + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
            if start <= t < end:
                targets.add(t)

    def call(t, pc):
        if t in exclude or t not in funcs:
            if hooks:
                return (f"{{ ee_fn boundary=c->hook?c->hook({t:#010x}u):NULL; "
                        f"if(boundary)boundary(c);else ee_unrecompiled(c, {t:#010x}u, {pc:#010x}u); }}")
            return f"ee_unrecompiled(c, {t:#010x}u, {pc:#010x}u);"
        callees.add(t)
        return f"f_{t:08X}(c);"

    # Computed local jumps (switch tables) may target any aligned instruction.
    # Keep dispatch within this function; foreign targets stop explicitly.
    indirect_jump = any(instr(code, p)[1] == "jr" and
                        ((code.word(p) >> 21) & 31) != 31
                        for p in range(start, end, 4))
    if indirect_jump:
        targets.update(range(start, end, 4))
    pc = start
    while pc < end:
        w, name = instr(code, pc)
        label = f"L_{pc:08X}: ee_sif_interrupt_point(c);" if pc in targets else ""
        stop_c = f" ee_end(c, EE_END_STOP_PC, {pc:#010x}u, \"stop\");" if pc == stop else ""
        base = LIKELY.get(name, name)
        is_ctl = base in BRANCH_COND or name in ("j", "jal", "jr", "jalr")
        if not is_ctl:
            lines.append(f"    {label}{stop_c} /* {pc:08X} {name} */ {emit_simple(pc, w, name)}")
            pc += 4
            continue

        dw, dname = instr(code, pc + 4)
        dbase = LIKELY.get(dname, dname)
        if dbase in BRANCH_COND or dname in ("j", "jal", "jr", "jalr"):
            lines.append(f"    {label} ee_unsupported(c, {pc:#010x}u, \"branch in delay slot\");")
            pc += 8
            continue
        delay = emit_simple(pc + 4, dw, dname)
        head = f"    {label}{stop_c} /* {pc:08X} {name} ; {dname} */ "

        if base in BRANCH_COND:
            t = u32(pc + 4 + (simm16(w) << 2))
            cond = BRANCH_COND[base](w)
            if t not in targets or not (start <= t < end):
                lines.append(head + f"ee_unsupported(c, {pc:#010x}u, \"branch out of function\");")
            elif name in LIKELY:
                lines.append(head + f"if ({cond}) {{ {delay} goto L_{t:08X}; }}")
            else:
                lines.append(head + f"{{ int take = ({cond}); {delay} if (take) goto L_{t:08X}; }}")
        elif name == "j":
            t = ((pc + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
            if start <= t < end:
                lines.append(head + f"{delay} goto L_{t:08X};")
            else:
                lines.append(head + f"{delay} {call(t, pc)} return;")
        elif name == "jal":
            t = ((pc + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
            lines.append(head + f"c->r[31].sd[0] = {pc + 8:#x}; {delay} {call(t, pc)}")
        elif name == "jr":
            if (w >> 21) & 31 == 31:
                lines.append(head + f"{delay} return;")
            else:
                cases = " ".join(f"case {t:#x}u: goto L_{t:08X};" for t in sorted(targets)
                                 if start <= t < end)
                lines.append(head + f"{{ uint32_t target = {rs32(w)}; {delay} switch (target) {{ "
                             + cases + f' default: ee_unrecompiled(c, target, {pc:#010x}u); }} }}')
        elif name == "jalr":
            link = set64((w >> 11) & 31, f"(int64_t)(int32_t){pc + 8:#x}u")
            lines.append(head + f"{{ uint32_t t = {rs32(w)}; {link} "
                                f"{delay} ee_call(c, t, {pc:#010x}u); }}")
        # A delay slot that is itself a branch target needs its own copy.
        if pc + 4 in targets:
            lines.append(f"    goto L_{pc + 8:08X}; L_{pc + 4:08X}: {delay}")
            targets.add(pc + 8)
        pc += 8

    # Labels that ended up referenced but past the last line.
    body = "\n".join(lines)
    missing = [t for t in sorted(targets) if f"L_{t:08X}:" not in body]
    tail = "".join(f"\n    L_{t:08X}: ee_unsupported(c, {t:#010x}u, \"fell off function\");" for t in missing)
    src = (f"/* {code.name(start)} {start:08X}-{end:08X} */\n"
           f"void f_{start:08X}(ee_ctx *c)\n{{\n"
           + (f"    if (c->hook) {{ ee_fn h = c->hook({start:#010x}u); if (h) {{ h(c); return; }} }}\n"
              if hooks else "")
           + f"{body}{tail}\n"
           f"    ee_unsupported(c, {end:#010x}u, \"fell off end of function\");\n}}\n")
    return src, callees


def load_starts(text_s: Path):
    lines = text_s.read_text(encoding="utf-8").splitlines()
    starts = []
    for i, line in enumerate(lines):
        if line.startswith("glabel "):
            m = re.search(r"/\* [0-9A-F]+ ([0-9A-F]{8}) ", lines[i + 1])
            if m:
                starts.append(int(m.group(1), 16))
    return sorted(starts)


def load_names(path: Path):
    names = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        m = re.match(r"(\w+) = (0x[0-9a-fA-F]+);", line)
        if m:
            names[int(m.group(2), 16)] = m.group(1)
    return names


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--core", default=str(ROOT / "extracted" / "core.bin"))
    ap.add_argument("--text-s", default=str(ROOT / "splat" / "asm" / "core" / "text.s"))
    ap.add_argument("--symbols", default=str(ROOT / "splat" / "symbol_addrs.txt"))
    ap.add_argument("--root", required=True, help="START or START:END")
    ap.add_argument("--stop", type=lambda s: int(s, 0), default=None)
    ap.add_argument("--exclude", default="")
    ap.add_argument("--data-refs", action="store_true",
                    help="also include functions whose address is loaded as a constant")
    ap.add_argument("--blob", action="append", default=[],
                    help="SRC:DST:SIZE code copied by the game from SRC to DST; all its "
                         "guessed functions become roots")
    ap.add_argument("--parts", type=int, default=1,
                    help="also write the function bodies to OUT_p0.c .. OUT_p{N-1}.c so a large "
                         "closure compiles in parallel; OUT keeps entry, lookup and code ranges")
    ap.add_argument("--table-roots", action="append", default=[],
                    help="START:END data range; every aligned word in it that is a function "
                         "start becomes a root (constructor lists, vtables, callback tables)")
    ap.add_argument("--extra-roots", default="",
                    help="functions reached only as data (e.g. SetSyscall handlers)")
    ap.add_argument("--hooks", action="store_true",
                    help="each function first asks c->hook for a replacement (native rewrites "
                         "compared with the original, tests/physics)")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    code = Code(Path(a.core).read_bytes(), load_starts(Path(a.text_s)), load_names(Path(a.symbols)))
    blob_roots = []
    for spec in a.blob:
        src, dst, size = (int(x, 0) for x in spec.split(":"))
        blob_roots += code.add_blob(src, dst, size)
    table_roots = []
    for spec in a.table_roots:
        lo, hi = (int(x, 0) for x in spec.split(":"))
        for addr in range(lo & ~3, hi, 4):
            v = struct.unpack_from("<I", code.core, addr - BASE)[0]
            if v in code.start_set:
                table_roots.append(v)
    blob_roots += sorted(set(table_roots))
    root_start, _, root_end = a.root.partition(":")
    root_start = int(root_start, 0)
    ends = {root_start: int(root_end, 0) if root_end else code.end_of(root_start)}
    exclude = {int(x, 0) for x in a.exclude.split(",") if x}

    # Closure over direct calls and tail calls. First pass finds the set.
    data_taken = set()
    funcs, todo = set(), [root_start] + blob_roots + [int(x, 0) for x in a.extra_roots.split(",") if x]
    while todo:
        f = todo.pop()
        if f in funcs or f in exclude:
            continue
        funcs.add(f)
        end = ends.get(f) or code.end_of(f)
        ends[f] = end
        for pc in range(f, end, 4):
            w, name = instr(code, pc)
            if name in ("jal", "j"):
                t = ((pc + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
                if name == "jal" or not (f <= t < end):
                    todo.append(t)
            elif a.data_refs and name == "lui":
                # lui + addiu/ori of a function start: address taken as data
                # (SetSyscall handler, thread entry, interrupt handler).
                reg, hi = (w >> 16) & 31, (w & 0xFFFF) << 16
                for k in range(pc + 4, end, 4):
                    w2, n2 = instr(code, k)
                    if n2 in ("addiu", "ori") and (w2 >> 21) & 31 == reg:
                        t = u32(hi + (simm16(w2) if n2 == "addiu" else w2 & 0xFFFF))
                        if t in code.start_set:
                            data_taken.add(t)
                            todo.append(t)
                        break

    prelude = [
        "/* GENERATED by tools/recomp/ee_recomp.py from the user's own disc. Do not commit. */",
        '#include "ee.h"', '#include "fpu.h"', '#include "mmi.h"', "",
    ] + [f"void f_{f:08X}(ee_ctx *c);" for f in sorted(funcs)] + [""]
    out = list(prelude)
    parts = [list(prelude) for _ in range(max(1, a.parts))] if a.parts > 1 else None
    unsupported = 0
    for i, f in enumerate(sorted(funcs)):
        src, _ = function_c(code, f, ends[f], funcs, exclude, a.stop, a.hooks)
        unsupported += src.count("ee_unsupported(c,") - 1
        (parts[i % a.parts] if parts else out).append(src)
    if parts:
        stem = Path(a.out)
        for k, body in enumerate(parts):
            dest = stem.with_name(f"{stem.stem}_p{k}{stem.suffix}")
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_text("\n".join(body) + "\n", encoding="utf-8", newline="\n")
    out.append(f"void recomp_entry(ee_ctx *c) {{ f_{root_start:08X}(c); }}")
    out.append("/* Copied code: guest address, size, FNV-1a 64 of the source bytes. */")
    ranges = []
    for src, dst, size in code.blobs:
        h = 0xCBF29CE484222325
        for byte in code.core[src - BASE:src - BASE + size]:
            h = ((h ^ byte) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
        ranges.append(f"    {{ {dst:#010x}u, {size:#x}u, {h:#018x}ull }},")
    out.append("const ee_code_range recomp_code_ranges[] = {")
    out += ranges or ["    { 0, 0, 0 },"]
    out.append("};")
    out.append(f"const int recomp_n_code_ranges = {len(ranges)};")
    out.append("ee_fn recomp_lookup(uint32_t a)\n{\n    switch (a) {")
    out += [f"    case {f:#010x}u: return f_{f:08X};" for f in sorted(funcs)]
    out.append("    default: return 0;\n    }\n}")
    Path(a.out).parent.mkdir(parents=True, exist_ok=True)
    Path(a.out).write_text("\n".join(out) + "\n", encoding="utf-8", newline="\n")
    print(f"{len(funcs)} functions ({len(data_taken)} reached as data), "
          f"{unsupported} unsupported sites -> {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
