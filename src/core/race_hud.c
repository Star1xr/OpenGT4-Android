/*
 * The race display. Each element keeps what the original's objects hold
 * (their common head, func_003AEBF8: +0 shown, +1/+2 the horizontal and
 * vertical alignment, +4/+6 the position, +8/+A the size, +C the scale),
 * with the values the game leaves in them during an Arcade race (the
 * RaceDisplay at 0x01CF4D50 of the ar_02/ar_03 captures), and is drawn
 * as its class's draw method does. Units are the display's 640x480; the
 * GS shows them on 448 lines.
 */
#include "race_hud.h"
#include "license_presentation.h"
#include "race_split_layout.h"
#include "race_pit_crew.h"

#include "font.h"
#include "glyph_sdf.h"
#include "gpb.h"
#include "rtext.h"
#include "tex1.h"
#include "platform/platform.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NO_TIME 0x157529FFu

/* ------------------------------------------------------------ assets */

typedef struct { const char *name; plat_texture *tex; int w, h; } hud_image;

enum {
    IMG_POSITION, IMG_LAP, IMG_BEST, IMG_LAST, IMG_POSITION_BASE, IMG_LAPTIME,
    IMG_REMOTE_MPH, IMG_REMOTE_KMPH, IMG_REV_BAR, IMG_GEAR_BASE, IMG_TIMING_LAMP,
    IMG_G_BASE, IMG_G_POINT, IMG_PEDALS, IMG_PEDAL_BAR, IMG_SIGN_ASM, IMG_SIGN_BRAKE,
    IMG_SIGN_OIL, IMG_FUEL_B0, IMG_FUEL_B1, IMG_POINT_TARGET, IMG_POINT_RIVAL,
    IMG_PANEL_BASE, IMG_KMPH, IMG_MPH, IMG_SPEED_80, IMG_SPEED_160, IMG_SPEED_240, IMG_SPEED_320,
    IMG_REV_4000, IMG_REV_5000, IMG_REV_6000, IMG_REV_7000, IMG_REV_8000, IMG_REV_9000, IMG_REV_10000,
    IMG_REV_12000, IMG_REV_18000, IMG_FUEL_A0, IMG_FUEL_A1, IMG_WRONG_WAY,
    IMG_REPLAY_PLATE, IMG_GT_LOGO, IMG_FIRST, IMG_SECOND, IMG_THIRD, IMG_PYLON,
    IMG_TOTAL, IMG_TIRE_WEAR,
    IMG_REFUEL_BG, IMG_REFUEL_BAR, IMG_REFUEL_SCALE,
    IMG_COUNT
};
static const char *const image_names[IMG_COUNT] = {
    "JP/position", "JP/lap", "JP/best_time", "JP/last_time", "position_base", "GT4_meter_texture_laptime",
    "remote_speed_mph", "remote_speed_kmph", "rev_bar", "gear_base", "timing_lamp",
    "g_meter_base", "g_meter_point", "pedals", "pedal_bar", "sign_asm", "sign_brake",
    "sign_oil", "fuel_b0", "fuel_b1", "point_target", "point_rival",
    "panel_base", "kmph", "mph", "speed_80km", "speed_160mph", "speed_240km", "speed_320km",
    "rev_4000", "rev_5000", "rev_6000", "rev_7000", "rev_8000", "rev_9000", "rev_10000",
    "rev_12000", "rev_18000", "fuel_a0", "fuel_a1", "GT4_meter_texture_wrongway",
    "replay_plate", "JP/gt_logo_mini", "mark_1st", "mark_2nd", "mark_3rd", "pylon",
    "JP/total_time", "tire_wear",
    "refuel_bg", "refuel_bar", "refuel_scale",
};

/* func_003A9640: the display's blinker: on for `duty` of each `period`
   (count periods, -1 for ever, then `hold`), its value rising and
   falling at its rates, kept in 0..1 */
typedef struct { float v, t, period, duty, rise, fall; int count, hold; } blinker;

static void blink(blinker *b, float dt)
{
    int on;
    if (b->count) {
        b->t += dt;
        if (b->period < b->t) {
            b->t -= (float)(int)(b->t / b->period) * b->period;
            if (b->count > 0) b->count--;
        }
        on = b->t < b->duty * b->period;
    } else on = b->hold;
    b->v = fminf(fmaxf(b->v + dt * (on ? b->rise : -b->fall), 0), 1);
}

/* A face of race.fnt as the display names it ("FC_26*80,80", "GT4meter#1"):
   #1 the ink alone (palette 1), else the ink over its black outline
   (palette 0); *x,y the size in percent */
typedef struct hud_font {
    char name[32];
    gt4_font face;
    plat_texture *ink, *outline;
    int palette;
    float sx, sy;
    int ok;
    struct hud_font *next;
} hud_font;

struct race_hud {
    gt4_vol *vol;
    hud_image img[IMG_COUNT];
    hud_font *fonts;
    int nfonts;
    float rank_now;        /* the rank element's rolling figure (+0x20) */
    int rank_shown;        /* its +0x18 */
    unsigned speed_tick;   /* the digital speedometer's frame count (+0x30) */
    int speed_now, speed_prev; /* its +0x34, +0x36 */
    float sign_last[3];    /* each sign's last brightness (+0x3C) */
    blinker sign_blink[3]; /* each sign's (+0x18) */
    blinker map_blink;     /* the mini map's player point (+0x48) */
    blinker wrong_blink;   /* the signal's blinker (+0x22C0) for the wrong way */
    int signal_last;
    float penalty_shown;   /* how long the penalty message has been up */
    float needle_last[2];  /* the round meters' last needle angles (+0x30), -1 none */
    /* the race's text (rtext/raceUS.rt2, page RaceDisplay) */
    uint8_t *rt_data;
    gt4_rtext rt;
    /* the big message (RaceDisplay +0x252C, at (320, 224)): its text,
       how long it stays and fades out, and the time shown so far */
    const char *msg;
    float msg_hold, msg_fade, msg_t;
    int was_counting, was_finished;
    float fade;            /* the whole display's light (+0xBC) */
    float shown;           /* that light as last drawn, with the start's rise */
    /* the music's title (RaceMusicDisplay, RaceDisplay +0x3EEC): the track
       asked and the one shown (+0x50), its texts (+0x18, +0x1C) and the
       envelope (+0x28, func_003A98E8/func_003A9780) */
    char music_want[96], music_artist_want[96], music_shown[96], music_title[96], music_artist[96];
    float music_v, music_t;
    int music_state;
    /* the GT logo (RaceDisplay +0x1170) put out while a new track's title
       waits and 11 s after (func_003A3AA8: its +0x24 in 0.5 s, back in 3) */
    float logo_out, logo_out_hold;
    float fade_rate;       /* its fall per second, 0 none */
    /* the course's map (GTCM): its picture, size, the course's point at
       its centre, the scale and the turn (degrees) */
    plat_texture *map;
    plat_texture *course_logo; /* course header+0xC4, RaceDisplay+0x11B0 */
    int logo_w, logo_h;
    int map_tw, map_th, map_w, map_h;
    float map_cx, map_cy, map_scale, map_angle;
    /* the timing lines (RaceTextList +0x17BC, 8 lines): their text,
       colour, place, start and length on the display's clock; when the
       queued lines end (+0x28) and when the last one starts (+0x24) */
    struct { char text[48]; uint32_t color; float x, y, start, dur; } lines[8];
    float clock, lines_end, lines_last;
    /* the icon beside the gap (+0x2D14): its car, place and time */
    int icon_car;
    float icon_x, icon_y, icon_start;
    void (*icon)(void *user, int car, float x, float y, float w, float h);
    void *icon_user;
};

static plat_texture *glyph_texture(const tex1_image *a)
{
    int big = a->width > a->height ? a->width : a->height, k = big > 0 ? 2048 / big : 1, w = 0, h = 0;
    uint8_t *sdf;
    plat_texture *t;
    if (k > 4) k = 4;
    if (k < 1) k = 1;
    sdf = gt4_glyph_sdf(a->rgba, a->width, a->height, k, &w, &h);
    if (!sdf) return plat_texture_create(a->width, a->height, a->rgba);
    t = plat_texture_create(w, h, sdf);
    free(sdf);
    return t;
}

static hud_font *font(race_hud *h, const char *spec)
{
    char base[32];
    const char *p;
    size_t n;
    hud_font *f;
    for (f = h->fonts; f; f = f->next)
        if (!strcmp(f->name, spec)) return f->ok ? f : NULL;
    /* 0044D920 -> 0044D9B8 resolves each requested font/style. The host's
       twelve-style quota dropped replay labels after a race. Keep stable
       nodes: rich_text can resolve Gaiji/%F while retaining its main face. */
    f = calloc(1, sizeof *f);
    if (!f) return NULL;
    f->next = h->fonts;
    h->fonts = f;
    h->nfonts++;
    snprintf(f->name, sizeof f->name, "%s", spec);
    n = strcspn(spec, "#*");
    snprintf(base, sizeof base, "%.*s", (int)n, spec);
    f->sx = f->sy = 1;
    for (p = spec + n; *p;) {
        if (*p == '#') f->palette = atoi(++p);
        else if (*p == '*') {
            int x = 100, y = 100;
            sscanf(++p, "%d,%d", &x, &y);
            f->sx = (float)x / 100;
            f->sy = (float)y / 100;
        }
        p += strcspn(p, "#*");
    }
    {
        const char *const files[] = { "/font/race.fnt", "/font/system.fnt" };
        for (unsigned i = 0; i < 2 && !f->ok; i++) {
            size_t size = 0;
            uint8_t *d = gt4_vol_read(h->vol, files[i], &size);
            if (d && gt4_font_decode(d, size, base, &f->face) == 1) f->ok = 1;
            free(d);
        }
    }
    if (!f->ok) {
        fprintf(stderr, "hud: no font %s\n", spec);
        return NULL;
    }
    f->ink = glyph_texture(&f->face.atlases[f->face.natlases > 1 ? 1 : 0]);
    f->outline = glyph_texture(&f->face.atlases[0]);
    return f;
}

race_hud *race_hud_open(gt4_vol *vol)
{
    race_hud *h = calloc(1, sizeof *h);
    size_t size = 0;
    uint8_t *pkg;
    if (!h) return NULL;
    h->vol = vol;
    h->icon_car = -1;
    pkg = gt4_vol_read(vol, "/race_display/gt4/US/display.gpb", &size);
    if (!pkg) {
        fprintf(stderr, "hud: no display.gpb\n");
        return h;
    }
    for (int i = 0; i < IMG_COUNT; i++) {
        char name[96];
        size_t n = 0;
        uint8_t *data;
        tex1_image *im = NULL;
        snprintf(name, sizeof name, "image/%s.png", image_names[i]);
        data = gpb_load(pkg, size, name, &n);
        if (data && tex1_decode(data, n, &im) > 0) {
            h->img[i].tex = plat_texture_create(im[0].width, im[0].height, im[0].rgba);
            h->img[i].w = im[0].width;
            h->img[i].h = im[0].height;
            tex1_free(im, 1);
        } else fprintf(stderr, "hud: no image %s\n", name);
        h->img[i].name = image_names[i];
        free(data);
    }
    free(pkg);
    for (int i = 0; i < 3; i++) h->sign_blink[i] = (blinker){ 1, 0, 0.2f, 0.5f, 3.4e38f, 14.285716f, -1, 0 };
    h->map_blink = (blinker){ 0, 0, 0.5f, 0.5f, 10.000001f, 3.3333335f, -1, 0 };
    h->wrong_blink = (blinker){ 1, 0, 0.2f, 0.3f, 20, 10, 0, 0 };
    h->fade = 1;
    h->needle_last[0] = h->needle_last[1] = -1;
    {
        size_t n = 0;
        h->rt_data = gt4_vol_read(vol, "/rtext/raceUS.rt2", &n);
        if (!h->rt_data || !gt4_rtext_open(&h->rt, h->rt_data, n)) fprintf(stderr, "hud: no race text\n");
    }
    return h;
}

static uint32_t le32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static float lef(const uint8_t *p) { uint32_t u = le32(p); float f; memcpy(&f, &u, 4); return f; }

