/*
 * opengt4_drive: drive the car of a PCSX2 savestate with the game's own
 * physics (transitional; docs/analysis/PHYSICS.md).
 *
 * The RAM of the savestate (eeMemory.bin, extracted locally) holds the race
 * as the game set it up: cars, their parameters, the course collision. Each
 * 1/60 s the simulation step runs: the parts rewritten in src/core/physics in
 * C, the rest still recompiled from the user's disc. The controller read
 * answers through the physics' host hooks (gt4_phys_pad): keyboard or a game controller. The car
 * is drawn on the course by the native renderer.
 *
 * Usage: opengt4_drive <disc.iso> <eeMemory.bin|-> [Scratchpad.bin]
 *        [--car ka_01] [--course tsukuba] [--elf core.elf] [--original] [--flip-steering] [--manual]
 *
 * With "-" instead of a snapshot the race is built from the executable's
 * image, unpacked from the disc (or --elf core.elf; tests/physics/race_build.c).
 *   --car/--course other than the snapshot's set the race up again with them
 *   (tests/physics/race_setup.c): physics from SpecDB and the course files.
 *   --arcade [--ai code,code,...]: a race against the AI (up to seven other
 *   cars, five by default) built the same way (race_build.c).
 *        [--auto frames --shot out.png]   (full throttle, then a capture; for checks)
 *        [--auto-shift n]   (with --auto: gear up every n steps)
 *
 * Keys: arrows (steer, throttle, brake), space (handbrake), A/Z (gear up,
 * down, with --manual), C/V (camera), R (back to the snapshot), Esc (quit).
 * A game controller: left stick, triggers, A (handbrake), shoulders (gears);
 * its rumble follows the game's vibration output. The keyboard is reported
 * as a digital pad (device kind 1: the game's slower easing of button
 * steering), the stick as an analog pad (2); --device n forces one.
 */
#include "ee.h"
#include "phys.h"
#include "car.h"
#include "natives.h"
#include "race_setup.h"
#include "race_build.h"
#include "specdb.h"
#include "spectables.h"
#include "vol.h"
#include "platform/platform.h"
#include "race_view.h"
#include "plat_sdl.h"
#include "miniz.h"

#include <SDL.h>
#include <fenv.h>
#include <math.h>
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE__)
#include <xmmintrin.h>
#define HAVE_MXCSR 1
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ee_fn recomp_lookup(uint32_t addr);
extern const ee_code_range recomp_code_ranges[];
extern const int recomp_n_code_ranges;

#define STEP        0x0034db30u
#define CONDUCTOR   0x01bf7860u   /* Time Trial snapshots; see PHYSICS.md */
#define CAR_OFFSET  0x6698u
#define STACK_TOP   0x01ef0000u
#define READ_PAD    0x00426d50u

static ee_ctx ctx;
static uint32_t conductor = CONDUCTOR;
static uint8_t *snapshot;

/* Driver command (32 bytes) as the controller read returns it: flags (bit 0
   steering analog, 1 throttle, 2 brake, 4 handbrake), [2] shift (1 up, -1
   down, while held), steering [4] -4096..4096, throttle [8], brake [0xa],
   handbrake [0xe] 0..4096. */
static struct { uint8_t flags; int8_t shift; int16_t steer, throttle, brake, handbrake; } command;
static int digital_steer = 4096;   /* value of a digital steering press */
static int device_kind = -1;       /* --device: the kind reported (default: 2 for a game controller, 1 for the keyboard) */
static int using_pad;
static uint32_t player_car;
static SDL_GameController *pad;

/* The player's car answered by SDL through the physics' host hooks
   (gt4_phys_pad); the other cars keep their controller objects (the AI). */
static int host_read(void *u, uint32_t car, uint8_t *cmd, const uint8_t *ev)
{
    (void)u;
    if (car != player_car) return 0;
    cmd[0] = command.flags;
    cmd[1] = (uint8_t)(device_kind >= 0 ? device_kind : using_pad ? 2 : 1);   /* 1 digital pad, 2 analog pad; 4 the GT Force, 6 another wheel */
    cmd[2] = (uint8_t)command.shift;
    cmd[3] = ev[3];                  /* the block's kind, as the controller echoes it */
    memcpy(cmd + 4, &command.steer, 2);
    memcpy(cmd + 8, &command.throttle, 2);
    memcpy(cmd + 0xa, &command.brake, 2);
    memcpy(cmd + 0xe, &command.handbrake, 2);
    return 1;
}

