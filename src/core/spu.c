/* PS2 SPU2 voice and reverb behaviour, from the hardware documentation
   (nocash psx-spx: "SPU ADPCM", "SPU Interpolation", "SPU Volume and
   ADSR Generator", "SPU Reverb Formula"); SPU2 keeps the PS1 voice and
   reverb units. Registers come from the game's driver, presets from the
   disc's LIBSD.IRX. */
#include "spu.h"
#include <string.h>

/* The hardware's Gaussian interpolation table (psx-spx "SPU
   Interpolation"), 512 entries. */
static const int16_t gauss[512] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    0,0,0,0,0,0,0,1,1,1,1,2,2,2,3,3,
    3,4,4,5,5,6,7,7,8,9,9,10,11,12,13,14,
    15,16,17,18,19,21,22,24,25,27,28,30,32,33,35,37,
    39,41,44,46,48,51,53,56,58,61,64,67,70,73,77,80,
    84,87,91,95,99,103,107,111,116,120,125,130,135,140,145,150,
    156,161,167,173,179,186,192,199,205,212,219,227,234,242,250,257,
    266,274,283,291,300,309,319,328,338,348,358,369,379,390,401,412,
    424,436,448,460,473,485,498,512,525,539,553,567,582,597,612,627,
    643,659,675,692,708,726,743,761,779,797,816,835,854,874,894,914,
    935,956,977,999,1020,1043,1066,1089,1112,1136,1160,1184,1209,1234,1260,1286,
    1312,1339,1366,1394,1422,1450,1479,1508,1537,1567,1598,1628,1660,1691,1723,1756,
    1789,1822,1856,1890,1924,1959,1995,2031,2067,2104,2141,2179,2217,2256,2295,2334,
    2374,2415,2456,2497,2539,2582,2624,2668,2712,2756,2801,2846,2892,2938,2985,3032,
    3079,3128,3176,3225,3275,3325,3376,3427,3479,3531,3584,3637,3691,3745,3799,3855,
    3910,3967,4023,4081,4138,4197,4255,4315,4374,4435,4495,4557,4619,4681,4744,4807,
    4871,4935,5000,5065,5131,5197,5264,5332,5399,5468,5536,5606,5676,5746,5817,5888,
    5959,6032,6104,6177,6251,6325,6400,6475,6550,6626,6702,6779,6856,6934,7012,7091,
    7170,7249,7329,7409,7490,7571,7653,7735,7817,7900,7983,8066,8150,8234,8319,8404,
    8489,8575,8661,8748,8834,8922,9009,9097,9185,9273,9362,9451,9541,9630,9720,9811,
    9901,9992,10083,10174,10266,10358,10450,10542,10635,10727,10820,10913,11007,11100,11194,11288,
    11382,11476,11571,11665,11760,11855,11950,12045,12140,12236,12331,12427,12522,12618,12714,12809,
    12905,13001,13097,13193,13289,13385,13481,13577,13673,13769,13865,13961,14056,14152,14248,14343,
    14439,14534,14630,14725,14820,14915,15010,15104,15199,15293,15387,15481,15575,15669,15762,15855,
    15948,16041,16133,16226,16317,16409,16500,16592,16682,16773,16863,16953,17042,17131,17220,17308,
    17396,17484,17571,17658,17744,17830,17916,18001,18086,18170,18254,18337,18420,18502,18584,18665,
    18746,18826,18905,18985,19063,19141,19219,19295,19372,19447,19522,19597,19671,19744,19816,19888,
    19959,20030,20100,20169,20238,20306,20373,20439,20505,20570,20634,20698,20760,20822,20884,20944,
    21004,21063,21121,21178,21235,21290,21345,21399,21452,21505,21556,21607,21657,21706,21754,21801,
    21848,21893,21938,21982,22025,22066,22107,22148,22187,22225,22262,22299,22334,22369,22402,22435,
    22467,22498,22527,22556,22584,22611,22637,22662,22686,22709,22731,22752,22772,22791,22809,22826,
    22842,22857,22872,22885,22897,22908,22918,22927,22935,22942,22948,22953,22957,22960,22962,22963,
};

