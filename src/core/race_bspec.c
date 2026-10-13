/*
 * B-Spec's race display (race_bspec.h): the monitor class of 003D8DB8
 * (its frame), 003DA150 (its drawing and change of mode) and the parts it
 * redraws, 003DAF60 (the standings and the header), 003DA7C8 (a row),
 * 003DB520 (the pace panel), 003DBBD0 (the speed and the map); the pad by
 * 003D9520 (every mode), 003D9720 (off), 003D95F0 (monitor), 003D97A8 /
 * 003D9918 (live) and 003D9838 (the orders). The monitor's own fields are
 * kept under their offsets.
 */
#include "race_bspec.h"
#include "race_pit_crew.h"
#include "rtext.h"
#include "strobe_player.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NO_TIME 0x157529ffu

struct race_bspec {
    strb_player *movie;
    uint8_t *rt_data;
    gt4_rtext rt;
    int rt_ok;
    race_bspec_host host;
    int mode;              /* +0x28: 0 off, 1 monitor, 2 live */
    int target;            /* +0x2C: the mode asked */
    int back;              /* +0x44: the monitor's R1 returns to the live view */
    float timer;           /* +0x80: 1 -> -1 over a mode change, -1 at rest */
    int player;            /* the player's car */
    int selected;          /* +0x64: the car watched */
    int pace, overtake, pit, text;   /* +0x4C, +0x50, +0x54, +0x58 */
    int speed;             /* +0x74: 1..3 */
    int map;               /* +0x70: the map shown */
    int held_l1;           /* +0x5C */
    unsigned blink;        /* +0x90 */
    int view;              /* +0x94: the live view's camera, 0..2 */
    int lap[RACE_BSPEC_CARS];         /* +0xE8: the lap a row last showed */
    uint32_t best[RACE_BSPEC_CARS];   /* +0x100: the best lap then */
    uint8_t best_mark[RACE_BSPEC_CARS];   /* +0xE2 */
    int state_ok;
    race_bspec_state s;
};

static const char *text_of(const race_bspec *b, const char *key)
{
    const char *t = b->rt_ok ? gt4_rtext_get(&b->rt, "RaceDisplay", key) : NULL;
    return t ? t : "";
}

static void sound(race_bspec *b, int id) { if (b->host.sound) b->host.sound(b->host.user, id); }

static void ext_path(void *user, const char *path, float x, float y, float w, float h, const uint8_t color[4])
{
    race_bspec *b = user;
    if (b->host.ext) b->host.ext(b->host.user, path, x, y, w, h, color);
}

race_bspec *race_bspec_open(gt4_vol *vol, const race_bspec_host *host, int player)
{
    race_bspec *b = calloc(1, sizeof *b);
    strb_host h;
    size_t n = 0;
    if (!b) return NULL;
    if (host) b->host = *host;
    b->rt_data = gt4_vol_read(vol, "/rtext/raceUS.rt2", &n);
    b->rt_ok = b->rt_data && gt4_rtext_open(&b->rt, b->rt_data, n);
    memset(&h, 0, sizeof h);
    h.ext_path = ext_path;
    h.user = b;
    h.palette = 1;
    h.field_size = 1;
    b->movie = strb_open(vol, "/menu/common/racemonitor.strb", b->rt_ok ? &b->rt : NULL, "RaceDisplay", &h);
    if (!b->movie) {
        free(b->rt_data);
        free(b);
        return NULL;
    }
    /* the savestate at the start (bspec slot 05): live, view 2, speed 1,
       the map on, the player's car watched */
    b->mode = b->target = 2;
    b->back = 1;
    b->timer = -1;
    b->player = b->selected = player;
    b->speed = 1;
    b->map = 1;
    b->view = 2;
    for (int i = 0; i < RACE_BSPEC_CARS; i++) b->best[i] = NO_TIME;
    strb_goto(b->movie, NULL, 1);
    return b;
}

void race_bspec_close(race_bspec *b)
{
    if (!b) return;
    strb_close(b->movie);
    free(b->rt_data);
    free(b);
}

/* ------------------------------------------------------------- texts */

/* func_0042E478: a lap's time, the dashes (0x7F) without one */
static void lap_time(uint32_t t, char *out, size_t n)
{
    if (t == NO_TIME) { snprintf(out, n, "\x7f\x7f'\x7f\x7f.\x7f\x7f\x7f"); return; }
    unsigned ms = t % 1000, s = t / 1000 % 60, m = t / 60000 % 60, h = t / 3600000;
    if (h) snprintf(out, n, "%u:%02u'%02u.%03u", h, m, s, ms);
    else snprintf(out, n, "%u'%02u.%03u", m, s, ms);
}