/* A pad has no steering wheel: the state stays as the game sets it. */
static int host_wheel(void *u, uint32_t car, int16_t state[2])
{
    (void)u;
    (void)state;
    return car == player_car;
}

/* The outputs: the large motor (0..127) to the controller's low-frequency
   rumble, renewed each step. */
static int host_write(void *u, uint32_t car, const uint8_t *out)
{
    (void)u;
    if (car != player_car) return 0;
    if (pad) SDL_GameControllerRumble(pad, (uint16_t)(out[1] > 127 ? 65535 : out[1] * 516), (uint16_t)(out[0] ? 32768 : 0), 100);
    return 1;
}

/* --original: the recompiled controller read replaced instead */
static void read_pad(ee_ctx *c)
{
    uint32_t out = c->r[5].uw[0], obj = c->r[4].uw[0];
    uint8_t keep = ee_r8(c, obj + 0x153);
    for (int k = 0; k < 32; k++) ee_w8(c, out + k, 0);
    ee_w8(c, out + 0, command.flags);
    ee_w8(c, out + 1, 2);   /* device kind: 2 analog pad (as in the snapshots); 4 is the GT Force wheel */
    ee_w8(c, out + 2, (uint8_t)command.shift);
    ee_w8(c, out + 3, keep);
    ee_w16(c, out + 4, (uint16_t)command.steer);
    ee_w16(c, out + 8, (uint16_t)command.throttle);
    ee_w16(c, out + 0xa, (uint16_t)command.brake);
    ee_w16(c, out + 0xe, (uint16_t)command.handbrake);
}

static ee_fn input_hook(uint32_t addr) { return addr == READ_PAD ? read_pad : NULL; }

static long read_file(const char *path, uint8_t *dst, size_t size)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f) return -1;
    n = (long)fread(dst, 1, size, f);
    fclose(f);
    return n;
}

/* The PS2 FPU mode (toward zero, no denormals) only while the step runs. */
static int step(void)
{
    jmp_buf exit_jmp;
    ee_fn fn = recomp_lookup(STEP);
    int ok = 1, round = fegetround();
#ifdef HAVE_MXCSR
    unsigned csr = _mm_getcsr();
#endif
    gt4_phys_fpu_enter();
    memset(ctx.r, 0, sizeof ctx.r);
    ctx.r[4].uw[0] = conductor;
    ctx.r[29].uw[0] = STACK_TOP;
    ctx.exit_jmp = &exit_jmp;
    if (setjmp(exit_jmp) != 0) {
        fprintf(stderr, "step ended at 0x%08x: %s\n", ctx.end_pc, ctx.end_detail);
        ok = 0;
    } else {
        fn(&ctx);
    }
    fesetround(round);
#ifdef HAVE_MXCSR
    _mm_setcsr(csr);
#endif
    return ok;
}

static float f32(uint32_t a) { float f; uint32_t v = ee_r32(&ctx, a); memcpy(&f, &v, 4); return f; }

/* Physics frame (x, y, z up) to the render frame (x, y up, z) of the course. */
static void to_render(const float v[3], float out[3]) { out[0] = v[0]; out[1] = v[2]; out[2] = -v[1]; }

static float ride_height(uint32_t car)
{
    float ground = 0;
    for (int i = 0; i < 4; i++) ground += f32(car + CAR_WHEELS + i * WHEEL_SIZE + WHEEL_8C);
    return f32(car + CAR_POSITION + 8) - ground * .25f;
}

static void pose(uint32_t car, float height, float pos[3], float rot[9])
{
    float p[3], a[3], up[3], fwd[3], u[3], c0[3], c2[3];
    for (int k = 0; k < 3; k++) {
        p[k] = f32(car + CAR_POSITION + 4 * k);
        a[k] = f32(car + CAR_MATRIX + 4 * k);          /* forward */
        up[k] = f32(car + CAR_MATRIX + 0x18 + 4 * k);  /* up */
    }
    for (int k = 0; k < 3; k++) p[k] -= up[k] * height;   /* model origin on the ground */
    to_render(p, pos);
    to_render(a, fwd);
    to_render(up, u);
    for (int k = 0; k < 3; k++) c2[k] = -fwd[k];
    c0[0] = u[1] * c2[2] - u[2] * c2[1];
    c0[1] = u[2] * c2[0] - u[0] * c2[2];
    c0[2] = u[0] * c2[1] - u[1] * c2[0];
    for (int r = 0; r < 3; r++) { rot[r * 3] = c0[r]; rot[r * 3 + 1] = u[r]; rot[r * 3 + 2] = c2[r]; }
}

