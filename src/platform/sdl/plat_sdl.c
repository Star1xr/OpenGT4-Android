/*
 * SDL2 backend: time, input events, dialogs and audio. Video (window,
 * OpenGL 2D and 3D) is in video_gl.c.
 */
#include "platform/platform.h"
#include "plat_sdl.h"

#include <SDL.h>
#include <stdlib.h>
#include <stdio.h>

static int           quit;
static unsigned music_fade_left,music_fade_total;
/* Playback must not repeatedly reopen a failed endpoint on the UI thread.
   A device-added event or explicit initialization can retry it. */
static int           audio_unavailable;
static Uint32        audio_failed_at;
static int           dialog_answer = -1;

void plat_sdl_dialog_answer(int answer) { dialog_answer = answer; }
int plat_sdl_get_dialog_answer(void){return dialog_answer;}

int plat_dialog(const char *title, const char *message, int confirm)
{
    if (dialog_answer >= 0) return confirm ? dialog_answer : 1;
    /* In-game confirmations require the app presenter; never open OS UI. */
    (void)title;(void)message;return -1;
}

uint64_t plat_time_ns(void)
{
    return (uint64_t)((double)SDL_GetPerformanceCounter() * 1e9 / (double)SDL_GetPerformanceFrequency());
}

void plat_sleep_ms(uint32_t ms) { SDL_Delay(ms); }

int plat_should_quit(void) { return quit; }

int plat_sdl_poll(plat_key_event *ev)
{
    SDL_Event e;memset(ev,0,sizeof *ev);
    plat_sdl_pad(0);
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            quit = 1;
        } else if(e.type==SDL_AUDIODEVICEADDED && !e.adevice.iscapture){
            if(audio_unavailable && (Sint32)(e.common.timestamp-audio_failed_at)>0)audio_unavailable=0;
        } else if(e.type==SDL_MOUSEMOTION && SDL_GetRelativeMouseMode()){
            ev->mouse=1;ev->mouse_x=e.motion.xrel;ev->mouse_y=e.motion.yrel;return 1;
        } else if(e.type==SDL_MOUSEWHEEL){
            ev->mouse=1;ev->wheel=e.wheel.y*(e.wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-1:1);return 1;
        } else if(e.type==SDL_TEXTINPUT){ev->key=0;snprintf(ev->text,sizeof ev->text,"%s",e.text.text);return 1;
        } else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            ev->key = e.key.keysym.sym;
            ev->ps2key = plat_sdl_ps2_key(&e, &ev->release);
            return 1;
        } else if (e.type == SDL_KEYUP) {
            ev->ps2key = plat_sdl_ps2_key(&e, &ev->release);
            if (ev->ps2key) { ev->key = 0; return 1; }
        } else if(e.type==SDL_CONTROLLERBUTTONDOWN || e.type==SDL_CONTROLLERBUTTONUP || e.type==SDL_CONTROLLERAXISMOTION){
            ev->ps2key=plat_sdl_ps2_key(&e,&ev->release);
            ev->key=e.type==SDL_CONTROLLERBUTTONUP?0:plat_sdl_controller_key(&e);
            if(ev->key || ev->ps2key){ev->pad=1;return 1;}
        }
    }
    return 0;
}

/* PCM voices are copied on submission, mixed on SDL's audio thread. */
static SDL_AudioDeviceID audio_device;
/* P: opt-in recording of this process's final PCM mix, excluding other
   desktop applications. First uint64 is the first block's monotonic ns;
   the remainder is 48000 Hz stereo S16. Disabled in ordinary playback. */
static FILE *audio_capture;
static int audio_capture_started;
static struct { int16_t *pcm; size_t frames, pos; } voices[16];
static plat_audio_cb audio_callback;
static void *audio_user;
/* Single producer / single consumer. Music synthesis never runs in the
   device callback; 170 ms of headroom absorbs scheduler/driver stalls. */
