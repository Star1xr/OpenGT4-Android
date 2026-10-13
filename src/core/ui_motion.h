#ifndef OPENGT4_UI_MOTION_H
#define OPENGT4_UI_MOTION_H
#include <stdint.h>

typedef struct {
    float width, height, text_width, speed, font_size;
    float target_x, target_y, x, y;
    int state, wait, fits, half_rate, ready;
    double ticks;
} gt4_tooltip_motion;

/* ToolTipFace controller: CORE 0024A198 / 0024A250 / 0024A2F8.
   Dimensions are logical PS2 pixels. One step is one original update. */
void gt4_tooltip_start(gt4_tooltip_motion *m, float width, float height,
                       float text_width, float font_size, int half_rate);
void gt4_tooltip_step(gt4_tooltip_motion *m);
void gt4_tooltip_advance(gt4_tooltip_motion *m, float seconds);

/* Cursor update block 002303A0..002304C8 (0022FFD0), chase 0057B088.
   Tilt follows only after both positional differences are below one pixel. */
void gt4_cursor_step(float *x, float *y, float *angle,
                     float target_x, float target_y, float target_angle, float speed);

/* func_0022FFD0, gates at 00230384 / 00230394 / 002303A0.
   Pausing or disabling follow retains all three values, including tilt. */
void gt4_cursor_update(float *x,float *y,float *angle,float target_x,float target_y,
                       float target_angle,float speed,int32_t paused,
                       int32_t follow,int32_t active);

/* MMoveActor 002C1BB0 / 002C1F38 and MFadeActor 0020C568. */
float gt4_move_value(float value,float target,float ratio);
float gt4_fade_value(float value,float target,float step);

/* MColorTransition 00287348: completion requires strict crossing. */
int gt4_color_step(float *value,float target,float step);
/* GS alpha parameter conversion at 004A2808, denominator 128. */
uint8_t gt4_gs_alpha(float value);

typedef struct {float x,y,w,h,source_x,source_y,alpha;} gt4_transition_tile;
typedef struct {
    int kind,stage,active,hold; /* stages: 0 outgoing, 1 wait, 2 incoming, 3 done */
    int mode; /* 0 panOut, 1 panIn, 2 panOutIn (0024D328). */
    int type,with_black,columns,rows;
    float value,step,range,slope,magnify_ratio,magnify_diff,black_opacity;
    uint32_t random;uint64_t frame;
    gt4_transition_tile tiles[32*24];
} gt4_transition_motion;
void gt4_transition_init(gt4_transition_motion *m,int kind);
void gt4_transition_start(gt4_transition_motion *m,int operation,float period);
void gt4_transition_step(gt4_transition_motion *m);
void gt4_transition_release(gt4_transition_motion *m);
float gt4_cross_scale(float value);
void gt4_block_setup(gt4_transition_motion *m);
int gt4_block_step(gt4_transition_tile *tile,float step);
/* func_0010AE40: signed duration and opacity occupy consecutive floats. */
int gt4_loading_fade_step(float *period,float *value,float seconds);
/* Pose mode 1: 00153BB8 -> 0057AFF0, fixed 60 Hz native step. */
void gt4_car_turn_step(float *angle,float *velocity);
/* Keep a map-model pivot at its authored resting position during rotation. */
void ui_model_pivot_offset(const float rotation[9],const float rest[9],const float pivot[3],float offset[3]);
#endif