/* The wheels for the drawing, per model slot (front, rear; the model's +x
   side first): steer, rotation and hub lift of the physics wheel whose
   mount is there. The lift is the suspension travel against the one at
   the start, where the model's wheels are. */
static float travel0[8][4];
static void wheel_poses(uint32_t car, int idx, const float rot[9], float out[4][3])
{
    memset(out, 0, 4 * sizeof *out);
    for (int i = 0; i < 4; i++) {
        uint32_t w = car + CAR_WHEELS + i * WHEEL_SIZE;
        float m[3], r[3], local[3];
        int slot;
        for (int k = 0; k < 3; k++) m[k] = f32(w + WHEEL_MOUNT + 4 * k);
        to_render(m, r);
        for (int k = 0; k < 3; k++) local[k] = rot[k] * r[0] + rot[3 + k] * r[1] + rot[6 + k] * r[2];
        slot = (local[2] > 0 ? 2 : 0) + (local[0] > 0 ? 0 : 1);
        out[slot][0] = f32(w + WHEEL_STEER);
        out[slot][1] = -f32(w + WHEEL_ROTATION);
        out[slot][2] = travel0[idx][i] - f32(w + WHEEL_TRAVEL);
    }
}

static float approach(float v, float target, float rate)
{
    if (v < target) return v + rate < target ? v + rate : target;
    return v - rate > target ? v - rate : target;
}

