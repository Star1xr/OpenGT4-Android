/*
 * Runs the original car setup (recompiled from the user's own CORE.GT4) on a
 * RAM snapshot taken during a race: func_003F3B68 builds the car's
 * intermediate record from its garage entry (SpecDB rows), func_00361908
 * builds the physics parameters the step reads (car+0x10). The results are
 * the oracle for the rewrite in src/core/physics (docs/analysis/PHYSICS.md).
 *
 * Usage: setup_run <eeMemory.bin> <conductor> <out_dir> [native]
 *
 * Arguments as in the race init (func_0034D490 -> func_00363B10) for car 0.
 * Output in out_dir:
 *   record.bin   the 0x600-byte intermediate record
 *   params.bin   the parameters, built in place (SETUP_ZERO=1: cleared first)
 *   snap.bin     the snapshot's parameters, for comparison
 *   ram.bin      guest RAM afterwards
 */
#include "ee.h"
#include "phys.h"
#include "natives.h"
#include "spectables.h"
#include "specdb.h"
#include "course.h"

#include <stdlib.h>
#include <string.h>

ee_fn recomp_lookup(uint32_t addr);
extern const ee_code_range recomp_code_ranges[];
extern const int recomp_n_code_ranges;

#define CAR_OFFSET   0x6698u
#define PARAMS_OFF   0x98u
#define PARAMS_SIZE  0x1100u
#define RECORD_SIZE  0x600u
/* TEST INPUT: stack and record in RAM that is zero in the
   snapshots (the physics_run stack). */
#define STACK_TOP    0x01ef0000u
#define RECORD_AT    0x01e20000u

/* SETUP_WATCH=offset: says which function was entered after the parameter
   word at that offset changed (the writer ran just before). */
static ee_ctx *watch_ctx;
static uint32_t watch_addr, watch_last, watch_prev;
static ee_fn watch_hook(uint32_t addr)
{
    uint32_t v = ee_r32(watch_ctx, watch_addr);
    if (v != watch_last) {
        printf("watch 0x%08x: %08x -> %08x before 0x%08x (previous entry 0x%08x)\n",
               watch_addr, watch_last, v, addr, watch_prev);
        watch_last = v;
    }
    watch_prev = addr;
    return NULL;
}

/* SETUP_SEED=n: the record's fields that choose paths get made-up values
   (plausible ranges, the same in every run with that seed), and the mode
   argument 0, 1 or 2. */