void race_hud_set_course(race_hud *h, const char *course)
{
    char path[64];
    size_t size = 0;
    uint8_t *d;
    if (h) {   /* a new race: no timing lines left from the last one */
        memset(h->lines, 0, sizeof h->lines);
        h->clock = h->lines_end = h->lines_last = 0;
        h->icon_car = -1;
    }
    uint32_t at;
    if (!h) return;
    if (h->map) plat_texture_destroy(h->map);
    if (h->course_logo) plat_texture_destroy(h->course_logo);
    h->map = NULL;
    h->course_logo = NULL;
    h->fade = 1;
    h->fade_rate = 0;
    h->was_counting = h->was_finished = 0;
    h->msg = NULL;
    snprintf(path, sizeof path, "/crs/%s", course);
    d = gt4_vol_read(h->vol, path, &size);
    if (!d || size < 0x100) { free(d); return; }
    /* func_0039AB68 at 0x0039B9B0: the course's Tex1 at header+0xC4,
       drawn at its own dimensions, ending at (604,441). */
    at = le32(d + 0xc4);
    if (at && at < size) {
        tex1_image *im = NULL;
        int count = tex1_decode(d + at, size - at, &im);
        if (count > 0) {
            h->course_logo = plat_texture_create(im[0].width, im[0].height, im[0].rgba);
            h->logo_w = im[0].width;
            h->logo_h = im[0].height;
            tex1_free(im, count);
        }
    }
    at = le32(d + 0x84);
    if ((size_t)at + 0x80 < size && !memcmp(d + at, "GTCM", 4)) {
        const uint8_t *m = d + at;
        tex1_image *im = NULL;
        h->map_w = (int)le32(m + 0x10);
        h->map_h = (int)le32(m + 0x14);
        h->map_cx = lef(m + 0x18);
        h->map_cy = lef(m + 0x1c);
        h->map_scale = lef(m + 0x20);
        h->map_angle = lef(m + 0x24);
        if (tex1_decode(m + le32(m + 0x28), size - at - le32(m + 0x28), &im) > 0) {
            h->map = plat_texture_create(im[0].width, im[0].height, im[0].rgba);
            h->map_tw = im[0].width;
            h->map_th = im[0].height;
            tex1_free(im, 1);
        }
    } else fprintf(stderr, "hud: no map in %s\n", path);
    free(d);
}

void race_hud_close(race_hud *h)
{
    if (!h) return;
    for (int i = 0; i < IMG_COUNT; i++) if (h->img[i].tex) plat_texture_destroy(h->img[i].tex);
    if (h->map) plat_texture_destroy(h->map);
    if (h->course_logo) plat_texture_destroy(h->course_logo);
    free(h->rt_data);
    for (hud_font *f = h->fonts, *next; f; f = next) {
        next = f->next;
        if (f->ink) plat_texture_destroy(f->ink);
        if (f->outline) plat_texture_destroy(f->outline);
        if (f->ok) gt4_font_free(&f->face);
        free(f);
    }
    free(h);
}

/* ----------------------------------------------------------- drawing */

/* An element's colour (0xAABBGGRR: 255 the full colour, func_004AA168
   halving it for the GS; alpha 0x80 opaque) with its alpha times `fade`
   (func_003AFEC8) and its colour times `bright` (func_003AFE18), as
   0..255 */
static void gs_color2(uint32_t c, float bright, float fade, uint8_t out[4])
{
    for (int i = 0; i < 3; i++) {
        float v = (float)((c >> (8 * i)) & 0xff) * bright;
        out[i] = (uint8_t)(v > 255 ? 255 : v < 0 ? 0 : v);
    }
    {
        float a = (float)(c >> 24) * 2 * fade;
        out[3] = (uint8_t)(a > 255 ? 255 : a < 0 ? 0 : a);
    }
}
static void gs_color(uint32_t c, float fade, uint8_t out[4]) { gs_color2(c, 1, fade, out); }

/* D_006184F0: the wide screen (16:9), race_hud_set_widescreen */
static int widescreen;
void race_hud_set_widescreen(int on) { widescreen = on != 0; }

/* func_003AFD18: where a thing of `size` starts for an element at p with
   alignment `mode` (0 from p, 1 ending at p, 2/3 centred); with +4 and
   the wide screen p moves 96 units left, or with +8 right */
static float align(int mode, float p, float size)
{
    if ((mode & 4) && widescreen) p += (mode & 8) ? 96.f : -96.f;
    switch (mode & 3) {
    case 0: return p;
    case 1: return p - size;
    default: return p - size * 0.5f;
    }
}

/* The whole display's light (RaceDisplay +0xBC) over each element */
static float light = 1;
static float hud_y_offset; /* current lower-display transform, 0039F698 */

static void quad(const plat_texture *t, float x, float y, float w, float hh, float u0, float v0, float u1, float v1,
                 const uint8_t color[4])
{
    uint8_t c[4] = { color[0], color[1], color[2], (uint8_t)(color[3] * light) };
    plat_draw_quad(t, x, y + hud_y_offset, w, hh, u0, v0, u1, v1, c);
}

/* func_003AEF18 (its own size) and func_003AF108 (w, h): an image */
static void image(const race_hud *h, int i, int ha, int va, float x, float y, float w, float hh, const uint8_t color[4])
{
    const hud_image *im = &h->img[i];
    if (!im->tex) return;
    if (w <= 0 || hh <= 0) { w = (float)im->w; hh = (float)im->h; }
    quad(im->tex, align(ha, x, w), align(va, y, hh), w, hh, 0, 0, 1, 1, color);
}

/* A glyph's advance: the digits at the pitch of the 0, as the faces draw
   them (hn_20R's 1 advances 12 against 15, its ink centred in the 15 of
   the others), so a figure keeps its place as it counts */
static float spaced;   /* the text context's letter spacing (func_0044DFA0) */
static float advance(const hud_font *f, unsigned c, const gt4_glyph *g)
{
    if (c >= '1' && c <= '9') {
        const gt4_glyph *z = gt4_font_glyph(&f->face, '0');
        if (z) return (float)z->advance + spaced;
    }
    return (float)g->advance + spaced;
}

static float text_width(const hud_font *f, const char *s)
{
    float w = 0;
    while (*s) {
        unsigned c = gt4_text_next(&s);
        const gt4_glyph *g = gt4_font_glyph(&f->face, c);
        if (g) w += advance(f, c, g);
    }
    return w * f->sx;
}

/* The text context (D_00623838) printing at x, y with its alignment
   (func_0044E1E0: horizontal 0 from x, 2 ending at x, 3 centred on it;
   vertical 0 the baseline at y, 4 the top, 0xC centred) */
static void text(const hud_font *f, float x, float y, int flags, const uint8_t color[4], const char *s)
{
    const tex1_image *a;
    float w, base;
    y += hud_y_offset;
    if (!f || !*s) return;
    a = &f->face.atlases[0];
    w = text_width(f, s);
    if ((flags & 3) == 2) x -= w;
    else if ((flags & 3) == 3) x -= w * 0.5f;
    switch (flags & 0xc) {
    case 4: base = y + f->face.baseline * f->sy; break;
    case 0xc: base = y - f->face.height * 0.5f * f->sy + f->face.baseline * f->sy; break;
    case 8: base = y - (f->face.height - f->face.baseline) * f->sy; break;
    default: base = y; break;
    }
    for (int pass = f->palette ? 1 : 0; pass < 2; pass++) {
        static const uint8_t black[4] = { 0, 0, 0, 255 };
        uint8_t c[4];
        const char *t = s;
        float pen = x;
        memcpy(c, pass ? color : black, 4);
        c[3] = (uint8_t)(color[3] * light);
        while (*t) {
            unsigned ch = gt4_text_next(&t);
            const gt4_glyph *g = gt4_font_glyph(&f->face, ch);
            if (!g) continue;
            if (g->w && g->h) {
                float u0 = (float)g->u / a->width, v0 = (float)g->v / a->height;
                float u1 = (float)(g->u + g->w) / a->width, v1 = (float)(g->v + g->h) / a->height;
                plat_draw_glyph(pass ? f->ink : f->outline, pen + g->x * f->sx, base + g->y * f->sy,
                                g->w * f->sx, g->h * f->sy, base, 0, u0, v0, u1, v1, c);
            }
            pen += advance(f, ch, g) * f->sx;
        }
    }
}

/* func_003AFF38: a time as M'SS.mmm (up to 999'59.999), none as the dash
   glyph (0x7F) in its place (D_006A2348) */
static void format_time(uint32_t ms, char out[16])
{
    if (ms > 0x157529FEu) { memcpy(out, "\x7f'\x7f\x7f.\x7f\x7f\x7f", 9); return; }
    if ((int32_t)ms > 59999999) ms = 59999999;
    snprintf(out, 16, "%u'%02u.%03u", ms / 60000, ms / 1000 % 60, ms % 1000);
}

/* func_003B00A8: the race's time, h:mm'ss.mmm (at most 9:59'59.999) */
static void format_total(uint32_t ms, char out[16])
{
    if (ms >= 0x157529FEu) { memcpy(out, "\x7f:\x7f\x7f'\x7f\x7f.\x7f\x7f\x7f", 12); return; }
    if (ms > 0x22550FFu) ms = 0x22550FFu;
    snprintf(out, 16, "%u:%02u'%02u.%03u", ms / 3600000, ms / 60000 % 60, ms / 1000 % 60, ms % 1000);
}

/* ------------------------------------------------ the top of the screen */

typedef gt4_split_place place;

/* the labels (RaceTexturePanel at RaceDisplay +0x1668, +0x1684, +0x16A0,
   +0x16BC): Position, Lap, Best Lap, Last Lap */
static const place label_at[4] = {
    { 56, 21, 0, 0, 6, 0 }, { 135, 21, 0, 0, 6, 0 }, { 550, 21, 0, 0, 14, 0 }, { 550, 74, 0, 0, 14, 0 },
};
static const place rank_at = { 56, 86, 42, 45, 6, 1 };      /* +0x1398, RaceRankDisplay */
static const place lap_at = { 135, 74, 100, 0, 6, 1 };      /* +0x1300, RaceLapDisplay */
static const place best_at = { 550, 58, 82, 15, 14, 0 };     /* +0x14A8, RaceTimeDisplay */
static const place last_at = { 550, 110, 82, 15, 14, 0 };    /* +0x1588 */
static const place timer_at = { 335, 107, 0, 0, 2, 1 };     /* +0x15F8, its icon at (-8, +7) */

/* func_003A3B58: the box, and the rank's figure rolling to the new one
   (a fifth of the way each frame, at least 0.01) in FC_26 inside it */
static void draw_rank(race_hud *h, int rank, const place *p)
{
    static const uint8_t white[4] = { 255, 255, 255, 255 };
    hud_font *f = font(h, "FC_26");
    float x0 = align(p->ha, (float)p->x, (float)p->w), y0 = align(p->va, (float)p->y, (float)p->h);
    uint8_t c[4];
    float d, frac, x, y, fh;
    int n;
    if (!h->rank_shown) h->rank_now = (float)rank;                     /* func_003A3EC8 */
    h->rank_shown = rank;
    image(h, IMG_POSITION_BASE, 0, 0, x0, y0, (float)p->w, (float)p->h, white);
    if (!f) return;
    d = ((float)rank - h->rank_now) * 0.2f;
    if (d > 0) h->rank_now = fminf(h->rank_now + fmaxf(d, 0.01f), (float)rank);
    else if (d < 0) h->rank_now = fmaxf(h->rank_now + fminf(d, -0.01f), (float)rank);
    n = (int)h->rank_now;
    frac = h->rank_now - (float)n;
    fh = (float)f->face.height;
    x = x0 + p->w * 0.5f - 1 + (n == 1 ? 4 : 0);
    y = y0 + p->h - 19 - frac * fh;
    gs_color(0x80C8C8C8u, 1, c);
    plat_set_clip(0, y0 + 8, 640, p->h - 11);
    {
        char s[2] = { (char)('0' + n), 0 };
        text(f, x, y, 0xf, c, s);
        if (frac != 0) {
            s[0] = (char)('0' + n + 1);
            text(f, x, y + fh, 0xf, c, s);
        }
    }
    plat_set_clip(0, 0, 0, 0);
}

/* func_003AB010 with func_003AAFA0's text: "%d/%d" (the lap up to 999),
   "%d" with no laps; FC_26*80,80, squeezed into the element's width */
