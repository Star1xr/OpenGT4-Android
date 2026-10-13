/* Oracle-only generated EE code; never linked into the application. */
#include "ee.h"
#include "ui_motion.h"
#include <stdlib.h>
#include <string.h>

ee_fn recomp_lookup(uint32_t pc);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"tooltip oracle line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void putf(uint8_t *p,float value){memcpy(p,&value,4);}
static int samef(const uint8_t *p,float value){return !memcmp(p,&value,4);}
static void puti(uint8_t *p,int value){memcpy(p,&value,4);}
static int invoke(ee_ctx *c){
    memset(c->r,0,sizeof c->r);c->r[4].uw[0]=0x1800000;c->r[29].uw[0]=0x1ef0000;
    jmp_buf jump;c->exit_jmp=&jump;c->end_reason=0;
    if(setjmp(jump)){fprintf(stderr,"original tooltip: %s\n",c->end_detail);return 0;}
    c->lookup(0x24a2f8)(c);return 1;
}
int main(int argc,char **argv){
    if(argc!=2){fprintf(stderr,"usage: ui_tooltip_oracle extracted/core.bin\n");return 2;}
    ee_ctx c;CHECK(!ee_init(&c));c.lookup=recomp_lookup;
    FILE *f=fopen(argv[1],"rb");CHECK(f);size_t size=fread(c.ram+0x100000,1,EE_RAM_SIZE-0x100000,f);CHECK(size>0x5994a0);fclose(f);
    const int states[]={1,2,3,4,5,6,10,11,100};unsigned count=0;
    for(int half=0;half<2;half++)for(int scenario=0;scenario<32;scenario++)for(unsigned state=0;state<sizeof states/sizeof states[0];state++) {
        gt4_tooltip_motion m;gt4_tooltip_start(&m,400+(float)scenario,22,scenario&1?1200:200,14,half);
        m.state=states[state];m.wait=scenario%5-2;
        m.target_x=scenario&2?-m.text_width-1:m.width*0.05f;
        m.target_y=scenario&4?0:m.height;
        m.x=m.target_x+4;m.y=m.target_y-3;
        uint8_t *p=c.ram+0x1800000;memset(p,0,0x154);
        puti(p+0x10c,1);puti(p+0x110,m.half_rate);puti(p+0x114,m.fits);
        p[0x118]=(uint8_t)m.state;putf(p+0x11c,m.text_width);putf(p+0x120,m.width);putf(p+0x124,m.height);
        putf(p+0x128,m.target_x);putf(p+0x12c,m.target_y);putf(p+0x138,m.x);putf(p+0x144,m.y);
        putf(p+0x148,m.font_size);putf(p+0x14c,m.speed);puti(p+0x150,m.wait);
        for(int tick=0;tick<180;tick++) {
            CHECK(invoke(&c));gt4_tooltip_step(&m);
            if(p[0x118]!=m.state || !samef(p+0x128,m.target_x) || !samef(p+0x12c,m.target_y) ||
               !samef(p+0x138,m.x) || !samef(p+0x144,m.y) || memcmp(p+0x150,&m.wait,4)) {
                fprintf(stderr,"half=%d scenario=%d initial=%d tick=%d native state=%d original=%u\n",half,scenario,states[state],tick,m.state,p[0x118]);return 1;
            }
            count++;
        }
    }
    /* Elapsed-time adapter must preserve the same original ticks. */
    gt4_tooltip_motion a,b;gt4_tooltip_start(&a,640,22,1800,14,0);b=a;
    for(int tick=0;tick<3000;tick++)gt4_tooltip_step(&a);
    for(int frame=0;frame<6000;frame++)gt4_tooltip_advance(&b,1.0f/120.0f);
    CHECK(a.state==b.state && a.wait==b.wait && !memcmp(&a.x,&b.x,4) && !memcmp(&a.y,&b.y,4));
    printf("PASS %u original tooltip updates: state, counters, targets and displayed float bytes identical; 120 Hz adapter matches 60 Hz ticks.\n",count);
    ee_free(&c);return 0;
}
