/*
 * Runs the original vehicle simulation step (func_0034DB30, recompiled from
 * the user's own CORE.GT4) on a RAM snapshot taken from a PCSX2 savestate
 * during a race, for docs/analysis/PHYSICS.md. The recompiled step is the
 * oracle the C rewrite of the physics is compared with.
 *
 * Usage: physics_run <eeMemory.bin> <Scratchpad.bin|-> <conductor> <frames> <out_dir> [native]
 *
 * `native` lists the original functions to replace with their rewrite
 * (src/core/physics), as comma-separated addresses or "all"; the run then
 * says how many times each replacement ran. Compare cars.bin of a run with
 * and without replacements (tests/physics/compare.py).
 *
 * Output in out_dir:
 *   cars.bin     for each frame (0 = snapshot, 1..frames after each step)
 *                the whole car array: 0x10a0 bytes per car, ncars cars
 *   trace.jsonl  kernel calls and the end event, if any
 *   ram.bin      guest RAM after the last step
 *
 * Nothing here is from the disc: the snapshot and the generated code stay
 * local (captures/, build/).
 */
#include "ee.h"
#include "phys.h"
#ifdef GT4_CONTROLLER_PACE_TEST
#include "driver_fixture.h"
#endif
#include "natives.h"
#include "race_setup.h"
#include "race_build.h"
#include "pad_fixed.h"
#include "spectables.h"
#include "vol.h"
#include "state_digest.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

ee_fn recomp_lookup(uint32_t addr);
extern const ee_code_range recomp_code_ranges[];
extern const int recomp_n_code_ranges;

#define STEP        0x0034db30u
#define CAR_OFFSET  0x6698u
#define CAR_SIZE    0x10a0u
#define NCARS_OFF   (0x10000u - 0x760u)
/* TEST INPUT: a stack in RAM that is zero in the snapshots (checked by the
   caller), and a return address no code lives at. */
#define STACK_TOP   0x01ef0000u
#define RETURN_PC   0x00000000u

/* PHYS_INPUT_SEED=n: the controller read (func_00426D50) returns made-up
   commands instead of the recorded pad, the same sequence in every run with
   that seed: device kind, analog or digital per axis, values held for 8 to
   40 calls. Reaches the digital ramps, the wheel paths and the assists. */
#define READ_PAD 0x00426d50u
static uint32_t input_state;
static int input_hold;
static uint8_t input_cmd[16];
static uint32_t input_rand(void)
{
    input_state = input_state * 1103515245u + 12345u;
    return input_state >> 8;
}
static void input_next(void)
{
    static const uint8_t kinds[] = { 2, 2, 2, 4, 4, 6, 3, 0 };
    uint8_t flags = (uint8_t)(input_rand() & 0x1f);
    memset(input_cmd, 0, sizeof input_cmd);
    input_cmd[0] = flags;
    input_cmd[1] = kinds[input_rand() % 8];
    input_cmd[2] = (uint8_t)(input_rand() % 4 == 0);
    if (getenv("PHYS_INPUT_SHIFT") && input_cmd[2] && input_rand() % 2) input_cmd[2] = 0xff;   /* shift down too */
    {
        int16_t steer = (flags & 1) ? (int16_t)((int)(input_rand() % 8193) - 4096)
                                    : (int16_t)((int)(input_rand() % 3) - 1);
        static const int fields[4] = { 8, 0xa, 0xc, 0xe }, bits[4] = { 2, 4, 8, 16 };
        memcpy(input_cmd + 4, &steer, 2);
        for (int k = 0; k < 4; k++) {
            int16_t v = (flags & bits[k]) ? (int16_t)(input_rand() % 4097) : (int16_t)(input_rand() % 2);
            if (input_rand() % 3 == 0) v = 0;
            memcpy(input_cmd + fields[k], &v, 2);
        }
    }
    input_hold = 8 + (int)(input_rand() % 33);
}
static void input_read(ee_ctx *c)
{
    uint32_t out = c->r[5].uw[0], obj = c->r[4].uw[0];
    if (--input_hold <= 0) input_next();
    for (int k = 0; k < 32; k++) ee_w8(c, out + k, k < 16 ? input_cmd[k] : 0);
    ee_w8(c, out + 3, ee_r8(c, obj + 0x153));
}
static ee_fn input_hook(uint32_t addr) { return addr == READ_PAD ? input_read : NULL; }

/* Same explicitly documented input fixture as gtmode/series_playable.c.
   Supplies pad commands on both oracle paths; never patches car state. */
static uint32_t series_input_car;
static int series_read(void *user,uint32_t car,uint8_t *cmd,const uint8_t *event)
{
    (void)user;memset(cmd,0,32);cmd[1]=1;cmd[3]=event[3];
    if(GT4_F32(car+0x5d0)>0 && GT4_F32(car+0x5d4)<10){cmd[0x14]=6;cmd[0x15]=1;}
    return 1;
}
static int series_state(void *user,uint32_t car,int16_t state[2])
{(void)user;(void)car;state[0]=state[1]=1;return 1;}
static int series_write(void *user,uint32_t car,const uint8_t *cmd)
{(void)user;(void)car;(void)cmd;return 1;}
static void series_pad_original(ee_ctx *c)
{
    uint32_t out=c->r[5].uw[0],event=c->r[6].uw[0];
    series_read(NULL,series_input_car,c->ram+out,c->ram+event);c->r[2].uw[0]=1;
}
static void series_state_original(ee_ctx *c)
{int16_t state[2]={1,1};memcpy(c->ram+c->r[5].uw[0],state,4);c->r[2].uw[0]=1;}
static void series_write_original(ee_ctx *c){c->r[2].uw[0]=1;}
static ee_fn series_input_hook(uint32_t addr)
{
    switch(addr){case READ_PAD:return series_pad_original;case 0x426ce0:return series_state_original;
    case 0x426ea0:return series_write_original;default:return NULL;}
}

static long read_file(const char *path, uint8_t *dst, size_t size)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f) return -1;
    n = (long)fread(dst, 1, size, f);
    fclose(f);
    return n;
}

#ifdef _MSC_VER
#include <stdlib.h>
static void invalid_parameter(const wchar_t *e, const wchar_t *f, const wchar_t *file, unsigned line, uintptr_t r)
{
    (void)e; (void)f; (void)file; (void)line; (void)r;
    fprintf(stderr, "CRT: invalid parameter\n");
    phys_print_recent();
    fflush(stderr);
}
#endif

/* Find an actual captured SpecDB row for each side of an A-spec branch. */
static uint32_t aspec_row(ee_ctx *c, uint32_t table, int type_two)
{
    uint32_t blob = ee_r32(c, ee_r32(c, 0x006235a8u + 0x134 + table * 4));
    uint8_t row[256];
    for (uint32_t j = 0; j < ee_r32(c, blob + 8); j++) {
        uint32_t key = ee_r32(c, blob + 16 + j * 8);
        if (phys_memory_src.row(NULL, table, key, 0, row, sizeof row) > 0 &&
            (row[0x10] == 2) == type_two) return key;
    }
    fprintf(stderr, "A-spec test: no table %u row with type_two=%d\n", table, type_two);
    exit(2);
}

