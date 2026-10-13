/* Rewritten physics functions swapped into the recompiled original step
   (physics_run, opengt4_drive): adapters from the MIPS registers, the bridge
   back to original code, and the table of replacements. */
#include "natives.h"
#include "specdb.h"
#include "phys.h"
#include "engine.h"
#include "car.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Adapters: arguments from the MIPS registers (a0.., f12..), result to v0/f0. */
#define A(n)  (c->r[4 + (n)].uw[0])
#define F(n)  (c->fpr[12 + (n)].f)
static void *gp(uint32_t a) { return a ? gt4_ptr(a) : NULL; }
static void h_curve(ee_ctx *c)   { c->fpr[0].f = gt4_curve_eval(gp(A(0)), F(0)); }
static void h_torque(ee_ctx *c)  { c->fpr[0].f = gt4_engine_torque(gp(A(0)), F(0), gp(A(1))); }
static void h_braking(ee_ctx *c) { c->fpr[0].f = gt4_engine_braking(gp(A(0)), F(0)); }
static void h_wear(ee_ctx *c)    { c->fpr[0].f = gt4_engine_wear_factor(gp(A(0)), gp(A(1))); }
static void h_atan(ee_ctx *c)    { c->fpr[0].f = gt4_atanf(F(0)); }
static void h_atan2(ee_ctx *c)   { c->fpr[0].f = gt4_atan2f(F(0), F(1)); }
static void h_engine(ee_ctx *c)  { gt4_car_engine_update(A(0)); }
static void h_wheel_dirs(ee_ctx *c) { gt4_car_wheel_directions(A(0)); }
static void h_aero(ee_ctx *c)    { gt4_car_aero(A(0)); }
static void h_grip(ee_ctx *c)    { gt4_car_tyre_grip(A(0)); }
static void h_drivetrain(ee_ctx *c) { gt4_car_drivetrain(A(0)); }
static void h_chassis(ee_ctx *c) { gt4_car_chassis(A(0)); }
static void h_sin(ee_ctx *c)     { c->fpr[0].f = gt4_sinf(F(0)); }
static void h_cos(ee_ctx *c)     { c->fpr[0].f = gt4_cosf(F(0)); }
static void h_wrap(ee_ctx *c)    { c->fpr[0].f = gt4_angle_wrap(F(0)); }
static void h_adiff(ee_ctx *c)   { c->fpr[0].f = gt4_angle_diff(F(0), F(1)); }
static void h_ground(ee_ctx *c)   { gt4_car_ground_query(A(0)); }
static void h_ray(ee_ctx *c)      { c->r[2].sd[0] = gt4_course_ray_down(A(0), gp(A(1)), gp(A(2))); }
static void h_walls(ee_ctx *c)    { gt4_walls(A(0), (int)A(1)); }
static void h_controls(ee_ctx *c) { gt4_car_controls(A(0), A(1)); }
static void h_ai_drive(ee_ctx *c) { gt4_ai_drive(A(0), A(1)); }
static void h_ai_steer(ee_ctx *c) { gt4_ai_steer_to(A(0), F(0)); }
static void h_ai_pedals(ee_ctx *c) { gt4_ai_pedals(A(0), F(0), F(1)); }
/* The driver step needs guest memory for the controller's command: taken
   below the caller's stack pointer, which is lowered past it meanwhile. */
static void h_driver(ee_ctx *c)
{
    uint32_t sp = c->r[29].uw[0];
    c->r[29].sd[0] = (int32_t)(sp - 0x40);
    gt4_car_driver(A(0), sp - 0x40);
    c->r[29].sd[0] = (int32_t)sp;
}
static void h_slip(ee_ctx *c)     { gt4_car_slip(A(0)); }
static void h_tyre_wear(ee_ctx *c) { gt4_car_tyre_wear(A(0)); }
static void h_wheel_setup(ee_ctx *c) { gt4_car_wheel_setup(A(0)); }
static void h_ground_info(ee_ctx *c) { gt4_car_ground_info(A(0)); }
static void h_race_end(ee_ctx *c) { gt4_car_race_end(A(0)); }
static void h_keep_state(ee_ctx *c) { gt4_car_keep_state(A(0)); }
static void h_brake_heat(ee_ctx *c) { gt4_car_brake_heat(A(0)); }
static void h_wheel_noise(ee_ctx *c) { gt4_car_wheel_noise(A(0)); }
static void h_gaps(ee_ctx *c)     { gt4_race_gaps(A(0)); }
static void h_handover(ee_ctx *c) { gt4_race_finish_handover(A(0)); }
static void h_drive_settings(ee_ctx *c) { gt4_drive_settings(A(0)); }
static void h_race_begin(ee_ctx *c) { gt4_race_begin(A(0)); }
static void h_race_before(ee_ctx *c) { gt4_race_before_step(A(0)); }
static void h_race_after(ee_ctx *c) { gt4_race_after_step(A(0)); }
static void h_records_reset(ee_ctx *c) { gt4_race_records_reset(A(0)); }
static void h_queue_reset(ee_ctx *c) { gt4_race_queue_reset(A(0)); }
static void h_queue_push(ee_ctx *c) { gt4_race_queue_push(A(0), (int32_t)A(1), A(2), (int32_t)A(3)); }
static void h_queue_take(ee_ctx *c)
{
    uint32_t v;
    int r = gt4_race_queue_take(A(0), (int32_t)A(1), A(2) ? &v : NULL, (int)A(3));
    if (r && A(2)) GT4_U32(A(2)) = v;
    c->r[2].sd[0] = r;
}
static void h_queue_peek(ee_ctx *c)
{
    uint32_t v;
    int r = gt4_race_queue_peek(A(0), (int32_t)A(1), A(2) ? &v : NULL, (int32_t)A(3));
    if (r && A(2)) GT4_U32(A(2)) = v;
    c->r[2].sd[0] = r;
}

