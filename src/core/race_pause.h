/*
 * The race's pause menu (the game's RacePause, vtable 0x681B28):
 * /menu/common/pause.strb, its scripts answered by func_003C3260's
 * callbacks (EXIT_MENU, RETRYENABLE, SCREENSETTINGENABLE, CONFIRMEXIT,
 * NAVIGATEVERTICAL / NAVIGATEALL, PLAY_SOUND, SETVAL / DEFAULTVAL).
 */
#ifndef OPENGT4_RACE_PAUSE_H
#define OPENGT4_RACE_PAUSE_H

#include "vol.h"

typedef struct race_pause race_pause;

/* what the menu chose (pause +0x1C as EXIT_MENU sets it) */
enum { RACE_PAUSE_OPEN = 0, RACE_PAUSE_CONTINUE = 3, RACE_PAUSE_EXIT = 4, RACE_PAUSE_RETRY = 5 };

typedef void (*race_pause_sound)(void *user, const char *name);

/* retry, screen, confirm: the menu's options (pause +0x28, +0x30, +0x2C) */
race_pause *race_pause_open(gt4_vol *vol, int retry, int screen, int confirm, race_pause_sound sound, void *user);
void race_pause_close(race_pause *p);
/* a key going down, as Flash's Key codes (37..40 the arrows, 13 enter) */
void race_pause_key(race_pause *p, int code);
void race_pause_update(race_pause *p, double dt);
void race_pause_draw(race_pause *p);
int race_pause_state(const race_pause *p);

/* Display options shared with ResultLicense (003EFEA8). */
void race_pause_display_options(int options[4]);
#endif