/* PHYS_AB=<case>[:trials]: functions no capture reaches, original against
   rewrite from the same made-up state of every car, compared over all
   the cars' memory. Cases: pit (func_00463CD8). */
static int ab_test(ee_ctx *c, uint32_t conductor, uint32_t ncars, const char *spec)
{
    uint32_t base = conductor + CAR_OFFSET, size = ncars * CAR_SIZE, seed = 12345;
    int trials = strchr(spec, ':') ? atoi(strchr(spec, ':') + 1) : 2000, bad = 0, moved = 0;
    uint8_t *keep = malloc(EE_RAM_SIZE), *orig = malloc(EE_RAM_SIZE);
    if (!keep || !orig) return 2;
    uint32_t aspec_course[2] = { 0 }, aspec_drive[2] = { 0 };
    if (!strncmp(spec, "aspec", 5))
        for (int k = 0; k < 2; k++) {
            aspec_course[k] = aspec_row(c, 0x22, k);
            aspec_drive[k] = aspec_row(c, 0x0a, k);
        }
    jmp_buf ended;
    c->exit_jmp = &ended;
    c->r[29].uw[0] = STACK_TOP;
    if (setjmp(ended) != 0) {
        fprintf(stderr, "run ended at 0x%08x: %s\n", c->end_pc, c->end_detail);
        phys_print_recent();
        return 2;
    }
#define RND() (seed = seed * 1103515245u + 12345u, seed >> 8)
    for (int t = 0; t < trials; t++) {
        uint32_t car, obj, b, hull = 0;
        base = conductor + CAR_OFFSET;
        size = ncars * CAR_SIZE;
        car = base + (uint32_t)(t % (int)ncars) * CAR_SIZE;
        obj = car + 0x14;
        b = car + 0x104;
        int32_t arg = 0;
        if (!strncmp(spec, "aspec", 5)) {
            /* func_003C0FF8: both COURSE/DRIVETRAIN adjustment branches,
               random/equal ratings, each car watched, one-car races and
               the 250-point mode. Rows come from the captured SpecDB. */
            uint32_t race = ee_r32(c, conductor), entries = ee_r32(c, race + 0x60);
            uint32_t list = ee_r32(c, entries + 8), world = ee_r32(c, race + 0x84);
            uint32_t n = 1 + (uint32_t)t % ncars, watched = RND() % n, expected, actual;
            ee_w32(c, entries, n);
            ee_w32(c, world + 0xd68, t % 11 == 0 ? 10 : 1);
            ee_w32(c, ee_r32(c, race + 0x70) + 0xa0, aspec_course[(t / 6) % 2]);
            for (uint32_t k = 0; k < ncars; k++) {
                uint32_t e = ee_r32(c, list + k * 4), p = ee_r32(c, ee_r32(c, e + 0x18) + 0x10);
                float rating = (float)(RND() % 100001) / 100000.f;
                if (t % 13 == 0) rating = .5f;
                memcpy(c->ram + p + 0xfe0, &rating, 4);
                ee_w32(c, e + 0x60, aspec_drive[(t / 12) % 2]);
                ee_w32(c, e + 0x64, 0x0a);
            }
            gt4_orig_call(0x003c0ff8u, race, watched, 0, 0);
            expected = gt4_orig_v0;
            actual = gt4_race_aspec_points(race, (int32_t)watched, &phys_memory_src);
            if (expected != actual) {
                if (bad < 5) printf("trial %d: %u cars, player %u: %u/%u points\n", t, n, watched, expected, actual);
                bad++;
            }
            ee_w32(c, entries, ncars);
            moved++;
            continue;
        } else if (!strncmp(spec, "pit", 3)) {
            float f = (float)(int32_t)(RND() % 2000) / 1000.0f - 0.5f;
            ee_w16(c, b + 0x4fa, (uint16_t)(RND() % 9000));
            ee_w8(c, obj + 0x3f, (uint8_t)(RND() % 2));
            memcpy(c->ram + obj + 0x34, &f, 4);
            ee_w8(c, car + 0x5b3, (uint8_t)(int8_t)((int)(RND() % 81) - 40));
            arg = (int32_t)(RND() % 30);
        } else if (!strncmp(spec, "curve", 5)) {
            /* the input curves: result and slope */
            static const uint32_t tables[] = { 0x00843538u, 0x00843548u, 0x00843558u };
            uint32_t tb = tables[t % 3], slot = 0x01ee8000u;
            if (t % 2) {   /* a made-up table of every type */
                float p3[3] = { (float)(int32_t)(RND() % 1000) / 2000.0f, (float)(int32_t)(RND() % 2000) / 1000.0f - 1.0f,
                                (float)(int32_t)(RND() % 4000) / 1000.0f - 2.0f };
                tb = slot + 0x10;
                ee_w32(c, tb, (uint32_t)(t / 2 % 5));
                memcpy(c->ram + tb + 4, p3, 12);
            }
            float x = (float)(int32_t)(RND() % 40001) / 10000.0f - 2.0f, y0, y1, d1 = 0;
            ee_w32(c, slot, 0);
            y0 = gt4_orig_call(0x0035a9d0u, tb, (t & 4) ? slot : 0, x, 0);
            y1 = gt4_input_curve(tb, (t & 4) ? &d1 : NULL, x);
            if (memcmp(&y0, &y1, 4) || ((t & 4) && memcmp(c->ram + slot, &d1, 4))) {
                if (bad < 5) printf("trial %d: table %08x x %g: %g/%g\n", t, tb, x, y0, y1);
                bad++;
            }
            moved++;
            continue;
        } else if (!strncmp(spec, "drvset", 6)) {
            /* the driving settings' expansion (func_00347A00): the bytes
               of D_00622110 (0x80) and D_0062218C (3 rows) made up */
            for (uint32_t k = 0; k < 0x100; k++) ee_w8(c, 0x00622110u + k, (uint8_t)((RND() % 4) ? RND() : 0));
            ee_w8(c, conductor + 0x10000u - 0x3422u, (uint8_t)(RND() % 3));
            base = 0;
            size = EE_RAM_SIZE;
            hull = conductor;
        }
#ifdef GT4_CONTROLLER_PACE_TEST
        else if (!strncmp(spec,"kind",4)) {
            arg=t-128;
            ee_w8(c,car+0x56a,(uint8_t)RND());ee_w8(c,car+0x56b,(uint8_t)RND());
            hull=car;
        }
#endif
        else if (!strncmp(spec, "event", 5)) {
            /* a controller event (func_003F19E0): kinds 4, 5, 6, 10 with
               made-up values and state, the whole RAM compared */
            static const int kinds[] = { 4, 5, 6, 10, 3 };
            uint32_t b2 = car + 0x104, o2 = car + 0x14, world = ee_r32(c, ee_r32(c, car) + 0x84);
            arg = kinds[RND() % 5];
            ee_w8(c, o2 + 0x6d, (uint8_t)(RND() % 3));
            ee_w8(c, o2 + 0x6e, (uint8_t)(RND() % 2));
            ee_w8(c, o2 + 0x1c, (uint8_t)(RND() % 7));
            ee_w8(c, b2 + 0x467, (uint8_t)(RND() % 2 ? 0 : 1));
            ee_w8(c, b2 + 0x4b1, (uint8_t)RND());
            ee_w16(c, b2 + 0x4ac, (uint16_t)(RND() % 3));
            ee_w32(c, world + 0xccc, RND() % 3);
            ee_w32(c, world + 0xcc0, (RND() % 6) - 1);
            {
                float fu = (RND() % 2) ? 10.0f : 0.0f;
                memcpy(c->ram + car + 0x5d0, &fu, 4);
            }
#ifdef GT4_CONTROLLER_PACE_TEST
            ee_w32(c, 0x01ee8100u, RND() % 5);
#else
            ee_w32(c, 0x01ee8100u, RND() % 3);
#endif
            base = 0;
            size = EE_RAM_SIZE;
            hull = car;
        } else if (!strncmp(spec, "model", 5)) {
            /* the driver model (func_00465078) on the pit line (4), or
               blending from it, at a made-up section */
            uint32_t lines = ee_r32(c, obj + 0xec), cnt;
            car = base + (uint32_t)(1 + t % ((int)ncars - 1)) * CAR_SIZE;
            obj = car + 0x14;
            lines = ee_r32(c, obj + 0xec);
            cnt = ee_r32(c, ee_r32(c, ee_r32(c, lines + 8) + 4 * 4));
            if (t & 1) ee_w8(c, obj + 0x1c, 4); else ee_w8(c, obj + 0x21, 4);
            ee_w8(c, obj + 0x1d, (uint8_t)(cnt ? RND() % cnt : 0));
            {
                float bl = (float)(int32_t)(RND() % 1000) / 1000.0f;
                memcpy(c->ram + obj + 0x2c, &bl, 4);
            }
            base = 0;
            size = EE_RAM_SIZE;
            hull = obj;
        } else if (!strncmp(spec, "finish", 6) || !strncmp(spec, "orders", 6)) {
            /* the model taking a car over at the end (func_00343D78) and
               the pit crew's orders (func_0035BFE8): made-up flags and
               settings, the whole RAM compared */
            uint32_t b2 = car + 0x104;
            ee_w8(c, b2 + 0x4b2, (uint8_t)(RND() % 2));
            ee_w8(c, b2 + 0x467, (uint8_t)(RND() % 9));
            ee_w8(c, b2 + 0x466, (uint8_t)(RND() % 5));
            ee_w8(c, b2 + 0x4b1, (uint8_t)RND());
            ee_w32(c, b2 + 0x464, (RND() % 3) ? 0 : RND() << 16);
            ee_w8(c, car + 0x30, (uint8_t)((RND() % 2) ? 7 : 0));
            ee_w8(c, conductor + 0x10000u - 0x3422u, (uint8_t)(RND() % 3));
            ee_w8(c, conductor + 0x10000u - 0x341fu, (uint8_t)(RND() % 2));
            for (int q = 0; q < 12; q++) ee_w8(c, conductor + 0xf824u + q, (uint8_t)((RND() % 3) ? 0 : 50 + RND() % 100));
            ee_w8(c, 0x00620de9u, (uint8_t)(RND() % 2));
            ee_w8(c, 0x00623e50u + 0xa, (uint8_t)(RND() % 2));
            ee_w8(c, 0x00623e50u + 0xb, (uint8_t)(RND() % 2));
            ee_w16(c, b2 + 0x4b8, (uint16_t)RND());
            ee_w32(c, 0x01ee8100u, RND());
            arg = (int32_t)(RND() % 6);
            if (!strncmp(spec, "orders", 6)) ee_w8(c, 0x01ee8100u, (uint8_t)(1 + RND() % 2));
            base = 0;
            size = EE_RAM_SIZE;
            hull = car;
        } else if (!strncmp(spec, "tyre", 4)) {
            /* tyre wear and heat: made-up marks (ascending), flags and
               wheel wear and temperature */
            uint32_t ss = conductor + 4;
            float m[8], h[6];
            m[0] = -(float)(int32_t)(RND() % 1000) / 10.0f; m[1] = (float)(int32_t)(RND() % 50) / 100.0f;
            m[2] = (float)(int32_t)(RND() % 100) / 10.0f + 0.1f; m[3] = (float)(int32_t)(RND() % 50) / 100.0f;
            m[4] = m[2] + (float)(int32_t)(RND() % 100) / 10.0f + 0.1f; m[5] = (float)(int32_t)(RND() % 50) / 100.0f;
            m[6] = m[4] + (float)(int32_t)(RND() % 100) / 10.0f + 0.1f; m[7] = (float)(int32_t)(RND() % 50) / 100.0f;
            h[0] = -(float)(int32_t)(RND() % 1000) / 10.0f - 1.0f; h[1] = (float)(int32_t)(RND() % 50) / 100.0f;
            h[2] = (float)(int32_t)(RND() % 1000) / 10.0f + 1.0f; h[3] = (float)(int32_t)(RND() % 50) / 100.0f;
            h[4] = h[2] + (float)(int32_t)(RND() % 1000) / 10.0f + 1.0f; h[5] = (float)(int32_t)(RND() % 50) / 100.0f;
            memcpy(c->ram + ss, m, 32);
            memcpy(c->ram + ss + 0x50, h, 24);
            ee_w8(c, ss + 0x68, (uint8_t)(t & 1));
            ee_w8(c, ss + 0x69, (uint8_t)((t >> 1) & 1 | ((t & 3) == 0)));
            for (int i = 0; i < 4; i++) {
                float wv = (float)(int32_t)(RND() % 60001) / 1000.0f - 30.0f, tv = (float)(int32_t)(RND() % 40001) / 100.0f - 150.0f;
                memcpy(c->ram + car + 0x164 + i * 0xec + 0xac, &wv, 4);
                memcpy(c->ram + car + 0x164 + i * 0xec + 0xa8, &tv, 4);
            }
            hull = car;
        } else if (!strncmp(spec, "frame", 5)) {
            /* the ground under the car for the three-wheel layouts (2, 3):
               made-up wheel heights and yaw */
            uint32_t params = ee_r32(c, car + 0x10);
            ee_w8(c, params + 0x30, (uint8_t)(2 + t % 2));
            for (int i = 0; i < 4; i++) {
                float hgt = (float)(int32_t)(RND() % 2001) / 1000.0f - 1.0f;
                memcpy(c->ram + car + 0x164 + i * 0xec + 0x8c, &hgt, 4);
            }
            {
                float yaw = (float)(int32_t)(RND() % 6283) / 1000.0f - 3.14f;
                memcpy(c->ram + car + 0x674, &yaw, 4);
            }
            hull = car;
        } else if (!strncmp(spec, "corner", 6)) {
            /* a car's wall hull with a made-up motion, an edge and a wall
               corner (in the unused stack area) */
            uint32_t h = conductor + 0xda6cu + 0xcu + (uint32_t)(t % (int)ncars) * 0x4f0u, w = 0x01ee8000u;
            uint32_t k = RND() % 8, e = h + 0x228u + k * 0x2cu;
            float v[10];
            for (int q = 0; q < 10; q++) v[q] = (float)(int32_t)(RND() % 20001) / 1000.0f - 10.0f;
            memcpy(c->ram + h + 0x390, &v[0], 8);            /* position x, y */
            memcpy(c->ram + h + 0x39c, &v[2], 4);            /* yaw */
            memcpy(c->ram + h + 0x3c4, &v[3], 12);           /* velocity */
            v[6] = v[6] / 5.0f;
            memcpy(c->ram + h + 0x3d0, &v[6], 4);            /* yaw rate */
            memcpy(c->ram + w, &v[7], 4);
            memcpy(c->ram + w + 8, &v[8], 4);
            {
                float a = v[9] * 0.3f, n[2] = { cosf(a), sinf(a) };
                memcpy(c->ram + e + 0x10, n, 8);
            }
            ee_w32(c, e + 0x28, w);
            ee_w32(c, h + 0x42c, k);
            hull = h;
            base = conductor + CAR_OFFSET;                    /* the cars through the hulls */
            size = 0xda78u + 6 * 0x4f0u - CAR_OFFSET;
            car = ee_r32(c, h + 0x450);
            (void)obj; (void)b; (void)car;
        } else if (!strncmp(spec, "queue", 5)) {
            /* the race's event queue (func_003B76F0, 003B7668, 003B77E0):
               made-up events, often a full queue; an add, a take or a
               look, the result at 0x01EE8200 */
            uint32_t q = 0x01ee9000u, count = (RND() % 3) ? 0x100u - RND() % 4 : RND() % 0x101;
            if (count > 0x100) count = 0x100;
            memset(c->ram + q + 0x808, 0, 0x1c);
            for (uint32_t i = 0; i < 0x100; i++) {
                uint32_t k = RND() % 0x1c;
                ee_w16(c, q + 8 + i * 8, (uint16_t)k);
                ee_w16(c, q + 10 + i * 8, (uint16_t)RND());
                ee_w32(c, q + 12 + i * 8, RND());
                if (i < count) ee_w8(c, q + 0x808 + k, (uint8_t)(ee_r8(c, q + 0x808 + k) + 1));
            }
            ee_w32(c, q + 0x824, count);
            ee_w32(c, 0x01ee8100u, 0);
            ee_w32(c, 0x01ee8200u, 0);
            arg = (int32_t)(RND() % 0x1c);
            hull = q;
            base = 0;
            size = EE_RAM_SIZE;
        } else if (!strncmp(spec, "racebegin", 9) || !strncmp(spec, "raceafter", 9)) {
            /* the race's start (func_003C0190: made-up start settings and
               time limit) and its part after the step (func_003C0310:
               made-up events 8, 15 and messages 0x11, 0x12, the cars'
               positions) */
            uint32_t race = ee_r32(c, conductor), q = race + 0xf4, n = RND() % 40;
            uint32_t set = ee_r32(c, race + 0x70);
            ee_w8(c, set + 0x40, (uint8_t)(RND() % 6));
            ee_w8(c, set + 0x41, (uint8_t)(RND() % 12));
            ee_w32(c, race + 0xc4, (RND() % 2) ? RND() % 10 : 0);
            memset(c->ram + q + 0x808, 0, 0x1c);
            for (uint32_t i = 0; i < n; i++) {
                static const uint16_t kinds[] = { 8, 0xf, 0x11, 0x12, 4, 0x18 };
                uint16_t k = kinds[RND() % 6];
                uint32_t v = k == 0xf ? RND() % 2 : (RND() % 6) | (RND() << 6);
                ee_w16(c, q + 8 + i * 8, k);
                ee_w16(c, q + 10 + i * 8, 2);
                ee_w32(c, q + 12 + i * 8, v);
                ee_w8(c, q + 0x808 + k, (uint8_t)(ee_r8(c, q + 0x808 + k) + 1));
            }
            ee_w32(c, q + 0x824, n);
            for (uint32_t i = 0; i < ncars; i++) ee_w8(c, base + i * CAR_SIZE + 0x5b3, (uint8_t)(RND() % ncars));
            hull = race;
            base = 0;
            size = EE_RAM_SIZE;
        } else if (!strncmp(spec, "raceevent", 9)) {
            /* the race's events from the step (func_0035E550 lap,
               0035E428 split, 0035E000 pit, 0035E8E8 laps behind, 0035E078
               gap, 0035DF80 event): made-up flags, times and an entry's
               lap list near full; op, car and values at 0x01EE8100.. */
            uint32_t race = ee_r32(c, conductor), i = RND() % ncars, cr = base + i * CAR_SIZE;
            uint32_t e = ee_r32(c, ee_r32(c, ee_r32(c, race + 0x60) + 8) + i * 4);
            ee_w8(c, cr + 0x104 + 0x441, (uint8_t)(RND() % 5 == 0));
            ee_w8(c, cr + 0x104 + 0x632, (uint8_t)(RND() % 5 == 0));
            ee_w8(c, cr + 0x736, (uint8_t)(RND() % 5 == 0));
            ee_w32(c, race + 0xdc, RND() % 5 == 0);
            ee_w32(c, race + 0xec, RND() % 5 == 0);
            ee_w8(c, conductor + 0xcbdcu, (uint8_t)((RND() % 4 == 0) ? 1 + RND() % 4 : 0));
            ee_w32(c, cr + 0x104 + 0x618, (RND() % 2) ? 0x157529ffu : RND() % 200000);
            ee_w32(c, e + 0x1a0, RND() % 4 == 0);
            ee_w32(c, e + 0x1a0 + 0xc, (RND() % 2) ? 1000 : RND() % 1000);
            ee_w32(c, e + 0x11e0, (RND() % 2) ? 0x157529ffu : RND() % 200000);
            ee_w32(c, e + 0x1158 + 0x40, (RND() % 2) ? 0x157529ffu : RND() % 200000);
            ee_w32(c, race + 0x928 + 0x40, (RND() % 2) ? 0x157529ffu : RND() % 200000);
            ee_w32(c, 0x01ee8100u, RND() % 6);
            ee_w32(c, 0x01ee8104u, i);
            ee_w32(c, 0x01ee8108u, (RND() % 8 == 0) ? RND() : RND() % 1200);
            ee_w32(c, 0x01ee810cu, (RND() % 8 == 0) ? RND() : RND() % 200000);
            ee_w32(c, 0x01ee8110u, (RND() % 8 == 0) ? RND() % 40 : RND() % 16);
            ee_w32(c, 0x01ee8114u, RND() % 2);
            hull = race;
            base = 0;
            size = EE_RAM_SIZE;
        } else if (!strncmp(spec, "finishline", 10)) {
            /* the Arcade's finish line (method 0xB8, func_00357D00): made-up
               race kind bytes, car, lap and times, the car's flags */
            uint32_t i = RND() % ncars, cr = base + i * CAR_SIZE;
            ee_w8(c, conductor + 0xcbdeu, (uint8_t)(RND() % 3));
            ee_w8(c, conductor + 0xf81au, (uint8_t)(RND() % 3));
            ee_w8(c, conductor + 0xcbdcu, (uint8_t)((RND() % 3 == 0) ? 4 : 0));
            ee_w8(c, cr + 0x104 + 0x441, (uint8_t)(RND() % 6 == 0));
            ee_w8(c, cr + 0x5b7, (uint8_t)(RND() % 2));
            ee_w32(c, 0x01ee8104u, i);
            ee_w32(c, 0x01ee8108u, RND() % 20);
            ee_w32(c, 0x01ee810cu, RND() % 900000);
            ee_w32(c, 0x01ee8110u, RND() % 200000);
            hull = conductor;
            base = 0;
            size = EE_RAM_SIZE;
        } else if (!strncmp(spec, "entryreset", 10) || !strncmp(spec, "records", 7)) {
            /* an entry's records (func_003B4E88) and the race's
               (func_003C0108, not in the recompiled set): made-up
               records, mode 0 or 1 */
            uint32_t race = ee_r32(c, conductor), list = ee_r32(c, race + 0x60);
            uint32_t e = ee_r32(c, ee_r32(c, list + 8) + (uint32_t)(RND() % ee_r32(c, list)) * 4);
            for (uint32_t k = 0; k < 0x1200; k += 4) ee_w32(c, e + k, (RND() % 3) ? RND() : 0);
            for (uint32_t k = 0x900; k < 0x980; k += 4) ee_w32(c, race + k, RND());
            ee_w32(c, 0x01ee8100u, RND() % 2);
            hull = !strncmp(spec, "records", 7) ? race : e;
            base = 0;
            size = EE_RAM_SIZE;
        } else if (!strncmp(spec, "racebefore", 10)) {
            /* the race's countdown (func_003C0208): made-up kind, seconds
               and frames left */
            uint32_t race = ee_r32(c, conductor);
            ee_w32(c, race + 0xb0, RND() % 2);
            ee_w32(c, race + 0xb4, (uint32_t)((int32_t)(RND() % 8) - 2));
            ee_w32(c, race + 0xb8, RND() % 12);
            ee_w32(c, race + 0xbc, RND() % 4);
            hull = race;
            base = 0;
            size = EE_RAM_SIZE;
        } else {
            fprintf(stderr, "unknown case %s\n", spec);
            return 2;
        }
        memcpy(keep, c->ram + base, size);
        if (!strncmp(spec, "drvset", 6)) gt4_orig_call(0x00347a00u, hull, 0, 0, 0);
#ifdef GT4_CONTROLLER_PACE_TEST
        else if (!strncmp(spec,"kind",4)) gt4_orig_call(0x00362e00u,hull+0x104,(uint32_t)arg,0,0);
#endif
        else if (!strncmp(spec, "event", 5)) { gt4_orig_a2 = (uint32_t)arg; gt4_orig_a3 = ee_r32(c, 0x01ee8100u); gt4_orig_call(0x003f19e0u, hull, 0, 0, 0); }
        else if (!strncmp(spec, "model", 5)) gt4_orig_call(0x00465078u, hull, 0, 0, 0);
        else if (!strncmp(spec, "finish", 6)) gt4_orig_call(0x00343d78u, hull, (uint32_t)arg, 0, 0);
        else if (!strncmp(spec, "orders", 6)) gt4_orig_call(0x0035bfe8u, hull, 0x01ee8100u, 0, 0);
        else if (!strncmp(spec, "tyre", 4)) gt4_orig_call(0x00356ef0u, hull, conductor + 4, 0, 0);
        else if (!strncmp(spec, "frame", 5)) gt4_orig_call(0x0035d350u, hull, 0, 0, 0);
        else if (!strncmp(spec, "corner", 6)) gt4_orig_call(0x0045e570u, hull, 0, 0, 0);
        else if (!strncmp(spec, "queue", 5)) {
            uint32_t op = (uint32_t)t % 3;
            gt4_orig_a2 = op == 0 ? 0x12345678u : (t & 4) ? 0x01ee8100u : 0;
            gt4_orig_a3 = op == 0 ? (uint32_t)(t & 3) : op == 1 ? (uint32_t)((t >> 3) & 1) : (uint32_t)(t >> 3) % 3;
            gt4_orig_call(op == 0 ? 0x003b76f0u : op == 1 ? 0x003b7668u : 0x003b77e0u, hull, (uint32_t)arg, 0, 0);
            if (op) ee_w32(c, 0x01ee8200u, gt4_orig_v0);
        }
        else if (!strncmp(spec, "racebefore", 10)) gt4_orig_call(0x003c0208u, hull, 0, 0, 0);
        else if (!strncmp(spec, "entryreset", 10)) gt4_orig_call(0x003b4e88u, hull, ee_r32(c, 0x01ee8100u), 0, 0);
        else if (!strncmp(spec, "records", 7)) gt4_orig_call(0x003c0108u, hull, 0, 0, 0);
        else if (!strncmp(spec, "finishline", 10)) {
            gt4_orig_a2 = ee_r32(c, 0x01ee8108u);
            gt4_orig_t0 = ee_r32(c, 0x01ee810cu);
            gt4_orig_t1 = ee_r32(c, 0x01ee8110u);
            gt4_orig_call(0x003479b8u, hull, ee_r32(c, 0x01ee8104u), 0, 0);
            ee_w32(c, 0x01ee8200u, gt4_orig_v0);
        }
        else if (!strncmp(spec, "racebegin", 9)) gt4_orig_call(0x003c0190u, hull, 0, 0, 0);
        else if (!strncmp(spec, "raceevent", 9)) {
            uint32_t op = ee_r32(c, 0x01ee8100u), i = ee_r32(c, 0x01ee8104u), x = ee_r32(c, 0x01ee8108u);
            uint32_t tm = ee_r32(c, 0x01ee810cu), sp = ee_r32(c, 0x01ee8110u), q = ee_r32(c, 0x01ee8114u);
            if (op == 0) { gt4_orig_a2 = x; gt4_orig_t0 = tm; gt4_orig_t1 = q; gt4_orig_call(0x0035e550u, conductor, i, 0, 0); }
            else if (op == 1) { gt4_orig_a2 = x; gt4_orig_a3 = sp; gt4_orig_t1 = tm; gt4_orig_t2 = q; gt4_orig_call(0x0035e428u, conductor, i, 0, 0); }
            else if (op == 2) { gt4_orig_a2 = sp; gt4_orig_call(0x0035e000u, hull, i, 0, 0); }
            else if (op == 3) { gt4_orig_a2 = x; gt4_orig_call(0x0035e8e8u, conductor, i, 0, 0); }
            else if (op == 4) { gt4_orig_a2 = tm - 100000u; gt4_orig_call(0x0035e078u, hull, i, 0, 0); }
            else { gt4_orig_a2 = i; gt4_orig_a3 = sp; gt4_orig_call(0x0035df80u, hull, x % 0x1c, 0, 0); }
        }
        else if (!strncmp(spec, "raceafter", 9)) gt4_orig_call(0x003c0310u, hull, 0, 0, 0);
        else gt4_orig_call(0x00463cd8u, obj, (uint32_t)arg, 0, 0);
        memcpy(orig, c->ram + base, size);
        moved += memcmp(orig, keep, size) != 0;
        memcpy(c->ram + base, keep, size);
        if (!strncmp(spec, "drvset", 6)) gt4_drive_settings(hull);
#ifdef GT4_CONTROLLER_PACE_TEST
        else if (!strncmp(spec,"kind",4)) fixture_controller_kind(hull,(uint32_t)arg);
#endif
        else if (!strncmp(spec, "event", 5)) gt4_controller_event(hull, arg, ee_r32(c, 0x01ee8100u));
        else if (!strncmp(spec, "model", 5)) gt4_ai_model(hull);
        else if (!strncmp(spec, "finish", 6)) gt4_car_finish_drive(hull, arg);
        else if (!strncmp(spec, "orders", 6)) gt4_pit_orders(hull, 0x01ee8100u);
        else if (!strncmp(spec, "tyre", 4)) gt4_tyre_condition(hull, conductor + 4);
        else if (!strncmp(spec, "frame", 5)) gt4_car_ground_frame(hull);
        else if (!strncmp(spec, "corner", 6)) gt4_walls_corner_hit(hull);
        else if (!strncmp(spec, "queue", 5)) {
            uint32_t op = (uint32_t)t % 3, v = 0, out = (t & 4) ? 0x01ee8100u : 0;
            int r;
            if (op == 0) gt4_race_queue_push(hull, arg, 0x12345678u, t & 3);
            else {
                r = op == 1 ? gt4_race_queue_take(hull, arg, out ? &v : NULL, (t >> 3) & 1)
                            : gt4_race_queue_peek(hull, arg, out ? &v : NULL, (t >> 3) % 3);
                if (r && out) ee_w32(c, out, v);
                ee_w32(c, 0x01ee8200u, (uint32_t)r);
            }
        }
        else if (!strncmp(spec, "racebefore", 10)) gt4_race_before_step(hull);
        else if (!strncmp(spec, "entryreset", 10)) gt4_race_entry_reset(hull, ee_r32(c, 0x01ee8100u));
        else if (!strncmp(spec, "records", 7)) gt4_race_records_start(hull);
        else if (!strncmp(spec, "finishline", 10))
            ee_w32(c, 0x01ee8200u, gt4_session(hull, 0xb8, ee_r32(c, 0x01ee8104u), ee_r32(c, 0x01ee8108u), 0,
                                               ee_r32(c, 0x01ee810cu), ee_r32(c, 0x01ee8110u), NULL));
        else if (!strncmp(spec, "racebegin", 9)) gt4_race_begin(hull);
        else if (!strncmp(spec, "raceevent", 9)) {
            uint32_t op = ee_r32(c, 0x01ee8100u), i = ee_r32(c, 0x01ee8104u), x = ee_r32(c, 0x01ee8108u);
            uint32_t tm = ee_r32(c, 0x01ee810cu), sp = ee_r32(c, 0x01ee8110u), q = ee_r32(c, 0x01ee8114u);
            if (op == 0) gt4_race_lap_event(conductor, i, x, tm, (int)q);
            else if (op == 1) gt4_race_split_event(conductor, i, x, sp, tm, (int)q);
            else if (op == 2) gt4_race_pit_message(hull, i, sp);
            else if (op == 3) gt4_race_behind(conductor, i, x);
            else if (op == 4) gt4_race_gap(hull, i, tm - 100000u);
            else gt4_race_event(hull, (int32_t)(x % 0x1c), i, (int32_t)sp);
        }
        else if (!strncmp(spec, "raceafter", 9)) gt4_race_after_step(hull);
        else gt4_ai_pit_throttle(obj, arg);
#ifdef GT4_CONTROLLER_PACE_TEST
        if(!t && getenv("GT4_CONTROLLER_PACE_NEGATIVE")) c->ram[car+0x14+0x70]^=1;
#endif
        if (base == 0) {   /* the whole RAM but the original's stack */
            memcpy(orig + 0x01ee0000u, c->ram + 0x01ee0000u, 0x10000u);
        }
        if (memcmp(orig, c->ram + base, size)) {
            int shown = 0;
            for (uint32_t k = 0; k < size; k += 4)
                if (memcmp(orig + k, c->ram + base + k, 4)) {
                    if (bad < 3 && shown < 8) {
                        float fo, fn;
                        memcpy(&fo, orig + k, 4);
                        memcpy(&fn, c->ram + base + k, 4);
                        printf("trial %d: +0x%x differs (%g / %g)\n", t, base ? k % CAR_SIZE : k, fo, fn);
                    }
                    if (++shown >= 8 || bad >= 3) break;
                }
            bad++;
        }
    }
#undef RND
    printf("%s: %d trials (%d changed the cars), %d differ\n", spec, trials, moved, bad);
    free(keep);
    free(orig);
    return bad != 0;
}

