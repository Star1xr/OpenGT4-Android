/*
 * EE MMI (128-bit multimedia) instructions for recompiled code.
 *
 * Semantics follow the EE Core Instruction Set manual. Each helper reads its
 * operands before writing, so rd may equal rs or rt. Writes to r0 are dropped.
 * HI/LO are 128 bits here: lower 64 bits in c->hi/c->lo, upper in c->hi1/c->lo1.
 * SA holds a byte count (MTSAB/MTSAH store bytes; QFSRV shifts by SA*8 bits).
 */
#include "mmi.h"

#include <string.h>

#define R(n) (c->r[n])

static void put(ee_ctx *c, int rd, const ee_u128 *v)
{
    if (rd)
        c->r[rd] = *v;
}

static ee_u128 get_hi(const ee_ctx *c) { ee_u128 v; v.ud[0] = c->hi; v.ud[1] = c->hi1; return v; }
static ee_u128 get_lo(const ee_ctx *c) { ee_u128 v; v.ud[0] = c->lo; v.ud[1] = c->lo1; return v; }
static void set_hi(ee_ctx *c, const ee_u128 *v) { c->hi = v->ud[0]; c->hi1 = v->ud[1]; }
static void set_lo(ee_ctx *c, const ee_u128 *v) { c->lo = v->ud[0]; c->lo1 = v->ud[1]; }

static int32_t sat32(int64_t v) { return v > INT32_MAX ? INT32_MAX : v < INT32_MIN ? INT32_MIN : (int32_t)v; }
static int16_t sat16(int32_t v) { return v > INT16_MAX ? INT16_MAX : v < INT16_MIN ? INT16_MIN : (int16_t)v; }
static int8_t  sat8(int32_t v)  { return v > INT8_MAX ? INT8_MAX : v < INT8_MIN ? INT8_MIN : (int8_t)v; }

/* ---- lane-wise arithmetic ---- */

#define LANES(name, T, n, field, expr)                                    \
    void ee_mmi_##name(ee_ctx *c, int rd, int rs, int rt)                 \
    {                                                                     \
        ee_u128 a = R(rs), b = R(rt), d;                                  \
        for (int i = 0; i < n; i++) {                                     \
            T x = (T)a.field[i], y = (T)b.field[i];                       \
            d.field[i] = expr;                                            \
        }                                                                 \
        put(c, rd, &d);                                                   \
    }

LANES(paddb,  uint8_t,  16, ub, (uint8_t)(x + y))
LANES(paddh,  uint16_t, 8,  uh, (uint16_t)(x + y))
LANES(paddw,  uint32_t, 4,  uw, x + y)
LANES(psubb,  uint8_t,  16, ub, (uint8_t)(x - y))
LANES(psubh,  uint16_t, 8,  uh, (uint16_t)(x - y))
LANES(psubw,  uint32_t, 4,  uw, x - y)
LANES(paddsb, int8_t,   16, ub, (uint8_t)sat8(x + y))
LANES(paddsh, int16_t,  8,  uh, (uint16_t)sat16(x + y))
LANES(paddsw, int32_t,  4,  uw, (uint32_t)sat32((int64_t)x + y))
LANES(psubsb, int8_t,   16, ub, (uint8_t)sat8(x - y))
LANES(psubsh, int16_t,  8,  uh, (uint16_t)sat16(x - y))
LANES(psubsw, int32_t,  4,  uw, (uint32_t)sat32((int64_t)x - y))
LANES(paddub, uint8_t,  16, ub, (uint8_t)(x + y > 0xff ? 0xff : x + y))
LANES(padduh, uint16_t, 8,  uh, (uint16_t)(x + y > 0xffff ? 0xffff : x + y))
LANES(psubub, uint8_t,  16, ub, (uint8_t)(x > y ? x - y : 0))
LANES(psubuh, uint16_t, 8,  uh, (uint16_t)(x > y ? x - y : 0))
LANES(pmaxh,  int16_t,  8,  uh, (uint16_t)(x > y ? x : y))
LANES(pminh,  int16_t,  8,  uh, (uint16_t)(x < y ? x : y))
LANES(pmaxw,  int32_t,  4,  uw, (uint32_t)(x > y ? x : y))
LANES(pminw,  int32_t,  4,  uw, (uint32_t)(x < y ? x : y))
LANES(pceqb,  uint8_t,  16, ub, (uint8_t)(x == y ? 0xff : 0))
LANES(pceqh,  uint16_t, 8,  uh, (uint16_t)(x == y ? 0xffff : 0))
LANES(pceqw,  uint32_t, 4,  uw, x == y ? 0xffffffffu : 0)
LANES(pcgtb,  int8_t,   16, ub, (uint8_t)(x > y ? 0xff : 0))
LANES(pcgth,  int16_t,  8,  uh, (uint16_t)(x > y ? 0xffff : 0))
LANES(pcgtw,  int32_t,  4,  uw, x > y ? 0xffffffffu : 0)

