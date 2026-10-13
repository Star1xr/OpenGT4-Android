/*
 * The pit menu (race_pitmenu.h): the tyre lists of 004076E0 (the 13
 * compounds of the car's tyre's class, 00441300) and the callbacks of
 * 003DCFB0, by name (the script calls them in mixed case).
 */
#include "race_pitmenu.h"
#include "rtext.h"
#include "strobe_player.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct race_pitmenu {
    gt4_vol *vol;
    strb_player *movie;
    race_pitmenu_sound sound;
    void *user;
    uint8_t *rt_data;
    gt4_rtext rt;
    int rt_ok;
    int up;                /* +0xDC */
    int progress;          /* +0xE0: 1 until the choice is confirmed */
    race_pitmenu_setup s;
    unsigned bits[2];      /* the lists by axle (list +0x48, +0x94) */
    int count[2];          /* +0x54, +0xA0 */
    int value[2];          /* +0xF0, +0xF4: the compounds' rows */
    int feed;              /* +0xF8 */
    int mode;              /* +0xFC */
    int chosen;
    race_pitmenu_choice choice;
};

/* func_00441300: a compound's class (0..5 road and sports, 6..10 racing,
   11 dirt, 12 snow) */
static int tyre_class(int c)
{
    if (c < 0 || c > 12) return -1;
    return c <= 5 ? 0 : c <= 10 ? 1 : c == 11 ? 2 : 3;
}

/* func_00407A60 / 00407B10: the compound of a row, -1 past them */
static int compound_at(const race_pitmenu *m, int axle, int row)
{
    for (int c = 0, n = 0; c < 13; c++)
        if (m->bits[axle] >> c & 1) { if (n == row) return c; n++; }
    return -1;
}

/* func_00407BC0 / 00407C40: the row of a compound, -1 not in the list */
static int row_of(const race_pitmenu *m, int axle, int c)
{
    int n = 0;
    if (c < 0 || c > 12 || !(m->bits[axle] >> c & 1)) return -1;
    for (int k = 0; k < c; k++) n += m->bits[axle] >> k & 1;
    return n;
}

static int named(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return 0;
    return !*a && !*b;
}

static double arg_num(const strb_value *v) { return v->t == STRB_STR ? atof(v->s) : v->n; }

/* func_003DCF10: an item by its instance's name */
static int item_of(const strb_value *v)
{
    const char *s = v->t == STRB_STR ? v->s : "";
    return named(s, "FTIRE") ? 0 : named(s, "RTIRE") ? 1 : named(s, "FEED") ? 2 : 3;
}

/* func_003AEAE8: the race's texts ("---" D_00621C94 is no key) */
static const char *text_of(const race_pitmenu *m, const char *key)
{
    const char *t = m->rt_ok ? gt4_rtext_get(&m->rt, "RaceDisplay", key) : NULL;
    return t ? t : key;
}

static void ret_str(strb_value *ret, const char *s)
{
    ret->t = STRB_STR;
    snprintf(ret->s, sizeof ret->s, "%s", s);
}
static void ret_num(strb_value *ret, double n) { ret->t = STRB_NUM; ret->n = n; }

static int call(void *user, const char *name, const strb_value *args, int nargs, strb_value *ret)
{
    race_pitmenu *m = user;
    int item = nargs > 0 ? item_of(&args[0]) : 3;
    memset(ret, 0, sizeof *ret);
    if (named(name, "PLAY_SOUND")) {
        if (m->sound && nargs > 0 && args[0].t == STRB_STR) m->sound(m->user, args[0].s);
    } else if (named(name, "PROGRESS")) {
        m->progress = 0;
    } else if (named(name, "EXIT")) {
        /* 003DD6D8 / 003DD720: the rows as compounds, -1 (0xFF) for "no
           change"; the mode only when it may change */
        int f = m->value[0] < m->count[0] ? compound_at(m, 0, m->value[0]) : -1;
        int r = m->value[1] < m->count[1] ? compound_at(m, 1, m->value[1]) : -1;
        m->choice.front = f < 0 ? 0xff : f;
        m->choice.rear = r < 0 ? 0xff : r;
        m->choice.feed = m->feed;
        m->choice.mode = m->s.mode ? m->mode != 0 : 0xff;
        m->chosen = 1;
        m->up = 0;
    } else if (named(name, "ITEM")) {          /* (name, row): its text */
        int row = nargs > 1 ? (int)arg_num(&args[1]) : 0;
        char key[24];
        if (item < 2) {
            if (!m->s.tyres) ret_str(ret, "---");
            else if (row == m->count[item]) ret_str(ret, text_of(m, "Tire_NoChange"));
            else {
                snprintf(key, sizeof key, "Tire_%02d", compound_at(m, item, row) % 13);
                ret_str(ret, text_of(m, key));
            }
        } else if (item == 2) ret_str(ret, m->s.feed ? text_of(m, row == 1 ? "DoFeed" : "DontFeed") : "---");
        else ret_str(ret, m->s.mode ? text_of(m, row == 1 ? "BSpecMode" : "ASpecMode") : "---");
    } else if (named(name, "NITEM")) {         /* (name): its rows */
        ret_num(ret, item < 2 ? (m->s.tyres ? m->count[item] + 1 : 1) : item == 2 ? (m->s.feed ? 2 : 1) : (m->s.mode ? 2 : 1));
    } else if (named(name, "DEFAULTVAL")) {    /* (name): the row it starts on */
        ret_num(ret, item < 2 ? m->value[item] : item == 2 ? m->feed != 0 : m->s.mode ? m->mode != 0 : 0);
    } else if (named(name, "RETURNVAL")) {     /* (item's number, row) */
        int v = nargs > 1 ? (int)arg_num(&args[1]) : 0;
        item = nargs > 0 ? (int)arg_num(&args[0]) : 3;
        if (item < 2) m->value[item] = v;
        else if (item == 2) m->feed = v != 0;
        else m->mode = v != 0;
    } else return 0;
    return 1;
}

