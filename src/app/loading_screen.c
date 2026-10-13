/* LoadingPS2 assets and placement from the user's own disc. No EE runtime. */
#include "loading_screen.h"
#include "platform/platform.h"
#include "inflate.h"
#include "tex1.h"
#include "ui_motion.h"
#include "../platform/sdl/video_gl.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static plat_texture *logos[2],*dial;
static unsigned variant;
static uint32_t choice_state;
static int preview_variant=-1;
static int active;
static uint64_t began;
static plat_texture *previous;
static float background_period,background_alpha,logo_period,logo_alpha;
static uint64_t last_draw;
static uint32_t u32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void textures(const uint8_t *data,size_t size,int index,plat_texture **target)
{
    tex1_image *images=NULL;int count=data?tex1_decode(data,size,&images):-1;
    if(count>index)*target=plat_texture_create(images[index].width,images[index].height,images[index].rgba);
    if(count>0)tex1_free(images,count);
}
void loading_screen_open(gt4_vol *vol,const char *disc)
{
    size_t size=0;uint8_t *data=gt4_vol_read(vol,"/advertise/gtloading.img",&size);
    textures(data,size,0,&logos[0]);textures(data,size,1,&logos[1]);free(data);
    /* func_00103B80 loads the embedded Tex1 at D_0068C080. CORE.GT4 is
       unpacked only to read this image; none of its code is executed. The
       native load records identify its location (NTSC-U v2.00). */
    data=gt4_disc_read_root(disc,"CORE.GT4",&size);
    if(data && size>6 && data[0]==1 && data[1]==1){
        size_t cap=u32(data+2);uint8_t *raw=cap>=0x12c && cap<32u*1024*1024?malloc(cap):NULL;
        if(raw && core_inflate_raw(data+6,size-6,raw,cap)==cap){
            for(size_t at=0x12c;at+8<=cap;){
                uint32_t addr=u32(raw+at),length=u32(raw+at+4);at+=8;
                if(length>cap-at)break;
                if(addr<=0x0068c080 && 0x0068c080-addr<length){
                    size_t off=0x0068c080-addr;
                    if(length-off>=16 && !memcmp(raw+at+off,"Tex1",4))textures(raw+at+off,length-off,0,&dial);
                    break;
                }
                at+=length;
            }
        }
        free(raw);
    }
    free(data);
    if(!logos[0] || !logos[1] || !dial)fprintf(stderr,"loading: original assets unavailable for this disc\n");
}
void loading_screen_close(void){for(int i=0;i<2;i++){plat_texture_destroy(logos[i]);logos[i]=NULL;}plat_texture_destroy(dial);dial=NULL;plat_texture_destroy(previous);previous=NULL;active=0;}
int loading_screen_active(void)
{
    if(active==2 && background_period==0 && logo_period==0){
        active=0;
        if(getenv("OPENGT4_LOADING_TRACE"))fprintf(stderr,"loading fade: complete\n");
    }
    return active;
}
int loading_screen_revealing(void){return active==2;}
int loading_screen_ready(void){return active==1 && background_alpha==1 && logo_alpha==1;}
void loading_screen_preview_variant(int index){preview_variant=index>=0 && index<2?index:-1;}
void loading_screen_begin(void)
{
    if(active)return;
    previous=video_capture_frame(previous);active=1;began=plat_time_ns();last_draw=0;
    /* func_001042C0 / 00104748: background first, then logo (001047E0). */
    background_period=1; background_alpha=0;logo_period=0;logo_alpha=0;
    /* func_00104748 takes func_00579358() & 1. Its software component,
       func_00579330, advances a uint32 by *17+17 and XORs a 16-bit rotate.
       The PS2 timer input is replaced by the platform's clock; this is not
       the shared original RNG state, so its sequence is not oracle-identical. */
    choice_state=choice_state*17u+17u;
    uint32_t mixed=choice_state^((choice_state<<16)|(choice_state>>16));
    variant=preview_variant>=0?(unsigned)preview_variant:(mixed+(uint32_t)began)&1u;
}
void loading_screen_end(void)
{
    if(active==1){
        /* func_001042C0 / 00104748, phase 2: both periods become -0.25. */
        background_period=logo_period=-0.25f;active=2;last_draw=0;
    }
}
void loading_screen_draw(void)
{
    uint8_t black[4]={0,0,0,255},white[4]={255,255,255,255},gray[4]={130,130,150,255};
    uint64_t now=plat_time_ns();
    float dt=last_draw?(float)((now-last_draw)/1e9):0;
    if(dt>1.0f/30)dt=1.0f/30;
    last_draw=now;
    gt4_loading_fade_step(&background_period,&background_alpha,dt);
    if(active==1 && background_alpha>=1 && logo_period==0 && logo_alpha==0)logo_period=1;
    gt4_loading_fade_step(&logo_period,&logo_alpha,dt);
    float alpha=background_alpha;
    if(getenv("OPENGT4_LOADING_TRACE"))fprintf(stderr,"loading fade: %s %.6f\n",active==2?"out":"in",alpha);
    float view=active==2?video_view_height():448;float sy=view/448;
    plat_video_set_view(640,view);
    if(active==1 && previous && alpha<1)plat_draw_quad(previous,0,0,640,view,0,1,1,0,white);
    black[3]=gray[3]=(uint8_t)(alpha*255+.5f);
    white[3]=(uint8_t)(logo_alpha*255+.5f);
    plat_draw_quad(NULL,0,0,640,view,0,0,1,1,black);
    /* func_00104630/00103018: indexed vertices D_006184A8 ->
       D_0068CA80 (gray GT, -100..92, y=56..-40) and D_0068CAC0
       (colored bars, +/-80, y=48..-32), projected into 640x448. */
    if(logos[variant]){
        if(variant==0)plat_draw_quad(logos[0],220,168*sy,192,96*sy,0,0,1,1,white);
        else plat_draw_quad(logos[1],240,176*sy,160,80*sy,0,0,1,1,white);
    }
    /* func_00102B30/00102D00: centre (0,-117), +/-32 vertices,
       texture zero. func_00104150 adds dt*720 degrees, then draw negates
       the angle in world Y-up coordinates (clockwise in screen space). */
    float angle=(float)((plat_time_ns()-began)/1e9)*720.0f;
    if(dial)plat_draw_rotated(dial,288,309*sy,64,64*sy,320,341*sy,angle,gray);
}