void ee_mmi_pnor(ee_ctx *c, int rd, int rs, int rt)
{
    ee_u128 d;
    d.ud[0] = ~(R(rs).ud[0] | R(rt).ud[0]);
    d.ud[1] = ~(R(rs).ud[1] | R(rt).ud[1]);
    put(c, rd, &d);
}

void ee_mmi_pabsw(ee_ctx *c, int rd, int rt)
{
    ee_u128 a = R(rt), d;
    for (int i = 0; i < 4; i++)
        d.uw[i] = a.sw[i] == INT32_MIN ? 0x7fffffffu : (uint32_t)(a.sw[i] < 0 ? -a.sw[i] : a.sw[i]);
    put(c, rd, &d);
}

void ee_mmi_pabsh(ee_ctx *c, int rd, int rt)
{
    ee_u128 a = R(rt), d;
    for (int i = 0; i < 8; i++) {
        int16_t v = (int16_t)a.uh[i];
        d.uh[i] = v == INT16_MIN ? 0x7fff : (uint16_t)(v < 0 ? -v : v);
    }
    put(c, rd, &d);
}

/* ---- shifts by immediate ---- */

void ee_mmi_psllh(ee_ctx *c, int rd, int rt, int sa) { ee_u128 a = R(rt), d; for (int i = 0; i < 8; i++) d.uh[i] = (uint16_t)(a.uh[i] << (sa & 15)); put(c, rd, &d); }
void ee_mmi_psrlh(ee_ctx *c, int rd, int rt, int sa) { ee_u128 a = R(rt), d; for (int i = 0; i < 8; i++) d.uh[i] = (uint16_t)(a.uh[i] >> (sa & 15)); put(c, rd, &d); }
void ee_mmi_psrah(ee_ctx *c, int rd, int rt, int sa) { ee_u128 a = R(rt), d; for (int i = 0; i < 8; i++) d.uh[i] = (uint16_t)((int16_t)a.uh[i] >> (sa & 15)); put(c, rd, &d); }
void ee_mmi_psllw(ee_ctx *c, int rd, int rt, int sa) { ee_u128 a = R(rt), d; for (int i = 0; i < 4; i++) d.uw[i] = a.uw[i] << (sa & 31); put(c, rd, &d); }
void ee_mmi_psrlw(ee_ctx *c, int rd, int rt, int sa) { ee_u128 a = R(rt), d; for (int i = 0; i < 4; i++) d.uw[i] = a.uw[i] >> (sa & 31); put(c, rd, &d); }
void ee_mmi_psraw(ee_ctx *c, int rd, int rt, int sa) { ee_u128 a = R(rt), d; for (int i = 0; i < 4; i++) d.uw[i] = (uint32_t)(a.sw[i] >> (sa & 31)); put(c, rd, &d); }

/* ---- interleave / pack ---- */

void ee_mmi_pextlw(ee_ctx *c, int rd, int rs, int rt)
{
    ee_u128 a = R(rs), b = R(rt), d;
    d.uw[0] = b.uw[0]; d.uw[1] = a.uw[0]; d.uw[2] = b.uw[1]; d.uw[3] = a.uw[1];
    put(c, rd, &d);
}

void ee_mmi_pextuw(ee_ctx *c, int rd, int rs, int rt)
{
    ee_u128 a = R(rs), b = R(rt), d;
    d.uw[0] = b.uw[2]; d.uw[1] = a.uw[2]; d.uw[2] = b.uw[3]; d.uw[3] = a.uw[3];
    put(c, rd, &d);
}

void ee_mmi_pextlh(ee_ctx *c, int rd, int rs, int rt)
{
    ee_u128 a = R(rs), b = R(rt), d;
    for (int i = 0; i < 4; i++) { d.uh[2 * i] = b.uh[i]; d.uh[2 * i + 1] = a.uh[i]; }
    put(c, rd, &d);
}

