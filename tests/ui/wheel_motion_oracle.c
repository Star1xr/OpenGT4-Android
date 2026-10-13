/* Original scalar wheel exposure and WheelContext bank selector. The generated
   EE functions are test-only; model/GS binding calls are recorded boundaries. */
#include "ee.h"
#include "wheel_motion.h"
#include "wheel_geometry.h"
#include <fenv.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"wheel oracle line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned selected,bindings;
static void bank(ee_ctx *c){selected=c->r[5].uw[0];c->r[2].uw[0]=0x1818000;}
static void reference(ee_ctx *c){ee_w32(c,c->r[4].uw[0]+8,0x1818000);}
static void bind(ee_ctx *c){CHECK(c->r[4].uw[0]==2);bindings++;}
/* Shape iteration is an input boundary. Position math remains the original
   00458AB8 EE function, operating directly on this synthetic position. */
static void shape_input(ee_ctx *c){c->r[2].uw[0]=0x1820000;}
static void iterator_begin(ee_ctx *c){ee_w32(c,c->r[4].uw[0]+0x40,0);}
static void iterator_position(ee_ctx *c){c->r[2].uw[0]=0x1821000;}
static void iterator_next(ee_ctx *c){ee_w32(c,c->r[4].uw[0]+0x40,1);}
static void bounds_output(ee_ctx *c){(void)c;CHECK(0);}
static ee_fn boundary(uint32_t pc)
{return pc==0x4552e8?shape_input:pc==0x494f80?iterator_begin:pc==0x495560?iterator_position:pc==0x494fc8?iterator_next:pc==0x4550b0?bounds_output:pc==0x454ef8?bank:pc==0x107be8?reference:pc==0x4568f0?bind:NULL;}
static ee_fn lookup(uint32_t pc){ee_fn fn=boundary(pc);return fn?fn:recomp_lookup(pc);}
static int invoke(ee_ctx *c,uint32_t pc)
{jmp_buf j;c->exit_jmp=&j;if(setjmp(j)){fprintf(stderr,"%s\n",c->end_detail);return 0;}ee_fn fn=c->lookup(pc);if(!fn)return 0;fn(c);return 1;}
static void putf(ee_ctx *c,uint32_t p,float f){memcpy(c->ram+p,&f,4);}
static uint32_t seed=0x453e68;
static float random_unit(void){seed=seed*1664525u+1013904223u;return (seed>>8)*(1.f/16777216.f);}
int main(int argc,char **argv)
{
    CHECK(argc==2 || (argc==3 && (!strcmp(argv[2],"--negative") || !strcmp(argv[2],"--geometry-negative"))));
    ee_ctx c;CHECK(!ee_init(&c));c.lookup=lookup;c.hook=boundary;
    CHECK(ee_load_image(&c,argv[1],0x100000)>0x5a48f0);
    const uint32_t object=0x1800000,car=0x1801000,course=0x1803000,header=0x1803100,env=0x1804000;
    const uint32_t model=0x1810000,context=0x1811000,car4=0x1812000,ref=0x1813000,stack=0x1ef0000;
    ee_w32(&c,object+4,car);ee_w32(&c,car+8,course);ee_w32(&c,course+4,header);ee_w32(&c,header+0x80,env);
    ee_w32(&c,model,car4);ee_w32(&c,car4+0x20,0x1814000);
    ee_w32(&c,context+4,model);ee_w32(&c,context+8,1);ee_w32(&c,context+0x10,ref);
    int mode=fegetround();CHECK(!fesetround(FE_TOWARDZERO));unsigned cases=0;
    for(unsigned i=0;i<50000;i++) {
        float current=random_unit()*12-6,previous=current+(random_unit()*4-2),t=random_unit(),exposure=random_unit()*2;
        if(i<16){static const float deg[]={0,4.999998f,5,5.000001f,-5,-5.000001f,90,-90};
            current=deg[i%8]/gt4_wheel_constant(0x42652ee0);previous=0;t=0;exposure=i<8?1:0;}
        putf(&c,car+0x188,current);putf(&c,car+0x910,previous);putf(&c,car+0x554,t);putf(&c,env+0xEA0,exposure);
        memset(c.r,0,sizeof c.r);c.r[4].uw[0]=object;c.r[29].uw[0]=stack;
        CHECK(invoke(&c,0x3d73a0));float degrees=gt4_wheel_motion_degrees(current,previous,t,exposure);
        CHECK(!memcmp(&degrees,&c.fpr[0].f,4));
        putf(&c,model+0xB8,c.fpr[0].f);memset(c.r,0,sizeof c.r);
        c.r[4].uw[0]=context;c.r[5].uw[0]=3;c.r[29].uw[0]=stack;
        CHECK(invoke(&c,0x453e68));unsigned predicted=3+gt4_wheel_motion_blurred(degrees);
        if(argc==3 && !strcmp(argv[2],"--negative") && !i)predicted++; /* prove the comparison detects a wrong bank */
        CHECK(predicted==selected);CHECK(c.r[2].uw[0]==0);cases++;
    }
    CHECK(bindings==cases);
    ee_w32(&c,car4+0x2c,0x1822000);c.ram[0x1822000+0x18]=1;
    unsigned geometry=0;
    for(unsigned i=0;i<50000;i++) {
        float bounds[3]={.6f+random_unit()*.3f,1.1f+random_unit()*.4f,.8f+random_unit()*.4f};
        float width=.1f+random_unit()*.3f,radius=.3f+random_unit()*.4f,sidewall=.04f+random_unit()*.1f;
        float p[3]={random_unit()-.5f,random_unit()+.1f,random_unit()-.5f};
        memcpy(c.ram+0x1821000,p,12);memcpy(c.ram+0x1823000,bounds,12);memset(c.r,0,sizeof c.r);
        c.r[4].uw[0]=car4;c.r[5].uw[0]=0;c.r[6].uw[0]=0x1823000;c.r[29].uw[0]=stack;
        c.fpr[12].f=width;c.fpr[13].f=radius;c.fpr[14].f=sidewall;
        CHECK(invoke(&c,0x458ab8));
        gt4_tire_fit_position(p,gt4_tire_fit_prepare(width,radius,sidewall,bounds));
        if(argc==3 && !strcmp(argv[2],"--geometry-negative") && !i)p[1]+=.1f;
        for(int k=0;k<3;k++){float expected;memcpy(&expected,c.ram+0x1821000+k*4,4);CHECK(fabsf(p[k]-expected)<=.000001f);}
        geometry++;
    }
    CHECK(!fesetround(mode));ee_free(&c);
    printf("PASS %u original tire deformation samples within 0.000001.\n",geometry);
    printf("PASS %u original wheel exposure values bit-identical and sharp/blur bank selections.\n",cases);return 0;
}