static void draw_lap(race_hud *h, int lap, int laps)
{
    const place *p = &lap_at;
    hud_font *f = font(h, "FC_26*80,80");
    char s[24];
    uint8_t c[4];
    float w, sx = 1, x, y;
    if (!f) return;
    if (lap > 999) lap = 999;
    if (laps) snprintf(s, sizeof s, "%d/%d", lap, laps < 999 ? laps : 999);
    else snprintf(s, sizeof s, "%d", lap);
    w = text_width(f, s);
    if (p->w > 0 && w > 0 && p->w / w < 1) { sx = p->w / w; w *= sx; }
    x = align(p->ha, (float)p->x, w);
    y = align(p->va, (float)p->y, (float)f->face.height) + (float)f->face.height;
    gs_color(0x80C8C8C8u, 1, c);
    {
        hud_font g = *f;
        g.sx *= sx;
        text(&g, x, y, 0, c, s);
    }
}

/* func_003A43A8: a time in hn_20R, its baseline at the element's y; the
   timer's icon (GT4_meter_texture_laptime) ends at the text's start plus
   (-8, +7) */
static void draw_time_text(race_hud *h, const place *p, const char *s, int icon)
{
    static const uint8_t white[4] = { 255, 255, 255, 255 };
    hud_font *f = font(h, "hn_20R");
    uint8_t c[4];
    float x;
    if (!f) return;
    x = align(p->ha, (float)p->x, text_width(f, s));
    gs_color(0x80C8C8C8u, 1, c);
    text(f, x, (float)p->y, 0, c, s);
    if (icon) image(h, IMG_LAPTIME, 1, 1, x + p->w - 8, (float)(p->y + p->h + 7), 0, 0, white);
}
static void draw_time(race_hud *h, const place *p, uint32_t ms, int icon)
{
    char s[16];
    format_time(ms, s);
    draw_time_text(h, p, s, icon);
}

/* ------------------------- the chase camera's panel (RaceSimplePanel) */

/* its elements, at RaceDisplay +0xD58 (the panel at (320, 464), shown) */
static const place remote_at = { 614, 420, 82, 26, 13, 0 };   /* +0x2C, RaceTexturePanel */
static const place speed_at = { 581, 438, 0, 0, 13, 1 };      /* +0x48, RaceDigitalSpeedmeter */
static const place fuel_at = { 107, 434, 86, 11, 4, 1 };     /* +0x80, RaceFuelMeter */
static const place tacho_at = { 614, 464, 160, 10, 13, 1 };   /* +0x100, RaceBattleTachometer */
static const place gear_at = { 526, 420, 24, 26, 13, 0 };     /* +0x124, RaceShiftPositionDisplay */
static const place lamp_at = { 498, 436, 8, 8, 13, 0 };       /* +0x1D4, RaceShiftTimingLampDisplay */
static const place pedals_at = { 26, 464, 22, 66, 4, 1 };    /* +0x210, RaceABMonitor */
static const place g_at = { 320, 464, 214, 24, 2, 1 };       /* +0x290, RaceSideGravityMeter */
static const place sign_at[3] = {                             /* +0x2BC, +0x300, +0x344, RaceIndicator */
    { 119, 464, 24, 24, 6, 1 }, { 147, 464, 24, 24, 6, 1 }, { 175, 464, 24, 24, 6, 1 },
};

static void rect(float x, float y, float w, float hh, const uint8_t c[4])
{
    uint8_t a = (uint8_t)(c[3] * light);
    const uint8_t q[4][4] = { { c[0], c[1], c[2], a }, { c[0], c[1], c[2], a },
                              { c[0], c[1], c[2], a }, { c[0], c[1], c[2], a } };
    plat_draw_quad4(NULL, x, y + hud_y_offset, w, hh, 0, 0, 1, 1, q);
}

/* func_003A3F98: the speed (func_00472940: m/s to km/h or mph, rounded)
   in GT4meter#1, right-aligned on "000" from the element's x, the larger
   of the last two samples, taken every other frame */
static void draw_speed(race_hud *h, const place *p, int speed)
{
    hud_font *f = font(h, "GT4meter#1");
    char s[8];
    uint8_t c[4];
    float right;
    if (!f) return;
    if (++h->speed_tick & 1) {
        h->speed_prev = h->speed_now;
        h->speed_now = speed < 0 ? -speed : speed;
    }
    right = align(p->ha, (float)p->x, text_width(f, "000")) + text_width(f, "000");
    snprintf(s, sizeof s, "%3d", h->speed_now > h->speed_prev ? h->speed_now : h->speed_prev);
    gs_color(0x80E3B896u, 1, c);
    text(f, right, (float)p->y, 2, c, s);
}

/* func_003EC930: rev_bar dark at half alpha, then lit up to rpm * 0.7 of
   the limit (in 40 steps, func_003ECA40), and the red line from 70.5% */
static void draw_tacho(race_hud *h, float rpm, float limit)
{
    const place *p = &tacho_at;
    const hud_image *im = &h->img[IMG_REV_BAR];
    float x0 = align(p->ha, (float)p->x, (float)p->w), y0 = align(p->va, (float)p->y, (float)p->h), frac;
    uint8_t c[4];
    if (!im->tex || limit <= 0) return;
    gs_color(0x80000000u, 0.5f, c);
    quad(im->tex, x0, y0, (float)p->w, (float)p->h, 0, 0, 1, 1, c);
    frac = fminf(rpm * 0.7f / limit, 1);
    frac = (float)(int)(frac * 40) / 40;
    if (frac > 0) {
        gs_color(0x80FFFFFFu, 1, c);
        quad(im->tex, x0, y0, p->w * frac, (float)p->h, 0, 0, frac, 1, c);
    }
    gs_color(0x70000099u, 1, c);
    rect(x0 + p->w * 0.705f, y0 - 3, (x0 + p->w - 1) - (x0 + p->w * 0.705f), 2, c);
}

/* func_003A6DE0: gear_base, and the gear in GT4meter#1 centred on it 12
   above its bottom: R for reverse, N (the font's C) in neutral; dark
   unless the shift flash lights it (+0x38, not kept yet) */
static void draw_gear(race_hud *h, int gear, int neutral)
{
    static const uint8_t white[4] = { 255, 255, 255, 255 };
    const place *p = &gear_at;
    hud_font *f = font(h, "GT4meter#1");
    float x0 = align(p->ha, (float)p->x, (float)p->w), y0 = align(p->va, (float)p->y, (float)p->h);
    char s[2] = { (char)(gear == 0 ? 'R' : neutral ? 'C' : '0' + gear), 0 };
    uint8_t c[4];
    image(h, IMG_GEAR_BASE, 0, 0, x0, y0, (float)p->w, (float)p->h, white);
    gs_color2(0x80E3B896u, 0, 1, c);
    if (f) text(f, x0 + p->w / 2, y0 + p->h - 12, 0xf, c, s);
}

/* func_003A7360: timing_lamp, red as bright as the lamp is lit times 1.5 */
static void draw_lamp(race_hud *h, int lit)
{
    uint8_t c[4];
    gs_color2(0x800000FFu, (lit ? 1.0f : 0) * 1.5f, 1, c);
    image(h, IMG_TIMING_LAMP, lamp_at.ha, lamp_at.va, (float)lamp_at.x, (float)lamp_at.y, 0, 0, c);
}

/* func_003A5648, func_003A54F8, func_003AB468: the pedals' frame and two
   bars filling from the bottom, the throttle (blue) inside its right
   edge, the brake (red) at its left, 4 x 56 from 4 below its top */
static void draw_pedals(race_hud *h, float throttle, float brake)
{
    static const uint8_t white[4] = { 255, 255, 255, 255 };
    const place *p = &pedals_at;
    float x0 = align(p->ha, (float)p->x, (float)p->w), y0 = align(p->va, (float)p->y, (float)p->h);
    const float v[2] = { throttle, brake }, bx[2] = { x0 + p->w - 5, x0 + 1 };
    const uint32_t col[2] = { 0x80E3B896u, 0x800000F0u };
    image(h, IMG_PEDALS, 0, 0, x0, y0, (float)p->w, (float)p->h, white);
    for (int i = 0; i < 2; i++) {
        float f = fminf(fmaxf(v[i], 0), 1);
        uint8_t c[4];
        gs_color(col[i], 1, c);
        if (f > 0) rect(bx[i], y0 + 4 + 56 * (1 - f), 4, 56 * f, c);
    }
}

/* func_003AB240: g_meter_base, and its point moved off the middle by the
   side force over 2 G (at most), 0.944 of the half width, 8 below its top */
static void draw_g(race_hud *h, float g)
{
    static const uint8_t white[4] = { 255, 255, 255, 255 };
    const place *p = &g_at;
    float x0 = align(p->ha, (float)p->x, (float)p->w), y0 = align(p->va, (float)p->y, (float)p->h);
    uint8_t c[4];
    g = fmaxf(fminf(g, 2), -2) / 2;
    image(h, IMG_G_BASE, 0, 0, x0, y0, (float)p->w, (float)p->h, white);
    gs_color(0x80E3B896u, 1, c);
    image(h, IMG_G_POINT, 2, 2, x0 + p->w * 0.5f + p->w * 0.5f * 0.944f * g, y0 + 8, 0, 0, c);
}

/* func_003A57A8: fuel_b0, and fuel_b1 over it to 10 steps of the tank
   (5.2 units each from 15, 4 more from the sixth) */
static void draw_fuel_at(race_hud *h, const place *p, float fuel, float cap)
{
    static const uint8_t white[4] = { 255, 255, 255, 255 };
    const hud_image *im = &h->img[IMG_FUEL_B1];
    float x0 = align(p->ha, (float)p->x, (float)p->w), y0 = align(p->va, (float)p->y, (float)p->h), w;
    int n;
    uint8_t c[4];
    image(h, IMG_FUEL_B0, 0, 0, x0, y0, (float)p->w, (float)p->h, white);
    if (!im->tex || cap <= 0) return;
    n = (int)(fminf(fmaxf(fuel / cap, 0), 1) * 10 + 0.5f);
    w = n * 5.2f + 15 + (n >= 6 ? 4 : 0);
    gs_color(0x80E3B896u, 1, c);
    quad(im->tex, x0, y0, w, 11, 0, 0, w / (float)im->w, 1, c);
}
static void draw_fuel(race_hud *h, float fuel, float cap) { draw_fuel_at(h, &fuel_at, fuel, cap); }
static void draw_refueling(race_hud *h,float fuel,float cap)
{
    gt4_pit_refuel_layout p;gt4_pit_refuel_meter(fuel,cap,&p);
    static const uint8_t white[4]={255,255,255,255};uint8_t c[4];gs_color(p.color,1,c);
    const int pictures[3]={IMG_REFUEL_BG,IMG_REFUEL_BAR,IMG_REFUEL_SCALE};
    for(unsigned i=0;i<3;i++){
        const float *r=p.rect[i];
        /* 003AF4F8 stretches the full image into its endpoints; a zero
           width must stay zero instead of image()'s intrinsic size. */
        quad(h->img[pictures[i]].tex,r[0],r[1],r[2],r[3],0,0,1,1,i==1?c:white);
    }
    hud_font *f=font(h,"hn_20R");char label[32];snprintf(label,sizeof label,"%d/%d",p.fuel,p.capacity);
    if(f)text(f,p.text[0],p.text[1],2,c,label);
}

/* B-Spec's panel: the fuel (+0x80) and the tyres (+0xD0) under the car's
   outline, bottom left. A: places from the user's PCSX2 print */
static const place bspec_fuel_at = { 74, 422, 86, 11, 0, 1 };   /* x the savestates' +0x80 */
static const place bspec_tire_at = { 30, 436, 40, 68, 0, 1 };
/* func_003A5FD8: tire_wear tinted 0x80877B80, the four tyres 6 x 11 at
   D_006A1520 (4, 8), (30, 8), (4, 50), (30, 50) in their colours */