void ee_mmi_pextuh(ee_ctx *c, int rd, int rs, int rt)
{
    ee_u128 a = R(rs), b = R(rt), d;
    for (int i = 0; i < 4; i++) { d.uh[2 * i] = b.uh[4 + i]; d.uh[2 * i + 1] = a.uh[4 + i]; }
    put(c, rd, &d);
}

void ee_mmi_pextlb(ee_ctx *c, int rd, int rs, int rt)
{
    ee_u128 a = R(rs), b = R(rt), d;
    for (int i = 0; i < 8; i++) { d.ub[2 * i] = b.ub[i]; d.ub[2 * i + 1] = a.ub[i]; }
    put(c, rd, &d);
}

void ee_mmi_pextub(ee_ctx *c, int rd, int rs, int rt)
{
    ee_u128 a = R(rs), b = R(rt), d;
    for (int i = 0; i < 8; i++) { d.ub[2 * i] = b.ub[8 + i]; d.ub[2 * i + 1] = a.ub[8 + i]; }
    put(c, rd, &d);
}

void ee_mmi_ppacb(ee_ctx *c, int rd, int rs, int rt)
{
    ee_u128 a = R(rs), b = R(rt), d;
    for (int i = 0; i < 8; i++) { d.ub[i] = b.ub[2 * i]; d.ub[8 + i] = a.ub[2 * i]; }
    put(c, rd, &d);
}

void ee_mmi_ppach(ee_ctx *c, int rd, int rs, int rt)
{
    ee_u128 a = R(rs), b = R(rt), d;
    for (int i = 0; i < 4; i++) { d.uh[i] = b.uh[2 * i]; d.uh[4 + i] = a.uh[2 * i]; }
    put(c, rd, &d);
}

void ee_mmi_ppacw(ee_ctx *c, int rd, int rs, int rt)
{
    ee_u128 a = R(rs), b = R(rt), d;
    d.uw[0] = b.uw[0]; d.uw[1] = b.uw[2]; d.uw[2] = a.uw[0]; d.uw[3] = a.uw[2];
    put(c, rd, &d);
}

/* 8:8:8:8 words to 1:5:5:5 halfwords, one per word lane. */
void ee_mmi_ppac5(ee_ctx *c, int rd, int rt)
{
    ee_u128 a = R(rt), d;
    for (int i = 0; i < 4; i++) {
        uint32_t w = a.uw[i];
        d.uw[i] = ((w >> 16) & 0x8000) | ((w >> 9) & 0x7c00) | ((w >> 6) & 0x03e0) | ((w >> 3) & 0x001f);
    }
    put(c, rd, &d);
}

/* 1:5:5:5 halfwords to 8:8:8:8 words. */
void ee_mmi_pext5(ee_ctx *c, int rd, int rt)
{
    ee_u128 a = R(rt), d;
    for (int i = 0; i < 4; i++) {
        uint32_t h = a.uw[i];
        d.uw[i] = ((h & 0x001f) << 3) | ((h & 0x03e0) << 6) | ((h & 0x7c00) << 9) | ((h & 0x8000) << 16);
    }
    put(c, rd, &d);
}

void ee_mmi_prevh(ee_ctx *c, int rd, int rt)
{
    ee_u128 a = R(rt), d;
    for (int i = 0; i < 4; i++) { d.uh[i] = a.uh[3 - i]; d.uh[4 + i] = a.uh[7 - i]; }
    put(c, rd, &d);
}

void ee_mmi_prot3w(ee_ctx *c, int rd, int rt)
{
    ee_u128 a = R(rt), d;
    d.uw[0] = a.uw[1]; d.uw[1] = a.uw[2]; d.uw[2] = a.uw[0]; d.uw[3] = a.uw[3];
    put(c, rd, &d);
}

void ee_mmi_pexew(ee_ctx *c, int rd, int rt)
{
    ee_u128 a = R(rt), d;
    d.uw[0] = a.uw[2]; d.uw[1] = a.uw[1]; d.uw[2] = a.uw[0]; d.uw[3] = a.uw[3];
    put(c, rd, &d);
}

void ee_mmi_pexcw(ee_ctx *c, int rd, int rt)
{
    ee_u128 a = R(rt), d;
    d.uw[0] = a.uw[0]; d.uw[1] = a.uw[2]; d.uw[2] = a.uw[1]; d.uw[3] = a.uw[3];
    put(c, rd, &d);
}

