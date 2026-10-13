/* The Time Trial's ghost through the game's own race (race_game: its
   controller, frames and start), not the bare physics of tt_build: the
   player's car follows the course's line as tt_build --ghost drives it,
   and the ghost of lap 2 must be where the car was in lap 1, step by step.
   --saved: lap 1 saved as a best-lap replay (race_game_ghost_export, the
   file's packing and back), the session's ghost forgotten, the file's
   loaded (race_game_ghost_import) and the race started again: its ghost
   must drive lap 1 of the first start.
   usage: tt_game_probe <disc> <course> <car> [--saved] [--negative] (hidden window) */
#include <SDL.h>
#include "plat_sdl.h"
#include "platform/platform.h"
#include "../../src/app/race_game.c"

static int (*game_read)(void *, uint32_t, uint8_t *, const uint8_t *);
static float drive_held;
/* the line driver of tt_build: the command the game reads for the player,
   set before the game's reader takes it (and keeps it on its tape) */
static int probe_read(void *u, uint32_t car, uint8_t *cmd, const uint8_t *ev)
{
    race_game *g = playing;
    float p[3], yaw, t[3], steer = 0, throttle = 0;
    if (g && car == player && gt4_race_run_place(g->run, 0, p, &yaw) && gt4_race_run_course_point(g->run, GT4_F32(car + 0x5a8) + 45, t)) {
        float fx = -sinf(yaw), fz = -cosf(yaw), dx = t[0] - p[0], dz = t[2] - p[2];
        float want = fmaxf(-1, fminf(1, -0.8f * atan2f(fx * dz - fz * dx, fx * dx + fz * dz)));
        drive_held += fmaxf(-0.04f, fminf(0.04f, want - drive_held));
        steer = drive_held;
        throttle = fabsf(steer) > .8f ? .5f : 1.f;
    }
    command.shift = 0;
    command.brake = command.handbrake = 0;
    if (getenv("PROBE_MANUAL") && car == player) {   /* the gears by hand: up near the top, down when slow, one press held 4 steps */
        static int held;
        int rpm = (int16_t)GT4_U16(car + CAR_ENGINE_RPM), gear = GT4_U8(car + CAR_GEAR);
        if (held > 0) { command.shift = (int8_t)(held > 100 ? -1 : 1); if (--held == 100) held = 0; }
        else if (rpm > 7000 && gear < 6) held = 4;
        else if (rpm < 3500 && gear > 2) held = 104;
    }
    if (getenv("PROBE_DIGITAL")) {   /* the keyboard's: the wheel full or straight, the throttle on or off (read_controls) */
        /* the wheel held while the line asks for more than a little: the
           game ramps a digital wheel itself */
        float aim = 0;
        if (gt4_race_run_place(g->run, 0, p, &yaw) && gt4_race_run_course_point(g->run, GT4_F32(car + 0x5a8) + 25, t)) {
            float fx = -sinf(yaw), fz = -cosf(yaw), dx = t[0] - p[0], dz = t[2] - p[2];
            aim = -atan2f(fx * dz - fz * dx, fx * dx + fz * dz);
        }
        int want = aim > .03f ? 1 : aim < -.03f ? -1 : 0;
        throttle = fabsf(aim) > .15f ? 0 : 1;
        command.flags = 0;
        command.device = 1;
        command.steer = (int16_t)(want * 4096);
        command.throttle = throttle > .75f;
    } else {
        command.flags = 0x17;
        command.device = 2;
        command.steer = (int16_t)(steer * 4096);
        command.throttle = (int16_t)(throttle * 4096);
    }
    return game_read(u, car, cmd, ev);
}

