/* RaceMission's conductor (00685DA0) against the original: the entries it
   does not share with the training's (0x08, 0xa0, 0xc0, 0x110, 0x160..0x180,
   0x190) through the native dispatcher (gt4_session_x) and the recompiled
   methods, from the same state; the whole guest memory compared after each
   case. Car lookup, the race's event/gap queue and the pit exit are native
   on both sides (explicit boundaries with their own proofs). */
#include "ee.h"
#include "car.h"
#include "race_run.h"
#include "license_session.h"
#include "career_license_disc.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"mission session line %d case %u pc %08x: %s\n",__LINE__,test,pc,#x);return 1;}}while(0)
static unsigned test;static uint32_t pc;
static void event(ee_ctx *c){gt4_race_event(c->r[4].uw[0],(int)c->r[5].uw[0],c->r[6].uw[0],(int)c->r[7].uw[0]);}
static void gap(ee_ctx *c){gt4_race_gap(c->r[4].uw[0],c->r[5].uw[0],c->r[6].uw[0]);}
static void pit(ee_ctx *c){c->r[2].ud[0]=(uint32_t)gt4_car_pit_leave(c->r[4].uw[0]);}
static void car_at(ee_ctx *c){c->r[2].ud[0]=c->r[4].uw[0]+0x6698u+c->r[5].uw[0]*CAR_SIZE;}
static ee_fn boundary(uint32_t p){switch(p){
 case 0x35df80:return event;case 0x35e078:return gap;case 0x345228:return pit;
 case 0x34c190:return car_at;default:return NULL;}}