static void h_step(ee_ctx *c)
{
    uint32_t sp = c->r[29].uw[0];
    c->r[29].sd[0] = (int32_t)(sp - 0x40);
    gt4_step(A(0), (int)A(1), sp - 0x40);
    c->r[29].sd[0] = (int32_t)sp;
}
/* SpecDB rows from the tables the game has in memory (D_006235A8 + 0x134:
   per table an object whose first word points to the GTDB data). */
static int mem_row(void *ctx, uint32_t table, uint32_t id, uint32_t nth, uint8_t *out, size_t cap)
{
    uint32_t obj, blob;
    (void)ctx;
    if (table >= 0x30) return 0;
    obj = *(uint32_t *)gt4_ptr(0x006235a8u + 0x134u + table * 4);
    if (!obj || obj >= EE_RAM_SIZE) return 0;
    blob = *(uint32_t *)gt4_ptr(obj);
    if (!blob || blob >= EE_RAM_SIZE) return 0;
    return gt4_specdb_row_nth(gt4_ptr(blob), EE_RAM_SIZE - blob, id, nth, out, cap);
}
/* func_00449CD8 on the string database in memory (D_00623788) */
static const char *mem_str(void *ctx, uint32_t i)
{
    uint32_t obj = *(uint32_t *)gt4_ptr(0x00623788u), blob;
    (void)ctx;
    if (!obj || obj >= EE_RAM_SIZE) return NULL;
    blob = *(uint32_t *)gt4_ptr(obj);
    if (!blob || blob >= EE_RAM_SIZE) return NULL;
    return gt4_specdb_string(gt4_ptr(blob), EE_RAM_SIZE - blob, i);
}
const gt4_specdb_src *phys_record_src;
const gt4_specdb_src phys_memory_src = { mem_row, NULL, mem_str };
static void h_car_record(ee_ctx *c)
{
    gt4_specdb_src db = { mem_row, NULL, mem_str };
    gt4_car_record(A(0), A(1), A(3), phys_record_src ? phys_record_src : &db);
}
static void h_garage_new(ee_ctx *c)
{
    uint32_t sp = c->r[29].uw[0];
    c->r[29].sd[0] = (int32_t)(sp - 0x700);
    c->r[2].sd[0] = gt4_garage_new(A(0), c->r[5].ud[0], phys_record_src ? phys_record_src : &phys_memory_src, sp - 0x700);
    c->r[29].sd[0] = (int32_t)sp;
}
static void h_lines_setup(ee_ctx *c) { gt4_lines_setup(A(0), A(1)); }
static void h_lines_clear(ee_ctx *c) { c->r[2].sd[0] = gt4_lines_clear(A(0)); }
static void h_course_distance(ee_ctx *c) { c->fpr[0].f = gt4_course_distance(A(0), A(1)); }
static void h_car_init(ee_ctx *c)
{
    uint32_t sp = c->r[29].uw[0];
    float pos[4] = { F(0), F(1), c->fpr[14].f, c->fpr[15].f };
    c->r[29].sd[0] = (int32_t)(sp - 0x100);
    c->r[2].sd[0] = (int32_t)gt4_car_init(A(0), A(1), A(2), A(3), c->r[8].uw[0], c->r[9].uw[0], c->r[10].uw[0],
                                          c->r[11].uw[0], pos, (int32_t)ee_r32(c, sp), (int32_t)ee_r32(c, sp + 8),
                                          ee_r32(c, sp + 0x10), sp - 0x100);
    c->r[29].sd[0] = (int32_t)sp;
}
static void h_race_init(ee_ctx *c)
{
    uint32_t sp = c->r[29].uw[0];
    gt4_specdb_src db = { mem_row, NULL, mem_str };
    c->r[29].sd[0] = (int32_t)(sp - 0x700);
    gt4_race_init(A(0), A(1), phys_record_src ? phys_record_src : &db, sp - 0x700);
    c->r[29].sd[0] = (int32_t)sp;
}
static void h_pit_tyres(ee_ctx *c)
{
    gt4_specdb_src db = { mem_row, NULL, mem_str, NULL };
    gt4_pit_tyres(A(0), phys_record_src ? phys_record_src : &db);
}
static void h_car_params(ee_ctx *c)
{
    uint32_t sp = c->r[29].uw[0];
    c->r[29].sd[0] = (int32_t)(sp - 0x40);
    gt4_car_params(A(0), A(1), (int32_t)A(2), A(3), c->r[8].uw[0], c->r[9].uw[0], sp - 0x40);
    c->r[29].sd[0] = (int32_t)sp;
    c->r[2].sd[0] = 0;
}
static void h_wheel_dir(ee_ctx *c)  { c->fpr[0].f = gt4_wheel_direction(gp(A(0)), F(0), F(1)); }