static int16_t sat16(int32_t v) { return (int16_t)(v < -32768 ? -32768 : v > 32767 ? 32767 : v); }

/* "SPU ADPCM": shift 13..15 act as 9; the filter pairs (60,0),
   (115,-52), (98,-55), (122,-60). Flag bit 2 (loop start) sets LSA when
   its block is decoded. */
static void decode_block(gt4_spu_voice *v)
{
    static const int pos[5] = { 0, 60, 115, 98, 122 }, neg[5] = { 0, 0, -52, -55, -60 };
    const uint8_t *b = v->wave->adpcm + v->block * 16;
    int shift = b[0] & 15, filter = (b[0] >> 4) & 7;
    if (shift > 12) shift = 9;
    if (filter > 4) filter = 4;
    if (b[1] & 4) v->loop = v->block;
    for (int j = 0; j < 28; j++) {
        int n = (b[2 + j / 2] >> ((j & 1) * 4)) & 15;
        int32_t s = (int32_t)(int16_t)(uint16_t)(n << 12) >> shift;
        s += (v->h1 * pos[filter] + v->h2 * neg[filter] + 32) >> 6;
        v->decoded[j] = sat16(s);
        v->h2 = v->h1;
        v->h1 = v->decoded[j];
    }
}

/* At the end of a block: loop end (bit 0) continues at LSA; without
   repeat (bit 1) the voice stops at once (release, level zero). */
static void next_block(gt4_spu_voice *v)
{
    uint8_t flags = v->wave->adpcm[v->block * 16 + 1];
    if (flags & 1) {
        v->block = v->loop;
        if (!(flags & 2)) {
            v->phase = GT4_SPU_OFF;
            v->envx = 0;
        }
    } else if (++v->block >= v->wave->blocks)
        v->block = v->loop;
    decode_block(v);
}

void gt4_spu_key_on(gt4_spu_voice *v)
{
    if (!v || !v->wave || !v->wave->blocks) return;
    v->block = 0;
    v->loop = 0;
    v->h1 = v->h2 = 0;
    v->index = 0;
    v->counter = 0;
    decode_block(v);
    v->ring[0] = v->ring[1] = v->ring[2] = 0;
    v->ring[3] = v->decoded[0];
    v->phase = GT4_SPU_ATTACK;
    v->envx = 0;
    v->env_counter = 0;
}

void gt4_spu_key_off(gt4_spu_voice *v)
{
    if (v && v->phase != GT4_SPU_OFF) {
        v->phase = GT4_SPU_RELEASE;
        v->env_counter = 0;
    }
}

/* "SPU Volume and ADSR Generator": rate = shift*4 + step; below shift 11
   the step grows, above it the level changes every 2^(shift-11) ticks.
   Exponential: decreasing steps scale with the level; increasing ones
   slow to a quarter above 6000h. */
static void envelope_tick(gt4_spu_voice *v, int rate, int decreasing, int exponential)
{
    int shift = rate >> 2;
    int32_t step = decreasing ? -8 + (rate & 3) : 7 - (rate & 3), increment = 0x8000;
    if (shift < 11) step *= 1 << (11 - shift);
    else increment = shift - 11 > 15 ? 0 : 0x8000 >> (shift - 11);
    if (exponential) {
        if (decreasing) step = (step * v->envx) >> 15;
        else if (v->envx >= 0x6000) {
            if (rate < 40) step >>= 2;
            else if (rate >= 44) increment >>= 2;
            else {
                step >>= 1;
                increment >>= 1;
            }
        }
    }
    v->env_counter -= increment;
    if (v->env_counter <= 0) {
        int32_t level = v->envx + step;
        v->env_counter += 0x8000;
        v->envx = (int16_t)(level < 0 ? 0 : level > 0x7fff ? 0x7fff : level);
    }
}

