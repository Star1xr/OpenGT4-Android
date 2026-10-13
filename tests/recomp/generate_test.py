"""Generate tests from synthetic instructions only; no game input."""
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/recomp"))
from ee_recomp import Code, function_c

base = 0x100000
programs = {
    base: [(0x1C << 26) | (4 << 21) | (5 << 11) | 4, 0x03E00008, 0],  # plzcw a1,a0
    base + 0x20: [(0x1C << 26) | (4 << 21) | (4 << 11) | 4, 0x03E00008, 0],
    base + 0x40: [0x0080F809, 0x24040000, 0x03E00008, 0],  # jalr a0; clear a0 in delay
    base + 0x80: [0x00800008, 0x2402000B, 0x24020016, 0x03E00008, 0x24030021],
    # c.lt.s f1,f2; bc1tl +2 (delay mov.s f3,f1 only if taken); add.s f3,f1,f2; jr ra
    base + 0xA0: [0x46020834, 0x45030002, 0x460008C6, 0x460208C0, 0x03E00008, 0],
    # sqrt.s f5,f2 in the encoding rabbitizer leaves unnamed (COP1 fmt S funct 4)
    base + 0xC0: [0x46020144, 0x03E00008, 0],
}
image = bytearray(0x100)
for addr, words in programs.items():
    struct.pack_into(f"<{len(words)}I", image, addr - base, *words)
code = Code(bytes(image), programs, {})
out = ['#include "ee.h"', '#include "fpu.h"', '#include "mmi.h"', '#include <stdio.h>',
       '#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\\n", __LINE__, #x); return 1; } } while (0)']
for addr, words in programs.items():
    out.append(function_c(code, addr, addr + 4*len(words), set(programs), set(), None)[0])
out.append(r'''
static void callback(ee_ctx *c) { c->r[2].ud[0] = 77; }
static ee_fn lookup(uint32_t a) { return a == 0x200000 ? callback : NULL; }
int main(void) {
    static ee_ctx c;
    jmp_buf j;
    const uint32_t values[] = {0, 0xffffffff, 0x80000000, 0x7fffffff, 1, 0xfffffffe, 0x100};
    const uint32_t expected[] = {31, 31, 0, 0, 30, 30, 22};
    CHECK(ee_init(&c) == 0);
    c.exit_jmp = &j; c.lookup = lookup;
    if (setjmp(j)) { puts(c.end_detail); return 1; }
    for (unsigned i=0; i<sizeof values/sizeof values[0]; i++) {
        c.r[4].uw[0] = values[i]; c.r[4].uw[1] = ~values[i];
        c.r[5].ud[1] = 0xabcdef0123456789ull;
        f_00100000(&c);
        CHECK(c.r[5].uw[0] == expected[i] && c.r[5].uw[1] == expected[i]);
        CHECK(c.r[5].ud[1] == 0xabcdef0123456789ull);
        c.r[4].ud[1] = 0x123456789abcdef0ull;
        f_00100020(&c);
        CHECK(c.r[4].uw[0] == expected[i] && c.r[4].uw[1] == expected[i]);
        CHECK(c.r[4].ud[1] == 0x123456789abcdef0ull);
    }
    c.r[4].ud[0] = 0x200000;
    f_00100040(&c);
    CHECK(c.r[4].ud[0] == 0 && c.r[2].ud[0] == 77 && c.r[31].uw[0] == 0x100048);
    c.r[4].ud[0] = 0x10008c;
    f_00100080(&c);
    CHECK(c.r[2].ud[0] == 11 && c.r[3].ud[0] == 33);
    c.r[4].ud[0] = 0x100088;
    f_00100080(&c);
    CHECK(c.r[2].ud[0] == 22 && c.r[3].ud[0] == 33);
    c.r[4].ud[0] = 0x1234;
    if (setjmp(j) == 0) { f_00100080(&c); CHECK(0); }
    CHECK(c.end_reason == EE_END_UNRECOMPILED);
    /* FPU through the recompiler. 1 < 2: likely branch taken, delay slot runs. */
    c.fpr[1].u = 0x3f800000u; c.fpr[2].u = 0x40000000u; c.fpr[3].u = 0;
    f_001000A0(&c);
    CHECK(c.fpr[3].u == 0x3f800000u);
    /* 3 < 2 false: delay slot annulled, add.s runs: 3 + 2 = 5. */
    c.fpr[1].u = 0x40400000u; c.fpr[3].u = 0;
    f_001000A0(&c);
    CHECK(c.fpr[3].u == 0x40a00000u);
    /* sqrt.s of -4: 2, with the invalid flag. */
    c.fpr[2].u = 0xc0800000u; c.fcr31 = 0;
    f_001000C0(&c);
    CHECK(c.fpr[5].u == 0x40000000u && (c.fcr31 & 0x00020000u));
    ee_free(&c);
    puts("PASSED synthetic PLZCW lanes/aliasing, JALR delay slot, computed local jump/invalid target, FPU compare/likely branch/sqrt");
    return 0;
}
''')
dest = Path(sys.argv[1])
dest.parent.mkdir(parents=True, exist_ok=True)
dest.write_text("\n".join(out), encoding="utf-8", newline="\n")
