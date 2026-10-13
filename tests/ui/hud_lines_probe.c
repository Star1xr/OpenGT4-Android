/* The race display's timing lines (RaceDisplay +0x17BC) through the game's
   race: an Arcade race with five rivals, the player's car following the
   course's line; at the first section the player passes, the queue's
   section (0xE), time (0x10) and gap (0x11) events of that step reach the
   display, and a frame half a second later is saved with its HUD.
   usage: hud_lines_probe <disc> <course> <out.png> [--negative] [--lapped]
   (--negative: no events handed over, so no line; must fail. --lapped:
   the player at a quarter of the throttle, five laps, until the leader laps it, and
   the laps-behind line (0x12, "+1 LAP") instead of the gap) */
#include <SDL.h>
#include "plat_sdl.h"
#include "platform/platform.h"
#include "miniz.h"
#include "../../src/app/race_game.c"

static int (*game_read)(void *, uint32_t, uint8_t *, const uint8_t *);
static float drive_held, throttle_most = 1;
static int16_t wanted = 0x11;
static int probe_read(void *u, uint32_t car, uint8_t *cmd, const uint8_t *ev)
{
    race_game *g = playing;
    float p[3], yaw, t[3], steer = 0, throttle = 0;
    if (g && car == player && gt4_race_run_place(g->run, 0, p, &yaw) && gt4_race_run_course_point(g->run, GT4_F32(car + 0x5a8) + 45, t)) {
        float fx = -sinf(yaw), fz = -cosf(yaw), dx = t[0] - p[0], dz = t[2] - p[2];
        float want = fmaxf(-1, fminf(1, -0.8f * atan2f(fx * dz - fz * dx, fx * dx + fz * dz)));
        drive_held += fmaxf(-0.04f, fminf(0.04f, want - drive_held));
        steer = drive_held;
        throttle = fminf(fabsf(steer) > .8f ? .5f : 1.f, throttle_most);
    }
    command.flags = 0x17;
    command.device = 2;
    command.shift = 0;
    command.steer = (int16_t)(steer * 4096);
    command.throttle = (int16_t)(throttle * 4096);
    command.brake = command.handbrake = 0;
    return game_read(u, car, cmd, ev);
}

/* whether this step left a fresh message of the wanted kind (0x11 or 0x12) of the player */
static int player_gap(race_game *g)
{
    uint32_t q = GT4_U32(gt4_race_run_conductor(g->run)) + 0xf4;
    for (uint32_t i = 0; i < GT4_U32(q + 0x824) && i < 0x100; i++) {
        uint32_t e = q + 8 + i * 8;
        if ((int16_t)GT4_U16(e) == wanted && GT4_U16(e + 2) == 2 && !(GT4_U32(e + 4) & 15)) return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 4) return 2;
    int negative = 0;
    for (int a = 4; a < argc; a++) {
        if (!strcmp(argv[a], "--negative")) negative = 1;
        if (!strcmp(argv[a], "--lapped")) { throttle_most = .25f; wanted = 0x12; }
    }
    plat_video_desc desc = { 640, 448, 0, 0, 0 };
    if (plat_video_init(&desc)) return 2;
    SDL_HideWindow(SDL_GetWindowFromID(1));
    plat_video_set_view(640, 448);
    race_game *g = race_game_open(argv[1]);
    char msg[256];
    static const char *const others[] = { "z_act_70", "today_g_85", "midget2_d_type_98", "s500_63", "ka_01" };
    if (g && wanted == 0x12) race_game_set_laps(g, 5);   /* room for the leader to come round */
    if (!g || !race_game_start(g, argv[2], "ka_01", 0, others, NULL, 5, msg, sizeof msg)) { fprintf(stderr, "start: %s\n", msg); return 1; }
    int ready = 0;
    for (int i = 0; i < 4000 && !ready; i++) { SDL_PumpEvents(); SDL_Delay(5); ready = race_game_poll(g); }
    if (ready != 1) { fprintf(stderr, "the race did not load\n"); return 1; }
    game_read = gt4_phys_pad.read;
    gt4_phys_pad.read = probe_read;
    int seen = -1;
    for (int f = 0; f < 60 * 60 * 8; f++) {
        if (negative) {   /* the events never reach the display */
            race_hud *keep = g->hud;
            g->hud = NULL;
            race_game_update(g, STEP);
            g->hud = keep;
            if (keep) race_hud_draw(keep, &(race_hud_state){ 0 }, (float)STEP);
        } else race_game_update(g, STEP);
        if (!g->run) { fprintf(stderr, "the race stopped\n"); return 1; }
        if (seen < 0 && player_gap(g)) { seen = f; fprintf(stderr, "frame %d: the player's first message %#x\n", f, wanted); }
        if (seen >= 0 && f == seen + 30) break;
        if (f % 2) continue;
        plat_video_begin_frame();   /* the display keeps its own clock by its draws */
        race_game_draw(g);
        race_game_draw_hud(g, STEP * 2);
        plat_video_end_frame();
    }
    if (seen < 0) { fprintf(stderr, "FAIL no message %#x from the race\n", wanted); return 1; }
    plat_video_begin_frame();
    race_game_draw(g);
    race_game_draw_hud(g, STEP);
    int w, h;
    size_t len;
    uint8_t *p = plat_sdl_read_pixels(&w, &h);
    void *png = p ? tdefl_write_image_to_png_file_in_memory(p, w, h, 4, &len) : NULL;
    FILE *o = png ? fopen(argv[3], "wb") : NULL;
    if (o) { fwrite(png, 1, len, o); fclose(o); }
    /* the line (352, 164) in its colour: some red (0xC8, 0, 0) or, for a
       gap, light blue (0x96, 0xB8, 0xE3) pixels in a box around it (the
       player behind: red only, the sky being light blue too) */
    int hits = 0;
    for (int y = (int)(150 * h / 480.f); y < (int)(172 * h / 480.f); y++)
        for (int x = (int)(290 * w / 640.f); x < (int)(420 * w / 640.f); x++) {
            const uint8_t *c = p + ((size_t)y * w + x) * 4;
            if ((c[0] > 170 && c[1] < 40 && c[2] < 40) || (wanted == 0x11 && c[2] > 200 && c[1] > 160 && c[1] < 210 && c[0] > 120 && c[0] < 180)) hits++;
        }
    free(p);
    mz_free(png);
    plat_video_end_frame();
    if (hits < 20) { fprintf(stderr, "FAIL no line on the display (%d pixels)\n", hits); return 1; }
    printf("PASS the %s line drawn (%d pixels), %s\n", wanted == 0x11 ? "gap" : "laps-behind", hits, argv[3]);
    return 0;
}
