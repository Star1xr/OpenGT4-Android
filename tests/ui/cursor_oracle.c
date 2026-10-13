/* Oracle-only generated EE code; never linked into the application. */
#include "ee.h"
#include "ui_motion.h"
#include <stdlib.h>
#include <string.h>

ee_fn recomp_lookup(uint32_t pc);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"cursor oracle line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void putf(uint8_t *p,float value){memcpy(p,&value,4);}
static int samef(const uint8_t *p,float value){return !memcmp(p,&value,4);}
static int invoke(ee_ctx *c){
    memset(c->r,0,sizeof c->r);c->r[20].uw[0]=0x1800000;
    c->r[17].uw[0]=0x18006ec;c->r[18].uw[0]=0x18006f8;c->r[29].uw[0]=0x1ef0000;
    jmp_buf jump;c->exit_jmp=&jump;c->end_reason=0;
    if(setjmp(jump)){
        /* End before drawing/resizing: these are the only allowed boundaries. */
        int expected=(c->end_reason==EE_END_STOP_PC && c->end_pc==0x230558) ||
                     (c->end_reason==EE_END_UNRECOMPILED && c->end_pc==0x2304dc &&
                      !strcmp(c->end_detail,"call to 0x002a3a18, not recompiled"));
        if(!expected)fprintf(stderr,"cursor oracle: %s\n",c->end_detail);
        return expected;
    }
    c->lookup(0x230384)(c);return 0;
}
int main(int argc,char **argv){
    if(argc!=2){fprintf(stderr,"usage: ui_cursor_oracle extracted/core.bin\n");return 2;}
    ee_ctx c;CHECK(!ee_init(&c));c.lookup=recomp_lookup;
    FILE *f=fopen(argv[1],"rb");CHECK(f);CHECK(fread(c.ram+0x100000,1,EE_RAM_SIZE-0x100000,f)>0x5994a0);fclose(f);
    const float differences[]={-640,-2,-1,-.999f,-.5f,0,.5f,.999f,1,2,640};
    const float speeds[]={0,.25f,1,2,8};unsigned count=0;
    const int32_t flags[]={0,1,-1,INT32_MIN};
    for(unsigned gate=0;gate<4;gate++)for(unsigned speed=0;speed<5;speed++)for(unsigned ix=0;ix<11;ix++)for(unsigned iy=0;iy<11;iy++)for(int pose=0;pose<9;pose++) {
        float x=320+differences[ix],y=240+differences[iy],angle=(float)(pose*22-90);
        float ta=(float)(45-pose*11);uint8_t *p=c.ram+0x1800000;
        memset(p,0,0x1d58);uint32_t context=0x1810000;memcpy(p+0x10,&context,4);
        putf(c.ram+context+0xa0,speeds[speed]);
        putf(p+0x6f4,x);putf(p+0x700,y);putf(p+0x70c,angle);
        putf(p+0x1d2c,320);putf(p+0x1d30,240);putf(p+0x1d34,ta);
        for(int tick=0;tick<90;tick++) {
            int32_t paused=gate==1?flags[tick%4]:0;
            int32_t follow=gate==2?flags[tick%4]:1;
            int32_t active=gate==3?flags[tick%4]:1;
            memcpy(p+0x6e0,&paused,4);memcpy(p+0x1d50,&follow,4);memcpy(p+0x1d40,&active,4);
            CHECK(invoke(&c));gt4_cursor_update(&x,&y,&angle,320,240,ta,speeds[speed],paused,follow,active);
            if(!samef(p+0x6f4,x) || !samef(p+0x700,y) || !samef(p+0x70c,angle)) {
                fprintf(stderr,"speed=%g dx=%g dy=%g pose=%d tick=%d\n",speeds[speed],differences[ix],differences[iy],pose,tick);return 1;
            }
            count++;
        }
    }
    printf("PASS %u original gated cursor updates: X, Y and angle float bytes identical.\n",count);
    ee_free(&c);return 0;
}
