/*
 * The leaves the wheels throw up where the course has them (the world's
 * +0x3238 object, its LeafParticle, vtable D_006863A0): triangles of the
 * course's collision marked 0x20 (Autumn Ring and its Mini, and their
 * reverse layouts, on this disc) make each wheel that slips over 2 m/s
 * throw a leaf (func_003C2160); each flies as a flat plate with drag and
 * spin (func_0040AFB0) until it falls through the ground, drawn with one
 * of the course's eight leaf pictures (header +0x9C, images 1..8),
 * lit by the course's light (func_0040B3C8, func_0040B4E0).
 * The world's sparks (SparkParticle, func_00390980 -> 003C24A8) are off
 * in the game: D_00621B50 +0xA8 is set to 0 (func_003C2B20) and nothing
 * sets it again.
 */
#ifndef OPENGT4_RACE_LEAVES_H
#define OPENGT4_RACE_LEAVES_H

#include <stddef.h>
#include <stdint.h>

typedef struct race_leaves race_leaves;

/* what the physics hands over for a wheel on a leaf triangle
   (func_003667A8's 0x3C bytes, and the car's velocity) */
typedef struct {
    float pos[3];        /* +0x08: the contact point, render frame */
    float heading;       /* +0x14 */
    float shade;         /* +0x18: the ground's light under it */
    float plane[4];      /* +0x1C: the ground's plane (normal, d) */
    float spin, ground;  /* +0x34 (wheel +0x28), +0x38 (wheel +0x20) */
    float vel[3];        /* the car's velocity, render frame */
} race_leaves_wheel;

/* a leaf to draw: its corners in view space (x right, y up, z towards
   the viewer), in the GS's strip order, their texture places, its colour
   (0..1 of the GS's 128) and which of the eight pictures (0..7) */
typedef struct {
    float xyz[4][3], uv[4][2];
    float rgb[3];
    int picture;
} race_leaves_quad;

/* env0: the course's Env0 block (its light), borrowed */
race_leaves *race_leaves_open(const uint8_t *env0, size_t size);
void race_leaves_close(race_leaves *l);
int race_leaves_snapshot(race_leaves *,const uint8_t records[][17],int count);
void race_leaves_emit(race_leaves *l, const race_leaves_wheel *w);
void race_leaves_update(race_leaves *l, float dt);
int race_leaves_live(const race_leaves *l);
/* tests: how high over their ground the leaves are, lowest and highest */
int race_leaves_height(const race_leaves *l, float *lo, float *hi);
int race_leaves_quads(const race_leaves *l, const float eye[3], const float rows[9], race_leaves_quad *out, int max);

#endif
