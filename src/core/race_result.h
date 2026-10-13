/*
 * The Arcade race's results (the game's ResultArcade, vtable 0x683288):
 * /menu/common/result.strb played over the race from the player's finish,
 * its rows filled as the cars run and finish (func_003DFAE0 each frame,
 * func_003DFCC8 at each finish), its scripts answered by the callbacks of
 * func_003DF3B8. Over when the movie calls cb_raceexit.
 */
#ifndef OPENGT4_RACE_RESULT_H
#define OPENGT4_RACE_RESULT_H

#include <stdint.h>
#include "vol.h"

typedef struct race_result race_result;

/* the race as the board reads it, per car (the player's index `player`) */
typedef struct {
    int cars, player;
    int rank[6];           /* car+0x5B3, 1-based: the board's order */
    int finished[6];       /* car+0x5B6 */
    uint32_t time[6];      /* car+0x718: at the finish its race time (ms) */
    int laps[6];           /* func_00343B50 */
    const char *name[6];   /* the car's name (its entry +0x350C) */
    /* ResultArcade +0x50 (003DFEB0): 0 the race time above; 4 the top
       speed (car +0x6CC, 003E0490 "%.2f%s" in the option's unit, as
       RaceMachineTest sets it for its top-speed test, 003BE07C) */
    int mode;
    float top_speed[6];    /* m/s */
    int mph;               /* D_006244D8: 1 mph, 0 km/h */
} race_result_state;

/* draws car `car`'s icon in the box (the extrender render_car_icon) */
typedef void (*race_result_icon)(void *user, int car, float x, float y, float w, float h, const uint8_t color[4]);

/* plays a system sound the movie asks for (play_sound) */
typedef void (*race_result_sound)(void *user, const char *name);
/* 003E0E38: the authored "screen" extrender places the backing race. */
typedef void (*race_result_screen)(void *user,float x,float y,float w,float h);
void race_result_set_screen(race_result *r,race_result_screen draw);
typedef struct {float position[3],angles[3],alpha;int model;} race_result_trophy_pose;
typedef void (*race_result_trophy)(void *user,const race_result_trophy_pose *pose);
void race_result_set_trophy(race_result *r,race_result_trophy draw);
race_result *race_result_open(gt4_vol *vol, race_result_icon icon, void *user);
race_result *race_result_mission_open(gt4_vol *vol,race_result_icon icon,void *user);
race_result *race_result_license_open(gt4_vol *vol,int32_t state,uint32_t time,const char *code,uint32_t bronze,void *user);
void race_result_key(race_result *r,int key);
int race_result_retry(const race_result *r);
void race_result_set_sound(race_result *r, race_result_sound sound);
void race_result_close(race_result *r);
/* one frame of the board and its movie (dt seconds) */
void race_result_update(race_result *r, const race_result_state *s, double dt);
/* the confirm button */
void race_result_press(race_result *r);
void race_result_draw(race_result *r);
/* 1 once the movie let the race go (cb_raceexit) */
int race_result_done(const race_result *r);

#endif