race_pitmenu *race_pitmenu_open(gt4_vol *vol, race_pitmenu_sound sound, void *user)
{
    race_pitmenu *m = calloc(1, sizeof *m);
    size_t n = 0;
    if (!m) return NULL;
    m->vol = vol;
    m->sound = sound;
    m->user = user;
    m->rt_data = gt4_vol_read(vol, "/rtext/raceUS.rt2", &n);
    m->rt_ok = m->rt_data && gt4_rtext_open(&m->rt, m->rt_data, n);
    return m;
}

void race_pitmenu_close(race_pitmenu *m)
{
    if (!m) return;
    strb_close(m->movie);
    free(m->rt_data);
    free(m);
}

void race_pitmenu_show(race_pitmenu *m, const race_pitmenu_setup *s)
{
    strb_host host;
    if (!m || !s || m->up) return;
    m->s = *s;
    /* 004076E0: the compounds of the class of each axle's tyre; 004078D0
       (GT Mode) keeps of them the player's own and the one on the car */
    for (int a = 0; a < 2; a++) {
        int cur = a ? s->rear : s->front, cls = tyre_class(cur);
        m->bits[a] = 0;
        m->count[a] = 0;
        for (int c = 0; c < 13; c++) {
            if (cls < 0 || tyre_class(c) != cls) continue;
            if ((s->owned_filter || s->owned[a]) && c != cur && !(s->owned[a] >> c & 1)) continue;
            m->bits[a] |= 1u << c;
            m->count[a]++;
        }
        /* 003DCC50: the row of the tyre on, else "no change" */
        m->value[a] = row_of(m, a, cur);
        if (m->value[a] < 0 || !s->tyres) m->value[a] = m->count[a];
    }
    m->mode = s->bspec != 0;
    m->feed = s->feed ? 1 : 0;
    m->progress = 1;
    m->chosen = 0;
    /* 004751D0: the movie from its start */
    strb_close(m->movie);
    memset(&host, 0, sizeof host);
    host.call = call;
    host.user = m;
    host.palette = 1;
    m->up = 1;
    m->movie = strb_open(m->vol, "/menu/common/pitmenu.strb", m->rt_ok ? &m->rt : NULL, "RaceDisplay", &host);
    if (!m->movie) { m->up = 0; return; }
    strb_navigate(m->movie, 0);
}

void race_pitmenu_hide(race_pitmenu *m)
{
    if (!m) return;
    m->up = 0;
    m->progress = 0;
}

int race_pitmenu_up(const race_pitmenu *m) { return m && m->up; }

void race_pitmenu_key(race_pitmenu *m, int code)
{
    if (m && m->up && m->movie) strb_key(m->movie, code);
}

/* 003DD5D8: the movie at 1/60 s a step */
void race_pitmenu_update(race_pitmenu *m, double dt)
{
    if (m && m->up && m->movie) strb_tick(m->movie, dt);
}

void race_pitmenu_draw(race_pitmenu *m)
{
    if (m && m->up && m->movie) strb_draw(m->movie);
}

int race_pitmenu_choice_take(race_pitmenu *m, race_pitmenu_choice *out)
{
    if (!m || !m->chosen) return 0;
    m->chosen = 0;
    if (out) *out = m->choice;
    return 1;
}