static uint32_t fuzz_state, fuzz_mode;
static uint32_t fuzz_rand(void)
{
    fuzz_state = fuzz_state * 1103515245u + 12345u;
    return fuzz_state >> 8;
}
static void fuzz_record(ee_ctx *c, uint32_t rec, uint32_t race, uint32_t seed)
{
    static const struct { uint16_t off; uint8_t size; int lo, hi; } f[] = {
        { 0x001, 1, 0, 7 }, { 0x002, 1, 0, 2 }, { 0x003, 1, 0, 4 }, { 0x005, 1, 0, 200 },
        { 0x006, 1, -50, 50 }, { 0x008, 1, 0, 150 }, { 0x020, 2, 0, 2000 }, { 0x022, 1, 30, 70 },
        { 0x023, 1, 0, 3 }, { 0x024, 1, 0, 255 }, { 0x025, 1, 0, 255 }, { 0x026, 1, 0, 255 },
        { 0x027, 1, 0, 255 }, { 0x028, 1, 0, 60 }, { 0x029, 1, 0, 60 }, { 0x02a, 1, 0, 255 },
        { 0x02b, 1, 0, 255 }, { 0x02c, 1, 0, 255 }, { 0x02d, 1, 0, 255 }, { 0x030, 2, 0, 2500 },
        { 0x032, 2, 0, 2500 }, { 0x038, 1, 0, 255 }, { 0x039, 1, 0, 255 }, { 0x03a, 2, 0, 1500 },
        { 0x040, 2, 0, 20000 }, { 0x042, 1, 0, 255 }, { 0x043, 1, 0, 255 }, { 0x044, 1, 0, 255 },
        { 0x04c, 1, 0, 8 }, { 0x04d, 1, 0, 255 }, { 0x04e, 1, 0, 255 }, { 0x04f, 1, 0, 255 },
        { 0x050, 1, 0, 255 }, { 0x088, 1, 0, 255 }, { 0x089, 1, 0, 255 }, { 0x08a, 1, 0, 255 },
        { 0x08b, 1, 0, 255 }, { 0x08e, 1, 0, 255 }, { 0x08f, 1, 0, 255 }, { 0x090, 1, -100, 100 },
        { 0x091, 1, -100, 100 }, { 0x096, 1, -30, 30 }, { 0x097, 1, -30, 30 }, { 0x098, 1, 0, 255 },
        { 0x099, 1, 0, 255 }, { 0x09a, 2, 0, 800 }, { 0x09c, 2, 0, 800 }, { 0x110, 1, 0, 120 },
        { 0x111, 1, 0, 12 }, { 0x112, 1, 0, 255 }, { 0x11c, 1, 0, 255 }, { 0x11d, 1, 0, 255 },
        { 0x11e, 2, -2000, 2000 }, { 0x128, 1, 0, 6 }, { 0x129, 1, 0, 6 }, { 0x178, 1, 1, 255 },
        { 0x179, 1, 0, 255 }, { 0x17a, 1, 0, 255 }, { 0x1a8, 1, 0, 255 }, { 0x1a9, 1, 0, 255 },
        { 0x1aa, 1, 0, 255 }, { 0x1ab, 1, 0, 255 }, { 0x1b8, 1, 0, 255 }, { 0x1ba, 1, 0, 255 },
        { 0x1bb, 1, 0, 255 }, { 0x1bc, 1, 0, 255 }, { 0x1bd, 1, 0, 255 }, { 0x1be, 1, 0, 255 },
        { 0x1bf, 1, 0, 255 }, { 0x1c0, 1, 0, 255 }, { 0x1c1, 1, 0, 255 }, { 0x1c2, 1, 0, 255 },
        { 0x1c3, 1, 0, 110 }, { 0x1e4, 1, 0, 110 }, { 0x1e5, 1, 0, 110 }, { 0x210, 1, 0, 255 },
        { 0x007, 1, 0, 150 }, { 0x0ba, 2, 0, 500 }, { 0x0bc, 1, 0, 255 }, { 0x0bd, 1, 0, 100 },
        { 0x0be, 1, 0, 80 }, { 0x108, 2, 0, 2000 }, { 0x10a, 2, 0, 2000 }, { 0x10c, 1, 0, 200 },
        { 0x113, 1, 0, 30 }, { 0x114, 1, 0, 4 }, { 0x115, 1, 0, 4 }, { 0x116, 1, 0, 80 },
        { 0x117, 1, 0, 80 }, { 0x118, 1, 0, 30 }, { 0x119, 1, 0, 30 }, { 0x11a, 1, 0, 60 },
        { 0x11b, 1, 0, 60 },
        { 0x12a, 1, 0, 100 }, { 0x12b, 1, 0, 30 }, { 0x150, 2, 500, 6000 }, { 0x152, 2, 0, 1500 },
        { 0x154, 1, 0, 5 }, { 0x155, 1, 1, 11 }, { 0x156, 1, 0, 1 }, { 0x157, 1, 0, 40 },
        { 0x158, 1, 0, 6 }, { 0x13a, 2, 0, 5000 }, { 0x13c, 2, 0, 4000 }, { 0x140, 2, 0, 3000 },
        { 0x144, 2, 0, 2000 }, { 0x148, 2, 0, 1500 }, { 0x138, 2, 0, 4000 }, { 0x160, 1, 0, 255 },
        { 0x162, 1, 0, 255 }, { 0x164, 2, 1, 3000 }, { 0x166, 2, 1, 3000 }, { 0x16a, 1, 0, 255 },
        { 0x16b, 1, 0, 255 }, { 0x16c, 1, 0, 255 }, { 0x16d, 1, 0, 255 }, { 0x16e, 1, 0, 255 },
        { 0x16f, 1, 0, 255 }, { 0x174, 2, 0, 500 }, { 0x176, 2, 0, 500 }, { 0x0b8, 2, 0, 2000 },
        { 0x1c8, 1, 20, 80 }, { 0x1c9, 1, 20, 80 }, { 0x1ca, 1, 12, 200 }, { 0x1cb, 1, 12, 200 },
        { 0x1cc, 1, 20, 80 }, { 0x1cd, 1, 20, 80 }, { 0x1e2, 1, -20, 20 }, { 0x1e3, 1, -20, 20 },
        { 0x210, 1, 0, 200 }, { 0x211, 1, 0, 255 }, { 0x212, 1, 0, 120 }, { 0x214, 1, 0, 1 },
        { 0x219, 1, 1, 6 }, { 0x21a, 1, 1, 6 }, { 0x21b, 1, 0, 4 }, { 0x21c, 1, 0, 6 },
        { 0x274, 1, 0, 255 }, { 0x280, 1, 0, 255 }, { 0x2b8, 1, 0, 200 }, { 0x2b9, 1, 0, 255 },
        { 0x2ba, 1, 0, 120 }, { 0x2bc, 1, 0, 1 }, { 0x2c1, 1, 1, 6 }, { 0x2c2, 1, 1, 6 },
        { 0x2c3, 1, 0, 4 }, { 0x2c4, 1, 0, 6 }, { 0x31c, 1, 0, 255 }, { 0x328, 1, 0, 255 },
        { 0x360, 1, 0, 200 }, { 0x361, 1, 0, 255 }, { 0x362, 1, 0, 120 }, { 0x364, 1, 0, 1 },
        { 0x369, 1, 1, 6 }, { 0x36a, 1, 1, 6 }, { 0x36b, 1, 0, 4 }, { 0x36c, 1, 0, 6 },
        { 0x3c4, 1, 0, 255 }, { 0x3d0, 1, 0, 255 }, { 0x1e0, 1, 0, 1 }, { 0x1f0, 1, 0, 4 },
        { 0x1f5, 1, 0, 4 }, { 0x1fb, 1, 0, 4 }, { 0x201, 1, 0, 4 }, { 0x207, 1, 0, 4 },
    };
    fuzz_state = seed;
    fuzz_mode = fuzz_rand() % 3;
    for (size_t k = 0; k < sizeof f / sizeof f[0]; k++) {
        int v;
        if (fuzz_rand() % 3 == 0) continue;                  /* keep the car's own value */
        v = f[k].lo + (int)(fuzz_rand() % (uint32_t)(f[k].hi - f[k].lo + 1));
        if (fuzz_rand() % 4 == 0) v = f[k].lo < 0 ? 0 : f[k].lo;   /* zero and "unset" cases */
        if (f[k].size == 1) ee_w8(c, rec + f[k].off, (uint8_t)v);
        else { ee_w8(c, rec + f[k].off, (uint8_t)v); ee_w8(c, rec + f[k].off + 1, (uint8_t)(v >> 8)); }
    }
    /* differential letters */
    for (uint32_t o = 0x168; o < 0x16a; o++)
        if (fuzz_rand() % 3 == 0) ee_w8(c, rec + o, (uint8_t)"frnmhvyt?"[fuzz_rand() % 9]);
    /* layouts with their own geometry: only table 2 has a wheelbase */
    if (ee_r8(c, rec + 1) == 6 || ee_r8(c, rec + 2) == 1) ee_w8(c, rec + 0x23, 2);
    /* springs and dampers (the motion ratio never 0) */
    for (uint32_t o = 0x58; o < 0x80; o++)
        if (fuzz_rand() % 3 == 0) ee_w8(c, rec + o, (uint8_t)(fuzz_rand() % 4 == 0 ? 0 : fuzz_rand()));
    for (uint32_t o = 0x92; o < 0x96; o++)
        if (fuzz_rand() % 3 == 0) ee_w8(c, rec + o, (uint8_t)fuzz_rand());
    for (uint32_t o = 0x8c; o < 0x8e; o++)
        if (fuzz_rand() % 3 == 0) ee_w8(c, rec + o, (uint8_t)(1 + fuzz_rand() % 200));
    /* the race's power limit (units of 10 PS) */
    if (fuzz_rand() % 3 == 0) ee_w8(c, race + 3, (uint8_t)(fuzz_rand() % 2 ? 0 : 5 + fuzz_rand() % 50));
    /* game options (D_00620CD8) the setup reads */
    {
        static const uint16_t opt[] = { 0x003, 0x004, 0x007, 0x008, 0x00a, 0x13c, 0x0bc, 0x0bd, 0x101, 0x102, 0x105, 0x106,
                                        0x107, 0x108, 0x109, 0x10a, 0x10b, 0x10c, 0x112, 0x118, 0x119 };
        for (size_t k = 0; k < sizeof opt / sizeof opt[0]; k++)
            if (fuzz_rand() % 3 == 0) ee_w8(c, 0x00620cd8u + opt[k], (uint8_t)(fuzz_rand() % 3 == 0 ? 0 : fuzz_rand() % 200));
    }
}

