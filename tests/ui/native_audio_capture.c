/* PC recording boundary: captures only the exact final app mix. */
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "platform/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"audio capture line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void music(int16_t *out,int frames,void *user)
{(void)user;for(int i=0;i<frames;i++){out[i*2]=2000;out[i*2+1]=-3000;}}
int main(int argc,char **argv)
{
    CHECK(argc==2 || argc==3);SDL_SetMainReady();SDL_setenv("SDL_AUDIODRIVER","dummy",1);
#ifdef _WIN32
    _putenv_s("OPENGT4_CAPTURE_AUDIO",argv[1]);
#else
    setenv("OPENGT4_CAPTURE_AUDIO",argv[1],1);
#endif
    CHECK(!SDL_Init(SDL_INIT_TIMER|SDL_INIT_EVENTS));uint64_t before=plat_time_ns();
    plat_audio_desc desc={48000,2};CHECK(!plat_audio_init(&desc,music,NULL));
    int16_t *voice=malloc(48000*4);CHECK(voice);
    for(int i=0;i<48000;i++){voice[i*2]=4000;voice[i*2+1]=5000;}
    CHECK(plat_audio_play(voice,48000,48000,2));free(voice);SDL_Delay(250);
    plat_audio_shutdown();uint64_t after=plat_time_ns();
    FILE *file=fopen(argv[1],"rb");CHECK(file);uint64_t stamp;CHECK(fread(&stamp,8,1,file)==1);
    CHECK(stamp>=before && stamp<=after);fseek(file,0,SEEK_END);long bytes=ftell(file);
    CHECK(bytes>=8+2048*4 && (bytes-8)%4==0);fseek(file,bytes-2048*4,SEEK_SET);
    int16_t final[4096];CHECK(fread(final,2,4096,file)==4096);fclose(file);
    int expected=argc==3?6001:6000;
    for(int i=0;i<2048;i++){CHECK(final[i*2]==expected);CHECK(final[i*2+1]==2000);}
    SDL_Quit();puts("native capture: exact music + voice PCM, monotonic timestamp, stereo S16");return 0;
}
