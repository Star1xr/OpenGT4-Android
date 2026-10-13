/* Mission stop timing against the original world, including its zero-duration
   alternate path. Cleanup services and scheduler are explicit boundaries. */
#include "ee.h"
#include "race_mission.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"mission exit line %d case %u: %s\n",__LINE__,test,#x);return 1;}}while(0)
static unsigned test,advanced,actions[8],action_count;
static void noop(ee_ctx *c){c->r[2].ud[0]=0;}
static void step(ee_ctx *c){advanced++;c->r[2].ud[0]=1;}
static void cleanup(ee_ctx *c){(void)c;actions[action_count++]=1;}
static void monitor(ee_ctx *c){(void)c;actions[action_count++]=2;}
static void release(ee_ctx *c){(void)c;actions[action_count++]=3;}
static void clock_reset(ee_ctx *c){(void)c;actions[action_count++]=4;}
static void wake(ee_ctx *c){(void)c;actions[action_count++]=5;}
static ee_fn hook(uint32_t p){
 if(p==0x389d98 || p==0x3b70f0 || p==0x111000 || p==0x3d8a60)return noop;
 if(p==0x389758)return step;
 if(p==0x33c3e8)return cleanup;
 if(p==0x111040)return monitor;
 if(p==0x389e88)return release;
 if(p==0x574f90)return clock_reset;
 if(p==0x1012c8)return wake;
 return NULL;
}
static ee_fn lookup(uint32_t p){ee_fn f=hook(p);return f?f:recomp_lookup(p);}
static int run(ee_ctx *c,uint32_t p,uint32_t world){
 memset(c->r,0,sizeof c->r);c->r[4].uw[0]=world;c->r[29].uw[0]=0x1ef0000;
 jmp_buf jump;c->exit_jmp=&jump;if(setjmp(jump))return 0;lookup(p)(c);return 1;
}
static void entry(ee_ctx *c,uint32_t table,uint32_t off,uint32_t fn){ee_w32(c,table+off,0);ee_w32(c,table+off+4,fn);}
int main(int argc,char **argv){
 CHECK(argc==2);ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0);c.lookup=lookup;c.hook=hook;
 const uint32_t world=0x1500000,vt=0x1550000,monitor_object=0x1560000,monitor_vt=0x1570000;
 for(unsigned off=0;off<0x500;off+=8)entry(&c,vt,off,0x111000);
 entry(&c,vt,0x280,0x3ed188);entry(&c,vt,0x2b8,0x38aab0);
 entry(&c,vt,0x2d0,0x33c3e8);entry(&c,vt,0x2b0,0x389e88);entry(&c,monitor_vt,0x40,0x111040);
 for(test=0;test<2048;test++){
  memset(c.ram+world,0,0x22c80);ee_w32(&c,world+0x64,vt);ee_w32(&c,world+0x74,monitor_object);ee_w32(&c,monitor_object+4,monitor_vt);
  unsigned special=test&1,demo=(test>>1)&1,state=(test>>2)%5;
  ee_w32(&c,world+0x125f8,special);ee_w32(&c,world+0xcd0,demo);ee_w32(&c,world+0xce0,state);
  uint32_t fade[4];for(unsigned i=0;i<4;i++){fade[i]=0x3d000000u+test*7919u+i*101u;ee_w32(&c,world+0xcf48+i*4,fade[i]);}
  advanced=action_count=0;
  CHECK(run(&c,0x33c490,world));CHECK(advanced==1 && !action_count);
  CHECK(run(&c,0x3ed188,world));
  int native=GT4_MISSION_RETURN;
  if(getenv("GT4_MISSION_EXIT_NEGATIVE") && !test)native=GT4_MISSION_STOPPED;
  CHECK(ee_r32(&c,world+0xd80)==1);
  CHECK((native==GT4_MISSION_STOPPED)==(ee_r32(&c,world+0xd18)!=0));
  CHECK(ee_r32(&c,world+0xce0)==(state?state:(special || demo)?1:0));
  CHECK(!action_count);
  native=gt4_mission_begin_frame(native);
  CHECK(run(&c,0x33c490,world));CHECK(!c.r[2].uw[0]);
  CHECK(native==GT4_MISSION_STOPPED && ee_r32(&c,world+0xd18)==1);
  CHECK(advanced==1 && action_count==5);for(unsigned i=0;i<5;i++)CHECK(actions[i]==i+1);
  for(unsigned i=0;i<4;i++)CHECK(ee_r32(&c,world+0xcf48+i*4)==fade[i]);
 }
 CHECK(gt4_mission_begin_frame(GT4_MISSION_RUNNING)==GT4_MISSION_RUNNING);
 CHECK(gt4_mission_begin_frame(GT4_MISSION_BOARD)==GT4_MISSION_BOARD);
 ee_free(&c);puts("PASS 2048 original mission exits: next tick, no extra race step, cleanup order, zero fade, normal/demo/special paths.");return 0;
}
