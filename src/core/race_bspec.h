/*
 * B-Spec's race display (the game's monitor at world+0xE424, class at
 * 003D8DB8 / 003DA150): /menu/common/racemonitor.strb driven by its
 * instance names. Three modes (+0x28): 2 the live view (the movie's
 * frame 1, the pace panel over the race), 1 the Race Monitor (frame 0
 * over the whole screen: standings, laps, gaps, the map), 0 the display
 * off. The orders (pace, overtake, pit) are the game's 003F1BD8, handed to
 * the host; the race itself is the host's.
 */
#ifndef OPENGT4_RACE_BSPEC_H
#define OPENGT4_RACE_BSPEC_H

#include <stdint.h>
#include "vol.h"

#define RACE_BSPEC_CARS 6

typedef struct {
    int index;            /* the car's place in the race's entries */
    int grid;             /* entry +0x3444: its letter (GRID's frame) */
    char name[64];        /* entry +0x350C */
    int lap;              /* func_00343B50 */
    uint32_t last_lap;    /* entry +0x11DC (0x157529FF none) */
    uint32_t best_lap;    /* func_0035E3F0 */
    uint32_t diff;        /* entry +0x11E8: behind the leader, ms */
    int diff_laps;        /* entry +0x11EC: or laps behind */
    int pits;             /* func_003FA498 */
    int state;            /* func_003DA6F8: 0, 2 fuel, 3 in the pit, 4 penalty, 5 a tyre */
    int in_pit;           /* func_0035BBD8 */
    int run_laps;         /* func_00343B48: car+0x5B0 */
    float distance;       /* car+0x5A8: along the lap */
    int pace_up;          /* func_003FA508: closing on the car ahead */
    int pit_lap[3];       /* func_003FA4B0: its planned stops' laps (0 none) */
    int pit_tire[3][2];   /* and their front and rear tyres (-1 / 0xFF none) */
} race_bspec_car;

typedef struct {
    int cars;
    race_bspec_car car[RACE_BSPEC_CARS];   /* in race order (func_003B6FD0) */
    int player;           /* the player's car's index */
    uint32_t clock;       /* the race's clock, ms (func_00358440) */
    int laps;             /* the session's 0x128 */
    int pace, overtake, pit;   /* the player's driver (003F1BD8 kinds 7, 8, 9) */
    float course_length;  /* func_00395400 */
    int feet;             /* D_006244D8: the gaps in feet (1) or metres (0) */
    int pit_sequence;     /* 003D9998: watched car's 0035BBD8, +0x40 gate */
} race_bspec_state;

/* the pad as the monitor reads it (its controller's bits, 00426AF8 the
   ones pressed, 00426AF0 the ones held) */
enum {
    RACE_BSPEC_R1 = 0x1, RACE_BSPEC_LEFT = 0x4, RACE_BSPEC_RIGHT = 0x8,
    RACE_BSPEC_UP = 0x20, RACE_BSPEC_DOWN = 0x40, RACE_BSPEC_L1 = 0x80,
    RACE_BSPEC_L1_LEFT = 0x100, RACE_BSPEC_L1_RIGHT = 0x200, RACE_BSPEC_MAP = 0x400,
    RACE_BSPEC_VIEW_BACK = 0x1000, RACE_BSPEC_VIEW = 0x2000, RACE_BSPEC_R2 = 0x8000,
    RACE_BSPEC_OVERTAKE = 0x10000, RACE_BSPEC_PIT = 0x20000
};

typedef struct {
    /* 003F1BD8: kind 4 the pace (0..4), 5 the overtake, 6 the pit (0/1)
       for the player's car */
    void (*order)(void *user, int kind, int value);
    /* the monitor's sound (+0x9C): 2, 3 or 6 */
    void (*sound)(void *user, int id);
    /* an extrender of the movie by its path ("CURRENTCAR_ICON.current_car",
       "BAR3.chip", "MAP_PANEL.map"), its box in 640x480 and colour */
    void (*ext)(void *user, const char *path, float x, float y, float w, float h, const uint8_t color[4]);
    void *user;
} race_bspec_host;

typedef struct race_bspec race_bspec;

race_bspec *race_bspec_open(gt4_vol *vol, const race_bspec_host *host, int player);
void race_bspec_close(race_bspec *b);
/* a frame: the race's state, the pad's bits pressed and held, seconds */
void race_bspec_update(race_bspec *b, const race_bspec_state *s, uint32_t pressed, uint32_t held, double dt);
void race_bspec_draw(race_bspec *b);
int race_bspec_mode(const race_bspec *b);      /* 0 off, 1 monitor, 2 live */
int race_bspec_speed(const race_bspec *b);     /* the view's speed (+0x74), 1..3 */
int race_bspec_selected(const race_bspec *b);  /* the car watched (+0x64) */
int race_bspec_view(const race_bspec *b);      /* the live view's camera (+0x94), 0..2 */
/* the car a row of the standings shows (BARn, 0-based), -1 none */
int race_bspec_row(const race_bspec *b, int r);
/* the help line's rtext key (RaceDisplay page), "" none */
const char *race_bspec_tooltip(const race_bspec *b);
/* the display's light 0..1 while it changes mode (+0x80) */
float race_bspec_light(const race_bspec *b);

#endif
