/* /pit/license_001: run the original 0042B440 parameter interpreter.
   Matrix stack and host sin/cos are explicit boundaries. Original
   0048B888/0048A970 compute rotations; no native parser in the oracle. */
#include "ee.h"
#include "license_motion.h"
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
static void scalar(ee_ctx*c){char*s=(char*)c->ram+c->r[5].uw[0];if(!strcmp(s,"CameraCoord"))coord[0]=c->fpr[12].f;if(!strcmp(s,"TargetCoord"))coord[1]=c->fpr[12].f;}
static void length(ee_ctx*c){c->r[2].uw[0]=(unsigned)strlen((char*)c->ram+c->r[4].uw[0]);}
static void sine(ee_ctx*c){float s=sinf(c->fpr[12].f),co=cosf(c->fpr[12].f);memcpy(c->ram+c->r[4].uw[0],&s,4);memcpy(c->ram+c->r[5].uw[0],&co,4);}
static ee_fn boundary(uint32_t p){
 if(p==0x57d8a0)return sine;if(p==0x57f260)return length;
 if(p<0x100 || p>=0x220 || p%8)return NULL;
 switch(p-0x100){case 8:return mark;case 0x10:return nop;case 0x18:return push;case 0x20:return pop;
 case 0x28:return translation;case 0x50:return lens;case 0x58:return camera;case 0x80:return scalar;case 0x98:return quaternion;default:return zero;}
}
static ee_fn lookup(uint32_t p){ee_fn fn=boundary(p);return fn?fn:recomp_lookup(p);}
static int call(ee_ctx*c,unsigned p){c->r[29].uw[0]=0x1ef0000;jmp_buf jump;c->exit_jmp=&jump;if(setjmp(jump)){fprintf(stderr,"original %08x %s\n",c->end_pc,c->end_detail);return 0;}recomp_lookup(p)(c);return 1;}
int main(int argc,char**argv){
 CHECK(argc==3);ee_ctx e;CHECK(!ee_init(&e));CHECK(ee_load_image(&e,argv[1],0x100000)>0x5a3900);
 const char *error;gt4_vol*v=gt4_vol_open(argv[2],&error);CHECK(v);size_t size;uint8_t*d=gt4_vol_read(v,"/pit/license_001",&size);CHECK(d);
 gt4_license_motion*m=gt4_license_motion_open(d,size);CHECK(m);memcpy(e.ram+BASE,d,size);e.lookup=lookup;e.hook=boundary;
 put(&e,OBJECT+4,VTABLE);for(unsigned i=0;i<0x120;i+=8){put(&e,VTABLE+i,0);put(&e,VTABLE+i+4,0x100+i);}
 unsigned cases=0;float maximum=0;
 for(unsigned bank=0;bank<2;bank++){
  unsigned root=BASE+word(d+4+bank*4);const uint8_t*b=d+word(d+4+bank*4);memset(e.r,0,sizeof e.r);e.r[4].uw[0]=root;CHECK(call(&e,0x427698));
  for(named=0;named<half(b+22);named++){
   const uint8_t*r=b+word(b+28)+named*16;unsigned slot=bank?3:0;CHECK(gt4_license_motion_select(m,slot,(char*)b+word(r)));
   for(tick=0;tick<900;tick++){
    CHECK(gt4_license_motion_step(m,slot,1.f/30));if(tick%13)continue;
    unsigned g,a;float t;CHECK(gt4_license_motion_sample(m,slot,&g,&a,&t));
    found=depth=look_count=fov_count=0;coord[0]=coord[1]=0;identity(current);put(&e,OBJECT,0);
    memset(e.r,0,sizeof e.r);e.r[4].uw[0]=root+word(b+16)+g*16;e.r[5].uw[0]=e.r[6].uw[0]=a;e.r[7].uw[0]=OBJECT;e.fpr[12].f=e.fpr[13].f=t;e.fpr[14].f=0;
    fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);int ok=call(&e,0x42b440);fesetenv(&env);CHECK(ok);CHECK(!depth);
    if(bank){
     float nlook[9],nfov,nc,nt;CHECK(look_count==1 && fov_count==1);CHECK(gt4_license_motion_camera(m,nlook,&nfov,&nc,&nt));
     if(getenv("GT4_LICENSE_CAMERA_NEGATIVE"))nlook[0]+=.125f;
     CHECK(!memcmp(nlook,look,36));CHECK(!memcmp(&nfov,&fov,4));CHECK(!memcmp(&nc,coord,4));CHECK(!memcmp(&nt,coord+1,4));
    }else{
     CHECK(found>0);const char*names[32];for(unsigned i=0;i<found;i++)names[i]=labels[i];float native[32][16];CHECK(gt4_license_motion_frames(m,slot,names,found,native)==(int)found);
     if(getenv("GT4_LICENSE_POSE_NEGATIVE"))native[0][12]+=.125f;
     for(unsigned i=0;i<found;i++)for(unsigned j=0;j<16;j++){float delta=fabsf(native[i][j]-frames[i][j]);if(delta>maximum)maximum=delta;if(delta>0.00002f)fprintf(stderr,"%s/%u %.9g vs %.9g\n",names[i],j,native[i][j],frames[i][j]);CHECK(isfinite(delta) && delta<=0.00002f);}
    }cases++;
   }
  }
 }
 printf("PASS %u original parameter streams; camera exact; actor matrix maximum error %.9g (tolerance 0.00002, host sin/cos and matrix stack boundaries).\n",cases,maximum);
 gt4_license_motion_close(m);free(d);gt4_vol_close(v);ee_free(&e);return 0;
}
