#include "model.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void w32(uint8_t *p,unsigned v){for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static void wf(uint8_t *p,float v){memcpy(p,&v,4);}
static void point(const float m[16],const float p[3],float out[3])
{for(int r=0;r<3;r++)out[r]=m[r]*p[0]+m[4+r]*p[1]+m[8+r]*p[2]+m[12+r];}
static void near3(const float a[3],const float b[3])
{for(int k=0;k<3;k++)CHECK(fabsf(a[k]-b[k])<2e-6f);}
int main(void)
{
    /* Minimal authored CAR4: tilted steering draw, independent body draw,
       and another steering pivot. No original asset is part of this test. */
    uint8_t car[1024]={0},*d=car+128;memcpy(car,"CAR4",4);w32(car+24,128);
    memcpy(d,"MDLS",4);w32(d+16,sizeof car-128);d[22]=d[24]=d[28]=d[30]=1;
    w32(d+56,128);w32(d+60,168);w32(d+68,172);w32(d+128+24,512);
    w32(d+168,192);w32(d+172,176);w32(d+176,400);memcpy(d+400,"Tex1",4);
    d[200]=1;d[202]=1;w32(d+208,32);d[212]=5;
    d[225]=0xc0;d[226]=3;d[227]=0x68;wf(d+240,1);wf(d+256,1);
    d[264]=0x40;d[265]=0xc0;d[266]=3;d[267]=0x64;
    d[292]=0x40;d[293]=0xc0;d[294]=2;d[295]=0x72;d[297]=3;
    uint8_t *c=d+512;*c++=43;*c++=0;*c++=12;
    *c++=17;wf(c,2);wf(c+4,3);wf(c+8,4);c+=12;
    *c++=20;wf(c,90);c+=4;
    *c++=6;*c++=8;*c++=0;*c++=1;*c++=2;*c++=0; /* original return 0 branch */
    *c++=4;*c++=0;*c++=13;*c++=4;*c++=0;
    *c++=12;*c++=17;wf(c,7);wf(c+4,8);wf(c+8,9);c+=12;
    *c++=6;*c++=8;*c++=0;*c++=0;uint8_t *last_draw=c;*c++=4;*c++=0;*c++=13;*c=0;
    gt4_model *m=gt4_model_decode(car,sizeof car,0);
    CHECK(m && m->count==3 && m->steering_count==2);
    CHECK(m->triangles[0].attachment==5 && !m->triangles[1].attachment && m->triangles[2].attachment==6);
    const float body_x[3]={1,0,0};near3(m->triangles[1].v[1].p,body_x);
    gt4_model *part=gt4_model_steering_part(m,0);CHECK(part && part->count==1 && !part->textures);
    float matrix[16],out[3];CHECK(gt4_model_steering_matrix(m,0,0,matrix));
    for(int j=0;j<3;j++){point(matrix,part->triangles[0].v[j].p,out);near3(out,m->triangles[0].v[j].p);}
    CHECK(gt4_model_steering_matrix(m,0,1.5707963267948966f,matrix));
    point(matrix,part->triangles[0].v[1].p,out);const float tilted_x[3]={2,3,5};near3(out,tilted_x);
    point(matrix,part->triangles[0].v[2].p,out);const float tilted_y[3]={1,3,4};near3(out,tilted_y);
    near3(matrix+12,(const float[3]){2,3,4});near3(m->triangles[1].v[1].p,body_x);
    float saved[16];memcpy(saved,matrix,sizeof saved);
    CHECK(!gt4_model_steering_matrix(m,0,NAN,matrix) && !memcmp(matrix,saved,sizeof saved));
    CHECK(!gt4_model_steering_matrix(m,0,FLT_MAX,matrix) && !memcmp(matrix,saved,sizeof saved));
    CHECK(!gt4_model_steering_matrix(m,2,0,matrix) && !gt4_model_steering_part(m,2));
    CHECK(gt4_model_steering_matrix(m,1,-1.5707963267948966f,matrix));
    gt4_model *second=gt4_model_steering_part(m,1);CHECK(second && second->count==1);
    point(matrix,second->triangles[0].v[1].p,out);near3(out,(const float[3]){7,7,9});
    gt4_model_free(second);gt4_model_free(part);gt4_model_free(m);
    /* A LoadMatrix replaces the callback turn, even in its enclosing push. */
    c=last_draw;*c++=15;
    for(int k=0;k<16;k++){wf(c,k%5==0?1.f:0.f);c+=4;}
    *c++=4;*c++=0;*c++=13;*c=0;
    m=gt4_model_decode(car,sizeof car,0);CHECK(m && m->count==3);
    CHECK(!m->triangles[2].attachment && !gt4_model_steering_part(m,1));
    near3(m->triangles[2].v[1].p,body_x);gt4_model_free(m);
    puts("steering: callback branch, tilted pivots, neutral reconstruction and body isolation passed");return 0;
}
