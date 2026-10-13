#ifndef GT4_STROBE_H
#define GT4_STROBE_H
#include "tex1.h"
#include <stddef.h>
#include <stdint.h>

/* Bitmap-rectangle subset of GT4's converted Flash (STRB version 0).
   Unsupported vector/scripted content fails instead of rendering substitutes. */
typedef struct {
    float x,y,w,h,u0,v0,u1,v1;
    unsigned bitmap;
    uint8_t color[4];
} gt4_strobe_quad;
typedef struct {unsigned first,count;} gt4_strobe_frame;
typedef struct {
    float width,height,rate;
    unsigned nframes,nbitmaps,nquads,loop_frames;
    tex1_image *bitmaps;
    gt4_strobe_frame *frames;
    gt4_strobe_quad *quads;
} gt4_strobe;
int gt4_strobe_decode(const uint8_t *data,size_t size,gt4_strobe *out);
void gt4_strobe_free(gt4_strobe *s);
unsigned gt4_strobe_frame_at(const gt4_strobe *s,double seconds);
#endif
