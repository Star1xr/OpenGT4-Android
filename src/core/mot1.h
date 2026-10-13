/*
 * GT4 "Mot1" motion files, the keyframe form used by /menu/select/light.mot:
 * one timeline whose named parameters are constants or cubic Bezier curves.
 * Layout and evidence: docs/SHOWROOM_SCENE.md. camera.mot uses the same
 * records (look-at and field of view).
 */
#ifndef OPENGT4_MOT1_H
#define OPENGT4_MOT1_H

#include <stddef.h>
#include <stdint.h>

typedef struct gt4_mot1 gt4_mot1;

/* Copies what it needs; NULL if the data is not a supported Mot1. */
gt4_mot1 *gt4_mot1_open(const uint8_t *data, size_t size);
void      gt4_mot1_close(gt4_mot1 *m);

/* The timeline: its end, and where playing past the end loops back to. */
float gt4_mot1_end(const gt4_mot1 *m);
float gt4_mot1_loop(const gt4_mot1 *m);
/* Time wrapped as the game does (CORE 0x0042D430): negative -> 0; past the
   end -> loop + (t - loop) mod (end - loop). */
float gt4_mot1_wrap(const gt4_mot1 *m, float t);

/* A named scalar parameter (e.g. "Alpha0", "ColorScale") at time t.
   Returns 0 if there is no such parameter. */
int gt4_mot1_value(const gt4_mot1 *m, const char *name, float t, float *out);

/* Light record kind (0x0C, 0x0D, 0x0E colours with 4 components; 0x0F
   position with 3) of light index `light` at time t. Returns the number of
   components written to out (0 if absent). */
int gt4_mot1_light(const gt4_mot1 *m, int kind, int light, float t, float out[4]);

/* Camera records at time t: look = eye xyz, target xyz, up xyz; fov is the
   vertical field of view in radians (the game converts it to degrees,
   0x0042D858, for its projection 0x0049AC00). Returns 0 if either is absent. */
int gt4_mot1_camera(const gt4_mot1 *m, float t, float look[9], float *fov);

/* Local translation/Euler channels preceding the named drawing marker,
   radians. These are local channels, not accumulated skeletal matrices. */
int gt4_mot1_transform(const gt4_mot1 *m,const char *target,float t,float position[3],float euler[3]);
/* Accumulated column-major frames at named drawing markers. Sine/cosine
   is an explicit boundary so the caller can use the original native math.
   Returns the number of matched names, or -1 for an invalid matrix stack. */
int gt4_mot1_frames(const gt4_mot1 *m,float t,const char *const *names,int count,
                    float (*out)[16],void (*sincos_fn)(float,float *,float *));
#endif