void ee_mmi_pinteh(ee_ctx *c, int rd, int rs, int rt)
{
    ee_u128 a = R(rs), b = R(rt), d;
    for (int i = 0; i < 4; i++) { d.uh[2 * i] = b.uh[2 * i]; d.uh[2 * i + 1] = a.uh[2 * i]; }
    put(c, rd, &d);
}

/* ---- HI/LO moves ---- */

void ee_mmi_pmfhi(ee_ctx *c, int rd) { ee_u128 v = get_hi(c); put(c, rd, &v); }
void ee_mmi_pmflo(ee_ctx *c, int rd) { ee_u128 v = get_lo(c); put(c, rd, &v); }
void ee_mmi_pmthi(ee_ctx *c, int rs) { ee_u128 v = R(rs); set_hi(c, &v); }
void ee_mmi_pmtlo(ee_ctx *c, int rs) { ee_u128 v = R(rs); set_lo(c, &v); }

void ee_mmi_pmfhl_lw(ee_ctx *c, int rd)
{
    ee_u128 hi = get_hi(c), lo = get_lo(c), d;
    d.uw[0] = lo.uw[0]; d.uw[1] = hi.uw[0]; d.uw[2] = lo.uw[2]; d.uw[3] = hi.uw[2];
    put(c, rd, &d);
}

void ee_mmi_pmfhl_uw(ee_ctx *c, int rd)
{
    ee_u128 hi = get_hi(c), lo = get_lo(c), d;
    d.uw[0] = lo.uw[1]; d.uw[1] = hi.uw[1]; d.uw[2] = lo.uw[3]; d.uw[3] = hi.uw[3];
    put(c, rd, &d);
}

void ee_mmi_pmfhl_lh(ee_ctx *c, int rd)
{
    ee_u128 hi = get_hi(c), lo = get_lo(c), d;
    d.uh[0] = lo.uh[0]; d.uh[1] = lo.uh[2]; d.uh[2] = hi.uh[0]; d.uh[3] = hi.uh[2];
    d.uh[4] = lo.uh[4]; d.uh[5] = lo.uh[6]; d.uh[6] = hi.uh[4]; d.uh[7] = hi.uh[6];
    put(c, rd, &d);
}

/* ---- multiplies ---- */

/* Eight 16x16 signed products; LO/HI get them in the order the manual gives. */
static void pmult_h(ee_ctx *c, int rd, int rs, int rt, int accumulate)
{
    ee_u128 a = R(rs), b = R(rt), hi = get_hi(c), lo = get_lo(c), d;
    int32_t p[8];
    for (int i = 0; i < 8; i++)
        p[i] = (int32_t)(int16_t)a.uh[i] * (int16_t)b.uh[i];
    if (accumulate) {
        p[0] += lo.sw[0]; p[1] += lo.sw[1]; p[2] += hi.sw[0]; p[3] += hi.sw[1];
        p[4] += lo.sw[2]; p[5] += lo.sw[3]; p[6] += hi.sw[2]; p[7] += hi.sw[3];
    }
    lo.sw[0] = p[0]; lo.sw[1] = p[1]; hi.sw[0] = p[2]; hi.sw[1] = p[3];
    lo.sw[2] = p[4]; lo.sw[3] = p[5]; hi.sw[2] = p[6]; hi.sw[3] = p[7];
    d.sw[0] = p[0]; d.sw[1] = p[2]; d.sw[2] = p[4]; d.sw[3] = p[6];
    set_hi(c, &hi); set_lo(c, &lo);
    put(c, rd, &d);
}

void ee_mmi_pmulth(ee_ctx *c, int rd, int rs, int rt) { pmult_h(c, rd, rs, rt, 0); }
void ee_mmi_pmaddh(ee_ctx *c, int rd, int rs, int rt) { pmult_h(c, rd, rs, rt, 1); }

static void pmult_w(ee_ctx *c, int rd, int rs, int rt, int is_signed)
{
    ee_u128 a = R(rs), b = R(rt), d;
    uint64_t p0, p1;
    if (is_signed) {
        p0 = (uint64_t)((int64_t)a.sw[0] * b.sw[0]);
        p1 = (uint64_t)((int64_t)a.sw[2] * b.sw[2]);
    } else {
        p0 = (uint64_t)a.uw[0] * b.uw[0];
        p1 = (uint64_t)a.uw[2] * b.uw[2];
    }
    c->lo  = (uint64_t)(int64_t)(int32_t)p0; c->hi  = (uint64_t)(int64_t)(int32_t)(p0 >> 32);
    c->lo1 = (uint64_t)(int64_t)(int32_t)p1; c->hi1 = (uint64_t)(int64_t)(int32_t)(p1 >> 32);
    d.ud[0] = p0; d.ud[1] = p1;
    put(c, rd, &d);
}