/* PHYS_PAD_HOST=1: the pad answered through the host hooks, which here
   hand each part back to the controller object (to check the plumbing
   against the plain run). */
static ee_ctx *pad_ctx;
#define pad_ram (pad_ctx->ram)
static uint32_t guest(const void *p) { return (uint32_t)((const uint8_t *)p - pad_ram); }
static uint32_t rd32(uint32_t a) { uint32_t v; memcpy(&v, pad_ram + a, 4); return v; }
static void pad_call(uint32_t car, uint32_t off, uint32_t a1, uint32_t a2)
{
    uint32_t obj = rd32(car + 0xc), e = rd32(obj + 0xd0) + off;
    int16_t adj;
    memcpy(&adj, pad_ram + e, 2);
    gt4_orig_a2 = a2;
    gt4_orig_call(rd32(e + 4), obj + (uint32_t)(int32_t)adj, a1, 0, 0);
}
static int host_read(void *ctx, uint32_t car, uint8_t *cmd, const uint8_t *ev)
{
    (void)ctx;
    pad_call(car, 0x28, guest(cmd), guest(ev));
    return 1;
}
static int host_state(void *ctx, uint32_t car, int16_t state[2])
{
    uint32_t buf = 0x01ee0780u;
    (void)ctx;
    memcpy(pad_ram + buf, state, 4);
    pad_call(car, 0x20, buf, rd32(car + 0xc));
    memcpy(state, pad_ram + buf, 4);
    return 1;
}
static int host_write(void *ctx, uint32_t car, const uint8_t *out)
{
    (void)ctx;
    if (!gt4_phys_headless) pad_call(car, 0x30, guest(out), 0);
    return 1;
}

