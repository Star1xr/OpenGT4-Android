/* Original CORE functions, generated only for this test. Input fixtures
   cover boundaries, exemptions and lazy globals; no assets are embedded. */
#include "ee.h"
#include "fpu.h"
#include "career_original.h"
#include <stdlib.h>
#include <string.h>
#define WEAR 0x1800000u
#define PARAMS 0x620320u
#define SETTINGS 0x620cd8u
#define CHECK(x) do{if(!(x)){fprintf(stderr,"service value case %u line %d: %s\n",test,__LINE__,#x);return 1;}}while(0)
ee_fn recomp_lookup(uint32_t);
static void w32(uint8_t *p,uint32_t x){memcpy(p,&x,4);}
static void wf(uint8_t *p,float x){memcpy(p,&x,4);}
static uint32_t bits(float x){uint32_t b;memcpy(&b,&x,4);return b;}
static uint32_t rng(uint32_t *s){*s=*s*1664525u+1013904223u;return *s;}
static int call(ee_ctx *c,uint32_t pc,uint32_t input){
 memset(c->r,0,sizeof c->r);c->r[4].uw[0]=input;c->r[29].uw[0]=0x1ef0000;
 jmp_buf j;c->exit_jmp=&j;ee_fpu_enter();if(setjmp(j)){ee_fpu_leave();fprintf(stderr,"original: %s\n",c->end_detail);return 0;}
 ee_fn fn=recomp_lookup(pc);if(!fn){ee_fpu_leave();return 0;}fn(c);ee_fpu_leave();return 1;
}
int main(int argc,char **argv){unsigned test=0;CHECK(argc==2);ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0x54f3f8);c.lookup=recomp_lookup;
 uint8_t initial[36],settings[0xd9],dirt[12];memcpy(initial,c.ram+PARAMS,sizeof initial);memcpy(settings,c.ram+SETTINGS,sizeof settings);memcpy(dirt,c.ram+0x620af0,12);
 CHECK(call(&c,0x3660d8,0x620b00));uint32_t seed=0x12345678;unsigned checks=0;
 for(test=0;test<4096;test++){
  uint8_t wear[32]={0};wf(wear+0x10,(float)((int32_t)(rng(&seed)%80000001)-10000000));wear[0x14]=test<256?(uint8_t)test:test%7==0?1:0;
  memcpy(c.ram+WEAR,wear,32);CHECK(call(&c,0x343a80,WEAR));uint32_t value=bits(gt4_career_dirtiness(wear,dirt));if(getenv("GT4_SERVICE_dirt_NEGATIVE"))value^=1;
  if(value!=c.fpr[0].u)fprintf(stderr,"dirt %08x/%08x\n",value,c.fpr[0].u);CHECK(value==c.fpr[0].u);checks++;
  /* Getter reads distance, setter writes the separate visual coefficient. */
  float change=(float)((int32_t)(rng(&seed)%24001)-12000)/10000;memcpy(c.ram+WEAR,wear,32);c.fpr[12].u=bits(change);CHECK(call(&c,0x3438a0,WEAR));gt4_career_set_dirtiness(wear,change);
  if(getenv("GT4_SERVICE_setter_NEGATIVE"))wear[12]^=1;CHECK(!memcmp(wear,c.ram+WEAR,32));checks++;
  uint8_t native_params[36],native_settings[0xd9];memcpy(native_params,initial,36);memcpy(native_settings,settings,sizeof settings);
  if(test%2){native_settings[0xcc]=1;wf(native_params+20,-(float)(rng(&seed)%5000000));wf(native_params+28,test%17?(float)(rng(&seed)%25000000):0);}
  else{native_settings[0xcc]=0;for(int i=0xd0;i<=0xd8;i++)native_settings[i]=(uint8_t)rng(&seed);}
  wf(wear+8,(float)((int32_t)(rng(&seed)%80000001)-40000000));
  memcpy(c.ram+WEAR,wear,32);memcpy(c.ram+PARAMS,native_params,36);memcpy(c.ram+SETTINGS,native_settings,sizeof settings);
  CHECK(call(&c,0x3437f0,WEAR));value=bits(gt4_career_oil_life(wear,native_params,native_settings));if(getenv("GT4_SERVICE_oil_NEGATIVE"))value^=1;
  if(value!=c.fpr[0].u)fprintf(stderr,"oil %08x/%08x\n",value,c.fpr[0].u);CHECK(value==c.fpr[0].u);CHECK(!memcmp(native_params,c.ram+PARAMS,36) && !memcmp(native_settings,c.ram+SETTINGS,sizeof settings));checks++;
 }
 ee_free(&c);printf("PASS %u exact original dirt/oil values and setters, exemptions and lazy globals\n",checks);return 0;
}