/* Bridge for rewritten code that still calls an original function: runs the
   recompiled one on the same context. The caller's stack pointer is kept;
   the callee builds its frame below it and restores what it saves. */
static ee_ctx *bridge_ctx;
static float orig_call(uint32_t addr, uint32_t a0, uint32_t a1, float f12, float f13)
{
    ee_ctx *c = bridge_ctx;
    ee_fn fn = c->lookup(addr);
    static FILE *log;
    static int log_checked;
    if (!log_checked) {                   /* PHYS_BRIDGE_LOG=file: each call, by address */
        const char *p = getenv("PHYS_BRIDGE_LOG");
        log_checked = 1;
        if (p) log = fopen(p, "w");
    }
    if (log) fprintf(log, "%08x\n", addr);
    if (!fn) ee_end(c, EE_END_UNRECOMPILED, addr, "bridge: 0x%08x not recompiled", addr);
    c->r[4].sd[0] = (int32_t)a0;
    c->r[5].sd[0] = (int32_t)a1;
    c->r[6].sd[0] = (int32_t)gt4_orig_a2;
    c->r[7].sd[0] = (int32_t)gt4_orig_a3;
    c->r[8].sd[0] = (int32_t)gt4_orig_t0;
    c->r[9].sd[0] = (int32_t)gt4_orig_t1;
    c->r[10].sd[0] = (int32_t)gt4_orig_t2;
    c->r[11].sd[0] = (int32_t)gt4_orig_t3;
    c->fpr[14].f = gt4_orig_f14;
    c->fpr[15].f = gt4_orig_f15;
    gt4_orig_a2 = gt4_orig_a3 = gt4_orig_t0 = gt4_orig_t1 = gt4_orig_t2 = gt4_orig_t3 = 0;
    gt4_orig_f14 = gt4_orig_f15 = 0;
    c->fpr[12].f = f12;
    c->fpr[13].f = f13;
    c->r[31].sd[0] = 0;
    fn(c);
    gt4_orig_v0 = c->r[2].uw[0];
    return c->fpr[0].f;
}

static struct { uint32_t addr; ee_fn fn; int on; long calls; } natives[] = {
    { 0x00359568u, h_curve },
    { 0x00361908u, h_car_params },
    { 0x003f3b68u, h_car_record },
    { 0x003f3400u, h_ai_drive },
    { 0x00464bb0u, h_ai_steer },
    { 0x00464770u, h_ai_pedals },
    { 0x00444480u, h_garage_new },
    { 0x0046b0a8u, h_lines_setup },
    { 0x00363b10u, h_car_init },
    { 0x0034d490u, h_race_init },
    { 0x0035c4d8u, h_pit_tyres },
    { 0x0046b9f0u, h_lines_clear },
    { 0x003955e8u, h_course_distance },
    { 0x0034e9a0u, h_torque },
    { 0x0034ea08u, h_braking },
    { 0x00350868u, h_wear },
    { 0x0057d008u, h_atan },
    { 0x0057d118u, h_atan2 },
    { 0x00357700u, h_engine },
    { 0x00357690u, h_wheel_dirs },
    { 0x003575e0u, h_wheel_dir },
    { 0x003f3090u, h_aero },
    { 0x00357418u, h_grip },
    { 0x00359830u, h_curve },
    { 0x003535b0u, h_drivetrain },
    { 0x00344530u, h_chassis },
    { 0x0057d1f0u, h_sin },
    { 0x0057d2b8u, h_cos },
    { 0x0036cb90u, h_wrap },
    { 0x0036cc40u, h_adiff },
    { 0x00344458u, h_ground },
    { 0x003e5a30u, h_ray },
    { 0x0034daa8u, h_walls },
    { 0x00349f48u, h_controls },
    { 0x00349fd0u, h_driver },
    { 0x00351778u, h_slip },
    { 0x00352ea0u, h_tyre_wear },
    { 0x00344420u, h_wheel_setup },
    { 0x00369888u, h_ground_info },
    { 0x00353218u, h_race_end },
    { 0x00343ed0u, h_keep_state },
    { 0x003f38d8u, h_brake_heat },
    { 0x0035a678u, h_wheel_noise },
    { 0x00365e80u, h_gaps },
    { 0x00366588u, h_handover },
    { 0x0034db30u, h_step },
    { 0x00347a00u, h_drive_settings },
    { 0x003c0190u, h_race_begin },
    { 0x003c0208u, h_race_before },
    { 0x003c0310u, h_race_after },
    { 0x003c12b0u, h_records_reset },
    { 0x003b7470u, h_queue_reset },
    { 0x003b76f0u, h_queue_push },
    { 0x003b7668u, h_queue_take },
    { 0x003b77e0u, h_queue_peek },
};
#define N_NATIVES (int)(sizeof natives / sizeof natives[0])

