#ifndef OPENGT4_CAR_SERVICE_H
#define OPENGT4_CAR_SERVICE_H
#include <stddef.h>
#include <stdint.h>
/* Additional models and independent motion controllers, 00153FC8/001540C8.
   Assets are borrowed from the project's cache; arrays belong to the widget. */
typedef struct {const uint8_t *data;size_t size;uint64_t serial;} gt4_service_model;
typedef struct {
    const struct gt4_mot1 *motion;
    float time,yaw;
    int model,alignment,hidden,playing,loop;
} gt4_service_motion;
typedef struct {float offset[3],yaw,scale[3];int draw;} gt4_service_placement;
/* 00156AA0 +0x14c..0x3d4: local draw parameters, before the VU matrix
   operations. Pass 1 is the floor reflection, pass 0 the regular model. */
gt4_service_placement gt4_service_place(const float geometry[8],int alignment,
                                      float yaw,float scale,int hidden,int reflection);
/* 0021B4F8 +0x18..0x20 / 0021B528 +0x00..0x50: normal controller with
   no queued replacement motion. Normalize once after the frame update. */
void gt4_service_clock(float *time,int *playing,int has_motion,float dt,float end,float loop_start,int loop);
/* 0021B4F8 advances only. 00156F18 normalizes the live camera controller;
   00273AC8 +0xa4 normalizes a COPY for each additional-model draw. */
void gt4_service_advance(float *time,int playing,int has_motion,float dt);
float gt4_service_sample(float time,int has_motion,float end,float loop_start,int loop);
/* 00153BB8 +0xd8..0x19c: motion=2 uses linear target easing, without
   shortest-angle wrapping. 00151960 supplies the gains and fade increment. */
void gt4_service_pose(float position[3],float angles[3],const float target_position[3],const float target_angles[3]);
float gt4_service_fade(float opacity);
/* 00364B30 +0x68..0x70: axle's upper suspension limit minus centre.
   The service's wheel-drop clamp is NOT the tire's radius. */
float gt4_service_drop_limit(const float axle[5]);
/* 004A2808 +0x00..0x4c: GS FIX coefficient, clamped and truncated. */
unsigned gt4_service_blend_fix(float opacity);
/* 001060F8 +0xd0..0x13c selects the video-mode correction;
   00107328 +0x04..0x30 applies it after the CarFace projection. */
void gt4_service_projection_scale(int video_mode,int widescreen,float scale[2]);
/* 00216F74..88 / 004A2C58 / CORE 00629DE0..F4: one reflected sample
   and two regular samples, quantized to GS XYOFFSET's sixteenths. */
void gt4_service_jitter(int samples,int index,float offset[2]);
/* 00156950 +0x50..0xa4: body lift and local wheel displacement. The body
   translation encloses the wheel draws (00451AB0 -> 004595C8/00459AD8);
   00459C10 adds each displacement to its hub, before that translation. */
void gt4_service_lift(float lift,const float drop_limit[4],float *body,float wheel[4]);
/* 00153CAC..00153D1C: position feedback, CarModel+58 per frame. */
void gt4_service_position(float position[3],const float target[3],float ratio);

#endif
