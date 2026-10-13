#include "course.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void w32(uint8_t *p,unsigned v){for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static void wf(uint8_t *p,float v){memcpy(p,&v,4);}
int main(void)
{
    uint8_t d[0x400]={0};gt4_course c;
    w32(d+4,0x100);memcpy(d+0x100,"MDLS",4);
    w32(d+0xdc,0x200);memcpy(d+0x200,"RNW4",4);wf(d+0x214,1000);
    d[0x23a]=1;d[0x246]=6;d[0x248]=2;
    w32(d+0x25c,0x100);w32(d+0x274,0x140);w32(d+0x278,0x1c0);
    wf(d+0x300,1);wf(d+0x304,2);wf(d+0x308,1);wf(d+0x30c,.5f);
    const float vertices[6][3]={{0,1,0},{4,5,0},{0,1,4},{0,10,0},{4,10,0},{0,10,4}};
    for(int i=0;i<6;i++)for(int k=0;k<3;k++)wf(d+0x340+i*16+k*4,vertices[i][k]);
    d[0x3c2]=1;d[0x3c4]=2;d[0x3cc]=3;d[0x3ce]=4;d[0x3d0]=5;
    CHECK(gt4_course_open(&c,d,sizeof d));
    gt4_grid g=gt4_course_grid(&c,0);CHECK(g.pos[0]==1 && g.pos[1]==2 && g.heading==.5f);
    float h=-99;CHECK(gt4_course_height(&c,1,1,3,&h) && h==2);
    CHECK(gt4_course_height(&c,1,1,20,&h) && h==10); /* overpass: choose layer below ceiling */
    CHECK(gt4_course_height(&c,0,0,2,&h) && h==1); /* triangle boundary */
    h=99;CHECK(!gt4_course_height(&c,5,5,20,&h) && h==99);
    CHECK(!gt4_course_height(&c,1,1,0,&h));
    CHECK(!gt4_course_height(&c,NAN,1,20,&h));
    CHECK(gt4_course_grid(&c,1).heading==0);
    gt4_course_surface *surface=gt4_course_surface_create(&c);CHECK(surface);
    float normal[3];CHECK(gt4_course_surface_at(surface,1,1,3,&h,normal) && h==2);
    CHECK(fabsf(normal[0]+.70710678f)<1e-6f && fabsf(normal[1]-.70710678f)<1e-6f);
    CHECK(gt4_course_surface_at(surface,1,1,20,&h,normal) && h==10 && normal[1]==1);
    int triangle=-1;
    CHECK(gt4_course_surface_triangle(surface,1,1,3,&h,NULL,&triangle) && h==2 && triangle==0);
    CHECK(gt4_course_surface_triangle(surface,1,1,20,&h,NULL,&triangle) && h==10 && triangle==1);
    h=99;CHECK(!gt4_course_surface_at(surface,-16,0,20,&h,normal) && h==99);
    CHECK(!gt4_course_surface_at(surface,NAN,0,20,&h,normal));
    /* Compare indexed queries against the original brute-force result,
       including triangle edges and layered road ceilings. */
    for(int x=-16;x<=32;x++)for(int z=-16;z<=32;z++)for(int ceiling=0;ceiling<=20;ceiling++) {
        float a=99,b=99;int fa=gt4_course_height(&c,x*.125f,z*.125f,(float)ceiling,&a);
        int fb=gt4_course_surface_at(surface,x*.125f,z*.125f,(float)ceiling,&b,NULL);
        CHECK(fa==fb && a==b);
    }
    gt4_course_surface_free(surface);
    /* Oversized triangles use the bounded fallback chain; queries must also
       cross negative and positive hash cells without losing road edges. */
    const float wide[3][3]={{-100,1,-100},{100,5,-100},{-100,1,100}};
    for(int i=0;i<3;i++)for(int k=0;k<3;k++)wf(d+0x340+i*16+k*4,wide[i][k]);
    surface=gt4_course_surface_create(&c);CHECK(surface);
    for(int x=-120;x<=120;x+=4)for(int z=-120;z<=120;z+=4) {
        float a=99,b=99;int fa=gt4_course_height(&c,(float)x,(float)z,20,&a);
        int fb=gt4_course_surface_at(surface,(float)x,(float)z,20,&b,NULL);CHECK(fa==fb && a==b);
    }
    gt4_course_surface_free(surface);
    for(int i=0;i<6;i++)for(int k=0;k<3;k++)wf(d+0x340+i*16+k*4,vertices[i][k]);
    for(size_t n=0;n<0x3d8;n++)CHECK(!gt4_course_open(&c,d,n));
    d[0x3c2]=6;CHECK(!gt4_course_open(&c,d,sizeof d) && !c.data);d[0x3c2]=1;
    wf(d+0x30c,NAN);CHECK(!gt4_course_open(&c,d,sizeof d));wf(d+0x30c,0);
    wf(d+0x344,INFINITY);CHECK(!gt4_course_open(&c,d,sizeof d));wf(d+0x344,1);
    w32(d+0x274,0xfffffff0);CHECK(!gt4_course_open(&c,d,sizeof d));
    CHECK(!gt4_course_open(&c,NULL,sizeof d));CHECK(!gt4_course_open(NULL,d,sizeof d));
    puts("course: grid, sloped road, overpass and malformed input passed");return 0;
}