/* func_003B00A8 for the header's TOTAL (003DAF60 drops the last four
   characters): h:mm'ss, at most 9:59'59 */
static void total_time(uint32_t t, char *out, size_t n)
{
    if (t >= 0x157529feu) { snprintf(out, n, "\x7f:\x7f\x7f'\x7f\x7f"); return; }
    if (t > 0x22550ffu) t = 0x22550ffu;
    snprintf(out, n, "%u:%02u'%02u", t / 3600000, t / 60000 % 60, t / 1000 % 60);
}

static int find_row(const race_bspec_state *s, int index)
{
    for (int i = 0; i < s->cars; i++) if (s->car[i].index == index) return i;
    return -1;
}

/* func_003DA518: a sprite of the movie at frame 1 when on, else 0 */
static void lit(race_bspec *b, const char *path, int on) { strb_goto(b->movie, path, on ? 1 : 0); }

/* func_003DA7C8: one row of the standings */
static void row(race_bspec *b, int r, const race_bspec_car *c)
{
    char path[48], t[64];
    int k = c->index >= 0 && c->index < RACE_BSPEC_CARS ? c->index : 0;
#define AT(name) (snprintf(path, sizeof path, "BAR%d." name, r + 1), path)
    strb_goto(b->movie, AT("GRID"), c->grid);
    strb_goto(b->movie, AT("ICON"), c->state);
    /* a lap ended: its best newly bettered lights BESTMARK */
    if (b->lap[k] != c->lap) {
        b->lap[k] = c->lap;
        b->best_mark[k] = c->best_lap < b->best[k];
        b->best[k] = c->best_lap;
    }
    strb_text_fit(b->movie, AT("NAME"), c->name);   /* 003DA668 */
    lap_time(c->last_lap, t, sizeof t);
    strb_text(b->movie, AT("TIME"), t);
    strb_visible(b->movie, AT("BESTMARK"), b->best_mark[k]);
    /* the gap: laps behind ("%d %s", func_003B0320), else the time behind,
       nothing for a negative one (the leader's) */
    if (c->diff_laps) {
        int l = c->diff_laps < 0 ? -c->diff_laps : c->diff_laps;
        snprintf(t, sizeof t, "%d %s", l, text_of(b, l == 1 ? "WordOfLap" : "WordOfLaps"));
    } else if ((int32_t)c->diff < 0) t[0] = 0;
    else lap_time(c->diff, t, sizeof t);
    strb_text(b->movie, AT("DIFF"), t);
    lap_time(c->best_lap, t, sizeof t);
    strb_text(b->movie, AT("BEST"), t);
    snprintf(t, sizeof t, "%d", c->lap);
    strb_text(b->movie, AT("LAP"), t);
    snprintf(t, sizeof t, "%d", c->pits);
    strb_text(b->movie, AT("PIT"), t);
#undef AT
}

/* func_003DAF60: the rows in race order (the watched car's lit, its name
   and letter in the header), the others hidden; the watched car's lap
   ("RaceMonitorLapFormat1" with the race's laps, 2 without) and the race's
   time */
static void standings(race_bspec *b)
{
    const race_bspec_state *s = &b->s;
    char path[16], t[96];
    int sel = find_row(s, b->selected), r;
    for (r = 0; r < s->cars && r < RACE_BSPEC_CARS; r++) {
        snprintf(path, sizeof path, "BAR%d", r + 1);
        row(b, r, &s->car[r]);
        strb_visible(b->movie, path, 1);
        lit(b, path, s->car[r].index == b->selected);
    }
    for (; r < RACE_BSPEC_CARS; r++) {
        snprintf(path, sizeof path, "BAR%d", r + 1);
        strb_visible(b->movie, path, 0);
    }
    if (sel >= 0) {
        strb_text(b->movie, "CURRENTCAR", s->car[sel].name);
        strb_goto(b->movie, "CURRENTCARGRID", s->car[sel].grid);
        snprintf(t, sizeof t, text_of(b, s->laps > 0 ? "RaceMonitorLapFormat1" : "RaceMonitorLapFormat2"),
                 s->car[sel].lap, s->laps);
        strb_text(b->movie, "LAP", t);
    }
    total_time(s->clock, t, sizeof t);
    strb_text(b->movie, "TOTAL", t);
}

/* func_003D8C18: the pace panel (and the orders) only for the player's
   own car watched (+0x40, the pit's own sequence, not kept: A) */
static int own(const race_bspec *b) { return b->selected == b->player; }

