/* RaceTraining initial scene against a user-provided EE snapshot.
   No dynamics correction: this isolates disc preparation and native routing. */
#include "career_license_disc.h"
#include "car.h"
#include "license_session.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"training capture line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t word(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
static float real(const uint8_t *p){float v;memcpy(&v,p,4);return v;}
static uint32_t player;
static int pad(void *ctx,uint32_t car,uint8_t *command,const uint8_t *event){
 (void)ctx;if(car!=player)return 0;memset(command,0,32);command[1]=1;command[3]=event[3];return 1;
}
static int wheel(void *ctx,uint32_t car,int16_t out[2]){(void)ctx;out[0]=out[1]=0;return car==player;}
static int vibration(void *ctx,uint32_t car,const uint8_t *data){(void)ctx;(void)data;return car==player;}
int main(int argc,char **argv){
 CHECK(argc==4 || (argc==5 && !strcmp(argv[4],"--trajectory")));
 int trajectory=argc==5;uint8_t *ref=malloc(0x2000000);CHECK(ref);FILE *f=fopen(argv[3],"rb");CHECK(f);
 CHECK(fread(ref,1,0x2000000,f)==0x2000000);fclose(f);uint32_t cond=0;
 for(uint32_t p=0x100000;p<0x2000000-0x10144;p+=4)
  if(word(ref+p+0x10140)==GT4_TRAINING_SESSION_VT){CHECK(!cond);cond=p;}
 CHECK(cond);uint32_t race=word(ref+cond),entries=word(ref+race+0x60),list=word(ref+entries+8);
 gt4_career_race_lookup_data lookup;CHECK(gt4_career_race_lookup_data_load(argv[1],&lookup));
 gt4_career_license_exam exam;char error[256];CHECK(gt4_career_license_prepare(&exam,argv[1],&lookup,argv[2],error,sizeof error));
 CHECK(exam.desc.license==2 && exam.desc.cars==(int)word(ref+entries));
 uint32_t first=word(ref+list);exam.desc.manual=word(ref+first+0x3448)!=0;
 exam.desc.assist_asm=word(ref+first+0x3450)!=0;exam.desc.assist_tcs=word(ref+first+0x3454)!=0;
 gt4_race_run *run=gt4_race_run_open(argv[1],&exam.desc,error,sizeof error);if(!run){fprintf(stderr,"%s\n",error);return 1;}
 uint32_t c=gt4_race_run_conductor(run),r=GT4_U32(c),l=GT4_U32(GT4_U32(r+0x60)+8);
 unsigned differences=0;
 for(int i=0;i<exam.desc.cars;i++){
  uint32_t e=GT4_U32(l+i*4),original=word(ref+list+i*4);
  CHECK(GT4_U32(e+0x3444)==word(ref+original+0x3444));
  CHECK(GT4_U32(e+0x198)==word(ref+original+0x198));
  for(unsigned k=0;k<0x178;k++){
   /* 00444480/00444690 and their default setters do not initialize
      structure padding. Snapshot contains prior stack bytes there.
      00444860 writes 12A,139/13A; 00445220 writes 15B/15C. */
   if((k>=0x14 && k<0x18) || (k>=0x19 && k<0x20) ||
      k==0x12b || k==0x13b || k==0x15d)continue;
   uint8_t actual=GT4_U8(e+0x20+k),expected=ref[original+0x20+k];
   if(getenv("GT4_TRAINING_CAPTURE_NEGATIVE") && !i && !k)actual^=1;
   if(actual!=expected){fprintf(stderr,"setup %d +%03x native %02x original %02x\n",i,k,actual,expected);differences++;}
  }
  for(unsigned k=0x3448;k<=0x3458;k+=4){
   if(GT4_U32(e+k)!=word(ref+original+k))fprintf(stderr,"entry %d +%x native %u original %u\n",i,k,GT4_U32(e+k),word(ref+original+k));
   CHECK(GT4_U32(e+k)==word(ref+original+k));
  }
 }
 for(unsigned k=0;k<0x80;k++)if(GT4_U8(c+0xf818+k)!=ref[cond+0xf818+k]){
  fprintf(stderr,"rules +%02x native %02x original %02x\n",k,GT4_U8(c+0xf818+k),ref[cond+0xf818+k]);differences++;
 }
 uint32_t ticks=word(ref+cond+0xf898)/50;CHECK(ticks<600);
 player=gt4_race_run_car(run,0);gt4_phys_pad.read=pad;gt4_phys_pad.wheel_state=wheel;gt4_phys_pad.write=vibration;
 for(uint32_t n=0;n<ticks;n++)CHECK(gt4_race_run_frame(run));
 for(int i=0;i<exam.desc.cars;i++){
  uint32_t car=gt4_race_run_car(run,i),e=word(ref+list+i*4),oc=word(ref+e+0x18);
  for(unsigned k=0;k<3;k++){
   float actual=GT4_F32(car+0x894+k*4),expected=real(ref+oc+0x894+k*4);
   printf("car %d axis %u tick %u native %.9g original %.9g delta %.9g\n",i,k,ticks,actual,expected,fabsf(actual-expected));
   if(fabsf(actual-expected)>.001f){
    fprintf(stderr,"A: position exceeds 1 mm at car %d axis %u; dynamics unchanged\n",i,k);
    if(trajectory)differences++;
   }
  }
 }
 gt4_race_run_close(run);free(ref);CHECK(!differences);
 puts("PASS Training snapshot setup/rules/options, ignoring uninitialized padding. Positions reported separately; no full trajectory claim.");return 0;
}
