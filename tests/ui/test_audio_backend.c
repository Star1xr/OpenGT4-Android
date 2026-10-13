#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "platform/platform.h"
#include "plat_sdl.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
static SDL_atomic_t calls,generated;
static int real_device;
static void synth(int16_t *out,int frames,void *user)
{
    (void)user;
    int call=SDL_AtomicAdd(&calls,1);
    if(call>16 && call%40==0)SDL_Delay(60); /* simulate a delayed producer */
    for(int i=0;i<frames;i++)out[i*2]=out[i*2+1]=(int16_t)(real_device ? 0 : (i%32)*100);
    SDL_AtomicAdd(&generated,frames);
}
int main(int argc,char **argv)
{
    SDL_SetMainReady();real_device=argc==2 && !strcmp(argv[1],"--real");
    if(!real_device)SDL_setenv("SDL_AUDIODRIVER","OpenGT4_invalid_test_driver",1);
    CHECK(SDL_Init(SDL_INIT_TIMER|SDL_INIT_EVENTS)==0);
    plat_audio_desc d={48000,2};
    if(!real_device) {
        CHECK(plat_audio_init(&d,NULL,NULL)==-1);
        SDL_setenv("SDL_AUDIODRIVER","dummy",1);
        int16_t silent[2]={0,0};
        for(int i=0;i<100;i++) {
            CHECK(plat_audio_play(silent,1,48000,2)==0);
            CHECK(plat_audio_set_music(synth,NULL)==-1);
        }
        /* Playback does not silently reopen after failure, while explicit
           initialization still permits endpoint recovery. */
    }
    CHECK(plat_audio_init(&d,synth,NULL)==0);
    uint64_t start=SDL_GetPerformanceCounter();
    if(real_device) {
        printf("driver: %s\n",SDL_GetCurrentAudioDriver());
        for(int second=0;second<8;second++) {
            SDL_Delay(1000);
            double t=(double)(SDL_GetPerformanceCounter()-start)/SDL_GetPerformanceFrequency();
            printf("%.3f s: generated %d frames, underruns %u\n",t,SDL_AtomicGet(&generated),plat_sdl_audio_underruns());
        }
    } else SDL_Delay(2000);
    double elapsed=(double)(SDL_GetPerformanceCounter()-start)/SDL_GetPerformanceFrequency();
    CHECK(plat_audio_set_music(NULL,NULL)==0);
    int before=SDL_AtomicGet(&generated);SDL_Delay(30);CHECK(SDL_AtomicGet(&generated)==before);
    CHECK(plat_audio_set_music(synth,NULL)==0);SDL_Delay(30);CHECK(SDL_AtomicGet(&generated)>before);
    plat_audio_shutdown();
    CHECK(plat_sdl_audio_underruns()==0);
    int generated_frames=SDL_AtomicGet(&generated);
    CHECK(generated_frames>elapsed*48000*0.85 && generated_frames<elapsed*48000*1.15+16384);
    printf("buffered audio: %d frames in %.3fs, zero underruns with 60ms producer stalls\n",generated_frames,elapsed);
    if(!real_device) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        SDL_setenv("SDL_AUDIODRIVER","OpenGT4_invalid_test_driver",1);
        CHECK(plat_audio_init(&d,NULL,NULL)==-1);
        SDL_setenv("SDL_AUDIODRIVER","dummy",1);SDL_Delay(2);
        SDL_Event added;SDL_zero(added);added.type=SDL_AUDIODEVICEADDED;
        CHECK(SDL_PushEvent(&added)==1);
        plat_key_event event;plat_sdl_poll(&event);
        int16_t silence[2]={0,0};CHECK(plat_audio_play(silence,1,48000,2)==1);
        plat_audio_shutdown();
        puts("missing endpoint: playback skips failed opens; device-added event restores audio");
    }
    SDL_Quit();return 0;
}
