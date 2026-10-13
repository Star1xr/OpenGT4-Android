#include "license_image.h"
#include <fenv.h>
#include <math.h>
#include <string.h>
static float bits(uint32_t u){float f;memcpy(&f,&u,4);return f;}
/* 004A8AD0 +0..0xd8. Keep each EE multiply and accumulator add
   separate; host logf has a different rounding/approximation contract. */
static float image_log(float x)
{
 uint32_t u;memcpy(&u,&x,4);int n=(int)(u>>23)-127;
 float m=bits((u&0x7fffff)|0x3f800000),m2=m*m,m3=m*m2,m4=m2*m2,m5=m2*m3;
 float d=30;d+=m*750;d+=m2*3000;d+=m3*3000;d+=m4*750;d+=m5*30;
 float a=-137;a-=m*1625;a-=m2*2000;a+=m3*2000;a+=m4*1625;a+=m5*137;
 float inverse=1.f/d;return (float)n*bits(0x3f317217)+a*inverse;
}
/* 004A8BB0 +0..0x100. Polynomial and exponent-bit construction. */
static float image_exp(float x)
{
 int n=(int)(x*bits(0x3fb8aa3b));float t=x-(float)n*bits(0x3f317217);
 float t2=t*t,t3=t*t2,t4=t2*t2,t5=t2*t3,t6=t3*t3,t7=t3*t4,t8=t4*t4;
 float a=t;a+=t2*.5f;a+=t3*bits(0x3e2aaaaa);a+=t4*bits(0x3d2aaaaa);
 a+=t5*bits(0x3c088888);a+=t6*bits(0x3ab60b60);a+=t7*bits(0x39500d00);a+=t8*bits(0x37d00d00);a+=1;
 n+=127;if(n<0)return 0;if(n>=256){
  /* 006AFC08 is 7FFFFFFF (EE max); host IEEE represents that as NaN.
     Use host max finite at this overflow boundary; valid display sliders
     only evaluate log(x<=1), so this branch is outside the tested domain. */
  return bits(0x7f7fffff);
 }return a*bits((uint32_t)n<<23);
}
void gt4_license_image_options(const int o[4],float p[6])
{
 fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
 /* 003EFDF0 +0x24..0xa8 -> 003EFC10 +0x2c..0xc8. 0057D6C0
    pow remains a host libm boundary. These are display option values,
    not fixed settings copied from one player's savestate. */
 float a=(float)o[0]/10,b=((float)o[1]*3)/5,c=((float)o[2]+10)/10;
 float d=((float)o[3]*2000)/10+5500,base=a!=0?powf(2,a):1;
 float t=((d-5500)*bits(0x3e4ccccc))/2000;
 p[0]=base*(1-t);p[1]=base;p[2]=base*(1+t);p[3]=0;p[4]=1-b*bits(0x3dcccccc);p[5]=c;
 fesetenv(&env);
}
float gt4_license_image_decay(float previous)
{
 fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
 /* 003E0E38 +0xa8..0xe4: per screen callback, before publishing
    Scene parameters. The scene consumes the previous frame's value. */
 float next=previous*bits(0x3f7ae147);fesetenv(&env);return next;
}
float gt4_license_result_tick(int *first)
{
 /* 003DF978 +0xb0 / 003E1C28; 003E1C80/003E1EA0 reset after
    advance on the first outcome, so that draw remains at time zero. */
 if(first && *first){*first=0;return 0;}
 return bits(0x3c888888);
}
void gt4_license_image_lut(const float p[6],uint8_t lut[256][3])
{
 fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
 /* 004A8CB8 +0x40..0x250: unswizzle CLUT bits 3/4 when storing
    the logical 256-entry table for the native shader. */
 float range=p[4]-p[3],inverse=range!=0?1.f/range:0,exponent[3];
 for(int c=0;c<3;c++)exponent[c]=1.f/fmaxf(p[c],0);
 for(unsigned i=0;i<256;i++){
  float x=(float)i*bits(0x3b808080);
  for(int c=0;c<3;c++)lut[i][c]=0;
  if(x>p[3]){
   float log=image_log(fminf((x-p[3])*inverse,1));
   for(int c=0;c<3;c++)lut[i][c]=(uint8_t)fminf(fmaxf(image_exp(log*exponent[c])*255,0),255);
  }
 }fesetenv(&env);
}
void gt4_license_image_mix(float saturation,uint8_t out[3][3])
{
 fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
 /* 004A7D90 +0xb0..0x1dc. RGB weights are the instruction bits,
    not the .299/.587/.114 matrix of the unrelated course grade. */
 float s=fminf(fmaxf(saturation,-1),1)*128,d=fmaxf(128-s,0);
 const uint32_t weights[3]={0x3e999999,0x3f170a3d,0x3de147ae};
 for(int input=0;input<3;input++){
  float off=d*bits(weights[input]);
  for(int output=0;output<3;output++)out[input][output]=(uint8_t)fminf(fmaxf(off+(input==output?s:0),0),255);
 }fesetenv(&env);
}
void gt4_license_image_pixel(const uint8_t lut[256][3],const uint8_t m[3][3],const uint8_t in[3],uint8_t out[3])
{
 /* 004A81A8: three MODULATE/add passes (GS ALPHA 0x48, FIX 128).
    Each contribution truncates before the next pass adds it. */
 for(int c=0;c<3;c++){
  unsigned sum=0;for(int i=0;i<3;i++)sum+=(lut[in[i]][i]*m[i][c])>>7;
  out[c]=(uint8_t)(sum>255?255:sum);
 }
}
void gt4_license_shadow_projection(const float way[3],const float n[3],float m[16])
{
 fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
 /* 00408490 +0x134..0x1d4 / 00407FA0. Horizontal extent is
    limited to 1.5; vertical column lies on the actor's ground plane. */
 float x=way[0],z=way[2],length=x*x+z*z;
 if(length>=2.25f){float k=1.5f/sqrtf(length);x*=k;z*=k;}
 memset(m,0,64);m[0]=m[10]=m[15]=1;m[4]=x;m[6]=z;
 m[5]=n[1]!=0?-(n[0]*x+n[2]*z)/n[1]:0;fesetenv(&env);
}
void gt4_license_light_color(const float color[3],float sun,float fade,float level,float blend,float exposure,float out[3])
{
 fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
 /* 00397FF8 +0x10..0x50, used by 0040A1D0 -> 00398210. */
 float f=(1-(1-sun)*fade)*blend;f*=level;f+=1-blend;f*=exposure;
 for(int c=0;c<3;c++)out[c]=color[c]*f;fesetenv(&env);
}