int main(int argc, char **argv)
{
    const char *car_code = "ka_01", *course = "tsukuba", *spr = NULL;
    int original = 0, flip = 0, running = 1, ready = 0, auto_frames = 0, steps = 0;
    const char *shot = NULL, *elf = NULL;   /* the executable: the disc's own unless --elf */
    int built = 0;
    float auto_steer = 0, start_orbit = 0;
    int auto_analog = 0;
    uint32_t car = CONDUCTOR + CAR_OFFSET, ncars = 1;
    float height[8], steer = 0, orbit = 0, zoom = 0;
    int arcade = 0;
    const char *ai = NULL;
    char ai_codes[7][32];
    int nai = 0;
    double acc = 0;
    uint64_t last;
    int manual = 0, auto_shift = 0;

    if (argc < 3) {
        fprintf(stderr, "usage: %s <disc.iso> <eeMemory.bin|-> [Scratchpad.bin] [--car code] [--course name] [--elf core.elf] [--original] [--flip-steering]\n", argv[0]);
        return 2;
    }
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--car") && i + 1 < argc) car_code = argv[++i];
        else if (!strcmp(argv[i], "--course") && i + 1 < argc) course = argv[++i];
        else if (!strcmp(argv[i], "--original")) original = 1;
        else if (!strcmp(argv[i], "--flip-steering")) flip = 1;
        else if (!strcmp(argv[i], "--auto") && i + 1 < argc) auto_frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot = argv[++i];
        else if (!strcmp(argv[i], "--auto-steer") && i + 1 < argc) auto_steer = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--auto-analog")) auto_analog = 1;
        else if (!strcmp(argv[i], "--orbit") && i + 1 < argc) start_orbit = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--digital-steer") && i + 1 < argc) digital_steer = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--elf") && i + 1 < argc) elf = argv[++i];
        else if (!strcmp(argv[i], "--manual")) manual = 1;
        else if (!strcmp(argv[i], "--device") && i + 1 < argc) device_kind = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--auto-shift") && i + 1 < argc) auto_shift = atoi(argv[++i]);   /* up every n steps */
        else if (!strcmp(argv[i], "--arcade")) arcade = 1;
        else if (!strcmp(argv[i], "--ai") && i + 1 < argc) ai = argv[++i];
        else spr = argv[i];
    }
    if (arcade) {
        /* the other cars: --ai, or the ones of the Arcade captures */
        const char *p = ai ? ai : "_1100_standard_type11_49,z_act_70,today_g_85,midget2_d_type_98,s500_63";
        while (p && *p && nai < 7) {
            size_t n = strcspn(p, ",");
            if (n >= sizeof ai_codes[0]) { fprintf(stderr, "car name too long\n"); return 2; }
            memcpy(ai_codes[nai], p, n);
            ai_codes[nai++][n] = 0;
            p = p[n] ? p + n + 1 : NULL;
        }
        if (strcmp(argv[2], "-")) { fprintf(stderr, "--arcade builds its race: use - for the snapshot\n"); return 2; }
    }
    if (ee_init(&ctx) != 0) return 2;
    if (!strcmp(argv[2], "-")) {
        /* no snapshot: the race built from the executable's image
           (tests/physics/race_build.c), set up to its end, stepped without
           the presentation (no sound or replay objects here) */
        char msg[256];
        uint32_t cond;
        if (!(arcade ? phys_race_build_arcade(ctx.ram, elf ? elf : argv[1], 1 + nai, &cond, msg, sizeof msg)
                     : phys_race_build(ctx.ram, elf ? elf : argv[1], &cond, msg, sizeof msg))) {
            fprintf(stderr, "race build: %s\n", msg);
            return 2;
        }
        built = 1;
        conductor = cond;
        car = cond + CAR_OFFSET;
        phys_race_full_init = 1;
        gt4_phys_headless = 1;
    } else if (read_file(argv[2], ctx.ram, EE_RAM_SIZE) != (long)EE_RAM_SIZE) {
        fprintf(stderr, "cannot read %s\n", argv[2]);
        return 2;
    }
    if (spr && read_file(spr, ctx.spr, EE_SPR_SIZE) != (long)EE_SPR_SIZE) {
        fprintf(stderr, "cannot read %s\n", spr);
        return 2;
    }
    if (!(snapshot = malloc(EE_RAM_SIZE))) return 2;
    memcpy(snapshot, ctx.ram, EE_RAM_SIZE);
    ctx.lookup = recomp_lookup;
    ctx.code_ranges = recomp_code_ranges;
    ctx.n_code_ranges = recomp_n_code_ranges;
    ctx.cop0[EE_COP0_STATUS] = 0x70030c13u;
    phys_natives_select(original ? NULL : "all");
    if (original) {
        phys_extra_hook = input_hook;
    } else {
        player_car = car;
        gt4_phys_pad.read = host_read;
        gt4_phys_pad.wheel_state = host_wheel;
        gt4_phys_pad.write = host_write;
        gt4_phys_scratch = STACK_TOP - 0x10000u;   /* below the stack the step uses */
        gt4_phys_specdb = &phys_memory_src;        /* the tables in memory; a built race's below */
    }
    phys_natives_install(&ctx);
    if (built || strcmp(car_code, "ka_01") || strcmp(course, "tsukuba")) {
        /* another car or course: set the snapshot's race up again with them,
           through the whole init (what follows the cars builds the walls'
           outlines of this course; without it the snapshot course's stay
           and the car drives through this one's walls) */
        const char *err = NULL;
        phys_race_full_init = 1;
        gt4_vol *vol = gt4_vol_open(argv[1], &err);
        int32_t id = -1;
        char msg[160];
        if (!vol) { fprintf(stderr, "cannot open %s: %s\n", argv[1], err ? err : "?"); return 2; }
        if (built || strcmp(car_code, "ka_01")) {
            size_t n = 0;
            uint8_t *labels = gt4_vol_read(vol, "/specdb/GT4_US2560/GENERIC_CAR.idi", &n);
            uint32_t v;
            if (!labels || !gt4_specdb_label(labels, n, car_code, &v)) { fprintf(stderr, "unknown car %s\n", car_code); return 2; }
            id = (int32_t)v;
            for (int k = 0; k < nai; k++) {
                if (!gt4_specdb_label(labels, n, ai_codes[k], &v)) { fprintf(stderr, "unknown car %s\n", ai_codes[k]); return 2; }
                phys_race_other_cars[k] = (int32_t)v;
            }
            free(labels);
        }
        if (built) {
            /* the SpecDB rows from the disc: a built race has none in memory
               (the source and the volume stay open while the demo runs) */
            static gt4_specdb_src disc;
            disc.row = gt4_spec_tables_row;
            disc.ctx = gt4_spec_tables_open(vol, NULL);
            disc.str = gt4_spec_tables_str;
            disc.car_parts = gt4_spec_tables_car_parts;
            phys_record_src = &disc;
            gt4_phys_specdb = &disc;
        }
        if (!phys_race_setup(&ctx, vol, conductor, built || strcmp(course, "tsukuba") ? course : NULL, id, msg, sizeof msg)) {
            fprintf(stderr, "race setup: %s\n", msg);
            return 2;
        }
        printf("race set up for %s on %s%s%s\n", car_code, course, msg[0] ? ": " : "", msg);
        memcpy(snapshot, ctx.ram, EE_RAM_SIZE);
        if (!built) gt4_vol_close(vol);
    }
    /* ee_init and the install leave the PS2 mode on; the step sets it itself */
    fesetround(FE_TONEAREST);