/* func_003DB520: PACE1..5 the pace, PACE6 the overtake, PACE7 the pit,
   PACETEXT the order's word (key_pace1..5, key_overtake, key_pitin) */
static void pace_panel(race_bspec *b)
{
    static const char *const keys[7] = { "key_pace1", "key_pace2", "key_pace3", "key_pace4", "key_pace5",
                                         "key_overtake", "key_pitin" };
    char path[24];
    strb_visible(b->movie, "PACEPANEL", own(b));
    for (int i = 1; i <= 7; i++) {
        snprintf(path, sizeof path, "PACEPANEL.PACE%d", i);
        lit(b, path, i <= 5 ? b->pace + 1 == i : i == 6 ? b->overtake > 0 : b->pit > 0);
    }
    strb_text(b->movie, "PACEPANEL.PACETEXT", b->text >= 0 && b->text < 7 ? text_of(b, keys[b->text]) : "");
}

/* func_003DBBD0: the view's speed ("x %d", blinking while L1 is held) and
   whether the map shows */
static void speed_panel(race_bspec *b)
{
    char t[16] = "";
    if (!b->held_l1 || !((b->blink >> 4) & 1)) snprintf(t, sizeof t, "x %d", b->speed);
    strb_text(b->movie, "SPEED", t);
    strb_visible(b->movie, "MAP_PANEL", b->map);
}

/* func_003DBDF0: how far a is ahead of b, 0 while either has no lap */
static float ahead(const race_bspec_state *s, const race_bspec_car *a, const race_bspec_car *b)
{
    if (!a->run_laps || !b->run_laps) return 0;
    return (s->course_length * (float)a->run_laps + a->distance) - (s->course_length * (float)b->run_laps + b->distance);
}
/* func_003DBE98: a gap's room in the strip */
static float room(float d)
{
    float k = (d - 20) * 0.0125f;
    return 62 + 38 * (k < 0 ? 0 : k > 1 ? 1 : k);
}

/* func_003DBF90: the cars along the strip in race order, each at the room
   its gap to the one ahead takes (the whole centred, from 20), its letter
   lit for the watched car, PACEUP_MARK when it closes on the car ahead
   (003FA508), GAPn between them (func_003DBEE8: the distance in feet or
   metres, laps when it is one or more); DISTUNIT "Gap (%s)" */
static void gaps(race_bspec *b)
{
    const race_bspec_state *s = &b->s;
    char path[32], t[48];
    float total = 0, x, prev = 0;
    int shown[RACE_BSPEC_CARS] = { 0 };
    for (int r = 1; r < s->cars; r++) total += room(ahead(s, &s->car[r - 1], &s->car[r]));
    x = (640 - total) * 0.5f + 20;
    for (int r = 0; r < s->cars; r++) {
        const race_bspec_car *c = &s->car[r];
        float d = 0;
        int letter = c->grid >= 0 && c->grid < RACE_BSPEC_CARS ? c->grid : 0;
        shown[letter] = 1;
        if (r) {
            d = ahead(s, &s->car[r - 1], c);
            x += room(d);
        }
        snprintf(path, sizeof path, "CARPOS_%c", 'A' + letter);
        strb_set_x(b->movie, path, x);
        strb_visible(b->movie, path, 1);
        snprintf(path, sizeof path, "CARPOS_%c.GRID", 'A' + letter);
        strb_goto(b->movie, path, letter * 2 + (c->index == b->selected));
        snprintf(path, sizeof path, "CARPOS_%c.PACEUP_MARK", 'A' + letter);
        strb_visible(b->movie, path, r && c->pace_up);
        if (r) {
            if (d <= 0) t[0] = 0;
            else if (s->course_length > 0 && s->course_length <= d) {
                int l = (int)(d / s->course_length);
                snprintf(t, sizeof t, "%d %s", l, text_of(b, l == 1 ? "WordOfLap" : "WordOfLaps"));
            } else snprintf(t, sizeof t, "%d", (int)(d * (s->feet ? 3.28084f : 1) + 0.5f));
            snprintf(path, sizeof path, "GAP%d.TEXT", r);
            strb_text(b->movie, path, t);
            snprintf(path, sizeof path, "GAP%d", r);
            strb_set_x(b->movie, path, (x + prev) * 0.5f);
            strb_visible(b->movie, path, 1);
        }
        prev = x;
    }
    for (int r = s->cars; r < RACE_BSPEC_CARS; r++) {
        if (r) { snprintf(path, sizeof path, "GAP%d", r); strb_visible(b->movie, path, 0); }
    }
    for (int l = 0; l < RACE_BSPEC_CARS; l++)
        if (!shown[l]) { snprintf(path, sizeof path, "CARPOS_%c", 'A' + l); strb_visible(b->movie, path, 0); }
    snprintf(t, sizeof t, text_of(b, "DistanceFormat"), s->feet ? "ft" : "m");
    strb_text(b->movie, "DISTUNIT", t);
}

