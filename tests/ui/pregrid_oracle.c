/* Test-only original EE scalar functions against native REP4 evaluation.
   White-box inclusion also verifies the original lens/roll spring boundary. */
#include "ee.h"
#include "../../src/core/course_pregrid.c"
#include <fenv.h>
#include <stdio.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"pregrid oracle line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void tangent(ee_ctx *c){c->fpr[0].f=tanf(c->fpr[12].f);}
static ee_fn boundary(uint32_t pc){return pc==0x57d380?tangent:NULL;}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
static void invoke(ee_ctx *c,uint32_t pc)
{jmp_buf j;c->exit_jmp=&j;CHECK(!setjmp(j));CHECK(c->lookup(pc));c->lookup(pc)(c);}
static void putf(ee_ctx *c,uint32_t at,float v){memcpy(c->ram+at,&v,4);}
int main(int argc,char **argv)
{
    CHECK(argc==3 || argc==4);ee_ctx c;CHECK(!ee_init(&c));c.lookup=lookup;c.hook=boundary;
    CHECK(ee_load_image(&c,argv[1],0x100000)>0x5a48f0);
    FILE *f=fopen(argv[2],"rb");CHECK(f);CHECK(!fseek(f,0,SEEK_END));long bytes=ftell(f);rewind(f);
    uint8_t *course=malloc(bytes);CHECK(course && fread(course,1,bytes,f)==(size_t)bytes);fclose(f);
    gt4_pregrid *p=gt4_pregrid_open(course,bytes);CHECK(p && p->count);
    const uint32_t object=0x1800000,stack=0x1ef0000;unsigned curves=0,times=0;
    int rounding=fegetround();CHECK(!fesetround(FE_TOWARDZERO));
    for(unsigned shot=0;shot<p->count;shot++){
        const uint8_t *e=p->entries+shot*0x7c;float seconds=f32(e+8);
        for(int frame=-1;frame<=(int)(seconds*60)+2;frame++){
            int done;float t=gt4_pregrid_time(seconds,frame,&done);
            putf(&c,object+8,seconds);c.r[4].uw[0]=object;c.r[5].sw[0]=frame;c.r[6].uw[0]=object+0x80;c.r[29].uw[0]=stack;
            invoke(&c,0x3fc9a0);CHECK(!memcmp(&t,&c.fpr[0].f,4) && done==(int)ee_r32(&c,object+0x80));times++;
            for(unsigned field=0x34;field<=0x74;field+=4){
                if(field>=0x5c && field<=0x60)continue;
                uint32_t off=u32(e+field);if(!off)continue;
                unsigned size=4+u32(p->data+off)*8;CHECK(size<0x10000);memcpy(c.ram+object,p->data+off,size);
                c.r[4].uw[0]=object;c.r[29].uw[0]=stack;c.fpr[12].f=t;
                invoke(&c,0x3ddbc8);float value=gt4_pregrid_curve(p->data+off,t);
                if(argc==4 && !curves)value+=1; /* intentional wrong authored coordinate */
                CHECK(!memcmp(&value,&c.fpr[0].f,4));curves++;
            }
        }
    }
    for(int i=1;i<1000;i++){
        float deg=i*.1f,sx,sy;putf(&c,object+0xc,deg);c.r[4].uw[0]=object;c.r[29].uw[0]=stack;
        invoke(&c,0x3794a8);gt4_pregrid_projection(deg,&sx,&sy);
        CHECK(!memcmp(&sx,c.ram+object+0x1c,4) && !memcmp(&sy,c.ram+object+0x24,4));
    }
    for(int k=0;k<2;k++){
        float state[9];spring_init(state,k?0:35,k?100:300,k?-20:-40,150,k?100:150);
        memcpy(c.ram+object,state,sizeof state);
        for(int frame=0;frame<2000;frame++){
            float value=(float)((frame*7)%99),actual;
            c.r[4].uw[0]=object;c.fpr[12].f=value;c.r[29].uw[0]=stack;invoke(&c,0x385098);
            actual=spring(state,value);CHECK(!memcmp(&actual,c.ram+object,4));CHECK(!memcmp(state,c.ram+object,sizeof state));
        }
    }
    /* 00375980 wraps entry index, resets frame, and samples frame 1 immediately. */
    for(unsigned cycle=0;cycle<2;cycle++)for(unsigned shot=0;shot<p->count;shot++){
        float duration=f32(p->entries+shot*0x7c+8);unsigned frames=(unsigned)(duration*60);
        for(unsigned frame=1;frame<=frames;frame++){gt4_pregrid_camera out;CHECK(gt4_pregrid_step(p,&out));CHECK(out.shot==shot && out.frame==frame);}
    }
    gt4_pregrid_reset(p);gt4_pregrid_camera zero,nearest;
    CHECK(gt4_pregrid_step(p,&zero));gt4_pregrid_reset(p);CHECK(!fesetround(FE_TONEAREST));
    CHECK(gt4_pregrid_step(p,&nearest));CHECK(fegetround()==FE_TONEAREST && !memcmp(&zero,&nearest,sizeof zero));
    CHECK(!fesetround(rounding));
    uint8_t saved=course[0xc0];course[0xc0]=0xff;CHECK(!gt4_pregrid_open(course,0xc4));course[0xc0]=saved;
    printf("PASS %u REP4 curve values and %u time/completion values bit-identical; 999 projections at the tan boundary; 4000 spring states; two shot cycles.\n",curves,times);
    gt4_pregrid_close(p);free(course);ee_free(&c);return 0;
}
