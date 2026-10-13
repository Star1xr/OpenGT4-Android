/* Original presentation decisions. World render/physics, result-row text,
   movie allocation/input and fade execution are explicit boundaries. */
#include "ee.h"
#include "race_mission.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"mission flow line %d case %u: %s\n",__LINE__,test,#x);return 1;}}while(0)
static unsigned test;static int returned;
static void noop(ee_ctx *c){c->r[2].ud[0]=0;}
static void finish(ee_ctx *c){(void)c;returned=1;}
static ee_fn hook(uint32_t p){
 if(p==0x3ed188)return finish;
 if(p==0x3dfcc8 || p==0x4751d0 || p==0x3dfe88 || p==0x111000)return noop;
 return NULL;
}
static ee_fn lookup(uint32_t p){ee_fn f=hook(p);return f?f:recomp_lookup(p);}
static int run(ee_ctx *c,uint32_t p,uint32_t a,uint32_t b){
 memset(c->r,0,sizeof c->r);c->r[4].uw[0]=a;c->r[5].uw[0]=b;c->r[29].uw[0]=0x1ef0000;
 jmp_buf jump;c->exit_jmp=&jump;if(setjmp(jump))return 0;lookup(p)(c);return 1;
}
static void entry(ee_ctx *c,uint32_t table,uint32_t off,uint32_t fn){ee_w32(c,table+off,0);ee_w32(c,table+off+4,fn);}
int main(int argc,char **argv){
 CHECK(argc==2);ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0);c.lookup=lookup;c.hook=hook;
 const uint32_t race=0x1500000,world=0x1510000,board=0x1540000,vt=0x1550000,q=race+0xf4;
 ee_w32(&c,world+0x64,vt);ee_w32(&c,world+0x6c,race);
 for(unsigned off=0;off<0x500;off+=8)entry(&c,vt,off,0x111000);entry(&c,vt,0x280,0x3ed188);
 unsigned cases=0;
 for(test=0;test<4096;test++){
  memset(c.ram+q,0,0x830);memset(c.ram+board,0,0x500);
  ee_w32(&c,board+0xc,0x683288);ee_w32(&c,board+0x10,1);ee_w32(&c,board+0x64,UINT32_MAX);
  unsigned n=test%17;ee_w32(&c,q+0x824,n);
  for(unsigned i=0;i<n;i++){
   unsigned type=(test*17+i*7)%27,car=(test+i)%6;
   ee_w16(&c,q+8+i*8,(uint16_t)type);ee_w32(&c,q+12+i*8,car);c.ram[q+0x808+type]++;
  }
  int previous=test&32?GT4_MISSION_BOARD:GT4_MISSION_RUNNING;
  ee_w32(&c,board,previous==GT4_MISSION_BOARD);returned=0;
  int actual=gt4_mission_presentation(c.ram+q,previous);
  CHECK(run(&c,0x3e05a0,board,race));
  CHECK(run(&c,0x389638,world,0));
  int expected=returned?GT4_MISSION_RETURN:ee_r32(&c,board)?GT4_MISSION_BOARD:GT4_MISSION_RUNNING;
  if(getenv("GT4_MISSION_FLOW_NEGATIVE") && !test)actual=GT4_MISSION_BOARD;
  CHECK(actual==expected);
  /* Starting ResultArcade grants exit without waiting for rival finish. */
  if(ee_r32(&c,board) && !previous)CHECK(ee_r32(&c,board+0x4fc)==1);
  cases++;
 }
 ee_free(&c);printf("PASS %u original mission board/failure decisions, rival finishes, event priority and immediate exit permission.\n",cases);return 0;
}
