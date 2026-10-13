#include "model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void w32(uint8_t *p,unsigned v){for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static void w16(uint8_t *p,unsigned v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void wf(uint8_t *p,float v){memcpy(p,&v,4);}
int main(void)
{
    uint8_t car[1088]={0};memcpy(car,"CAR4",4);w32(car+24,64);uint8_t *d=car+64;
    memcpy(d,"MDLS",4);w32(d+16,1024);d[22]=d[24]=d[28]=d[30]=1;
    w32(d+56,128);w32(d+60,168);w32(d+68,172);w32(d+152,512);
    w32(d+168,192);w32(d+172,176);w32(d+176,400);memcpy(d+400,"Tex1",4);
    d[200]=1;d[202]=1;w32(d+208,32);d[212]=5;
    d[225]=0xc0;d[226]=3;d[227]=0x68;wf(d+240,1);wf(d+256,1);
    d[264]=0x40;d[265]=0xc0;d[266]=3;d[267]=0x64;
    d[292]=0x40;d[293]=0xc0;d[294]=2;d[295]=0x72;d[297]=3;
    uint8_t *c=d+512;
    for(int i=0;i<4;i++) {
        *c++=12;*c++=6;w16(c,11+i);c+=2;*c++=2;
        uint8_t *e0=c,*e1=c+2;c+=4;*c++=8;uint8_t *skip=c++;
        w16(e0,(unsigned)(c-e0));*c++=4;*c++=0;*c++=8;uint8_t *exit=c++;
        w16(e1,(unsigned)(c-e1));*c++=4;*c++=0;*c++=4;*c++=0;
        *skip=(uint8_t)(c-skip);*exit=(uint8_t)(c-exit);
        *c++=13;*c++=4;*c++=0;
    }
    *c=0;
    gt4_model *m=gt4_model_decode(car,sizeof car,0);CHECK(m && m->count==8);
    for(int i=0;i<4;i++){CHECK(m->triangles[i*2].attachment==i+1);CHECK(!m->triangles[i*2+1].attachment);}
    gt4_model *combined=calloc(1,sizeof *combined);const float pose[1][6]={{1,2,3,-1,1,1}};
    CHECK(gt4_model_attach(combined,m,pose,1));CHECK(combined->triangles[6].attachment==4);
    gt4_model_free(combined);gt4_model_free(m);
    m=gt4_model_decode_part(d,1024,0,0);CHECK(m && m->count==4);
    for(int i=0;i<4;i++)CHECK(!m->triangles[i].attachment);gt4_model_free(m);
    d[517]=1;d[518]=0;CHECK(!gt4_model_decode(car,sizeof car,0));
    puts("model: wheel visibility branches, attachment frames, stack restoration and bounds passed");return 0;
}
