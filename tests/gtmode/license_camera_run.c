/* Original EE blocks: 0037E018 converts Mot1 radians to camera degrees;
   003794A8 projects; 0033C7CC advances fades; 003BC7DC changes their rate;
   0033FA40 emits the image factor. tan is an explicit host boundary. */
#include "ee.h"
#include "license_presentation.h"
#include <fenv.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define OBJ 0x1800000u
#define CON 0x1840000u
#define STACK 0x1ef0000u
#define CHECK(x) do{if(!(x)){fprintf(stderr,"license camera line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static unsigned draws;static float drawn_factor;
static void tangent(ee_ctx*c){c->fpr[0].f=tanf(c->fpr[12].f);}
static void nop(ee_ctx*c){(void)c;}
static void draw(ee_ctx*c){draws++;drawn_factor=c->fpr[12].f;if(c->r[5].uw[0]!=0)abort();}
static ee_fn boundary(uint32_t pc){switch(pc){case 0x57d380:return tangent;case 0x396ad0:return nop;case 0x3425e0:return draw;default:return NULL;}}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
static void word(ee_ctx*c,uint32_t a,uint32_t v){memcpy(c->ram+a,&v,4);}
static void putf(ee_ctx*c,uint32_t a,float v){memcpy(c->ram+a,&v,4);}
static int call(ee_ctx*c,uint32_t pc){
 jmp_buf jump;c->exit_jmp=&jump;c->r[29].uw[0]=STACK;
 if(setjmp(jump)){if(pc==0x37e018 && c->end_reason==EE_END_STOP_PC && c->end_pc==0x37e04c)return 1;
  fprintf(stderr,"original stopped %08x %s\n",c->end_pc,c->end_detail);return 0;}
 CHECK(recomp_lookup(pc));recomp_lookup(pc)(c);return pc!=0x37e018;
}
int main(int argc,char**argv){
 CHECK(argc==2);ee_ctx e;CHECK(!ee_init(&e));CHECK(ee_load_image(&e,argv[1],0x100000)>0x5a3900);e.lookup=lookup;e.hook=boundary;
 fenv_t env;fegetenv(&env);CHECK(!fesetround(FE_TOWARDZERO));unsigned projections=0,fades=0,rates=0,factors=0;
 for(int frames=-1200;frames<=1200;frames++){
  e.r[4].uw[0]=1;e.r[5].uw[0]=OBJ;e.fpr[12].f=(float)frames/60.f;
  CHECK(call(&e,0x37d5b0));int control=gt4_license_opening_control(frames);
  if(getenv("GT4_LICENSE_OPENING_CONTROL_NEGATIVE"))control++;
  CHECK(control==(int)e.r[2].uw[0]);
 }
 for(unsigned i=1;i<=2000;i++){
  float rad=i*.001f,sx,sy;putf(&e,STACK+0x30,rad);CHECK(call(&e,0x37e018));putf(&e,OBJ+0xc,e.fpr[21].f);
  e.r[4].uw[0]=OBJ;CHECK(call(&e,0x3794a8));gt4_license_camera_projection(rad,&sx,&sy);
  if(getenv("GT4_LICENSE_CAMERA_NEGATIVE"))sx*=1.01f;
  CHECK(!memcmp(&sx,e.ram+OBJ+0x1c,4) && !memcmp(&sy,e.ram+OBJ+0x24,4));projections++;
 }
 for(unsigned i=0;i<=1000;i++){
  float value=i*.001f;draws=0;e.r[4].uw[0]=OBJ;putf(&e,OBJ+0xcf4c,value);word(&e,OBJ+0xcf60,0);CHECK(call(&e,0x33fa40));
  float f=gt4_license_fade_factor(value);if(getenv("GT4_LICENSE_FADE_FACTOR_NEGATIVE"))f+=.01f;
  CHECK(draws==(unsigned)(value<1));CHECK(draws?!memcmp(&f,&drawn_factor,4):f==1);factors++;
 }
 /* Exercise signed rates, clamp crossings and every phase pair. Other
    Scene members after the fade loop receive inert, mapped fixtures. */
 word(&e,OBJ+0x6c,OBJ+0x100);word(&e,OBJ+0x180,OBJ+0x200);
 for(int enabled=0;enabled<=1;enabled++)for(int previous=0;previous<=3;previous++)for(int phase=0;phase<=3;phase++){
  for(int r=-2;r<=2;r++)for(int v=-1;v<=5;v++){
   gt4_license_fade n={v*.25f,r*.5f};float dt=1.f/60.f;
   putf(&e,OBJ+0xcf48,.5f);putf(&e,OBJ+0xcf50,.125f);putf(&e,OBJ+0xcf4c,n.value);putf(&e,OBJ+0xcf54,n.rate);
   e.r[16].uw[0]=OBJ;e.fpr[20].f=dt;CHECK(call(&e,0x33c7cc));
   word(&e,CON+0x18,enabled);word(&e,CON+0x28,previous);word(&e,CON+0x24,phase);
   e.r[16].uw[0]=CON;e.r[17].uw[0]=OBJ;CHECK(call(&e,0x3bc7dc));
   gt4_license_fade_step(&n,enabled,previous,phase,dt);
   if(getenv("GT4_LICENSE_FADE_NEGATIVE"))n.value+=.01f;
   if(getenv("GT4_LICENSE_FADE_PHASE_NEGATIVE"))n.rate+=.01f;
   CHECK(!memcmp(&n.value,e.ram+OBJ+0xcf4c,4));CHECK(!memcmp(&n.rate,e.ram+OBJ+0xcf54,4));fades++;rates++;
  }
 }
 /* Complete close/cut/open sequence, preserving inherited rate per tick. */
 gt4_license_fade n;gt4_license_fade_begin(&n);putf(&e,OBJ+0xcf4c,n.value);putf(&e,OBJ+0xcf54,n.rate);
 for(unsigned i=0;i<600;i++){
  int previous=i<120?0:i<240?1:2,phase=i<119?0:i<239?1:2;
  e.r[16].uw[0]=OBJ;e.fpr[20].f=1.f/60.f;CHECK(call(&e,0x33c7cc));
  word(&e,CON+0x18,1);word(&e,CON+0x28,previous);word(&e,CON+0x24,phase);e.r[16].uw[0]=CON;e.r[17].uw[0]=OBJ;CHECK(call(&e,0x3bc7dc));
  gt4_license_fade_step(&n,1,previous,phase,1.f/60.f);CHECK(!memcmp(&n.value,e.ram+OBJ+0xcf4c,4));CHECK(!memcmp(&n.rate,e.ram+OBJ+0xcf54,4));fades++;
 }
 CHECK(n.value==1 && n.rate==1);fesetenv(&env);
 /* Public helpers restore the caller's FPU mode. */
 CHECK(!fesetround(FE_TONEAREST));float sx,sy;gt4_license_camera_projection(.64f,&sx,&sy);CHECK(fegetround()==FE_TONEAREST);
 gt4_license_fade_step(&n,0,0,0,.01f);CHECK(fegetround()==FE_TONEAREST);gt4_license_fade_factor(.5f);CHECK(fegetround()==FE_TONEAREST);fesetenv(&env);
 printf("PASS %u lens projections at host tan boundary; %u fade steps, %u phase/rate cases and %u image factors bit-identical.\n",projections,fades,rates,factors);ee_free(&e);return 0;
}
