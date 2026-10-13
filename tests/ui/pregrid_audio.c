/* Tests the app bridge against both actual ADS sources, including a deliberate
   wrong-volume control. It does not prove equivalence to PS2 SPU2 output. */
#include "pregrid_audio.h"
#include "pcm_music.h"
#include "platform/platform.h"
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"pregrid audio line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static plat_audio_cb callback;
static void *owner;
static unsigned starts,stops;
int plat_audio_set_music(plat_audio_cb cb,void *user){callback=cb;owner=user;starts++;return 0;}
void plat_audio_release_music(void *user){if(owner==user){callback=NULL;owner=NULL;stops++;}}
static void foreign(int16_t *out,int frames,void *user){(void)out;(void)frames;(void)user;}
static void await_stream(pregrid_audio *p,uint32_t clock)
{
    uint32_t begin=SDL_GetTicks();
    while(!callback){CHECK(SDL_GetTicks()-begin<30000);pregrid_audio_update(p,"default_quick",120,clock);SDL_Delay(5);}
}
static uint32_t next_clock(uint32_t *state,unsigned wanted)
{
    for(uint32_t clock=0;;clock++){uint32_t trial=*state;
        if(gt4_pcm_music_random(&trial,clock)%2==wanted){*state=trial;return clock;}}
}
int main(int argc,char **argv)
{
    CHECK(argc==2 || (argc==3 && !strcmp(argv[2],"--negative")));
    CHECK(SDL_Init(SDL_INIT_TIMER)==0);const char *error=NULL;
    gt4_vol *v=gt4_vol_open(argv[1],&error);CHECK(v && gt4_pcm_music_count(v,"default_quick")==2);
    pregrid_audio *p=pregrid_audio_create(argv[1]);CHECK(p);uint32_t state=0;
    for(unsigned sequence=0;sequence<2;sequence++){
        uint32_t clock=next_clock(&state,sequence);await_stream(p,clock);CHECK(starts==sequence+1);
        gt4_pcm_music *reference=gt4_pcm_music_open(v,"default_quick",sequence);CHECK(reference);
        int16_t actual[4096],expected[4096];uint64_t energy=0;
        for(int block=0;block<200;block++){
            callback(actual,2048,owner);gt4_pcm_music_render(expected,2048,reference);
            for(unsigned i=0;i<4096;i++){CHECK(actual[i]==(int)expected[i]*120/128);energy+=(uint64_t)abs(actual[i]);}
        }
        CHECK(energy>0);pregrid_audio_update(p,"default_quick",0,clock+1);callback(actual,2048,owner);
        for(unsigned i=0;i<4096;i++)CHECK(actual[i]==(argc==3?1:0));
        gt4_pcm_music_close(reference);pregrid_audio_update(p,NULL,0,0);
        CHECK(!callback && stops==sequence+1);
    }
    /* Cancel a pending decode, immediately enter again: only the latest
       generation may publish a callback, using its own selector state. */
    uint32_t stale=next_clock(&state,0);pregrid_audio_update(p,"default_quick",120,stale);
    pregrid_audio_update(p,NULL,0,0);uint32_t fresh=next_clock(&state,1);await_stream(p,fresh);
    CHECK(starts==3);gt4_pcm_music *reference=gt4_pcm_music_open(v,"default_quick",1);CHECK(reference);
    int16_t actual[4096],expected[4096];callback(actual,2048,owner);gt4_pcm_music_render(expected,2048,reference);
    for(unsigned i=0;i<4096;i++)CHECK(actual[i]==(int)expected[i]*120/128);
    gt4_pcm_music_close(reference);
    /* Replacing the callback in the destination screen must survive cleanup. */
    int destination;plat_audio_set_music(foreign,&destination);pregrid_audio_close(p);
    CHECK(owner==&destination && callback==foreign && stops==2);
    plat_audio_release_music(&destination);
    /* SlideRoot names a track explicitly, waits for EOF and can request the
       same track again. Preparing is already an active stream to its watcher. */
    p=pregrid_audio_create(argv[1]);CHECK(p);unsigned serial=1;
    pregrid_audio_update_ex(p,"racing",120,0,8,0,serial);CHECK(pregrid_audio_playing(p));
    uint32_t begin=SDL_GetTicks();while(!callback){CHECK(SDL_GetTicks()-begin<30000);pregrid_audio_update_ex(p,"racing",120,0,8,0,serial);SDL_Delay(5);}
    reference=gt4_pcm_music_open(v,"racing",8);CHECK(reference);gt4_pcm_music_once(reference,1);
    unsigned blocks=0;while(!gt4_pcm_music_ended(reference)){
        CHECK(++blocks<40000);callback(actual,2048,owner);gt4_pcm_music_render(expected,2048,reference);
        for(unsigned i=0;i<4096;i++)CHECK(actual[i]==(int)expected[i]*120/128);
    }
    CHECK(!pregrid_audio_playing(p));gt4_pcm_music_close(reference);
    serial++;pregrid_audio_update_ex(p,"racing",120,0,8,0,serial);CHECK(pregrid_audio_playing(p));
    begin=SDL_GetTicks();while(!callback){CHECK(SDL_GetTicks()-begin<30000);pregrid_audio_update_ex(p,"racing",120,0,8,0,serial);SDL_Delay(5);}
    reference=gt4_pcm_music_open(v,"racing",8);CHECK(reference);callback(actual,2048,owner);gt4_pcm_music_render(expected,2048,reference);
    for(unsigned i=0;i<4096;i++)CHECK(actual[i]==(int)expected[i]*120/128);
    gt4_pcm_music_close(reference);pregrid_audio_close(p);CHECK(!callback);
    /* Decode under the loader, remain silent through its fade, publish on
       the first destination update without another random choice/worker. */
    p=pregrid_audio_create(argv[1]);CHECK(p);begin=SDL_GetTicks();
    do{
        pregrid_audio_update_gated(p,"default_quick",120,0,0,1,42,getenv("GT4_PCM_LOAD_GATE_NEGATIVE")!=NULL);
        CHECK(!callback && !owner);CHECK(SDL_GetTicks()-begin<30000);SDL_Delay(1);
    }while(!pregrid_audio_prepared(p));
    pregrid_audio_update_gated(p,"default_quick",120,0,0,1,42,getenv("GT4_PCM_LOAD_GATE_NEGATIVE")!=NULL);
    CHECK(!callback && !owner);
    unsigned before=starts;
    pregrid_audio_update_gated(p,"default_quick",120,0,0,1,42,1);
    CHECK(callback && starts==before+1);
    reference=gt4_pcm_music_open(v,"default_quick",0);CHECK(reference);
    callback(actual,2048,owner);gt4_pcm_music_render(expected,2048,reference);
    for(unsigned i=0;i<4096;i++)CHECK(actual[i]==(int)expected[i]*120/128);
    gt4_pcm_music_close(reference);pregrid_audio_close(p);CHECK(!callback);
    gt4_vol_close(v);SDL_Quit();
    puts("PASS PCM app bridge: both tracks, raw volume, mute, stop, restart, cancelled request and callback ownership.");return 0;
}
