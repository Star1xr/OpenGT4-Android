/*
 * A course's camera motions: the Mot1 banks of the course file (header
 * +0xD0 for the standing start, +0xD4 for the rolling one), whose packed
 * motions are camera paths of ten curves (eye, target, up, the field of
 * view in radians) in the frame of a reference (the pole's grid place or
 * the car), as the start camera (CameraPathanim, func_0037D620) plays
 * them through func_003EBCE8 / func_0042B440.
 */
#ifndef OPENGT4_COURSE_MOTION_H
#define OPENGT4_COURSE_MOTION_H

#include <stddef.h>
#include <stdint.h>

typedef struct gt4_motion_bank gt4_motion_bank;

/* data: the Mot1 block (copied what it needs); NULL when not one */
gt4_motion_bank *gt4_motion_bank_open(const uint8_t *data, size_t size);
void gt4_motion_bank_close(gt4_motion_bank *b);
/* the bank's packed motions (0-based) and their names */
int gt4_motion_bank_count(const gt4_motion_bank *b);
const char *gt4_motion_bank_name(const gt4_motion_bank *b, int motion);
/* Motion `motion` at its time t (wrapped as func_0042D430): eye, target
   and up (look[9]) and the field of view (radians). 0 when absent. */
int gt4_motion_bank_camera(const gt4_motion_bank *b, int motion, float t, float look[9], float *fov);

#endif
