/*
 * Transitional: another car and course on a snapshot's race. The course's
 * runway and lines come from the disc (native loaders) into the memory
 * where the snapshot's course file was, the runway at its own offset in the
 * course file as the game has it (the race init allocates in the file's
 * first megabytes, where only models would be); car 0 gets a new garage entry
 * (native, from SpecDB); then the game's race init (func_0034D490,
 * recompiled) sets the race up again, up to the point where it would load
 * sound and replays (func_003F1E60).
 *
 * When the time trial's run-up line has no ground under it (the Photo
 * Travel places, gymkhanas: not Time Trial courses), the car is placed on
 * its grid slot instead (gt4_car_place, func_003630C0), a standing start.
 * With the runway where the snapshot's course models were, the race init's
 * own allocations wrote over it (the kd tree of the Nurburgring, the grid
 * of Suzuka) and many real courses fell back to that.
 */
#include "race_setup.h"
#include "car.h"
#include "natives.h"
#include "course.h"
#include "spectables.h"

#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ee_fn recomp_lookup(uint32_t addr);

#define RACE_INIT   0x0034d490u
#define AFTER_CARS  0x003f1e60u   /* first call after the cars' loop */
#define CAR_OFFSET  0x6698u
#define STACK_TOP   0x01ef0000u
#define SCRATCH     0x01e32000u   /* 0x680 bytes, zero in the snapshots */

#define SESSION_VT  0x10140u
#define COURSE_END  0x01b00000u   /* the course data stays below (the race's objects are at 0x1BE0000 on) */      /* conductor: the session's virtual table */
/* PHYS_PROGRESS: where the setup is, to stderr (for crashes) */
#define STAGE(n) do { if (getenv("PHYS_PROGRESS")) { fprintf(stderr, "setup %d\n", n); fflush(stderr); } } while (0)

/* How each car starts comes from the session (virtual +0xF0, written
   through a2; 0: standing on its grid slot). With force_grid it is 0. */
/* 1: the whole race init, past the cars (what a snapshot already has
   from its own init: the walls' car outlines, the pit's tyres...) */
int phys_race_full_init;
int32_t phys_race_other_cars[7] = { -1, -1, -1, -1, -1, -1, -1 };

static ee_ctx *stop_ctx;
static int force_grid, in_start;
static uint32_t start_fn;
static ee_fn (*chained)(uint32_t addr);
static void start_grid(ee_ctx *c)
{
    uint32_t a2 = c->r[6].uw[0];
    in_start = 1;
    recomp_lookup(start_fn)(c);
    in_start = 0;
    GT4_U32(a2) = 0;
}
static ee_fn stop_hook(uint32_t addr)
{
    if (addr == AFTER_CARS && stop_ctx && !phys_race_full_init && !getenv("PHYS_FULLINIT")) longjmp(*stop_ctx->exit_jmp, 2);
    if (addr == start_fn && stop_ctx && force_grid && !in_start) return start_grid;
    return chained ? chained(addr) : NULL;
}

static int race_init(ee_ctx *c, uint32_t conductor, char *msg, size_t len)
{
    int ok;
    memset(c->r, 0, sizeof c->r);
    c->r[4].uw[0] = conductor;
    c->r[5].uw[0] = GT4_U8(conductor + 0xcbdcu);
    c->r[29].uw[0] = STACK_TOP;
    start_fn = GT4_U32(GT4_U32(conductor + SESSION_VT) + 0xf0 + 4);
    chained = phys_extra_hook;
    phys_extra_hook = stop_hook;
    stop_ctx = c;
    ok = run(c, RACE_INIT, msg, len);
    stop_ctx = NULL;
    phys_extra_hook = chained;
    return ok;
}

static int run(ee_ctx *c, uint32_t addr, char *msg, size_t len)
{
    jmp_buf exit_jmp, *keep = c->exit_jmp;
    int rc;
    c->exit_jmp = &exit_jmp;
    rc = setjmp(exit_jmp);
    if (rc == 0) recomp_lookup(addr)(c);
    c->exit_jmp = keep;
    if (rc == 1) {
        snprintf(msg, len, "0x%08x stopped at 0x%08x: %s", addr, c->end_pc, c->end_detail);
        return 0;
    }
    return 1;
}