enum { MUSIC_RING=16384, MUSIC_AHEAD=8192, MUSIC_BLOCK=512 };
static int16_t music_ring[MUSIC_RING*2];
static SDL_atomic_t music_read,music_write,music_stop,music_underruns,music_played;
/* The last block the device took, under the device lock. */
static int music_block_start,music_block_frames;
static Uint64 music_block_stamp;
static SDL_Thread *music_thread;
static void music_produce(void)
{
    int16_t block[MUSIC_BLOCK*2];
    int r=SDL_AtomicGet(&music_read),w=SDL_AtomicGet(&music_write);
    int queued=(w-r)&(MUSIC_RING-1);
    if(queued>=MUSIC_AHEAD) return;
    audio_callback(block,MUSIC_BLOCK,audio_user);
    for(int i=0;i<MUSIC_BLOCK;i++) {
        int at=(w+i)&(MUSIC_RING-1);
        music_ring[at*2]=block[i*2];music_ring[at*2+1]=block[i*2+1];
    }
    SDL_AtomicSet(&music_write,(w+MUSIC_BLOCK)&(MUSIC_RING-1));
}
static int music_worker(void *unused)
{
    (void)unused;SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
    while(!SDL_AtomicGet(&music_stop)) {
        int queued=(SDL_AtomicGet(&music_write)-SDL_AtomicGet(&music_read))&(MUSIC_RING-1);
        if(queued<MUSIC_AHEAD) music_produce(); else SDL_Delay(1);
    }
    return 0;
}

