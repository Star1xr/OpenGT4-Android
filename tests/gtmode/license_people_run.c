/* Original 00408B08. CarInfo and wheel queries supply inputs; record
   004081B8's local position/heading. atan2 is an explicit host boundary. */
#include "ee.h"
#include "license_concourse.h"
#include <math.h>
#include <fenv.h>
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
static float seat_x,seat_z,shift_x,wheels[2][2],recorded[4];static unsigned calls;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"people line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void nop(ee_ctx*c){(void)c;}
static void present(ee_ctx*c){c->r[2].uw[0]=1;}
static void x(ee_ctx*c){c->fpr[0].f=seat_x;}
static void z(ee_ctx*c){c->fpr[0].f=seat_z;}
static void shift(ee_ctx*c){c->fpr[0].f=shift_x;}
static void wx(ee_ctx*c){if(c->r[5].uw[0]>1)abort();c->fpr[0].f=wheels[c->r[5].uw[0]][0];}
static void wz(ee_ctx*c){if(c->r[5].uw[0]>1)abort();c->fpr[0].f=wheels[c->r[5].uw[0]][1];}
static void angle(ee_ctx*c){c->fpr[0].f=atan2f(c->fpr[12].f,c->fpr[13].f);}
static void record(ee_ctx*c){calls++;memcpy(recorded,c->ram+c->r[7].uw[0],12);recorded[3]=c->fpr[12].f;}
static ee_fn boundary(uint32_t p){switch(p){case 0x38db30:return nop;case 0x38dc18:return present;case 0x38dd08:return x;case 0x38dd68:return z;case 0x38e008:return shift;case 0x344aa8:return wx;case 0x344c08:return wz;case 0x57d118:return angle;case 0x4081b8:return record;default:return NULL;}}
static ee_fn lookup(uint32_t p){ee_fn f=boundary(p);return f?f:recomp_lookup(p);}
int main(int argc,char**argv){
 CHECK(argc==2);ee_ctx e;CHECK(!ee_init(&e));CHECK(ee_load_image(&e,argv[1],0x100000)>0x5a3900);e.lookup=lookup;e.hook=boundary;
 unsigned cases=0;fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
 for(int side=-1;side<=1;side++)for(unsigned type=0;type<16;type++){
  seat_x=side*.3f;seat_z=type*.125f-1;shift_x=0;wheels[0][0]=-.6f-type*.01f;wheels[1][0]=.6f+type*.01f;wheels[0][1]=wheels[1][1]=-1.2f-type*.125f;
  float native[3][4];gt4_license_concourse_people(seat_x,seat_z,shift_x,wheels,native);
  if(getenv("GT4_LICENSE_PEOPLE_NEGATIVE"))native[0][0]+=.125f;
  for(unsigned actor=0;actor<3;actor++){
   memset(e.r,0,sizeof e.r);e.r[4].uw[0]=0x1800000;e.r[5].uw[0]=actor;e.r[6].uw[0]=0x1800200;e.r[7].uw[0]=0x1800300;e.r[29].uw[0]=0x1ef0000;
   calls=0;jmp_buf jump;e.exit_jmp=&jump;CHECK(!setjmp(jump));recomp_lookup(0x408b08)(&e);CHECK(calls==1);CHECK(!memcmp(native[actor],recorded,16));cases++;
  }
 }fesetenv(&env);ee_free(&e);printf("PASS %u original actor local placements/headings; CarInfo/wheels and atan2 boundaries.\n",cases);return 0;
}
