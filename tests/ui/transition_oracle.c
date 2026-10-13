/* Original transition maths only; generated oracle never enters the app. */
#include "ee.h"
#include "ui_motion.h"
#include "music.h"
#include <string.h>
#include <math.h>
int plat_audio_play(const int16_t *pcm,size_t frames,int rate,int channels){(void)pcm;(void)frames;(void)rate;(void)channels;return 0;}
ee_fn recomp_lookup(uint32_t pc);
static unsigned render_calls;
static void draw_stub(ee_ctx *c){(void)c;render_calls++;}
static uint32_t original_sequence;
static void sequence_stub(ee_ctx *c){c->r[2].uw[0]=original_sequence;}
static void sound_boundary(ee_ctx *c){(void)c;}
static ee_fn boundary_hook(uint32_t pc){
    if(pc==0x55bec0)return sequence_stub;
    if(pc==0x55bf18 || pc==0x55c020)return sound_boundary;
    return NULL;
}
static ee_fn oracle_lookup(uint32_t pc){
    if(pc==0x1fff000)return draw_stub;
    if(pc==0x55bec0)return sequence_stub;
    if(pc==0x55bf18 || pc==0x55c020)return sound_boundary;
    return recomp_lookup(pc);
}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"transition oracle line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void putf(uint8_t *p,float value){memcpy(p,&value,4);}
static void put32(uint8_t *p,uint32_t value){memcpy(p,&value,4);}
static int invoke(ee_ctx *c,uint32_t pc,int stop){
    jmp_buf jump;c->exit_jmp=&jump;c->end_reason=0;
    if(setjmp(jump))return stop && c->end_reason==EE_END_STOP_PC;
    c->lookup(pc)(c);return !stop;
}
int main(int argc,char **argv){
    CHECK(argc==2);ee_ctx c;CHECK(!ee_init(&c));c.lookup=oracle_lookup;c.hook=boundary_hook;
    FILE *f=fopen(argv[1],"rb");CHECK(f);CHECK(fread(c.ram+0x100000,1,EE_RAM_SIZE-0x100000,f)>0x5994a0);fclose(f);
    unsigned music_cases=0;
    for(int group=0;group<10;group++)for(unsigned sequence=0;sequence<256;sequence++)for(int fade=0;fade<2;fade++){
        uint8_t *table=c.ram+0x6239a8,*manager=c.ram+0x8468d0;
        memset(table,0x5a,120);memset(manager,0,24);
        original_sequence=sequence==255?0xffffffffu:sequence;
        put32(manager,(unsigned)group);put32(manager+20,0x1800000);
        uint8_t expected[120];memcpy(expected,table,120);
        put32(expected+group*12+8,gt4_music_next_sequence(original_sequence));
        memset(c.r,0,sizeof c.r);c.r[4].uw[0]=1;c.r[29].uw[0]=0x1ef0000;c.fpr[12].f=.3f;
        CHECK(invoke(&c,fade?0x460fe0:0x461068,0));
        CHECK(!memcmp(table,expected,120));CHECK(!memcmp(manager+20,"\0\0\0\0",4));music_cases++;
    }
    printf("PASS %u original music resume updates: all 120 group bytes identical.\n",music_cases);
    unsigned loading_cases=0;
    const float durations[]={-2,-1,-.25f,0,.25f,1,2};
    const float steps[]={0,1.0f/120,1.0f/60,1.0f/30,.5f};
    for(unsigned d=0;d<7;d++)for(unsigned t=0;t<5;t++)for(int v=-4;v<=36;v++){
        float period=durations[d],value=v/32.0f;
        for(int tick=0;tick<180;tick++){
            putf(c.ram+0x1800000,period);putf(c.ram+0x1800004,value);
            memset(c.r,0,sizeof c.r);c.r[4].uw[0]=0x1800000;c.fpr[12].f=steps[t];
            CHECK(invoke(&c,0x10ae40,0));
            int done=gt4_loading_fade_step(&period,&value,steps[t]);
            CHECK(c.r[2].uw[0]==(unsigned)done);
            CHECK(!memcmp(c.ram+0x1800000,&period,4));CHECK(!memcmp(c.ram+0x1800004,&value,4));
            loading_cases++;
        }
    }
    printf("PASS %u original loading fade updates: duration, alpha and result identical.\n",loading_cases);
    unsigned ready_cases=0;
    const int32_t flags[]={0,1,-1,(-2147483647-1)};
    for(int stage=-2;stage<=5;stage++)for(int mode=-2;mode<=5;mode++)for(int a=0;a<4;a++) {
        gt4_transition_motion m;gt4_transition_init(&m,1);
        m.stage=stage;m.mode=mode;m.active=flags[a];
        uint8_t *actor=c.ram+0x1800000;memset(actor,0xa5,0x40);
        put32(actor+0x10,(uint32_t)m.active);put32(actor+0x14,(uint32_t)mode);put32(actor+0x18,(uint32_t)stage);
        uint8_t expected[0x40];memcpy(expected,actor,sizeof expected);
        memset(c.r,0,sizeof c.r);c.r[4].uw[0]=0x1800000;
        CHECK(invoke(&c,0x24d328,0));gt4_transition_release(&m);
        put32(expected+0x10,(uint32_t)m.active);put32(expected+0x18,(uint32_t)m.stage);
        CHECK(!memcmp(actor,expected,sizeof expected));ready_cases++;
    }
    printf("PASS %u original page-ready release cases, complete actor bytes identical.\n",ready_cases);
    unsigned tiles=0,updates=0;const float ranges[]={0,.125f,1,2};const float slopes[]={-2,0,2};
    for(int grid=0;grid<3;grid++)for(int range=0;range<4;range++)for(int slope=0;slope<3;slope++) {
        gt4_transition_motion m;gt4_transition_init(&m,2);
        m.columns=grid==0?1:grid==1?8:32;m.rows=grid==0?1:grid==1?6:24;
        m.range=ranges[range];m.slope=slopes[slope];
        for(int repeat=0;repeat<4;repeat++) {
            uint8_t *actor=c.ram+0x1800000;memset(actor,0,0x40);
            put32(actor+0x20,m.random);putf(actor+0x24,m.range);putf(actor+0x28,m.slope);
            put32(actor+0x30,m.columns);put32(actor+0x34,m.rows);put32(actor+0x38,0x1810000);
            memset(c.r,0,sizeof c.r);c.r[4].uw[0]=0x1800000;c.r[29].uw[0]=0x1ef0000;
            CHECK(invoke(&c,0x27aee0,0));gt4_block_setup(&m);CHECK(!memcmp(actor+0x20,&m.random,4));
            for(int k=0;k<m.columns*m.rows;k++) {
                CHECK(!memcmp(c.ram+0x1810000+k*28,&m.tiles[k],28));tiles++;
                for(int frame=0;frame<40;frame++) {
                    memset(c.r,0,sizeof c.r);c.r[4].uw[0]=0x1810000+k*28;c.fpr[12].f=.13333332538604736f;
                    CHECK(invoke(&c,0x27a1b8,0));int result=gt4_block_step(&m.tiles[k],c.fpr[12].f);
                    CHECK(c.r[2].uw[0]==(unsigned)result);CHECK(!memcmp(c.ram+0x1810000+k*28,&m.tiles[k],28));updates++;
                }
            }
        }
    }
    for(int i=-100;i<=200;i++) {
        float value=(float)i/100;putf(c.ram+0x1800024,value);c.r[18].uw[0]=0x1800000;
        CHECK(invoke(&c,0x28a61c,1));float result=gt4_cross_scale(value);CHECK(!memcmp(&c.fpr[20].f,&result,4));
    }
    unsigned cross_updates=0;
    const float periods[]={.125f,.25f,.5f,1,2};
    for(int period=0;period<5;period++)for(int start=0;start<32;start++) {
        gt4_transition_motion m;gt4_transition_init(&m,1);
        m.active=1;m.stage=2;m.value=(float)start/31;m.step=1/(periods[period]*60);
        uint8_t *actor=c.ram+0x1800000;memset(actor,0,0x40);
        put32(actor+0x10,1);put32(actor+0x18,2);put32(actor+0x20,1);
        putf(actor+0x24,m.value);putf(actor+0x2c,m.step);
        for(int frame=0;frame<150;frame++) {
            memset(c.r,0,sizeof c.r);c.r[4].uw[0]=0x1800000;c.r[5].uw[0]=0x1830000;c.r[29].uw[0]=0x1ef0000;
            CHECK(invoke(&c,0x28a6b0,0));gt4_transition_step(&m);
            CHECK(!memcmp(actor+0x24,&m.value,4));
            uint32_t active,stage,hold;memcpy(&active,actor+0x10,4);memcpy(&stage,actor+0x18,4);memcpy(&hold,actor+0x3c,4);
            CHECK(active==(unsigned)m.active && stage==(unsigned)m.stage && hold==(unsigned)m.hold);cross_updates++;
        }
    }
    unsigned color_updates=0;
    /* Preserve native stage/active completion, stub only the render boundary. */
    for(int period=0;period<5;period++)for(int direction=0;direction<2;direction++)for(int start=0;start<33;start++) {
        uint8_t *actor=c.ram+0x1800000;memset(actor,0,0x40);
        float value=(float)start/32,target=direction?0:1;
        float step=1/(periods[period]*60);if(direction)step=-step;
        int stage=direction?2:0,active=1;
        put32(actor+0x10,1);put32(actor+0x18,(uint32_t)stage);
        putf(actor+0x20,value);putf(actor+0x24,fabsf(step));
        put32(c.ram+0x1830004,0x1840000);
        const unsigned slots[]={0x1c0,0x1d8,0x198,0x1a0};
        for(int k=0;k<4;k++){put32(c.ram+0x1840000+slots[k],0);put32(c.ram+0x1840000+slots[k]+4,0x1fff000);}
        for(int frame=0;frame<150;frame++) {
            memset(c.r,0,sizeof c.r);c.r[4].uw[0]=0x1800000;c.r[5].uw[0]=0x1830000;c.r[29].uw[0]=0x1ef0000;
            CHECK(invoke(&c,0x287348,0));
            if(active && stage!=1 && gt4_color_step(&value,target,step)) {
                if(direction){stage=3;active=0;}else stage=1;
            }
            CHECK(!memcmp(actor+0x20,&value,4));
            uint32_t ee_active,ee_stage;memcpy(&ee_active,actor+0x10,4);memcpy(&ee_stage,actor+0x18,4);
            CHECK(ee_active==(unsigned)active && ee_stage==(unsigned)stage);color_updates++;
        }
    }
    unsigned alpha_cases=0;
    for(int i=-4096;i<=8192;i++) {
        float value=(float)i/4096;
        memset(c.r,0,sizeof c.r);c.r[4].uw[0]=0x64;c.fpr[12].f=value;
        CHECK(invoke(&c,0x4a2808,0));
        CHECK(ee_r8(&c,0x7000219c)==gt4_gs_alpha(value));alpha_cases++;
    }
    printf("PASS %u original GS alpha coefficients, byte-identical.\n",alpha_cases);
    printf("PASS %u original colour fade updates including strict completion; %u render boundary calls.\n",color_updates,render_calls);
    printf("PASS %u original cross phase updates including completion state.\n",cross_updates);
    printf("PASS %u original block initializations, %u block updates, 301 cross scales: float bytes identical.\n",tiles,updates);
    ee_free(&c);return 0;
}