static void draw_tires(race_hud *h, const race_hud_state *s)
{
    static const int at[4][2] = { { 4, 8 }, { 30, 8 }, { 4, 50 }, { 30, 50 } };
    const place *p = &bspec_tire_at;
    float x0 = align(p->ha, (float)p->x, (float)p->w), y0 = align(p->va, (float)p->y, (float)p->h);
    uint8_t c[4];
    gs_color(0x80877B80u, 1, c);
    image(h, IMG_TIRE_WEAR, 0, 0, x0, y0, (float)p->w, (float)p->h, c);
    for (int i = 0; i < 4; i++) {
        uint8_t k[4] = { s->tire_rgb[i][0], s->tire_rgb[i][1], s->tire_rgb[i][2], (uint8_t)(255 * light) };
        rect(x0 + at[i][0], y0 + at[i][1], 6, 11, k);
    }
}

/* func_003A6C28: a sign, its colour as bright as its state over 2 (3
   blinks), at least last frame's */
static void draw_sign(race_hud *h, int i, int state, float blink)
{
    static const int img[3] = { IMG_SIGN_ASM, IMG_SIGN_BRAKE, IMG_SIGN_OIL };
    static const uint32_t col[3] = { 0x80E3B896u, 0x800000C8u, 0x800000C8u };
    const place *p = &sign_at[i];
    float b = state == 3 ? blink : state * 0.5f;
    uint8_t c[4];
    gs_color2(col[i], fmaxf(b, h->sign_last[i]), 1, c);
    image(h, img[i], p->ha, p->va, (float)p->x, (float)p->y, (float)p->w, (float)p->h, c);
    h->sign_last[i] = b;
}

/* ------------------------------------------- the mini map (RaceMiniMap) */

static const place map_at = { 24, 96, 0, 0, 4, 0 };   /* RaceDisplay +0x1108 */

/* func_003BEBA0: a course point on the map, about its centre */
static void map_point(const race_hud *h, float x, float z, float *mx, float *my)
{
    float dx = x - h->map_cx, dz = z - h->map_cy, a = h->map_angle * 3.14159265f / 180;
    float s = sinf(a) * h->map_scale, c = cosf(a) * h->map_scale;
    *mx = dx * c - dz * s;
    *my = dx * s + dz * c;
}

/* func_003BEE78 with func_003BF150: the map centred in its 180 x 180 (at
   the element's place), the player's point dark under it, the others'
   (point_rival) white over it, then the player's (point_target) with the
   blinker's light; a car shows when it races (3), faded when 2 */
static void draw_map(race_hud *h, const race_hud_state *s, float light)
{
    static const uint8_t white[4] = { 255, 255, 255, 255 };
    float cx, cy;
    const place *p = s->split ? &gt4_split_layout[SPLIT_MAP] : &map_at;
    if (!h->map) return;
    cx = align(p->ha, (float)p->x, (float)h->map_w) + h->map_w * 0.5f;
    cy = align(p->va, (float)p->y, (float)h->map_h) + h->map_h * 0.5f;
    for (int pass = 0; pass < 4; pass++) {
        if (pass == 1) {
            quad(h->map, cx - h->map_tw * 0.5f, cy - h->map_th * 0.5f, (float)h->map_tw, (float)h->map_th,
                           0, 0, 1, 1, white);
            continue;
        }
        for (int i = 0; i < s->cars && i < 6; i++) {
            int me = i == (s->split ? s->player : 0), img;
            uint32_t col;
            uint8_t c[4];
            float mx, my;
            if (me != (pass != 2)) continue;
            if (s->map_show[i] != 3 && s->map_show[i] != 2) continue;
            img = me ? IMG_POINT_TARGET : IMG_POINT_RIVAL;
            col = pass == 0 ? 0x80000000u : 0x80FFFFFFu;
            gs_color(col, pass == 3 ? light * 0.7f + 0.3f : 1, c);
            if (s->map_show[i] == 2) c[3] /= 2;
            map_point(h, s->map_x[i], s->map_y[i], &mx, &my);
            image(h, img, 2, 2, cx + mx, cy + my, 0, 0, c);
        }
    }
}

/* B-Spec's Race Monitor tyres (its TIRE_PANEL's "tire" extrender; the
   monitor draws the panel's tyres through 003A5FD8 at 003D8038): the
   car's outline the box's height, the fuel to its right. A: the class's
   placement inside the box taken from the user's PCSX2 print */
void race_hud_draw_tire_panel(race_hud *h, const race_hud_state *s, float x, float y, float w, float hh, float alpha)
{
    static const int at[4][2] = { { 4, 8 }, { 30, 8 }, { 4, 50 }, { 30, 50 } };
    uint8_t c[4];
    float k, x0, y0;
    if (!h || !s || alpha <= 0) return;
    plat_video_set_view(640, 480);
    light = fminf(alpha, 1);
    k = hh * 0.8f / 68;
    x0 = x + 8;
    y0 = y + hh * 0.1f;
    gs_color(0x80877B80u, 1, c);
    image(h, IMG_TIRE_WEAR, 0, 0, x0, y0, 40 * k, 68 * k, c);
    for (int i = 0; i < 4; i++) {
        uint8_t q[4] = { s->tire_rgb[i][0], s->tire_rgb[i][1], s->tire_rgb[i][2], (uint8_t)(255 * light) };
        rect(x0 + at[i][0] * k, y0 + at[i][1] * k, 6 * k, 11 * k, q);
    }
    if (s->fuel_cap > 0) {
        place f = { (int)(x0 + 48 * k + 8), (int)(y + hh * 0.5f), 86, 11, 0, 1 };
        draw_fuel_at(h, &f, s->fuel, s->fuel_cap);
    }
    (void)w;
}

/* B-Spec's Race Monitor map (its MAP_PANEL extrender, the monitor's own
   RaceMiniMap +0x358, 003AE138): the course about (cx, cy), each car its
   letter (A + its grid slot), the watched one orange; the user's PCSX2
   print shows them so. A: the class's own placement and font not read */
void race_hud_draw_map_cars(race_hud *h, const race_hud_state *s, float cx, float cy, float box, const int letter[6], int lit, float alpha)
{
    uint8_t white[4] = { 255, 255, 255, 255 }, ink[4] = { 0xff, 0xff, 0xff, 255 }, orange[4] = { 0xff, 0x99, 0x00, 255 };
    hud_font *f = font(h, "hn_20R");
    if (!h || !s || !h->map || alpha <= 0) return;
    white[3] = ink[3] = orange[3] = (uint8_t)(255 * fminf(alpha, 1));
    plat_video_set_view(640, 480);
    light = 1;
    /* in the panel's box (179 square), the cars in proportion. A: about
       0.65 of the box as the user's PCSX2 print has it (the monitor's
       RaceMiniMap scale, 003ADF00 / 0044E390, not read) */
    float k = 0.65f * fminf(1, fminf(box / (float)h->map_w, box / (float)h->map_h));
    quad(h->map, cx - h->map_tw * 0.5f * k, cy - h->map_th * 0.5f * k, h->map_tw * k, h->map_th * k, 0, 0, 1, 1, white);
    if (!f) return;
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < s->cars && i < 6; i++) {
            char t[2] = { (char)('A' + (letter[i] >= 0 && letter[i] < 6 ? letter[i] : 0)), 0 };
            float mx, my;
            if ((i == lit) != (pass == 1)) continue;   /* the watched car's on top */
            if (s->map_show[i] != 3 && s->map_show[i] != 2) continue;
            map_point(h, s->map_x[i], s->map_y[i], &mx, &my);
            text(f, cx + mx * k - text_width(f, t) * 0.5f, cy + my * k, 0, i == lit ? orange : ink, t);
        }
}

/* ------------------------------------------------- the big message */

static const char *race_text(const race_hud *h, const char *key)
{
    const char *s = h->rt_data ? gt4_rtext_get(&h->rt, "RaceDisplay", key) : NULL;
    return s ? s : "";
}

/* func_003A4E00/func_003A98E8: delay, rise, hold until duration minus
   the fall time, then fall. The race clock supplies elapsed seconds. */
static float intro_alpha(float elapsed, float delay, float duration, float rise, float fall)
{
    float t = elapsed - delay;
    return fminf(fmaxf(t / rise, 0), fminf(fmaxf((duration - t) / fall, 0), 1));
}

/* func_0039C188 (standing) / func_0039C318 (rolling) call vtable+0x158,
   func_0039C570. Layout from func_0039AB68: +0x267C at (320,264),
   +0x27CC at (320,304), width 560; func_003A4AE8 uses the text baseline.
   The logo (+0x11B0) has its own envelope, independent of the HUD. */
static void draw_intro(race_hud *h, const race_hud_state *s)
{
    float elapsed = s->intro_elapsed;
    float duration, alpha, logo_alpha;
    uint8_t c[4];
    char points[128];
    const char *label, *format;
    hud_font *f;
    if (!s->rolling && (!s->start_seconds || s->start_frames <= 0)) return;
    if (s->rolling) {
        duration = fminf(fmaxf(s->start_seconds - .5f, 2), 3.5f);
        alpha = intro_alpha(elapsed, .2f, duration, .1f, .3f);
        logo_alpha = intro_alpha(elapsed, .2f, fminf(fmaxf((float)s->start_seconds, 0), 2), .1f, .3f);
    } else {
        duration = fminf(fmaxf(s->start_seconds - 3.5f, 2), 5);
        alpha = intro_alpha(elapsed, .5f, duration, .5f, 1);
        logo_alpha = intro_alpha(elapsed, .5f, fminf(fmaxf(s->start_seconds - 5.f, 0), 5), .5f, 1);
    }
    light = 1;
    if (alpha > 0 && s->cars > 1) {
        spaced = 1.2f; /* message+0x24 == 1, func_003A4AE8 */
        label = race_text(h, "AspecRaceFormat1");
        format = race_text(h, "AspecRaceFormat2");
        f = font(h, "hb_24#1");
        gs_color(0x80c8c8c8u, alpha, c);
        text(f, 320, 264, 3, c, label);
        if (*format) {
            snprintf(points, sizeof points, format, s->aspec_points);
            f = font(h, "hb_24#1*130,130");
            gs_color(0x800086e6u, alpha, c);
            text(f, 320, 304, 3, c, points);
        }
        spaced = 0;
    }
    if (logo_alpha > 0 && h->course_logo) {
        gs_color(0x80ffffffu, logo_alpha, c);
        quad(h->course_logo, 604.f - h->logo_w, 441.f - h->logo_h,
             (float)h->logo_w, (float)h->logo_h, 0, 0, 1, 1, c);
    }
}

/* ------------------------------------------------- the timing lines */

/* func_003A50D0: a line on the list at (320, 139) moved by (dx, dy) (the
   list's +0x18/+0x1C, back to 0 after), in a free one of its 8 lines, for
   `dur` seconds faded in and out in 0.15: with `with_last` 0 after the
   lines already queued (+0x28), else from where the last one starts
   (+0x24, the gap with its section's time) */
static float add_line(race_hud *h, const char *text, uint32_t color, float dx, float dy, float dur, int with_last)
{
    int k;
    float start;
    for (k = 0; k < 8; k++) if (h->clock >= h->lines[k].start + h->lines[k].dur) break;
    if (k == 8) return -1;
    if (!with_last) {
        start = fmaxf(h->lines_end, h->clock);
        h->lines_end = start + dur;
    } else {
        start = fmaxf(h->lines_last, h->clock);
        h->lines_end = fmaxf(h->lines_end, start + dur);
    }
    h->lines_last = start;
    snprintf(h->lines[k].text, sizeof h->lines[k].text, "%s", text);
    h->lines[k].color = color;
    h->lines[k].x = 320 + dx;
    h->lines[k].y = 139 + dy;
    h->lines[k].start = start;
    h->lines[k].dur = dur;
    return start;
}

void race_hud_section_time(race_hud *h, int section, uint32_t ms)
{
    char t[16], line[48];
    if (!h) return;
    format_time(ms, t);
    snprintf(line, sizeof line, "%s%d   %s", race_text(h, "WordOfSectionTime"), section, t);   /* D_006A0770 */
    add_line(h, line, 0x80C8C8C8u, 0, 0, 3, 0);
}

/* func_003A0A78 with the display's +0x58 bit 10 clear and the mode not 8
   or 9 (the Arcade's captures): the time alone */
void race_hud_lap_time(race_hud *h, uint32_t ms)
{
    char t[16];
    if (!h) return;
    format_time(ms, t);
    add_line(h, t, 0x80C8C8C8u, 0, 0, 3, 0);
}

