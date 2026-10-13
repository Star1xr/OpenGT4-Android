/* The tyres' smoke through the game's race: an Arcade race, the player's
   car flat out after the rolling start, then at full lock with the
   handbrake so it slides; the live particles are counted each step and
   the frame saved once there are 40. The smoke's pixels are told from the
   frame drawn without it (the same moment, the smoke unset; the player's
   car not drawn in either, so it does not hide what is behind it).
   --leaves (Autumn Ring): the car driven at the nearest triangle marked
   for leaves flat out, wheels spinning, the frame saved once four leaves
   are in the air, the leaves' pixels told the same way.
   usage: smoke_probe <disc> <course> <out.png> [--car <label>] [--leaves] [--negative]
   (the Ford GT by default: a car that spins its wheels on tarmac)
   (--negative: the wheels' blocks never reach the smoke or the leaves;
   must fail)
   SMOKE_LOG=1 prints the player's wheels' blocks, SMOKE_TEX=<png> the
   texture as decoded, SMOKE_DIFF=<png> the smoke's pixels eight times */
#include <SDL.h>
#include "plat_sdl.h"
#include "platform/platform.h"
#include "miniz.h"
#include "tex1.h"
#include "../../src/app/race_game.c"

static int (*game_read)(void *, uint32_t, uint8_t *, const uint8_t *);
static int step_no;
/* --leaves: the course's collision triangles marked 0x20 (their centres),
   the player's car driven at the nearest one flat out */
static float (*leafy)[3];
static int leafy_n;
static void find_leafy(const gt4_course *c)
{
    const uint8_t *d = c->data, *rw = d + c->runway;
    uint32_t t, l, v, s, end = 0xffffffffu;
    memcpy(&t, rw + 0x78, 4); memcpy(&l, rw + 0x7c, 4); memcpy(&v, rw + 0x74, 4); memcpy(&s, rw + 0x80, 4);
    if (l > t && l < end) end = l;
    if (v > t && v < end) end = v;
    if (s > t && s < end) end = s;
    leafy = malloc((end - t) / 12 * sizeof *leafy);
    for (uint32_t i = 0; leafy && i < (end - t) / 12; i++) {
        const uint8_t *tr = rw + t + i * 12;
        if (!(tr[7] & 0x20)) continue;
        for (int j = 0; j < 3; j++) leafy[leafy_n][j] = 0;
        for (int k = 0; k < 3; k++) {
            float p[3];
            memcpy(p, rw + v + (uint32_t)(tr[k * 2] | tr[k * 2 + 1] << 8) * 16, sizeof p);
            for (int j = 0; j < 3; j++) leafy[leafy_n][j] += p[j] / 3;
        }
        leafy_n++;
    }
}
static int probe_read(void *u, uint32_t car, uint8_t *cmd, const uint8_t *ev)
{
    int turn = step_no >= 960;   /* the rolling start's auto drive (to about step 850), then two
                                    seconds flat out, then full lock and the handbrake */
    float steer = 1, p[3], yaw;
    command.flags = 0x17;
    command.device = 2;
    command.shift = 0;
    command.brake = 0;
    command.handbrake = car == player && turn ? 4096 : 0;
    if (leafy_n && car == player && playing && gt4_race_run_place(playing->run, 0, p, &yaw)) {
        int best = 0;
        float bd = 1e30f;
        for (int i = 0; i < leafy_n; i++) {
            float dx = leafy[i][0] - p[0], dz = leafy[i][2] - p[2];
            if (dx * dx + dz * dz < bd) { bd = dx * dx + dz * dz; best = i; }
        }
        {
            float fx = -sinf(yaw), fz = -cosf(yaw), dx = leafy[best][0] - p[0], dz = leafy[best][2] - p[2];
            steer = fmaxf(-1, fminf(1, -0.8f * atan2f(fx * dz - fz * dx, fx * dx + fz * dz)));
        }
        command.handbrake = 0;
        turn = step_no >= 850;
    }
    command.steer = car == player && turn ? (int16_t)(steer * 4096) : 0;
    command.throttle = car == player ? 4096 : 0;
    return game_read(u, car, cmd, ev);
}

