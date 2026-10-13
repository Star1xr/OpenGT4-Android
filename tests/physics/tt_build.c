/* The Time Trial built from the disc against the PCSX2 Time Trial
   captures (a_parado, nur_02: Ford Ka '01): the session and world tables,
   the world's mode, the two entries (the player's car and its ghost) and
   the ghost's state while it has no lap to drive.
   usage: tt_build <disc.iso> <capture.eeMemory.bin> <course> [--negative]
   --negative builds an Arcade race instead and must fail.
   tt_build <disc.iso> --ghost <course> [--ghost-negative]: two laps with a
   fixed driver along the course's line; on the second the ghost must drive
   the first again, place for place, step for step. --ghost-negative
   compares it 30 steps off and must fail. */
#include "race_run.h"
#include "car.h"
#include "specdb.h"
#include "vol.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pad_read(void *u, uint32_t car, uint8_t *cmd, const uint8_t *ev)
{ (void)u; (void)car; (void)ev; memset(cmd, 0, 16); return 1; }
static int pad_wheel(void *u, uint32_t car, int16_t state[2]) { (void)u; (void)car; state[0] = state[1] = 0; return 1; }
static int pad_write(void *u, uint32_t car, const uint8_t *out) { (void)u; (void)car; (void)out; return 1; }

/* --ghost: the player's car follows the course's line 45 m ahead, the
   wheel turned at most 0.04 a step, full throttle but in the turns */
static gt4_race_run *drive_run;
static float drive_held;
static int drive_read(void *u, uint32_t car, uint8_t *cmd, const uint8_t *ev)
{
    float p[3], yaw, t[3], steer = 0, throttle = 0;
    (void)u;
    memset(cmd, 0, 0x20);
    cmd[3] = ev[3];
    if (car != gt4_race_run_car(drive_run, 0)) { cmd[1] = 1; return 1; }
    if (gt4_race_run_place(drive_run, 0, p, &yaw) && gt4_race_run_course_point(drive_run, GT4_F32(car + 0x5a8) + 45, t)) {
        float fx = -sinf(yaw), fz = -cosf(yaw), dx = t[0] - p[0], dz = t[2] - p[2];
        float want = -0.8f * atan2f(fx * dz - fz * dx, fx * dx + fz * dz);
        want = fmaxf(-1, fminf(1, want));
        drive_held += fmaxf(-0.04f, fminf(0.04f, want - drive_held));
        steer = drive_held;
        throttle = fabsf(steer) > .8f ? .5f : 1.f;
    }
    int16_t s16 = (int16_t)(steer * 4096), t16 = (int16_t)(throttle * 4096);
    cmd[0] = 0x17;
    cmd[1] = 2;
    memcpy(cmd + 4, &s16, 2);
    memcpy(cmd + 8, &t16, 2);
    return 1;
}

