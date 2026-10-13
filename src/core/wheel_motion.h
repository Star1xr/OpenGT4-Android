#ifndef OPENGT4_WHEEL_MOTION_H
#define OPENGT4_WHEEL_MOTION_H
#include <stdint.h>
#include <string.h>
#include <math.h>
static inline float gt4_wheel_constant(uint32_t bits)
{float f;memcpy(&f,&bits,4);return f;}
/* 003D73A0 -> 00344E48: difference between interpolation samples 0/1,
   multiplied by Env0+EA0 exposure and the original radians-to-degrees.
   previous is car+910, already corrected across complete wheel turns. */
static inline float gt4_wheel_motion_degrees(float current,float previous,float interpolation,float exposure)
{
    float t=0+interpolation,now=previous*t+current*(1-t);
    t=1+interpolation;float before=previous*t+current*(1-t);
    float delta=(now-before)*exposure;
    return delta*gt4_wheel_constant(0x42652ee0u);
}
/* 00453EE4..00453F28: preserve multiply/divide order and strict > at
   0x3DB2B8C1. Do not substitute a guessed km/h threshold. */
static inline int gt4_wheel_motion_blurred(float degrees)
{
    float a=(degrees*gt4_wheel_constant(0x40490fdau))/180.f;
    if(a<0)a=-a;
    return gt4_wheel_constant(0x3db2b8c1u)<a;
}
static inline float gt4_wheel_motion_radians(float degrees)
{
    float a=(degrees*gt4_wheel_constant(0x40490fdau))/180.f;
    return a<0?-a:a;
}
/* 00453BB0 +80..+278: two draws per reduction, halving the angular
   offset until the requested sample count is reached. The GPU consumes
   this plan; no asset pixels or generated EE code are part of the port. */
typedef struct { float angle,size,source_size,offset; } gt4_wheel_accum_draw;
typedef struct { int size,stages;gt4_wheel_accum_draw draw[62]; } gt4_wheel_accum_plan;
static inline gt4_wheel_accum_plan gt4_wheel_accumulation(int width,int samples,float radians)
{
    gt4_wheel_accum_plan p={0};p.size=width<64?width:64;
    float angle=radians*.5f;
    for(unsigned count=2;;count*=2) {
        int n=p.stages*2;float source=p.stages?(float)p.size:(float)width;
        p.draw[n]=(gt4_wheel_accum_draw){0,(float)p.size,source,gt4_wheel_constant(0xbeccccccu)};
        p.draw[n+1]=(gt4_wheel_accum_draw){angle,(float)p.size,source,.5f};
        p.stages++;
        if(count>=(unsigned)samples || p.stages==31)break;
        angle*=.5f;
    }
    return p;
}
/* 00453A58 +40..+130: the two-times-tall rotated quad. Sine/cosine
   are PC libm boundaries, not a claim of bit-identical VU/GS pixels. */
static inline void gt4_wheel_accum_quad(const gt4_wheel_accum_draw *d,float xy[8])
{
    float half=d->size*.5f,s=sinf(d->angle)*half,c=cosf(d->angle)*half;
    float minus=c-s,plus=c+s;
    xy[0]=half-minus;xy[1]=d->size+2*(half-plus)+d->offset;
    xy[2]=half+plus;xy[3]=d->size+2*(half-minus)+d->offset;
    xy[4]=half+minus;xy[5]=d->size+2*(half+plus)+d->offset;
    xy[6]=half-plus;xy[7]=d->size+2*(half+minus)+d->offset;
}
#endif