/* func_003B03B8: a tyre's short name (TireShort_nn), "-" for none */
static const char *tire_short(const race_bspec *b, int k, char *buf, size_t n)
{
    char key[24];
    if (k < 0 || k == 0xff || k >= 13) return "-";
    snprintf(key, sizeof key, "TireShort_%02d", k);
    snprintf(buf, n, "%s", text_of(b, key));
    return buf;
}

/* func_003DB7B8: the watched car's planned stops (the player's own only),
   the latest first, "-" for the rows past them */
static void pit_panel(race_bspec *b)
{
    const race_bspec_state *s = &b->s;
    int r = find_row(s, b->selected), count = 0;
    char path[24], t[32], k[32];
    strb_visible(b->movie, "PITPANEL", own(b));
    if (r < 0) return;
    for (int i = 0; i < 3; i++) count += s->car[r].pit_lap[2 - i] > 0;
    for (int i = 0; i < 3; i++) {
        int lap = 0, f = -1, rr = -1;
        if (i < count) {
            int e = count - i - 1;
            lap = s->car[r].pit_lap[e];
            f = s->car[r].pit_tire[e][0];
            rr = s->car[r].pit_tire[e][1];
        }
        if (lap > 0) snprintf(t, sizeof t, "%d", lap); else snprintf(t, sizeof t, "-");
        snprintf(path, sizeof path, "PITPANEL.PIT%dLAP", i + 1);
        strb_text(b->movie, path, t);
        snprintf(path, sizeof path, "PITPANEL.PIT%dF", i + 1);
        strb_text(b->movie, path, tire_short(b, f, k, sizeof k));
        snprintf(path, sizeof path, "PITPANEL.PIT%dR", i + 1);
        strb_text(b->movie, path, tire_short(b, rr, k, sizeof k));
    }
}

/* ------------------------------------------------------------- modes */

/* func_003D9C70: a mode asked; the display fades out and in (+0x80) */
static void ask_mode(race_bspec *b, int mode)
{
    if (mode == b->target) return;
    if (mode == 1) b->back = b->mode != 0;
    b->target = mode;
    b->timer = fabsf(b->timer);
}

/* func_003D9158 / 003D91C0 / 003D92B0: the movie's frame for the mode */
static void enter_mode(race_bspec *b)
{
    b->mode = b->target;
    strb_goto(b->movie, NULL, b->mode == 1 ? 0 : 1);
}

/* func_003D9B20: the driver's orders as they are; the word shown the pit,
   else the overtake, else the pace */
static void orders(race_bspec *b)
{
    b->pace = b->s.pace;
    b->overtake = b->s.overtake;
    b->pit = b->s.pit;
    b->text = b->pit ? 6 : b->overtake ? 5 : b->pace;
}

/* func_003D9838: the orders from the pad, for the player's own car */
static void pad_orders(race_bspec *b, uint32_t pressed)
{
    int sfx = 3;
    if (!(pressed & (RACE_BSPEC_LEFT | RACE_BSPEC_RIGHT | RACE_BSPEC_OVERTAKE | RACE_BSPEC_PIT)) || !b->host.order) return;
    if (!own(b)) { sound(b, 3); return; }
    /* 003D9DB0 / 003D9D88 -> 003D9D28: the pace one down or up, 0..4 */
    if ((pressed & RACE_BSPEC_LEFT) && b->pace > 0) { b->host.order(b->host.user, 4, b->pace - 1); sfx = 2; }
    if ((pressed & RACE_BSPEC_RIGHT) && b->pace < 4) { b->host.order(b->host.user, 4, b->pace + 1); sfx = 2; }
    /* 003D9DD8 / 003D9E30: the overtake and the pit turned over */
    if (pressed & RACE_BSPEC_OVERTAKE) { b->host.order(b->host.user, 5, !b->overtake); sfx = 6; }
    if (pressed & RACE_BSPEC_PIT) { b->host.order(b->host.user, 6, !b->pit); sfx = 6; }
    sound(b, sfx);
}

/* func_003D9E88 / 003D9F20: the car before or after the watched one in
   race order, round the ends */
static void select_car(race_bspec *b, int step)
{
    const race_bspec_state *s = &b->s;
    int r = find_row(s, b->selected);
    if (s->cars <= 0) return;
    r = r < 0 ? 0 : (r + step + s->cars) % s->cars;
    b->selected = s->car[r].index;
    if (b->mode == 1) sound(b, 2);
}

