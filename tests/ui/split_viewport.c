/* GPU integration: each HUD stays inside its own half, including logical
   view changes made by race_hud_draw; restoring the panel restores full UI. */
#include "platform/platform.h"
#include "video_gl.h"
#include "plat_sdl.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"split viewport line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void fill(const uint8_t c[4]){plat_draw_quad(NULL,-20,-20,680,520,0,0,1,1,c);}
int main(int argc,char **argv)
{
    (void)argv;plat_video_desc d={640,448,0,0,0};CHECK(!plat_video_init(&d));
    const uint8_t red[4]={255,0,0,255},blue[4]={0,0,255,255},green[4]={0,255,0,255};
    plat_video_begin_frame();plat_video_set_view(640,448);video_ui_panel(0,224);
    plat_video_set_view(640,240);fill(red);
    plat_video_set_view(640,448);video_ui_panel(argc>1?0:224,224);
    plat_video_set_view(640,240);fill(blue);video_ui_panel(0,0);
    int w,h;unsigned char *px=plat_sdl_read_pixels(&w,&h);CHECK(px && w==640 && h==448);
    const unsigned char *top=px+(100*w+320)*4,*bottom=px+(340*w+320)*4;
    CHECK(top[0]==255 && top[2]==0 && bottom[0]==0 && bottom[2]==255);free(px);
    /* Rank rolling-digit clip is local to each half, survives batch flush,
       and resetting it must leave the enclosing viewport in force. */
    int marker_x[2][2],marker_y[2];
    for(int i=0;i<2;i++){
        int x,y,pw,ph;
        plat_video_set_view(640,448);video_viewport(0,i*224.f,640,224,&x,&y,&pw,&ph);
        marker_x[i][0]=x+20*pw/640;marker_x[i][1]=x+620*pw/640;
        marker_y[i]=h-y-ph+65*ph/240;
        plat_video_set_view(640,448);video_ui_panel(i*224.f,224);
        plat_video_set_view(640,240);plat_set_clip(0,16,640,34);fill(green);
        /* Markers near both horizontal edges catch a full-screen aspect fit
           accidentally cropping the local 640-unit coordinate space. */
        plat_set_clip(0,0,0,0);
        plat_draw_quad(NULL,10,60,20,20,0,0,1,1,green);
        plat_draw_quad(NULL,610,60,20,20,0,0,1,1,green);
        plat_set_clip(0,0,0,0);video_ui_panel(0,0);
    }
    px=plat_sdl_read_pixels(&w,&h);CHECK(px);
    for(int i=0;i<2;i++){
        const unsigned char *inside=px+((i*224+30)*w+320)*4;
        const unsigned char *outside=px+((i*224+100)*w+320)*4;
        CHECK(inside[1]==255 && outside[1]==0);
        CHECK(px[(marker_y[i]*w+marker_x[i][0])*4+1]==255);
        CHECK(px[(marker_y[i]*w+marker_x[i][1])*4+1]==255);
    }
    free(px);
    plat_video_set_view(640,448);fill(green);px=plat_sdl_read_pixels(&w,&h);CHECK(px);
    top=px+(100*w+320)*4;bottom=px+(340*w+320)*4;CHECK(top[1]==255 && bottom[1]==255);free(px);
    plat_video_end_frame();plat_video_shutdown();puts("PASS independent HUD viewports and full-screen restore");return 0;
}
