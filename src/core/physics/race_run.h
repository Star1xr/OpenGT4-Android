/*
 * A race run natively: the game's executable image from the user's disc
 * (for its data tables), the race's objects made as the menu would, the
 * course, the cars and the race set up by the rewritten code, then stepped
 * frame by frame. Nothing goes to original code. One race at a time (the
 * physics' memory is global, gt4_phys_mem).
 */
#ifndef OPENGT4_RACE_RUN_H
#define OPENGT4_RACE_RUN_H

#include <stddef.h>
#include <stdint.h>
#include "course_gadget.h"

typedef struct {
    char course[32];      /* a /crs name, e.g. "tsukuba" */
    int cars;             /* 1..6, the player's first */
    uint32_t car[6];      /* GENERIC_CAR ids */
    /* Optional serialized owned-car body (0x4b8 bytes, class header removed).
       Consumed synchronously by open; NULL constructs the stock setup. */
    const uint8_t *owned_car[6];
    /* Decoded 151-byte RACE row; NULL uses the Arcade path. A career
       caller must supply the original grid and owned player entry. */
    const uint8_t *event_row;
    int championship;     /* event world: 003EF498 mode 14 instead of 13 */
    char license_code[32]; /* original RACE label for result title (003BCB38) */
    int license;          /* 1: 003BBF90 RaceLicense; 2: 003ED298 RaceTraining */
    uint64_t tuned_car[6]; /* optional ENEMY_CARS key (namespace 0x26) */
    uint64_t event_paint[6]; /* RACES second qword: one-based, 0/-1 random */
    /* State after opponent selection; consumed synchronously and advanced
       by 0040AA48. Required for a prepared event grid. */
    uint32_t *event_rng;
    int laps;             /* 0: two (the option race_laps' default) */
    int tire;             /* every car's tyres (the Arcade's): 1 + the
                             compound level of its parts list (the driving
                             options' index), 0 the car's own */
    int manual;           /* the player's gearbox: 0 automatic, 1 manual */
    int assist_asm;       /* the player's aids (game.assist_asm, */
    int assist_tcs;       /* game.assist_tcs): both off in the captures */
    /* RaceSplitBattle: two human entries (0032F028, 003470B0). */
    int split_battle, tire_2p, manual_2p, assist_asm_2p, assist_tcs_2p;
    int split_boost, split_handicap, split_tire_damage;
    int time_trial;       /* the Arcade's Time Trial (00112620): its own
                             session and world, one car, no last lap */
    int world_mode;       /* the world's mode (+0xD68, the names at 0069FE58)
                             when not the path's own: 11 GTMODE_FREE_RUN,
                             12 GTMODE_FREE_PRACTICE; 0 keeps it */
    int bspec;            /* B-Spec (the quick menu's COMMAND_SPECTATOR): the
                             world's +0xCC0..+0xCCC all 1, so 003F1760 gives
                             the player's car to its driver (kind 1) */
    int pit_menu;         /* a host with the pit menu (world+0xE408, the menu
                             003DCA28 makes): the watched car's stop waits
                             for its choice (0035AF18 asks 0038B980); without
                             one the crew chooses */
    /* The Arcade's QuickTune (quick work +0x3AC..+0x3BC), written into the
       player's entry by 00111DC0 when IsQuickTuned (+0x488): power and
       weight as percentages over 100, the tyre kinds by axle, the gearbox's
       top speed (setup +0x12A, 0 keeps the ratios) */
    int quick_tuned;
    int quick_power, quick_weight, quick_front_tire, quick_rear_tire, quick_gear;
} gt4_race_desc;

typedef struct gt4_race_run gt4_race_run;
const gt4_course_gadgets *gt4_race_run_objects(const gt4_race_run *run);

/* The Time Trial's ghost: car 1 drives the best lap again while this is 1
   (the race records the player's commands itself; func_003F5B80) */
#define GT4_GHOST_CAR 0x10a0
int gt4_race_run_ghost_on(const gt4_race_run *r);
/* The ghost's best lap (the best-lap replay a Time Trial saves): the
   player's car at the lap's start, the start event's remaining thirds and
   step fraction, the lap's time (the HUD's best lap's unit) and its
   commands, lent; 0 when there is none yet */
int gt4_race_run_ghost_get(const gt4_race_run *r, const uint8_t **car, uint32_t *rest, float *frac,
                           uint32_t *lap_time, const uint8_t **tape, size_t *steps);
/* a saved best lap as the ghost to beat (copied): the ghost drives it from
   the next lap the player starts, until a better one replaces it */
int gt4_race_run_ghost_set(gt4_race_run *r, const uint8_t car[GT4_GHOST_CAR], uint32_t rest, float frac,
                           uint32_t lap_time, const uint8_t *tape, size_t steps);

