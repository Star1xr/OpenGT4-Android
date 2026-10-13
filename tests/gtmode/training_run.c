/* RaceTraining preparation/dispatch against original instructions.
   Menu invocation, base constructors and world queries are explicit
   boundaries. Dynamic solvers are reused, never calibrated here. */
#include "ee.h"
#include "car.h"
#include "license_session.h"
#include "career_license_disc.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
static unsigned group,number,selected,start_calls;
static uint32_t world;
static const uint8_t *race_row;
static void load_row(ee_ctx *c){memcpy(c->ram+c->r[4].uw[0],race_row,151);}
static void exam_number(ee_ctx *c){c->r[2].uw[0]=number;}
static void exam_group(ee_ctx *c){c->r[2].uw[0]=group+4;}
static void training(ee_ctx *c){selected=2;c->r[2].uw[0]=0;}
static void license(ee_ctx *c){selected=1;c->r[2].uw[0]=0;}
static void noop(ee_ctx *c){(void)c;}
static void world_kind(ee_ctx *c){c->r[2].uw[0]=GT4_U32(world+0xcc8);}
static void replay(ee_ctx *c){c->r[2].uw[0]=0;}
static void behind(ee_ctx *c){gt4_race_behind(c->r[4].uw[0],c->r[5].uw[0],c->r[6].uw[0]);}
static void start(ee_ctx *c){(void)c;start_calls++;}
static ee_fn boundary(uint32_t p){switch(p){
 case 0x4470e8:return load_row;
 case 0x38a3b8:return exam_number;case 0x38a3d0:return exam_group;
 case 0x114e38:return training;case 0x11cb38:return license;
 case 0x3bb340:case 0x3d6268:return noop;
 case 0x3f1760:return world_kind;case 0x3c0e78:return replay;
 case 0x35e8e8:return behind;case 0x34ca20:return start;
 default:return NULL;}}
