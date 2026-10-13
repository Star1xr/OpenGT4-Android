/* Scalar Photo Develop passes against the original EE executable.
   GS matrix uploads and libm pow are explicit hardware/library boundaries. */
#include "ee.h"
#include "photo_mode.h"
#include <fenv.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define OBJ 0x1800000u
#define DATA 0x1840000u
#define STACK 0x1ef0000u
#define CHECK(x) do{if(!(x)){fprintf(stderr,"photo optics %d: %s\n",__LINE__,#x);return 1;}}while(0)
static float projection[16],captured[16],translation[2],original_sample_time;
static void time_boundary(ee_ctx *c){original_sample_time=c->fpr[24].f;longjmp(*c->exit_jmp,2);}
static void nop(ee_ctx *c){(void)c;}
static void power(ee_ctx *c){c->fpr[0].f=powf(c->fpr[12].f,c->fpr[13].f);}
static void get_projection(ee_ctx *c){memcpy(c->ram+c->r[4].uw[0],projection,64);}
static void set_projection(ee_ctx *c){memcpy(captured,c->ram+c->r[4].uw[0],64);}
static void translate(ee_ctx *c){translation[0]=c->fpr[12].f;translation[1]=c->fpr[13].f;}
static ee_fn boundary(uint32_t pc){switch(pc){
 case 0x57d6c0:return power;case 0x3326e8:return nop;
 case 0x4a7550:return get_projection;case 0x4a74b4:return set_projection;case 0x4a7844:return translate;
 case 0x1fff000:return time_boundary;
 default:return NULL;}}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
static int call(ee_ctx *c,uint32_t pc){jmp_buf jump;c->exit_jmp=&jump;c->r[29].uw[0]=STACK;
 if(setjmp(jump)){fprintf(stderr,"original stopped %08x %s\n",c->end_pc,c->end_detail);return 0;}
 CHECK(recomp_lookup(pc));recomp_lookup(pc)(c);return 1;}
