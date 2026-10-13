/* The original per-car race sound (003B5E80 -> 00392640), recompiled from
   the user's CORE.GT4, run on PCSX2 memory captures and compared with the
   rewrite (race_car_sound.c): the frame byte for byte, then every request
   to the sound driver. Only the driver calls (00462A88, 00392EB8), the
   VU0 transform (004A7AA8) and the random draw are stood in; the camera's
   getters run as recompiled code and feed the rewrite's listener.
   usage: race_car_sound_oracle <capture.eeMemory.bin>... [--negative | --negative-ambient | --negative-voice] */
#include "ee.h"
#include "phys.h"
#include "race_car_sound.h"
#include "race_sound.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"race car sound line %d: %s\n",__LINE__,#x);exit(1);}}while(0)

ee_fn recomp_lookup(uint32_t);
static float view[16];
static int in_frame, in_requests, negative, negative_ambient, negative_voice, negative_listener;
static uint8_t frame[0x2d0];
typedef struct { uint32_t handle; int32_t a1, a2, a3; uint32_t f12, f13, f14; } call;
typedef struct { uint32_t effect; uint32_t f12, f13, f14, f15; } shot;
static call calls[64];
static shot shots[16];
static int ncalls, nshots, restarts;
static uint32_t random_value;

static uint32_t bits(float f){uint32_t u;memcpy(&u,&f,4);return u;}
int plat_audio_play(const int16_t *p,size_t n,int r,int ch){(void)p;(void)n;(void)r;(void)ch;return 0;}
static void ret(ee_ctx *c){(void)c;}
static void transform_hook(ee_ctx *c)
{
    uint32_t out=c->r[4].uw[0],in=c->r[5].uw[0];float v[4],o[4];
    for(int k=0;k<4;k++){uint32_t u=ee_r32(c,in+k*4);memcpy(v+k,&u,4);}
    for(int k=0;k<4;k++)o[k]=((view[k]*v[0]+view[4+k]*v[1])+view[8+k]*v[2])+view[12+k]*v[3];
    for(int k=0;k<4;k++)ee_w32(c,out+k*4,bits(o[k]));
}
static void voice_hook(ee_ctx *c)
{
    CHECK(ncalls<64);call *k=calls+ncalls++;
    k->handle=c->r[4].uw[0];k->a1=(int32_t)c->r[5].uw[0];k->a2=(int32_t)c->r[6].uw[0];k->a3=(int32_t)c->r[7].uw[0];
    k->f12=c->fpr[12].u;k->f13=c->fpr[13].u;k->f14=c->fpr[14].u;
}
static void shot_hook(ee_ctx *c)
{
    CHECK(nshots<16);shot *s=shots+nshots++;
    s->effect=c->r[5].uw[0];s->f12=c->fpr[12].u;s->f13=c->fpr[13].u;s->f14=c->fpr[14].u;s->f15=c->fpr[15].u;
}
static void restart_hook(ee_ctx *c){(void)c;restarts++;}
static void random_hook(ee_ctx *c){c->r[2].sd[0]=(int32_t)random_value;}
static void requests_hook(ee_ctx *c)
{
    memcpy(frame,c->ram+(c->r[5].uw[0]&0x1ffffff),sizeof frame);
    in_requests=1;recomp_lookup(0x00392640u)(c);in_requests=0;
}
static ee_fn boundary(uint32_t pc)
{
    switch(pc){
    case 0x004a7aa8: return transform_hook;
    case 0x004a5348: case 0x004a7454: case 0x00372080: return ret;
    case 0x00392fc8: case 0x003924d0: return restart_hook;
    case 0x00579358: return random_hook;
    case 0x00462a88: return voice_hook;
    case 0x00392eb8: return shot_hook;
    case 0x00392640: return in_frame && !in_requests?requests_hook:NULL;
    default: return NULL;
    }
}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
static int invoke(ee_ctx *c,uint32_t pc)
{jmp_buf j;c->exit_jmp=&j;if(setjmp(j)){fprintf(stderr,"%s\n",c->end_detail);return 0;}ee_fn fn=c->lookup(pc);if(!fn)return 0;fn(c);return 1;}
/* a virtual call on the camera manager: (s16 adjust, u32 function) */
static int vcall(ee_ctx *c,uint32_t obj,uint32_t off,uint32_t a1,float f12)
{
    uint32_t vt=ee_r32(c,obj),e=vt+off;int16_t adj=(int16_t)(ee_r32(c,e)&0xffff);
    memset(c->r,0,sizeof c->r);c->r[4].uw[0]=obj+(uint32_t)(int32_t)adj;c->r[5].uw[0]=a1;c->r[29].uw[0]=0x1ef0000;
    c->fpr[12].f=f12;CHECK(invoke(c,ee_r32(c,e+4)));return 1;
}
static uint32_t find_manager(ee_ctx *c)
{
    for(uint32_t a=0x1000000;a<0x2000000;a+=4)
        if(ee_r32(c,a)==0x0067a078u && ee_r32(c,a-0x80+0x6c)>0x100000 && ee_r32(c,a-0x80+0x6c)<0x2000000)return a;
    return 0;
}
static int same(uint32_t a,uint32_t b){return a==b || ((a|b)&0x7fffffff)==0;}
int main(int argc,char **argv)
{
    CHECK(argc>=2);
    ee_ctx c;CHECK(!ee_init(&c));c.lookup=lookup;c.hook=boundary;
    uint8_t *port=malloc(0x2000000);CHECK(port);
    unsigned frames=0,requests=0,fields=0,captures=0,ambients=0,voices=0,pitches=0;
    for(int a=1;a<argc;a++){negative|=!strcmp(argv[a],"--negative");negative_ambient|=!strcmp(argv[a],"--negative-ambient");negative_voice|=!strcmp(argv[a],"--negative-voice");negative_listener|=!strcmp(argv[a],"--negative-listener");}
    for(int a=1;a<argc;a++){
        if(argv[a][0]=='-')continue;
        FILE *fp=fopen(argv[a],"rb");CHECK(fp);size_t n=fread(c.ram,1,0x2000000,fp);fclose(fp);CHECK(n==0x2000000);
        uint32_t mgr=find_manager(&c);if(!mgr){printf("skip %s: no camera manager\n",argv[a]);continue;}
        captures++;
        uint32_t world=mgr-0x80,race=ee_r32(&c,world+0x6c),list=ee_r32(&c,race+0x60);
        uint32_t cam=ee_r32(&c,mgr+ee_r32(&c,mgr+0x9bc)*0x19c+0x13c);
        for(int k=0;k<16;k++){uint32_t u=ee_r32(&c,cam+0x100+k*4);memcpy(view+k,&u,4);}
        /* the listener, through the manager's own getters */
        gt4_car_listener L;memset(&L,0,sizeof L);memcpy(L.view,view,sizeof view);
        static const uint32_t pos0[3]={0x20,0x28,0x30},pos1[3]={0xe0,0xe8,0xf0};
        for(int k=0;k<3;k++){vcall(&c,mgr,pos0[k],0,0);L.eye_previous[k]=c.fpr[0].f;vcall(&c,mgr,pos1[k],1,0);L.eye[k]=c.fpr[0].f;}
        vcall(&c,mgr,0x100,0,0);L.continuous=(int)c.r[2].sw[0];
        vcall(&c,mgr,0x108,0,0);L.type=(int)c.r[2].sw[0];
        vcall(&c,mgr,0x128,0,0);L.angle=c.fpr[0].f;
        /* the master: 00391CD8 * D_006213E0 */
        memset(c.r,0,sizeof c.r);c.r[29].uw[0]=0x1ef0000;CHECK(invoke(&c,0x00391cd8u));
        float master=c.fpr[0].f*gt4_f32(ee_r32(&c,0x6213e0));
        int replay=ee_r32(&c,world+0xcc8)!=0;
        unsigned cars=ee_r32(&c,list);
        memcpy(port,c.ram,0x2000000);
        for(unsigned i=0;i<cars;i++){
            uint32_t entry=ee_r32(&c,ee_r32(&c,list+8)+i*4),sound=entry+0x2900;
            vcall(&c,mgr,0x168,entry,0);int target=(int)c.r[2].sw[0];
            /* the rewrite's state: the capture's sound object */
            gt4_car_sound_state st;memset(&st,0,sizeof st);st.started=(int)ee_r32(&c,sound+0xb38);
            static const uint32_t obj[7]={0x14,0x3f4,0x2b8,0x474,0x474+0x1a0,0x474+0x340,0x474+0x4e0};
            for(int k=0;k<7;k++){st.invert[k][0]=c.ram[sound+obj[k]];st.invert[k][1]=c.ram[sound+obj[k]+1];}
            for(int k=0;k<3;k++)st.center[k]=gt4_f32(ee_r32(&c,sound+0x468+k*4));
            for(int w=0;w<4;w++)for(int k=0;k<11;k++)st.wheel[w][k]=gt4_f32(ee_r32(&c,sound+0x5e8+w*0x1a0+k*4));
            gt4_car_sound_setup setup;memset(&setup,0,sizeof setup);
            for(int k=0;k<11;k++)setup.wheel_on[k]=c.ram[0x88f2c0+k];
            for(int k=0;k<3;k++){setup.falloff[k]=gt4_f32(ee_r32(&c,0x621350+k*44+0x24));setup.near[k]=gt4_f32(ee_r32(&c,0x621350+k*44+0x28));}
            setup.master=master;
            random_value=0x2468ace1u*(i+1);
            /* the original */
            ncalls=nshots=restarts=0;memset(frame,0,sizeof frame);
            memset(c.r,0,sizeof c.r);c.r[4].uw[0]=entry;c.r[5].uw[0]=mgr;c.r[6].uw[0]=(uint32_t)replay;c.r[29].uw[0]=0x1ef0000;
            in_frame=1;CHECK(invoke(&c,0x003b5e80u));in_frame=0;
            /* the rewrite, on its own copy of the memory */
            gt4_phys_mem=port;gt4_phys_fpu_enter();
            gt4_car_sound_frame f;gt4_car_sound_out out;
            gt4_car_listener native_listener=L;
            /* Reproduce a PC catch-up tick reusing the preceding camera.
               003B5E80 +0x1FC..+0x37C must detect the wrong Doppler. */
            if(negative_listener)memcpy(native_listener.eye,native_listener.eye_previous,sizeof native_listener.eye);
            gt4_car_sound_frame_build(&f,&st,&out,entry,&native_listener,target,replay,random_value);
            if(negative && !i && captures==1)((float *)(f.b+0x4c))[0]*=1.5f;
            /* the frame, field by field (the stack's garbage words skipped) */
            for(uint32_t o=0;o<0x2cc;o+=4){
                int used=o==0 || o==8 || (o>=0x10 && o<0x98) || (o>=0x2a0 && o<0x2cc);
                if(o>=0xa0 && o<0x2a0){uint32_t r=(o-0xa0)&0x7f;used=r<0x10 || (r>=0x24 && r<0x7c);}
                if(!used)continue;
                uint32_t x,y;memcpy(&x,frame+o,4);memcpy(&y,f.b+o,4);
                /* a silent slot's pitch may be the original's uninitialised
                   local (00360028 when 0034FCC0 returns early) */
                if(o>=0xa0 && o<0x2a0 && (((o-0xa0)&0x7f)>=0x24) && !(((o-0xa0)&0x7f)&4)){
                    uint32_t v;memcpy(&v,frame+o-4,4);if(!(v&0x7fffffff))continue;}
                if(!same(x,y)){fprintf(stderr,"%s car %u frame +0x%03X: original %08X rewrite %08X\n",argv[a],i,o,x,y);exit(1);}
                fields++;
            }
            frames++;
            CHECK(!!restarts==out.restart);
            /* the requests, given the original's frame */
            memcpy(f.b,frame,sizeof frame);
            gt4_car_sound_requests(&f,&st,&setup,&out);
            static const uint32_t engine[3]={0x28,0x2c,0x2cc};
            for(int k=0;k<3;k++){
                uint32_t o=ee_r32(&c,sound+engine[k]);gt4_car_sound_engine *e=out.engine+k;
                CHECK((int16_t)(ee_r32(&c,o+0x4c)&0xffff)==e->left && (int16_t)(ee_r32(&c,o+0x4c)>>16)==e->right);
                CHECK((int32_t)ee_r32(&c,o+0x40)==e->scale && (int32_t)ee_r32(&c,o+0x3c)==e->priority);
                CHECK((int16_t)ee_r32(&c,o+0x48)==e->rpm);requests++;
            }
            int expected=0;
            for(int k=0;k<GT4_CAR_TONES;k++)expected+=out.tone_on[k];
            CHECK(expected==ncalls);
            for(int n2=0;n2<ncalls;n2++){
                call *k=calls+n2;uint32_t h=k->handle-sound;int t;
                if(h==0x278)t=GT4_CAR_TURBINE;else if(h==0x298)t=GT4_CAR_TURBINE2;
                else if(h>=0x408 && h<0x468)t=GT4_CAR_IN_CAR+(int)(h-0x408)/0x20;
                else{uint32_t r=h-0x488;t=GT4_CAR_WHEEL0+(int)(r/0x1a0)*11+(int)(r%0x1a0)/0x20;}
                gt4_car_sound_tone *tn=out.tone+t;
                if(!same(k->f12,bits(tn->left)) || !same(k->f13,bits(tn->right)) || k->a1!=tn->priority ||
                   (k->a1 && !same(k->f14,bits(tn->pitch)))){
                    fprintf(stderr,"%s car %u tone %d: original %08X %08X %08X %d rewrite %08X %08X %08X %d\n",argv[a],i,t,
                        k->f12,k->f13,k->f14,k->a1,bits(tn->left),bits(tn->right),bits(tn->pitch),tn->priority);exit(1);}
                requests++;
            }
            CHECK(nshots==out.shots);
            for(int k=0;k<nshots;k++){
                CHECK(shots[k].effect==out.shot[k].effect);
                float g=gt4_f32(shots[k].f12)*gt4_f32(shots[k].f13);
                CHECK(same(bits(gt4_f32(shots[k].f14)*g),bits(out.shot[k].left)) && same(bits(gt4_f32(shots[k].f15)*g),bits(out.shot[k].right)));
                requests++;
            }
        }
        /* the driver's registers (0055D890) from each live hardware voice
           the capture holds, against what the driver computed there */
        {
            const uint32_t M=0x6518c8;
            for(int i=0;i<46;i++){
                uint32_t v=M+0xa8+i*0xe0;
                if(!ee_r32(&c,v+0x30) || c.ram[v+0x1f] || c.ram[v+0x20])continue;
                gt4_sd_voice s;memset(&s,0,sizeof s);
                s.group=(int32_t)ee_r32(&c,v+0x30);s.master=(int32_t)ee_r32(&c,v+0x4c);s.group2=(int32_t)ee_r32(&c,v+0x50);
                s.level=(int32_t)ee_r32(&c,v+0x54);s.v58=(int32_t)ee_r32(&c,v+0x58);s.v5c=(int32_t)ee_r32(&c,v+0x5c);s.v60=(int32_t)ee_r32(&c,v+0x60);
                s.left=(int16_t)(ee_r32(&c,v+0x64)&0xffff);s.right=(int16_t)(ee_r32(&c,v+0x64)>>16);
                s.pan=(int32_t)ee_r32(&c,v+0x68);s.pan6c=(int32_t)ee_r32(&c,v+0x6c);s.pan70=(int32_t)ee_r32(&c,v+0x70);
                s.root=(int32_t)ee_r32(&c,v+0x78);s.note=(int32_t)ee_r32(&c,v+0x7c);s.fine=(int32_t)ee_r32(&c,v+0x80);
                s.range=(int32_t)ee_r32(&c,v+0x84);s.bend=(int32_t)ee_r32(&c,v+0x88);
                int16_t lr[2];gt4_sd_volume(&s,(int32_t)ee_r32(&c,M+0x4c+s.group*12),ee_r32(&c,M)!=0,c.ram+0x6c8ac0,lr);
                if(negative_voice)lr[0]++;
                uint32_t want=ee_r32(&c,v+0x94)>>16|ee_r32(&c,v+0x98)<<16;
                if((uint16_t)lr[0]!=(want&0xffff) || (uint16_t)lr[1]!=(want>>16)){
                    fprintf(stderr,"%s voice %d volume: driver %04X %04X rewrite %04X %04X\n",argv[a],i,want&0xffff,want>>16,(uint16_t)lr[0],(uint16_t)lr[1]);exit(1);}
                voices++;
                if(s.note==0x3c0){
                    uint16_t p=gt4_sd_pitch(&s,c.ram+0x654738);
                    if(p!=(ee_r32(&c,v+0x90)&0xffff)){fprintf(stderr,"%s voice %d pitch: driver %04X rewrite %04X\n",argv[a],i,ee_r32(&c,v+0x90)&0xffff,p);exit(1);}
                    pitches++;
                }
            }
        }
        /* the course's ambient groups (00457F60) from this camera */
        {
            uint32_t course=ee_r32(&c,ee_r32(&c,race+0x80)+4),crss=ee_r32(&c,course+0x8c);
            if(crss){
                const uint32_t at=0x1ee0000;float right[3]={view[0],view[4],view[8]},vel[3];
                for(int k=0;k<3;k++)vel[k]=L.eye_previous[k]-L.eye[k];
                for(int k=0;k<3;k++){ee_w32(&c,at+k*4,bits(L.eye_previous[k]));ee_w32(&c,at+0x10+k*4,bits(right[k]));ee_w32(&c,at+0x20+k*4,bits(vel[k]));}
                gt4_ambient_state as;gt4_ambient_state_reset(&as);
                /* the speakers' levels as the capture holds them */
                int idx=0;
                for(int g=0;g<5;g++){uint32_t list=ee_r32(&c,crss+0x14+g*4);for(uint32_t i=0;i<ee_r32(&c,list);i++)as.level[idx++]=gt4_f32(ee_r32(&c,ee_r32(&c,list+4+i*4)+0xc));}
                memcpy(port,c.ram,0x2000000);
                for(int round=0;round<3;round++){
                    memset(c.r,0,sizeof c.r);c.r[4].uw[0]=crss;c.r[5].uw[0]=at+0x40;c.r[6].uw[0]=race;c.r[7].uw[0]=at;
                    c.r[8].uw[0]=at+0x10;c.r[9].uw[0]=at+0x20;c.r[29].uw[0]=0x1ef0000;
                    CHECK(invoke(&c,0x00457f60u));
                    float out[5][3];gt4_phys_mem=port;gt4_phys_fpu_enter();
                    gt4_ambient_groups(&as,crss,race,L.eye_previous,right,vel,out);
                    if(negative_ambient && round==2)out[2][1]+=0.25f;
                    for(int g=0;g<5;g++)for(int k=0;k<3;k++){
                        uint32_t x=ee_r32(&c,at+0x40+g*12+k*4),y=bits(out[g][k]);
                        if(!same(x,y)){fprintf(stderr,"%s ambient group %d.%d round %d: original %08X rewrite %08X\n",argv[a],g,k,round,x,y);exit(1);}
                        fields++;
                    }
                    idx=0;
                    for(int g=0;g<5;g++){uint32_t list=ee_r32(&c,crss+0x14+g*4);for(uint32_t i=0;i<ee_r32(&c,list);i++){
                        uint32_t x=ee_r32(&c,ee_r32(&c,list+4+i*4)+0xc);CHECK(same(x,bits(as.level[idx])));idx++;fields++;}}
                }
                ambients++;
            }
        }
    }
    CHECK(captures>0);
    CHECK(voices>0 && pitches>0);
    printf("PASS %u captures, %u car frames, %u ambient blocks x 3 frames (%u fields), %u driver requests, %u SPU volumes, %u pitches identical to the original\n",
           captures,frames,ambients,fields,requests,voices,pitches);
    free(port);ee_free(&c);return 0;
}