#ifdef HAVE_MXCSR
    _mm_setcsr(_mm_getcsr() & ~0x8040u);
#endif
    if (manual) {                        /* manual gearbox: driver kind 4 (3 takes the clutch on the brake pedal) */
        ee_w8(&ctx, car + CAR_546, 4);
        memcpy(snapshot, ctx.ram, EE_RAM_SIZE);
    }
    ncars = ee_r32(&ctx, conductor + 0xf8a0u);
    if (ncars < 1 || ncars > 8) ncars = 1;
    for (uint32_t k = 0; k < ncars; k++) {
        uint32_t c = car + k * CAR_SIZE;
        height[k] = ride_height(c);
        for (int i = 0; i < 4; i++) travel0[k][i] = f32(c + CAR_WHEELS + i * WHEEL_SIZE + WHEEL_TRAVEL);
    }

    plat_video_desc desc = { 1280, 896, 0, 1, 1 };
    if (plat_video_init(&desc) != 0) {
        fprintf(stderr, "cannot open a window: %s\n", SDL_GetError());
        return 1;
    }
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    for (int i = 0; i < SDL_NumJoysticks() && !pad; i++)
        if (SDL_IsGameController(i)) pad = SDL_GameControllerOpen(i);
    plat_video_set_view(640, 448);
    race_view *s = race_view_open(argv[1]);
    {
        const char *names[7];
        for (int k = 0; k < nai; k++) names[k] = ai_codes[k];
        if (s && nai) race_view_set_opponents(s, names, NULL, nai);
    }
    if (!s || !race_view_request(s, course, car_code, 0)) {
        fprintf(stderr, "cannot load course %s\n", course);
        return 1;
    }
    printf("physics: %s; %s\n", original ? "original only" : "rewritten parts in C, the rest original",
           pad ? SDL_GameControllerName(pad) : "keyboard");

    last = plat_time_ns();
    while (running) {
        SDL_Event ev;
        uint64_t now = plat_time_ns();
        double dt = (now - last) / 1e9;
        const uint8_t *keys;
        float want_steer, throttle, brake, hand;
        last = now;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) running = 0;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_r) memcpy(ctx.ram, snapshot, EE_RAM_SIZE);
        }
        if (!ready) {
            int r = race_view_poll(s, 0);
            if (r < 0) { fprintf(stderr, "cannot load course %s\n", course); break; }
            if (r > 0) { ready = 1; race_view_camera(s, start_orbit, 0); }
        }

        /* Controls. The keyboard is a digital pad (the buttons as they are:
           the game's own digital handling eases steering, throttle and
           brake); a game controller is an analog pad. */
        keys = SDL_GetKeyboardState(NULL);
        want_steer = (float)(keys[SDL_SCANCODE_LEFT] - keys[SDL_SCANCODE_RIGHT]);
        throttle = keys[SDL_SCANCODE_UP] ? 1.f : 0.f;
        brake = keys[SDL_SCANCODE_DOWN] ? 1.f : 0.f;
        hand = keys[SDL_SCANCODE_SPACE] ? 1.f : 0.f;
        command.shift = (int8_t)(keys[SDL_SCANCODE_A] - keys[SDL_SCANCODE_Z]);
        if (pad) command.shift = (int8_t)(command.shift + SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)
                                          - SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER));
        steer = want_steer;
        command.flags = 0;
        if (auto_frames) {
            throttle = 1;
            steer = auto_steer;
            command.flags = auto_analog ? 0x17 : 0;
            if (auto_shift && steps % auto_shift < 6) command.shift = 1;
        }
        if (pad) {
            float x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) / 32767.f;
            float rt = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 32767.f;
            float lt = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 32767.f;
            if (fabsf(x) > .08f || rt > .05f || lt > .05f) {
                command.flags = 0x17;
                steer = fabsf(x) > .08f ? -x : 0;
                throttle = rt > .05f ? rt : 0;
                brake = lt > .05f ? lt : 0;
            }
            if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A)) hand = 1;
        }
        using_pad = command.flags != 0;
        if (flip) steer = -steer;
        if (command.flags) {
            command.steer = (int16_t)(steer * 4096);
            command.throttle = (int16_t)(throttle * 4096);
            command.brake = (int16_t)(brake * 4096);
            command.handbrake = (int16_t)(hand * 4096);
        } else {
            command.steer = (int16_t)(steer * digital_steer);
            command.throttle = throttle > 0;
            command.brake = brake > 0;
            command.handbrake = hand > 0;
        }
        orbit = (float)(keys[SDL_SCANCODE_V] - keys[SDL_SCANCODE_C]) * (float)dt;
        zoom = (float)(keys[SDL_SCANCODE_PAGEDOWN] - keys[SDL_SCANCODE_PAGEUP]) * (float)dt * 3;

        /* the simulation at its own 60 Hz */
        acc += dt;
        if (acc > .1) acc = .1;
        while (ready && acc >= 1.0 / 60) {
            if (!step()) { running = 0; break; }
            acc -= 1.0 / 60;
            if (++steps % 60 == 0)
                printf("t %4.0fs  %6.1f km/h  %5d rpm  gear %d  pos (%.1f, %.1f, %.2f)\n", steps / 60.0,
                       f32(car + CAR_SPEED) * 3.6f, (int16_t)ee_r16(&ctx, car + CAR_ENGINE_RPM), ee_r8(&ctx, car + CAR_GEAR),
                       f32(car + CAR_POSITION), f32(car + CAR_POSITION + 4), f32(car + CAR_POSITION + 8));
            if (auto_frames && steps % (getenv("DRIVE_LOG_EVERY") ? atoi(getenv("DRIVE_LOG_EVERY")) : 20) == 0)
                fprintf(stderr, "steer cmd %d  56c %.4f  w0 %.4f  yaw %.4f  kind %d 56a %d 56b %d btn %d gear %d\n", command.steer, f32(car + CAR_56C), f32(car + CAR_WHEELS), f32(car + CAR_ANGLES + 8), ee_r8(&ctx, car + CAR_546), ee_r8(&ctx, car + 0x56a), ee_r8(&ctx, car + 0x56b), (int8_t)ee_r8(&ctx, car + 0x567), ee_r8(&ctx, car + CAR_GEAR));
        }

        plat_video_begin_frame();
        if (ready) {
            float pos[3], rot[9];
            char title[160];
            pose(car, height[0], pos, rot);
            race_view_set_pose(s, pos, rot);
            {
                float wheels[4][3];
                wheel_poses(car, 0, rot, wheels);
                race_view_set_wheels(s, wheels);
            }
            for (uint32_t k = 1; k < ncars; k++) {
                float p2[3], r2[9], wheels[4][3];
                uint32_t c = car + k * CAR_SIZE;
                pose(c, height[k], p2, r2);
                wheel_poses(c, (int)k, r2, wheels);
                race_view_set_opponent(s, (int)k - 1, p2, r2, wheels);
            }
            race_view_camera(s, orbit, zoom);
            race_view_draw(s);
            snprintf(title, sizeof title, "OpenGT4 drive | %3.0f km/h | %5d rpm | marcha %d | setas, espaco, C/V camera, R reinicia, Esc sai",
                     fabsf(f32(car + CAR_SPEED)) * 3.6f, (int16_t)ee_r16(&ctx, car + CAR_ENGINE_RPM), ee_r8(&ctx, car + CAR_GEAR));
            SDL_SetWindowTitle(SDL_GetWindowFromID(1), title);
            if (auto_frames && steps >= auto_frames) {
                if (shot) {
                    int w = 0, h = 0;
                    unsigned char *px = plat_sdl_read_pixels(&w, &h);
                    size_t len = 0;
                    void *png = px ? tdefl_write_image_to_png_file_in_memory(px, w, h, 4, &len) : NULL;
                    FILE *f = png ? fopen(shot, "wb") : NULL;
                    if (f) { fwrite(png, 1, len, f); fclose(f); printf("saved %s\n", shot); }
                    mz_free(png);
                    free(px);
                }
                running = 0;
            }
        }
        plat_video_end_frame();
    }
    race_view_close(s);
    plat_video_shutdown();
    return 0;
}