/* SETUP_GARAGE=n: the garage entry gets other parts (random rows of each
   part's table, in the game's memory) and tuning settings, and the mode 0,
   2 or 3; the same in every run with that seed. */
static uint32_t random_row(ee_ctx *c, uint32_t table)
{
    uint32_t obj = ee_r32(c, 0x006235a8u + 0x134u + table * 4), blob, count;
    if (!obj) return 0xffffffffu;
    blob = ee_r32(c, obj);
    count = ee_r32(c, blob + 8);
    if (!count) return 0xffffffffu;
    return ee_r32(c, blob + 16 + (fuzz_rand() % count) * 8);
}
static void fuzz_garage(ee_ctx *c, uint32_t gar, uint32_t seed, uint32_t *mode)
{
    /* key slot, table (optional slots: the guessed part) */
    static const struct { uint16_t off; uint8_t table, optional; } k[] = {
        { 0x20, 1, 0 }, { 0x28, 2, 0 }, { 0x30, 6, 0 }, { 0x38, 0xc, 0 }, { 0x40, 0xa, 0 },
        { 0x48, 0xb, 0 }, { 0x50, 3, 0 }, { 0x58, 0x18, 0 }, { 0x60, 0x19, 0 }, { 0x68, 0x1a, 0 },
        { 0x70, 9, 0 }, { 0x78, 8, 1 }, { 0x80, 7, 0 }, { 0x88, 0xd, 1 }, { 0x90, 0xf, 1 },
        { 0x98, 0x10, 1 }, { 0xa0, 0x11, 1 }, { 0xa8, 0x12, 1 }, { 0xb0, 0xe, 0 }, { 0xb8, 0x16, 1 },
        { 0xc0, 0x15, 1 }, { 0xc8, 0x17, 1 }, { 0xd0, 0x13, 1 }, { 0xd8, 0x14, 1 }, { 0xe0, 4, 0 },
        { 0xe8, 5, 0 }, { 0xf8, 0x1b, 1 }, { 0x108, 0x1c, 1 },
    };
    fuzz_state = seed * 7919u + 17u;
    *mode = (uint32_t[3]){ 0, 2, 3 }[fuzz_rand() % 3];
    for (size_t i = 0; i < sizeof k / sizeof k[0]; i++) {
        uint32_t id, table = k[i].table;
        if (fuzz_rand() % 2) continue;
        if (k[i].optional && fuzz_rand() % 3 == 0) { id = 0xffffffffu; table = 0xffffffffu; }
        else id = random_row(c, table);
        if (id == 0xffffffffu) table = 0xffffffffu;
        ee_w32(c, gar + k[i].off, id);
        ee_w32(c, gar + k[i].off + 4, table);
    }
    for (uint32_t o = 0x12f; o <= 0x168; o++) {
        if (o >= 0x110 && o < 0x12c) continue;
        if (fuzz_rand() % 3 == 0) ee_w8(c, gar + o, (uint8_t)(fuzz_rand() % 6));
    }
    if (fuzz_rand() % 3 == 0) ee_w8(c, gar + 0x18, (uint8_t)(fuzz_rand() % 5));
}

/* SETUP_COURSES: every course of the disc with a .ad file: its runway and
   lines loaded into free memory and hooked to the snapshot's course
   object; runway relocation and line setup, original against rewrite. */
