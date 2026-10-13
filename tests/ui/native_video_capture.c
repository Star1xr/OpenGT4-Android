/* Synthetic export boundary: real 1080p pixels, frame order, no upscale. */
#define SDL_MAIN_HANDLED
#include "platform/platform.h"
#include "video_gl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"video capture line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void env(const char *key,const char *value)
{
#ifdef _WIN32
    _putenv_s(key,value);
#else
    SDL_setenv(key,value,1);
#endif
}
int main(int argc,char **argv)
{
    CHECK(argc==2 || argc==3);SDL_SetMainReady();
    char times[1024],gate[1024];snprintf(times,sizeof times,"%s.csv",argv[1]);
    snprintf(gate,sizeof gate,"%s.start",argv[1]);remove(gate);
    env("OPENGT4_CAPTURE_PIPE",argv[1]);
    env("OPENGT4_CAPTURE_TIMES",times);
    env("OPENGT4_CAPTURE_GATE",gate);
    env("OPENGT4_CAPTURE_SIZE","1920x1080");
    env("OPENGT4_CAPTURE_FIXED_STEP","1");
    env("OPENGT4_WINDOW_POSITION","-1600,30");
    plat_video_desc desc={640,360,0,0,0};CHECK(!plat_video_init(&desc));
    SDL_HideWindow(SDL_GL_GetCurrentWindow());video_set_aspect(1);
    plat_video_set_view(640,480);CHECK(!video_export_fixed_step());
    plat_video_begin_frame();plat_video_end_frame();CHECK(!video_export_fixed_step());
    FILE *start=fopen(gate,"wb");CHECK(start);fclose(start);
    for(int i=0;i<6;i++){
        uint8_t color[4]={(uint8_t)(30+i*20),80,140,255};
        plat_video_begin_frame();plat_draw_quad(NULL,0,0,640,480,0,0,1,1,color);
        plat_video_end_frame();CHECK(video_export_fixed_step());
    }
    plat_video_shutdown();CHECK(!video_export_fixed_step());
    FILE *f=fopen(argv[1],"rb");CHECK(f);fseek(f,0,SEEK_END);
    CHECK(ftell(f)==6L*1920*1080*4);
    for(int i=0;i<6;i++){
        uint8_t pixel[4];fseek(f,((long)i*1920*1080+540*1920+960)*4,SEEK_SET);
        CHECK(fread(pixel,1,4,f)==4);
        CHECK(pixel[0]==30+i*20+(argc==3) && pixel[1]==80 && pixel[2]==140 && pixel[3]==255);
    }
    fclose(f);f=fopen(times,"rb");CHECK(f);char line[256];CHECK(fgets(line,sizeof line,f));
    unsigned long long previous=0;
    for(int i=0;i<6;i++){
        unsigned long long frame,stamp;int w,h,up,filter;float sharp;
        CHECK(fgets(line,sizeof line,f));
        CHECK(sscanf(line,"%llu,%llu,%d,%d,%d,%d,%f",&frame,&stamp,&w,&h,&up,&filter,&sharp)==7);
        CHECK(frame==(unsigned)i && stamp>previous && w==1920 && h==1080 && up==0 && filter==0 && sharp==0);
        previous=stamp;
    }
    CHECK(!fgets(line,sizeof line,f));fclose(f);
    puts("native video: six distinct native 1080p frames, original UI sampler, ordered timestamps");return 0;
}