static int sample_time(ee_ctx *c,int sample,int count,float extent)
{
 uint32_t world=DATA+0x1000,vt=DATA+0x2000,list=DATA+0x3000,callback=0x1fff000;
 memcpy(c->ram+OBJ,&world,4);memcpy(c->ram+world+0x64,&vt,4);memcpy(c->ram+vt+0xd4,&callback,4);
 memcpy(c->ram+OBJ+0x48,&list,4);memcpy(c->ram+OBJ+0x118,&count,4);memcpy(c->ram+OBJ+0x120,&extent,4);
 uint32_t freeze=1,width=640,height=480;
 memcpy(c->ram+OBJ+0x124,&freeze,4);memcpy(c->ram+OBJ+0xc,&width,4);memcpy(c->ram+OBJ+0x10,&height,4);
 c->r[4].uw[0]=OBJ;c->r[5].uw[0]=DATA;c->r[6].uw[0]=DATA;c->r[7].uw[0]=0;c->r[8].uw[0]=0;c->r[9].uw[0]=sample;c->r[29].uw[0]=STACK;
 jmp_buf jump;c->exit_jmp=&jump;int ended=setjmp(jump);
 if(ended)return ended==2;
 recomp_lookup(0x334730)(c);return 0;
}
int main(int argc,char **argv)
{
 CHECK(argc==2);ee_ctx e;CHECK(!ee_init(&e));CHECK(ee_load_image(&e,argv[1],0x100000)>0x5a3900);e.lookup=lookup;e.hook=boundary;
 fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);unsigned colors=0,lenses=0;
 uint32_t count_instruction;memcpy(&count_instruction,e.ram+0x3c4d98,4);CHECK((count_instruction&0xffffu)==GT4_PHOTO_SAMPLES);
 const char *filters[]={"none","monochrome","mature"};
 for(int b=-8;b<=8;b++)for(int t=-5;t<=5;t++)for(int contrast=0;contrast<=5;contrast++)for(int f=0;f<3;f++){
  gt4_photo_mode p={0};p.brightness=(float)b;p.white_balance=t;p.contrast=(float)contrast;strcpy(p.color_filter,filters[f]);
  float actual[6];gt4_photo_color(&p,actual);e.r[4].uw[0]=OBJ;e.fpr[12].f=b*.125f;e.fpr[13].f=(float)contrast;
  e.fpr[14].f=1;e.fpr[15].f=(float)t*2000/5+5500;e.fpr[16].f=5500;CHECK(call(&e,0x3efc10));
  if(f==1){float zero=0;memcpy(e.ram+OBJ+20,&zero,4);}
  if(f==2){float m[6]={1.5f,.94999998807907104f,.59999996423721313f,0,1,.29999998211860657f};
   memcpy(e.ram+DATA,m,24);e.r[4].uw[0]=OBJ;e.r[5].uw[0]=DATA;CHECK(call(&e,0x3efd80));}
  if(getenv("GT4_PHOTO_COLOR_NEGATIVE"))actual[0]+=.01f;
  CHECK(!memcmp(actual,e.ram+OBJ,24));colors++;
 }
 enum{SAMPLES=1024};float samples[SAMPLES][4];gt4_photo_samples(samples,SAMPLES);
 e.r[4].uw[0]=OBJ;e.r[5].uw[0]=0x12345678u;CHECK(call(&e,0x579438));
 for(int i=0;i<SAMPLES;i++){
  float s[4];do{for(int k=0;k<2;k++){e.r[4].uw[0]=OBJ;CHECK(call(&e,0x579548));s[k]=e.fpr[0].f*2-1;}}while(s[0]*s[0]+s[1]*s[1]>1);
  for(int k=2;k<4;k++){e.r[4].uw[0]=OBJ;CHECK(call(&e,0x579548));s[k]=e.fpr[0].f;}
  if(getenv("GT4_PHOTO_SAMPLE_NEGATIVE"))samples[i][0]+=.001f;
  CHECK(!memcmp(samples[i],s,16));
 }
 for(int n=0;n<512;n++){
  float aspect=n&1?4.f/3:16.f/9,sy=.5f+(n%11)*.3f,aperture=1.f+(n%16)*.5f,focus=n%9?2.f+n*.25f:.01f;
  memset(projection,0,sizeof projection);projection[0]=sy/aspect;projection[5]=sy;projection[10]=projection[15]=1;
  float shift[2],jitter[2];gt4_photo_lens(35,aperture,focus,aspect,sy,samples[n],shift,jitter);
  e.fpr[12].f=focus;e.fpr[13].f=aperture;e.fpr[14].f=35;e.fpr[15].f=aspect;e.fpr[16].f=aspect;
  e.fpr[17].f=samples[n][0];e.fpr[18].f=samples[n][1];e.fpr[19].f=0;memset(e.ram+STACK,0,4);CHECK(call(&e,0x332550));
  if(getenv("GT4_PHOTO_LENS_NEGATIVE"))shift[0]+=.001f;
  for(int k=0;k<2;k++){CHECK(fabsf(shift[k]-translation[k])<=.0000001f);CHECK(fabsf(jitter[k]-captured[8+k])<=.0000001f);}lenses++;
 }
 unsigned times=0;
 for(int i=-2;i<=20;i++){
  int index=i<0?0:i>18?18:i;int32_t encoded;memcpy(&encoded,e.ram+0x620ef0+index*4,4);
  e.r[4].sd[0]=i;CHECK(call(&e,0x36f628));CHECK(e.r[2].sw[0]==encoded);
  float seconds=gt4_photo_shutter_seconds(encoded);e.r[4].sd[0]=encoded;CHECK(call(&e,0x36fa20));CHECK(!memcmp(&seconds,&e.fpr[0].f,4));
  float extent=gt4_photo_exposure(seconds);e.r[4].uw[0]=OBJ;e.fpr[12].f=seconds;CHECK(call(&e,0x333680));CHECK(!memcmp(&extent,e.ram+OBJ+0x120,4));
  for(int n=0;n<GT4_PHOTO_SAMPLES;n++){
   CHECK(sample_time(&e,n,GT4_PHOTO_SAMPLES,extent));float t=gt4_photo_time(extent,n,GT4_PHOTO_SAMPLES);
   if(getenv("GT4_PHOTO_TIME_NEGATIVE"))t+=.001f;
   CHECK(!memcmp(&t,&original_sample_time,4));
   float current[12],previous[12],out[12];for(int k=0;k<12;k++){current[k]=(float)((k+1)*(i+7))*.13f;previous[k]=current[k]-(float)(k+1)*.003f;}
   gt4_photo_lerp(out,current,previous,12,t);memcpy(e.ram+DATA,current,48);memcpy(e.ram+DATA+64,previous,48);
   e.r[4].uw[0]=DATA+128;e.r[5].uw[0]=12;e.r[6].uw[0]=DATA+64;e.r[7].uw[0]=DATA;e.fpr[12].f=t;
   CHECK(call(&e,0x33b358));CHECK(!memcmp(out,e.ram+DATA+128,48));times++;
  }
 }
 for(int i=0;i<3;i++){float seconds=i?i==1?0.01666666567325592041f:1:0;float extent=gt4_photo_exposure(seconds);
  e.r[4].uw[0]=OBJ;e.fpr[12].f=seconds;CHECK(call(&e,0x333680));CHECK(!memcmp(&extent,e.ram+OBJ+0x120,4));}
 printf("PASS %u exact shutter/sample-time/interpolation cases, exposure clamp boundaries.\n",times);
 fesetenv(&env);ee_free(&e);printf("PASS %u exact original color/filter cases, %u exact lens samples, %u optical matrices (tolerance 1e-7).\n",colors,SAMPLES,lenses);return 0;
}
