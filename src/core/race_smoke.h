/*
 * The tyres' smoke (the world's +0x31D0 object): a pool of 256 particles
 * emitted at each wheel's contact point (func_003E75E8), moved and faded
 * each frame (func_003E7A58) and drawn as turned squares facing the view
 * (func_003E7F08 -> 003E8610/003E8438). How each kind of surface smokes
 * comes from the course file's "Env0" block (course header +0x80): six
 * classes of 0x44 bytes at +0x4D0, the surface to class table D_006A3A00.
 */
#ifndef OPENGT4_RACE_SMOKE_H
#define OPENGT4_RACE_SMOKE_H

#include <stddef.h>
#include <stdint.h>

typedef struct race_smoke race_smoke;

/* what the physics hands over for a wheel (func_003667A8's 0x3C bytes) */
typedef struct {
    int wheel, surface;
    float pos[3];      /* the contact point, render frame */
    float shade;       /* +0x18: the ground's light under it */
    float plane[4];    /* the ground's plane (normal, d) */
    float amount;      /* +0x2C: how much it smokes */
} race_smoke_wheel;

/* a square to draw: its corners in view space (x right, y up, z towards
   the viewer: in front is negative), their texture places, the colour
   (0..1, alpha with the GS's 128 = 1 already applied) */
typedef struct {
    float xyz[4][3], uv[4][2];
    float rgba[4];
} race_smoke_quad;

/* env0: the course file's Env0 block (borrowed, at least 0x920 bytes) */
race_smoke *race_smoke_open(const uint8_t *env0, size_t size);
void race_smoke_close(race_smoke *s);
void race_smoke_clear(race_smoke *s);
void race_smoke_emit(race_smoke *s, const race_smoke_wheel *w);
void race_smoke_update(race_smoke *s, float dt);
/* the live particles as squares for a camera at eye with the world to
   view rows (right, up, back), sorted far to near (func_003E9018) when the
   course blends them (Env0 +0x918 0x44); returns how many */
int race_smoke_quads(const race_smoke *s, const float eye[3], const float rows[9], race_smoke_quad *out, int max);
/* Env0 +0x918: 0x44 (Cs - Cd) * As + Cd, 0x48 Cs * As + Cd */
int race_smoke_blend(const race_smoke *s);
int race_smoke_live(const race_smoke *s);
/* Film SMOK records, as read by 003E96B8; replace without emission/ticks. */
int race_smoke_snapshot(race_smoke *s,const uint8_t (*records)[33],int count);

#endif