/* func_003A12A0 (message 0x13, a top speed already in the option's
   unit): 003B0230 "ddd.d" (dashes for none), the unit (00472680), a line
   as the lap's (hn_20R, 0x80C8C8C8, 3 s) */
void race_hud_top_speed(race_hud *h, float speed, int mph)
{
    char t[24];
    if (!h) return;
    if (!(speed > 0)) snprintf(t, sizeof t, "\x7f\x7f\x7f.\x7f");
    else {
        float v = speed < 999.9f ? speed : 999.9f;
        int n = (int)(v * 10.f + 0.5f);
        snprintf(t, sizeof t, "%d%d%d.%d", n / 1000, n % 1000 / 100, n % 100 / 10, n % 10);
    }
    strncat(t, mph ? "mph" : "km/h", sizeof t - strlen(t) - 1);
    add_line(h, t, 0x80C8C8C8u, 0, 0, 3, 0);
}

void race_hud_gap(race_hud *h, int32_t ms, int field, int car)
{
    char line[48];
    uint32_t color;
    float start;
    if (!h || h->shown < 1) return;   /* func_003A04B0: none while the display is not fully lit */
    /* func_003A04B0: field 1 light blue, 2 red, else by its sign */
    color = field == 1 ? 0x80E3B896u : field == 2 || ms > 0 ? 0x800000C8u : 0x80E3B896u;
    line[0] = ms < 0 ? '\x7f' : '+';   /* the dash glyph for a lead */
    line[1] = ' ';
    format_time((uint32_t)(ms < 0 ? -ms : ms), line + 2);
    start = add_line(h, line, color, car >= 0 ? 32 : 0, 25, 3, 1);
    if (car >= 0 && start >= 0) {   /* +0x2D14 at the list's place less (89, -22), for 2.85 s */
        h->icon_car = car;
        h->icon_x = 320 - 89;
        h->icon_y = 139 + 22;
        h->icon_start = start;
    }
}

/* func_003A0E40: laps behind ("+1 LAP", the lead's "1 LAP" in blue,
   func_003B0320: the sign, then "%d %s" with WordOfLap for one,
   WordOfLaps else), with the icon at the list's place less (82, -22) */
void race_hud_laps_behind(race_hud *h, int laps, int car)
{
    char line[48];
    float start;
    if (!h || h->shown < 1) return;   /* func_003A0308 */
    line[0] = laps < 0 ? '' : '+';
    snprintf(line + 1, sizeof line - 1, "%d %s", laps < 0 ? -laps : laps,
             race_text(h, (laps < 0 ? -laps : laps) == 1 ? "WordOfLap" : "WordOfLaps"));
    start = add_line(h, line, laps < 0 ? 0x80FF8080u : 0x800000C8u, car >= 0 ? 32 : 0, 25, 3, 1);
    if (car >= 0 && start >= 0) {
        h->icon_car = car;
        h->icon_x = 320 - 82;
        h->icon_y = 139 + 22;
        h->icon_start = start;
    }
}

void race_hud_set_icon(race_hud *h, void (*icon)(void *, int, float, float, float, float), void *user)
{
    if (h) { h->icon = icon; h->icon_user = user; }
}

/* func_003A5060 / func_003A4AE8: each line in hn_20R, centred, as light as
   its envelope; the icon (func_003A6678: the car's 96x64 picture in a
   64x32 box from its place, 003AFD78 with the alignment 003A0ECC sets by
   003AECA0: x 0, its left edge; y 2, its middle) */
static void draw_lines(race_hud *h, float dt)
{
    hud_font *f = font(h, "hn_20R");
    uint8_t c[4];
    h->clock += dt;
    for (int k = 0; k < 8; k++) {
        float a = intro_alpha(h->clock, h->lines[k].start, h->lines[k].dur, .15f, .15f);
        if (a <= 0 || !f) continue;
        gs_color(h->lines[k].color, a * light, c);
        text(f, h->lines[k].x, h->lines[k].y, 3, c, h->lines[k].text);
    }
    if (h->icon && h->icon_car >= 0) {
        float a = intro_alpha(h->clock, h->icon_start, 2.85f, .15f, .15f);
        if (a > 0) h->icon(h->icon_user, h->icon_car, h->icon_x, h->icon_y - 16, 64, 32);
        else if (h->clock > h->icon_start + 2.85f) h->icon_car = -1;
    }
}

/* func_003A4CF0: a message for `hold` seconds, then fading out */
static void message(race_hud *h, const char *text, float hold, float fade)
{
    h->msg = text;
    h->msg_hold = hold;
    h->msg_fade = fade;
    h->msg_t = 0;
}

/* func_003A4AE8: the message in FC_48 (its colour 0x80C8C8C8), centred on
   (320, 224), its baseline there, as light as its envelope */
static void draw_message(race_hud *h, float dt)
{
    hud_font *f;
    uint8_t c[4];
    float a;
    if (!h->msg) return;
    h->msg_t += dt;
    a = h->msg_t <= h->msg_hold ? 1 : h->msg_fade > 0 ? 1 - (h->msg_t - h->msg_hold) / h->msg_fade : 0;
    if (a <= 0) { h->msg = NULL; return; }
    f = font(h, "FC_48");
    if (!f) return;
    gs_color(0x80C8C8C8u, a, c);
    text(f, align(2, 320, text_width(f, h->msg)), 224, 0, c, h->msg);
}

/* func_0039D390 runs after drawing the message: at count_frames%60 == 1
   it sets the next second's numeral. Counts 3/2/1 last 0.8 s including
   a 0.15 s fade (func_003A4E00), in FC_48 at the message's baseline.
   Derive the pulse from simulation frames so 4x and skipped draw frames
   retain the original timing. Higher counts require a staggered start
   and a fully visible display (0x0039D488..0x0039D4C4). */
static void draw_countdown(race_hud *h, const race_hud_state *s, float display_light)
{
    int frames = s->count_frames, count;
    float elapsed, alpha;
    char number[16];
    uint8_t c[4];
    hud_font *f;
    if (s->rolling || frames <= 0) return;
    count = (frames + 59) / 60;
    /* RaceDisplayLicense 003BD740 +0x20..0x44 omits signals 1 and 2
       for examinations with a contact/pylon limit. */
    if(s->license && s->license_pylon_limit>0 && count>0 && count<3)return;
    elapsed = (count * 60 - frames) / 60.f;
    if (count < 4) alpha = fminf(fmaxf((.8f - elapsed) / .15f, 0), 1);
    else {
        if (frames == s->start_frames || display_light < 1) return;
        alpha = 1;
    }
    if (alpha <= 0) return;
    snprintf(number, sizeof number, "%d", count);
    f = font(h, "FC_48");
    if (!f) return;
    gs_color(0x80c8c8c8u, alpha, c);
    text(f, align(2, 320, text_width(f, number)), 224, 0, c, number);
}

/* func_0039D6D8 / func_0039D7C8: the car's signal in the second message
   (RaceDisplay +0x228C: hn_20R, 0x80C8C8C8, centred on x 320 with its
   baseline at 430 over the chase panel, 348 over the onboard one), shown
   while the signal lasts (func_0039D9D8: 0.04 s, no fade); the pit
   messages spaced 1.2; the wrong way (1-3, 13, 14) the arrow blinking
   five times (0.2 s, on 0.3 of it) */
static void draw_signal(race_hud *h, const race_hud_state *s, float dt)
{
    static const int keys[16] = { 0, -1, -1, -1, 0x150, 0x154, 0x158, 0x15c, 0x160, 0x164, 0x168, 0x16c, 0x170, -1, -1, 0x14 };
    static const char *const text_of[] = {
        "FULL THROTTLE", "GET READY TO DRIVE", "GO!", "Pit Lane entrance speed too high",
        "No Pitting-Stop allowed during last lap.", "AUTO DRIVE", "SPEED LIMIT", "In progress..", "Return to course",
    };
    static const char *const key_of[] = {
        "AutoDriveFullThrottle", "AutoDriveReady", "AutoDriveGo", "PitTooFast", "PitNotAllowed",
        "AutoDrive", "SpeedLimit", "InProgress", "ReturnToCourse",
    };
    float y = s->panel ? 348.f : 430.f;
    int code = s->signal;
    /* RaceDisplayLicense 003BD638 +0x74..0xe0: leader wait supersedes
       the normal signal, original rtext table +0x128 (KeepLeader). */
    if(s->license && s->license_wait>0){
        char t[96];uint8_t c[4];hud_font *f=font(h,"hn_20R");
        const char *fmt=race_text(h,"KeepLeader");if(!f || !*fmt)return;
        snprintf(t,sizeof t,fmt,s->license_wait);gs_color(0x80c8c8c8u,1,c);
        text(f,align(2,320,text_width(f,t)),y,0,c,t);return;
    }
    /* func_0039D6D8: while a penalty runs (func_0035FC80: its frames left,
       in whole seconds rounded up) the message is the rtext "Penalty"
       ("%d sec. penalty", the text table's +0x124), in 0x800000C8, coming
       in over 0.2 s (func_003A4CF0), instead of the signal */
    if (s->penalty > 0) {
        hud_font *f = font(h, "hn_20R");
        const char *fmt = race_text(h, "Penalty");
        char t[64];
        uint8_t c[4];
        h->penalty_shown += dt;
        if (!f) return;
        snprintf(t, sizeof t, *fmt ? fmt : "%d sec. penalty", (s->penalty + 59) / 60);
        gs_color(0x800000C8u, fminf(h->penalty_shown / .2f, 1), c);
        text(f, align(2, 320, text_width(f, t)), y, 0, c, t);
        return;
    }
    h->penalty_shown = 0;
    if (code < 0 || code > 15) code = 0;
    if (code != h->signal_last && keys[code] < 0) h->wrong_blink = (blinker){ 1, 0, 0.2f, 0.3f, 20, 10, 5, 0 };
    h->signal_last = code;
    blink(&h->wrong_blink, dt);
    if (keys[code] < 0) {
        uint8_t c[4] = { 255, 255, 255, (uint8_t)(255 * h->wrong_blink.v) };
        image(h, IMG_WRONG_WAY, 2, 1, 320, y, 0, 0, c);
        return;
    }
    if (keys[code] > 0) {
        hud_font *f = font(h, "hn_20R");
        int i = keys[code] == 0x14 ? -1 : (keys[code] - 0x150) / 4;
        const char *t = i < 0 ? race_text(h, "TimeLimit") : race_text(h, key_of[i]);
        uint8_t c[4];
        if (!*t && i >= 0) t = text_of[i];
        if (!f || !*t) return;
        gs_color(0x80C8C8C8u, 1, c);
        if (code == 7 || code == 8 || code == 9 || code == 11 || code == 12) spaced = 1.2f;
        text(f, align(2, 320, text_width(f, t)), y, 0, c, t);
        spaced = 0;
    }
}

/* --------------------------- the onboard camera's panel (RaceOnboardPanel) */

/* its elements, at RaceDisplay +0xD58 - 0xC50 (the panel at (320, 452)) */
static const place base_at = { 320, 452, 336, 102, 2, 1 };       /* +0x50, RaceTexturePanel */
static const place speedo_at = { 152, 352, 130, 100, 0, 0 };     /* +0x6C, RaceOnboardSpeedmeter */
static const place tacho_round_at = { 488, 352, 130, 100, 1, 0 };/* +0xC8, RaceOnboardTachometer */
static const place odo_at = { 217, 407, 0, 0, 2, 1 };            /* +0x9C0, RaceOdometer */
static const place speed2_at = { 318, 411, 0, 0, 2, 1 };         /* +0x9E8, RaceDigitalSpeedmeter */
static const place gear2_at = { 320, 388, 24, 26, 2, 1 };        /* +0xA20 */
static const place lamp2_at = { 423, 390, 8, 8, 2, 2 };          /* +0xAD0 */
static const place sign2_at[3] = {                                /* +0xB3C, +0xB80, +0xBC4 */
    { 121, 452, 24, 24, 1, 2 }, { 154, 452, 24, 24, 1, 2 }, { 486, 452, 24, 24, 0, 2 },
};
static const place unit_at = { 320, 428, 28, 9, 2, 1 };          /* +0xC08 */
static const place g2_at = { 320, 452, 214, 24, 2, 2 };          /* +0xC24 */

