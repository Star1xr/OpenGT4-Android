/* 003CA698 -> 003CAAC8 -> 003CA168 execute in the EE oracle.
   Only the GS submission/allocation and host trig are boundaries. */
#include "ee.h"
#include "race_pit_crew.h"
#include "physics/car.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fenv.h>
ee_fn recomp_lookup(uint32_t);
#define OBJECT 0x1800000u
#define ROOT 0x1810000u
static unsigned sample,draws,allocation;static float strips[2][48][3];
#define CHECK(x) do{if(!(x)){fprintf(stderr,"cable sample %u line %d: %s\n",sample,__LINE__,#x);return 1;}}while(0)
static void nop(ee_ctx*c){(void)c;}
static void alloc(ee_ctx*c){c->r[2].uw[0]=allocation;allocation+=c->r[4].uw[0]*16;}
static void draw(ee_ctx*c){if(draws<2 && c->r[4].uw[0]==48)memcpy(strips[draws++],c->ram+c->r[5].uw[0],48*12);}
static void sine(ee_ctx*c){GT4_F32(c->r[4].uw[0])=sinf(c->fpr[12].f);GT4_F32(c->r[5].uw[0])=cosf(c->fpr[12].f);}
static ee_fn boundary(uint32_t p){switch(p){case 0x4a5b70:return alloc;case 0x49fd50:return draw;case 0x57d8a0:return sine;case 0x4a1638:case 0x4ab040:case 0x4a2808:case 0x4aa1d8:return nop;default:return NULL;}}
static ee_fn lookup(uint32_t p){ee_fn f=boundary(p);return f?f:recomp_lookup(p);}
static int call(ee_ctx*c,unsigned pc){c->r[29].uw[0]=0x1ef0000;jmp_buf j;c->exit_jmp=&j;if(setjmp(j)){fprintf(stderr,"EE %08x %s\n",c->end_pc,c->end_detail);return 0;}recomp_lookup(pc)(c);return 1;}
int main(int argc,char**argv){CHECK(argc==2 || argc==3);ee_ctx e;CHECK(!ee_init(&e));CHECK(ee_load_image(&e,argv[1],0x100000)>0x5a3900);e.lookup=lookup;e.hook=boundary;gt4_phys_mem=e.ram;
 fenv_t saved;fegetenv(&saved);gt4_phys_fpu_enter();float maximum=0;
 for(sample=0;sample<400;sample++){
  gt4_pit_crew c={0};unsigned role=sample%4;for(unsigned j=0;j<3;j++)c.box[j]=((int)(sample*(j+11)%101)-50)*.25f;c.box[3]=((int)(sample%13)-6)*.35f;
  float end[3]={c.box[0]+(sample%7)*.5f-1.5f,c.box[1]+(sample%5)*.25f,c.box[2]+(sample%11)*.35f-1.75f};
  c.cable[role][0]=((int)(sample%11)-5)*.015f;c.cable[role][2]=((int)(sample%17)-8)*.02f;
  memset(e.ram+OBJECT,0,0x100);memcpy(e.ram+OBJECT+role*48+36,end,12);GT4_F32(OBJECT+0xc0+role*4)=c.cable[role][0]*gt4_f32(0x40c7ae14);GT4_F32(OBJECT+0xd0+role*4)=c.cable[role][2]*gt4_f32(0x40c7ae14);GT4_U32(OBJECT+0xe0+role*4)=1;memcpy(e.ram+ROOT,c.box,12);
  memset(e.r,0,sizeof e.r);e.r[4].uw[0]=OBJECT;e.r[5].uw[0]=ROOT;e.fpr[12].f=c.box[3];e.fpr[13].f=c.box[1];CHECK(call(&e,0x3ca698));
  draws=0;allocation=0x1820000;memset(e.r,0,sizeof e.r);e.r[4].uw[0]=OBJECT;CHECK(call(&e,0x3caac8));CHECK(draws==2);
  float native[276][3];gt4_pit_cable_vertices(&c,role,end,native);if(argc==3)native[0][1]+=.125f;
  unsigned vertex=0;static const unsigned corner[6]={0,1,2,2,1,3};
  for(unsigned s=0;s<2;s++)for(unsigned i=0;i<23;i++)for(unsigned v=0;v<6;v++,vertex++)for(unsigned j=0;j<3;j++){
   float d=fabsf(native[vertex][j]-strips[s][i*2+corner[v]][j]);maximum=fmaxf(maximum,d);
   if(d>2e-5f)fprintf(stderr,"strip %u segment %u vertex %u axis %u %.9g vs %.9g\n",s,i,v,j,native[vertex][j],strips[s][i*2+corner[v]][j]);CHECK(isfinite(d) && d<=2e-5f);
  }
 }
 printf("PASS %u cables / 110400 vertices against original curve/strip generation; max %.9g, tolerance 2e-5.\n",sample,maximum);fesetenv(&saved);ee_free(&e);return 0;
}
