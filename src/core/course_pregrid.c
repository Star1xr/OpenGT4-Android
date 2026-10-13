#include "course_pregrid.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <fenv.h>
struct gt4_pregrid { const uint8_t *data, *entries; size_t size; unsigned count, shot, frame; float lens[9], roll[9]; int first; };
static uint32_t u32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static float f32(const uint8_t *p){uint32_t u=u32(p);float f;memcpy(&f,&u,4);return f;}
static float bits(uint32_t u){float f;memcpy(&f,&u,4);return f;}
/* 003DDBC8 +0x14..+0xA0: upper bound, then linear interpolation;
   the final segment extrapolates (the authored knots often stop at .99). */
float gt4_pregrid_curve(const uint8_t *p,float t)
{
    unsigned n=u32(p),lo=0,hi=n;
    while(lo<hi){unsigned m=(lo+hi)/2;if(t<f32(p+4+m*8))hi=m;else lo=m+1;}
    unsigned i=lo?lo-1:0;if(i>=n-1)i=n-2;
    float a=f32(p+4+i*8),b=f32(p+12+i*8),x=f32(p+8+i*8),y=f32(p+16+i*8);
    return ((y-x)*(t-a))/(b-a)+x;
}
/* 003FC9A0 +0x00..+0x5C: completion strictly after, not at, 1. */
float gt4_pregrid_time(float seconds,int frame,int *done)
{float t=(float)frame/(seconds*60.f);*done=t<0 || t>1;return fmaxf(0,fminf(1,t));}
/* 003794A8 +0x28..+0x98: vertical FOV, original degree/aspect bits.
   libm tan replaces the PS2 library boundary 0057D380; compare with tolerance. */
void gt4_pregrid_projection(float degrees,float *sx,float *sy)
{float a=(degrees*.5f)*bits(0x3c8efa34);float cot=1.f/tanf(a);*sx=cot*bits(0x3f3340cd);*sy=cot;}
static int valid_curve(const gt4_pregrid *p,uint32_t at)
{
    if(!at)return 1;
    if(at>p->size || p->size-at<4)return 0;
    unsigned n=u32(p->data+at);if(n<2 || n>(p->size-at-4)/8)return 0;
    float last=-INFINITY;
    for(unsigned i=0;i<n;i++){float t=f32(p->data+at+4+i*8),v=f32(p->data+at+8+i*8);if(!isfinite(t) || !isfinite(v) || t<=last)return 0;last=t;}
    return f32(p->data+at+4)==0;
}
gt4_pregrid *gt4_pregrid_open(const uint8_t *course,size_t size)
{
    if(!course || size<0xc4)return NULL;
    uint32_t at=u32(course+0xc0);if(!at || at>size || size-at<0x28 || memcmp(course+at,"REP4",4))return NULL;
    gt4_pregrid *p=calloc(1,sizeof *p);if(!p)return NULL;
    p->data=course+at;p->size=size-at;p->count=u32(p->data+0x20);
    uint32_t list=u32(p->data+0x24);
    if(!p->count || list>p->size || p->count>(p->size-list)/0x7c)goto invalid;
    p->entries=p->data+list;
    for(unsigned i=0;i<p->count;i++){
        const uint8_t *e=p->entries+i*0x7c;
        /* 0037C138 +0x60/+0x2A8: time-driven, world eye/target.
           Other REP4 kinds belong to replay tracking, not this menu path. */
        if((u32(e)&0x13f)!=0x10 || !(f32(e+8)>0) || !isfinite(f32(e+8)))goto invalid;
        for(unsigned k=0x34;k<=0x58;k+=4)if(!valid_curve(p,u32(e+k)))goto invalid;
        for(unsigned k=0x64;k<=0x74;k+=4)if(!valid_curve(p,u32(e+k)))goto invalid;
        if(!u32(e+0x4c) || !(f32(e+0x5c)>0) || !(f32(e+0x60)>=f32(e+0x5c)))goto invalid;
    }
    gt4_pregrid_reset(p);return p;
invalid:free(p);return NULL;
}
void gt4_pregrid_close(gt4_pregrid *p){free(p);}
void gt4_pregrid_reset(gt4_pregrid *p){if(p){p->shot=p->frame=0;p->first=1;}}
unsigned gt4_pregrid_count(const gt4_pregrid *p){return p?p->count:0;}
static float curve(const gt4_pregrid *p,const uint8_t *e,unsigned field,float t,float missing)
{uint32_t at=u32(e+field);return at?gt4_pregrid_curve(p->data+at,t):missing;}
int gt4_pregrid_sample(const gt4_pregrid *p,unsigned shot,int frame,gt4_pregrid_camera *o,int *done)
{
    if(!p || shot>=p->count || !o || !done)return 0;
    const uint8_t *e=p->entries+shot*0x7c;float t=gt4_pregrid_time(f32(e+8),frame,done);
    memset(o,0,sizeof *o);o->shot=shot;o->frame=frame;
    /* 003FC7B0's missing coordinate default is 1. */
    for(int k=0;k<3;k++){o->eye[k]=curve(p,e,0x34+k*4,t,1);o->target[k]=curve(p,e,0x40+k*4,t,1);}
    o->roll=curve(p,e,0x6c,t,0);o->fov=fmaxf(f32(e+0x5c),fminf(f32(e+0x60),curve(p,e,0x4c,t,1)));
    gt4_pregrid_projection(o->fov,&o->sx,&o->sy);return 1;
}
/* 00385058 init, 00385098 +0x00..+0xF0 spring update. The pregrid's
   world-target path retains raw yaw/pitch; only lens and roll use springs
   (0037C138 +0x618..+0x640, +0x63C..+0x69C). */
static void spring_init(float s[9],float value,float stiffness,float damping,float rate,float cap)
{float v[9]={value,value,0,0,stiffness,damping,1,rate,cap};memcpy(s,v,sizeof v);}
static float spring(float s[9],float value)
{
    float d=value-s[1];s[1]=value;if(s[3]>0 && fabsf(d)>s[3])s[1]=value=d>0?s[1]-d+s[3]:s[1]-d-s[3];
    float a=((s[4]*(s[1]-s[0])+s[5]*s[2])/60.f)*s[6];s[2]+=a;
    if(s[8]>0 && s[2]>s[8])s[2]=s[8];if(s[7]>0 && s[2]<-s[7])s[2]=-s[7];
    s[0]+=s[2]/60.f;return s[0];
}
static int step(gt4_pregrid *p,gt4_pregrid_camera *o)
{
    int done;if(!p || !gt4_pregrid_sample(p,p->shot,(int)++p->frame,o,&done))return 0;
    /* 00378F28 +0xEC increments before sampling; 00375980 +0xCC..+0x1C0
       starts the next shot on its first tick in this same presentation frame. */
    if(done){p->shot=(p->shot+1)%p->count;p->frame=1;p->first=1;gt4_pregrid_sample(p,p->shot,1,o,&done);}
    if(p->first){spring_init(p->lens,o->fov,300,-40,150,150);spring_init(p->roll,o->roll,100,-20,150,100);p->first=0;}
    o->fov=spring(p->lens,o->fov);o->roll=spring(p->roll,o->roll);
    gt4_pregrid_projection(o->fov,&o->sx,&o->sy);return 1;
}
int gt4_pregrid_step(gt4_pregrid *p,gt4_pregrid_camera *o)
{
    /* EE FCR31 rounds toward zero. Restore the UI caller's rounding mode. */
    int mode=fegetround();fesetround(FE_TOWARDZERO);
    int ok=step(p,o);fesetround(mode);return ok;
}