/* the round meters (RaceRoundMeterBase): the scale's picture by the top
   (func_003EB860: the first entry at or above it, D_006A3C50 for mph,
   D_006A3C88 for km/h; func_003EC4E0 for the revs, D_006A3D38), the
   needle about (+0x38, +0x3C) from the centre at 60 units a radius, from
   -0.3097 to 0.9613 of it, turning from +0x48 to +0x4C degrees over the
   range, in +0x34 steps from where it was (func_003BE5F8) */
typedef struct { int img; float top; } meter_scale;
static const meter_scale speed_mph[] = { { IMG_SPEED_80, 80 }, { IMG_SPEED_160, 160 }, { IMG_SPEED_240, 240 } };
static const meter_scale speed_kmh[] = { { IMG_SPEED_80, 80 }, { IMG_SPEED_160, 160 }, { IMG_SPEED_240, 240 }, { IMG_SPEED_320, 320 } };
static const struct { float above; int img; float top; } rev_scales[] = {
    { 4000, IMG_REV_4000, 4000 }, { 5000, IMG_REV_5000, 5000 }, { 6000, IMG_REV_6000, 6000 },
    { 7000, IMG_REV_7000, 7000 }, { 8000, IMG_REV_8000, 8000 }, { 9000, IMG_REV_9000, 9000 },
    { 10000, IMG_REV_10000, 10000 }, { 12000, IMG_REV_12000, 12000 }, { 1e30f, IMG_REV_18000, 18000 },
};

/* func_003EC608, drawn by func_003EC438: the tachometer's red zone, a band
   from the needle's angle at the red line to the scale's end, from 0.6833
   of the needle's 60 to 0.311 further out, in segments of ten degrees (at
   least two), dark red (0x80000099) */
static void draw_red_zone(float cx, float cy, float a_from, float a_to)
{
    const float r_in = 0.6833333f * 60, r_out = (0.6833333f + 0.3109244f) * 60;
    uint8_t c[4];
    gs_color(0x80000099u, 1, c);
    c[3] = (uint8_t)(c[3] * light);
    if (a_to <= a_from) return;
    /* the band as thin turned strips (the platform draws rectangles) */
    for (float a = a_from; a < a_to; a += 0.5f) {
        float step = fminf(0.5f, a_to - a), mid = a + step * 0.5f, w = (r_in + r_out) * 0.5f * step * 3.14159265f / 180;
        plat_draw_rotated(NULL, cx + r_in, cy - w * 0.5f - 0.25f, r_out - r_in, w + 0.5f, cx, cy, mid, c);
    }
}

static void draw_round_meter(race_hud *h, int which, const place *p, int img, float value, float top,
                             float dx, float a0, float a1, int steps, float red_from)
{
    static const uint8_t bright[4] = { 255, 255, 255, 255 };
    float x0 = align(p->ha, (float)p->x, (float)p->w), y0 = align(p->va, (float)p->y, (float)p->h);
    float cx = x0 + p->w * 0.5f + dx, cy = y0 + p->h * 0.5f + 15, a, from;
    uint8_t c[4];
    /* the red zone first (the meter's +0x70), the scale over it */
    if (red_from > 0 && top > 0) draw_red_zone(cx, cy, red_from / top * (a1 - a0) + a0, a1);
    image(h, img, 0, 0, x0, y0, (float)p->w, (float)p->h, bright);
    a = (top > 0 ? value / top : 0) * (a1 - a0) + a0;
    a = fminf(fmaxf(a, a0), a1 + 2);
    from = h->needle_last[which] < 0 ? a : h->needle_last[which];
    h->needle_last[which] = a;
    gs_color(0x800000C8u, 1.0f / 6, c);
    for (int i = 0; i < steps; i++) {
        float t = (float)(i + 1) / (float)steps, ang = from + (a - from) * t;
        /* the needle's strip from radius -0.3097 to 0.9613 of 60, its
           width 0.0226 to 0.0129 of that (D_006A28B0) */
        const float r0 = -0.3097f * 60, r1 = 0.9613f * 60, hw = 0.0175f * 1.271f * 60;
        uint8_t q[4] = { c[0], c[1], c[2], (uint8_t)(c[3] * light) };
        plat_draw_rotated(NULL, cx + r0, cy - hw, r1 - r0, hw * 2, cx, cy, ang, q);
    }
}

/* func_003A4158: the odometer (car+0x6DC by 160 in mph, 100 else, then
   tenths up to 999999) in GT4oddmeter#1 as %06d */
static void draw_odometer(race_hud *h, unsigned odo, int unit)
{
    hud_font *f = font(h, "GT4oddmeter#1");
    char s[16];
    uint8_t c[4];
    unsigned v = odo / (unit == 1 ? 160u : 100u);
    if (v > 999999999u) v = 999999999u;
    v /= 10;
    if (v > 999999u) v = 999999u;
    if (!f) return;
    snprintf(s, sizeof s, "%06u", v);
    gs_color(0x80E3B896u, 1, c);
    text(f, align(odo_at.ha, (float)odo_at.x, text_width(f, s)), (float)odo_at.y, 0, c, s);
}

/* func_003A8968: the pedals' arcs between the meters, pedal_bar filling
   from its bottom (58 units from 23 below the speedometer's top): the
   brake in red (0x800000F0) at 121 right of the speedometer, the throttle
   in blue (0x80E3B896) at 195, the picture mirrored; then, with a tank,
   the fuel's arc left of the speedometer (fuel_a0, and fuel_a1 over it in
   tenths, red below three) */
static void draw_onboard_bars(race_hud *h, const race_hud_state *s)
{
    const hud_image *bar = &h->img[IMG_PEDAL_BAR];
    float sx = (float)speedo_at.x, sy = (float)speedo_at.y, bottom = sy + 23 - 1.5f + 58;
    uint8_t c[4];
    if (bar->tex) {
        float b = fminf(fmaxf(s->brake, 0), 1), t = fminf(fmaxf(s->throttle, 0), 1);
        gs_color(0x800000F0u, 1, c);
        if (b > 0) quad(bar->tex, sx + 121, bottom - b * 58, 20, b * 58, 0, 1 - b, 1, 1, c);
        gs_color(0x80E3B896u, 1, c);
        if (t > 0) quad(bar->tex, sx + 195, bottom - t * 58, 20, t * 58, 1, 1 - t, 0, 1, c);
    }
    if (s->fuel_cap > 0) {
        static const uint8_t white[4] = { 255, 255, 255, 255 };
        const hud_image *fill = &h->img[IMG_FUEL_A1];
        float x0 = sx - 22, y0 = sy + 19;
        int n = (int)(fminf(fmaxf(s->fuel / s->fuel_cap, 0), 1) * 10 + 0.5f);
        image(h, IMG_FUEL_A0, 0, 0, x0, y0, 38, 64, white);
        if (n > 0 && fill->tex) {
            float top = 60 - n * 52 * 0.1f - (n >= 6 ? 4 : 0);
            gs_color(n < 3 ? 0x800000C8u : 0x80E3B896u, 1, c);
            quad(fill->tex, x0, y0 + top, 38, 64 - top, 0, top / 64, 1, 1, c);
        }
    }
}

/* func_003A8708 */
static void draw_onboard_panel(race_hud *h, const race_hud_state *s)
{
    static const uint8_t white[4] = { 255, 255, 255, 255 };
    static const int state_of[4] = { 0, 1, 3, 0 };
    static const int lock_of[2] = { 0, 2 };
    const meter_scale *ms = s->unit == 1 ? speed_mph : speed_kmh;
    int nms = s->unit == 1 ? 3 : 4, si = nms - 1, ri = 8;
    float sp = s->speed * 3.6f * (s->unit == 1 ? 0.6214f : 1);
    for (int i = 0; i < nms - 1; i++) if (s->speed_scale <= ms[i].top) { si = i; break; }
    for (int i = 0; i < 9; i++) if (s->tacho_scale <= rev_scales[i].above) { ri = i; break; }
    image(h, IMG_PANEL_BASE, base_at.ha, base_at.va, (float)base_at.x, (float)base_at.y, (float)base_at.w,
          (float)base_at.h, white);
    if (s->driving) draw_odometer(h, s->odometer, s->unit);
    {
        uint8_t c[4];
        gs_color2(0x800000FFu, (s->shift_lamp ? 1.0f : 0) * 1.5f, 1, c);
        image(h, IMG_TIMING_LAMP, lamp2_at.ha, lamp2_at.va, (float)lamp2_at.x, (float)lamp2_at.y, 0, 0, c);
    }
    draw_round_meter(h, 0, &speedo_at, ms[si].img, sp, ms[si].top, 1, 146, 366, 5, 0);
    draw_round_meter(h, 1, &tacho_round_at, rev_scales[ri].img, s->rpm, rev_scales[ri].top, -1, 174, 394, 8, s->rev_limit);
    {   /* the gear, as the chase camera's panel draws it */
        hud_font *f = font(h, "GT4meter#1");
        const place *p = &gear2_at;
        float x0 = align(p->ha, (float)p->x, (float)p->w), y0 = align(p->va, (float)p->y, (float)p->h);
        char g[2] = { (char)(s->gear == 0 ? 'R' : '0' + s->gear), 0 };
        uint8_t c[4];
        image(h, IMG_GEAR_BASE, 0, 0, x0, y0, (float)p->w, (float)p->h, white);
        gs_color2(0x80E3B896u, 0, 1, c);
        if (f) text(f, x0 + p->w / 2, y0 + p->h - 12, 0xf, c, g);
    }
    draw_speed(h, &speed2_at, (int)(sp + 0.5f));
    if (s->driving) {
        static const int img[3] = { IMG_SIGN_ASM, IMG_SIGN_BRAKE, IMG_SIGN_OIL };
        static const uint32_t col[3] = { 0x80E3B896u, 0x800000C8u, 0x800000C8u };
        const int st[3] = { state_of[s->aid & 3], lock_of[s->brake_lock != 0], 0 };
        for (int i = 0; i < 3; i++) {
            uint8_t c[4];
            gs_color2(col[i], st[i] * 0.5f, 1, c);
            image(h, img[i], sign2_at[i].ha, sign2_at[i].va, (float)sign2_at[i].x, (float)sign2_at[i].y,
                  (float)sign2_at[i].w, (float)sign2_at[i].h, c);
        }
    }
    image(h, s->unit == 1 ? IMG_MPH : IMG_KMPH, unit_at.ha, unit_at.va, (float)unit_at.x, (float)unit_at.y,
          (float)unit_at.w, (float)unit_at.h, white);
    {
        const place *p = &g2_at;
        float x0 = align(p->ha, (float)p->x, (float)p->w), y0 = align(p->va, (float)p->y, (float)p->h), g;
        uint8_t c[4];
        g = fmaxf(fminf(s->side_g, 2), -2) / 2;
        image(h, IMG_G_BASE, 0, 0, x0, y0, (float)p->w, (float)p->h, white);
        gs_color(0x80E3B896u, 1, c);
        image(h, IMG_G_POINT, 2, 2, x0 + p->w * 0.5f + p->w * 0.5f * 0.944f * g, y0 + 8, 0, 0, c);
    }
    draw_onboard_bars(h, s);
}

/* func_003A78B8 */
static void draw_simple_panel(race_hud *h, const race_hud_state *s)
{
    static const uint8_t white[4] = { 255, 255, 255, 255 };
    static const int state_of[4] = { 0, 1, 3, 0 };    /* D_006A0690 */
    static const int lock_of[2] = { 0, 2 };           /* D_006A06A0 */
    float sp = s->speed * 3.6f * (s->unit == 1 ? 0.6214f : 1);
    if (s->bspec) {
        /* the panel's +0x18 (no gauges), +0x19/+0x1B (the fuel +0x80 and
           the tyres +0xD0, 003A5FD8): the B-Spec savestates' panel. A: the
           places taken from the user's PCSX2 print against ours */
        const float up = 12;
        image(h, s->unit == 1 ? IMG_REMOTE_MPH : IMG_REMOTE_KMPH, remote_at.ha, remote_at.va, (float)remote_at.x,
              (float)remote_at.y - up, (float)remote_at.w, (float)remote_at.h, white);
        { place sp_at = speed_at; sp_at.y -= (int)up; draw_speed(h, &sp_at, (int)(sp + 0.5f)); }
        draw_tires(h, s);
        if (s->fuel_cap > 0) draw_fuel_at(h, &bspec_fuel_at, s->fuel, s->fuel_cap);
        return;
    }
    image(h, s->unit == 1 ? IMG_REMOTE_MPH : IMG_REMOTE_KMPH, remote_at.ha, remote_at.va, (float)remote_at.x,
          (float)remote_at.y, (float)remote_at.w, (float)remote_at.h, white);
    draw_speed(h, &speed_at, (int)(sp + 0.5f));
    if (s->driving && s->fuel_cap > 0) draw_fuel(h, s->fuel, s->fuel_cap);
    draw_tacho(h, s->rpm, s->rev_limit);
    draw_gear(h, s->gear, 0);
    draw_lamp(h, s->shift_lamp);
    draw_pedals(h, s->throttle, s->brake);
    draw_g(h, s->side_g);
    if (s->driving) {
        draw_sign(h, 0, state_of[s->aid & 3], 1);
        draw_sign(h, 1, lock_of[s->brake_lock != 0], 1);
        draw_sign(h, 2, 0, 1);
    }
}

