/* Compare layout writes and trophy springs against the original executable.
   Asset loading/model selection and common HUD initialization are boundaries. */
#include "ee.h"
#include "license_presentation.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define DISPLAY 0x1800000u
#define CHECK(x) do{if(!(x)){fprintf(stderr,"license presentation line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void noop(ee_ctx *c){(void)c;}
static ee_fn boundary(uint32_t pc){return pc==0x3a2360 || pc==0x454b90?noop:NULL;}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
static void put(ee_ctx*c,uint32_t a,uint32_t v){memcpy(c->ram+a,&v,4);}
static int call_at(ee_ctx *c,uint32_t pc,uint32_t object,int state,float dt){
 memset(c->r,0,sizeof c->r);c->r[4].uw[0]=object;c->r[5].uw[0]=state;
 c->r[29].uw[0]=0x1ef0000;c->fpr[12].f=dt;jmp_buf jump;c->exit_jmp=&jump;
 if(setjmp(jump)){fprintf(stderr,"original %08x %s\n",c->end_pc,c->end_detail);return 0;}
 recomp_lookup(pc)(c);return 1;
}
static int call(ee_ctx *c,uint32_t pc,int state,float dt){return call_at(c,pc,DISPLAY,state,dt);}
static void file_word(uint8_t *d,unsigned at,uint32_t v){memcpy(d+at,&v,4);}
int main(int argc,char **argv){
 CHECK(argc==2);ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0x5a3900);
 c.lookup=lookup;c.hook=boundary;
 for(unsigned i=0;i<3;i++){put(&c,DISPLAY+0x3f30+i*0x70+0x14,0x67f900);put(&c,DISPLAY+0x4080+i*0x94+0x14,0x67f000);}
 CHECK(call(&c,0x3bd808,0,0));gt4_license_layout p;gt4_license_layout_make(&p);
 if(getenv("GT4_LICENSE_PRESENTATION_NEGATIVE"))p.times[0].x++;
 for(unsigned i=0;i<3;i++){CHECK(!memcmp(c.ram+DISPLAY+0x3f30+i*0x70,&p.times[i],12));CHECK(!memcmp(c.ram+DISPLAY+0x4080+i*0x94,&p.marks[i],12));}
 CHECK(!memcmp(c.ram+DISPLAY+0x42c8,&p.pylons,12));CHECK(!memcmp(c.ram+DISPLAY+0x4360,&p.pylon_label,12));
 for(int state=-1;state<7;state++){
  float v[10][4];gt4_license_trophy_begin(v,state);CHECK(call(&c,0x3e18d0,state,0));
  CHECK(!memcmp(v,c.ram+DISPLAY+0x584,sizeof v));
  for(unsigned tick=0;tick<1200;tick++){
   float dt=tick%3==0?1.f/120:tick%3==1?1.f/60:1.f/30;
   gt4_license_trophy_step(v,dt);CHECK(call(&c,0x3e1a80,0,dt));
   CHECK(!memcmp(v,c.ram+DISPLAY+0x584,sizeof v));
  }
 }
 for(unsigned pattern=0;pattern<256;pattern++)for(unsigned variant=0;variant<3;variant++){
  uint8_t d[1024]={0};memcpy(d,"MDLS",4);d[0x2c]=3;d[0x1a]=2;d[0x1c]=d[0x1e]=1;
  file_word(d,0x60,80);for(unsigned i=0;i<3;i++)file_word(d,80+i*4,128);
  file_word(d,0x44,96);file_word(d,96,104);file_word(d,104,400);
  uint8_t *t=d+400;memcpy(t,"Tex1",4);t[0x10]=32;t[0x14]=1;
  file_word(t,0x18,48);file_word(t,0x20,128);memset(t+48,255,8);file_word(t,128,3);
  for(unsigned i=0;i<3;i++){file_word(t,132+i*4,160+i*12);file_word(t,160+i*12,1);
   file_word(t,164+i*12,((pattern+i)&15)<<19|((pattern*61+i*13)&0x3fff)<<5|((pattern+i)&31));}
  memcpy(c.ram+DISPLAY,d,sizeof d);put(&c,DISPLAY+400+0x18,DISPLAY+448);put(&c,DISPLAY+400+0x20,DISPLAY+528);
  for(unsigned i=0;i<3;i++)put(&c,DISPLAY+532+i*4,DISPLAY+560+i*12);
  CHECK(call_at(&c,0x498ca0,DISPLAY+400,(int)variant,0));
  CHECK(gt4_license_trophy_variant(d,sizeof d,variant));
  if(getenv("GT4_LICENSE_CLUT_NEGATIVE"))d[448]^=1;
  CHECK(!memcmp(d+448,c.ram+DISPLAY+448,8));
  CHECK(!gt4_license_trophy_variant(d,sizeof d,3));CHECK(!gt4_license_trophy_variant(d,64,0));
 }
 ee_free(&c);puts("PASS original licence HUD layout, 9600 trophy spring steps and 768 CLUT selections; common HUD and GPU rendering are boundaries.");return 0;
}
