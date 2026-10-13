/* Tex1 +08/+10/+18: authored MXL and MIPTBP levels.
   Distinct solid colors prove these are read from VRAM, not downsampled. */
#include "tex1.h"
#include "model.h"
#include "course.h"
#include "vol.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"mipmap line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void word(unsigned char *p,unsigned n){for(int i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
static void quad(unsigned char *p,unsigned long long n){word(p,(unsigned)n);word(p+4,(unsigned)(n>>32));}
typedef struct {float point[9];unsigned triangle;} overlap;
static int point_compare(const void *a,const void *b){return memcmp(a,b,12);}
static int overlap_compare(const void *a,const void *b){return memcmp(((const overlap*)a)->point,((const overlap*)b)->point,36);}
int main(int argc,char **argv){
 unsigned char data[512]={0};memcpy(data,"Tex1",4);data[20]=1;data[22]=4;word(data+24,48);word(data+28,88);
 quad(data+48,(1ull<<14)|(3ull<<26)|(3ull<<30));
 quad(data+56,(3ull<<2)|(1ull<<5)|(5ull<<6));
 quad(data+64,32ull|(1ull<<14)|(64ull<<20)|(1ull<<34)|(96ull<<40)|(1ull<<54));
 unsigned offset=136;
 const unsigned char colors[4][4]={{255,0,0,128},{0,255,0,64},{0,0,255,32},{127,63,31,16}};
 for(unsigned level=0;level<4;level++){
  unsigned side=8>>level;unsigned char *tr=data+88+12*level;
  word(tr,offset);tr[4]=(unsigned char)(level*32);tr[6]=1;tr[8]=tr[10]=(unsigned char)side;
  for(unsigned i=0;i<side*side;i++)memcpy(data+offset+i*4,colors[level],4);
  offset+=side*side*4;
 }
 tex1_image *im=NULL;CHECK(tex1_decode(data,offset,&im)==1);CHECK(im->mip_count==3 && im->minify==5);
 if(argc>1 && !strcmp(argv[1],"--negative"))im->mip_rgba[1][0]^=1;
 for(unsigned level=1;level<4;level++)for(unsigned i=0;i<(8u>>level)*(8u>>level);i++)CHECK(!memcmp(im->mip_rgba[level-1]+i*4,colors[level],4));
 tex1_free(im,1);
 for(unsigned n=0;n<offset;n++){im=NULL;CHECK(tex1_decode(data,n,&im)==-1);}
 quad(data+56,7ull<<2);CHECK(tex1_decode(data,offset,&im)==-1);
 if(argc>1 && strcmp(argv[1],"--negative")) {
  const char *error=NULL;gt4_vol *vol=gt4_vol_open(argv[1],&error);CHECK(vol);
  size_t size;unsigned char *bytes=gt4_vol_read(vol,"/crs/tsukuba",&size);gt4_course course;
  CHECK(bytes && gt4_course_open(&course,bytes,size));
  gt4_model *world=gt4_model_decode_world(bytes+course.world,size-course.world,NULL);CHECK(world);
  unsigned textures=0,levels=0;
  for(int i=0;i<world->ntextures;i++)if(world->textures[i].mip_count){textures++;levels+=world->textures[i].mip_count;
   for(unsigned level=0;level<world->textures[i].mip_count;level++)CHECK(world->textures[i].mip_rgba[level]);
  }
  CHECK(textures>0 && levels>0);printf("Tsukuba: %u authored mip textures, %u levels retained\n",textures,levels);
  overlap *sorted=malloc((size_t)world->count*sizeof *sorted);CHECK(sorted);
  for(int i=0;i<world->count;i++){for(int v=0;v<3;v++)memcpy(sorted[i].point+v*3,world->triangles[i].v[v].p,12);qsort(sorted[i].point,3,12,point_compare);sorted[i].triangle=(unsigned)i;}
  qsort(sorted,world->count,sizeof *sorted,overlap_compare);unsigned overlaps=0;
  for(int i=1;i<world->count;i++)if(!overlap_compare(sorted+i-1,sorted+i)){
   const gt4_triangle *a=world->triangles+sorted[i-1].triangle,*b=world->triangles+sorted[i].triangle;
   if(a->texture>=0 && b->texture>=0 && a->texture!=b->texture && a->kind==b->kind && !a->blend_enabled && !b->blend_enabled){
    if(overlaps++<30)printf("overlap triangles %u/%u textures %d/%d mip %u/%u pos %.2f %.2f %.2f\n",sorted[i-1].triangle,sorted[i].triangle,a->texture,b->texture,world->textures[a->texture].mip_count,world->textures[b->texture].mip_count,a->v[0].p[0],a->v[0].p[1],a->v[0].p[2]);
   }
  }
  printf("opaque distinct-texture exact overlap pairs: %u\n",overlaps);
  gt4_course_vision vision;unsigned at=(unsigned)bytes[8]|(unsigned)bytes[9]<<8|(unsigned)bytes[10]<<16|(unsigned)bytes[11]<<24;
  CHECK(gt4_course_vision_open(&vision,bytes+at,size-at,world->source_models));
  unsigned total_nodes=(unsigned)vision.data[48]|(unsigned)vision.data[49]<<8|(unsigned)vision.data[50]<<16|(unsigned)vision.data[51]<<24;
  unsigned table=(unsigned)vision.data[52]|(unsigned)vision.data[53]<<8|(unsigned)vision.data[54]<<16|(unsigned)vision.data[55]<<24,maximum=0;
  unsigned char *active=calloc(world->source_models,1);CHECK(active);
  for(unsigned node=0;node<total_nodes;node++) {
   float pos[3];memcpy(pos,vision.data+table+node*16+4,12);const unsigned char *list=gt4_course_vision_select(&vision,pos,NULL);CHECK(list);
   memset(active,0,world->source_models);unsigned count=(unsigned)list[0]|(unsigned)list[1]<<8;
   for(unsigned j=0;j<count;j++)active[(unsigned)list[2+j*2]|(unsigned)list[3+j*2]<<8]=1;
   unsigned remaining=0,cross=0;
   for(int i=1;i<world->count;i++)if(!overlap_compare(sorted+i-1,sorted+i)) {
    const gt4_triangle *a=world->triangles+sorted[i-1].triangle,*b=world->triangles+sorted[i].triangle;
    if(a->texture>=0 && b->texture>=0 && a->texture!=b->texture && a->kind==b->kind && !a->blend_enabled && !b->blend_enabled && active[a->source_model] && active[b->source_model]){remaining++;if(a->source_model!=b->source_model)cross++;}
   }
   if(remaining>maximum){maximum=remaining;printf("node %u remaining %u cross-model %u\n",node,remaining,cross);}
  }
  printf("maximum selected opaque overlap across %u nodes: %u\n",total_nodes,maximum);free(active);free(sorted);
  gt4_model_free(world);free(bytes);gt4_vol_close(vol);
 }
 puts("authored mipmap levels: passed");return 0;
}
