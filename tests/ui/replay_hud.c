/* Replay text must survive the fonts cached by the preceding race. Compare
   fresh and used HUDs with real disc text/atlases; no physics is involved. */
#include <SDL.h>
#include "plat_sdl.h"
#include "video_gl.h"
#include "platform/platform.h"
#include "miniz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int omit_glyphs;
static void probe_glyph(const plat_texture *t,float x,float y,float w,float h,
                        float baseline,float blur,float u0,float v0,float u1,float v1,const uint8_t c[4])
{if(!omit_glyphs)plat_draw_glyph(t,x,y,w,h,baseline,blur,u0,v0,u1,v1,c);}
#define plat_draw_glyph probe_glyph
#include "../../src/core/race_hud.c"
#undef plat_draw_glyph
#define CHECK(x) do{if(!(x)){fprintf(stderr,"replay HUD %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint8_t *frame(race_hud *h,const char *label,int pause,int *w,int *height)
{
    plat_video_begin_frame();plat_video_set_view(640,480);
    const uint8_t bg[4]={160,160,160,255};
    plat_draw_quad(NULL,0,0,640,480,0,0,1,1,bg);
    race_hud_draw_replay(h,label,1,0);
    if(pause)race_hud_draw_simple_pause(h);
    uint8_t *p=plat_sdl_read_pixels(w,height);CHECK(p);
    plat_video_end_frame();return p;
}
static void picture(const char *path,const uint8_t *p,int w,int h)
{
    size_t n;void *png=tdefl_write_image_to_png_file_in_memory(p,w,h,4,&n);CHECK(png);
    FILE *f=fopen(path,"wb");CHECK(f && fwrite(png,1,n,f)==n);fclose(f);mz_free(png);
}
int main(int argc,char **argv)
{
    CHECK(argc>=2);int negative=argc>2 && !strcmp(argv[2],"--negative");
    CHECK(!plat_video_init(&(plat_video_desc){640,448,0,0,0}));SDL_HideWindow(plat_sdl_window());
    const char *why=NULL;gt4_vol *v=gt4_vol_open(argv[1],&why);CHECK(v);
    race_hud *used=race_hud_open(v);CHECK(used);
    /* These are the distinct styles requested by race panels, start/finish,
       opening and track captions. Twelve filled the old fixed cache. */
    const char *styles[]={"FC_26","FC_26*80,80","hn_20R","GT4meter#1",
        "hb_24#1","hb_24#1*130,130","FC_48","GT4oddmeter#1","hm_16#1",
        "Gaiji#1","hm_16#1*90,90","hc_20#1*70,70"};
    for(unsigned i=0;i<sizeof styles/sizeof *styles;i++)CHECK(font(used,styles[i]));
    const char *labels[]={"ReplayModeNormal","ReplayModeDive","ReplayModeNormalDisplay",
        "ReplayModeBGMOff","ReplayModeBGMOn","ReplayModeAccelerate","ReplayModeNormalSpeed"};
    unsigned cases=0;
    for(int wide=0;wide<2;wide++){
        SDL_SetWindowSize(plat_sdl_window(),wide?960:640,wide?540:448);
        video_set_aspect(wide);race_hud_set_widescreen(wide);
        for(unsigned k=0;k<sizeof labels/sizeof *labels;k++){
            CHECK(*race_text(used,labels[k]));race_hud *fresh=race_hud_open(v);CHECK(fresh);
            int w,h,w2,h2;uint8_t *a=frame(fresh,labels[k],k==0,&w,&h);
            omit_glyphs=negative;uint8_t *b=frame(used,labels[k],k==0,&w2,&h2);omit_glyphs=0;
            CHECK(w==w2 && h==h2);size_t n=(size_t)w*h*4;
            unsigned changed=0;for(size_t i=0;i<n;i++)if(a[i]!=b[i])changed++;
            fprintf(stderr,"%s wide=%d: %u differing channels\n",labels[k],wide,changed);
            if(argc>2 && !negative && !wide && k==0)picture(argv[2],b,w,h);
            CHECK(!changed);free(a);free(b);race_hud_close(fresh);cases++;
        }
    }
    /* Repeated alerts reuse styles, and adding a rich-text face must not
       invalidate the pointer already used to measure the main font. */
    int count=used->nfonts;CHECK(count>12);
    hud_font *main_face=font(used,"hn_14#1*100,107");CHECK(main_face);
    for(int i=0;i<20;i++){
        int w,h;uint8_t *p=frame(used,labels[i%7],1,&w,&h);free(p);
        CHECK(used->nfonts==count && font(used,"hn_14#1*100,107")==main_face);
    }
    race_hud_close(used);gt4_vol_close(v);plat_video_shutdown();
    printf("PASS %u replay labels/pause frames after race font cache, exact native RGBA.\n",cases);return 0;
}
