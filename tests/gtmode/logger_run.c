/* The user's extracted executable is the oracle. No game bytes are embedded. */
#include "ee.h"
#include "fpu.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define BANK 0x1800000u
#define SAMPLER 0x1850000u
#define CAR 0x1860000u
#define CHECK(x) do{if(!(x)){fprintf(stderr,"logger line %d scenario %u tick %u: %s\n",__LINE__,scenario,tick,#x);return 1;}}while(0)
static unsigned scenario,tick;
static uint32_t seed=0x986fc65u;
static uint32_t random_word(void){seed=seed*1664525u+1013904223u;return seed;}
static void put(ee_ctx*c,uint32_t a,uint32_t v){memcpy(c->ram+a,&v,4);}
static void pf(ee_ctx*c,uint32_t a,float v){memcpy(c->ram+a,&v,4);}
static uint32_t word(ee_ctx*c,uint32_t a){uint32_t v;memcpy(&v,c->ram+a,4);return v;}
static int call(ee_ctx*c,uint32_t pc,uint32_t object,uint32_t a1,uint32_t a2,uint32_t a3,float f12,float f13){
 memset(c->r,0,sizeof c->r);c->r[4].uw[0]=object;c->r[5].uw[0]=a1;c->r[6].uw[0]=a2;c->r[7].uw[0]=a3;
 c->r[29].uw[0]=0x1ef0000;c->fpr[12].f=f12;c->fpr[13].f=f13;
 jmp_buf jump;c->exit_jmp=&jump;if(setjmp(jump)){fprintf(stderr,"original %08x %s\n",c->end_pc,c->end_detail);return 0;}
 recomp_lookup(pc)(c);return 1;
}
static int spacing_original(ee_ctx *c,float length,float *result){
 /* Execute the original bounded arithmetic block, without the replay worker.
    The generated function intentionally stops at the first instruction outside
    00123050..00123078. Any other exception is an oracle failure. */
 memset(c->r,0,sizeof c->r);c->r[21].uw[0]=BANK;
 c->fpr[21].f=length;c->fpr[1].f=0.5f;jmp_buf jump;c->exit_jmp=&jump;
 if(!setjmp(jump)){recomp_lookup(0x123050)(c);return 0;}
 if(c->end_pc!=0x12307c || strcmp(c->end_detail,"fell off end of function"))return 0;
 memcpy(result,c->ram+BANK+0x40008,4);return 1;
}
int main(int argc,char **argv){
 CHECK(argc==2);CHECK(sizeof(gt4_logger_point)==8);ee_ctx c;CHECK(!ee_init(&c));
 CHECK(ee_load_image(&c,argv[1],0x100000)>0x344850-0x100000);c.lookup=recomp_lookup;ee_fpu_enter();
 gt4_logger_point *points=malloc(GT4_LOGGER_POINTS*sizeof *points);CHECK(points);
 unsigned feeds=0,lookups=0,packed=0;
 for(scenario=0;scenario<36;scenario++){
  float length=scenario%3==0?412.6f:scenario%3==1?20832.8f:50000.125f;
  float spacing=gt4_logger_spacing(length);
  float expected;CHECK(spacing_original(&c,length,&expected));CHECK(!memcmp(&spacing,&expected,4));
  memset(points,0xa5,GT4_LOGGER_POINTS*sizeof *points);memset(c.ram+BANK,0xa5,0x4000c);
  memset(c.ram+SAMPLER,0,0x50);gt4_logger_sampler s={0};
  CHECK(call(&c,0x11e400,SAMPLER,BANK,BANK+0x40000,CAR,spacing,length));
  gt4_logger_begin(&s,points,spacing,length);pf(&c,BANK+0x40008,spacing);
  float distance=-3.f;
  for(tick=0;tick<850;tick++){
   uint8_t *car=c.ram+CAR;memset(car,0,0x10a0);
   float advance=(float)(random_word()%1200)*0.003f;
   if(tick%23==0)advance=-2.25f;if(tick%31==0)advance=0;
   distance+=advance;
   if(tick==849 && scenario%6==0)distance=length+spacing*20000.f;
   int16_t lap=(int16_t)(distance/length)+1;float local=distance-(float)(lap-1)*length;
   memcpy(car+0x5b0,&lap,2);pf(&c,CAR+0x5a8,local);
   pf(&c,CAR+0x6c8,(float)(random_word()%100000)*0.003f);
   pf(&c,CAR+0x58c,(float)(random_word()%1200)*0.001f);
   pf(&c,CAR+0x590,(float)(random_word()%1200)*0.001f);car[0x624]=(uint8_t)random_word();
   pf(&c,CAR+0x554,(float)(random_word()%1000)*0.001f);
   const unsigned xyz[]={0x8a4,0x888,0x8ac,0x890};
   for(unsigned j=0;j<4;j++)pf(&c,CAR+xyz[j],(float)((int)(random_word()%100000)-50000)*0.1f);
   gt4_logger_input v;gt4_logger_car_input(&v,car);
   gt4_logger_sample(&s,&v);CHECK(call(&c,0x11e450,SAMPLER,0,0,0,0,0));feeds++;
   CHECK(s.count==word(&c,BANK+0x40000));
   CHECK(!memcmp(&s.origin,c.ram+SAMPLER,12*sizeof(float)));
   CHECK(!memcmp(&s.spacing,c.ram+SAMPLER+0x38,3*sizeof(float)));
   CHECK(s.first==(int)word(&c,SAMPLER+0x48));
   if(getenv("GT4_LOGGER_NEGATIVE") && s.count)points[0].speed_gear^=8;
   CHECK(!memcmp(points,c.ram+BANK,GT4_LOGGER_POINTS*sizeof *points));
   if(s.count){
    CHECK(call(&c,0x11dbb0,BANK,0,0,0,0,0));float native=gt4_logger_length(&s);CHECK(!memcmp(&native,&c.fpr[0].f,4));
    float probes[]={-10.f,0.f,spacing*0.9f,spacing*1.6f,spacing*123.75f,gt4_logger_length(&s)+10.f};
    for(unsigned j=0;j<6;j++){
     CHECK(call(&c,0x11dbe8,BANK,0,0,0,probes[j],0));native=gt4_logger_speed_at(&s,probes[j]);CHECK(!memcmp(&native,&c.fpr[0].f,4));
     CHECK(call(&c,0x11dc80,BANK,0,0,0,probes[j],0));native=gt4_logger_x_at(&s,probes[j]);CHECK(!memcmp(&native,&c.fpr[0].f,4));
     CHECK(call(&c,0x11dd10,BANK,0,0,0,probes[j],0));native=gt4_logger_z_at(&s,probes[j]);CHECK(!memcmp(&native,&c.fpr[0].f,4));lookups+=3;
    }
   }
  }
  if(scenario%6==0)CHECK(s.count==GT4_LOGGER_POINTS);packed+=s.count;
 }
 free(points);ee_free(&c);ee_fpu_leave();printf("PASS original Logger: %u car inputs, %u packed points, %u distance lookups and 36 spacings, exact bytes/floats. Replay reconstruction and empty-track lookups are boundaries.\n",feeds,packed,lookups);return 0;
}
