/* Immutable scene CRSS against original 00457F60, without PCSX2/UI. */
#include "ee.h"
#include "vol.h"
#include "photo_mode.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"photo audio line %d: %s\n",__LINE__,#x);return 1;}}while(0)
ee_fn recomp_lookup(uint32_t);
static uint32_t rd(const uint8_t *p){uint32_t n;memcpy(&n,p,4);return n;}
static void wr(uint8_t *p,uint32_t n){memcpy(p,&n,4);}
int main(int argc,char **argv)
{
 CHECK(argc>=3);ee_ctx e;CHECK(!ee_init(&e));CHECK(ee_load_image(&e,argv[2],0x100000)>0);
 e.lookup=recomp_lookup;gt4_vol *v=gt4_vol_open(argv[1],NULL);CHECK(v);
 const char *scenes[]={"brooklyn","forum","fremont","gion","grandcanyon","louisburg","nanzenji","rialt","sagano","sanmarco","shibuya","snow","times","tsukiji","tsumago"};
 int blocks=0;
 for(int scene=0;scene<15;scene++){
  char name[64];snprintf(name,sizeof name,"/crs/ss_%s",scenes[scene]);size_t size;uint8_t *file=gt4_vol_read(v,name,&size);CHECK(file && size>0xd0);
  uint32_t at=rd(file+0x8c);CHECK(at && at<size);const uint8_t *data=file+at;size_t bytes=size-at;CHECK(bytes<0x1000000);
  gt4_photo_location location;CHECK(gt4_photo_location_read(file,size,&location));
  const uint32_t base=0x800000,race=0x1d00000,head=0x1d01000,params=0x1d02000;
  for(int sample=0;sample<8;sample++){
   memcpy(e.ram+base,data,bytes);wr(e.ram+base+4,base);
   for(int g=0;g<5;g++){
    uint32_t list=rd(data+0x14+g*4);wr(e.ram+base+0x14+g*4,base+list);
    for(unsigned i=0;i<rd(data+list);i++){uint32_t sp=rd(data+list+4+i*4);wr(e.ram+base+list+4+i*4,base+sp);wr(e.ram+base+sp+12,0);}
   }
   wr(e.ram+race+0x60,head);wr(e.ram+head+0x14,0);
   float eye[3]={location.camera[0]+sample*3,1.3f,location.camera[2]-sample*2},right[3]={sample&1?0.f:1.f,0,sample&1?1.f:0.f},vel[3]={.1f,0,-.2f};
   memcpy(e.ram+params,eye,12);memcpy(e.ram+params+16,right,12);memcpy(e.ram+params+32,vel,12);
   memset(e.r,0,sizeof e.r);e.r[4].uw[0]=base;e.r[5].uw[0]=params+64;e.r[6].uw[0]=race;e.r[7].uw[0]=params;e.r[8].uw[0]=params+16;e.r[9].uw[0]=params+32;e.r[29].uw[0]=0x1ef0000;
   jmp_buf j;e.exit_jmp=&j;CHECK(!setjmp(j));recomp_lookup(0x457f60)(&e);
   float expected[5][3];memcpy(expected,e.ram+params+64,sizeof expected);
   if(argc>3 && scene==0 && sample==0)expected[0][0]+=.25f;
   for(int g=0;g<5;g++)CHECK(expected[g][0]==0 && expected[g][1]==0 && expected[g][2]==1);
   blocks++;
  }
  free(file);
 }
 CHECK(blocks==120);printf("PASS 15 Photo Travel scenes, 120 original speaker blocks: no authored ambient sound.\n");gt4_vol_close(v);ee_free(&e);return 0;
}
