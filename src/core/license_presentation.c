#include "license_presentation.h"
#include <string.h>
#include <math.h>
#include <fenv.h>
#include "course_pregrid.h"
int gt4_license_opening_control(int frames){
 /* 0037D5B0 +14..6C, D_00621290 thresholds 1,2,3,4,11 seconds.
    Integer frame boundaries preserve the strict less-than comparisons. */
 return frames<60?5:frames<120?4:frames<180?3:frames<240?2:1;
}
int gt4_license_menu_cycle_update(gt4_license_menu_cycle *c)
{
    int request=0;
    /* 0012B928 +40..B0: callback 0012B888 acknowledges a rebuilt Scene.
       +258..2C8 advances alpha by 0x3C888888 / signed duration. */
    if(c->paused){
        if(c->ready){c->paused=0;c->duration=-.5f;}
        else if(!c->job){c->job=1;request=1;}
    }
    if(c->duration!=0){
        fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
        uint32_t u=0x3c888888;float dt;memcpy(&dt,&u,4);
        c->alpha+=dt/c->duration;
        if(c->duration>0 && c->alpha>=1){c->alpha=1;c->duration=0;}
        else if(c->duration<0 && c->alpha<=0){c->alpha=0;c->duration=0;}
        fesetenv(&env);
    }
    return request;
}
void gt4_license_menu_cycle_draw(gt4_license_menu_cycle *c,int ended)
{
    if(c->paused)return;
    /* 0012BC18 +6C..114: step only in phase 0. End sets phase 2,
       one-second close; two black draws precede the asynchronous reset. */
    if(!c->phase){if(ended){c->phase=2;c->duration=1;}}
    else if(c->alpha>=1){
        if(--c->phase==0){c->paused=1;c->ready=0;c->job=0;}
    }
}
static uint32_t word(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
static unsigned half(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static int span(size_t n,size_t off,size_t len){return off<=n && len<=n-off;}
int gt4_license_trophy_variant(uint8_t *d,size_t n,unsigned variant)
{
    /* 003E18D0 +0x158..0x194 sets ModelContext +0x18; 00455370
       invokes 004551D0 to select material bank and each Tex1's CLUT.
       This operates on relative file offsets, before model relocation. */
    if(n<128 || memcmp(d,"MDLS",4) || variant>=d[0x2c])return 0;
    size_t banks=word(d+0x60);
    if(!span(n,banks+(size_t)variant*4,4))return 0;
    uint32_t bank=word(d+banks+(size_t)variant*4);if(!span(n,bank,(size_t)half(d+0x1a)*80))return 0;
    memcpy(d+0x40,&bank,4);d[0x2d]=(uint8_t)variant;
    size_t lists=word(d+0x44);unsigned tables=half(d+0x1e),textures=half(d+0x1c);
    if(!span(n,lists,(size_t)tables*4))return 0;
    for(unsigned i=0;i<tables;i++){
        size_t list=word(d+lists+i*4);if(!list)continue;
        if(!span(n,list,(size_t)textures*4))return 0;
        for(unsigned j=0;j<textures;j++){
            size_t off=word(d+list+j*4);if(!off)continue;
            if(!span(n,off,48) || memcmp(d+off,"Tex1",4))return 0;
            uint8_t *t=d+off;size_t vars=word(t+0x20);if(!vars)continue;
            if(!span(n,off+vars,4) || variant>=word(t+vars))return 0;
            if(!span(n,off+vars+4+variant*4,4))return 0;
            size_t entries=word(t+vars+4+variant*4);
            if(!span(n,off+entries,4))return 0;unsigned count=half(t+entries);
            if(!span(n,off+entries+4,(size_t)count*4))return 0;
            for(unsigned k=0;k<count;k++){
                /* 00498CA0 +0xb0..0x140: descriptor packs texture index,
                   CPSM, CBP relative to +0x10, and CSA. */
                uint32_t patch=word(t+entries+4+k*4);unsigned index=patch>>23;
                size_t tex=word(t+0x18)+(size_t)index*40;
                if(!span(n,off+tex,8) || index>=half(t+0x14))return 0;
                uint64_t value;memcpy(&value,t+tex,8);
                value=(value&UINT64_C(0xff87ffffffffffff))|((uint64_t)((patch>>19)&15)<<51);
                value=(value&UINT64_C(0xfff8001fffffffff))|((uint64_t)(((patch>>5)+half(t+0x10))&0x3fff)<<37);
                value=(value&UINT64_C(0xe0ffffffffffffff))|((uint64_t)(patch&31)<<56);
                memcpy(t+tex,&value,8);
            }
        }
    }
    return 1;
}
static float bits(uint32_t n){float f;memcpy(&f,&n,4);return f;}
void gt4_license_camera_projection(float radians,float *sx,float *sy)
{
    fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
    /* 0037DC78 +0x3a0..0x3d0: radians * 180 / original pi, then
       003794A8 +0x28..0x98. Preserve both conversions and their order.
       0057D380 tan remains an explicit host libm boundary (C). */
    float degrees=(radians*180.f)/bits(0x40490fda);
    gt4_pregrid_projection(degrees,sx,sy);fesetenv(&env);
}
void gt4_license_fade_begin(gt4_license_fade *f)
{
    /* 0033BDB0 +0x270..0x298 initializes CF48/CF4C to 1, rates to 0. */
    f->value=1;f->rate=0;
}
void gt4_license_fade_step(gt4_license_fade *f,int enabled,int previous,int phase,float dt)
{
    fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
    /* 0033C4F0 +0x2dc..0x328 runs before SceneLicense 003BC6E0.
       The new phase's velocity takes effect on the following tick. */
    float delta=dt*f->rate;f->value=fminf(1.f,fmaxf(0.f,f->value+delta));
    if(enabled && previous!=phase){
        /* 003BC6E0 +0x10c..0x16c, gates 004097F8/00409828;
           durations 00409878 (2 seconds) / 00409888 (1 second). */
        if(phase==1)f->rate=-1.f/2.f;
        if(phase==2)f->rate=1.f/1.f;
    }fesetenv(&env);
}
float gt4_license_fade_factor(float value)
{
    fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
    /* 0033FA40 +0x2c..0x48: retain the two subtractions, which affect
       small values. 003425E0 receives the surviving image factor. */
    float factor=1.f;if(value<1.f){factor=value*value;factor*=value;factor=1.f-factor;factor=1.f-factor;}
    fesetenv(&env);return factor;
}
void gt4_license_layout_make(gt4_license_layout *p)
{
    /* 003BD808 +0x78..0x1d0: common display is initialized separately.
       These eight elements are RaceDisplayLicense's additional layout. */
    memset(p,0,sizeof *p);
    for(int i=0;i<3;i++){
        p->times[i]=(gt4_license_element){1,13,0,0,608,(int16_t)(110+i*28),0,0};
        p->marks[i]=(gt4_license_element){1,14,0,0,584,(int16_t)(110+i*28),0,0};
    }
    p->pylons=(gt4_license_element){0,6,1,0,135,74,100,0};
    p->pylon_label=(gt4_license_element){0,6,0,0,135,20,30,35};
}
void gt4_license_trophy_begin(float v[10][4],int state)
{
    /* 003E18D0 +0x30..0x158. No guessed trophy placement or speed. */
    memset(v,0,10*4*sizeof(float));
    v[2][0]=-.5f;v[2][1]=-2.5f;v[2][2]=.5f;
    v[7][1]=60;v[7][2]=.5f;
    v[9][1]=1;v[9][2]=1;
    if(state>0 && state<3){v[4][0]=-22.5f;v[2][2]=bits(0x3eb33333);}
    else if(state==3){v[4][0]=180;v[7][0]=1000;v[7][1]=60;v[7][2]=bits(0x3f999999);}
    else {v[4][0]=270;v[3][0]=-180;v[3][2]=1.5f;v[7][2]=3;}
}
static void step(float v[4],float dt)
{
    /* 003E2058: preserve the original single-precision operation order. */
    float delta=v[1]-v[0];delta=delta*v[2];delta=delta+v[3];
    delta=delta*dt;v[0]=v[0]+delta;
}
void gt4_license_trophy_step(float v[10][4],float dt)
{
    /* 003E1A80 +0x38..0xa4: rotations receive the third spring bank's
       instantaneous value as drift, before that rotation advances. */
    for(int i=0;i<3;i++){step(v[6+i],dt);v[3+i][3]=v[6+i][0];step(v[i],dt);step(v[3+i],dt);}
    step(v[9],dt);
}