/* func_004AC400 returns VU0's random register minus one (vrnext): used by
   effects, not by the dynamics. Stand-in with the same generator, seeded
   with a fixed value (the console's register is not in eeMemory.bin). */
static uint32_t vu0_r = 0x3f800001u;
static void h_vu0_random(ee_ctx *c)
{
    uint32_t x = (vu0_r >> 4) & 1, y = (vu0_r >> 22) & 1;
    vu0_r = (((vu0_r << 1) ^ x ^ y) & 0x007fffffu) | 0x3f800000u;
    c->fpr[0].f = gt4_f32(vu0_r) - 1.0f;
}

/* Calls per original function (OPENGT4_COVERAGE=file writes them). */
uint32_t *phys_coverage;

ee_fn (*phys_extra_hook)(uint32_t addr);

/* PHYS_STUB=addr,...: those original functions return at once (v0 0),
   to see whether the physics depends on them */
static void stub_fn(ee_ctx *c) { c->r[2].sd[0] = 0; }
static int is_stubbed(uint32_t addr)
{
    static const char *list;
    static int read;
    char name[16];
    if (!read) { read = 1; list = getenv("PHYS_STUB"); }
    if (!list) return 0;
    snprintf(name, sizeof name, "0x%06x", addr);
    return strstr(list, name) != NULL;
}

/* the last functions entered, for where a run ended */
static uint32_t recent[32], recent_game[32];
static unsigned recent_n, recent_game_n;
void phys_print_recent(void)
{
    for (unsigned k = 0; k < 32 && k < recent_n; k++) fprintf(stderr, " %08x", recent[(recent_n + k) % 32]);
    fprintf(stderr, "\n game:");
    for (unsigned k = 0; k < 32 && k < recent_game_n; k++) fprintf(stderr, " %08x", recent_game[(recent_game_n + k) % 32]);
    fprintf(stderr, "\n");
}

static ee_fn hook(uint32_t addr)
{
    ee_fn extra = phys_extra_hook ? phys_extra_hook(addr) : NULL;
    recent[recent_n++ % 32] = addr;
    if (addr < 0x00480000u) recent_game[recent_game_n++ % 32] = addr;
    if (is_stubbed(addr)) return stub_fn;
    if (phys_coverage && addr >= 0x00100000u && addr < 0x00620000u) phys_coverage[(addr - 0x00100000u) >> 2]++;
    if (extra) return extra;
    if (addr == 0x004ac400u) return h_vu0_random;
    for (int i = 0; i < N_NATIVES; i++)
        if (natives[i].addr == addr && natives[i].on) {
            natives[i].calls++;
            return natives[i].fn;
        }
    return NULL;
}

void phys_natives_select(const char *list)
{
    for (int i = 0; i < N_NATIVES; i++) {
        char name[16];
        snprintf(name, sizeof name, "0x%06x", natives[i].addr);
        natives[i].on = list && (!strcmp(list, "all") || strstr(list, name) != NULL);
    }
}

void phys_natives_install(ee_ctx *c)
{
    c->hook = hook;
    gt4_phys_mem = c->ram;
    gt4_phys_fpu_enter();
    gt4_orig_call = orig_call;
    if (getenv("PHYS_HEADLESS")) gt4_phys_headless = 1;   /* the step without its presentation */
    if (getenv("PHYS_UNVERIFIED")) gt4_phys_unverified = 1;   /* the rewrites no capture reached yet */
    bridge_ctx = c;
}

void phys_natives_report(void)
{
    for (int i = 0; i < N_NATIVES; i++)
        if (natives[i].on) printf("native 0x%08x: %ld calls\n", natives[i].addr, natives[i].calls);
}