struct course_list { char name[160][40]; int n; };
static void list_course(void *user, const char *name, int is_dir, uint32_t size)
{
    struct course_list *l = user;
    size_t k = strlen(name);
    (void)size;
    if (is_dir || k < 4 || strcmp(name + k - 3, ".ad") || l->n >= 160) return;
    memcpy(l->name[l->n], name, k - 3);
    l->name[l->n][k - 3] = 0;
    l->n++;
}
static int line_setup_run(ee_ctx *c, uint32_t lines, uint32_t course, int native)
{
    jmp_buf exit_jmp;
    phys_natives_select(native ? "0x46b0a8,0x46b9f0" : NULL);
    memset(c->ram + lines, 0, 0x10);
    c->exit_jmp = &exit_jmp;
    if (setjmp(exit_jmp) != 0) { printf("  stopped at 0x%08x: %s\n", c->end_pc, c->end_detail); return 1; }
    for (int k = 0; k < 2; k++) {
        ee_fn fn = recomp_lookup(k ? 0x0046b9f0u : 0x0046b0a8u);
        memset(c->r, 0, sizeof c->r);
        c->r[4].uw[0] = lines;
        c->r[5].uw[0] = course;
        c->r[29].uw[0] = STACK_TOP;
        fn(c);
    }
    return 0;
}
static int all_courses(ee_ctx *c, gt4_vol *vol, uint32_t course)
{
    static struct course_list l;
    const uint32_t ad = 0x01e30000u, lines = 0x01e2f000u;
    uint32_t rw;
    uint32_t obj = ee_r32(c, course + 4), keep_rw = ee_r32(c, obj + 0xdc), keep_ad = ee_r32(c, course + 0x9c);
    int bad = 0;
    gt4_vol_list(vol, "/crs", list_course, &l);
    for (int i = 0; i < l.n; i++) {
        char path[128];
        size_t n, an;
        uint8_t *crs, *adf, *keep;
        gt4_course co;
        uint32_t size, adln;
        snprintf(path, sizeof path, "/crs/%s", l.name[i]);
        crs = gt4_vol_read(vol, path, &n);
        snprintf(path, sizeof path, "/crs/%s.ad", l.name[i]);
        adf = gt4_vol_read(vol, path, &an);
        if (getenv("SETUP_ONLY") && strcmp(getenv("SETUP_ONLY"), l.name[i])) { free(crs); free(adf); continue; }
        if (!crs || !adf || !gt4_course_open(&co, crs, n)) { printf("%-20s cannot read\n", l.name[i]); free(crs); free(adf); continue; }
        memcpy(&size, crs + co.runway + 8, 4);
        memcpy(&adln, adf + 4, 4);
        /* big runways go where the snapshot's is, over what follows it */
        rw = size <= 0xb0000 ? 0x01e40000u : keep_rw;
        if (an > 0x10000) { printf("%-20s too big (runway 0x%x, ad 0x%zx)\n", l.name[i], size, an); free(crs); free(adf); continue; }
        memcpy(c->ram + rw, crs + co.runway, size);
        {
            /* the original relocation first, kept to compare */
            jmp_buf exit_jmp;
            uint8_t *orig = malloc(size);
            int d = 0;
            memset(c->r, 0, sizeof c->r);
            c->r[4].uw[0] = rw;
            c->r[29].uw[0] = STACK_TOP;
            c->exit_jmp = &exit_jmp;
            phys_natives_select(NULL);
            if (setjmp(exit_jmp) == 0) recomp_lookup(0x003e5818u)(c);
            memcpy(orig, c->ram + rw, size);
            memcpy(c->ram + rw, crs + co.runway, size);
            gt4_runway_relocate(rw);
            for (uint32_t o = 0; o < size; o++) d += orig[o] != c->ram[rw + o];
            if (d) printf("%-20s runway relocation: %d bytes differ\n", l.name[i], d);
            bad += d != 0;
            free(orig);
        }
        ee_w32(c, obj + 0xdc, rw);
        ee_w32(c, course + 0x9c, ad + adln);
        memcpy(c->ram + ad, adf, an);
        line_setup_run(c, lines, course, 0);
        keep = malloc(an + 0x10);
        memcpy(keep, c->ram + ad, an);
        memcpy(keep + an, c->ram + lines, 0x10);
        memcpy(c->ram + ad, adf, an);
        line_setup_run(c, lines, course, 1);
        {
            int d = 0;
            for (size_t o = 0; o < an; o++)
                if (keep[o] != c->ram[ad + o]) {
                    if (getenv("SETUP_ALL") && d < 16) printf("  +0x%zx (ADLN +0x%zx): %02x %02x\n", o, o - adln, keep[o], c->ram[ad + o]);
                    d++;
                }
            d += memcmp(keep + an, c->ram + lines, 0x10) != 0;
            printf("%-20s runway 0x%06x, lines %s, %d bytes differ\n", l.name[i], size,
                   ee_r32(c, ad + adln) == 0x4e4c4441u ? "ADLN" : "other", d);
            bad += d != 0;
        }
        free(keep);
        free(crs);
        free(adf);
    }
    ee_w32(c, obj + 0xdc, keep_rw);
    ee_w32(c, course + 0x9c, keep_ad);
    printf("%d courses differ\n", bad);
    return bad != 0;
}

static void list_entry(void *user, const char *name, int is_dir, uint32_t size)
{
    (void)user;
    printf("  %s%s %u\n", name, is_dir ? "/" : "", size);
}

static gt4_specdb_src disc_src(gt4_spec_tables *t)
{
    gt4_specdb_src s = { gt4_spec_tables_row, t, gt4_spec_tables_str, gt4_spec_tables_car_parts };
    return s;
}

/* the last functions entered, for where a run stops */
static ee_ctx *stop_ctx;
static uint32_t recent[24];
static int recent_n;
/* SETUP_KIND: the start kind the session gives each car (its virtual
   +0xF0 writes it through a2), replaced after the call */