int main(int argc, char **argv)
{
#ifdef _MSC_VER
    _set_invalid_parameter_handler(invalid_parameter);
#endif
    static ee_ctx c;
    jmp_buf exit_jmp;
    char path[512];
    uint32_t conductor, ncars;
    int frames, frame;
    FILE *cars, *f;

    gt4_phys_specdb = &phys_memory_src;   /* the pit service's tyre change */
    gt4_phys_scratch = 0x01ee0000u;       /* below the stack, never reached */
    if (getenv("PHYS_PAD_HOST")) {
        pad_ctx = &c;
        gt4_phys_pad.read = host_read;
        gt4_phys_pad.wheel_state = host_state;
        gt4_phys_pad.write = host_write;
    }
    if (argc < 6) {
        fprintf(stderr, "usage: %s <eeMemory.bin> <Scratchpad.bin|-> <conductor> <frames> <out_dir>\n", argv[0]);
        return 2;
    }
    conductor = (uint32_t)strtoul(argv[3], NULL, 0);
    if(getenv("PHYS_SERIES_INPUT")){
        series_input_car=conductor+CAR_OFFSET;phys_extra_hook=series_input_hook;
        gt4_phys_pad.read=series_read;gt4_phys_pad.wheel_state=series_state;gt4_phys_pad.write=series_write;
    }
    frames = atoi(argv[4]);
    if (ee_init(&c) != 0) return 2;
    if (!strcmp(argv[1], "-") && getenv("PHYS_BUILD")) {
        /* PHYS_BUILD=<core.elf>: no snapshot, the race built from the
           executable's image (tests/physics/race_build.c) */
        char msg[256];
        uint32_t built;
        /* PHYS_ARCADE=n: a race against the AI with n cars */
        if (!(getenv("PHYS_ARCADE") ? phys_race_build_arcade(c.ram, getenv("PHYS_BUILD"), atoi(getenv("PHYS_ARCADE")), &built, msg, sizeof msg)
                                    : phys_race_build(c.ram, getenv("PHYS_BUILD"), &built, msg, sizeof msg))) {
            fprintf(stderr, "race build: %s\n", msg);
            return 2;
        }
    } else if (read_file(argv[1], c.ram, EE_RAM_SIZE) != (long)EE_RAM_SIZE) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    if (strcmp(argv[2], "-") && read_file(argv[2], c.spr, EE_SPR_SIZE) != (long)EE_SPR_SIZE) {
        fprintf(stderr, "cannot read %s\n", argv[2]);
        return 2;
    }
    ncars = ee_r32(&c, conductor + NCARS_OFF);
    if (ncars == 0 || ncars > 8) {
        fprintf(stderr, "no cars at conductor 0x%08x (%u)\n", conductor, ncars);
        return 2;
    }
    snprintf(path, sizeof path, "%s/trace.jsonl", argv[5]);
    c.trace = fopen(path, "w");
    snprintf(path, sizeof path, "%s/cars.bin", argv[5]);
    cars = fopen(path, "wb");
    if (!c.trace || !cars) {
        fprintf(stderr, "cannot write to %s\n", argv[5]);
        return 2;
    }
    c.lookup = recomp_lookup;
    c.code_ranges = recomp_code_ranges;
    c.n_code_ranges = recomp_n_code_ranges;
    c.cop0[EE_COP0_STATUS] = 0x70030c13u;   /* TEST INPUT, as in main_run */
    c.exit_jmp = &exit_jmp;

    phys_natives_select(argc > 6 ? argv[6] : NULL);
    if (getenv("OPENGT4_COVERAGE")) phys_coverage = calloc(0x00520000u / 4, sizeof *phys_coverage);
    if (getenv("PHYS_INPUT_SEED")) {
        input_state = (uint32_t)strtoul(getenv("PHYS_INPUT_SEED"), NULL, 0);
        phys_extra_hook = input_hook;
    }
    phys_natives_install(&c);

    if (getenv("OPENGT4_MATHCHECK")) {
        /* Original vs rewritten float functions over a sweep of inputs. */
        static const struct { uint32_t addr; float (*fn)(float); const char *name; } fns[] = {
            { 0x0057d1f0u, gt4_sinf, "sin" }, { 0x0057d2b8u, gt4_cosf, "cos" },
            { 0x0057d008u, gt4_atanf, "atan" }, { 0x0036cb90u, gt4_angle_wrap, "wrap" },
            { 0x0057d380u, gt4_tanf, "tan" },
        };
        c.hook = NULL;
        c.r[29].uw[0] = STACK_TOP;
        for (int k = 0; k < 5; k++) {
            long bad = 0;
            ee_fn orig = recomp_lookup(fns[k].addr);
            for (int j = -2000000; j <= 2000000; j++) {
                float x = (float)j * 1e-5f, a, b;
                c.fpr[12].f = x;
                orig(&c);
                a = c.fpr[0].f;
                b = fns[k].fn(x);
                if (memcmp(&a, &b, 4) && bad++ < 5)
                    printf("%s(%.9g = %08x): orig %.9g (%08x) new %.9g (%08x)\n", fns[k].name, x,
                           *(uint32_t *)&x, a, *(uint32_t *)&a, b, *(uint32_t *)&b);
            }
            printf("%s: %ld differences\n", fns[k].name, bad);
        }
        return 0;
    }

    if (getenv("PHYS_DISC")) {
        /* PHYS_DISC=<disc image> [PHYS_COURSE=name] [PHYS_CAR=id]: set the
           race up again first (tests/physics/race_setup.c), as the drive
           demo does */
        const char *err = NULL;
        char msg[256];
        gt4_vol *vol = gt4_vol_open(getenv("PHYS_DISC"), &err);
        if (!vol) {
            fprintf(stderr, "disc: %s\n", err ? err : "cannot open");
            return 2;
        }
        static gt4_specdb_src disc;
        disc.row = gt4_spec_tables_row;
        disc.ctx = gt4_spec_tables_open(vol, NULL);
        disc.str = gt4_spec_tables_str;
        disc.car_parts = gt4_spec_tables_car_parts;
        phys_record_src = &disc;   /* the SpecDB rows from the disc, not the game's memory */
        if (getenv("PHYS_CARS")) {   /* PHYS_CARS=id,id,...: the other entries' cars from the disc too */
            const char *p = getenv("PHYS_CARS");
            for (int k = 0; k < 7 && p && *p; k++) {
                phys_race_other_cars[k] = (int32_t)strtol(p, NULL, 0);
                p = strchr(p, ',');
                if (p) p++;
            }
        }
        if (!phys_race_setup(&c, vol, conductor, getenv("PHYS_COURSE"),
                             getenv("PHYS_CAR") ? (int32_t)strtol(getenv("PHYS_CAR"), NULL, 0) : -1, msg, sizeof msg)) {
            fprintf(stderr, "race setup: %s\n", msg);
            phys_print_recent();
            return 1;
        }
        if (msg[0]) printf("race setup: %s\n", msg);
        c.exit_jmp = &exit_jmp;
    }

    for (uint32_t which = 0; which < 3 + ncars; which++) {
        /* PHYS_POKE=off=val[,off=val]: change the player car's parameters,
           as a tuning part would (off from the parameters, a byte, or a
           float when val has a point); PARAMS_DRIVE 0x234 is the drivetrain.
           PHYS_POKE_CAR: the same from the car itself (0x546 the driver
           kind: automatic, manual...); PHYS_POKE_CARS: from every car, or
           from its parameters with a leading p; PHYS_POKE_COND: from the
           conductor (race settings) */
        const char *list = getenv(which == 0 ? "PHYS_POKE" : which == 1 ? "PHYS_POKE_CAR"
                                  : which == 2 ? "PHYS_POKE_COND" : "PHYS_POKE_CARS");
        uint32_t car = conductor + CAR_OFFSET + (which < 3 ? 0 : which - 3) * CAR_SIZE;
        uint32_t params = which == 0 ? ee_r32(&c, car + 0x10u) : which == 2 ? conductor : car;
        for (const char *p = list; p && *p; p = strchr(p, ',') ? strchr(p, ',') + 1 : NULL) {
            char *e;
            uint32_t off;
            if (*p == 'p') { params = ee_r32(&c, car + 0x10u); p++; }
            else if (which >= 3) params = car;
            off = (uint32_t)strtoul(p, &e, 0);
            const char *v = *e == '=' ? e + 1 : e;
            if (params == 0 || params + off + 4 > EE_RAM_SIZE) break;
            if (strcspn(v, ",.") < strcspn(v, ",")) {
                float x = strtof(v, NULL);
                memcpy(c.ram + params + off, &x, 4);
            } else {
                c.ram[params + off] = (uint8_t)strtoul(v, NULL, 0);
            }
        }
    }

    if (getenv("PHYS_PAD_FIXED")) {   /* the made-up driver of pad_fixed.h for the player's car */
        pad_fixed_car = conductor + CAR_OFFSET;
        gt4_phys_pad.read = pad_fixed_read;
        gt4_phys_pad.wheel_state = pad_fixed_wheel;
        gt4_phys_pad.write = pad_fixed_write;
    }
    if (getenv("PHYS_BEGIN")) {
        /* PHYS_BEGIN=1: the race's start (func_003C0190), its countdown's
           kind -1 with the course's rolling start (as the world does) */
        uint32_t race = ee_r32(&c, conductor);
        jmp_buf begin_jmp;
        c.exit_jmp = &begin_jmp;
        if (setjmp(begin_jmp) != 0) {
            fprintf(stderr, "race start ended at 0x%08x: %s\n", c.end_pc, c.end_detail);
            return 1;
        }
        memset(c.r, 0, sizeof c.r);
        c.r[4].uw[0] = race;
        c.r[29].uw[0] = STACK_TOP;
        gt4_race_records_start(race);
        recomp_lookup(0x003c0190u)(&c);
        if (ee_r8(&c, conductor + 0xcbe4u)) ee_w32(&c, race + 0xb4, 0xffffffffu);
        gt4_race_entries_begin(race, conductor);
        c.exit_jmp = &exit_jmp;
    }
    if (getenv("PHYS_AB")) return ab_test(&c, conductor, ncars, getenv("PHYS_AB"));

    ee_fn step = recomp_lookup(STEP);
    if (!step) {
        fprintf(stderr, "step 0x%08x was not recompiled\n", STEP);
        return 2;
    }
    if(!state_write(cars,c.ram + conductor + CAR_OFFSET,CAR_SIZE * ncars))return 2;
    for (frame = 1; frame <= frames; frame++) {
        if (getenv("PHYS_PROGRESS")) { fprintf(stderr, "frame %d\n", frame); fflush(stderr); }
        memset(c.r, 0, sizeof c.r);
        c.r[4].uw[0] = conductor;   /* a0: the conductor */
        c.r[5].uw[0] = 0;           /* a1: 0, as the per-frame caller 0x389b08 */
        c.r[29].uw[0] = STACK_TOP;
        c.r[31].uw[0] = RETURN_PC;
        if (setjmp(exit_jmp) != 0) {
            fprintf(stderr, "frame %d: run ended at 0x%08x: %s\n", frame, c.end_pc, c.end_detail);
            phys_print_recent();
            fclose(cars);
            fclose(c.trace);
            return 1;
        }
        if (getenv("PHYS_FRAME")) {
            /* PHYS_FRAME=1: the race object's parts around the step too
               (func_003C0208 before, func_003C0310 after), as the world's
               frame (func_00389B08) has them */
            uint32_t race = ee_r32(&c, conductor);
            if(getenv("PHYS_SERIES_INPUT")){
                /* 00389758 ->003B75D8, the production caller's event age.
                   Use original instructions on both sides of this oracle. */
                c.r[4].uw[0]=race+0xf4;recomp_lookup(0x003b75d8u)(&c);
            }
            c.r[4].uw[0] = race;
            recomp_lookup(0x003c0208u)(&c);
            memset(c.r, 0, sizeof c.r);
            c.r[4].uw[0] = conductor;
            c.r[29].uw[0] = STACK_TOP;
            c.r[31].uw[0] = RETURN_PC;
            step(&c);
            memset(c.r, 0, sizeof c.r);
            c.r[4].uw[0] = race;
            c.r[29].uw[0] = STACK_TOP;
            c.r[31].uw[0] = RETURN_PC;
            recomp_lookup(0x003c0310u)(&c);
        } else {
            step(&c);
        }
        if(!state_write(cars,c.ram + conductor + CAR_OFFSET,CAR_SIZE * ncars))return 2;
    }
    fclose(cars);
    fclose(c.trace);
    snprintf(path, sizeof path, "%s/ram.bin", argv[5]);
    f = fopen(path, "wb");
    if (f) {
        fwrite(c.ram, 1, EE_RAM_SIZE, f);
        fclose(f);
    }
    if (phys_coverage && (f = fopen(getenv("OPENGT4_COVERAGE"), "w")) != NULL) {
        for (uint32_t k = 0; k < 0x00520000u / 4; k++)
            if (phys_coverage[k]) fprintf(f, "0x%06x %u\n", 0x00100000u + k * 4, phys_coverage[k]);
        fclose(f);
    }
    printf("%d frames, %u cars\n", frames, ncars);
    phys_natives_report();
    return 0;
}