void ee_mmi_pmultw(ee_ctx *c, int rd, int rs, int rt) { pmult_w(c, rd, rs, rt, 1); }
void ee_mmi_pmultuw(ee_ctx *c, int rd, int rs, int rt) { pmult_w(c, rd, rs, rt, 0); }

/* Pipeline-1 scalar forms: like mult/div/madd but on HI1/LO1. */
void ee_mmi_mult1(ee_ctx *c, int rd, int rs, int rt)
{
    int64_t p = (int64_t)R(rs).sw[0] * R(rt).sw[0];
    c->lo1 = (uint64_t)(int64_t)(int32_t)p;
    c->hi1 = (uint64_t)(int64_t)(int32_t)(p >> 32);
    if (rd) c->r[rd].ud[0] = c->lo1;
}

void ee_mmi_div1(ee_ctx *c, int rs, int rt)
{
    int32_t n = R(rs).sw[0], d = R(rt).sw[0], q, r;
    if (d == 0) { q = n < 0 ? 1 : -1; r = n; }
    else if (n == INT32_MIN && d == -1) { q = n; r = 0; }
    else { q = n / d; r = n % d; }
    c->lo1 = (uint64_t)(int64_t)q;
    c->hi1 = (uint64_t)(int64_t)r;
}

void ee_mmi_divu1(ee_ctx *c, int rs, int rt)
{
    uint32_t n = R(rs).uw[0], d = R(rt).uw[0];
    uint32_t q = d ? n / d : 0xffffffffu, r = d ? n % d : n;
    c->lo1 = (uint64_t)(int64_t)(int32_t)q;
    c->hi1 = (uint64_t)(int64_t)(int32_t)r;
}

static void madd_common(ee_ctx *c, int rd, int rs, int rt, int pipe1, int is_signed)
{
    uint64_t *hi = pipe1 ? &c->hi1 : &c->hi, *lo = pipe1 ? &c->lo1 : &c->lo;
    uint64_t acc = ((uint64_t)(uint32_t)*hi << 32) | (uint32_t)*lo;
    uint64_t p = is_signed ? (uint64_t)((int64_t)R(rs).sw[0] * R(rt).sw[0])
                           : (uint64_t)R(rs).uw[0] * R(rt).uw[0];
    uint64_t sum = acc + p;
    *lo = (uint64_t)(int64_t)(int32_t)sum;
    *hi = (uint64_t)(int64_t)(int32_t)(sum >> 32);
    if (rd) c->r[rd].ud[0] = *lo;
}

void ee_mmi_madd(ee_ctx *c, int rd, int rs, int rt)   { madd_common(c, rd, rs, rt, 0, 1); }
void ee_mmi_maddu(ee_ctx *c, int rd, int rs, int rt)  { madd_common(c, rd, rs, rt, 0, 0); }
void ee_mmi_madd1(ee_ctx *c, int rd, int rs, int rt)  { madd_common(c, rd, rs, rt, 1, 1); }
void ee_mmi_maddu1(ee_ctx *c, int rd, int rs, int rt) { madd_common(c, rd, rs, rt, 1, 0); }

/* ---- shift amount register ---- */

void ee_mmi_mfsa(ee_ctx *c, int rd) { if (rd) c->r[rd].ud[0] = c->sa; }
void ee_mmi_mtsa(ee_ctx *c, int rs) { c->sa = R(rs).uw[0]; }
void ee_mmi_mtsab(ee_ctx *c, int rs, uint32_t imm) { c->sa = (R(rs).uw[0] & 0xf) ^ (imm & 0xf); }
void ee_mmi_mtsah(ee_ctx *c, int rs, uint32_t imm) { c->sa = ((R(rs).uw[0] & 0x7) ^ (imm & 0x7)) << 1; }

/* rd = low 128 bits of (rs:rt) >> (SA * 8), rs being the upper half. */
void ee_mmi_qfsrv(ee_ctx *c, int rd, int rs, int rt)
{
    uint8_t buf[32], out[16];
    unsigned sh = c->sa & 0xf;
    ee_u128 d;
    memcpy(buf, &R(rt), 16);
    memcpy(buf + 16, &R(rs), 16);
    memcpy(out, buf + sh, 16);
    memcpy(&d, out, 16);
    put(c, rd, &d);
}
