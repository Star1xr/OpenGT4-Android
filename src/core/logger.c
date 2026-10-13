#include "logger.h"
#include <string.h>

static float minimum(float a,float b){return a<b?a:b;}
static float maximum(float a,float b){return a>b?a:b;}
static float car_float(const uint8_t *p){float v;memcpy(&v,p,4);return v;}

float gt4_logger_spacing(float length)
{
    /* 00122FA8 +0x9c..0xd0: divide by 0x467ffc00, minimum 0.5 m. */
    return maximum(length/16383.f,0.5f);
}
void gt4_logger_begin(gt4_logger_sampler *s,gt4_logger_point *p,float spacing,float length)
{
    /* 0011E400 +0x0..0x48 leaves previous values until the first input. */
    s->points=p;s->count=0;s->spacing=spacing;s->inverse_spacing=1.f/spacing;
    s->course_length=length;s->first=1;
    s->maximum_speed=s->speed_area=s->accelerator_area=s->brake_area=0;
}
void gt4_logger_car_input(gt4_logger_input *v,const uint8_t *c)
{
    /* 0011E450 +0x60..0xb8; getters 00343B48, 00344698, 003446A0.
       00344798/00344828 +0x0..0x28 interpolate the car's X/Z with f12=0. */
    int16_t lap;memcpy(&lap,c+0x5b0,2);v->lap=lap;
    v->distance=car_float(c+0x5a8);v->speed=car_float(c+0x6c8);
    v->gear=c[0x624];v->accelerator=car_float(c+0x58c);v->brake=car_float(c+0x590);
    float t=0.f+car_float(c+0x554),u=1.f-t;
    float x0=car_float(c+0x8a4)*t,x1=car_float(c+0x888)*u;
    float z0=car_float(c+0x8ac)*t,z1=car_float(c+0x890)*u;
    v->x=x0+x1;v->z=z0+z1;
}
static float linear(float x0,float x1,float y0,float y1,float x)
{
    /* 0011E310 +0x0..0x34: preserve every intermediate float rounding. */
    float a=y0*x1,b=y1*x0,dx=x1-x0,dy=y1-y0;
    float intercept=a-b,inverse=1.f/dx;
    dy=dy*x;dy=dy+intercept;return dy*inverse;
}
static float area(float x0,float x1,float y0,float y1,float left,float right)
{
    /* 0011E348 +0x0..0xb4: clipped trapezoidal integral. */
    if(x1<left || right<x0)return 0.f;
    if(x0<left || right<x1){
        float dx=x1-x0,a=y0*x1,b=y1*x0,inverse=1.f/dx;
        float dy=y1-y0,intercept=a-b;
        if(x0<left){float v=dy*left;v=v+intercept;y0=v*inverse;x0=left;}
        if(right<x1){float v=dy*right;v=v+intercept;y1=v*inverse;x1=right;}
    }
    float v=y0+y1,d=x1-x0;v=v*d;return v*0.5f;
}
void gt4_logger_sample(gt4_logger_sampler *s,const gt4_logger_input *v)
{
    /* 0011E450 +0x48..0x458. Reverse movement updates prior controls but
       cannot append samples. A full buffer returns before previous updates. */
    if(s->count>=GT4_LOGGER_POINTS)return;
    float lap_distance=(float)(v->lap-1)*s->course_length;
    float absolute=v->distance+lap_distance;
    s->maximum_speed=maximum(s->maximum_speed,v->speed);
    if(s->first){s->origin=s->last_distance=s->previous_distance=absolute;s->first=0;}
    else if(absolute<=s->last_distance)absolute=s->last_distance;
    else{
        s->last_distance=absolute;
        float new_distance=absolute-s->origin,old_distance=s->previous_distance-s->origin;
        float base=(float)s->count*s->spacing,next=base+s->spacing;
        s->speed_area+=area(old_distance,new_distance,s->previous_speed,v->speed,base,next);
        s->accelerator_area+=area(old_distance,new_distance,s->previous_accelerator,v->accelerator,base,next);
        s->brake_area+=area(old_distance,new_distance,s->previous_brake,v->brake,base,next);
        while(next<=new_distance){
            float speed=s->speed_area*40.f,accel=s->accelerator_area*255.f,brake=s->brake_area*255.f;
            speed=speed*s->inverse_spacing;accel=accel*s->inverse_spacing;brake=brake*s->inverse_spacing;
            speed=speed+0.5f;accel=accel+0.5f;brake=brake+0.5f;
            float x=linear(old_distance,new_distance,s->previous_x,v->x,next);
            float z=linear(old_distance,new_distance,s->previous_z,v->z,next);
            gt4_logger_point *p=s->points+s->count;
            p->speed_gear=(uint16_t)(((uint32_t)(int32_t)minimum(speed,8191.f)<<3)|(v->gear&7));
            p->accelerator=(uint8_t)(int32_t)minimum(accel,255.f);
            p->brake=(uint8_t)(int32_t)minimum(brake,255.f);
            p->x=(int16_t)(int32_t)x;p->z=(int16_t)(int32_t)z;
            if(++s->count>=GT4_LOGGER_POINTS)return;
            next=next+s->spacing;base=base+s->spacing;
            s->speed_area=area(old_distance,new_distance,s->previous_speed,v->speed,base,next);
            s->accelerator_area=area(old_distance,new_distance,s->previous_accelerator,v->accelerator,base,next);
            s->brake_area=area(old_distance,new_distance,s->previous_brake,v->brake,base,next);
        }
    }
    s->previous_distance=absolute;s->previous_speed=v->speed;
    s->previous_accelerator=v->accelerator;s->previous_brake=v->brake;
    s->previous_x=v->x;s->previous_z=v->z;
}
float gt4_logger_length(const gt4_logger_sampler *s)
{
    /* 0011DBB0 +0x20..0x30, including count=0 => -spacing. */
    return (float)((int32_t)s->count-1)*s->spacing;
}
static uint32_t index_at(const gt4_logger_sampler *s,float distance)
{
    return (uint32_t)(minimum(maximum(distance,0.f),gt4_logger_length(s))/s->spacing);
}
float gt4_logger_speed_at(const gt4_logger_sampler *s,float distance)
{
    /* 0011DBE8 +0x24..0x8c; empty tracks are an API safety boundary. */
    return s->count?(float)(s->points[index_at(s,distance)].speed_gear>>3)/40.f:0.f;
}
float gt4_logger_x_at(const gt4_logger_sampler *s,float distance)
{
    /* 0011DC80 +0x30..0x8c uses the preceding spatial sample. */
    return s->count?(float)s->points[index_at(s,distance-s->spacing)].x:0.f;
}
float gt4_logger_z_at(const gt4_logger_sampler *s,float distance)
{
    /* 0011DD10 +0x30..0x8c, same preceding spatial sample as X. */
    return s->count?(float)s->points[index_at(s,distance-s->spacing)].z:0.f;
}