/* func_004940E8: "%B<c>" draws c in the Gaiji font (the pad's buttons),
   grey 0x646464 but for c..f, whose colours D_006AE738 holds; the rest
   in the text's own font and colour. x, y the text's start and baseline;
   with draw 0 only the width. */
static float rich_text(race_hud *h, const hud_font *f, float x, float y, const uint8_t color[4], const char *s, int draw)
{
    static const uint8_t grey[4] = { 0x64, 0x64, 0x64, 255 };
    hud_font *gaiji = font(h, "Gaiji");
    char run[256];
    float pen = x;
    while (*s) {
        size_t n = 0;
        while (s[n] && !(s[n] == '%' && (s[n + 1] == 'B' || s[n + 1] == 'F') && s[n + 2])) n++;
        if (n) {
            snprintf(run, sizeof run, "%.*s", (int)(n < sizeof run - 1 ? n : sizeof run - 1), s);
            if (draw) text(f, pen, y, 0, color, run);
            pen += text_width(f, run);
            s += n;
        }
        if (*s == '%' && s[1] == 'F') {   /* a font to the end: its name up to a space */
            char name[32];
            size_t k = strcspn(s + 2, " ");
            snprintf(name, sizeof name, "%.*s", (int)(k < sizeof name - 1 ? k : sizeof name - 1), s + 2);
            s += 2 + k + (s[2 + k] == ' ');
            f = font(h, name);
            if (!f) return pen - x;
            continue;
        }
        if (*s == '%') {
            char c[2] = { s[2], 0 };
            /* D_006AE738: Triangle, Circle, Cross, Square in their colours */
            static const uint8_t own[4][3] = { { 0x00, 0x99, 0x9b }, { 0xf7, 0x1c, 0x54 }, { 0x51, 0x72, 0xb3 }, { 0xf7, 0x68, 0xa1 } };
            uint8_t k[4];
            memcpy(k, grey, 4);
            if (s[2] >= 'c' && s[2] <= 'f') memcpy(k, own[s[2] - 'c'], 3);
            k[3] = color[3];
            if (gaiji) {
                if (draw) text(gaiji, pen, y, 0, k, c);
                pen += text_width(gaiji, c);
            }
            s += 3;
        }
    }
    return pen - x;
}

/* The replay's plate and logo (RaceReplayModeDisplay; the images
   replay_plate and JP/gt_logo_mini of display.gpb). Placed and coloured
   from the PCSX2 savestates rv_02..04 (A: the class's own layout not
   read): the plate, white tinted 228,132,0, at (20, 18.5) of the 640x448
   frame with its rtext black and centred; the logo at (415, 363), white
   at half, while the race's display is off. Drawn in the display's
   640x480, the frame's lines stretched to it as PCSX2 shows them. */
void race_hud_draw_replay(race_hud *h, const char *label, float alpha, int logo)
{
    static const uint8_t black[4] = { 0, 0, 0, 255 };
    const float k = 480.f / 448;   /* the GS frame's 448 lines in the display's 480 */
    if (!h) return;
    plat_video_set_view(640, 480);
    light = 1;
    if (logo && h->logo_out < 1) {   /* func_003A3A00: 0.5 x its light x (1 - +0x24) */
        const uint8_t half[4] = { 255, 255, 255, (uint8_t)(128 * (1 - h->logo_out)) };
        image(h, IMG_GT_LOGO, 0, 0, 415, 363 * k, 188, 50 * k, half);
    }
    if (label && alpha > 0) {
        const uint8_t plate[4] = { 228, 132, 0, (uint8_t)(255 * alpha) };
        uint8_t ink[4] = { 0, 0, 0, (uint8_t)(255 * alpha) };
        hud_font *f = font(h, "hn_14#1*100,107");   /* ink only, as the savestates show it */
        const char *msg = race_text(h, label);
        float w;
        (void)black;
        image(h, IMG_REPLAY_PLATE, 0, 0, 20, 18.5f * k, 132, 20 * k, plate);
        if (f && *msg) {
            w = rich_text(h, f, 0, 0, ink, msg, 0);
            rich_text(h, f, 86 - w * .5f, 28.5f * k - f->face.height * .5f * f->sy + f->face.baseline * f->sy, ink, msg, 1);
        }
    }
}

/* The replay's opening (func_0039BEC8 / func_0039C028, then vtable+0x168
   func_0039C838): the course's logo (+0x11B0) and two messages, each
   0.1 s late, 6 s long, in and out in a second. +0x267C the car's name
   (entry +0x348C) in hb_24 at (320, 220), +0x27CC under it at (320, 260)
   in hm_16 RaceInfoFormat ("%s  %s  %s"): the power ("%d%s" with the
   option's unit, func_004729A0 truncated; "--%s" when none), the tyres
   ("%s/%s" of TireShort_%02d, "-" past 12 or none, func_003B03B8) and the
   mode's name (its rtext, "%s%d" with a number above 0), colour
   0x80C8C8C8, letters spaced as the start's (+0x24 set) */
void race_hud_draw_opening(race_hud *h, const race_hud_opening *o, float elapsed)
{
    static const char *const units[7] = { "PS", "HP", "BHP", "CH", "kW", "CV", "PF" };
    float a = intro_alpha(elapsed, .1f, 6, 1, 1);
    char power[32], tyres[64], mode[64], tyre[2][32], line[192];
    const char *fmt;
    uint8_t c[4];
    hud_font *f;
    int unit;
    if (!h || !o || a <= 0) return;
    plat_video_set_view(640, 480);
    light = 1;
    gs_color(0x80ffffffu, a, c);
    if (h->course_logo)
        quad(h->course_logo, 604.f - h->logo_w, 441.f - h->logo_h,
             (float)h->logo_w, (float)h->logo_h, 0, 0, 1, 1, c);
    unit = o->unit >= 0 && o->unit < 7 ? o->unit : 1;
    {
        float p = o->power;
        if (unit == 1 || unit == 2 || unit == 4) {
            /* 004729A0: HP's original factor is one ULP below .98619f. */
            uint32_t bits=unit==4?0x3f3c49bau:0x3f7c76f2u;float factor;
            memcpy(&factor,&bits,4);p*=factor;
        }
        if (o->power > 0 && p > 0) snprintf(power, sizeof power, "%d%s", (int)p, units[unit]);
        else snprintf(power, sizeof power, "--%s", units[unit]);
    }
    for (int k = 0; k < 2; k++) {
        int t = k ? o->rear : o->front;
        char key[32];
        if (t < 0 || t >= 13) snprintf(tyre[k], sizeof tyre[k], "-");
        else {
            snprintf(key, sizeof key, "TireShort_%02d", t);
            snprintf(tyre[k], sizeof tyre[k], "%s", race_text(h, key));
        }
    }
    snprintf(tyres, sizeof tyres, "%s/%s", tyre[0], tyre[1]);
    snprintf(mode, sizeof mode, "-");
    if (o->mode && *o->mode && *race_text(h, o->mode)) {
        if (o->mode_n > 0) snprintf(mode, sizeof mode, "%s%d", race_text(h, o->mode), o->mode_n);
        else snprintf(mode, sizeof mode, "%s", race_text(h, o->mode));
    }
    fmt = race_text(h, "RaceInfoFormat");
    line[0] = 0;
    if (*fmt) snprintf(line, sizeof line, fmt, power, tyres, mode);
    spaced = 1.2f;
    gs_color(0x80c8c8c8u, a, c);
    if (o->car && (f = font(h, "hb_24#1"))) text(f, 320, 220, 3, c, o->car);
    if ((f = font(h, "hm_16#1"))) text(f, 320, 260, 3, c, line);
    spaced = 0;
}

/* The replay's pause (func_003C3B08): the frame's bottom (438/480 of
   the frame's height down) black (0x80000000: the GS's 0x80 is opaque),
   and over it the
   rtext SimplePauseMessage ("Press %Bb to resume") in hn_14 at 107%
   width, colour 0x80A7A5A5, centred in the box (0, 408, 640, 40) of
   the 640x448 frame */
void race_hud_music(race_hud *h, const char *title, const char *artist)
{
    if (!h || !title) return;
    /* a race's display starts with no track shown (+0x50) */
    if (!title[0]) h->music_shown[0] = 0;
    snprintf(h->music_want, sizeof h->music_want, "%s", title);
    snprintf(h->music_artist_want, sizeof h->music_artist_want, "%s", artist ? artist : "");
}

/* func_003A22A0: a new track's title, once the display is lit and no
   message is up, after half a second for ten, in and out in a second
   (+0x20, +0x24); func_003AE350 draws it: the Gaiji note (n) at
   (x - 16, y), the title in hm_16 at 90%, the artist in hc_20 at 70%
   18 lower, letters 2 apart, colour 0x80C8C8C8, from (41, 432) of the
   640x480 (rp_03/04) */
static void draw_music(race_hud *h, float dt, int allowed)
{
    hud_font *note, *ft, *fa;
    uint8_t ink[4] = { 200, 200, 200, 255 };
    /* func_003A22A0 asks the logo out on each call while the track is not
       the shown one (+0x50), before it may show it */
    if (h->music_want[0] && strcmp(h->music_want, h->music_shown)) h->logo_out_hold = 11;
    if (h->logo_out_hold > 0) {
        h->logo_out_hold -= dt;
        h->logo_out = fminf(h->logo_out + dt / .5f, 1);
    } else
        h->logo_out = fmaxf(h->logo_out - dt / 3, 0);
    if (allowed && h->music_want[0] && strcmp(h->music_want, h->music_shown)) {
        memcpy(h->music_shown, h->music_want, sizeof h->music_shown);
        memcpy(h->music_title, h->music_want, sizeof h->music_title);
        memcpy(h->music_artist, h->music_artist_want, sizeof h->music_artist);
        h->music_state = 0;
        h->music_t = .5f;
    }
    /* func_003A9780 */
    if (h->music_state == 0) {
        if ((h->music_t -= dt) < 0) { h->music_state = 1; h->music_t += 10; }
        h->music_v -= dt;
    } else if (h->music_state == 1) {
        if ((h->music_t -= dt) < 0) h->music_state = 2;
        h->music_v += dt;
    } else h->music_v -= dt;
    h->music_v = fmaxf(0, fminf(1, h->music_v));
    if (h->music_v <= 0 || !h->music_title[0]) return;
    ink[3] = (uint8_t)(255 * h->music_v);
    light = 1;
    /* func_0044DFA0(2): the context's +0x70 with +0x2C on. A: taken as
       letters 0.7 apart, the title's width in rp_04 (224 of the 640) */
    spaced = .7f;
    if ((note = font(h, "Gaiji#1"))) text(note, 41 - 16, 432, 0, ink, "n");
    if ((ft = font(h, "hm_16#1*90,90"))) text(ft, 41, 432, 0, ink, h->music_title);
    if (h->music_artist[0] && (fa = font(h, "hc_20#1*70,70"))) text(fa, 41, 432 + 18, 0, ink, h->music_artist);
    spaced = 0;
}

/* func_003A1488: START for a second, out in 0.15 (RaceDisplay +0x252C) */
void race_hud_start(race_hud *h)
{
    if (h) message(h, race_text(h, "RaceStart"), 1, 0.15f);
}