static int ghost_test(const char *disc, const char *course, int negative)
{
    const char *err;
    gt4_vol *v = gt4_vol_open(disc, &err);
    size_t n = 0;
    uint8_t *labels = v ? gt4_vol_read(v, "/specdb/GT4_US2560/GENERIC_CAR.idi", &n) : NULL;
    gt4_race_desc d;
    memset(&d, 0, sizeof d);
    snprintf(d.course, sizeof d.course, "%s", course);
    d.cars = 1;
    d.time_trial = 1;
    if (!labels || !gt4_specdb_label(labels, n, getenv("TT_CAR") ? getenv("TT_CAR") : "ka_01", &d.car[0])) { fprintf(stderr, "no such car\n"); return 2; }   /* TT_CAR: another car */
    free(labels);
    gt4_vol_close(v);
    gt4_phys_pad.read = drive_read;
    gt4_phys_pad.wheel_state = pad_wheel;
    gt4_phys_pad.write = pad_write;
    char msg[256];
    drive_run = gt4_race_run_open(disc, &d, msg, sizeof msg);
    if (!drive_run) { fprintf(stderr, "build: %s\n", msg); return 1; }
    enum { MAXF = 60 * 60 * 6 };
    static float lap1[MAXF][3];
    int n1 = 0, k2 = -1, bad = 0;
    double worst = 0;
    uint32_t car0 = gt4_race_run_car(drive_run, 0);
    for (int f = 0; f < MAXF * 3; f++) {
        uint16_t before = GT4_U16(car0 + 0x104 + 0x4ac);
        if (!gt4_race_run_frame(drive_run)) { fprintf(stderr, "frame %d failed\n", f); return 1; }
        uint16_t lap = GT4_U16(car0 + 0x104 + 0x4ac);
        float p[3], yaw;
        if (lap != before) {
            fprintf(stderr, "frame %d: lap %u -> %u, ghost %s\n", f, before, lap, gt4_race_run_ghost_on(drive_run) ? "on" : "off");
            if (lap == 3) break;
            if (lap == 2) k2 = 0;
        }
        (void)yaw;
        if (lap == 1 && n1 < MAXF) memcpy(lap1[n1++], gt4_ptr(car0 + CAR_POSITION), sizeof p);   /* the physics' own place */
        if (lap == 2 && k2 >= 0) {
            int k = k2++ + (negative ? 30 : 0);
            if (k < n1 && gt4_race_run_ghost_on(drive_run)) {
                memcpy(p, gt4_ptr(car0 + CAR_SIZE + CAR_POSITION), sizeof p);
                double e = sqrt((p[0] - lap1[k][0]) * (p[0] - lap1[k][0]) + (p[1] - lap1[k][1]) * (p[1] - lap1[k][1]) +
                                (p[2] - lap1[k][2]) * (p[2] - lap1[k][2]));
                if (e > worst) worst = e;
                if (e > 0.01 && !bad) { fprintf(stderr, "step %d of the lap: the ghost %.3f m from the first lap\n", k2 - 1, e); bad = 1; }
            }
        }
    }
    gt4_race_run_close(drive_run);
    if (n1 < 600 || k2 < 600) { fprintf(stderr, "FAIL the laps did not run (lap 1 %d steps, lap 2 %d)\n", n1, k2); return 1; }
    if (bad) { fprintf(stderr, "FAIL the ghost leaves the first lap (worst %.3f m)\n", worst); return 1; }
    printf("PASS the ghost drives the first lap again: %d steps, worst %.4f m\n", n1 < k2 ? n1 : k2, worst);
    return 0;
}

static uint8_t *cap;
static uint32_t c32(uint32_t a) { return cap[a] | cap[a + 1] << 8 | cap[a + 2] << 16 | (uint32_t)cap[a + 3] << 24; }

