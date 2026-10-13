#ifndef OPENGT4_ALBUM_SLIDE_H
#define OPENGT4_ALBUM_SLIDE_H
#include <stdint.h>
#define GT4_ALBUM_CAPACITY 400
/* Native counterpart of 001C5E48..001C6580's 0x54-byte slide. */
typedef struct {
 int32_t state,width,height,reserved,format;
 float focal_x,focal_y,crop_x0,crop_y0,crop_x1,crop_y1;
 int32_t play_frames,transition_frames,ticks,position,step,phase,reverse,effect;
 float random1,random2;
} gt4_album_slide;
typedef struct {float x,y,w,h;uint8_t alpha;} gt4_album_rect;
void gt4_album_slide_shuffle(int *order,int count,uint32_t *random_state);
void gt4_album_slide_init(gt4_album_slide *s,int width,int height,int play,int transition,int effect,int reverse,float random1,float random2);
void gt4_album_slide_tick(gt4_album_slide *s);
void gt4_album_slide_out(gt4_album_slide *s);
int gt4_album_slide_rect(const gt4_album_slide *s,const float viewport[4],gt4_album_rect *out);
#endif
