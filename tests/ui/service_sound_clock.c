/* Scalar sequencer and automation oracle. SPU/IOP and voice allocation
   are boundaries; this test does not certify the final waveform. */
#include "ee.h"
#include "fpu.h"
#include "sound_sequence.h"
#include <stdlib.h>
#include <string.h>
#define OBJ 0x1800000u
#define VT 0x1801000u
#define DATA 0x1802000u
#define RAMP 0x1803000u
#define VALUE 0x1803010u
#define CHECK(x) do{if(!(x)){fprintf(stderr,"service sound case %u line %d: %s\n",test,__LINE__,#x);return 1;}}while(0)
ee_fn recomp_lookup(uint32_t);
static unsigned seed=0x20261008,original_events,dispatch_count,current_frame,event_frame[32];
static unsigned delays[32];
static unsigned next(void){seed=seed*1664525u+1013904223u;return seed;}
static void nothing(ee_ctx *c){(void)c;}
static void dispatch(ee_ctx *c){
    unsigned i=dispatch_count++;original_events++;event_frame[i]=current_frame;
    if(i==31){ee_w32(c,OBJ+0x60,3);c->r[2].uw[0]=1;}
    else {float due;memcpy(&due,c->ram+OBJ+0x4c,4);due=gt4_se_clock(due,(float)delays[i]);memcpy(c->ram+OBJ+0x4c,&due,4);c->r[2].uw[0]=0;}
}
static ee_fn boundary(uint32_t pc){return pc==0x100080?dispatch:pc==0x100088?nothing:NULL;}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
static int invoke(ee_ctx *c,uint32_t pc){jmp_buf j;c->exit_jmp=&j;c->r[29].uw[0]=0x1ef0000;if(setjmp(j)){fprintf(stderr,"%s\n",c->end_detail);return 0;}recomp_lookup(pc)(c);return 1;}
static float readf(ee_ctx *c,uint32_t p){float f;memcpy(&f,c->ram+p,4);return f;}
static void writef(ee_ctx *c,uint32_t p,float f){memcpy(c->ram+p,&f,4);}
int main(int argc,char **argv){
    unsigned test=0,checks=0;CHECK(argc==2);ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0x590000);c.lookup=lookup;c.hook=boundary;
    for(test=0;test<512;test++){
        float tempo=(float)(30+next()%211),resolution=(float)(24+next()%937),hz=test%2?60:50;
        writef(&c,OBJ+0x40,resolution);writef(&c,0x654730,hz);c.r[4].uw[0]=OBJ;c.fpr[12].f=tempo;
        ee_fpu_enter();CHECK(invoke(&c,0x55a858));float rate=gt4_se_rate(tempo,resolution,hz);ee_fpu_leave();
        if(getenv("GT4_SE_RATE_NEGATIVE"))rate*=.5f;
        CHECK(!memcmp(&rate,c.ram+OBJ+0x48,4));checks++;
        gt4_se_ramp native={(float)(next()%128),0,(float)(next()%64)/128.f,1};int32_t value=(int)native.target;
        memcpy(c.ram+RAMP,&native,sizeof native);ee_w32(&c,VALUE,(uint32_t)value);
        int ms=1+next()%2001,delta=(int)(next()%257)-128;
        c.r[4].uw[0]=RAMP;c.r[5].sw[0]=ms;c.r[6].sw[0]=value+delta;c.r[7].sw[0]=value;
        ee_fpu_enter();CHECK(invoke(&c,0x55d420));gt4_se_ramp_start(&native,ms,value+delta,value,hz);ee_fpu_leave();
        CHECK(!memcmp(&native,c.ram+RAMP,sizeof native));checks++;
        for(unsigned f=0;f<256;f++){
            c.r[4].uw[0]=RAMP;c.r[5].uw[0]=VALUE;ee_fpu_enter();CHECK(invoke(&c,0x55d470));int changed=gt4_se_ramp_step(&native,&value);ee_fpu_leave();
            if(getenv("GT4_SE_RAMP_NEGATIVE") && !f)value++;
            CHECK(!memcmp(&native,c.ram+RAMP,sizeof native));CHECK(value==(int32_t)ee_r32(&c,VALUE));CHECK(changed==(int)c.r[2].uw[0]);checks++;
        }
    }
    /* Repeated original driver calls, including coincident events, empty
       delays and the 8-tick first frame of the 120 BPM / 240 ppqn case. */
    for(test=0;test<256;test++){
        float elapsed=0,due=0,rate=gt4_se_rate(120,240,60);unsigned emitted=0;
        memset(c.ram+OBJ,0,0x70);ee_w32(&c,OBJ+0x60,2);ee_w32(&c,OBJ+0x64,VT);ee_w32(&c,OBJ+0x58,DATA);
        ee_w32(&c,VT+12,0x100080);ee_w32(&c,VT+20,0x100088);writef(&c,OBJ+0x48,rate);
        original_events=dispatch_count=0;for(unsigned i=0;i<32;i++)delays[i]=next()%65;
        for(unsigned frame=0;frame<300 && emitted<32;frame++){
            current_frame=frame;c.r[4].uw[0]=OBJ;ee_fpu_enter();CHECK(invoke(&c,0x55ab98));
            elapsed=gt4_se_clock(elapsed,rate);
            while(emitted<32 && gt4_se_due(elapsed,due)){unsigned i=emitted++;if(i<31)due=gt4_se_clock(due,(float)delays[i]);}
            ee_fpu_leave();if(getenv("GT4_SE_SCHEDULE_NEGATIVE") && !frame)emitted++;
            CHECK(emitted==original_events);CHECK(elapsed==readf(&c,OBJ+0x44));CHECK(due==readf(&c,OBJ+0x4c));checks++;
        }
        CHECK(emitted==32);
        unsigned ticks=0;for(unsigned i=0;i<32;i++){CHECK(gt4_se_sample_time(ticks)==event_frame[i]*800);ticks+=delays[i];checks++;}
    }
    printf("PASS %u exact sequencer clock, dispatch and pitch-ramp states\n",checks);ee_free(&c);return 0;
}
