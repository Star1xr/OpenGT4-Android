/* Synthetic GL probe: no disc, captured state or original assets. */
#include "platform/platform.h"
#include "video_gl.h"
#include "ui_motion.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"blend probe line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
    (void)argc;(void)argv;
    plat_video_desc desc={640,480,0,0,0};CHECK(plat_video_init(&desc)==0);
    SDL_HideWindow(SDL_GL_GetCurrentWindow());plat_video_set_view(640,480);
    const uint8_t white[4]={255,255,255,255},black[4]={0,0,0,255};
    const uint8_t yellow[4]={255,255,0,255},red[4]={255,0,0,255};
    for(int coefficient=0;coefficient<=128;coefficient++) {
        plat_video_begin_frame();
        plat_draw_quad(NULL,0,0,640,480,0,0,1,1,white);
        /* A cursor-like foreground mark must receive the same overlay. */
        plat_draw_quad(NULL,300,220,40,40,0,0,1,1,yellow);
        float factor=(float)coefficient/128;
        plat_draw_quad_blend(NULL,0,0,640,480,0,0,1,1,black,factor);
        /* The next ordinary draw must restore the ordinary blend state. */
        plat_draw_quad(NULL,100,100,20,20,0,0,1,1,red);
        video_flush2d();uint8_t pixel[4];
        glReadPixels(320,240,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        int expected=(int)floorf(255*(1-factor)+.5f);
        CHECK(abs((int)pixel[0]-expected)<=1 && abs((int)pixel[1]-expected)<=1 && pixel[2]==0);
        glReadPixels(110,370,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        CHECK(pixel[0]==255 && pixel[1]==0 && pixel[2]==0);
        plat_video_end_frame();
    }
    /* Full framebuffer captures retain their display aspect while scaled.
       Negative substitutes an ordinary texture: the old 4:3 width fails. */
    for(int wide=0;wide<2;wide++){
        video_set_menu_projection(!(wide && getenv("GT4_CAPTURE_ASPECT_NEGATIVE")));
        video_set_aspect(wide);plat_video_begin_frame();
        plat_draw_quad(NULL,0,0,640,480,0,0,1,1,white);
        plat_texture *capture=video_grab(NULL,0,0,640,480);CHECK(capture);
        if(getenv("GT4_CAPTURE_ASPECT_NEGATIVE") && wide){
            plat_texture_destroy(capture);capture=plat_texture_create(1,1,white);
        }
        plat_draw_quad(NULL,0,0,640,480,0,0,1,1,black);
        plat_draw_quad(capture,160,120,320,240,0,1,1,0,white);
        video_flush2d();uint8_t pixel[4];
        glReadPixels(170,240,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);CHECK(pixel[0]==255);
        glReadPixels(150,240,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);CHECK(pixel[0]==0);
        plat_texture_destroy(capture);plat_video_end_frame();
    }
    /* 00215FE0 scales each authored X by framebuffer_width/640.
       Exercise both a widget and its scissor at an actual 16:9 resolution;
       ordinary race HUD projection deliberately keeps its own aspect. */
    SDL_SetWindowSize(SDL_GL_GetCurrentWindow(),960,540);
    video_set_aspect(1);
    for(int menu=0;menu<2;menu++){
        video_set_menu_projection(menu && !getenv("GT4_MENU_SCISSOR_NEGATIVE"));
        plat_video_begin_frame();
        plat_draw_quad(NULL,0,0,640,480,0,0,1,1,black);
        plat_set_clip(96,120,64,240);
        plat_draw_quad(NULL,64,120,128,240,0,0,1,1,red);
        video_flush2d();uint8_t pixel[4];
        glReadPixels(170,270,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        CHECK(pixel[0]==(menu?255:0));
        glReadPixels(260,270,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
        CHECK(pixel[0]==(menu?0:255));
        plat_set_clip(0,0,0,0);plat_video_end_frame();
    }
    /* 00217058/004A59B8's extended ModelFace frustum must retain both
       viewport axes. The old spanning-X special case compressed X x4. */
    plat_video_set_view(640,480);video_set_menu_projection(1);
    int bx,by,bw,bh,x,y,w,h;
    video_viewport(0,0,640,480,&bx,&by,&bw,&bh);
    video_viewport(-960,-720,2560,1920,&x,&y,&w,&h);
    if(getenv("GT4_MODEL_VIEWPORT_NEGATIVE")){x=bx;w=bw;}
    CHECK(w==4*bw && h==4*bh);
    CHECK(2*x+w==2*bx+bw && 2*y+h==2*by+bh);
    plat_video_shutdown();
    puts("PASS 129 GL blend coefficients and foreground overlay/order; byte rounding tolerance 1.");
    return 0;
}
