#ifndef OPENGT4_PS2_MEDIA_H
#define OPENGT4_PS2_MEDIA_H
#include "vol.h"
#include "driver_pose.h"
typedef struct {int replays,photos,films,skipped;char error[256];} gt4_media_import;
/* Read-only source, validated before any local writes. Idempotent filenames. */
int gt4_media_import_ps2(gt4_vol *,const char *path,gt4_media_import *);
/* 00334FC8/00335248: section stream, no reinterpretation as an image. */
int gt4_media_film_valid(const uint8_t *,size_t);
/* Owned decoded fields; the source stream remains unchanged. */
typedef struct {
 char code[64],model[128],parts[4][128];int paint;
} gt4_film_car;
typedef struct {
 float camera[17];float parameters[8][91];int cars;
 gt4_driver_skeleton driver[8];uint8_t has_driver[8],has_driver_grasp[8];float driver_grasp[8][2];
 uint8_t smoke[256][33];int smoke_count;
 uint8_t leaves[128][17];int leaf_count;
} gt4_film_frame;
typedef struct {
 char course[64],race[64];gt4_film_car car[8];
 int cars,frames,width,height,portrait,samples,scene;
 float aspect,exposure,lens[6],develop[8];
 uint8_t environment[0x17f4];int has_environment;
 gt4_film_frame frame[2];
} gt4_film;
int gt4_media_film_decode(const uint8_t *,size_t,gt4_film *);
/* 00334B28: film development coefficients, including its filter enum. */
void gt4_media_film_color(const gt4_film *,float out[6]);
#endif
