/* The Logger's view (logger_view.c) on synthetic tracks: both cut to the
   shorter (00123204), the window fitted at the first draw (00122F1C), the
   cursor stopping at an end and wrapping on a new push (001208BC), the
   speed at the cursor (0011DBE8). --negative expects a wrong length. */
#include "logger_view.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static gt4_logger_point pa[GT4_LOGGER_POINTS],pb[GT4_LOGGER_POINTS];
static int lines,quads,texts;
static void line(void *u,float x0,float y0,float x1,float y1,float w,const uint8_t c[4]){(void)u;(void)x0;(void)y0;(void)x1;(void)y1;(void)w;(void)c;lines++;}
static void quad(void *u,const float xy[4][2],const uint8_t c[4][4]){(void)u;(void)xy;(void)c;quads++;}
static void text(void *u,const char *f,float x,float y,float s,float r,const uint8_t c[4],const char *t){(void)u;(void)f;(void)x;(void)y;(void)s;(void)r;(void)c;(void)t;texts++;}
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#c);return 1;}}while(0)

static void fill(gt4_logger_sampler *s,gt4_logger_point *p,uint32_t n,float kmh)
{
    memset(s,0,sizeof *s);s->points=p;s->count=n;s->spacing=0.5f;s->inverse_spacing=2;s->maximum_speed=kmh/3.6f;
    for(uint32_t i=0;i<n;i++){p[i].speed_gear=(uint16_t)((uint32_t)(kmh/3.6f*40.f+.5f)<<3|3);p[i].accelerator=200;p[i].brake=0;}
}
int main(int argc,char **argv)
{
    int negative=argc>1 && !strcmp(argv[1],"--negative");
    gt4_logger_sampler a,b;fill(&a,pa,2001,120.f);fill(&b,pb,1001,90.f);
    gt4_logger_view v;gt4_logger_view_init(&v,1);
    gt4_logger_view_analyze(&v,&a,&b,1,90000,95000);
    CHECK(v.analyzed && v.present[1]);
    CHECK(v.count[0]==1001 && v.count[1]==1001);          /* cut to the shorter */
    float expect=negative?1000.f:500.f;
    CHECK(fabsf(v.length-expect)<1e-3f);
    gt4_logger_draw_ops ops={NULL,line,quad,text,NULL};
    gt4_logger_view_draw(&v,0,40,640,274,1,&ops);
    CHECK(fabsf(v.zoom-640.f/500.f)<1e-4f);                /* fitted to the widget */
    CHECK(fabsf(v.left)<1e-3f && fabsf(v.right-500.f)<1e-2f);
    CHECK(lines>0 && quads>0 && texts>0);
    CHECK(fabsf(gt4_logger_view_speed(&v,0)-120.f)<0.1f && fabsf(gt4_logger_view_speed(&v,1)-90.f)<0.1f);
    /* the cursor to the right end, held there, wrapping after a release */
    for(int i=0;i<1000;i++)gt4_logger_view_input(&v,1,0,0,0);
    CHECK(v.center==v.length && v.wrap_right==1);
    gt4_logger_view_input(&v,0,0,0,0);CHECK(v.wrap_right==2);
    gt4_logger_view_input(&v,1,0,0,0);CHECK(v.center==0.f);
    /* the right stick's zoom stays in 0.4..10 */
    for(int i=0;i<2000;i++)gt4_logger_view_input(&v,0,0,1,0);
    CHECK(v.zoom<=10.f && v.zoom>9.9f);
    puts("PASS logger view");return 0;
}
