/*
 * A player for GT4's converted Flash (STRB version 0) with its scripts:
 * nested sprites on their timelines, placements with their transforms and
 * colours, rectangle shapes (solid and bitmap fills), edit texts in the
 * disc's fonts, buttons and the ActionScript subset the race screens use
 * (frame actions, enterFrame clip actions, play/stop/goto, members of
 * named instances, host functions). The host answers the script's calls
 * (the game's registered callbacks, e.g. ResultArcade's func_003DF3B8)
 * and draws the "extrender" placements (the car icons).
 *
 * The movie's 640x480 units are drawn in a 640x480 view.
 */
#ifndef OPENGT4_STROBE_PLAYER_H
#define OPENGT4_STROBE_PLAYER_H

#include <stdint.h>
#include "rtext.h"
#include "vol.h"

typedef struct strb_player strb_player;

typedef enum { STRB_UNDEF, STRB_NUM, STRB_STR, STRB_BOOL, STRB_OBJ, STRB_FUNC } strb_type;
typedef struct {
    strb_type t;
    double n;              /* STRB_NUM, STRB_BOOL */
    char s[96];            /* STRB_STR */
    void *obj;             /* STRB_OBJ: an instance; STRB_FUNC: the timeline it was made on */
    int place;             /*           and its placement, -1 itself, -2 the Key object */
    size_t code, pool;     /* STRB_FUNC: its DefineFunction record, the constant pool then in force */
} strb_value;

typedef struct {
    /* A script's call of a host function: 0 when there is none */
    int (*call)(void *user, const char *name, const strb_value *args, int nargs, strb_value *ret);
    /* An extrender placement inside the instance named `name`, its box in
       the view's units, the colour multiplier (0..255) */
    void (*ext)(void *user, const char *name, float x, float y, float w, float h, const uint8_t color[4]);
    void *user;
    /* the same placement by its named instances from the root and the
       extrender's own name ("BAR3.chip"), for a host whose anonymous
       copies differ only by where they sit; NULL when unused */
    void (*ext_path)(void *user, const char *path, float x, float y, float w, float h, const uint8_t color[4]);
    /* the palette of fonts named without "#": func_00490290 sets the
       text engine's (D_00624980 +0x2C) only on "#", so such a font draws
       with the one left by the texts before it (0 ink over outline, 1 ink
       alone) */
    int palette;
    /* the edit texts' heights already hold their faces' percentages
       (racemonitor.strb), so these are not taken again */
    int field_size;
} strb_host;

/* path: the movie on the disc; rt: the texts its edit texts' keys name
   (page `page`), NULL to show the keys */
strb_player *strb_open(gt4_vol *vol, const char *path, const gt4_rtext *rt, const char *page, const strb_host *host);
void strb_close(strb_player *p);
/* Runs the movie for dt seconds at its own rate */
void strb_tick(strb_player *p, double dt);
/* The confirm button: the buttons on screen run their press actions */
void strb_press(strb_player *p);
/* A key going down, as Flash's Key codes (37..40 the arrows, 13 enter):
   the keyDown clip events run with Key.getCode() answering it; with the
   buttons active the arrows move the focus between them and enter presses
   the focused one. */
void strb_key(strb_player *p, int code);
/* The buttons' navigation (the host's NavigateVertical / NavigateAll):
   up/down only, or every arrow; either makes the buttons active again */
void strb_navigate(strb_player *p, int all);
void strb_draw(strb_player *p);
/* the same with every colour's alpha times `alpha` (a display fading) */
void strb_draw_faded(strb_player *p, float alpha);
/* The host's own reach into the movie, as the game's native code drives
   it by instance names (dotted paths from the root, "BAR1.NAME"; NULL or
   "" the root): a sprite to a frame and stopped (0 the first), an edit
   text's text, a sprite shown or hidden. 0 when the path names nothing. */
int strb_goto(strb_player *p, const char *path, int frame);
int strb_text(strb_player *p, const char *path, const char *text);
/* the same cut to the edit text's width with "..." (func_0048F790) */
int strb_text_fit(strb_player *p, const char *path, const char *text);
int strb_visible(strb_player *p, const char *path, int on);
/* a sprite's _x in its parent (func_0047DA90), the authored one no more */
int strb_set_x(strb_player *p, const char *path, float x);

#endif
