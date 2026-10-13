/* GT Auto service motions: run the original 0042B440 parameter interpreter.
   Matrix stack and host sin/cos are explicit boundaries. Original
   0048B888/0048A970 compute rotations; no native parser in the oracle. */
#include "ee.h"
#include "mot1.h"
#include "gpb.h"
#include "vol.h"
#include <math.h>
#include <fenv.h>
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define BASE 0x1900000u
#define OBJECT 0x1800000u
#define VTABLE (OBJECT+0x100)
#define SCRATCH (OBJECT+0x400)
#define CHECK(x) do{if(!(x)){fprintf(stderr,"pose line %d (%u/%u): %s\n",__LINE__,named,tick,#x);return 1;}}while(0)
static unsigned named,tick,depth,found,look_count,fov_count;
static float current[16],stack[32][16],frames[32][16],look[9],fov,coord[2];
static char labels[32][64];
static float light_values[4][3][4],channel_values[3];static unsigned light_counts[4][3],channel_counts[3];
static const char *channel_names[]={"fade","lift","shadowOpacity"};
static unsigned word(const uint8_t*d){unsigned v;memcpy(&v,d,4);return v;}
static unsigned half(const uint8_t*d){return d[0]|(unsigned)d[1]<<8;}
static void put(ee_ctx*c,unsigned p,unsigned v){memcpy(c->ram+p,&v,4);}
static void identity(float m[16]){memset(m,0,64);m[0]=m[5]=m[10]=m[15]=1;}
static void product(const float right[16]){
 float result[16];for(unsigned r=0;r<4;r++)for(unsigned col=0;col<4;col++)
  result[col*4+r]=((current[r]*right[col*4]+current[4+r]*right[col*4+1])+current[8+r]*right[col*4+2])+current[12+r]*right[col*4+3];
 memcpy(current,result,64);
}
static void zero(ee_ctx*c){c->r[2].uw[0]=0;}
static void nop(ee_ctx*c){(void)c;}
static void push(ee_ctx*c){(void)c;if(depth>=32)abort();memcpy(stack[depth++],current,64);}
static void pop(ee_ctx*c){(void)c;if(!depth)abort();memcpy(current,stack[--depth],64);}
static void mark(ee_ctx*c){if(found>=32)abort();snprintf(labels[found],64,"%s",c->ram+c->r[5].uw[0]);memcpy(frames[found++],current,64);}
static void translation(ee_ctx*c){float m[16];identity(m);for(unsigned i=0;i<3;i++)m[12+i]=c->fpr[12+i].f;product(m);}
static void quaternion(ee_ctx*c){
 for(unsigned i=0;i<4;i++)memcpy(c->ram+SCRATCH+i*4,&c->fpr[12+i].f,4);
 c->r[4].uw[0]=SCRATCH;c->r[5].uw[0]=SCRATCH+0x10;c->fpr[12].f=c->fpr[13].f=c->fpr[14].f=0;
 recomp_lookup(0x48a970)(c);float m[16];memcpy(m,c->ram+SCRATCH+0x10,64);product(m);
}
static void lens(ee_ctx*c){fov=c->fpr[12].f;fov_count++;}
static void camera(ee_ctx*c){for(unsigned i=0;i<8;i++)look[i]=c->fpr[12+i].f;memcpy(look+8,c->ram+c->r[29].uw[0],4);look_count++;}
static void scalar(ee_ctx*c){char*s=(char*)c->ram+c->r[5].uw[0];if(!strcmp(s,"CameraCoord"))coord[0]=c->fpr[12].f;if(!strcmp(s,"TargetCoord"))coord[1]=c->fpr[12].f;
 for(unsigned i=0;i<3;i++)if(!strcmp(s,channel_names[i])){channel_values[i]=c->fpr[12].f;channel_counts[i]++;}}
