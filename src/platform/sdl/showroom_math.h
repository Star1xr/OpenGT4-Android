#ifndef OPENGT4_SHOWROOM_MATH_H
#define OPENGT4_SHOWROOM_MATH_H
#include <math.h>
#include <stdint.h>
/* The model's alpha is a coefficient (128 = 1), not ordinary 8-bit opacity. */
static inline unsigned showroom_alpha_byte(float coefficient)
{return (unsigned)floorf(fmaxf(0,fminf(255,coefficient*128))+.5f);}
static inline uint8_t showroom_blend_channel(float source,uint8_t destination,
    float source_alpha,float destination_alpha,uint8_t equation,uint8_t fixed,int enabled)
{
    float v[4]={source,destination,0,0},f[4]={source_alpha,destination_alpha,fixed/128.f,0};
    float color=enabled?(v[equation&3]-v[(equation>>2)&3])*f[(equation>>4)&3]+v[(equation>>6)&3]:source;
    return (uint8_t)fmaxf(0,fminf(255,color));
}
/* Address each bilinear tap separately: clamp is not texture repetition. */
static inline int showroom_texel(int at,int size,unsigned mode,unsigned minimum,unsigned maximum)
{
    if(mode==0)return ((at%size)+size)%size;
    if(mode==3)at=(int)(((unsigned)at&minimum)|maximum);
    else if(mode==2){if(at<(int)minimum)at=(int)minimum;if(at>(int)maximum)at=(int)maximum;}
    if(at<0)return 0;if(at>=size)return size-1;return at;
}
/* Body model coordinates are relative to the chassis reference plane.
   Suspension ride heights lift that plane; wheel centres remain on the ground. */
static inline void showroom_body_pose(float point[3],float sine,float cosine,float lift)
{
    float y=point[1],z=point[2];point[1]=cosine*y+sine*z+lift;point[2]=cosine*z-sine*y;
}
/* CarModel::Callback 004537A4 supplies (callback-11)&1 to 00454110.
   00454154..00454178: side 1 translates +width/2, side 0 -width/2. */
static inline void showroom_wheel_attachment(float point[3],unsigned native,float width)
{point[0]+=((native&1)? .5f:-.5f)*width;}
/* func_0034BA30: diameter <61 is whole inches, larger multiples of 10
   are whole inches divided by 10; the remaining values are tenths.
   This includes 26/41-inch vintage wheels as well as modern 17-inch rims. */
static inline void showroom_tire_size(const uint8_t size[4],float *width,float *rim,float *radius)
{
    unsigned d=size[2];
    *width=(float)(size[3]?size[3]*5u:10u)*.001f;
    *rim=d<61?d*.0254f*.5f:d%10==0?(d/10)*.0254f*.5f:d*.0254f*.05f;
    *radius=*rim+*width*size[0]*.05f;
}
#endif
