/* PCM stream bridge: quick-arcade.ad L159, 0023E740 and 0045FFF8.
   Keep it independent of the menu MIDI worker: these files live in /bgm. */
#include "pregrid_audio.h"
#include "pcm_music.h"
#include "platform/platform.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct pregrid_audio {
    const char *disc;
    gt4_pcm_music *pcm;
    SDL_Thread *thread;
    SDL_atomic_t ready,volume,ended,paused;
    char group[64],worker_group[64],loaded_group[64];
    int selected,repeat,worker_selected,worker_repeat;unsigned serial;
    uint32_t random_state,pending_random,worker_random;
    unsigned request,worker_request,sequence;
    int wanted,playing;
};
static void render(int16_t *out,int frames,void *user)
{
    pregrid_audio *p=user;
    if(SDL_AtomicGet(&p->paused)){memset(out,0,(size_t)frames*4);return;}   /* sound.pauseStream: held where it is */
    gt4_pcm_music_render(out,frames,p->pcm);
    if(gt4_pcm_music_ended(p->pcm))SDL_AtomicSet(&p->ended,1);
    /* quick-arcade.ad L159 / 00180AA8 +0x78..+0xA4: saved raw /128. */
    int volume=SDL_AtomicGet(&p->volume);
    if(volume!=128)for(int i=0;i<frames*2;i++)out[i]=(int16_t)((int)out[i]*volume/128);
}
static int prepare(void *user)
{
    pregrid_audio *p=user;const char *error=NULL;
    gt4_vol *v=gt4_vol_open(p->disc,&error);
    unsigned count=v?gt4_pcm_music_count(v,p->worker_group):0;
    unsigned sequence=count?(p->worker_selected<0?p->worker_random:(unsigned)p->worker_selected)%count:0;
    /* The old callback has been detached before a new request starts.
       Reuse a decoded track when the original random selector picks it again. */
    if(!p->pcm || p->sequence!=sequence || strcmp(p->loaded_group,p->worker_group) || !count){
        gt4_pcm_music_close(p->pcm);p->pcm=NULL;
        if(count)p->pcm=gt4_pcm_music_open(v,p->worker_group,sequence);
    }
    p->sequence=sequence;snprintf(p->loaded_group,sizeof p->loaded_group,"%s",p->worker_group);
    gt4_pcm_music_once(p->pcm,!p->worker_repeat);if(v)gt4_vol_close(v);
    if(!p->pcm)fprintf(stderr,"music: cannot decode PCM/%s\n",p->worker_group);
    SDL_AtomicSet(&p->ready,1);return 0;
}
pregrid_audio *pregrid_audio_create(const char *disc)
{
    pregrid_audio *p=calloc(1,sizeof *p);if(p)p->disc=disc;return p;
}
void pregrid_audio_update(pregrid_audio *p,const char *group,int volume,uint32_t clock)
{pregrid_audio_update_ex(p,group,volume,clock,-1,1,0);}
void pregrid_audio_pause(pregrid_audio *p,int paused){if(p)SDL_AtomicSet(&p->paused,paused!=0);}
int pregrid_audio_playing(pregrid_audio *p){return p && p->wanted && ((!p->playing && (p->thread || p->worker_request!=p->request)) || (p->playing && !SDL_AtomicGet(&p->ended)));}
int pregrid_audio_prepared(pregrid_audio *p){return p && p->wanted && p->request==p->worker_request && SDL_AtomicGet(&p->ready);}
void pregrid_audio_update_ex(pregrid_audio *p,const char *group,int volume,uint32_t clock,int sequence,int repeat,unsigned serial)
{pregrid_audio_update_gated(p,group,volume,clock,sequence,repeat,serial,1);}
void pregrid_audio_update_gated(pregrid_audio *p,const char *group,int volume,uint32_t clock,int sequence,int repeat,unsigned serial,int publish)
{
    if(!p)return;
    int wanted=group && *group && strlen(group)<sizeof p->group;
    if(!wanted){
        /* Owner check prevents this stream from stopping a movie or MIDI
           source which the destination menu has installed in the meantime. */
        if(p->playing)plat_audio_release_music(p);
        p->playing=0;p->wanted=0;return;
    }
    if(volume<0)volume=0;if(volume>128)volume=128;
    SDL_AtomicSet(&p->volume,volume);
    if(!p->wanted || strcmp(p->group,group) || p->selected!=sequence || p->repeat!=repeat || p->serial!=serial){
        if(p->playing)plat_audio_release_music(p);p->playing=0;
        snprintf(p->group,sizeof p->group,"%s",group);p->selected=sequence;p->repeat=repeat;p->serial=serial;
        p->wanted=1;p->request++;
        /* 0045FFF8 +0x48, 00579330/00579358. A: PC clock replaces PS2 timer. */
        p->pending_random=gt4_pcm_music_random(&p->random_state,clock);
    }
    if(p->thread){
        if(!SDL_AtomicGet(&p->ready))return;
        SDL_WaitThread(p->thread,NULL);p->thread=NULL;
    }
    if(p->worker_request!=p->request){
        p->worker_request=p->request;p->worker_random=p->pending_random;
        snprintf(p->worker_group,sizeof p->worker_group,"%s",p->group);p->worker_selected=p->selected;p->worker_repeat=p->repeat;
        SDL_AtomicSet(&p->ready,0);
        p->thread=SDL_CreateThread(prepare,"pregrid PCM preparation",p);
        if(!p->thread)fprintf(stderr,"music: cannot start PCM worker: %s\n",SDL_GetError());
        return;
    }
    if(!publish && p->playing){plat_audio_release_music(p);p->playing=0;}
    if(publish && !p->playing && SDL_AtomicGet(&p->ready) && p->pcm){
        gt4_pcm_music_rewind(p->pcm);SDL_AtomicSet(&p->ended,0);
        p->playing=plat_audio_set_music(render,p)==0;
        if(p->playing){printf("music: PCM/%s sequence=%u, 48000 Hz\n",p->loaded_group,p->sequence);fflush(stdout);}
    }
}
void pregrid_audio_close(pregrid_audio *p)
{
    if(!p)return;
    if(p->playing)plat_audio_release_music(p);
    if(p->thread)SDL_WaitThread(p->thread,NULL);
    gt4_pcm_music_close(p->pcm);free(p);
}
