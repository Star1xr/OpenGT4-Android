/*
 * The pit menu (the game's class at 003DCA28..003DD7D8, world+0xE40C):
 * /menu/common/pitmenu.strb, its items FTIRE, RTIRE, FEED and DMODE
 * answered by 003DCFB0's callbacks (PLAY_SOUND, PROGRESS, EXIT, ITEM,
 * NITEM, DEFAULTVAL, RETURNVAL). The host opens it when the watched car's
 * stop asks (0035BC70) and hands the choice to the car (0035BF08).
 */
#ifndef OPENGT4_RACE_PITMENU_H
#define OPENGT4_RACE_PITMENU_H

#include "vol.h"

typedef struct race_pitmenu race_pitmenu;

typedef void (*race_pitmenu_sound)(void *user, const char *name);

/* the menu's setup (003DCC50 / 003DD540) */
typedef struct {
    int tyres, feed, mode;     /* what may change: +0xE8, +0xEC, +0x100 */
    int front, rear;           /* the car's compounds (cond+0xFB20), 0..12 */
    unsigned owned[2];         /* the compounds the player has by axle
                                  (004078D0, GT Mode), bits; 0 all */
    int owned_filter;          /* distinguish zero owned compounds from Arcade */
    int bspec;                 /* the driving mode now (+0xFC: world+0xCC8 1 or 3) */
} race_pitmenu_setup;

/* the choice (EXIT): the compounds (0xFF none), the fuel, the driving
   mode (0 A-Spec, 1 B-Spec, 0xFF kept) */
typedef struct { int front, rear, feed, mode; } race_pitmenu_choice;

race_pitmenu *race_pitmenu_open(gt4_vol *vol, race_pitmenu_sound sound, void *user);
void race_pitmenu_close(race_pitmenu *m);
/* 003DD540: shown from its start, unless it is up */
void race_pitmenu_show(race_pitmenu *m, const race_pitmenu_setup *s);
/* 003DD5C8: taken down without a choice */
void race_pitmenu_hide(race_pitmenu *m);
int race_pitmenu_up(const race_pitmenu *m);
/* a key going down, as Flash's Key codes (37..40 the arrows, 13 enter) */
void race_pitmenu_key(race_pitmenu *m, int code);
void race_pitmenu_update(race_pitmenu *m, double dt);
void race_pitmenu_draw(race_pitmenu *m);
/* 1 once the menu was left with a choice (taken: the next call 0) */
int race_pitmenu_choice_take(race_pitmenu *m, race_pitmenu_choice *out);

#endif
