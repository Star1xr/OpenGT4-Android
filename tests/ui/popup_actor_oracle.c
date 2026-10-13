/* Test-only original actor blocks; generated code stays outside the app. */
#include "ee.h"
#include "ui_motion.h"
#include <string.h>
ee_fn recomp_lookup(uint32_t pc);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"actor oracle line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void putf(uint8_t *p,float value){memcpy(p,&value,4);}
static int samef(const uint8_t *p,float value){return !memcmp(p,&value,4);}
static int invoke(ee_ctx *c,uint32_t pc){
    jmp_buf jump;c->exit_jmp=&jump;c->end_reason=0;
    if(setjmp(jump))return c->end_reason==EE_END_STOP_PC;
    c->lookup(pc)(c);return 0;
}
int main(int argc,char **argv){
    if(argc!=2)return 2;ee_ctx c;CHECK(!ee_init(&c));c.lookup=recomp_lookup;
    FILE *f=fopen(argv[1],"rb");CHECK(f);CHECK(fread(c.ram+0x100000,1,EE_RAM_SIZE-0x100000,f)>0x5994a0);fclose(f);
    const float speeds[]={.25f,.5f,1,2,4};unsigned count=0;
    for(int speed=0;speed<5;speed++)for(int direction=0;direction<2;direction++)for(int start=0;start<32;start++) {
        uint8_t *p=c.ram+0x1800000;memset(p,0,0x80);
#ifdef TEST_FADE
        float value=(float)start/31.0f,target=direction?0:1;
        float period=.5f/speeds[speed],step=1.0f/(period*60.0f);if(direction)step=-step;
        putf(p+0x18,value);
#else
        float value=480+(float)start,target=direction?-100-(float)start:120+(float)start;
        float ratio=.20000000298023224f*speeds[speed];
        putf(p+0x40,value);putf(p+0x2c,target);putf(p+0x50,ratio);
#endif
        for(int tick=0;tick<120;tick++) {
            memset(c.r,0,sizeof c.r);c.r[16].uw[0]=0x1800000;c.r[29].uw[0]=0x1ef0000;
#ifdef TEST_FADE
            c.fpr[2].f=target;c.fpr[20].f=step;
            CHECK(invoke(&c,0x20c568));value=gt4_fade_value(value,target,step);CHECK(samef(p+0x18,value));
#else
            c.r[4].uw[0]=0x1800028;
            CHECK(invoke(&c,0x2c1bb0));value=gt4_move_value(value,target,ratio);CHECK(samef(p+0x40,value));
#endif
            count++;
        }
    }
#ifdef TEST_FADE
    printf("PASS %u original popup fade updates: alpha float bytes identical.\n",count);
#else
    printf("PASS %u original popup move updates: position float bytes identical.\n",count);
#endif
    ee_free(&c);return 0;
}