static void audio_mix(void *unused, Uint8 *stream, int bytes)
{
    (void)unused;
    int16_t *out=(int16_t *)stream; int frames=bytes/4;
    SDL_memset(stream,0,(size_t)bytes);
    if(audio_callback) {
        int r=SDL_AtomicGet(&music_read),w=SDL_AtomicGet(&music_write);
        int available=(w-r)&(MUSIC_RING-1),take=frames<available?frames:available;
        for(int i=0;i<take;i++) {
            int at=(r+i)&(MUSIC_RING-1);
            out[i*2]=music_ring[at*2];out[i*2+1]=music_ring[at*2+1];
        }
        SDL_AtomicSet(&music_read,(r+take)&(MUSIC_RING-1));
        /* The device plays this block over the next frames/48000 s. */
        music_block_start=SDL_AtomicGet(&music_played);music_block_frames=take;
        music_block_stamp=SDL_GetPerformanceCounter();
        SDL_AtomicAdd(&music_played,take);
        if(take<frames)SDL_AtomicAdd(&music_underruns,1);
    }
    for (int f=0;f<frames;f++) {
        int l=out[f*2],r=out[f*2+1];
        if(music_fade_total){
            l=(int)((int64_t)l*music_fade_left/music_fade_total);
            r=(int)((int64_t)r*music_fade_left/music_fade_total);
            if(music_fade_left)music_fade_left--;
        }
        for (int v=0;v<16;v++) if (voices[v].pcm && voices[v].pos<voices[v].frames) {
            size_t p=voices[v].pos++*2;
            l+=voices[v].pcm[p]; r+=voices[v].pcm[p+1];
        }
        out[f*2]=(int16_t)(l<-32768?-32768:l>32767?32767:l);
        out[f*2+1]=(int16_t)(r<-32768?-32768:r>32767?32767:r);
    }
    if(audio_capture){
        if(!audio_capture_started){uint64_t stamp=plat_time_ns();fwrite(&stamp,sizeof stamp,1,audio_capture);audio_capture_started=1;}
        fwrite(stream,1,(size_t)bytes,audio_capture);
    }
}
int plat_audio_init(const plat_audio_desc *desc, plat_audio_cb cb, void *user)
{
    if (audio_device) return 0;
    if (desc->sample_rate!=48000 || desc->channels!=2)return -1;
    if(SDL_InitSubSystem(SDL_INIT_AUDIO)!=0){audio_unavailable=1;audio_failed_at=SDL_GetTicks();return -1;}
    if(SDL_GetNumAudioDevices(0)==0) {
        audio_unavailable=1;audio_failed_at=SDL_GetTicks();
        SDL_Log("Audio unavailable: no playback device; continuing without audio");
        return -1;
    }
    SDL_AudioSpec spec; SDL_zero(spec);
    spec.freq=48000; spec.format=AUDIO_S16SYS; spec.channels=2; spec.samples=1024; spec.callback=audio_mix;
    audio_device=SDL_OpenAudioDevice(NULL,0,&spec,NULL,0);
    if (!audio_device) { audio_unavailable=1;audio_failed_at=SDL_GetTicks();SDL_Log("Audio unavailable: %s",SDL_GetError()); return -1; }
    audio_unavailable=0;
    audio_callback=cb; audio_user=user;
    SDL_AtomicSet(&music_read,0);SDL_AtomicSet(&music_write,0);
    SDL_AtomicSet(&music_stop,0);SDL_AtomicSet(&music_underruns,0);
    if(cb) {
        for(int i=0;i<MUSIC_AHEAD/MUSIC_BLOCK;i++)music_produce();
        music_thread=SDL_CreateThread(music_worker,"OpenGT4 music",NULL);
        if(!music_thread) { SDL_CloseAudioDevice(audio_device);audio_device=0;audio_callback=NULL;audio_user=NULL;return -1; }
    }
    {const char *path=getenv("OPENGT4_CAPTURE_AUDIO");if(path && *path)audio_capture=fopen(path,"wb");audio_capture_started=0;}
    SDL_PauseAudioDevice(audio_device,0); return 0;
}
int plat_audio_set_music(plat_audio_cb cb,void *user)
{
    plat_audio_desc desc={48000,2};
    if(!audio_device && (audio_unavailable || plat_audio_init(&desc,NULL,NULL)))return -1;
    SDL_AtomicSet(&music_stop,1);if(music_thread){SDL_WaitThread(music_thread,NULL);music_thread=NULL;}
    SDL_LockAudioDevice(audio_device);
    music_fade_left=music_fade_total=0;
    audio_callback=cb;audio_user=user;SDL_AtomicSet(&music_read,0);SDL_AtomicSet(&music_write,0);SDL_AtomicSet(&music_stop,0);
    SDL_AtomicSet(&music_played,0);music_block_start=music_block_frames=0;music_block_stamp=0;
    int result=0;
    if(cb){for(int i=0;i<MUSIC_AHEAD/MUSIC_BLOCK;i++)music_produce();music_thread=SDL_CreateThread(music_worker,"OpenGT4 music",NULL);
        if(!music_thread){audio_callback=NULL;audio_user=NULL;result=-1;}}
    SDL_UnlockAudioDevice(audio_device);return result;
}
void plat_audio_release_music(void *user)
{
    if(audio_device && audio_callback && audio_user==user)plat_audio_set_music(NULL,NULL);
}
uint64_t plat_audio_music_position(void)
{
    /* The device takes 1024-frame blocks (21 ms); counting only whole blocks
       makes a clock that jumps, and pictures following it skip frames.
       Advance through the current block by elapsed time instead. */
    if(!audio_device)return (uint64_t)(unsigned)SDL_AtomicGet(&music_played);
    SDL_LockAudioDevice(audio_device);
    int start=music_block_start,frames=music_block_frames;Uint64 stamp=music_block_stamp;
    SDL_UnlockAudioDevice(audio_device);
    if(!stamp)return (uint64_t)(unsigned)SDL_AtomicGet(&music_played);
    Uint64 into=(SDL_GetPerformanceCounter()-stamp)*48000/SDL_GetPerformanceFrequency();
    return (uint64_t)(unsigned)start+(into<(Uint64)frames?into:(Uint64)frames);
}
int plat_audio_play(const int16_t *pcm, size_t frames, int rate, int channels)
{
    if (!pcm || !frames || frames>48000*30 || rate!=48000 || channels!=2) return 0;
    plat_audio_desc desc={48000,2};
    if (!audio_device && (audio_unavailable || plat_audio_init(&desc,NULL,NULL)!=0)) return 0;
    int16_t *copy=malloc(frames*4); if (!copy) return 0;
    SDL_memcpy(copy,pcm,frames*4);
    SDL_LockAudioDevice(audio_device);
    int slot=-1;
    for (int i=0;i<16;i++) if (!voices[i].pcm || voices[i].pos==voices[i].frames) { slot=i; break; }
    int16_t *old=NULL;
    if (slot>=0) { old=voices[slot].pcm; voices[slot].pcm=copy; voices[slot].frames=frames; voices[slot].pos=0; }
    SDL_UnlockAudioDevice(audio_device);
    free(old);
    if (slot<0) free(copy);
    return slot>=0;
}
void plat_audio_shutdown(void)
{
    SDL_AtomicSet(&music_stop,1);
    if(music_thread) { SDL_WaitThread(music_thread,NULL);music_thread=NULL; }
    if (audio_device) SDL_CloseAudioDevice(audio_device);
    if(audio_capture){fclose(audio_capture);audio_capture=NULL;}audio_capture_started=0;
    audio_device=0; audio_callback=NULL; audio_user=NULL;audio_unavailable=0;
    for (int i=0;i<16;i++) { free(voices[i].pcm); SDL_memset(&voices[i],0,sizeof voices[i]); }
}

unsigned plat_sdl_audio_underruns(void) { return (unsigned)SDL_AtomicGet(&music_underruns); }

void plat_audio_fade_music(float seconds)
{
    if(!audio_device)return;
    SDL_LockAudioDevice(audio_device);
    music_fade_total=music_fade_left=seconds>0 && seconds<60?(unsigned)(seconds*48000):1;
    SDL_UnlockAudioDevice(audio_device);
}
