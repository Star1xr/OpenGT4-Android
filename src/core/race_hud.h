/*
 * The race display (the HUD): the original's RaceDisplay elements drawn
 * from the disc's /race_display/gt4/<lang>/display.gpb and race.fnt, at
 * the positions and alignments the game leaves them (640x480 units, the
 * top-level RaceDisplay at 0x01CF4D50 of the Arcade captures), each drawn
 * as its class does (func_003A3B58 the rank, func_003A43A8 the times,
 * func_003AB010 the lap, ...).
 */
#ifndef OPENGT4_RACE_HUD_H
#define OPENGT4_RACE_HUD_H

#include <stdint.h>
#include "vol.h"

typedef struct race_hud race_hud;

/* What the display shows of the watched car (the RaceDisplay's update,
   func_0039DBB8, func_0039ECD0). Times in ms, 0x157529FF none. */
typedef struct {
    int rank, cars;
    int split, player; /* horizontal Battle2P mode 8; watched entry 0/1 */
    int lap, laps;
    uint32_t lap_time, best_lap, last_lap;
    float speed;          /* m/s */
    float rpm;
    int gear;
    float throttle, brake, side_g;
    int shift_lamp;
    float fuel, fuel_cap;  /* no tank at 0 */
    int refueling;       /* 0039E790..0039E7C8: 0035BEB0 == 1 */
    float rev_limit;       /* the tachometer's top */
    int aid, brake_lock;   /* the signs' sources */
    int driving;           /* the player drives: the signs and fuel shown */
    float map_x[6], map_y[6]; /* each car on the course (the render frame's x, z) */
    int map_show[6];       /* how it shows on the map: 3, 2 faded, else not */
    int start_frames;      /* frames to the start: START when it reaches 0 */
    int count_frames;      /* func_0034C1C8: watched car's countdown */
    int rolling;           /* the rolling start uses a different display sequence */
    int start_seconds;     /* the initial countdown duration (race+0xB8) */
    float intro_elapsed;   /* simulation time, also runs during a rolling start */
    int aspec_points;       /* func_003C0FF8: the offered points */
    float speed_scale;     /* the speedometer's top (func_0036CDF8: 80 km/h or 50 mph per step) */
    float tacho_scale;     /* the round tachometer's top (params+0x1075 * 1000) */
    unsigned odometer;     /* car+0x6DC */
    int finished;          /* the watched car finished: FINISH, the display fades */
    int panel;            /* 0 the chase camera's (RaceSimplePanel), 1 the onboard one */
    /* B-Spec's display (003A2360 cases 2/3: D_006A0BA0, the panel's
       +0x18/+0x19/+0x1B): the race's time ("Total Time", +0x1438) in place
       of the best and last laps; the panel without its gauges, with the
       fuel and the tyres (tire_wear, 003A5FD8) in their colours
       (003D80A8 -> 003446D8 -> 003677F8, RGB) */
    int bspec;
    uint32_t total_time;
    uint8_t tire_rgb[4][3];
    int unit;             /* D_006244D8: 0 km/h, 1 mph */
    int signal;           /* the car's signal (car+0x6FE) */
    int penalty;          /* the penalty's frames left (car+0x712, func_0035FC60), 0 once finished */
    int replay;           /* RaceDisplay +0x20 2 (the replay; 0 in the race): the music's title */
    int license,license_rank,license_pylons,license_pylon_limit,license_wait;
    uint32_t license_times[3]; /* 003C0B88: gold, silver, bronze */
} race_hud_state;

race_hud *race_hud_open(gt4_vol *vol);
/* The course's map (its file's GTCM block, header +0x84) for the mini map */
void race_hud_set_course(race_hud *h, const char *course);
void race_hud_close(race_hud *h);
/* One frame of the display over the race view (dt seconds). Leaves the
   2D view at 640x448. */
void race_hud_draw(race_hud *h, const race_hud_state *s, float dt);
/* the start (race event 4 of the watched car, func_0039E9B0): START */
void race_hud_start(race_hud *h);
/* message 0x13: a top speed in the option's unit (Power & Speed, 003A12A0) */
void race_hud_top_speed(race_hud *h, float speed, int mph);
/* D_006184F0: 16:9; the elements whose alignment has +4 move 96 units out */
void race_hud_set_widescreen(int on);
/* a help line (a RaceDisplay key, its %B buttons) in the bottom band */
void race_hud_draw_tooltip(race_hud *h, const char *key, float alpha);
/* B-Spec's monitor tyres and fuel in the box (x, y, w, h) */
void race_hud_draw_tire_panel(race_hud *h, const race_hud_state *s, float x, float y, float w, float hh, float alpha);
/* a paint chip (func_0042E160): four colours as the car's VARIATION row
   keeps them (gt4_race_run_paint_chip), in a box of 640x480 */
void race_hud_draw_chip(const uint32_t chip[4], float x, float y, float w, float hh, float alpha);
/* the sections of a lap (the monitor's "sector"), n colours 0xAABBGGRR,
   the current one marked */
void race_hud_draw_sectors(const uint32_t *colors, int n, int current, float x, float y, float w, float hh, float alpha);
/* B-Spec's monitor map: the course about (cx, cy), each car's letter, `lit` orange */
void race_hud_draw_map_cars(race_hud *h, const race_hud_state *s, float cx, float cy, float box, const int letter[6], int lit, float alpha);
/* the replay's pause: "Press SELECT to resume" on the darkened bottom */
void race_hud_draw_simple_pause(race_hud *h);
/* the playing track (each frame); its title is shown once per track:
   with the race's display (race_hud_draw) or over the replay
   (race_hud_draw_music) */
void race_hud_music(race_hud *h, const char *title, const char *artist);
void race_hud_draw_music(race_hud *h, float dt);
/* the replay's plate (rtext key `label`, faded by alpha; NULL none) and,
   with logo, the GT4 logo of the modes without the race's display */
void race_hud_draw_replay(race_hud *h, const char *label, float alpha, int logo);
/* the replay's opening: the course's logo, the car's name and its power,
   tyres and mode, `elapsed` seconds into the replay */
typedef struct {
    const char *car;      /* the car's name */
    float power;          /* peak power, PS; 0 or less shows "--" */
    int unit;             /* the power unit option: PS HP BHP CH kW CV PF */
    int front, rear;      /* tyre types 0..12, -1 none */
    const char *mode;     /* the mode's rtext key (ARCADE_TIME_TRIAL ...) */
    int mode_n;           /* a number after it when above 0 */
} race_hud_opening;
void race_hud_draw_opening(race_hud *h, const race_hud_opening *o, float elapsed);
/* The race's timing lines under the lap timer (RaceDisplay +0x17BC): a
   section's time (func_003A0C30, "T1   0'25.955"), a lap's
   (func_003A0A78) and the player's gap (func_003A1098: to the leader, or
   the second's back to the leader; field 1 or a lead in light blue, else
   red) with the icon of the car it is against (-1 none), drawn by `icon` */
void race_hud_section_time(race_hud *h, int section, uint32_t ms);
void race_hud_lap_time(race_hud *h, uint32_t ms);
void race_hud_gap(race_hud *h, int32_t ms, int field, int car);
/* laps behind (func_003A0E40): the player's, or the second's when leading (negative) */
void race_hud_laps_behind(race_hud *h, int laps, int car);
void race_hud_set_icon(race_hud *h, void (*icon)(void *user, int car, float x, float y, float w, float h), void *user);

#endif