void race_hud_draw_music(race_hud *h, float dt)
{
    if (!h) return;
    plat_video_set_view(640, 480);
    draw_music(h, dt, 1);
}

void race_hud_draw_simple_pause(race_hud *h)
{
    static const uint8_t shade[4] = { 0, 0, 0, 255 }, ink[4] = { 0xa5, 0xa5, 0xa7, 255 };
    hud_font *f;
    const char *msg;
    float w;
    if (!h) return;
    plat_video_set_view(640, 480);   /* the GS frame's 448 lines in the display's 480 */
    light = 1;
    rect(0, 438, 640, 42, shade);
    f = font(h, "hn_14#1*107,107");
    msg = race_text(h, "SimplePauseMessage");
    if (!f || !*msg) return;
    w = rich_text(h, f, 0, 0, ink, msg, 0);
    /* the box's middle: the line centred on y 428 */
    rich_text(h, f, 320 - w * .5f, 428.f * 480 / 448 - f->face.height * .5f * f->sy + f->face.baseline * f->sy, ink, msg, 1);
}

/* A help line (a RaceDisplay rtext key with its pad's buttons, "Tool Tip1"
   for B-Spec's live view, "RaceMonitorToolTip1" for its monitor) in the
   bottom band, as SimplePauseMessage draws (A: the class at the monitor's
   +0x394, 003A4CF0, not read; its band and font taken from the replay's,
   which the PCSX2 prints of B-Spec match) */
void race_hud_draw_tooltip(race_hud *h, const char *key, float alpha)
{
    uint8_t shade[4] = { 0, 0, 0, 255 }, ink[4] = { 0xa5, 0xa5, 0xa7, 255 };
    hud_font *f;
    const char *msg;
    float w;
    if (!h || !key || !*key || alpha <= 0) return;
    shade[3] = ink[3] = (uint8_t)(255 * fminf(alpha, 1));
    plat_video_set_view(640, 480);
    light = 1;
    rect(0, 438, 640, 42, shade);
    f = font(h, "hn_14#1*107,107");
    msg = race_text(h, key);
    if (!f || !*msg) return;
    w = rich_text(h, f, 0, 0, ink, msg, 0);
    rich_text(h, f, 320 - w * .5f, 428.f * 480 / 448 - f->face.height * .5f * f->sy + f->face.baseline * f->sy, ink, msg, 1);
}

void race_hud_draw(race_hud *h, const race_hud_state *s, float dt)
{
    static const uint8_t white[4] = { 255, 255, 255, 255 };
    static const int labels[4] = { IMG_POSITION, IMG_LAP, IMG_BEST, IMG_LAST };
    float display_light;
    if (!h || !s) return;
    hud_y_offset = 0;
    plat_video_set_view(640, s->split ? GT4_SPLIT_HEIGHT : 480);
    /* the race's events: the start (race_hud_start) and the finish (func_003A1590: FINISH for 3.7
       seconds, out in 0.3; the display fading out in a quarter second) */
    if (s->finished && !h->was_finished && !s->license) {
        message(h, race_text(h, "RaceFinish"), 3.7f, 0.3f);
        h->fade_rate = 4;
    }
    h->was_finished = s->finished;
    /* RaceDisplayLicense 003BCFF0/003BD438 fades the display, without
       the Arcade FINISH message. 003BD3B0 also hides its extra elements. */
    if(s->license && s->finished)h->fade_rate=4;
    h->fade = fmaxf(h->fade - h->fade_rate * dt, 0);
    light = h->fade;
    /* func_0039C188: RaceDisplay+0xBC starts at zero, waits the race's
       initial seconds (+0xB8) minus 4.5, then rises in 0.5 s through
       func_003A98E8/func_003A9780. Expressed in the countdown's remaining
       frames, so loading time and the test speed do not shift it. */
    if (!s->rolling && s->start_frames > 0)
        light *= fminf(fmaxf((4.5f - s->start_frames / 60.f) * 2, 0), 1);
    else if (s->rolling) /* func_0039C318: delay clamp(initial seconds,0,2), rise 0.5 */
        light *= fminf(fmaxf((s->intro_elapsed - fminf(fmaxf((float)s->start_seconds, 0), 2)) * 2, 0), 1);
    h->shown = light;
    for (int i = 0; i < 4; i++) {
        if(s->split && i<2)continue;
        if(s->license && (i==3 || (i==0 && !s->license_rank) || (i==1 && s->license_pylon_limit>0)))continue;
        if(s->bspec && i==3)continue;   /* D_006A0BA0: the total time alone (+0x16A0 JP/total_time) */
        const place *p=s->split ? &gt4_split_layout[i==2?SPLIT_BEST_LABEL:SPLIT_LAST_LABEL] : &label_at[i];
        image(h, s->bspec && i==2 ? IMG_TOTAL : labels[i], p->ha, p->va, (float)p->x, (float)p->y, 0, 0, white);
    }
    if(!s->license || s->license_rank)draw_rank(h, s->rank, s->split ? &gt4_split_layout[SPLIT_RANK] : &rank_at);
    /* 003BCE70 +0x104..0x138 sets common flag 0x10000 for pylons. */
    if(!s->split && (!s->license || s->license_pylon_limit<=0))draw_lap(h, s->lap, s->laps);
    if(s->bspec){   /* +0x1438 at (550, 58): the race's time (003B00A8) */
        char t[16];format_total(s->total_time,t);draw_time_text(h,&best_at,t,0);
    }else{
        draw_time(h, s->split ? &gt4_split_layout[SPLIT_BEST] : &best_at, s->best_lap, 0);
        if(!s->license)draw_time(h, s->split ? &gt4_split_layout[SPLIT_LAST] : &last_at, s->last_lap, 0);
    }
    draw_time(h, s->split ? &gt4_split_layout[SPLIT_TIMER] : &timer_at, s->lap_time, 1);
    if(s->license && !s->finished){
        gt4_license_layout layout;gt4_license_layout_make(&layout);
        for(int i=0;i<3;i++){
            gt4_license_element *e=&layout.times[i];
            place p={e->x,e->y,e->w,e->h,e->ha,e->va};
            draw_time(h,&p,s->license_times[i],0);
            /* 003BD150 +0x94: the mark ends 15 units left of the
               time's start, at baseline +8 (003A43A8 +0x138). */
            hud_font *f=font(h,"hn_20R");char t[16];format_time(s->license_times[i],t);
            if(f)image(h,IMG_FIRST+i,1,1,align(e->ha,e->x,text_width(f,t))-15,e->y+8,0,0,white);
        }
        if(s->license_pylon_limit){
            gt4_license_element *e=&layout.pylons;place p={e->x,e->y,e->w,e->h,e->ha,e->va};
            char t[32];uint8_t c[4];gs_color(0x80c8c8c8u,1,c);
            /* RaceQuantityCounter 003AAFA0 +0x24..0x5c: clamp both
               values to +0x70 (9999999, constructor 003AAEE0 +0x18),
               format 006A1488; 003AB010 uses
               FC_26*80,80, fitting a wide counter into its authored width. */
            hud_font *f=font(h,"FC_26*80,80");
            snprintf(t,sizeof t,"%d/%d",s->license_pylons<9999999?s->license_pylons:9999999,s->license_pylon_limit<9999999?s->license_pylon_limit:9999999);
            if(f){hud_font scaled=*f;float w=text_width(f,t);
                if(w>p.w && p.w>0){scaled.sx*=p.w/w;w=p.w;}
                float y=align(p.va,p.y,scaled.face.height*scaled.sy)+scaled.face.height*scaled.sy;
                text(&scaled,align(p.ha,p.x,w),y,0,c,t);
            }
            /* 003BD150 +0x120, texture named at 006A2830. */
            e=&layout.pylon_label;image(h,IMG_PYLON,e->ha,e->va,e->x,e->y,e->w,e->h,white);
        }
    }
    blink(&h->map_blink, dt);
    draw_map(h, s, h->map_blink.v);
    hud_y_offset = s->split ? -GT4_SPLIT_HEIGHT : 0;
    if (s->panel == 0) draw_simple_panel(h, s);
    else draw_onboard_panel(h, s);
    draw_signal(h, s, dt);
    hud_y_offset = s->split ? gt4_split_layout[SPLIT_LINES].y - 139.f : 0;
    draw_lines(h, dt);
    if(s->refueling && s->fuel_cap>0)draw_refueling(h,s->fuel,s->fuel_cap);
    display_light = light;
    light = 1;
    hud_y_offset = s->split ? gt4_split_layout[SPLIT_MESSAGE].y - 224.f : 0;
    draw_message(h, dt);
    draw_countdown(h, s, display_light);
    hud_y_offset = 0;
    if(!s->license)draw_intro(h, s);
    /* func_003A22A0 leaves at once while 003A1628 (+0x20 below 2) holds:
       0 in every race capture (ar_*, s0*), 2 in the replays' (rp_*, rv_*).
       The race shows no title and keeps its logo */
    if (s->replay) draw_music(h, dt, display_light >= 1 && !h->msg);
    plat_video_set_view(640, 448);
}

/* func_0042DC90 / 0042E160: a paint chip. Its colours are the leading
   ones with bit 24; one fills the box, more split it from the top
   (D_006A4F38: 0, 1/2, 3/4, 7/8, 1) or, with bit 27 of the first, from
   the bottom (D_006A4F50: 1, 1/2, 1/4, 1/8, 0); bit 26 shades each band
   into the next colour (0042E0A0) instead of filling it (0042E008). The
   frame (+0/+4, a texture) is none here. */
void race_hud_draw_chip(const uint32_t chip[4], float x, float y, float w, float hh, float alpha)
{
    static const float down[5] = { 0, 0.5f, 0.75f, 0.875f, 1 }, up[5] = { 1, 0.5f, 0.25f, 0.125f, 0 };
    uint8_t c[4][4], q[4][4];
    const float *t;
    int n = 0, shade;
    if (!chip || alpha <= 0) return;
    while (n < 4 && (chip[n] & 0x01000000u)) n++;
    if (!n) return;
    plat_video_set_view(640, 480);
    for (int i = 0; i < n; i++) {
        for (int k = 0; k < 3; k++) c[i][k] = (uint8_t)(chip[i] >> (8 * k));
        c[i][3] = (uint8_t)(255 * fminf(alpha, 1));
    }
    t = (chip[0] >> 27 & 1) ? up : down;
    shade = chip[0] >> 26 & 1;
    if (n == 1) {
        plat_draw_quad(NULL, x, y, w, hh, 0, 0, 1, 1, c[0]);
        return;
    }
    for (int i = 0; i < (shade ? n - 1 : n); i++) {
        int last = shade ? i + 1 >= n - 1 : i + 1 >= n;
        float a = y + t[i] * hh, b = y + (last ? t[4] : t[i + 1]) * hh;
        const uint8_t *top = c[i], *bottom = shade ? c[i + 1] : c[i];
        if (b < a) { float s = a; a = b; b = s; const uint8_t *u = top; top = bottom; bottom = u; }
        memcpy(q[0], top, 4); memcpy(q[1], top, 4);
        memcpy(q[2], bottom, 4); memcpy(q[3], bottom, 4);
        plat_draw_quad4(NULL, x, a, w, b - a, 0, 0, 1, 1, q);
    }
}

/* func_003D7930 "sector": a row's car through the lap's sections, one
   band each in its colour (003DC7B0, 0xAABBGGRR), the section it is in
   marked along the bottom fifth (0x80C8C8C8) */
void race_hud_draw_sectors(const uint32_t *colors, int n, int current, float x, float y, float w, float hh, float alpha)
{
    uint8_t c[4];
    if (!colors || n <= 0 || alpha <= 0) return;
    plat_video_set_view(640, 480);
    for (int i = 0; i < n; i++) {
        float x0 = x + w * (float)i / (float)n, x1 = x0 + w / (float)n;
        gs_color(colors[i], alpha, c);
        plat_draw_quad(NULL, x0, y, x1 - x0, hh, 0, 0, 1, 1, c);
        if (i == current) {
            float top = y + hh * 0.8f;
            gs_color(0x80c8c8c8u, alpha, c);
            plat_draw_quad(NULL, x0, top, x1 - x0, y + hh - top, 0, 0, 1, 1, c);
        }
    }
}