/* disc: the user's disc image. NULL on failure, with msg saying why. */
gt4_race_run *gt4_race_run_open(const char *disc, const gt4_race_desc *d, char *msg, size_t len);
void gt4_race_run_close(gt4_race_run *r);

/* One frame (1/60 s): the race's countdown, the step, its events. The
   player's controller comes through gt4_phys_pad. 0 when it failed. */
int gt4_race_run_frame(gt4_race_run *r);

/* Car i for drawing, in the render frame (x, y up, z; the physics' x, z,
   -y): the model's origin on the ground (pos), its rotation (row-major,
   world = rot * model, the model facing -z), and per wheel slot (front,
   rear; the model's +x side first) its steer, rotation and hub lift
   against the start's. 0 when there is no such car. */
int gt4_race_run_pose(const gt4_race_run *r, int i, float pos[3], float rot[9], float wheels[4][3]);

/* The conductor (the race session) and car i, as guest addresses of the
   physics' memory (car.h has the layout). */
uint32_t gt4_race_run_conductor(const gt4_race_run *r);
/* 1 once the player's car finished (car+0x5B6, the model drives it on) */
int gt4_race_run_finished(const gt4_race_run *r);
/* 003F8AB0/003F8980: -2 while running, -1 failed, 2/3/4 medals;
   +0x8c is the evaluated attempt time. Does not grant a licence. */
int gt4_race_run_license_result(const gt4_race_run *r,int32_t *state,uint32_t *time);
uint32_t gt4_race_run_car(const gt4_race_run *r, int i);
/* the course's line at distance d (func_0034C518), in the render frame */
int gt4_race_run_course_point(const gt4_race_run *r, float d, float out[3]);
/* the replay cameras' checks, in the render frame: the ground triangle
   under p (func_003FCB20, -1 none); the wall check from -> to
   (func_003FE7B0, 1 at a wall; *id the ground triangle carried along);
   p's distance along the course (func_003955E8) */
int gt4_race_run_ground_id(const gt4_race_run *r, const float p[3]);
int gt4_race_run_camera_wall(const gt4_race_run *r, float margin, const float from[3], const float to[3], float out[3], int *id);
float gt4_race_run_course_distance(const gt4_race_run *r, const float p[3]);
int      gt4_race_run_cars(const gt4_race_run *r);
/* car i's mass as its race took it (car +CAR_MASS_A, kg) */
float    gt4_race_run_mass(const gt4_race_run *r, int i);
/* Tests: the guest memory replaced by a PCSX2 capture's (eeMemory.bin,
   32 MB), for a frozen race at the captured moment. 0 if unreadable. */
int      gt4_race_run_restore(gt4_race_run *r, const char *path);

/* What the race display reads of car i each frame (the RaceDisplay's
   update, func_0039DBB8 and func_0039ECD0). Times are in ms, 0x157529FF
   none. */
typedef struct {
    int rank;              /* car+0x5B3, 1-based */
    int lap;               /* func_00343B50: the lap the car is on */
    int laps;              /* the session's 0x128: the race's laps, 0 none */
    uint32_t lap_time;     /* func_00357800: this lap so far */
    uint32_t best_lap;     /* func_0035E3F0 (car+0x71C) */
    uint32_t last_lap;     /* its entry's +0x11DC */
    float speed;           /* m/s (car+0x6C8) */
    float rpm;             /* func_00350F30 (car+0x738) */
    int gear;              /* car+0x624 */
    float throttle, brake; /* func_00344698, func_003446A0 */
    float side_g;          /* -func_003451B0 * 0.102 (car+0x664) */
    int shift_lamp;        /* func_003F3758 */
    int finished;          /* car+0x5B6 */
    float fuel, fuel_cap;  /* car+0x5D4, +0x5D0 (func_00354E60, func_00354E58); no tank at 0 */
    float rev_limit;       /* the tachometer's top: params+0x1074 * 100 (func_0036CDF8) */
    int aid;               /* the aids' sign: max(car+0x6FC, +0x6FD) */
    int brake_lock;        /* the brake sign: car+0x594 above 0.25 (func_00344658) */
    int driving;           /* the player drives it (func_003451D8) */
    int start_frames;      /* frames left to the start (race+0xBC; event 4 at 0) */
    int count_frames;      /* func_0034C1C8: includes the watched car's start delay */
    int start_seconds;     /* race+0xB8: the initial countdown duration */
    int grid_slot;         /* entry+0x3444: CameraPathanim mirrors odd slots */
    float intro_elapsed;   /* simulation seconds since the start presentation */
    int aspec_points;      /* func_003C0FF8: the points offered for this grid */
    int speed_steps;       /* func_0036CDF8: (int)(params+0xFD8 * 1.1 / 22.2222) + 1, 2..4 */
    float tacho_scale;     /* params+0x1075 * 1000 */
    uint32_t odometer;     /* car+0x6DC */
    int rolling;           /* cond+0xCBE4: the race starts rolling */
    int signal;            /* car+0x6FE: the car's signal (func_0035ACD8), 4 FULL
                              THROTTLE, 5 GET READY TO DRIVE, 6 GO!, ... */
    uint32_t line_time;    /* car+0x718: the race's clock (ms) when the car last
                              crossed the line; at its finish its race time */
} gt4_race_hud;
int gt4_race_run_hud(const gt4_race_run *r, int i, gt4_race_hud *out);
/* func_0034C1C8: car i's countdown, frames (race+0xBC + cond+0xCBFC+i*4) */
int gt4_race_run_countdown(const gt4_race_run *r, int i);
/* Car i on the mini map: the render frame's x and z (func_00344798,
   func_00344828); returns how it shows (func_00366678: 3 racing, 2 faded,
   0 or 1 not drawn; 0 too when there is no such car). */