static void log_wheel(void *user, uint32_t car, uint32_t e)
{
    if (getenv("SMOKE_LOG") && step_no % 30 == 0 && car == gt4_race_run_car(((race_game *)user)->run, 0)) {
        gt4_race_hud hd = { 0 };
        gt4_race_run_hud(((race_game *)user)->run, 0, &hd);
        fprintf(stderr, "step %d wheel %u surface %u amount %.4f shade %.3f speed %.1f start %d\n", step_no, GT4_U32(e), GT4_U32(e + 4), GT4_F32(e + 0x2c), GT4_F32(e + 0x18), hd.speed, hd.start_frames);
    }
    smoke_wheel(user, car, e);
}

static int leaf_calls;
static void log_leaf(void *user, uint32_t car, uint32_t e, uint32_t vel)
{
    if (getenv("SMOKE_LOG") && leaf_calls++ % 20 == 0)
        fprintf(stderr, "leaf wheel step %d slip %.2f\n", step_no, GT4_F32(e + 0x38) - GT4_F32(e + 0x34));
    leaf_wheel(user, car, e, vel);
}

static uint8_t *frame(race_game *g, int *w, int *h)
{
    plat_video_begin_frame();
    race_game_draw(g);
    uint8_t *p = plat_sdl_read_pixels(w, h);
    plat_video_end_frame();
    return p;
}

