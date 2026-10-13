#ifndef GT4_LOGGER_VIEW_H
#define GT4_LOGGER_VIEW_H
#include "logger.h"
#include <stdint.h>

/* The Logger screen's control (mLoggerControl, 0011EA90 constructor):
   two analysed tracks and the graph's window. Field comments give the
   control's offsets. */
typedef struct {
    const gt4_logger_point *points[2];
    uint32_t count[2];       /* data +0x40000/+0x40004, cut to the shorter */
    float spacing;           /* data +0x40008 */
    int present[2];          /* the replay entry exists (entry +0x1D0 == 0) */
    uint32_t total_time[2];  /* +0x290 + i*8: the entry's time at its last lap */
    int metric;              /* +0x2B8: D_006244D8 == 0 */
    float grid, speed_scale; /* +0x2BC 25 m (25.146 m), +0x2C0 1 (1.609344) */
    int wrap_left, wrap_right; /* +0x2C4, +0x2C8 */
    float center, scroll;    /* +0x2CC, +0x2D0 */
    float left, top, right, bottom; /* +0x2D4..+0x2E0 */
    float zoom, vzoom;       /* +0x2E4, +0x2E8 */
    float width, height, top_edge; /* +0x2EC, +0x2F0, +0x2F4 */
    float start, end;        /* +0x2F8, +0x2FC */
    float length;            /* +0x300 */
    float speed_top;         /* +0x308 */
    int speed_top_i;         /* +0x30C */
    int analyzed;            /* +0x2A8 */
    int fit;                 /* 00122F1C's window still to fit to the widget's width */
    /* GameData -0x5CA8..-0x5CA4 (00123640..001236A0) */
    int show_track[2], show_speed, show_accel, show_brake;
} gt4_logger_view;

void gt4_logger_view_init(gt4_logger_view *v, int metric);
/* 00122FA8 +0x240..0x308 and 00122D38 +0x1E4..: both tracks cut to the
   shorter, the window over the whole length. b may equal a (no second
   entry: present[1] 0). */
void gt4_logger_view_analyze(gt4_logger_view *v, const gt4_logger_sampler *a, const gt4_logger_sampler *b,
                             int present_b, uint32_t time_a, uint32_t time_b);
/* 001206A0 +0x40..0x9B0: the sticks (left x/y, right x/y, -1..1). */
void gt4_logger_view_input(gt4_logger_view *v, float lx, float ly, float rx, float ry);
/* the course map's span and point (001206A0 +0x598..: start, end, point) */
void gt4_logger_view_span(const gt4_logger_view *v, float *start, float *end, float *point);
/* speed (km/h) of track i at the cursor (0011DBE8 * 3.6) */
float gt4_logger_view_speed(const gt4_logger_view *v, int i);

/* Drawing at the widget (x, y, w, h) of the 640x480 screen. */
typedef struct {
    void *user;
    void (*line)(void *user, float x0, float y0, float x1, float y1, float width, const uint8_t rgba[4]);
    /* corners top-left, top-right, bottom-left, bottom-right */
    void (*quad)(void *user, const float xy[4][2], const uint8_t rgba[4][4]);
    /* face, top-left (x, y), scale; right>0 aligns right in [x, x+right] */
    void (*text)(void *user, const char *face, float x, float y, float scale, float right, const uint8_t rgba[4], const char *s);
    float (*text_height)(void *user, const char *face, float scale);
} gt4_logger_draw_ops;
/* 00120CF0: the graph's layers. */
void gt4_logger_view_draw(gt4_logger_view *v, float x, float y, float w, float h, float alpha, const gt4_logger_draw_ops *ops);
#endif