static void adsr_tick(gt4_spu_voice *v)
{
    uint16_t a1 = v->adsr1, a2 = v->adsr2;
    int32_t sustain = ((a1 & 15) + 1) * 0x800;
    if (sustain > 0x7fff) sustain = 0x7fff;
    switch (v->phase) {
    case GT4_SPU_ATTACK:
        envelope_tick(v, (a1 >> 8) & 0x7f, 0, a1 >> 15);
        if (v->envx >= 0x7fff) {
            v->phase = GT4_SPU_DECAY;
            v->env_counter = 0;
        }
        break;
    case GT4_SPU_DECAY:
        envelope_tick(v, ((a1 >> 4) & 15) << 2, 1, 1);
        if (v->envx <= sustain) {
            v->phase = GT4_SPU_SUSTAIN;
            v->env_counter = 0;
        }
        break;
    case GT4_SPU_SUSTAIN:
        envelope_tick(v, (a2 >> 6) & 0x7f, (a2 >> 14) & 1, a2 >> 15);
        break;
    case GT4_SPU_RELEASE:
        envelope_tick(v, (a2 & 31) << 2, 1, (a2 >> 5) & 1);
        if (v->envx <= 0) v->phase = GT4_SPU_OFF;
        break;
    default:
        break;
    }
}

void gt4_spu_voice_render(gt4_spu_voice *v, int32_t *dry, int32_t *wet, int frames)
{
    if (!v || !v->wave || v->phase == GT4_SPU_OFF) return;
    /* Fixed volume: bits 14..0 hold volume/2, sign in bit 14. */
    int32_t left = (int16_t)(uint16_t)((uint16_t)v->voll << 1), right = (int16_t)(uint16_t)((uint16_t)v->volr << 1);
    uint32_t pitch = v->pitch > 0x3fff ? 0x3fff : v->pitch;
    for (int f = 0; f < frames && v->phase != GT4_SPU_OFF; f++) {
        int i = (v->counter >> 4) & 0xff;
        int32_t s = ((gauss[0x0ff - i] * v->ring[0]) >> 15) + ((gauss[0x1ff - i] * v->ring[1]) >> 15) +
                    ((gauss[0x100 + i] * v->ring[2]) >> 15) + ((gauss[i] * v->ring[3]) >> 15);
        s = (sat16(s) * v->envx) >> 15;
        int32_t l = (s * left) >> 15, r = (s * right) >> 15;
        dry[f * 2] += l;
        dry[f * 2 + 1] += r;
        if (v->reverb && wet) {
            wet[f * 2] += l;
            wet[f * 2 + 1] += r;
        }
        adsr_tick(v);
        v->counter += pitch;
        while (v->counter >= 0x1000) {
            v->counter -= 0x1000;
            if (++v->index == 28) {
                v->index = 0;
                next_block(v);
            }
            v->ring[0] = v->ring[1];
            v->ring[1] = v->ring[2];
            v->ring[2] = v->ring[3];
            v->ring[3] = v->decoded[v->index];
        }
    }
}

/* LIBSD.IRX keeps one 34-halfword record per reverb mode: the 32
   registers in the PS1 order, then two spare halfwords. Mode 0 (off)
   only sets the four same/diff reflection addresses to 1; modes 1..6
   take the input at full scale (vLIN = vRIN = 8000h). */
int gt4_spu_reverb_preset(gt4_spu_reverb *r, const uint8_t *b, size_t n, unsigned mode)
{
    enum { RECORD = 0x44 };
    if (!r || !b || mode < 1 || mode > 9) return 0;
    for (size_t o = 0; o + RECORD * 10 <= n; o += 2) {
        int match = 1;
        for (unsigned k = 0; k < 32 && match; k++) {
            unsigned v = b[o + k * 2] | b[o + k * 2 + 1] << 8;
            match = v == (k == 10 || k == 11 || k == 18 || k == 19);
        }
        for (unsigned m = 1; m <= 6 && match; m++) {
            const uint8_t *p = b + o + m * RECORD + 60;
            match = p[0] == 0 && p[1] == 0x80 && p[2] == 0 && p[3] == 0x80;
        }
        if (!match) continue;
        for (unsigned k = 0; k < 32; k++)
            r->regs[k] = (uint16_t)(b[o + mode * RECORD + k * 2] | b[o + mode * RECORD + k * 2 + 1] << 8);
        return 1;
    }
    return 0;
}

static int16_t mul(int32_t a, int16_t v) { return sat16((a * v) >> 15); }

