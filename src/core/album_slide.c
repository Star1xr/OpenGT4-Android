#include "album_slide.h"
#include <string.h>
#include "pcm_music.h"
static float clamp(float x,float a,float b){return x<a?a:x>b?b:x;}
void gt4_album_slide_shuffle(int *order,int count,uint32_t *random_state)
{
 /* 001C6BE8 +C0..108 swaps each position with a draw across the full
    list, rather than shrinking the draw range. Capacity +58 is 400. */
 for(int i=0;i<count;i++){int j=(int)((float)count*gt4_pcm_music_random_unit(random_state))%count;
  int t=order[i];order[i]=order[j];order[j]=t;}
}
void gt4_album_slide_init(gt4_album_slide *s,int width,int height,int play,int transition,int effect,int reverse,float random1,float random2)
{
 /* 001AAFD0 +94..C4 clamps play to [3,60] seconds. 001C5E88
    +10/+24 converts seconds to frames at 60; +88..C0 resets counters. */
 memset(s,0,sizeof *s);s->width=width;s->height=height;s->format=3;
 /* P: native PNGs have no PS2 photo crop metadata. Whole exported image. */
 s->crop_x1=s->crop_y1=1;
 s->play_frames=(play<3?3:play>60?60:play)*60;
 s->transition_frames=transition*60;s->step=1;s->effect=effect;s->reverse=reverse;
 s->random1=random1;s->random2=random2;
}
void gt4_album_slide_tick(gt4_album_slide *s)
{
 /* 001C5F80 +20..84 and switch table 00693D50: state 0 fade in,
    1 present, 2 waiting for next image, 3 fade out, 4 retire, 5 free. */
 if(s->phase<0)s->phase=0;
 s->position+=s->step;if(s->position<0)s->position=0;s->ticks++;
 int total=s->play_frames+2*s->transition_frames;if(s->position>total)s->position=total;
 int next=s->state;
 switch(s->state){
 case 0:if(++s->phase>=s->transition_frames)next=1;break;
 case 1:if(s->ticks>=s->play_frames)next=2;break;
 case 3:if(++s->phase>=s->transition_frames)next=4;break;
 case 4:if(++s->phase>=4)next=5;break;
 }
 if(next!=s->state){s->state=next;s->phase=0;}
}
void gt4_album_slide_out(gt4_album_slide *s)
{ /* 001C60B0 +0..20 */ if(s->state>=0 && s->state<3){s->state=3;s->phase=0;} }
int gt4_album_slide_rect(const gt4_album_slide *s,const float v[4],gt4_album_rect *o)
{
 /* 001C6160 +58..C0: aspect fit; +118..1BC crop zoom; +1C8..2B8
    constrain focal point; 001C60D8 interpolation. No host easing. */
 if(s->width<=0 || s->height<=0)return 0;
 float w=(float)s->width,h=(float)s->height,hw=v[2]*.5f,hh=v[3]*.5f;
 float fit=v[2]/w;if(v[3]/h<fit)fit=v[3]/h;
 float zoom=fit,fx=w*.5f,fy=h*.5f;
 if(s->format==3){
  fx=w*(s->focal_x+1)*.5f;fy=h*(1-s->focal_y)*.5f;
  float span=(v[3]/h<v[2]/w?s->crop_y1-s->crop_y0:s->crop_x1-s->crop_x0)*.5f;
  span=clamp(span,.5f,1);span=span+(1-span)*(s->random1*s->random1)-s->random2*.19999998807907104f;
  span=clamp(span,.5f,1);zoom=fit/span;
 }
 if(fx*zoom<hw)fx=hw/zoom;
 if((w-fx)*zoom<hw){fx=w-hw/zoom;if(fx*zoom<hw)fx=w*.5f;}
 if(fy*zoom<hh)fy=hh/zoom;
 if((h-fy)*zoom<hh){fy=h-hh/zoom;if(fy*zoom<hh)fy=h*.5f;}
 if(!s->effect){fx=w*.5f;fy=h*.5f;zoom=fit;}
 float t=clamp((float)s->position/(float)(s->play_frames+2*s->transition_frames),0,1);
 if(s->reverse)t=1-t;
 float ratio=(zoom/fit-1)*t+1;if(ratio<1)ratio=1;
 float scale=zoom/ratio;fx=(w*.5f-fx)*t+fx;fy=(h*.5f-fy)*t+fy;
 o->x=v[0]+hw-fx*scale;o->y=v[1]+hh-fy*scale;o->w=w*scale;o->h=h*scale;
 int alpha=255;
 if(s->state==0)alpha=s->transition_frames?(int)((float)s->phase/(float)s->transition_frames*255):255;
 else if(s->state==3)alpha=s->transition_frames?(int)((1-(float)s->phase/(float)s->transition_frames)*255):0;
 else if(s->state>=4)alpha=0;
 o->alpha=(uint8_t)(alpha<0?0:alpha>255?255:alpha);
 return s->state<4;
}
