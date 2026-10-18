/*
 * Android on-screen pad: the touch PS2 controls.
 *
 * The pad presents itself to SDL as a virtual game controller, so the device
 * adapter in src/platform/sdl/pad_sdl.c reads it exactly like a wired
 * DualShock 2: the player's pad state, the menus' engine keysyms and the
 * game's own key configuration all stay untouched. Only the hit testing and
 * the drawing live here.
 *
 * Nothing of the game is copied: every symbol is generated at start (shapes and
 * a 5x7 caption font), so the repository carries no image and no disc data is
 * read. The pad is drawn in framebuffer pixels (video_ui_pixels_begin), one
 * unit per pixel, anchored to the screen corners, so the Resolution setting,
 * the 4:3/16:9 choice and the panel size never move or scale it. Its sizes are
 * in dp (the display's own density), so the controls feel the same on a phone
 * and on a tablet.
 */
#include "platform/platform.h"
#include "android_platform.h"
#include "plat_sdl.h"
#include "video_gl.h"

#include <SDL.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#undef ALOGI
#undef ALOGW
#undef ALOGE
#define ALOGI(...) plat_android_log(ANDROID_LOG_INFO, __VA_ARGS__)
#define ALOGW(...) plat_android_log(ANDROID_LOG_WARN, __VA_ARGS__)
#define ALOGE(...) plat_android_log(ANDROID_LOG_ERROR, __VA_ARGS__)

/* ---- the virtual controller ----
   Button indices follow the order SDL numbers the buttons of a virtual
   joystick from its mask (SDL_virtualjoystick.c): A, B, X, Y, BACK, START,
   LEFTSHOULDER, RIGHTSHOULDER, then the four D-pad directions. Entries left
   out of the mask do not consume an index. L2 and R2 are the trigger axes,
   which is how a real pad reports them. */
enum { VP_A, VP_B, VP_X, VP_Y, VP_BACK, VP_START, VP_LB, VP_RB,
       VP_UP, VP_DOWN, VP_LEFT, VP_RIGHT, VP_COUNT };
enum { VA_LX, VA_LY, VA_RX, VA_RY, VA_L2, VA_R2, VA_COUNT };

/* ---- the pad's own keys ---- */
enum { TK_CROSS, TK_CIRCLE, TK_TRIANGLE, TK_SQUARE, TK_L1, TK_L2, TK_R1, TK_R2,
       TK_START, TK_SELECT, TK_UP, TK_DOWN, TK_LEFT, TK_RIGHT, TK_COUNT };

static const char *const key_name[TK_COUNT] = {
    "cross", "circle", "triangle", "square", "l1", "l2", "r1", "r2",
    "start", "select", "dpad-up", "dpad-down", "dpad-left", "dpad-right"
};

/* ---- symbols ---- */
enum { GLYPH_DISC, GLYPH_RING, GLYPH_TRIANGLE, GLYPH_SQUARE, GLYPH_CROSS,
       GLYPH_UP, GLYPH_DOWN, GLYPH_LEFT, GLYPH_RIGHT, GLYPH_COUNT };

enum { STYLE_ACTION, STYLE_DPAD, STYLE_BAR };

/* The drawn keys, in the order the layout fills them. */
enum { K_CROSS, K_CIRCLE, K_TRIANGLE, K_SQUARE, K_L1, K_L2, K_R1, K_R2,
       K_START, K_SELECT, K_UP, K_DOWN, K_LEFT, K_RIGHT, K_COUNT };

/* One drawn and touched button, in framebuffer pixels with y running down. */
typedef struct {
    unsigned char key;      /* TK_* */
    unsigned char style;    /* STYLE_* */
    signed char   glyph;    /* symbol cell, -1 none */
    const char   *label;    /* caption, NULL none */
    float x, y, w, h;
    uint8_t color[3];
} pad_key;

/* The pad's own colours: flat and quiet, so it reads as a port's overlay and
   not as an emulator skin. The four symbols keep their usual hues. */