int phys_race_setup(ee_ctx *c, gt4_vol *vol, uint32_t conductor, const char *course_name,
                    int32_t car_id, char *msg, size_t len)
{
    uint32_t car = conductor + CAR_OFFSET, course = GT4_U32(GT4_U32(conductor) + 0x80), obj = GT4_U32(course + 4);
    uint32_t entry = GT4_U32(GT4_U32(GT4_U32(GT4_U32(car) + 0x60) + 8));
    int ok, standing = 0;
    msg[0] = 0;
    if (course_name) {
        char path[128];
        size_t n = 0, an = 0;
        uint8_t *crs, *adf;
        gt4_course co;
        uint32_t size, rw, ad;
        snprintf(path, sizeof path, "/crs/%s", course_name);
        crs = gt4_vol_read(vol, path, &n);
        snprintf(path, sizeof path, "/crs/%s.ad", course_name);
        adf = gt4_vol_read(vol, path, &an);
        if (!crs || !adf || !gt4_course_open(&co, crs, n)) {
            snprintf(msg, len, "cannot read course %s", course_name);
            free(crs);
            free(adf);
            return 0;
        }
        memcpy(&size, crs + co.runway + 8, 4);
        rw = obj + (uint32_t)co.runway;
        ad = (rw + size + 15u) & ~15u;
        if (size > n - co.runway || an > 0x40000u || ad + an > COURSE_END) {
            snprintf(msg, len, "course %s too big", course_name);
            free(crs);
            free(adf);
            return 0;
        }
        memcpy(gt4_ptr(rw), crs + co.runway, size);
        STAGE(1);
        gt4_runway_relocate(rw);
        GT4_U32(obj + 0xdc) = rw;
        memcpy(gt4_ptr(ad), adf, an);
        for (uint32_t k = 0; k < 5; k++) {
            uint32_t off;
            memcpy(&off, adf + 4 + k * 4, 4);
            GT4_U32(course + 0x9c + k * 4) = off ? ad + off : 0;
        }
        free(crs);
        free(adf);
    }
    for (uint32_t i = 0; i < 8; i++) {
        int32_t id = i ? phys_race_other_cars[i - 1] : car_id;
        uint32_t list = GT4_U32(GT4_U32(GT4_U32(conductor) + 0x60) + 8), e;
        if (id < 0 || i >= GT4_U32(GT4_U32(GT4_U32(conductor) + 0x60))) continue;
        e = GT4_U32(list + i * 4);
        {
            gt4_spec_tables *t = gt4_spec_tables_open(vol, NULL);
            gt4_specdb_src db = { gt4_spec_tables_row, t, gt4_spec_tables_str, gt4_spec_tables_car_parts };
            memset(gt4_ptr(SCRATCH), 0, 0x680);
            STAGE(2);
            ok = gt4_garage_new(e + 0x20, (uint32_t)id, &db, SCRATCH);
            gt4_spec_tables_close(t);
        }
        if (!ok) {
            snprintf(msg, len, "no garage entry for car 0x%x", (unsigned)id);
            return 0;
        }
    }
    {
        uint8_t *before = malloc(EE_RAM_SIZE);
        if (!before) return 0;
        memcpy(before, c->ram, EE_RAM_SIZE);
        force_grid = 0;
        STAGE(3);
        ok = race_init(c, conductor, msg, len);
        if (ok && !(fabsf(GT4_F32(car + 0x10c)) < 1e20f)) {
            standing = 1;
            /* the rolling start's run-up line has no ground under it: the
               game's own standing start on the grid slot instead */
            memcpy(c->ram, before, EE_RAM_SIZE);
            force_grid = 1;
        STAGE(4);
            ok = race_init(c, conductor, msg, len);
            force_grid = 0;
        }
        STAGE(5);
        free(before);
        if (!ok) return 0;
    }
    if (standing || !(fabsf(GT4_F32(car + 0x10c)) < 1e20f)) {
        /* On such courses the grid table is not a racing grid either (on
           Suzuka the cars face the pits): the car goes on the driving-line
           point nearest its grid slot that has ground, facing along it. */
        uint32_t rw = GT4_U32(obj + 0xdc), slot = GT4_U32(entry + 0x3444), g = GT4_U32(rw + 0x5c) + slot * 16;
        uint32_t lines = conductor + 0xda50u, table = GT4_U32(lines + 8);
        float gx = GT4_F32(g), gy = -GT4_F32(g + 8), best = 1e30f, x = 0, y = 0, h = 0, heading = 0;
        gt4_ray_hit hit;
        for (uint32_t k = 0; k < 8 && table; k++) {
            uint32_t line = GT4_U32(table + k * 4);
            int32_t n = line ? (int32_t)GT4_U32(line) : 0;
            for (int32_t i = 0; i < n; i++) {
                uint32_t pt = line + 0x40 + (uint32_t)i * 0x40;
                float q[3] = { GT4_F32(pt + 4), GT4_F32(pt + 0xc) + 5.0f, -GT4_F32(pt + 8) };
                float dx = q[0] - gx, dy = GT4_F32(pt + 8) - gy, d = dx * dx + dy * dy;
                if (!(d < best) || GT4_F32(pt + 0xc) == 0 || !gt4_course_ray_down(rw, &hit, q)) continue;
                best = d;
                x = q[0];
                y = GT4_F32(pt + 8);
                h = GT4_F32(pt + 0xc);
                heading = GT4_F32(pt + 0x24);
            }
        }
        if (!(best < 1e30f)) {
            snprintf(msg, len, "no ground to start on");
            return 0;
        }
        STAGE(6);
        gt4_car_place(car, 0xffffffffu, x, y, h, heading);
        snprintf(msg, len, "standing start on the driving line by the grid (the run-up line has no ground)");
    }
    return 1;
}