/* One 24 kHz step of "SPU Reverb Formula". Register addresses are in
   8-byte units (four halfwords); "-2" bytes is the previous sample. */
static void reverb_step(gt4_spu_reverb *r, int16_t in_l, int16_t in_r, int16_t out[2])
{
    const uint16_t *g = r->regs;
#define AT(reg, back) r->buffer[(r->at + (uint32_t)g[reg] * 4u + r->size - (back)) % r->size]
    int16_t vIIR = (int16_t)g[2], vC1 = (int16_t)g[3], vC2 = (int16_t)g[4], vC3 = (int16_t)g[5], vC4 = (int16_t)g[6];
    int16_t vWALL = (int16_t)g[7], vAPF1 = (int16_t)g[8], vAPF2 = (int16_t)g[9], vLIN = (int16_t)g[30], vRIN = (int16_t)g[31];
    uint32_t d1 = g[0] * 4u, d2 = g[1] * 4u;
    int16_t lin = mul(in_l, vLIN), rin = mul(in_r, vRIN);
    int16_t lsame = sat16(mul(sat16(lin + mul(AT(16, 0), vWALL) - AT(10, 1)), vIIR) + AT(10, 1));
    int16_t rsame = sat16(mul(sat16(rin + mul(AT(17, 0), vWALL) - AT(11, 1)), vIIR) + AT(11, 1));
    int16_t ldiff = sat16(mul(sat16(lin + mul(AT(25, 0), vWALL) - AT(18, 1)), vIIR) + AT(18, 1));
    int16_t rdiff = sat16(mul(sat16(rin + mul(AT(24, 0), vWALL) - AT(19, 1)), vIIR) + AT(19, 1));
    AT(10, 0) = lsame;
    AT(11, 0) = rsame;
    AT(18, 0) = ldiff;
    AT(19, 0) = rdiff;
    int32_t lout = mul(AT(12, 0), vC1) + mul(AT(14, 0), vC2) + mul(AT(20, 0), vC3) + mul(AT(22, 0), vC4);
    int32_t rout = mul(AT(13, 0), vC1) + mul(AT(15, 0), vC2) + mul(AT(21, 0), vC3) + mul(AT(23, 0), vC4);
    int16_t l = sat16(lout - mul(AT(26, d1), vAPF1)), rr = sat16(rout - mul(AT(27, d1), vAPF1));
    lout = sat16(mul(l, vAPF1) + AT(26, d1));
    rout = sat16(mul(rr, vAPF1) + AT(27, d1));
    AT(26, 0) = l;
    AT(27, 0) = rr;
    l = sat16(lout - mul(AT(28, d2), vAPF2));
    rr = sat16(rout - mul(AT(29, d2), vAPF2));
    lout = sat16(mul(l, vAPF2) + AT(28, d2));
    rout = sat16(mul(rr, vAPF2) + AT(29, d2));
    AT(28, 0) = l;
    AT(29, 0) = rr;
#undef AT
    out[0] = mul(lout, r->evoll);
    out[1] = mul(rout, r->evolr);
    r->at = (r->at + 1) % r->size;
}

/* A: the hardware's half-rate filters around the reverb unit are not
   reproduced; the pair average and linear upsampling stand in. */
void gt4_spu_reverb_render(gt4_spu_reverb *r, const int32_t *wet, int32_t *dry, int frames)
{
    if (!r || !r->buffer || !r->size) return;
    for (int f = 0; f < frames; f++) {
        r->half[0] += wet[f * 2];
        r->half[1] += wet[f * 2 + 1];
        if (++r->phase == 2) {
            int16_t out[2];
            r->phase = 0;
            reverb_step(r, sat16(r->half[0] / 2), sat16(r->half[1] / 2), out);
            r->half[0] = r->half[1] = 0;
            for (int c = 0; c < 2; c++) {
                r->out[c][0] = r->out[c][1];
                r->out[c][1] = out[c];
            }
        }
        for (int c = 0; c < 2; c++)
            dry[f * 2 + c] += r->phase ? (r->out[c][0] + r->out[c][1]) / 2 : r->out[c][1];
    }
}
