/* Daihatsu MIRA TR-XX Avanzato R '97 /car/lod/daih0002.
   Compare the combined native stream with each authored callback0 path
   independently selected in its branch table, bypassing lamp tagging.
   00455708 control flow, 00453698 callback0; MDLS +7C1/+7CB/+7EB/+7FD. */
#include "model.h"
#include "vol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"brake branch line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static unsigned u32(const unsigned char *p){return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
static unsigned u16(const unsigned char *p){return p[0]|(unsigned)p[1]<<8;}
static void put16(unsigned char *p,unsigned x){p[0]=(unsigned char)x;p[1]=(unsigned char)(x>>8);}
int main(int argc,char **argv){
 CHECK(argc>=2);gt4_vol *vol=gt4_vol_open(argv[1],NULL);CHECK(vol);size_t size;unsigned char *data=gt4_vol_read(vol,"/car/lod/daih0002",&size);CHECK(data);
 unsigned base=u32(data+24);CHECK(base+0x7ff<size);unsigned char *fork=data+base+0x7c1;CHECK(fork[0]==6 && !u16(fork+1) && fork[3]==2);
 unsigned released=4+u16(fork+4),pressed=6+u16(fork+6);CHECK(released==10 && pressed==42);
 gt4_model *combined=gt4_model_decode(data,size,0);CHECK(combined);unsigned shared=0;
 for(int i=0;i<combined->count;i++){shared+=!combined->triangles[i].lamp;if(argc>2 && !strcmp(argv[2],"--negative") && !combined->triangles[i].lamp)combined->triangles[i].lamp=1;}
 CHECK(shared>0);unsigned char *copy=malloc(size);CHECK(copy);
 for(unsigned brake=0;brake<2;brake++){
  memcpy(copy,data,size);unsigned target=brake?pressed:released;
  /* Both entries target the same authored branch, so the decoder runs
     just that original path with no dual-branch lamp bookkeeping. */
  put16(copy+base+0x7c1+4,target-4);put16(copy+base+0x7c1+6,target-6);
  gt4_model *reference=gt4_model_decode(copy,size,0);CHECK(reference);unsigned at=0;
  for(int i=0;i<combined->count;i++){
   const gt4_triangle *triangle=combined->triangles+i;
   if(triangle->lamp && triangle->lamp!=1+brake)continue;
   CHECK(at<(unsigned)reference->count);gt4_triangle actual=*triangle;actual.lamp=0;
   const gt4_triangle *expected=reference->triangles+at;
   CHECK(!memcmp(actual.v,expected->v,sizeof actual.v));
   CHECK(actual.texture==expected->texture && actual.kind==expected->kind && actual.blend_enabled==expected->blend_enabled);
   CHECK(!memcmp(&actual.state,&expected->state,sizeof actual.state));
   CHECK(actual.source_model==expected->source_model && actual.wheel_blur_texture==expected->wheel_blur_texture && actual.attachment==expected->attachment && expected->lamp==0);at++;
  }
  CHECK(at==(unsigned)reference->count);printf("brake%u: %u triangles match authored branch, including state/vertices; shared %u\n",brake,at,shared);gt4_model_free(reference);
 }
 free(copy);gt4_model_free(combined);free(data);gt4_vol_close(vol);puts("PASS MIRA body retained in both brake states; negative tags body as released only");return 0;
}
