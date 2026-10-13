/* Original 00453BB0/00453A58 draw graph. GS/resource calls are explicit
   recorded boundaries; sin/cos are an input boundary, not PS2 math proof. */
#include "ee.h"
#include "wheel_motion.h"
#include <fenv.h>
#include <stdio.h>
#include <stdlib.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"accum oracle line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static gt4_wheel_accum_plan plan;
static gt4_wheel_accum_draw recorded;
static unsigned draws,copies,height_calls,filters,masks;
static int inside,width,negative;
static void no_op(ee_ctx *c){(void)c;}
static void set_width(ee_ctx *c){CHECK((int)c->r[5].uw[0]==plan.size);}
static void set_height(ee_ctx *c){CHECK((int)c->r[5].uw[0]==plan.size*(height_calls++?1:3));}
static void format(ee_ctx *c){CHECK((int)c->r[5].uw[0]==width);}
static void filter(ee_ctx *c){CHECK(c->r[4].uw[0]==(filters++?2:3));CHECK(c->r[5].uw[0]==1);}
static void mask(ee_ctx *c){CHECK(c->r[4].uw[0]==(masks++%2?0:3));}
static void sine(ee_ctx *c){float s=sinf(c->fpr[12].f),co=cosf(c->fpr[12].f);memcpy(c->ram+c->r[4].uw[0],&s,4);memcpy(c->ram+c->r[5].uw[0],&co,4);}
static void polygon(ee_ctx *c)
{
    CHECK(c->r[4].uw[0]==4 && c->r[7].uw[0]==0);
    float xy[8],uv[8]={0,0,recorded.source_size,0,recorded.source_size,recorded.source_size,0,recorded.source_size};
    gt4_wheel_accum_quad(&recorded,xy);
    for(int i=0;i<8;i++) {
        float original;memcpy(&original,c->ram+c->r[5].uw[0]+4*i,4);CHECK(fabsf(original-xy[i])<=.00002f);
        CHECK(!memcmp(c->ram+c->r[6].uw[0]+4*i,&uv[i],4));
    }
}
static void rotation(ee_ctx *c)
{
    CHECK(draws<(unsigned)plan.stages*2);
    recorded=(gt4_wheel_accum_draw){c->fpr[12].f,c->fpr[13].f,c->fpr[14].f,c->fpr[15].f};
    gt4_wheel_accum_draw expected=plan.draw[draws];if(negative && !draws)expected.offset+=1;
    CHECK(!memcmp(&recorded,&expected,sizeof expected));draws++;
    inside=1;recomp_lookup(0x453a58)(c);inside=0;
}
static void reduction(ee_ctx *c)
{
    float s=(float)plan.size,expected[8]={0,0,s,s,0,s,s,3*s};
    for(int i=0;i<8;i++)CHECK(!memcmp(&c->fpr[12+i].f,&expected[i],4));
    CHECK(draws==2*(copies+1));copies++;
}
static ee_fn boundary(uint32_t pc)
{
    switch(pc) {
    case 0x453a58:return inside?NULL:rotation;
    case 0x57d8a0:return sine;case 0x49b540:return polygon;case 0x4a6668:return reduction;
    case 0x105440:return set_width;case 0x105450:return set_height;case 0x1055c0:return format;
    case 0x4a4550:return filter;case 0x4a5fe0:return mask;
    case 0x4a5418:case 0x105528:case 0x1056a0:case 0x499728:case 0x49cc48:
    case 0x4ab040:case 0x4a29a8:case 0x4aa1d8:case 0x105728:case 0x4a4910:
    case 0x4a5688:case 0x4a1638:return no_op;
    default:return NULL;
    }
}
static ee_fn lookup(uint32_t pc){ee_fn h=boundary(pc);return h?h:recomp_lookup(pc);}
int main(int argc,char **argv)
{
    CHECK(argc==2 || (argc==3 && !strcmp(argv[2],"--negative")));negative=argc==3;
    ee_ctx c;CHECK(!ee_init(&c));c.lookup=lookup;c.hook=boundary;CHECK(ee_load_image(&c,argv[1],0x100000)>0x5a48f0);
    ee_w32(&c,0x1801000+0x18,0x1802000);int rounding=fegetround();CHECK(!fesetround(FE_TOWARDZERO));
    const int samples[]={2,3,4,5,8,16,17,32};unsigned cases=0;
    for(int exponent=0;exponent<=10;exponent++)for(int m=0;m<8;m++)for(int a=0;a<32;a++) {
        width=1<<exponent;float angle=a*(gt4_wheel_constant(0x40490fda)/8);
        plan=gt4_wheel_accumulation(width,samples[m],angle);draws=copies=height_calls=filters=masks=0;
        ee_w64(&c,0x1802000,(uint64_t)exponent<<26);memset(c.r,0,sizeof c.r);
        c.r[4].uw[0]=0x1800000;c.r[5].uw[0]=0x1801000;c.r[6].uw[0]=samples[m];c.r[29].uw[0]=0x1ef0000;c.fpr[12].f=angle;
        jmp_buf j;c.exit_jmp=&j;if(setjmp(j)){fprintf(stderr,"%s\n",c.end_detail);return 1;}
        recomp_lookup(0x453bb0)(&c);
        CHECK(draws==(unsigned)plan.stages*2 && copies==(unsigned)plan.stages && masks==draws && height_calls==2 && filters==2);cases++;
    }
    CHECK(!fesetround(rounding));ee_free(&c);
    printf("PASS %u original accumulation graphs E; rotated XY C within 0.00002 (shared libm boundary).\n",cases);return 0;
}