static const uint8_t color_triangle[3] = { 145, 230, 175 };
static const uint8_t color_circle[3]   = { 242, 150, 150 };
static const uint8_t color_cross[3]    = { 150, 185, 245 };
static const uint8_t color_square[3]   = { 235, 165, 225 };
static const uint8_t color_neutral[3]  = { 222, 230, 242 };

static pad_key keys[K_COUNT];
static int layout_w, layout_h;   /* 0 until the first frame knows the size */

/* ---- generated symbol atlas ---- */
#define ATLAS_W 512
#define ATLAS_H 256
#define FONT_X  0
#define FONT_Y  192
#define FONT_CELL_W 32
#define FONT_CELL_H 48

typedef struct { int x, y, w, h; } atlas_cell;

static const atlas_cell glyph_cell[GLYPH_COUNT] = {
    {   0,   0, 128, 128 },  /* disc: the round backing of a button */
    { 128,   0, 128, 128 },  /* ring: its rim */
    { 256,   0,  64,  64 },  /* triangle */
    { 320,   0,  64,  64 },  /* square */
    { 384,   0,  64,  64 },  /* cross */
    { 256,  64,  64,  64 },  /* arrow up */
    { 320,  64,  64,  64 },  /* arrow down */
    { 384,  64,  64,  64 },  /* arrow left */
    { 448,  64,  64,  64 },  /* arrow right */
};

