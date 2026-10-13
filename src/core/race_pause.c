/*
 * The race's pause menu (race_pause.h): the movie and the callbacks of
 * func_003C3260, by name (the script calls them in mixed case).
 */
#include "race_pause.h"
#include "rtext.h"
#include "strobe_player.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct race_pause {
    strb_player *movie;
    int state, retry, screen, confirm;
    int nav;               /* a navigation asked while the movie opened: 1 vertical, 2 all */
    race_pause_sound sound;
    void *user;
    uint8_t *rt_data;
    gt4_rtext rt;
    int rt_ok;
};

/* the screen setting's two values (func_003C3238: BRIGHTNESS 0, else 1),
   kept for the session like the game's display settings (D_00622F4C) */
static int screen_value[2];

static int named(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return 0;
    return !*a && !*b;
}

static double arg_num(const strb_value *v) { return v->t == STRB_STR ? atof(v->s) : v->n; }

static int call(void *user, const char *name, const strb_value *args, int nargs, strb_value *ret)
{
    race_pause *p = user;
    memset(ret, 0, sizeof *ret);
    if (named(name, "PLAY_SOUND")) {
        if (p->sound && nargs > 0 && args[0].t == STRB_STR) p->sound(p->user, args[0].s);
    } else if (named(name, "SETVAL")) {            /* (name, value) */
        if (nargs > 1) screen_value[named(args[0].s, "BRIGHTNESS") ? 0 : 1] = (int)arg_num(&args[1]);
    } else if (named(name, "DEFAULTVAL")) {        /* (name): the value kept */
        ret->t = STRB_NUM;
        ret->n = nargs > 0 ? screen_value[named(args[0].s, "BRIGHTNESS") ? 0 : 1] : 0;
    } else if (named(name, "EXIT_MENU")) {         /* "exit" 4, "retry" 5, "continue" 3 */
        const char *s = nargs > 0 && args[0].t == STRB_STR ? args[0].s : "";
        p->state = !strcmp(s, "exit") ? RACE_PAUSE_EXIT : !strcmp(s, "retry") ? RACE_PAUSE_RETRY : RACE_PAUSE_CONTINUE;
    } else if (named(name, "RETRYENABLE") || named(name, "SCREENSETTINGENABLE") || named(name, "CONFIRMEXIT")) {
        ret->t = STRB_BOOL;
        ret->n = named(name, "RETRYENABLE") ? p->retry : named(name, "CONFIRMEXIT") ? p->confirm : p->screen;
    } else if (named(name, "NAVIGATEVERTICAL") || named(name, "NAVIGATEALL")) {
        /* the movie's first frames run inside strb_open, before p->movie */
        if (p->movie) strb_navigate(p->movie, named(name, "NAVIGATEALL"));
        else p->nav = named(name, "NAVIGATEALL") ? 2 : 1;
    } else return 0;
    return 1;
}

race_pause *race_pause_open(gt4_vol *vol, int retry, int screen, int confirm, race_pause_sound sound, void *user)
{
    race_pause *p = calloc(1, sizeof *p);
    strb_host host;
    if (!p) return NULL;
    p->retry = retry;
    p->screen = screen;
    p->confirm = confirm;
    p->sound = sound;
    p->user = user;
    memset(&host, 0, sizeof host);
    host.call = call;
    host.user = p;
    /* its fonts have no "#": the race's texts leave the ink alone (the
       PCSX2 pause: black labels without an outline, as the replay's
       hn_14 message) */
    host.palette = 1;
    /* the movie's texts are keys of the race's rtext, page RaceDisplay
       as the results' ("Screen Setting" -> "Screen Settings", the PCSX2
       pause) */
    {
        size_t n = 0;
        p->rt_data = gt4_vol_read(vol, "/rtext/raceUS.rt2", &n);
        p->rt_ok = p->rt_data && gt4_rtext_open(&p->rt, p->rt_data, n);
    }
    p->movie = strb_open(vol, "/menu/common/pause.strb", p->rt_ok ? &p->rt : NULL, "RaceDisplay", &host);
    if (!p->movie) {
        free(p->rt_data);
        free(p);
        return NULL;
    }
    /* 00474F38 +0x4C..0x60: input navigator starts with +0x14/+0x18
       both 1 (NavigateAll). pause.strb's menu_no_screen label skips
       frames 3/19 which request it explicitly; Battle2P uses that label. */
    strb_navigate(p->movie, p->nav != 1);
    return p;
}

void race_pause_close(race_pause *p)
{
    if (!p) return;
    strb_close(p->movie);
    free(p->rt_data);
    free(p);
}

void race_pause_key(race_pause *p, int code) { if (p) strb_key(p->movie, code); }
void race_pause_update(race_pause *p, double dt) { if (p) strb_tick(p->movie, dt); }
void race_pause_draw(race_pause *p) { if (p) strb_draw(p->movie); }
int race_pause_state(const race_pause *p) { return p ? p->state : RACE_PAUSE_CONTINUE; }

void race_pause_display_options(int options[4])
{
    /* Preserve the session's BRIGHTNESS/contrast values. The existing
       native option store exposes these two; colour options remain 0. */
    options[0]=screen_value[0];options[1]=screen_value[1];options[2]=options[3]=0;
}