int gt4_race_run_xy(const gt4_race_run *r, int i, float *x, float *y);
/* Car i's place as the cameras take it: the drawn reference point
   (func_00344798, func_003447F8, func_00344828) and heading
   (func_003448B8), in the render frame */
int gt4_race_run_place(const gt4_race_run *r, int i, float p[3], float *yaw);
/* Where car i's shadow lies (func_00451D18 through the race pose,
   D_00682F98 +0x40 and +0x38): the drawn reference point (+0x894) and a
   frame of the heading and the ground's pitch and roll under the car
   (+0x674, +0x678, +0x67C), world = rot * p + pos in the render frame.
   sun and level are the ground's two values under the wheels (the ground
   blocks' +0x14 and +0x18), the mean of the four (func_003D7460). */
int gt4_race_run_shadow(const gt4_race_run *r, int i, float pos[3], float rot[9], float *sun, float *level);
/* A camera on car i (CameraOnBoard/RideOn, func_0037B020) of type 0 (on
   the car: the bumper view) or 1 (behind it): point is where the car sits
   in the view (its model's camera point of that index, y up, looking down
   -z); the view turned by the car's heading and pitch, for type 0 with
   its roll and the view's bob, for 1 less the view's lag (car+0x700/
   0x704, the body springs) and no roll. The camera's own shake (type 0,
   its springs at +0x1D0..0x224) is not taken yet. Out: the eye in the
   render frame and the world-to-view rows (right, up, back). Other types
   (func_0037B580: 6 the roof view) take the car's turns as they are and
   the point's own turns (+0xC, +0x10, +0x14, degrees). */
int gt4_race_run_camera(const gt4_race_run *r, int i, int type, const float point[8], float eye[3], float rows[9]);

/* The setup's 0..1 rating of cars as they come (func_00130540, what the
   Arcade picks its opponents by): a garage entry from the SpecDB, its
   record and parameters, params+0xFE0. The executable's data is loaded
   from the disc on the first miss and let go after each call; the ratings
   are kept. Not while a race runs (the physics' memory is global). */
typedef struct gt4_car_ratings gt4_car_ratings;
gt4_car_ratings *gt4_car_ratings_open(const char *disc);
void gt4_car_ratings_close(gt4_car_ratings *r);
/* keys: GENERIC_CAR ids (or table << 32 | id); a car the SpecDB cannot
   make rates 0. 0 when the disc could not be read. */
int gt4_car_ratings_get(gt4_car_ratings *r, const uint64_t *keys, int n, float *out);
/* What the quick menu's grid shows of a car as it comes: the engine
   summary's peak power (PS, truncated) and its weight (kg). Checked
   against the grids of two Arcade captures (twelve cars). */
int gt4_car_ratings_spec(gt4_car_ratings *r, uint64_t key, int *power, int *weight);
/* A car as a race has it (gt4_race_run_setup): its peak power (PS, as a
   float; -1 when the car's spec hides it) and its front and rear tyres'
   types (0..12, -1 none), as the replay's opening shows them */
int gt4_car_ratings_setup(gt4_car_ratings *r, const uint8_t setup[0x178], float *power, int *front, int *rear);
/* a car's setup as the race took it (its garage entry, 0x178 bytes) */
int gt4_race_run_setup(const gt4_race_run *r, int i, uint8_t out[0x178]);
/* the car's paint chip (func_00445AE8): four colours, each 0xRRGGBB with
   its flags in the top byte (bit 24 a colour, 25..27 the chip's style) */
int gt4_race_run_paint_chip(const gt4_race_run *r, int i, uint32_t chip[4]);
/* 00111B50 +0x1F0..0x234: the QuickTune's gearbox range of car i, from its
   TRANSMISSION row (+0x25 adjustable, +0x26/+0x27 the limits) and setup
   +0x12A; all 0 when the row is not adjustable. */
int gt4_race_run_gear_range(const gt4_race_run *r, int i, int *gear, int *lo, int *hi);

#endif
