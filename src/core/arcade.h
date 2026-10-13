/*
 * The Arcade's single race as the menu sets it up: the SpecDB answers the
 * menu scripts ask (course type, the car's arcade info, its drivetrain,
 * paints) and the opponents' choice (MCarData.PickUpEnemyCars,
 * func_001308C8). Tables come from the user's disc.
 */
#ifndef OPENGT4_ARCADE_H
#define OPENGT4_ARCADE_H

#include "vol.h"

#include <stddef.h>
#include <stdint.h>

typedef struct gt4_arcade gt4_arcade;
/* 0012F258 -> 004489A0(5): GENERIC_CAR+0x12 bit 5. */
int gt4_arcade_test_car(gt4_arcade *a,const char *car);

gt4_arcade *gt4_arcade_open(gt4_vol *vol);
void        gt4_arcade_close(gt4_arcade *a);

/* The game's random generator (func_00579330, func_00578598) on its own
   state: a float in [0, 1). */
void  gt4_arcade_seed(gt4_arcade *a, uint32_t seed);
float gt4_arcade_random01(gt4_arcade *a);

/* A car label's key: GENERIC_CAR (table 0), else ENEMY_CARS (0x26 << 32
   | id); ~0 when neither has it (func_00443E00 twice). */
uint64_t gt4_arcade_car_key(gt4_arcade *a, const char *label);

/* MCourseData.GetType (func_001B3030): "dry", "wet", "dirt", "snow",
   "unknown". GetAttrString (func_001B3320): the COURSE row's attribute
   string, "" when there is none or the course is unknown. */
const char *gt4_arcade_course_type(gt4_arcade *a, const char *course);
const char *gt4_arcade_course_attr(gt4_arcade *a, const char *course);
/* 00394ED8 -> 00447F48/00447FD0: zero means no pit package. */
int gt4_arcade_course_pit(gt4_arcade *a,const char *course);

/* MCarData.GetRaceCode (func_0012E150): the race of this car's arcade info
   for the course type (0 dry, else the other set) and difficulty (0..2),
   NULL when it has none. */
const char *gt4_arcade_race_code(gt4_arcade *a, const char *car, int type, int difficulty);
/* MCarData.ShiftCarCode (func_0012F3E0): on a dirt or snow course the
   car's version for it (an ENEMY_CARS label), else NULL (the car stays). */
const char *gt4_arcade_shift_car(gt4_arcade *a, const char *car, const char *course);
/* MCarData.IsTireNormal (func_0012E388), RaceForbidden (func_00130420) */
int gt4_arcade_tire_normal(gt4_arcade *a, const char *car);
/* MCarData.IsDirtRunnable (func_0012E498): a dirt variant is registered.
   The original menu uses the same eligibility check on snow courses. */
int gt4_arcade_dirt_runnable(gt4_arcade *a, const char *car);
int gt4_arcade_race_forbidden(gt4_arcade *a, const char *car);

/* MCarGarage of a car as it comes: getDTrainType (func_0013CC28: "FR",
   "FF", "4WD", "MR", "RR", "--"), isAutomaticOnly (func_0013ED58),
   getColorNum (func_00445828). */
const char *gt4_arcade_drivetrain(gt4_arcade *a, const char *car);
/* Stock catalog parts used by MQuickWork (00445EE8..00445FB8,00445E20). */
int gt4_arcade_catalog_parts(gt4_arcade *a,const char *car,uint8_t engine[128],int *drive);
int gt4_arcade_catalog_geometry(gt4_arcade *a,const char *car,uint16_t out[4]);
int gt4_arcade_automatic_only(gt4_arcade *a, const char *car);
int gt4_arcade_color_num(gt4_arcade *a, const char *car);

/* The cars' ratings (func_00130540: the setup's 0..1 rating, params
   +0xFE0, of the car as it comes): 1 when out[] holds them. */
typedef int (*gt4_arcade_rater)(void *ctx, const uint64_t *keys, int n, float *out);

typedef struct { const char *car; int color; } gt4_arcade_enemy;

/* MCarData.PickUpEnemyCars (func_001308C8): up to count (at most 5) cars
   of list around the player's rating + offset (range: the window's half
   width), the player's own car left out, weakest first; dirt: only cars
   with a dirt version. Returns how many (the entries' car points into
   list), -1 when the ratings failed. */
int gt4_arcade_pick_enemies(gt4_arcade *a, gt4_arcade_rater rate, void *ctx, const char *player,
                            const char *const *list, int n, float offset, float range, int count, int dirt,
                            gt4_arcade_enemy out[5]);

#endif
