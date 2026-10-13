#include "ee.h"
#include "phys.h"
#include "race_car_sound.h"
#include "driver_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"backfire line %d case %u: %s\n",__LINE__,cases,#x);return 1;}}while(0)
static unsigned cases;
int main(int argc,char **argv)
{
 CHECK(argc==2 || argc==3);ee_ctx e;CHECK(!ee_init(&e));CHECK(ee_load_image(&e,argv[1],0x100000)>0x5a3900);
 gt4_phys_mem=e.ram;gt4_phys_fpu_enter();const uint32_t state=0x1800000,car=0x1810000,param=0x1820000;
 GT4_U32(car+0x10)=param;
 static const uint32_t rates[]={0,0x3dcccccb,0x3dcccccc,0x3dcccccd,0x3e800000,0x3f000000,0x3f800000};
 for(unsigned r=0;r<sizeof rates/sizeof *rates;r++){
  gt4_car_backfire native;gt4_car_backfire_init(&native);memcpy(e.ram+state,&native,sizeof native);GT4_U32(state+4)=car;
  GT4_U32(param+0x10dc)=rates[r];
  for(unsigned t=0;t<2400;t++){
   GT4_U8(car+0x6c4)=(t%19)<8?1:(t%19)<16?0:2;
   uint8_t car_before[0x800];memcpy(car_before,e.ram+car,sizeof car_before);
   float dt=t%3==0?1.f/120:t%3==1?1.f/60:1.f/30;
   memset(e.r,0,sizeof e.r);e.r[4].uw[0]=state;e.r[29].uw[0]=0x1ef0000;e.fpr[12].f=dt;recomp_lookup(0x3d6d30)(&e);
   gt4_car_backfire_step(&native,car,dt);
   if(argc==3 && t==200)native.b[0x64]^=1;
   if(memcmp(native.b+0x10,e.ram+state+0x10,0x5c))for(unsigned i=0x10;i<0x6c;i+=4){uint32_t a,b;memcpy(&a,native.b+i,4);memcpy(&b,e.ram+state+i,4);if(a!=b)fprintf(stderr,"rate%u t%u +%x native%08x EE%08x\n",r,t,i,a,b);}
   CHECK(!memcmp(native.b+0x10,e.ram+state+0x10,0x5c));CHECK(!memcmp(car_before,e.ram+car,sizeof car_before));
   uint32_t count=gt4_car_backfire_take(&native);CHECK(count==GT4_U32(state+0x64));GT4_U32(state+0x64)=0;cases++;
  }
 }
 /* Original relocated CARINFO getter against bounded file metadata. */
 uint8_t asset[0x200]={0};memcpy(asset,"CAR4",4);uint32_t info=0x40,points=0x80;
 memcpy(asset+0x10,&info,4);memcpy(asset+info+0x3c,&points,4);
 for(unsigned n=0;n<=8;n++){
  memcpy(asset+info+0x38,&n,4);GT4_U32(state+0x18)=param;GT4_U32(param+0x10)=car;GT4_U32(car+0x38)=n;
  memset(e.r,0,sizeof e.r);e.r[4].uw[0]=state;e.r[29].uw[0]=0x1ef0000;recomp_lookup(0x390898)(&e);
  CHECK(gt4_car_exhaust_count(asset,sizeof asset)==e.r[2].uw[0]);cases++;
 }
 CHECK(gt4_car_exhaust_count(asset,0x100)==0);
 printf("PASS %u original exhaust pulse frames, particles, local RNG, threshold/edges and read-only physics\n",cases);ee_free(&e);return 0;
}
