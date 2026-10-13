/* 00114498 versus native result processing. Presentation, record lookup,
   sorted-rank/prize access and calendar insertion are explicit boundaries.
   Calendar call arguments are captured, never filled by the native port. */
#include "ee.h"
#include "career_original.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"event result line %d: %s\n",__LINE__,#x);return 1;}}while(0)
#define GAME 0x1800000u
#define WORLD 0x1840000u
#define RACE 0x1841000u
#define ENTRIES 0x1842000u
#define SLOTS 0x1843000u
#define ENTRY 0x1844000u
#define CAR 0x1850000u
#define RECORD 0x1851000u
#define QUICK 0x1852000u
#define BEST 0x1853000u
static int32_t rank,aspec;
static uint32_t lap,prize,calendar[3],calendar_calls;
static int default_history;
static void noop(ee_ctx *c){(void)c;}
static void label(ee_ctx *c){c->r[2].ud[0]=0x1854000;}
static void record(ee_ctx *c){c->r[2].ud[0]=RECORD;}
static void best_lap(ee_ctx *c){c->r[2].ud[0]=lap;}
static void position(ee_ctx *c){c->r[2].sd[0]=rank;}
static void race_prize(ee_ctx *c){c->r[2].uw[0]=prize;}
static void score(ee_ctx *c){c->r[2].sd[0]=aspec;}
static void compare(ee_ctx *c){c->r[2].ud[0]=!default_history;}
static void history(ee_ctx *c){calendar[0]=c->r[6].uw[0];calendar[1]=c->r[7].uw[0];calendar[2]=c->r[8].uw[0];calendar_calls++;}
static void fallback_history(ee_ctx *c){calendar[0]=c->r[7].uw[0];calendar[1]=c->r[8].uw[0];calendar[2]=c->r[9].uw[0];calendar_calls++;}
static ee_fn boundary(uint32_t pc){switch(pc){
 case 0x114390:case 0x3b6fd0:case 0x110898:return noop;
 case 0x447280:return label;case 0x430d20:return record;case 0x35e3f0:return best_lap;
 case 0x110570:return position;case 0x4475a8:return race_prize;case 0x3c0ff0:return score;
 case 0x57f238:return compare;case 0x42f270:return history;case 0x42f448:return fallback_history;default:return NULL;}}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
static void w32(ee_ctx *c,uint32_t at,uint32_t v){memcpy(c->ram+at,&v,4);}
static uint32_t r32(ee_ctx *c,uint32_t at){uint32_t v;memcpy(&v,c->ram+at,4);return v;}
static uint32_t next(uint32_t *seed){*seed=*seed*1664525u+1013904223u;return *seed;}
int main(int argc,char **argv){
 CHECK(argc==2);ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0x54f3f8);c.lookup=lookup;c.hook=boundary;
 w32(&c,0x622f4c,GAME);w32(&c,WORLD+0x6c,RACE);w32(&c,RACE+0x60,ENTRIES);
 w32(&c,ENTRIES+8,SLOTS);w32(&c,SLOTS,ENTRY);w32(&c,ENTRY+0x18,CAR);
 uint32_t seed=0x20261003;
 for(unsigned test=0;test<8192;test++){
  gt4_career_event_result native;memset(&native,0xa7,sizeof native);
  for(unsigned i=0;i<sizeof native.statistics;i++)native.statistics[i]=(uint8_t)(next(&seed)>>24);
  native.record=next(&seed);native.previous_best=(uint8_t)next(&seed);
  rank=(int32_t)(test%7);aspec=(int32_t)(next(&seed)%1024)-256;
  lap=next(&seed);prize=next(&seed);int finished=(test/7)%2,mode=(test/14)%4;
  if(test<8)prize=(uint32_t[]){0,1,0x7fffffff,0x80000000,0xffffffff,100,0xffff0000,0x10000}[test];
  memcpy(c.ram+GAME+0x348,native.statistics,sizeof native.statistics);w32(&c,RECORD,native.record);
  c.ram[BEST]=native.previous_best;c.ram[CAR+0x5b6]=(uint8_t)finished;w32(&c,WORLD+0xcc8,mode);
  /* Force old work prizes too: an abandoned result does not overwrite
     this field; its initialization to zero belongs to the entry builder. */
  native.awarded_prize=next(&seed);w32(&c,QUICK+0x4a4,native.awarded_prize);
  calendar_calls=0;default_history=(test/56)%2;
  memset(c.r,0,sizeof c.r);c.r[4].uw[0]=QUICK;c.r[5].uw[0]=WORLD;c.r[6].uw[0]=BEST;c.r[29].uw[0]=0x1ef0000;
  jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));recomp_lookup(0x114498)(&c);
  gt4_career_process_event_result(&native,finished,rank,lap,mode,aspec,prize);
  if(getenv("GT4_CAREER_EVENT_RESULT_NEGATIVE") && !test)native.statistics[0x54]^=1;
  CHECK(!memcmp(native.statistics,c.ram+GAME+0x348,sizeof native.statistics));
  CHECK(native.record==r32(&c,RECORD) && native.previous_best==c.ram[BEST]);
  CHECK(native.awarded_prize==r32(&c,QUICK+0x4a4));
  CHECK(calendar_calls==1 && native.calendar_grade==calendar[0] && native.calendar_method==calendar[1] && native.calendar_time==calendar[2]);
 }
 puts("PASS event result: 8192 original finished/abandoned/rank/mode/prize/record/statistics/history cases at explicit boundaries; credits are paid later by the prize menu.");
 ee_free(&c);return 0;
}
