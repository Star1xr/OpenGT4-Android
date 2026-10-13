#ifndef OPENGT4_PHYS_RACE_SETUP_H
#define OPENGT4_PHYS_RACE_SETUP_H
#include "ee.h"
#include "vol.h"

/* Transitional: course (NULL: keep) and car 0 (GENERIC_CAR id, -1: keep) of
   a snapshot's race replaced, the race set up again. 1 on success; msg
   says why not, or notes how the car was placed. */
int phys_race_setup(ee_ctx *c, gt4_vol *vol, uint32_t conductor, const char *course,
                    int32_t car_id, char *msg, size_t len);

/* The other entries' cars (GENERIC_CAR ids; -1: keep), entries 1.. of
   the race: their garage entries are made from the disc too */
extern int32_t phys_race_other_cars[7];

/* 1: run the race init to its end (a race built without a snapshot needs
   it); 0: stop after the cars, keeping the snapshot's own */
extern int phys_race_full_init;
#endif