static uint32_t kind_target;
static int in_kind;
static void kind_wrap(ee_ctx *c)
{
    uint32_t a2 = c->r[6].uw[0];
    in_kind = 1;
    recomp_lookup(kind_target)(c);
    in_kind = 0;
    ee_w32(c, a2, (uint32_t)strtol(getenv("SETUP_KIND"), NULL, 0));
}
static ee_fn recent_hook(uint32_t addr)
{
    if (addr >= 0x00340000u && addr < 0x00470000u) recent[recent_n++ % 24] = addr;
    if (addr == 0x003f1e60u && stop_ctx) longjmp(*stop_ctx->exit_jmp, 2);   /* the cars are set up */
    if (addr == kind_target && !in_kind && stop_ctx && getenv("SETUP_KIND")) return kind_wrap;
    if (stop_ctx && getenv("SETUP_TRACE") && (addr == 0x003630c0u || addr == 0x0034c5e0u))
        printf("  0x%08x a1 %08x t2 %d t3 %d f12..f15 %.2f %.2f %.2f %.3f\n", addr, stop_ctx->r[5].uw[0], (int)stop_ctx->r[10].uw[0],
               (int)stop_ctx->r[11].uw[0], stop_ctx->fpr[12].f, stop_ctx->fpr[13].f, stop_ctx->fpr[14].f, stop_ctx->fpr[15].f);
    return NULL;
}
static void print_recent(void)
{
    for (int k = 0; k < 24 && k < recent_n; k++) printf(" %08x", recent[(recent_n + k) % 24]);
    printf("\n");
}

static int call(ee_ctx *c, uint32_t addr, const uint32_t *args, int n)
{
    jmp_buf exit_jmp;
    ee_fn fn = recomp_lookup(addr);
    static const int regs[6] = { 4, 5, 6, 7, 8, 9 };
    if (!fn) {
        fprintf(stderr, "0x%08x was not recompiled\n", addr);
        return 1;
    }
    memset(c->r, 0, sizeof c->r);
    for (int k = 0; k < n; k++) c->r[regs[k]].uw[0] = args[k];
    c->r[29].uw[0] = STACK_TOP;
    c->r[31].uw[0] = 0;
    c->exit_jmp = &exit_jmp;
    if (setjmp(exit_jmp) != 0) {
        fprintf(stderr, "0x%08x: run ended at 0x%08x: %s\n", addr, c->end_pc, c->end_detail);
        return 1;
    }
    fn(c);
    return 0;
}

static int write_file(const char *dir, const char *name, const void *p, size_t n)
{
    char path[512];
    FILE *f;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    if (!(f = fopen(path, "wb"))) {
        fprintf(stderr, "cannot write %s\n", path);
        return 1;
    }
    fwrite(p, 1, n, f);
    fclose(f);
    return 0;
}