int main(int argc, char **argv)
{
    int negative = argc > 4 && !strcmp(argv[4], "--negative"), bad = 0;
    if (argc >= 4 && !strcmp(argv[2], "--ghost")) return ghost_test(argv[1], argv[3], argc > 4 && !strcmp(argv[4], "--ghost-negative"));
    if (argc < 4) { fprintf(stderr, "usage: tt_build <disc.iso> <capture> <course> [--negative]\n"); return 2; }
    FILE *f = fopen(argv[2], "rb");
    if (!f) { fprintf(stderr, "no capture\n"); return 2; }
    cap = malloc(0x2000000);
    if (!cap || fread(cap, 1, 0x2000000, f) != 0x2000000) { fprintf(stderr, "short capture\n"); return 2; }
    fclose(f);
    /* the capture's conductor: the Time Trial session's table is at +0x10140 */
    uint32_t ccond = 0;
    for (uint32_t a = 0x1000000; a + 0x10144 < 0x2000000; a += 4)
        if (c32(a + 0x10140) == 0x00685890u && c32(a) && c32(c32(a) + 0x84) < 0x2000000 && c32(c32(c32(a) + 0x84) + 0x64) == 0x00680140u) { ccond = a; break; }
    if (!ccond) { fprintf(stderr, "not a Time Trial capture\n"); return 2; }

    const char *err;
    gt4_vol *v = gt4_vol_open(argv[1], &err);
    size_t n = 0;
    uint8_t *labels = v ? gt4_vol_read(v, "/specdb/GT4_US2560/GENERIC_CAR.idi", &n) : NULL;
    gt4_race_desc d;
    memset(&d, 0, sizeof d);
    snprintf(d.course, sizeof d.course, "%s", argv[3]);
    d.cars = 1;
    d.time_trial = !negative;
    if (!labels || !gt4_specdb_label(labels, n, "ka_01", &d.car[0])) { fprintf(stderr, "no ka_01\n"); return 2; }
    free(labels);
    gt4_vol_close(v);
    gt4_phys_pad.read = pad_read;
    gt4_phys_pad.wheel_state = pad_wheel;
    gt4_phys_pad.write = pad_write;
    char msg[256];
    gt4_race_run *r = gt4_race_run_open(argv[1], &d, msg, sizeof msg);
    if (!r) { fprintf(stderr, "build: %s\n", msg); return 1; }
    for (int i = 0; i < 600; i++) {   /* ten seconds: the ghost has stopped (kind 6, then 8) */
        if (getenv("TT_START_TRACE") && !(i % 100)) {
            uint32_t c0 = gt4_race_run_conductor(r) + 0x6698u;
            fprintf(stderr, "frame %d: car 0 at %.1f m, %.2f m/s, driver kind %u, held %d\n", i, GT4_F32(c0 + 0x5a8),
                    GT4_F32(c0 + 0x640), GT4_U8(c0 + 0x56b), (int)GT4_U32(GT4_U32(gt4_race_run_conductor(r)) + 0xbc));
        }
        if (!gt4_race_run_frame(r)) { fprintf(stderr, "frame %d failed\n", i); return 1; }
    }

    uint32_t cond = gt4_race_run_conductor(r), race = GT4_U32(cond), world = GT4_U32(race + 0x84);
    uint32_t crace = c32(ccond), cworld = c32(crace + 0x84), centries = c32(crace + 0x60);
#define SAME(what, ours, theirs) do { uint32_t o_ = (ours), t_ = (theirs); \
        if (o_ != t_) { fprintf(stderr, "%s: 0x%x, the capture 0x%x\n", what, o_, t_); bad = 1; } } while (0)
    SAME("session table", GT4_U32(cond + 0x10140), c32(ccond + 0x10140));
    SAME("world table", GT4_U32(world + 0x64), c32(cworld + 0x64));
    SAME("world mode", GT4_U32(world + 0xd68), c32(cworld + 0xd68));
    SAME("entries", GT4_U32(GT4_U32(race + 0x60)), c32(centries));
    SAME("cond+0xF8A0 cars", GT4_U32(cond + 0xf8a0), c32(ccond + 0xf8a0));
    SAME("cond+0xF864", GT4_U32(cond + 0xf864), c32(ccond + 0xf864));
    for (int i = 0; i < 2; i++) {
        uint32_t e = GT4_U32(GT4_U32(GT4_U32(race + 0x60) + 8) + (uint32_t)i * 4), ce = c32(c32(centries + 8) + (uint32_t)i * 4);
        uint32_t car = cond + 0x6698u + (uint32_t)i * CAR_SIZE, ccar = ccond + 0x6698u + (uint32_t)i * CAR_SIZE;
        char what[64];
        snprintf(what, sizeof what, "entry %d grid slot", i); SAME(what, GT4_U32(e + 0x3444), c32(ce + 0x3444));
        snprintf(what, sizeof what, "entry %d car", i); SAME(what, GT4_U32(e + 0x20), c32(ce + 0x20));
        snprintf(what, sizeof what, "entry %d profile kind", i); SAME(what, GT4_U32(GT4_U32(e + 0x343c) + 0x1cc), c32(c32(ce + 0x343c) + 0x1cc));
        snprintf(what, sizeof what, "car %d mode +0x56A", i); SAME(what, GT4_U8(car + 0x56a), cap[ccar + 0x56a]);
        snprintf(what, sizeof what, "car %d index +0x544", i); SAME(what, GT4_U8(car + 0x544), cap[ccar + 0x544]);
        if (i == 1 && cap[ccar + 0x56b] == 8) {   /* the ghost waiting for a lap (nur_02 holds it at 6 a moment later) */
            snprintf(what, sizeof what, "car %d driver kind +0x56B", i); SAME(what, GT4_U8(car + 0x56b), cap[ccar + 0x56b]);
            snprintf(what, sizeof what, "car %d +0x545", i); SAME(what, GT4_U8(car + 0x545), cap[ccar + 0x545]);
        }
    }
    gt4_race_run_close(r);
    if (bad) { fprintf(stderr, "FAIL the Time Trial built from the disc against %s\n", argv[2]); return 1; }
    printf("PASS the Time Trial built from the disc: tables, mode, two entries and the waiting ghost as %s\n", argv[2]);
    return 0;
}