int main(int argc, char **argv)
{
    if (argc < 4) return 2;
    int negative = 0, saved = 0;
    for (int a = 4; a < argc; a++) {
        if (!strcmp(argv[a], "--negative")) negative = 1;
        if (!strcmp(argv[a], "--saved")) saved = 1;
    }
    plat_video_desc desc = { 640, 448, 0, 0, 0 };
    if (plat_video_init(&desc)) return 2;
    SDL_HideWindow(SDL_GetWindowFromID(1));
    plat_video_set_view(640, 448);
    race_game *g = race_game_open(argv[1]);
    char msg[256];
    if (!g) return 2;
    race_game_set_time_trial(g, 1);
    /* the driving options as the menu gives them (main.c: TCS on unless set) */
    race_game_set_driving(g, getenv("PROBE_TIRE") ? atoi(getenv("PROBE_TIRE")) : -1, getenv("PROBE_MANUAL") != NULL,
                          getenv("PROBE_ASM") ? atoi(getenv("PROBE_ASM")) : 0, getenv("PROBE_TCS") ? atoi(getenv("PROBE_TCS")) : 0);
    if (!race_game_start(g, argv[2], argv[3], 0, NULL, NULL, 0, msg, sizeof msg)) { fprintf(stderr, "start: %s\n", msg); return 1; }
    int ready = 0;
    for (int i = 0; i < 4000 && !ready; i++) { SDL_PumpEvents(); SDL_Delay(5); ready = race_game_poll(g); }
    if (ready != 1) { fprintf(stderr, "the race did not load\n"); return 1; }
    game_read = gt4_phys_pad.read;
    gt4_phys_pad.read = probe_read;
    enum { MAXF = 60 * 60 * 6 };
    static float lap1[MAXF][3];
    int n1 = 0, k2 = -1, bad = 0;
    double worst = 0;
    uint32_t car0 = gt4_race_run_car(g->run, 0);
    for (int f = 0; f < MAXF * 3; f++) {
        uint16_t before = GT4_U16(car0 + 0x104 + 0x4ac);
        race_game_update(g, STEP);   /* one step a call, as a frame at 60 Hz */
        if (!g->run) { fprintf(stderr, "the race stopped\n"); return 1; }
        uint16_t lap = GT4_U16(car0 + 0x104 + 0x4ac);
        if (lap != before) {
            fprintf(stderr, "frame %d: lap %u -> %u, ghost %s\n", f, before, lap, gt4_race_run_ghost_on(g->run) ? "on" : "off");
            if (lap == 3 || (saved && lap == 2)) break;
            if (lap == 2) k2 = 0;
        }
        if (lap == 1 && n1 < MAXF) memcpy(lap1[n1++], gt4_ptr(car0 + CAR_POSITION), 12);
        if (lap == 2 && k2 >= 0) {
            /* a ghost started behind the step (its fraction below the
               recording's) took one step of its own first (func_0035ABA8) */
            static int ahead = -1;   /* 0 or 1, from the lap's first step */
            if (ahead < 0) {
                float p[3], d[2];
                memcpy(p, gt4_ptr(car0 + CAR_SIZE + CAR_POSITION), 12);
                for (int s = 0; s < 2; s++) { d[s] = 0; for (int c = 0; c < 3; c++) d[s] += (p[c] - lap1[s][c]) * (p[c] - lap1[s][c]); }
                ahead = d[1] < d[0];
                fprintf(stderr, "the ghost starts %s the lap's first step\n", ahead ? "one step past" : "at");
            }
            int k = k2++ + (negative ? 30 : 0) + ahead;
            if (k < n1 && gt4_race_run_ghost_on(g->run)) {
                float p[3];
                memcpy(p, gt4_ptr(car0 + CAR_SIZE + CAR_POSITION), 12);
                double e = sqrt((p[0] - lap1[k][0]) * (p[0] - lap1[k][0]) + (p[1] - lap1[k][1]) * (p[1] - lap1[k][1]) +
                                (p[2] - lap1[k][2]) * (p[2] - lap1[k][2]));
                if (e > worst) worst = e;
                if (e > 0.01 && !bad) { fprintf(stderr, "step %d of the lap: the ghost %.3f m from the first lap\n", k2 - 1, e); bad = 1; }
            }
        }
    }
    if (saved) {   /* the best lap saved and loaded: a new start, its ghost from the file */
        gt4_replay_file r, back;
        uint8_t *packed;
        size_t bytes;
        if (!race_game_ghost_export(g, &r) || !gt4_replay_pack(&r, &packed, &bytes) || !gt4_replay_unpack(packed, bytes, &back)) {
            fprintf(stderr, "FAIL no best lap to save\n");
            return 1;
        }
        free(packed);
        fprintf(stderr, "saved the best lap: %zu steps, %u ms, %zu bytes\n", back.steps, back.best_lap, bytes);
        if (getenv("PROBE_GHOST_DIR")) {   /* tests: the file kept in that save folder's replays/ */
            char name[64];
            gt4_save_init(getenv("PROBE_GHOST_DIR"));
            if (gt4_replay_store(&r, name, sizeof name)) fprintf(stderr, "stored %s\n", name);
        }
        race_game_stop(g);
        g->ghost.on = 0;   /* the session's own forgotten: only the file's */
        if (!race_game_ghost_import(g, &back)) { fprintf(stderr, "FAIL the best lap was not taken back\n"); return 1; }
        gt4_replay_free(&back);
        gt4_phys_pad.read = game_read;
        if (!race_game_start(g, argv[2], argv[3], 0, NULL, NULL, 0, msg, sizeof msg)) { fprintf(stderr, "start: %s\n", msg); return 1; }
        ready = 0;
        for (int i = 0; i < 4000 && !ready; i++) { SDL_PumpEvents(); SDL_Delay(5); ready = race_game_poll(g); }
        if (ready != 1) { fprintf(stderr, "the race did not load again\n"); return 1; }
        game_read = gt4_phys_pad.read;
        gt4_phys_pad.read = probe_read;
        drive_held = 0;
        car0 = gt4_race_run_car(g->run, 0);
        k2 = -1;
        bad = 0;
        worst = 0;
        int ahead = -1;
        for (int f = 0; f < MAXF; f++) {
            uint16_t before = GT4_U16(car0 + 0x104 + 0x4ac);
            race_game_update(g, STEP);
            if (!g->run) { fprintf(stderr, "the race stopped\n"); return 1; }
            uint16_t lap = GT4_U16(car0 + 0x104 + 0x4ac);
            if (lap != before) {
                fprintf(stderr, "again, frame %d: lap %u -> %u, ghost %s\n", f, before, lap, gt4_race_run_ghost_on(g->run) ? "on" : "off");
                if (lap == 2) break;
                if (lap == 1) k2 = 0;
            }
            if (lap == 1 && k2 >= 0 && gt4_race_run_ghost_on(g->run)) {
                float p[3];
                memcpy(p, gt4_ptr(car0 + CAR_SIZE + CAR_POSITION), 12);
                if (ahead < 0) {
                    float d[2];
                    for (int s = 0; s < 2; s++) { d[s] = 0; for (int c = 0; c < 3; c++) d[s] += (p[c] - lap1[s][c]) * (p[c] - lap1[s][c]); }
                    ahead = d[1] < d[0];
                }
                int k = k2++ + (negative ? 30 : 0) + ahead;
                if (k < n1) {
                    double e = sqrt((p[0] - lap1[k][0]) * (p[0] - lap1[k][0]) + (p[1] - lap1[k][1]) * (p[1] - lap1[k][1]) +
                                    (p[2] - lap1[k][2]) * (p[2] - lap1[k][2]));
                    if (e > worst) worst = e;
                    if (e > 0.01 && !bad) { fprintf(stderr, "step %d of the lap: the loaded ghost %.3f m from the saved lap\n", k2 - 1, e); bad = 1; }
                }
            }
        }
        if (k2 < 600) { fprintf(stderr, "FAIL the loaded ghost did not drive (%d steps)\n", k2); return 1; }
        if (bad) { fprintf(stderr, "FAIL the loaded ghost leaves the saved lap (worst %.3f m)\n", worst); return 1; }
        printf("PASS the saved best lap, loaded in a new start, drives the lap again: %d steps, worst %.4f m\n", k2, worst);
        return 0;
    }
    if (n1 < 600 || k2 < 600) { fprintf(stderr, "FAIL the laps did not run (lap 1 %d steps, lap 2 %d)\n", n1, k2); return 1; }
    if (bad) { fprintf(stderr, "FAIL the ghost leaves the first lap in the game (worst %.3f m)\n", worst); return 1; }
    printf("PASS the ghost drives the first lap again in the game: %d steps, worst %.4f m\n", n1 < k2 ? n1 : k2, worst);
    return 0;
}