static ee_fn lookup(uint32_t p){ee_fn f=boundary(p);return f?f:recomp_lookup(p);}
static int call(ee_ctx *c,uint32_t pc,uint32_t a0,uint32_t a1,uint32_t a2,uint32_t a3,uint32_t t0,uint32_t t1){
 memset(c->r,0,sizeof c->r);memset(c->fpr,0,sizeof c->fpr);
 c->r[4].uw[0]=a0;c->r[5].uw[0]=a1;c->r[6].uw[0]=a2;c->r[7].uw[0]=a3;
 c->r[8].uw[0]=t0;c->r[9].uw[0]=t1;c->r[29].uw[0]=0x1ef0000;
 gt4_phys_mem=c->ram;jmp_buf jump;c->exit_jmp=&jump;
 if(setjmp(jump)){fprintf(stderr,"training original %08x %s\n",c->end_pc,c->end_detail);return 0;}
 recomp_lookup(pc)(c);return 1;
}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"training line %d case %u: %s\n",__LINE__,test,#x);return 1;}}while(0)
int main(int argc,char **argv){
 unsigned test=0;if(argc!=2)return 2;ee_ctx original;CHECK(!ee_init(&original));
 original.lookup=lookup;original.hook=boundary;
 gt4_career_race_lookup_data data;CHECK(gt4_career_race_lookup_data_load(argv[1],&data));
 const char *groups[]={"l0b","l0a","lib","lia","l0s"};
 for(group=0;group<5;group++)for(number=1;number<=16;number++,test++){
  char code[32],error[256];snprintf(code,sizeof code,"%s%04u",groups[group],number);
  gt4_career_license_exam exam;CHECK(gt4_career_license_prepare(&exam,argv[1],&data,code,error,sizeof error));
  selected=0;CHECK(call(&original,0x10fbd8,0,0,0,0,0,0));
  int actual=exam.desc.license;if(getenv("GT4_TRAINING_NEGATIVE") && !test)actual=2;
  CHECK(actual==(int)selected);
  /* The descriptor's six-entry limit is from 003D5AB8 +2C. */
  CHECK(call(&original,0x3d5ab8,0x1800000,0,0,0,0,0));
  if(selected==1)GT4_U32(0x1800118)=2; /* 003BDB98 +2C */
  race_row=exam.row;CHECK(call(&original,0x3bb478,0x1800000,0,0,0,0,0));
  CHECK(exam.desc.cars==(int)GT4_U32(0x1800118));
  if(selected==2){
   gt4_race_run *prepared=gt4_race_run_open(argv[1],&exam.desc,error,sizeof error);CHECK(prepared);
   uint8_t *ram=gt4_phys_mem;uint32_t c=gt4_race_run_conductor(prepared),r=GT4_U32(c);
   memcpy(original.ram,ram,0x2000000);
   CHECK(call(&original,0x3ecc70,0x1800000,r,0,0,0,0));
   uint32_t entries=GT4_U32(r+0x60),list=GT4_U32(entries+8);
   for(int i=0;i<exam.desc.cars;i++){
    uint32_t e=GT4_U32(list+i*4),expected=GT4_U32(e+0x3444);
    gt4_phys_mem=ram;CHECK(GT4_U32(e+0x3444)==expected);
    /* The original preparation marks a visible guide, no ghost.
       End-to-end trajectory comparisons remain a separate audit. */
    gt4_phys_mem=original.ram;
   }
   gt4_phys_mem=ram;gt4_race_run_close(prepared);
  }
 }
 gt4_race_desc desc={0};desc.cars=1;desc.car[0]=0x3b;strcpy(desc.course,"tsukuba");char error[256];
 gt4_race_run *run=gt4_race_run_open(argv[1],&desc,error,sizeof error);CHECK(run);
 uint8_t *native=gt4_phys_mem;uint32_t cond=gt4_race_run_conductor(run),race=GT4_U32(cond);
 world=GT4_U32(race+0x84);GT4_U32(cond+0x10140)=GT4_TRAINING_SESSION_VT;
 uint8_t *base=malloc(0x2000000);CHECK(base);memcpy(base,native,0x2000000);
 const uint32_t offs[]={0x30,0x60,0x118,0x120,0x140,0x178};
 const uint32_t pcs[]={0x3f7050,0x3f70d0,0x3f7090,0x3f70a8,0x3f70d8,0x3f70e8};
 for(test=0;test<4096;test++){
  gt4_phys_mem=native;
  for(unsigned i=0;i<6;i++){GT4_U16(cond+0xf86c+i*2)=(uint16_t)(test*65521u+i);GT4_U8(cond+0xf866+i)=(uint8_t)(test+i);}
  for(unsigned i=0;i<6;i++)if((test+i)%5==0)GT4_U8(cond+0xf866+i)=0;
  GT4_U8(cond+0xf83a)=(uint8_t)test;
  GT4_U8(cond+0xf818+0x4e)=(uint8_t)(test>>4);GT4_U8(cond+0xcbdd)=(uint8_t)test;
  memcpy(original.ram,native,0x2000000);
  for(unsigned j=0;j<6;j++){
   gt4_phys_mem=native;uint32_t value=gt4_session(cond,offs[j],test%6,0,0,0,0,NULL);
   CHECK(call(&original,pcs[j],cond,test%6,0,0,0,0));
   if(getenv("GT4_TRAINING_GETTER_NEGATIVE") && !test && !j)value++;
   CHECK(value==original.r[2].uw[0]);
  }
  /* Training start-control delegates to existing 0034CE48: compare
     four output words, profiles, all world kinds and replay mode 4. */
  gt4_phys_mem=native;memcpy(native,base,0x2000000);
  uint32_t entry=GT4_U32(GT4_U32(GT4_U32(race+0x60)+8));uint32_t profile=GT4_U32(entry+0x343c);
  GT4_U32(profile+0x1cc)=test%2;GT4_U32(entry+0x3448)=test%7;GT4_U32(world+0xcc8)=test%6;
  GT4_U8(cond+0xcbdc)=test%5;memset(native+0x1e30000,0xa5,16);
  memcpy(original.ram,native,0x2000000);
  gt4_session(cond,0xe8,0,0x1e30000,0x1e30004,0x1e30008,0x1e3000c,NULL);
  CHECK(call(&original,0x3f7078,cond,0,0x1e30000,0x1e30004,0x1e30008,0x1e3000c));
  if(getenv("GT4_TRAINING_START_NEGATIVE") && !test)native[0x1e30008]^=1;
  CHECK(!memcmp(native+0x1e30000,original.ram+0x1e30000,16));
 }
 gt4_phys_mem=native;gt4_race_run_close(run);free(base);ee_free(&original);
 puts("PASS 80 original class selections/counts and 28672 training getter/start-control comparisons. Dynamics unchanged.");return 0;
}