static void light(ee_ctx*c,unsigned kind){unsigned l=c->r[5].uw[0];if(l>=3)abort();for(unsigned i=0;i<(kind==3?3:4);i++)light_values[kind][l][i]=c->fpr[12+i].f;light_counts[kind][l]++;}
static void diffuse(ee_ctx*c){light(c,0);}static void ambient(ee_ctx*c){light(c,1);}static void specular(ee_ctx*c){light(c,2);}static void light_position(ee_ctx*c){light(c,3);}
static void length(ee_ctx*c){c->r[2].uw[0]=(unsigned)strlen((char*)c->ram+c->r[4].uw[0]);}
static void sine(ee_ctx*c){float s=sinf(c->fpr[12].f),co=cosf(c->fpr[12].f);memcpy(c->ram+c->r[4].uw[0],&s,4);memcpy(c->ram+c->r[5].uw[0],&co,4);}
static void host_sincos(float x,float *s,float *co){*s=sinf(x);*co=cosf(x);}
/* Axis rotations and scale terminate at the VU matrix boundary. */
static void axis(ee_ctx*c,unsigned a){float m[16];identity(m);float s=sinf(c->fpr[12].f),co=cosf(c->fpr[12].f);unsigned b=(a+1)%3,d=(a+2)%3;m[b*4+b]=m[d*4+d]=co;m[b*4+d]=s;m[d*4+b]=-s;product(m);}
static void rx(ee_ctx*c){axis(c,0);}static void ry(ee_ctx*c){axis(c,1);}static void rz(ee_ctx*c){axis(c,2);}
static void scaling(ee_ctx*c){float m[16];identity(m);for(unsigned i=0;i<3;i++)m[i*5]=c->fpr[12+i].f;product(m);}
static ee_fn boundary(uint32_t p){
 if(p==0x57d8a0)return sine;if(p==0x57f260)return length;
 if(p<0x100 || p>=0x220 || p%8)return NULL;
 switch(p-0x100){case 8:return mark;case 0x10:return nop;case 0x18:return push;case 0x20:return pop;
 case 0x28:return translation;case 0x30:return rx;case 0x38:return ry;case 0x40:return rz;case 0x48:return scaling;
 case 0x50:return lens;case 0x58:return camera;case 0x60:return diffuse;case 0x68:return ambient;case 0x70:return specular;case 0x78:return light_position;case 0x80:return scalar;case 0x98:return quaternion;default:return zero;}
}
static ee_fn lookup(uint32_t p){ee_fn fn=boundary(p);return fn?fn:recomp_lookup(p);}
static int call(ee_ctx*c,unsigned p){c->r[29].uw[0]=0x1ef0000;jmp_buf jump;c->exit_jmp=&jump;if(setjmp(jump)){fprintf(stderr,"original %08x %s\n",c->end_pc,c->end_detail);return 0;}recomp_lookup(p)(c);return 1;}
int main(int argc,char**argv){
 CHECK(argc==3);ee_ctx e;CHECK(!ee_init(&e));CHECK(ee_load_image(&e,argv[1],0x100000)>0x5a3900);
 const char *error;gt4_vol*v=gt4_vol_open(argv[2],&error);CHECK(v);e.lookup=lookup;e.hook=boundary;
 put(&e,OBJECT+4,VTABLE);for(unsigned i=0;i<0x120;i+=8){put(&e,VTABLE+i,0);put(&e,VTABLE+i+4,0x100+i);}
 const char *paths[]={"carwash_motion1.mot","carwash_motion2.mot","carwash_cloth1.mot","carwash_cloth2.mot",
 "oil_change.mot","oil_change_cart.mot","oil_change_start.mot","oil_change_cart_start.mot","oil_change_end.mot","oil_change_cart_end.mot",
 "car_wash_cam.mot","oil_change_f_cam.mot","oil_change_cam.mot"};
 unsigned cases=0;float maximum=0;
 for(named=0;named<13;named++){
  size_t size,psize;uint8_t *pkg=gt4_vol_read(v,named<4 || named==10?"/projects/gtmode/US/CarWashRoot.gpb":"/projects/gtmode/US/OilChangeRoot.gpb",&psize);CHECK(pkg);
  char path[160];snprintf(path,sizeof path,"image/gtmode/gtauto/%s",paths[named]);uint8_t*d=gpb_load(pkg,psize,path,&size);free(pkg);CHECK(d);
  gt4_mot1*m=gt4_mot1_open(d,size);CHECK(m);memcpy(e.ram+BASE,d,size);
  memset(e.r,0,sizeof e.r);e.r[4].uw[0]=BASE;CHECK(call(&e,0x427698));
  for(tick=0;tick<241;tick++){
   float t=gt4_mot1_end(m)*(float)tick/240;
   found=depth=look_count=fov_count=0;memset(light_counts,0,sizeof light_counts);memset(channel_counts,0,sizeof channel_counts);identity(current);put(&e,OBJECT,0);memset(e.r,0,sizeof e.r);
   e.r[4].uw[0]=BASE+word(d+16);e.r[5].uw[0]=e.r[6].uw[0]=0;e.r[7].uw[0]=OBJECT;e.fpr[12].f=e.fpr[13].f=t;e.fpr[14].f=0;
   fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);int ok=call(&e,0x42b440);fesetenv(&env);CHECK(ok);CHECK(!depth);
   CHECK(named>=10 || found>0);const char*names[32];for(unsigned i=0;i<found;i++)names[i]=labels[i];float native[32][16];CHECK(gt4_mot1_frames(m,t,names,found,native,host_sincos)==(int)found);
   if(getenv("GT4_SERVICE_MOTION_NEGATIVE"))native[0][12]+=.125f;
   for(unsigned i=0;i<found;i++)for(unsigned j=0;j<16;j++){
    float delta=fabsf(native[i][j]-frames[i][j]);if(delta>maximum)maximum=delta;
    if(delta>0.00002f)fprintf(stderr,"%s %s/%u at %.9g: %.9g vs %.9g\n",paths[named],names[i],j,t,native[i][j],frames[i][j]);CHECK(isfinite(delta) && delta<=0.00002f);
   }
   if(look_count || fov_count){float nl[9],nf;CHECK(gt4_mot1_camera(m,t,nl,&nf));
    for(unsigned i=0;i<9;i++)CHECK(fabsf(nl[i]-look[i])<=.00002f);CHECK(fabsf(nf-fov)<=.00002f);}
   for(unsigned i=0;i<3;i++)if(channel_counts[i]){float value;CHECK(gt4_mot1_value(m,channel_names[i],t,&value));
    if(getenv("GT4_SERVICE_CHANNELS_NEGATIVE"))value+=.125f;CHECK(fabsf(value-channel_values[i])<=.00002f);}
   for(unsigned k=0;k<4;k++)for(unsigned l=0;l<3;l++)if(light_counts[k][l]){float values[4];int n=gt4_mot1_light(m,0x0c+k,l,t,values);
    CHECK(n==(k==3?3:4));for(int i=0;i<n;i++)CHECK(fabsf(values[i]-light_values[k][l][i])<=.00002f);}
   fesetenv(&env);cases++;
  }
  gt4_mot1_close(m);free(d);
 }
 printf("PASS %u original service parameter streams; matrix maximum error %.9g (tolerance 0.00002, host sin/cos and matrix stack boundaries).\n",cases,maximum);
 gt4_vol_close(v);ee_free(&e);return 0;
}