int main(int argc, char **argv)
{
    if (argc < 4) return 2;
    int negative = 0, leaves = 0;
    const char *mine = "ford_gt_05";
    for (int a = 4; a < argc; a++) {
        if (!strcmp(argv[a], "--negative")) negative = 1;
        else if (!strcmp(argv[a], "--leaves")) leaves = 1;
        else if (!strcmp(argv[a], "--car") && a + 1 < argc) mine = argv[++a];
    }
    plat_video_desc desc = { 640, 448, 0, 0, 0 };
    if (plat_video_init(&desc)) return 2;
    SDL_HideWindow(SDL_GetWindowFromID(1));
    plat_video_set_view(640, 448);
    race_game *g = race_game_open(argv[1]);
    char msg[256];
    static const char *const others[] = { "z_act_70" };
    if (!g || !race_game_start(g, argv[2], mine, 0, others, NULL, 1, msg, sizeof msg)) { fprintf(stderr, "start: %s\n", msg); return 1; }
    int ready = 0;
    for (int i = 0; i < 4000 && !ready; i++) { SDL_PumpEvents(); SDL_Delay(5); ready = race_game_poll(g); }
    if (ready != 1) { fprintf(stderr, "the race did not load\n"); return 1; }
    if (!g->smoke) { fprintf(stderr, "FAIL no smoke for the course\n"); return 1; }
    if (leaves && !g->leaves) { fprintf(stderr, "FAIL no leaves for the course\n"); return 1; }
    gt4_wheel_smoke = negative ? NULL : log_wheel;
    gt4_wheel_leaves = negative || !g->leaves ? NULL : log_leaf;
    if (leaves) find_leafy(race_view_course(g->view));
    game_read = gt4_phys_pad.read;
    gt4_phys_pad.read = probe_read;
    int most = 0, at = -1;
    for (int f = 0; f < 60 * (leaves ? 60 : 25); f++) {
        step_no = f;
        race_game_update(g, STEP);
        if (!g->run) { fprintf(stderr, "the race stopped\n"); return 1; }
        int n = leaves ? race_leaves_live(g->leaves) : race_smoke_live(g->smoke);
        if (leaves && getenv("SMOKE_LOG") && n) {
            float lo, hi;
            race_leaves_height(g->leaves, &lo, &hi);
            fprintf(stderr, "leaves step %d: %d live, %.2f..%.2f m over the ground\n", f, n, lo, hi);
        }
        if (n > most) { most = n; at = f; }
        if (leaves) {   /* some of them up in the air */
            float lo, hi;
            race_leaves_height(g->leaves, &lo, &hi);
            if (n >= 4 && hi > 1) break;
        } else if (n >= 40) break;   /* thick enough to see */
    }
    fprintf(stderr, "most live particles %d (step %d)\n", most, at);
    if (leaves && getenv("SMOKE_LOG")) {   /* tests: where the leaves are against the camera */
        float eye[3], rows[9], proj[3];
        static race_leaves_quad lq[128];
        if (race_view_get_camera(g->view, eye, rows, proj)) {
            int n = race_leaves_quads(g->leaves, eye, rows, lq, 128);
            fprintf(stderr, "eye %.1f %.1f %.1f, %d leaves\n", eye[0], eye[1], eye[2], n);
            for (int i = 0; i < n && i < 4; i++)
                fprintf(stderr, " leaf %d picture %d view %.2f %.2f %.2f rgb %.2f %.2f %.2f\n", i, lq[i].picture,
                        lq[i].xyz[0][0], lq[i].xyz[0][1], lq[i].xyz[0][2], lq[i].rgb[0], lq[i].rgb[1], lq[i].rgb[2]);
        }
    }
    int w, h, w2, h2;
    race_view_show_car(g->view, 0);   /* the effect, not the car in front of it */
    uint8_t *with = frame(g, &w, &h);
    if (leaves) race_view_set_leaves(g->view, NULL);
    else race_view_set_smoke(g->view, NULL);
    uint8_t *without = frame(g, &w2, &h2);
    race_view_set_smoke(g->view, g->smoke);
    race_view_set_leaves(g->view, g->leaves);
    if (!with || !without || w != w2 || h != h2) { fprintf(stderr, "no frame\n"); return 1; }
    size_t len;
    void *png = tdefl_write_image_to_png_file_in_memory(with, w, h, 4, &len);
    FILE *o = png ? fopen(argv[3], "wb") : NULL;
    if (o) { fwrite(png, 1, len, o); fclose(o); }
    mz_free(png);
    int changed = 0;
    for (int i = 0; i < w * h; i++) {
        int d = abs(with[i * 4] - without[i * 4]) + abs(with[i * 4 + 1] - without[i * 4 + 1]) + abs(with[i * 4 + 2] - without[i * 4 + 2]);
        if (d > 6) changed++;
    }
    if (getenv("SMOKE_TEX")) {   /* tests: the smoke's texture (course header +0x68) as decoded, to SMOKE_TEX */
        const gt4_course *c = race_view_course(g->view);
        uint32_t t;
        tex1_image *im = NULL;
        memcpy(&t, c->data + 0x68, 4);
        int k = tex1_decode(c->data + t, c->size - t, &im);
        if (k > 0) {
            fprintf(stderr, "smoke texture: %d images, %dx%d (stored %dx%d) tfx %d tcc %d mag %d min %d\n", k, im[0].width, im[0].height,
                    im[0].tex_w, im[0].tex_h, im[0].texture_function, im[0].color_component, im[0].magnify, im[0].minify);
            int amin = 255, amax = 0, cmax = 0;
            for (int i = 0; i < im[0].width * im[0].height; i++) {
                int a = im[0].native_alpha ? im[0].native_alpha[i] : im[0].rgba[i * 4 + 3];
                if (a < amin) amin = a;
                if (a > amax) amax = a;
                if (im[0].rgba[i * 4] > cmax) cmax = im[0].rgba[i * 4];
            }
            fprintf(stderr, "alpha %d..%d, red at most %d\n", amin, amax, cmax);
            void *d = tdefl_write_image_to_png_file_in_memory(im[0].rgba, im[0].width, im[0].height, 4, &len);
            FILE *tf = d ? fopen(getenv("SMOKE_TEX"), "wb") : NULL;
            if (tf) { fwrite(d, 1, len, tf); fclose(tf); }
            mz_free(d);
            tex1_free(im, k);
        }
    }
    if (getenv("SMOKE_DIFF")) {   /* tests: the difference, eight times, to SMOKE_DIFF */
        for (int i = 0; i < w * h * 4; i++) without[i] = (uint8_t)(i % 4 == 3 ? 255 : abs(with[i] - without[i]) * 8 > 255 ? 255 : abs(with[i] - without[i]) * 8);
        void *d = tdefl_write_image_to_png_file_in_memory(without, w, h, 4, &len);
        FILE *df = d ? fopen(getenv("SMOKE_DIFF"), "wb") : NULL;
        if (df) { fwrite(d, 1, len, df); fclose(df); }
        mz_free(d);
    }
    free(with);
    free(without);
    fprintf(stderr, "pixels they change: %d\n", changed);
    if (most < (leaves ? 4 : 10) || changed < (leaves ? 40 : 200)) { fprintf(stderr, "FAIL nothing from the wheels (%d particles, %d pixels)\n", most, changed); return 1; }
    printf("PASS %d %s at most, %d pixels of them, %s\n", most, leaves ? "leaves" : "smoke particles", changed, argv[3]);
    return 0;
}