int main(int argc, char **argv)
{
    static ee_ctx c;
    uint32_t conductor, car, entry, garage, mode, params;
    FILE *f;

    if (argc < 4) {
        fprintf(stderr, "usage: %s <eeMemory.bin> <conductor> <out_dir> [native]\n", argv[0]);
        return 2;
    }
    conductor = (uint32_t)strtoul(argv[2], NULL, 0);
    if (ee_init(&c) != 0) return 2;
    if (!(f = fopen(argv[1], "rb")) || fread(c.ram, 1, EE_RAM_SIZE, f) != EE_RAM_SIZE) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    fclose(f);
    c.lookup = recomp_lookup;
    c.code_ranges = recomp_code_ranges;
    c.n_code_ranges = recomp_n_code_ranges;
    c.cop0[EE_COP0_STATUS] = 0x70030c13u;
    phys_natives_select(argc > 4 ? argv[4] : NULL);
    phys_natives_install(&c);

    car = conductor + CAR_OFFSET;
    params = ee_r32(&c, car + 0x10);
    if (params != conductor + PARAMS_OFF) {
        fprintf(stderr, "car 0 parameters at 0x%08x, not conductor+0x98\n", params);
        return 2;
    }
    /* func_0034D490: entry = [[race+0x60]+8][0], its garage record at +0x20;
       the mode is func_0034C968 (conductor+0xCBDD, 0xFF as 0). */
    entry = ee_r32(&c, ee_r32(&c, ee_r32(&c, ee_r32(&c, car) + 0x60) + 8));
    mode = ee_r8(&c, conductor + 0xcbddu);
    if (mode == 0xff) mode = 0;
    printf("entry 0x%08x mode %u\n", entry, mode);
    if (getenv("SETUP_WATCH")) {
        watch_ctx = &c;
        watch_addr = params + (uint32_t)strtoul(getenv("SETUP_WATCH"), NULL, 0);
        watch_last = ee_r32(&c, watch_addr);
        phys_extra_hook = watch_hook;
    }
    if (write_file(argv[3], "snap.bin", c.ram + params, PARAMS_SIZE)) return 2;
    if (getenv("SETUP_ROW")) {
        /* SETUP_ROW=table:id[:nth] prints a row of the game's tables and exits */
        uint32_t table = 0, id = 0, nth = 0, obj, blob;
        uint8_t row[512];
        int n;
        sscanf(getenv("SETUP_ROW"), "%i:%i:%i", (int *)&table, (int *)&id, (int *)&nth);
        obj = ee_r32(&c, 0x006235a8u + 0x134u + table * 4);
        blob = obj ? ee_r32(&c, obj) : 0;
        n = blob ? gt4_specdb_row_nth(c.ram + blob, EE_RAM_SIZE - blob, id, nth, row, sizeof row) : 0;
        for (int i = 0; i < n; i++) printf("%02x%s", row[i], i % 16 == 15 ? "\n" : " ");
        printf("\n%d bytes\n", n);
        return 0;
    }
    if (getenv("SETUP_DISC")) {
        /* SETUP_DISC=<disc image>: the tables of the disc against the game's
           memory, and the native record reads the disc. */
        static gt4_specdb_src disc;
        const char *err = NULL;
        gt4_vol *vol = gt4_vol_open(getenv("SETUP_DISC"), &err);
        gt4_spec_tables *tables;
        if (!vol) {
            fprintf(stderr, "disc: %s\n", err ? err : "cannot open");
            return 2;
        }
        tables = gt4_spec_tables_open(vol, NULL);
        if (getenv("SETUP_LIST")) gt4_vol_list(vol, getenv("SETUP_LIST"), list_entry, NULL);
        if (getenv("SETUP_READ")) {
            /* SETUP_READ=<path>: that file of the disc against the string
               database the game has in memory (D_00623788) */
            size_t size = 0;
            uint8_t *d = gt4_vol_read(vol, getenv("SETUP_READ"), &size);
            uint32_t blob = ee_r32(&c, ee_r32(&c, 0x00623788u));
            printf("%s: %s, %zu bytes, %s\n", getenv("SETUP_READ"), d ? "read" : "not found", size,
                   d && blob + size <= EE_RAM_SIZE && !memcmp(c.ram + blob, d, size) ? "same as memory" : "differs");
            free(d);
        }
        for (uint32_t t = 0; t < GT4_SPEC_TABLES; t++) {
            size_t size;
            const uint8_t *d = gt4_spec_tables_data(tables, t, &size);
            uint32_t obj = ee_r32(&c, 0x006235a8u + 0x134u + t * 4), blob = obj ? ee_r32(&c, obj) : 0;
            if (!gt4_spec_tables_name(t)) continue;
            if (!d) printf("table %2u %-16s not on the disc\n", t, gt4_spec_tables_name(t));
            else if (!blob || blob + size > EE_RAM_SIZE) printf("table %2u %-16s not in memory\n", t, gt4_spec_tables_name(t));
            else printf("table %2u %-16s %7u bytes %s\n", t, gt4_spec_tables_name(t), (unsigned)size,
                        memcmp(d, c.ram + blob, size) ? "DIFFER" : "same");
        }
        if (getenv("SETUP_RUNWAY")) {
            /* SETUP_RUNWAY=<course>: its runway from the disc, relocated by
               the rewrite at the snapshot's address, against the snapshot */
            char path[128];
            size_t n;
            uint8_t *crs;
            gt4_course co;
            uint32_t mesh = ee_r32(&c, ee_r32(&c, ee_r32(&c, car + 8) + 4) + 0xdc), size;
            snprintf(path, sizeof path, "/crs/%s", getenv("SETUP_RUNWAY"));
            crs = gt4_vol_read(vol, path, &n);
            if (!crs || !gt4_course_open(&co, crs, n)) { printf("cannot read %s\n", path); return 2; }
            memcpy(&size, crs + co.runway + 8, 4);
            {
                uint8_t *snap_copy = malloc(size);
                uint32_t diff = 0;
                memcpy(snap_copy, c.ram + mesh, size);
                memcpy(c.ram + mesh, crs + co.runway, size);
                gt4_runway_relocate(mesh);
                for (uint32_t o = 0; o < size; o += 4)
                    if (memcmp(snap_copy + o, c.ram + mesh + o, 4)) {
                        if (diff < 10) printf("runway +0x%x: snapshot %08x, disc %08x\n", o,
                                              *(uint32_t *)(snap_copy + o), *(uint32_t *)(c.ram + mesh + o));
                        diff++;
                    }
                printf("runway %s: 0x%x bytes at 0x%08x, %u words differ\n", path, size, mesh, diff);
            }
            return 0;
        }
        if (getenv("SETUP_COURSES")) return all_courses(&c, vol, ee_r32(&c, car + 8));
        if (getenv("SETUP_REINIT")) {
            /* SETUP_REINIT=<course or ->: another course's runway and lines
               (from the disc, where the snapshot's course models were) and,
               with SETUP_CAR=id, a new garage entry for car 0; then the race
               init (func_0034D490) again. ram.bin: the memory afterwards, for
               physics_run. */
            uint32_t course = ee_r32(&c, ee_r32(&c, conductor) + 0x80), obj = ee_r32(&c, course + 4);
            const char *name = getenv("SETUP_REINIT");
            jmp_buf exit_jmp;
            ee_fn fn = recomp_lookup(0x0034d490u);
            if (strcmp(name, "-")) {
                char path[128];
                size_t n, an;
                uint8_t *crs, *adf;
                gt4_course co;
                uint32_t size, rw = obj, ad = obj + 0x200000u;
                snprintf(path, sizeof path, "/crs/%s", name);
                crs = gt4_vol_read(vol, path, &n);
                snprintf(path, sizeof path, "/crs/%s.ad", name);
                adf = gt4_vol_read(vol, path, &an);
                if (!crs || !adf || !gt4_course_open(&co, crs, n)) { printf("cannot read course %s\n", name); return 2; }
                memcpy(&size, crs + co.runway + 8, 4);
                if (size > 0x200000u || an > 0x40000u) { printf("course too big\n"); return 2; }
                memcpy(c.ram + rw, crs + co.runway, size);
                gt4_runway_relocate(rw);
                ee_w32(&c, obj + 0xdc, rw);
                memcpy(c.ram + ad, adf, an);
                for (int k = 0; k < 5; k++) {
                    uint32_t off;
                    memcpy(&off, adf + 4 + k * 4, 4);
                    ee_w32(&c, course + 0x9c + (uint32_t)k * 4, off ? ad + off : 0);
                }
                printf("course %s: runway 0x%x bytes at 0x%08x, lines at 0x%08x\n", name, size, rw, ad);
                free(crs);
                free(adf);
            }
            if (getenv("SETUP_CAR")) {
                uint32_t id = (uint32_t)strtoul(getenv("SETUP_CAR"), NULL, 0);
                gt4_specdb_src src = disc_src(tables);
                int ok = gt4_garage_new(entry + 0x20, id, &src, 0x01e32000u);
                printf("car 0x%x: garage %s\n", id, ok ? "built" : "FAILED");
            }
            if (getenv("SETUP_POKE")) {
                /* SETUP_POKE=addr=byte,...: settings changed before the init */
                const char *q = getenv("SETUP_POKE");
                while (*q) {
                    char *e;
                    uint32_t addr = (uint32_t)strtoul(q, &e, 0), v = 0;
                    if (*e == '=') v = (uint32_t)strtoul(e + 1, &e, 0);
                    ee_w8(&c, addr, (uint8_t)v);
                    q = *e == ',' ? e + 1 : e;
                    if (e == q && *q && *q != ',') break;
                }
            }
            memset(c.r, 0, sizeof c.r);
            c.r[4].uw[0] = conductor;
            c.r[5].uw[0] = ee_r8(&c, conductor + 0xcbdcu);
            c.r[29].uw[0] = STACK_TOP;
            c.exit_jmp = &exit_jmp;
            kind_target = ee_r32(&c, ee_r32(&c, conductor + 0x10140u) + 0xf0 + 4);
            /* the forced kind replaces what the original session method
               writes: with it the session stays the original */
            if (getenv("SETUP_KIND")) gt4_session_orig = 1;
            phys_extra_hook = recent_hook;
            stop_ctx = &c;
            int rc = setjmp(exit_jmp);
            if (rc == 1) { printf("race init stopped at 0x%08x: %s\n", c.end_pc, c.end_detail); print_recent(); return 1; }
            if (rc == 0) fn(&c);
            stop_ctx = NULL;
            printf("race init done; car 0 params at 0x%08x, position %.2f %.2f %.2f\n", ee_r32(&c, car + 0x10),
                   ((float *)(c.ram + car + 0x104))[0], ((float *)(c.ram + car + 0x104))[1], ((float *)(c.ram + car + 0x104))[2]);
            {
                uint32_t rw = ee_r32(&c, obj + 0xdc), g = ee_r32(&c, rw + 0x5c);
                for (int k = 0; k < 2; k++) {
                    gt4_ray_hit hit;
                    float p[3] = { GT4_F32(g + k * 16), GT4_F32(g + k * 16 + 4) + 5.0f, GT4_F32(g + k * 16 + 8) };
                    int h = gt4_course_ray_down(rw, &hit, p);
                    {
                        jmp_buf ej;
                        uint32_t hb = 0x01e33000u, pb = 0x01e33100u;
                        memcpy(c.ram + pb, p, 12);
                        memset(c.r, 0, sizeof c.r);
                        c.r[4].uw[0] = rw; c.r[5].uw[0] = hb; c.r[6].uw[0] = pb; c.r[29].uw[0] = STACK_TOP;
                        c.exit_jmp = &ej;
                        phys_natives_select(NULL);
                        if (setjmp(ej) == 0) recomp_lookup(0x003e5a30u)(&c);
                        printf("  original: %u y %.2f leaf %d%s", c.r[2].uw[0], GT4_F32(hb + 4), (int16_t)GT4_U16(hb + 0x2e), "\n");
                    }
                    printf("grid %d (%.2f %.2f %.2f): ray %s y %.2f leaf %d%s", k, p[0], p[1] - 5.0f, p[2], h ? "hit" : "miss", hit.pos[1], hit.leaf, "\n");
                }
            }
            if (write_file(argv[3], "ram.bin", c.ram, EE_RAM_SIZE)) return 2;
            return 0;
        }
        if (getenv("SETUP_LINES")) {
            /* SETUP_LINES=<course>: its .ad file from the disc where the
               snapshot has it (the ADLN block at course+0x9C, 0x80 into the
               file), a new lines object, and the race init's conversion
               (func_0046B0A8, func_0046B9F0); against the snapshot. */
            char path[128];
            size_t n;
            uint8_t *ad, *snap_ad, snap_lines[0x40];
            uint32_t course = ee_r32(&c, car + 8), adln = ee_r32(&c, course + 0x9c), base = adln - 0x80;
            uint32_t lines = 0x01e30000u, snap_obj = 0x01c052b0u, diff = 0;
            jmp_buf exit_jmp;
            snprintf(path, sizeof path, "/crs/%s.ad", getenv("SETUP_LINES"));
            ad = gt4_vol_read(vol, path, &n);
            if (!ad) { printf("cannot read %s\n", path); return 2; }
            snap_ad = malloc(n);
            memcpy(snap_ad, c.ram + base, n);
            memcpy(snap_lines, c.ram + snap_obj, sizeof snap_lines);
            memcpy(c.ram + base, ad, n);
            memset(c.ram + lines, 0, 0x40);
            c.exit_jmp = &exit_jmp;
            if (setjmp(exit_jmp) != 0) { printf("line setup stopped at 0x%08x: %s\n", c.end_pc, c.end_detail); return 1; }
            for (int k = 0; k < 2; k++) {
                ee_fn fn = recomp_lookup(k ? 0x0046b9f0u : 0x0046b0a8u);
                memset(c.r, 0, sizeof c.r);
                c.r[4].uw[0] = lines;
                c.r[5].uw[0] = course;
                c.r[29].uw[0] = STACK_TOP;
                fn(&c);
            }
            for (uint32_t o = 0x80; o < n; o += 4)
                if (memcmp(snap_ad + o, c.ram + base + o, 4)) {
                    if (diff < 12) printf("ad +0x%x: snapshot %08x, rebuilt %08x\n", o, *(uint32_t *)(snap_ad + o), ee_r32(&c, base + o));
                    diff++;
                }
            if (write_file(argv[3], "lines.bin", c.ram + base, n) || write_file(argv[3], "linesobj.bin", c.ram + lines, 0x10)) return 2;
            printf("%s: %u words differ; lines object %s\n", path, diff,
                   memcmp(snap_lines, c.ram + lines, sizeof snap_lines) ? "DIFFERS" : "same");
            for (int o = 0; o < 0x40; o += 4) printf("%08x%s", ee_r32(&c, lines + o), o % 32 == 28 ? "\n" : " ");
            for (int o = 0; o < 0x40; o += 4) printf("%08x%s", *(uint32_t *)(snap_lines + o), o % 32 == 28 ? "\n" : " ");
            return 0;
        }
        disc.row = gt4_spec_tables_row;
        disc.ctx = tables;
        disc.str = gt4_spec_tables_str;
        disc.car_parts = gt4_spec_tables_car_parts;
        phys_record_src = &disc;
    }

    if (getenv("SETUP_NEWCAR")) {
        /* SETUP_NEWCAR=id: a new garage entry for that GENERIC_CAR row, by
           the original (func_00444480) and the rewrite; entry.bin holds both
           (0x178 bytes each) and the snapshot's entry of car 0. */
        uint32_t id = (uint32_t)strtoul(getenv("SETUP_NEWCAR"), NULL, 0), a = 0x01e30000u, b = 0x01e31000u;
        const gt4_specdb_src *src = phys_record_src ? phys_record_src : &phys_memory_src;
        jmp_buf exit_jmp;
        ee_fn fn = recomp_lookup(0x00444480u);
        int ok_a, ok_b;
        memset(c.ram + a, 0, 0x178);
        memset(c.ram + b, 0, 0x178);
        memset(c.r, 0, sizeof c.r);
        c.r[4].uw[0] = a;
        c.r[5].ud[0] = id;                       /* key: table 0 (GENERIC_CAR), row id */
        c.r[29].uw[0] = STACK_TOP;
        c.exit_jmp = &exit_jmp;
        if (!fn || setjmp(exit_jmp) != 0) { fprintf(stderr, "func_00444480 failed\n"); return 1; }
        fn(&c);
        ok_a = (int)c.r[2].uw[0];
        ok_b = gt4_garage_new(b, id, src, 0x01e32000u);
        printf("new car 0x%x: original %d, rewrite %d, %s\n", id, ok_a, ok_b,
               memcmp(c.ram + a, c.ram + b, 0x178) ? "entries DIFFER" : "entries same");
        {
            uint8_t out[3 * 0x178];
            memcpy(out, c.ram + a, 0x178);
            memcpy(out + 0x178, c.ram + b, 0x178);
            memcpy(out + 0x2f0, c.ram + entry + 0x20, 0x178);
            if (write_file(argv[3], "entry.bin", out, sizeof out)) return 2;
        }
        return 0;
    }
    garage = entry + 0x20;
    if (getenv("SETUP_CAR")) {
        /* SETUP_CAR=id: a new garage entry for that car (func_00444480),
           instead of the snapshot's, for the record and the parameters */
        uint32_t gar = 0x01e30000u;
        jmp_buf exit_jmp;
        ee_fn fn = recomp_lookup(0x00444480u);
        memset(c.ram + gar, 0, 0x178);
        memset(c.r, 0, sizeof c.r);
        c.r[4].uw[0] = gar;
        c.r[5].ud[0] = (uint32_t)strtoul(getenv("SETUP_CAR"), NULL, 0);
        c.r[29].uw[0] = STACK_TOP;
        c.exit_jmp = &exit_jmp;
        if (!fn || setjmp(exit_jmp) != 0) { fprintf(stderr, "func_00444480 failed\n"); return 1; }
        fn(&c);
        if (!c.r[2].uw[0]) { printf("no such car\n"); return 3; }
        if (write_file(argv[3], "entry.bin", c.ram + gar, 0x178)) return 2;
        garage = gar;
    }
    if (getenv("SETUP_GARAGE")) fuzz_garage(&c, garage, (uint32_t)strtoul(getenv("SETUP_GARAGE"), NULL, 0), &mode);
    {
        uint32_t a[4] = { RECORD_AT, garage, 0, mode };
        if (call(&c, 0x003f3b68u, a, 4)) return 1;
    }
    if (write_file(argv[3], "record.bin", c.ram + RECORD_AT, RECORD_SIZE)) return 2;
    if (getenv("SETUP_ZERO")) memset(c.ram + params, 0, PARAMS_SIZE);
    if (getenv("SETUP_SEED")) fuzz_record(&c, RECORD_AT, conductor + 0xf818u, (uint32_t)strtoul(getenv("SETUP_SEED"), NULL, 0));
    {
        /* func_00363B10: a2 = (s8)car+0x56A, a3 = 0, t0 = conductor+0xF818,
           t1 = entry+0x3468. */
        uint32_t kind = getenv("SETUP_SEED") ? fuzz_mode : (uint32_t)(int32_t)(int8_t)ee_r8(&c, car + 0x56a);
        uint32_t a[6] = { params, RECORD_AT, kind, 0,
                          conductor + 0xf818u, entry + 0x3468u };
        if (call(&c, 0x00361908u, a, 6)) return 1;
    }
    if (write_file(argv[3], "params.bin", c.ram + params, PARAMS_SIZE)) return 2;
    if (write_file(argv[3], "ram.bin", c.ram, EE_RAM_SIZE)) return 2;
    phys_natives_report();
    return 0;
}