static void pad(race_bspec *b, uint32_t pressed, uint32_t held)
{
    if (b->target != b->mode) return;   /* nothing while the display changes */
    /* 003D9520, every mode: up and down the car watched */
    if (pressed & RACE_BSPEC_UP) select_car(b, -1);
    if (pressed & RACE_BSPEC_DOWN) select_car(b, 1);
    /* 003D9918 (off and live): the live view's camera */
    if (b->mode != 1) {
        if (pressed & RACE_BSPEC_VIEW_BACK) b->view = b->view > 0 ? b->view - 1 : 2;
        if (pressed & RACE_BSPEC_VIEW) b->view = (b->view + 1) % 3;
    }
    switch (b->mode) {
    case 0:   /* 003D9720: R1 the monitor, an order's button the live view */
        if (pressed & RACE_BSPEC_R1) ask_mode(b, 1);
        else if (pressed & 0x3800eu) ask_mode(b, 2);
        break;
    case 1:   /* 003D95F0 */
        if (pressed & RACE_BSPEC_R1) { ask_mode(b, b->back ? 2 : 0); break; }
        if (held & RACE_BSPEC_L1) {
            if (pressed & RACE_BSPEC_L1) { b->blink = 0; sound(b, 2); }
            b->held_l1 = 1;
            /* 003DA008 / 003D9FB8: the speed one down or up, 1..3 */
            if (pressed & RACE_BSPEC_L1_LEFT) { if (b->speed > 1) { b->speed--; sound(b, 2); } else sound(b, 3); }
            if (pressed & RACE_BSPEC_L1_RIGHT) { if (b->speed < 3) { b->speed++; sound(b, 2); } else sound(b, 3); }
        } else b->held_l1 = 0;
        if (pressed & RACE_BSPEC_MAP) { b->map = !b->map; sound(b, 2); }
        pad_orders(b, pressed);
        break;
    case 2:   /* 003D97A8: R1 the monitor, R2 the display off */
        if (pressed & RACE_BSPEC_R1) ask_mode(b, 1);
        else if (pressed & RACE_BSPEC_R2) ask_mode(b, 0);
        pad_orders(b, pressed);
        break;
    }
}

void race_bspec_update(race_bspec *b, const race_bspec_state *s, uint32_t pressed, uint32_t held, double dt)
{
    if (!b || !s) return;
    b->s = *s;
    b->state_ok = 1;
    if (find_row(s, b->selected) < 0) b->selected = b->player;
    orders(b);
    /* 003D9998 +78..130 forces the live view during stages 1..5.
       003D8C48 (+40/+48) then suppresses ALL monitor pad handlers;
       the pit menu owns input independently of monitor mode. */
    if(s->pit_sequence>0 && s->pit_sequence<6)ask_mode(b,2);
    if(gt4_pit_monitor_input(s->pit_sequence,b->target!=b->mode))pad(b, pressed, held);
    /* 003D8DB8: the change of mode over 0.2 s each way */
    b->timer = fmaxf(b->timer - (float)dt / 0.2f, -1);
    if (b->timer <= 0 && b->mode != b->target) enter_mode(b);
    b->blink = (b->blink + 1) & 0x1f;
    pace_panel(b);
    standings(b);
    gaps(b);
    speed_panel(b);
    pit_panel(b);
    strb_tick(b->movie, dt);
}

void race_bspec_draw(race_bspec *b)
{
    float light = race_bspec_light(b);
    if (!b || !b->state_ok || light <= 0) return;
    strb_draw_faded(b->movie, light);
}

int race_bspec_mode(const race_bspec *b) { return b ? b->mode : 0; }
int race_bspec_speed(const race_bspec *b) { return b ? b->speed : 1; }
int race_bspec_selected(const race_bspec *b) { return b ? b->selected : 0; }
int race_bspec_view(const race_bspec *b) { return b ? b->view : 2; }
int race_bspec_row(const race_bspec *b, int r)
{
    return b && b->state_ok && r >= 0 && r < b->s.cars ? b->s.car[r].index : -1;
}
float race_bspec_light(const race_bspec *b)
{
    if (!b) return 0;
    if (b->mode == 0 && b->target == 0) return 0;
    return fminf(fabsf(b->timer), 1);
}
const char *race_bspec_tooltip(const race_bspec *b)
{
    /* 003D91C0 / 003D92B0: the help line of the monitor and of the live view */
    if (!b || b->mode == 0) return "";
    return b->mode == 1 ? "RaceMonitorToolTip1" : "Tool Tip1";
}
