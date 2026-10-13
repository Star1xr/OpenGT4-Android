/* 00458008 relocation +004580F0 selection, against native immutable Vls0. */
#include "course.h"
#include "vol.h"
#include "ee.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"vision %d: %s\n",__LINE__,#x);return 1;}}while(0)
extern ee_fn recomp_lookup(uint32_t pc);
static unsigned rd(const unsigned char *p){return (unsigned)p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
static int call(ee_ctx *e,unsigned pc,unsigned a){memset(e->r,0,sizeof e->r);e->r[4].uw[0]=a;e->r[29].uw[0]=0x1eff000;jmp_buf j;e->exit_jmp=&j;if(setjmp(j)){fprintf(stderr,"original stopped %08x\n",e->end_pc);return 0;}recomp_lookup(pc)(e);return 1;}
int main(int argc,char **argv){
 CHECK(argc>=3);ee_ctx e;CHECK(!ee_init(&e));CHECK(ee_load_image(&e,argv[1],0x100000)>0);e.lookup=recomp_lookup;
 gt4_vol *vol=gt4_vol_open(argv[2],NULL);CHECK(vol);const char *courses[]={"tsukuba","ss_brooklyn","nurburgring","laguna","grandvalley","suzuka"};unsigned cases=0;
 for(unsigned c=0;c<sizeof courses/sizeof courses[0];c++){
  char path[128];snprintf(path,sizeof path,"/crs/%s",courses[c]);size_t size;unsigned char *bytes=gt4_vol_read(vol,path,&size);if(!bytes){fprintf(stderr,"course unavailable %s\n",courses[c]);continue;}
  unsigned world=rd(bytes+4),models=(unsigned)bytes[world+22]|(unsigned)bytes[world+23]<<8;
  for(unsigned pass=0;pass<3;pass++){
   unsigned at=rd(bytes+8+pass*4);CHECK(at && at<size);gt4_course_vision vision;CHECK(gt4_course_vision_open(&vision,bytes+at,size-at,models));
   unsigned base=0x1400000;CHECK(size-at<0xb00000);memcpy(e.ram+base,bytes+at,size-at);CHECK(call(&e,0x458008,base));
   unsigned seed=123+pass;unsigned nodes=rd(vision.data+48),table=rd(vision.data+52);
   for(unsigned k=0;k<nodes+512;k++){
    float pos[3];if(k<nodes)memcpy(pos,vision.data+table+k*16+4,12);else for(unsigned j=0;j<3;j++){seed=seed*1664525u+1013904223u;pos[j]=(float)((int)(seed%40000)-20000)/8;}
    unsigned native;const unsigned char *list=gt4_course_vision_select(&vision,pos,&native);CHECK(list);
    e.fpr[12].f=pos[0];e.fpr[13].f=pos[1];e.fpr[14].f=pos[2];CHECK(call(&e,0x4580f0,base));
    unsigned original=e.r[2].uw[0]-base;CHECK(original>=table && (original-table)%16==0);original=(original-table)/16;
    if(argc>3 && !strcmp(argv[3],"--negative") && cases==0)native^=1;
    CHECK(native==original);unsigned ptr=rd(e.ram+e.r[2].uw[0]),count=(unsigned)list[0]|(unsigned)list[1]<<8;
    CHECK(!memcmp(e.ram+ptr,list,2+count*2));cases++;
   }
   printf("%s pass%u: %u nodes, selection/list matched\n",courses[c],pass,nodes);
  }free(bytes);
 }
 CHECK(cases>1000);printf("PASS %u original vision selections/lists, exact\n",cases);gt4_vol_close(vol);ee_free(&e);return 0;
}