static ee_fn lookup(uint32_t p){ee_fn f=boundary(p);return f?f:recomp_lookup(p);}
static int call(ee_ctx *c,uint32_t a0,uint32_t a1,uint32_t a2,uint32_t a3,float force){
 memset(c->r,0,sizeof c->r);memset(c->fpr,0,sizeof c->fpr);
 c->r[4].uw[0]=a0;c->r[5].uw[0]=a1;c->r[6].uw[0]=a2;c->r[7].uw[0]=a3;
 c->fpr[12].f=force;c->r[29].uw[0]=0x1ef0000;
 jmp_buf jump;c->exit_jmp=&jump;gt4_phys_mem=c->ram;
 if(setjmp(jump)){fprintf(stderr,"original stopped at %08x: %s\n",c->end_pc,c->end_detail);return 0;}
 recomp_lookup(pc)(c);return 1;
}
static uint32_t next(uint32_t *s){*s=*s*1664525u+1013904223u;return *s;}
static uint32_t at(uint32_t c,uint32_t i){return c+0x6698u+i*CAR_SIZE;}
static int same(const uint8_t *a,const uint8_t *b){
 for(uint32_t i=0;i<0x2000000;i+=4096)if(!(i>=0x1ee0000 && i<0x1f00000) && memcmp(a+i,b+i,4096)){   /* the original's own stack aside */
  for(uint32_t k=i;k<i+4096;k++)if(a[k]!=b[k]){fprintf(stderr,"at %x: %02x vs %02x\n",k,a[k],b[k]);break;}
  return 0;}
 return 1;
}
int main(int argc,char **argv){
 CHECK(argc==2);char message[256];gt4_career_race_lookup_data races;CHECK(gt4_career_race_lookup_data_load(argv[1],&races));
 gt4_career_license_exam mission;CHECK(gt4_career_mission_prepare(&mission,argv[1],&races,"l0m0011",message,sizeof message));
 gt4_race_run *race=gt4_race_run_open(argv[1],&mission.desc,message,sizeof message);
 if(!race){fprintf(stderr,"%s\n",message);return 1;}
 uint32_t c=gt4_race_run_conductor(race);CHECK(GT4_U32(c+0x10140)==GT4_MISSION_SESSION_VT);
 uint8_t *native=gt4_phys_mem,*base=malloc(0x2000000);CHECK(base);memcpy(base,native,0x2000000);
 ee_ctx original;CHECK(!ee_init(&original));original.lookup=lookup;original.hook=boundary;
 uint32_t seed=0x20261009;gt4_phys_fpu_enter();
 uint32_t points=gt4_session(c,0x150,0,0,0,0,0,NULL);CHECK(points>0 && points<=64);
 unsigned cases=0;
 for(test=0;test<6144;test++){
  uint32_t kind=test%6,v=0,o=0;
  gt4_phys_mem=native;memcpy(native,base,0x2000000);
  uint32_t cars=1+next(&seed)%6;GT4_U32(c+0xf8a0)=cars;
  for(uint32_t i=0;i<6;i++){
   uint32_t car=at(c,i);
   GT4_U8(car+0x5b6)=(uint8_t)(next(&seed)%3==0);GT4_U8(car+0x5b3)=(uint8_t)(1+next(&seed)%6);
   GT4_U8(car+0x75f)=(uint8_t)(next(&seed)%2);GT4_U32(car+0x718)=next(&seed)%200000;
  }
  GT4_U8(c+0xcbe3)=(uint8_t)next(&seed);GT4_U8(c+0xf81a)=(uint8_t)(next(&seed)%2?5:3);
  GT4_U8(c+0xf866)=(uint8_t)(next(&seed)%2);GT4_U32(c+0xf898)=next(&seed)%900000;
  uint32_t a1=next(&seed)%cars,a2=next(&seed)%300000,a3=0;float force=0;
  switch(kind){
  case 0: /* 0x08: the timing points' new passages and their gaps */
   for(uint32_t p=0;p<points;p++){
    uint8_t n=(uint8_t)(next(&seed)%(cars+1)),first=(uint8_t)(n?next(&seed)%(n+1):0);
    GT4_U8(c+0xc+0xd400+p)=n;GT4_U8(c+0xc+0xd440+p)=first;
    for(uint32_t k=0;k<6;k++){GT4_U8(c+0xc+0xd480+p*6+k)=(uint8_t)(next(&seed)%cars);GT4_U32(c+0xc+0xcde0+p*24+k*4)=next(&seed)%600000;}
   }
   o=0x08;pc=0x3f6e30;break;
  case 1:o=0xa0;pc=0x3f6e50;break;             /* a lap's passage, +0x718 cleared */
  case 2:o=0xc0;pc=0x3f6ee8;break;             /* finish events once */
  case 3:o=0x110;pc=0x3f6ed0;a1=at(c,a1);break; /* the time left */
  case 4:{static const uint32_t off[5]={0x160,0x168,0x170,0x178,0x180},fn[5]={0x3f7010,0x3f7018,0x3f7020,0x3f7028,0x3f7030};
   o=off[test/6%5];pc=fn[test/6%5];break;}
  case 5: /* 0x190: the common contact penalty (0035FCC8) */
   o=0x190;pc=0x3f7038;a1=at(c,a1);a2=next(&seed)%4;a3=next(&seed)%18;force=(float)((int)(next(&seed)%20001)-10000);
   GT4_U8(c+0x10000-0x79e)=(uint8_t)(next(&seed)%4?next(&seed):0);GT4_U8(c+0x10000-0x7a0)=(uint8_t)next(&seed);
   GT4_U8(c+0x10000-0x79f)=(uint8_t)next(&seed);GT4_U16(a1+0x104+0x60e)=(uint16_t)(next(&seed)%3==0);
   GT4_U8(a1+0x104+0x62e)=(uint8_t)(next(&seed)%3==0);GT4_U8(a1+0x104+0x466)=(uint8_t)(next(&seed)%7==0);break;
  }
  memcpy(original.ram,native,0x2000000);
  v=gt4_session_x(c,o,a1,a2,a3,0,0,0,force,NULL);
  CHECK(call(&original,c,a1,a2,a3,force));
  if(getenv("GT4_MISSION_SESSION_NEGATIVE") && test==2)native[c+0xcbe3]^=0x80;
  CHECK(same(native,original.ram));
  if(o==0x110 || o>=0x160 && o<=0x180)CHECK(v==original.r[2].uw[0]);
  cases++;
 }
 free(base);ee_free(&original);gt4_phys_mem=native;gt4_race_run_close(race);
 printf("PASS mission conductor: %u original cases (gaps, lap passage, finish events, time left, flags, contact penalty), whole memory.\n",cases);return 0;
}
