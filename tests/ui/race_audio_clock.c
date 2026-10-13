/* The actual app, fixed physics/input, at 60/30/15 drawing updates per
   second. Every 60 Hz sound tick must see the same camera and produce
   the same engine registers and PCM, including constant-RPM layers.
   This is a PC transport regression, not an original-waveform oracle. */
#include <SDL.h>
#include "plat_sdl.h"
#include "platform/platform.h"
static int fixture_music(plat_audio_cb cb,void *user){(void)cb;(void)user;return 0;}
static void fixture_release(void *user){(void)user;}
#define plat_audio_set_music fixture_music
#define plat_audio_release_music fixture_release
#include "../../src/app/race_audio.c"
#undef plat_audio_set_music
#undef plat_audio_release_music
static Uint8 fixture_key[SDL_NUM_SCANCODES];
static const Uint8 *fixture_keys(int *n){if(n)*n=SDL_NUM_SCANCODES;return fixture_key;}
static void observed_audio_step(race_audio *a,const gt4_race_run *run);
#define SDL_GetKeyboardState fixture_keys
#define race_audio_step observed_audio_step
#include "../../src/app/race_game.c"
#undef race_audio_step
#undef SDL_GetKeyboardState
#define CHECK(x) do{if(!(x)){fprintf(stderr,"audio clock line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
enum { TICKS=1200 };
typedef struct { float eye[3];gt4_engine_control engine[3];uint16_t pitch[VOICES];int16_t pcm[800*2]; } clock_frame;
static clock_frame reference[TICKS];
static int frame,batch,negative;
static int capture_stems;
static int listener_negative,chase_frames,manual_mode;
static float previous_eye[3];
static void observed_audio_step(race_audio *a,const gt4_race_run *run)
{
    CHECK(a && frame<TICKS);
    if(capture_stems && batch>1)CHECK(a->bgm_option==-1 && a->music_volume==0);
    /* Only the sound observation is held at 3500 RPM. Restore the sensor
       immediately: physics and input are identical across the runs. */
    unsigned car=gt4_race_run_car(run,0);uint16_t rpm=GT4_U16(car+0x104+0x4fa);
    GT4_U16(car+0x104+0x4fa)=3500;
    if(negative && batch>1 && frame%batch)memcpy(a->camera_eye,previous_eye,sizeof previous_eye);
    if(a->camera_view==1 && frame>=1000){
        gt4_car_listener l;listener(a,&l,a->camera_eye,a->camera_rows,a->camera_view);
        /* 00379448 f12=0 current +10; f12=1 preceding +7C.
           Catching up with our own car must not transpose its pitch. */
        if(listener_negative){float swap[3];memcpy(swap,l.eye,sizeof swap);memcpy(l.eye,l.eye_previous,sizeof swap);memcpy(l.eye_previous,swap,sizeof swap);}
        CHECK(!memcmp(l.eye_previous,a->camera_eye,sizeof l.eye_previous));
        CHECK(!memcmp(l.eye,a->eye_previous,sizeof l.eye));
        unsigned cond=gt4_race_run_conductor(run),entry=GT4_U32(GT4_U32(GT4_U32(GT4_U32(cond)+0x60)+8));
        CHECK(GT4_U32(entry+0x3448)==manual_mode);
        CHECK(GT4_U8(car+0x546)==manual_mode);
        gt4_car_sound_state st=a->state[0];gt4_car_sound_frame f;gt4_car_sound_out out;
        gt4_car_sound_frame_build(&f,&st,&out,entry,&l,1,0,0);
        CHECK(fabsf(*(float*)(f.b+0x50)-1.f)<0.0001f);chase_frames++;
    }
    race_audio_step(a,run);GT4_U16(car+0x104+0x4fa)=rpm;
    clock_frame actual={0};memcpy(actual.eye,a->camera_eye,sizeof actual.eye);
    for(int k=0;k<3;k++)actual.engine[k]=a->engine[0][k].control;
    for(int i=0;i<VOICES;i++)actual.pitch[i]=a->regs[i].pitch;
    render(actual.pcm,800,a);
    if(batch==1)reference[frame]=actual;
    else if(memcmp(&reference[frame],&actual,sizeof actual)) {
        fprintf(stderr,"audio clock differs at tick %d with %d steps per draw\n",frame,batch);exit(1);
    }
    memcpy(previous_eye,a->camera_eye,sizeof previous_eye);frame++;
}
int main(int argc,char **argv)
{
    CHECK(argc>=2);negative=argc==3 && !strcmp(argv[2],"--negative");
    manual_mode=argc==3 && (!strcmp(argv[2],"--manual") || !strcmp(argv[2],"--manual-negative"));
    listener_negative=argc==3 && !strcmp(argv[2],"--listener-negative");
    int mute_negative=argc==3 && !strcmp(argv[2],"--capture-stems-negative");
    int stems=capture_stems=argc==3 && (!strcmp(argv[2],"--capture-stems") || mute_negative);
    plat_video_desc desc={640,448,0,0,0};CHECK(!plat_video_init(&desc));SDL_HideWindow(SDL_GetWindowFromID(1));
    const char *cars[]={"crx_sir_90","mustang_gt_05"};
    for(int c=0;c<2;c++)for(batch=1;batch<=4;batch*=2) {
        race_game *g=race_game_open(argv[1]);char msg[256];CHECK(g);
        race_game_set_driving(g,0,manual_mode && !(argc==3 && !strcmp(argv[2],"--manual-negative")),0,0);
        CHECK(race_game_start(g,"tsukuba",cars[c],0,NULL,NULL,0,msg,sizeof msg));
        int ready=0;for(int i=0;i<4000 && !ready;i++){SDL_PumpEvents();SDL_Delay(5);ready=race_game_poll(g);}CHECK(ready==1);
        unsigned cond=gt4_race_run_conductor(g->run),race=GT4_U32(cond);
        GT4_U32(race+0xbc)=0;GT4_U32(cond+0xcbfc)=0;
        if(stems){
#ifdef _WIN32
            _putenv_s("OPENGT4_CAPTURE_NO_MUSIC",batch>1 && !mute_negative?"1":"");
#else
            if(batch>1 && !mute_negative)setenv("OPENGT4_CAPTURE_NO_MUSIC","1",1);
            else unsetenv("OPENGT4_CAPTURE_NO_MUSIC");
#endif
        }
        race_game_set_audio(g,stems && batch>1?120:0,stems && batch>1,120);race_game_update(g,0);
        fixture_key[SDL_SCANCODE_UP]=1;frame=0;memset(previous_eye,0,sizeof previous_eye);
        /* Avoid binary subtraction rounding the last 1/60 just below STEP. */
        for(int i=0;i<TICKS/batch;i++)race_game_update(g,STEP*batch+1e-12);
        CHECK(frame==TICKS);fixture_key[SDL_SCANCODE_UP]=0;race_game_close(g);
        printf("PASS %s: %d ticks at %d steps/draw, camera, layers, pitch and PCM identical.\n",cars[c],TICKS,batch);
    }
    CHECK(chase_frames>0);
    plat_video_shutdown();return 0;
}