/* The captions the pad uses: L1 L2 R1 R2 START SELECT. */
static const struct { char c; uint8_t rows[7]; } font_glyphs[] = {
    { 'L', { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1e } },
    { '1', { 0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e } },
    { '2', { 0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f } },
    { 'R', { 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11 } },
    { 'S', { 0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e } },
    { 'E', { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f } },
    { 'T', { 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
    { 'A', { 0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 } },
    { 'C', { 0x0f, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0f } },
};
#define FONT_GLYPHS (sizeof font_glyphs / sizeof font_glyphs[0])

static plat_texture *atlas;

/* ---- pad state ---- */
static SDL_Joystick *vpad;
static int vpad_device = -1;
static int pad_on;
static int pad_failed;     /* the attach failed once; do not retry every frame */
static unsigned held;      /* keys a finger holds now (drives the drawing) */
static unsigned applied;   /* keys already written to the virtual controller */

#define PAD_FINGERS 8
typedef struct { int used, resolved, id, key; float x, y; } pad_finger;
static pad_finger fingers[PAD_FINGERS];
static int touch_logged;

/* ---- signed distances of the symbols, in atlas pixels ----
   Positive is inside the ink; the painters turn that into one pixel of edge. */
static float cell_w, cell_h;

static float sd_disc(float x, float y)
{
    float r = fminf(cell_w, cell_h) * .5f - 2.5f;
    return r - hypotf(x - cell_w * .5f, y - cell_h * .5f);
}

static float sd_ring(float x, float y)
{
    float t = fminf(cell_w, cell_h) * .12f;
    float r = fminf(cell_w, cell_h) * .5f - t * .5f - 2.5f;
    return t * .5f - fabsf(hypotf(x - cell_w * .5f, y - cell_h * .5f) - r);
}

static float sd_box(float x, float y)
{
    float t = fminf(cell_w, cell_h) * .12f;
    float b = fminf(cell_w, cell_h) * .5f - t * .5f - 2.5f;
    float dx = fabsf(x - cell_w * .5f) - b, dy = fabsf(y - cell_h * .5f) - b;
    float sd = hypotf(fmaxf(dx, 0), fmaxf(dy, 0)) + fminf(fmaxf(dx, dy), 0);
    return t * .5f - fabsf(sd);
}

/* Distance into a convex polygon; its edges point inward by construction. */
static float poly_distance(const float *pts, int n, float x, float y)
{
    float cx = 0, cy = 0, inside = -1e9f;
    for (int i = 0; i < n; i++) { cx += pts[2 * i]; cy += pts[2 * i + 1]; }
    cx /= (float)n; cy /= (float)n;
    for (int i = 0; i < n; i++) {
        float ax = pts[2 * i], ay = pts[2 * i + 1];
        float bx = pts[2 * ((i + 1) % n)], by = pts[2 * ((i + 1) % n) + 1];
        float ex = bx - ax, ey = by - ay, len = hypotf(ex, ey);
        if (len < 1e-6f) continue;
        float nx = -ey / len, ny = ex / len;
        if (nx * (cx - ax) + ny * (cy - ay) < 0) { nx = -nx; ny = -ny; }
        float d = (x - ax) * nx + (y - ay) * ny;
        if (d > inside) inside = d;
    }
    return inside;
}

static float sd_triangle(float x, float y)
{
    float t = fminf(cell_w, cell_h) * .13f;
    const float pts[6] = { cell_w * .5f, 2.5f, cell_w - 2.5f, cell_h - 2.5f, 2.5f, cell_h - 2.5f };
    return t * .5f - fabsf(poly_distance(pts, 3, x, y));
}

static float segment_distance(float px, float py, float ax, float ay, float bx, float by)
{
    float ex = bx - ax, ey = by - ay;
    float t = ((px - ax) * ex + (py - ay) * ey) / (ex * ex + ey * ey);
    t = fminf(1, fmaxf(0, t));
    return hypotf(px - (ax + t * ex), py - (ay + t * ey));
}

/* The cross is one thick stroke along each diagonal, as the DualShock draws it. */
static float sd_cross(float x, float y)
{
    float t = fminf(cell_w, cell_h) * .17f, m = fminf(cell_w, cell_h) * .2f;
    float d1 = segment_distance(x, y, m, m, cell_w - m, cell_h - m);
    float d2 = segment_distance(x, y, cell_w - m, m, m, cell_h - m);
    return t * .5f - fminf(d1, d2);
}

/* A filled arrow, rotated so one cell serves all four directions. */
static float sd_arrow(float x, float y, int quarter_turns)
{
    const float w = cell_w, h = cell_h, m = fminf(w, h) * .18f;
    float pts[6] = { w * .5f, m, w - m, h - m, m, h - m };
    float cx = w * .5f, cy = h * .5f;
    for (int i = 0; i < 3; i++) {
        float px = pts[2 * i] - cx, py = pts[2 * i + 1] - cy;
        for (int q = 0; q < quarter_turns; q++) { float t = px; px = -py; py = t; }
        pts[2 * i] = cx + px; pts[2 * i + 1] = cy + py;
    }
    return poly_distance(pts, 3, x, y);
}

/* Quarter turns run the apex through right, down and left (rows grow down,
   so the first turn takes it to +x). */
static float sd_arrow_up(float x, float y)    { return sd_arrow(x, y, 0); }
static float sd_arrow_down(float x, float y)  { return sd_arrow(x, y, 2); }
static float sd_arrow_left(float x, float y)  { return sd_arrow(x, y, 3); }
static float sd_arrow_right(float x, float y) { return sd_arrow(x, y, 1); }

static void paint(uint8_t *img, const atlas_cell *cell, float (*sd)(float, float))
{
    cell_w = (float)cell->w; cell_h = (float)cell->h;
    for (int y = 0; y < cell->h; y++) for (int x = 0; x < cell->w; x++) {
        float a = sd(x + .5f, y + .5f) + .5f;    /* one pixel of edge */
        if (a <= 0) continue;
        if (a > 1) a = 1;
        uint8_t *p = img + ((size_t)(cell->y + y) * ATLAS_W + cell->x + x) * 4;
        p[0] = p[1] = p[2] = 255;
        p[3] = (uint8_t)(a * 255 + .5f);
    }
}

/* The captions are painted at four times the 5x7 pattern, so the atlas holds
   them once and every label is one quad per character. */
static void paint_caption(uint8_t *img, int index, const uint8_t rows[7])
{
    for (int y = 0; y < 7; y++) for (int x = 0; x < 5; x++) {
        if (!(rows[y] & (1 << (4 - x)))) continue;
        for (int sy = 0; sy < 4; sy++) for (int sx = 0; sx < 4; sx++) {
            uint8_t *p = img + ((size_t)(FONT_Y + 10 + y * 4 + sy) * ATLAS_W +
                                FONT_X + index * FONT_CELL_W + 6 + x * 4 + sx) * 4;
            p[0] = p[1] = p[2] = 255;
            p[3] = 255;
        }
    }
}

static void atlas_build(void)
{
    uint8_t *img = calloc((size_t)ATLAS_W * ATLAS_H, 4);
    if (!img) return;
    paint(img, &glyph_cell[GLYPH_DISC], sd_disc);
    paint(img, &glyph_cell[GLYPH_RING], sd_ring);
    paint(img, &glyph_cell[GLYPH_TRIANGLE], sd_triangle);
    paint(img, &glyph_cell[GLYPH_SQUARE], sd_box);
    paint(img, &glyph_cell[GLYPH_CROSS], sd_cross);
    paint(img, &glyph_cell[GLYPH_UP], sd_arrow_up);
    paint(img, &glyph_cell[GLYPH_DOWN], sd_arrow_down);
    paint(img, &glyph_cell[GLYPH_LEFT], sd_arrow_left);
    paint(img, &glyph_cell[GLYPH_RIGHT], sd_arrow_right);
    for (size_t i = 0; i < FONT_GLYPHS; i++) paint_caption(img, (int)i, font_glyphs[i].rows);
    atlas = plat_texture_create(ATLAS_W, ATLAS_H, img);
    free(img);
    if (!atlas) ALOGW("touch pad: the symbol atlas could not be created");
}

static int caption_index(char c)
{
    for (size_t i = 0; i < FONT_GLYPHS; i++) if (font_glyphs[i].c == c) return (int)i;
    return 0;
}

/* ---- the layout, in framebuffer pixels ----
   Every size is a dp count times the display's density, computed from the
   screen's shorter side and clamped, so a panel whose reported DPI is odd
   still gets a thumb-sized button. */

static float pad_unit(int w, int h)
{
    float ddpi = 0, hdpi = 0, vdpi = 0, s = 0;
    if (SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi) == 0 && (ddpi > 0 || hdpi > 0))
        s = (ddpi > 0 ? ddpi : hdpi) / 160.f;
    float d = 64.f * s, minwh = (float)(w < h ? w : h);
    if (!(d > minwh * .055f)) d = minwh * .055f;
    if (d > minwh * .105f) d = minwh * .105f;
    return d / 64.f;
}

static void key_set(int i, unsigned char key, unsigned char style, signed char glyph,
                    const char *label, float x, float y, float w, float h, const uint8_t color[3])
{
    keys[i].key = key; keys[i].style = style; keys[i].glyph = glyph; keys[i].label = label;
    keys[i].x = x; keys[i].y = y; keys[i].w = w; keys[i].h = h;
    memcpy(keys[i].color, color, 3);
}

static void layout_build(int w, int h)
{
    float s = pad_unit(w, h), d = 64 * s, m = 16 * s, fw = (float)w, fh = (float)h;
    /* The thumbs sit in the bottom corners, in the arrangement a DualShock
       carries: the action diamond right, the D-pad left, and the centre of the
       frame free for driving. Rows grow down, so row zero is the top one. */
    float ax = fw - m, ay = fh - m - 3 * d;
    key_set(K_TRIANGLE, TK_TRIANGLE, STYLE_ACTION, GLYPH_TRIANGLE, NULL, ax - 2 * d, ay, d, d, color_triangle);
    key_set(K_SQUARE,   TK_SQUARE,   STYLE_ACTION, GLYPH_SQUARE,   NULL, ax - 3 * d, ay + d, d, d, color_square);
    key_set(K_CIRCLE,   TK_CIRCLE,   STYLE_ACTION, GLYPH_RING,     NULL, ax - d,     ay + d, d, d, color_circle);
    key_set(K_CROSS,    TK_CROSS,    STYLE_ACTION, GLYPH_CROSS,    NULL, ax - 2 * d, ay + 2 * d, d, d, color_cross);
    key_set(K_UP,    TK_UP,    STYLE_DPAD, GLYPH_UP,    NULL, m + d,     ay, d, d, color_neutral);
    key_set(K_LEFT,  TK_LEFT,  STYLE_DPAD, GLYPH_LEFT,  NULL, m,         ay + d, d, d, color_neutral);
    key_set(K_RIGHT, TK_RIGHT, STYLE_DPAD, GLYPH_RIGHT, NULL, m + 2 * d, ay + d, d, d, color_neutral);
    key_set(K_DOWN,  TK_DOWN,  STYLE_DPAD, GLYPH_DOWN,  NULL, m + d,     ay + 2 * d, d, d, color_neutral);
    /* Shoulders at the top corners, the trigger above the bumper as on the pad. */
    float bw = 78 * s, bh = 32 * s, bg = 8 * s;
    key_set(K_L2, TK_L2, STYLE_BAR, -1, "L2", m, m, bw, bh, color_neutral);
    key_set(K_L1, TK_L1, STYLE_BAR, -1, "L1", m, m + bh + bg, bw, bh, color_neutral);
    key_set(K_R2, TK_R2, STYLE_BAR, -1, "R2", fw - m - bw, m, bw, bh, color_neutral);
    key_set(K_R1, TK_R1, STYLE_BAR, -1, "R1", fw - m - bw, m + bh + bg, bw, bh, color_neutral);
    /* Start and Select at the bottom centre. */
    float sw = 78 * s, sh = 30 * s;
    key_set(K_SELECT, TK_SELECT, STYLE_BAR, -1, "SELECT", fw * .5f - bg * .5f - sw, fh - m - sh, sw, sh, color_neutral);
    key_set(K_START,  TK_START,  STYLE_BAR, -1, "START",  fw * .5f + bg * .5f,      fh - m - sh, sw, sh, color_neutral);
    layout_w = w; layout_h = h;
}

/* ---- drawing ---- */

static void draw_cell(int glyph, float x, float y, float w, float h, const uint8_t color[4])
{
    const atlas_cell *c = &glyph_cell[glyph];
    plat_draw_quad(atlas, x, y, w, h, (float)c->x / ATLAS_W, (float)c->y / ATLAS_H,
                   (float)(c->x + c->w) / ATLAS_W, (float)(c->y + c->h) / ATLAS_H, color);
}

/* The atlas holds each caption once; a character is one cell of it, and the
   pattern inside the cell is centred both ways. */
static void draw_caption(const char *text, float x, float y, float w, float h, const uint8_t color[4])
{
    int n = text ? (int)strlen(text) : 0;
    if (!n) return;
    float ch = h * .6f, cw = ch * ((float)FONT_CELL_W / FONT_CELL_H), step = cw * .92f;
    float left = x + (w - ((n - 1) * step + cw)) * .5f, top = y + (h - ch) * .5f;
    for (int i = 0; i < n; i++) {
        int g = caption_index(text[i]);
        plat_draw_quad(atlas, left + i * step, top, cw, ch,
                       (float)(FONT_X + g * FONT_CELL_W) / ATLAS_W, (float)FONT_Y / ATLAS_H,
                       (float)(FONT_X + (g + 1) * FONT_CELL_W) / ATLAS_W, (float)(FONT_Y + FONT_CELL_H) / ATLAS_H,
                       color);
    }
}

static void draw_key(const pad_key *b)
{
    int down = (held >> b->key) & 1;
    uint8_t back[4] = { 8, 11, 16, down ? 150 : 92 };
    uint8_t ink[4] = { b->color[0], b->color[1], b->color[2], down ? 255 : 205 };
    switch (b->style) {
    case STYLE_ACTION: {
        /* The rim sits outside the hit rectangle, so the round button is a
           little bigger than the touch target and the target can stay square. */
        float r = b->w * .14f, s = b->w + 2 * r, g = b->w * .5f;
        draw_cell(GLYPH_DISC, b->x - r, b->y - r, s, s, back);
        draw_cell(GLYPH_RING, b->x - r, b->y - r, s, s, ink);
        draw_cell(b->glyph, b->x + (b->w - g) * .5f, b->y + (b->h - g) * .5f, g, g, ink);
        break;
    }
    case STYLE_DPAD: {
        float g = b->w * .54f;
        draw_cell(GLYPH_DISC, b->x, b->y, b->w, b->h, back);
        draw_cell(b->glyph, b->x + (b->w - g) * .5f, b->y + (b->h - g) * .5f, g, g, ink);
        break;
    }
    default: /* STYLE_BAR */
        plat_draw_quad(NULL, b->x, b->y, b->w, b->h, 0, 0, 1, 1, back);
        draw_caption(b->label, b->x, b->y, b->w, b->h, ink);
        break;
    }
}

void plat_android_touch_draw(void)
{
    if (!pad_on) return;
    /* The pad belongs to the whole frame: leave any race HUD panel and clip the
       game may have set for its own layers. */
    video_ui_panel(0, 0);
    plat_set_clip(0, 0, 0, 0);
    if (!atlas) return;
    int w = 0, h = 0;
    video_ui_pixels_begin(&w, &h);
    if (w > 0 && h > 0) {
        /* The frame's own size, which is what the overlay was laid out for. */
        if (w != layout_w || h != layout_h) layout_build(w, h);
        for (int i = 0; i < K_COUNT; i++) draw_key(&keys[i]);
    }
    video_ui_pixels_end();
}

/* ---- the touch surface ---- */

/* A touch, in framebuffer pixels with y running down: exactly the space the
   overlay draws in, so no transform and no mirroring sit between them. SDL
   normalizes a finger against the surface, whose size is the drawable size. */
static int touch_point(float nx, float ny, float *x, float *y)
{
    SDL_Window *window = plat_sdl_window();
    int dw = 0, dh = 0;
    if (!window) return 0;
    SDL_GL_GetDrawableSize(window, &dw, &dh);
    if (dw <= 0 || dh <= 0) return 0;
    *x = nx * (float)dw; *y = ny * (float)dh;
    return 1;
}

/* The key whose centre is nearest, with a little room around the ink so a
   thumb does not have to be exact. */
static int hit_key(float gx, float gy)
{
    int best = -1;
    float best_d = 0;
    for (int i = 0; i < K_COUNT; i++) {
        const pad_key *b = &keys[i];
        float pad = b->w * .12f, cx = b->x + b->w * .5f, cy = b->y + b->h * .5f;
        if (gx < b->x - pad || gx > b->x + b->w + pad || gy < b->y - pad || gy > b->y + b->h + pad) continue;
        float dx = gx - cx, dy = gy - cy, d = dx * dx + dy * dy;
        if (best < 0 || d < best_d) { best = i; best_d = d; }
    }
    return best;
}

static void finger_down(int id, float x, float y)
{
    for (int i = 0; i < PAD_FINGERS; i++)
        if (fingers[i].used && fingers[i].id == id) { fingers[i].x = x; fingers[i].y = y; return; }
    for (int i = 0; i < PAD_FINGERS; i++)
        if (!fingers[i].used) {
            fingers[i] = (pad_finger){ 1, 0, id, -1, x, y };
            return;
        }
}

static void finger_up(int id)
{
    for (int i = 0; i < PAD_FINGERS; i++)
        if (fingers[i].used && fingers[i].id == id) { fingers[i].used = 0; fingers[i].key = -1; return; }
}

static void fingers_drop(void)
{
    memset(fingers, 0, sizeof fingers);
    for (int i = 0; i < PAD_FINGERS; i++) fingers[i].key = -1;
}

/* Every event stays in the queue: the game reads the same fingers for its own
   purposes. Only the state this pad needs is copied out. */
static int touch_watch(void *user, SDL_Event *e)
{
    (void)user;
    switch (e->type) {
    case SDL_FINGERDOWN: finger_down((int)e->tfinger.fingerId, e->tfinger.x, e->tfinger.y); break;
    case SDL_FINGERUP: finger_up((int)e->tfinger.fingerId); break;
    case SDL_APP_WILLENTERBACKGROUND:
    case SDL_APP_DIDENTERBACKGROUND: fingers_drop(); break;
    case SDL_WINDOWEVENT:
        if (e->window.event == SDL_WINDOWEVENT_FOCUS_LOST || e->window.event == SDL_WINDOWEVENT_HIDDEN ||
            e->window.event == SDL_WINDOWEVENT_MINIMIZED) fingers_drop();
        break;
    default: break;
    }
    return 1;
}

/* ---- the virtual controller ---- */

static int vpad_button_of(int key)
{
    switch (key) {
    case TK_CROSS: return VP_A;
    case TK_CIRCLE: return VP_B;
    case TK_TRIANGLE: return VP_Y;
    case TK_SQUARE: return VP_X;
    case TK_L1: return VP_LB;
    case TK_R1: return VP_RB;
    case TK_START: return VP_START;
    case TK_SELECT: return VP_BACK;
    case TK_UP: return VP_UP;
    case TK_DOWN: return VP_DOWN;
    case TK_LEFT: return VP_LEFT;
    case TK_RIGHT: return VP_RIGHT;
    default: return -1;   /* the triggers are axes */
    }
}

static void pad_release(void)
{
    for (int key = 0; key < TK_COUNT; key++) {
        int b = vpad_button_of(key);
        if (b >= 0 && vpad) SDL_JoystickSetVirtualButton(vpad, b, 0);
    }
    if (vpad) {
        SDL_JoystickSetVirtualAxis(vpad, VA_L2, SDL_JOYSTICK_AXIS_MIN);
        SDL_JoystickSetVirtualAxis(vpad, VA_R2, SDL_JOYSTICK_AXIS_MIN);
    }
    applied = held = 0;
}

static void pad_write(unsigned want)
{
    if (!vpad) { applied = want; return; }
    for (int key = 0; key < TK_COUNT; key++) {
        int was = (applied >> key) & 1, now = (want >> key) & 1;
        if (was == now) continue;
        if (key == TK_L2 || key == TK_R2)
            SDL_JoystickSetVirtualAxis(vpad, key == TK_L2 ? VA_L2 : VA_R2, now ? SDL_JOYSTICK_AXIS_MAX : SDL_JOYSTICK_AXIS_MIN);
        else
            SDL_JoystickSetVirtualButton(vpad, vpad_button_of(key), (Uint8)now);
    }
    applied = want;
    held = want;
}

static void pad_attach(void)
{
    SDL_VirtualJoystickDesc desc;
    if (pad_on) return;
    memset(&desc, 0, sizeof desc);
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    desc.naxes = VA_COUNT;
    desc.nbuttons = VP_COUNT;
    desc.vendor_id = 0x4f50;   /* "OP" */
    desc.product_id = 0x4754;  /* "GT" */
    desc.name = "OpenGT4 Touch Pad";
    desc.button_mask = (1u << SDL_CONTROLLER_BUTTON_A) | (1u << SDL_CONTROLLER_BUTTON_B) |
                       (1u << SDL_CONTROLLER_BUTTON_X) | (1u << SDL_CONTROLLER_BUTTON_Y) |
                       (1u << SDL_CONTROLLER_BUTTON_BACK) | (1u << SDL_CONTROLLER_BUTTON_START) |
                       (1u << SDL_CONTROLLER_BUTTON_LEFTSHOULDER) | (1u << SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) |
                       (1u << SDL_CONTROLLER_BUTTON_DPAD_UP) | (1u << SDL_CONTROLLER_BUTTON_DPAD_DOWN) |
                       (1u << SDL_CONTROLLER_BUTTON_DPAD_LEFT) | (1u << SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    desc.axis_mask = (1u << SDL_CONTROLLER_AXIS_LEFTX) | (1u << SDL_CONTROLLER_AXIS_LEFTY) |
                     (1u << SDL_CONTROLLER_AXIS_RIGHTX) | (1u << SDL_CONTROLLER_AXIS_RIGHTY) |
                     (1u << SDL_CONTROLLER_AXIS_TRIGGERLEFT) | (1u << SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    vpad_device = SDL_JoystickAttachVirtualEx(&desc);
    if (vpad_device < 0) {
        pad_failed = 1;
        ALOGE("touch pad: the virtual controller could not be attached (%s)", SDL_GetError());
        return;
    }
    vpad = SDL_JoystickOpen(vpad_device);
    if (!vpad) {
        pad_failed = 1;
        ALOGE("touch pad: the virtual controller could not be opened (%s)", SDL_GetError());
        SDL_JoystickDetachVirtual(vpad_device);
        vpad_device = -1;
        return;
    }
    pad_on = 1;
    ALOGW("touch pad: on, the controls are on the screen");
}

static void pad_detach(void)
{
    pad_release();
    if (vpad) { SDL_JoystickClose(vpad); vpad = NULL; }
    if (vpad_device >= 0) { SDL_JoystickDetachVirtual(vpad_device); vpad_device = -1; }
    pad_on = 0;
}

/* A connected controller always wins the first player's port, so the on-screen
   pad exists only while the device has none. */
static void pad_sync(void)
{
    int physical = 0;
    for (int i = 0; i < SDL_NumJoysticks() && !physical; i++)
        if (!SDL_JoystickIsVirtual(i) &&
            (SDL_IsGameController(i) || SDL_JoystickGetDeviceType(i) == SDL_JOYSTICK_TYPE_WHEEL)) physical = 1;
    if (physical && pad_on) {
        ALOGW("touch pad: off, a controller is connected");
        pad_detach();
    } else if (!physical && !pad_on && !pad_failed) pad_attach();
}

void plat_android_touch_init(void)
{
    plat_input_init();   /* the virtual controller needs the subsystem */
    SDL_AddEventWatch(touch_watch, NULL);
    atlas_build();
    pad_sync();
}

void plat_android_touch_shutdown(void)
{
    SDL_DelEventWatch(touch_watch, NULL);
    pad_detach();
    if (atlas) { plat_texture_destroy(atlas); atlas = NULL; }
}

void plat_android_touch_update(void)
{
    unsigned want = 0;
    SDL_Window *window = plat_sdl_window();
    int dw = 0, dh = 0;
    if (window) SDL_GL_GetDrawableSize(window, &dw, &dh);
    if (dw > 0 && dh > 0 && (dw != layout_w || dh != layout_h)) layout_build(dw, dh);
    pad_sync();
    for (int i = 0; i < PAD_FINGERS; i++) {
        if (!fingers[i].used) continue;
        if (!fingers[i].resolved && layout_w > 0) {
            /* A finger claims the key it landed on. The layout follows the
               frame, so the mapping happens here and not in the event callback,
               which may run before the first frame. */
            float gx = 0, gy = 0;
            int index = touch_point(fingers[i].x, fingers[i].y, &gx, &gy) ? hit_key(gx, gy) : -1;
            fingers[i].resolved = 1;
            fingers[i].key = index >= 0 ? keys[index].key : -1;
            if (++touch_logged <= 8)
                ALOGI("touch: %.3f,%.3f -> %.0f,%.0f -> %s", fingers[i].x, fingers[i].y, gx, gy,
                      index >= 0 ? key_name[fingers[i].key] : "none");
        }
        if (fingers[i].key >= 0) want |= 1u << fingers[i].key;
    }
    pad_write(want);
}
