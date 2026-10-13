/* MenuRaceBase update/draw/reset against the original executable. Scene
   work, job scheduling, pad and GS calls are declared boundaries here. */
#include "ee.h"
#include "license_presentation.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define MENU 0x1800000u
#define SCENE 0x1810000u
#define VT 0x1850000u
#define TASK 0x1860000u
#define CHECK(x) do{if(!(x)){fprintf(stderr,"menu cycle tick %u line %d: %s\n",tick,__LINE__,#x);return 1;}}while(0)
static unsigned tick,steps,length,jobs,closes,resets;static int queued;
static void word(ee_ctx*c,unsigned a,unsigned v){memcpy(c->ram+a,&v,4);}
static void nop(ee_ctx*c){(void)c;}
static void zero(ee_ctx*c){c->r[2].uw[0]=0;}
static void one(ee_ctx*c){c->r[2].uw[0]=1;}
static void alloc(ee_ctx*c){c->r[2].uw[0]=TASK;}
static void schedule(ee_ctx*c){(void)c;jobs++;queued=1;}
static void world(ee_ctx*c){if(++steps==length)word(c,SCENE+0xce0,1);}
static void close_scene(ee_ctx*c){(void)c;closes++;}
static void reset_scene(ee_ctx*c){
 if(c->r[4].uw[0]!=SCENE || c->r[5].uw[0]!=2 || c->r[6].uw[0]!=0)abort();
 resets++;steps=0;word(c,SCENE+0xce0,0);
}
static ee_fn boundary(uint32_t pc){switch(pc){
 case 0xf10000:return world;case 0xf10004:return nop;case 0xf10008:return close_scene;
 case 0x387ef0:return reset_scene;case 0x5750c0:return alloc;case 0x5788b8:return schedule;
 case 0x426a08:return zero;case 0x105818:return one;
 case 0x2638d8:case 0x2638e0:case 0x548228:case 0x576100:case 0x576140:
 case 0x4a5418:case 0x4a5348:case 0x4a53f8:case 0x12bfe0:case 0x12c0c0:
 case 0x1056a0:case 0x4ab040:case 0x4a1638:case 0x4a2808:case 0x4aa1d8:
 case 0x4a6140:case 0x4a5688:case 0x4a5400:return nop;
 default:return NULL;}}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
static int call(ee_ctx*c,unsigned pc){
 memset(c->r,0,sizeof c->r);c->r[4].uw[0]=MENU;c->r[6].uw[0]=1;c->r[29].uw[0]=0x1ef0000;
 jmp_buf jump;c->exit_jmp=&jump;if(setjmp(jump)){fprintf(stderr,"original %08x %s\n",c->end_pc,c->end_detail);return 0;}
 recomp_lookup(pc)(c);return 1;
}
int main(int argc,char**argv){
 tick=0;CHECK(argc==2);ee_ctx e;CHECK(!ee_init(&e));CHECK(ee_load_image(&e,argv[1],0x100000)>0x5a3900);
 e.lookup=lookup;e.hook=boundary;word(&e,0x618840,SCENE);word(&e,SCENE+0x64,VT);word(&e,SCENE+0xcc8,2);
 word(&e,VT+0x2c,0xf10000);word(&e,VT+0x3c,0xf10004);word(&e,VT+0x234,0xf10004);word(&e,VT+0x2c4,0xf10008);
 unsigned cases=0;const unsigned lengths[]={1,17,1471,3750};
 for(unsigned l=0;l<4;l++)for(unsigned delay=0;delay<3;delay++){
  gt4_license_menu_cycle c={0};memset(e.ram+MENU+0xa0,0,sizeof c);word(&e,SCENE+0xce0,0);
  steps=jobs=closes=resets=0;queued=0;length=lengths[l];unsigned native_steps=0,pending=0,completed=0;
  for(tick=0;completed<3 && tick<20000;tick++){
   if(queued && pending++>=delay*4){CHECK(call(&e,0x12b888));c.job=0;c.ready=1;queued=0;pending=0;native_steps=0;completed++;}
   unsigned old_jobs=jobs;CHECK(call(&e,0x12b928));int request=gt4_license_menu_cycle_update(&c);
   CHECK((unsigned)request==jobs-old_jobs);
   if(!c.paused && !c.phase)native_steps++;
   CHECK(call(&e,0x12bc18));gt4_license_menu_cycle_draw(&c,native_steps>=length);
   if(getenv("GT4_LICENSE_MENU_CYCLE_NEGATIVE") && tick==length+1)c.alpha+=.01f;
   CHECK(!memcmp(&c,e.ram+MENU+0xa0,sizeof c));CHECK(native_steps==steps);
   CHECK(closes==resets && resets==completed);cases++;
  }CHECK(completed==3 && jobs==3);
 }
 printf("PASS %u original menu ticks: end, 1s close, two black draws, delayed reconstruction and 0.5s open; Scene/GS/scheduler are boundaries.\n",cases);
 ee_free(&e);return 0;
}
