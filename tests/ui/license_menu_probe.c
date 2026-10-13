/* 0011D1B8 +44..60 and 0012BFE0 +28..7C: the license menu
   retains the complete Scene. This verifies the native draw bridge,
   not camera fidelity or equivalence to the GS rasterizer. */
#include <SDL.h>
#include "plat_sdl.h"
#include "platform/platform.h"
#include "render3d.h"
#include "miniz.h"
#include "career_license_disc.h"
#include "../../src/app/race_game.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"license menu line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint8_t *frame(race_game *g,int *w,int *h,int missing)
{
    plat_video_begin_frame();
    int ok=missing?race_view_draw_pregrid(g->view):race_game_draw_pregrid(g);
    uint8_t *pixels=ok?plat_sdl_read_pixels(w,h):NULL;
    plat_video_end_frame();return pixels;
}
int main(int argc,char **argv)
{
    CHECK(argc==3 || argc==4);
    int negative=argc==4 && !strcmp(argv[3],"--negative");
    CHECK(!plat_video_init(&(plat_video_desc){960,720,0,0,0}));
    SDL_HideWindow(SDL_GetWindowFromID(1));plat_video_set_view(640,480);
    race_game *g=race_game_open(argv[1]);CHECK(g);
    gt4_career_race_lookup_data lookup;gt4_career_license_exam exam;char error[256];
    CHECK(gt4_career_race_lookup_data_load(argv[1],&lookup));
    CHECK(gt4_career_license_prepare(&exam,argv[1],&lookup,"l0b0001",error,sizeof error));
    CHECK(race_game_start_prepared(g,&exam.desc,error,sizeof error));
    int ready=0;for(int i=0;i<2000 && !ready;i++){SDL_PumpEvents();SDL_Delay(5);ready=race_game_poll(g);}
    CHECK(ready==1 && race_game_license_active(g));
    float initial_pos[3],rot[9],wheels[4][3];CHECK(gt4_race_run_pose(g->run,0,initial_pos,rot,wheels));
    gt4_replay_file saved;CHECK(race_game_read_replay(g,"/replay/license/usl0b0001",&saved,error,sizeof error));
    size_t saved_steps=saved.steps;g->tape=saved.tape;saved.tape=NULL;gt4_replay_free(&saved);
    g->tape_n=g->tape_cap=saved_steps;g->thumb_on=2;g->license_log.count=2;
    uint8_t *attempt=g->tape;gt4_logger_sampler log=g->license_log;
    gt4_race_hud hud;CHECK(gt4_race_run_hud(g->run,0,&hud));
    printf("menu initial countdown: %d\n",hud.start_frames);
    for(int i=0;i<120;i++)race_game_pregrid_update(g,1./60);
    int w,h;uint8_t *visible=frame(g,&w,&h,negative);CHECK(visible);
    CHECK(r3d_frame_stats().wheel_passes+r3d_frame_stats().wheel_cache_hits>0);
    size_t len;void *png=tdefl_write_image_to_png_file_in_memory(visible,w,h,4,&len);
    FILE *out=png?fopen(argv[2],"wb"):NULL;CHECK(out);CHECK(fwrite(png,1,len,out)==len);fclose(out);mz_free(png);
    race_view_show_car(g->view,0);int w2,h2;uint8_t *hidden=frame(g,&w2,&h2,0);
    CHECK(hidden && w==w2 && h==h2);unsigned changed=0;
    for(size_t i=0;i<(size_t)w*h*4;i++)changed+=visible[i]!=hidden[i];
    CHECK(changed>0);race_view_show_car(g->view,1);free(visible);free(hidden);
    for(int i=0;i<1080;i++)race_game_pregrid_update(g,1./60);
    CHECK(g->license_menu_playing && g->license_menu_at>0 && !g->result);
    float moved[3];CHECK(gt4_race_run_pose(g->run,0,moved,rot,wheels));
    CHECK(memcmp(initial_pos,moved,sizeof moved));
    CHECK(g->tape==attempt && g->tape_n==saved_steps && g->thumb_on==2 && !memcmp(&g->license_log,&log,sizeof log));
    gt4_replay_file teacher;CHECK(race_game_read_replay(g,"/replay/license/usl0b0001",&teacher,error,sizeof error));
    if(getenv("GT4_LICENSE_MENU_DEMO_NEGATIVE"))g->license_menu_tape[4]^=1;
    CHECK(g->license_menu_steps==teacher.steps && !memcmp(g->license_menu_tape,teacher.tape,teacher.steps*32));
    size_t cursor=g->license_menu_at;g->license_menu_at=0;uint8_t pad_command[32],ev[32]={0};
    for(size_t i=0;i<teacher.steps;i++){CHECK(host_read(NULL,player,pad_command,ev));CHECK(!memcmp(pad_command,teacher.tape+i*32,32));}
    g->license_menu_at=cursor;gt4_replay_free(&teacher);
    /* Entire B-1 playback, including TV after the opening. The first run
       is a native determinism reference, not a PCSX2 trajectory oracle. */
    typedef struct {float pos[3],rot[9],wheels[4][3],eye[3],rows[9],lens[3],clock,distance;} stamp;
    stamp *sequence=calloc(g->license_menu_steps,sizeof *sequence);CHECK(sequence);
    unsigned cycle=0,samples=0,waiting=0;size_t previous=1;
    uint8_t *menu_tape=NULL;
    race_game_return_pregrid(g);
    while(cycle<3 && waiting++<20000){
        unsigned parts=cycle==0?1:cycle==1?2:4;
        for(unsigned i=0;i<parts;i++)race_game_pregrid_update(g,1./(60*parts));
        if(!menu_tape)menu_tape=g->license_menu_tape;
        CHECK(g->license_menu_tape==menu_tape && g->license_menu_steps==1471);
        CHECK(g->tape==attempt && g->tape_n==saved_steps && g->thumb_on==2 && !memcmp(&g->license_log,&log,sizeof log));
        if(g->license_menu_at==previous)continue;
        if(g->license_menu_at==1){previous=1;continue;}
        CHECK(g->license_menu_at==previous+1);previous=g->license_menu_at;
        CHECK(g->license_menu_ticks==previous-1 && g->license_menu_cycles==cycle);
        /* TV must consume the camera anchor, independently of the mesh origin.
           CarTarget 003FBE70 +E4..118, getters +554 blend. */
        gt4_tvcam_target target;CHECK(tv_target(g,0,&target));
        if(getenv("GT4_LICENSE_TV_TARGET_NEGATIVE"))memcpy(target.pos,g->license_menu_pos,12);
        uint32_t camera_car=gt4_race_run_car(g->run,0);float blend=GT4_F32(camera_car+0x554);
        for(unsigned i=0;i<3;i++){
            float expected=GT4_F32(camera_car+0x8a4+i*4)*blend+GT4_F32(camera_car+0x888+i*4)*(1-blend);
            CHECK(!memcmp(target.pos+i,&expected,4));
        }
        CHECK(target.distance==GT4_F32(camera_car+0x5a8));
        stamp s={0};CHECK(gt4_race_run_pose(g->run,0,s.pos,s.rot,s.wheels));
        CHECK(race_view_get_camera(g->view,s.eye,s.rows,s.lens));
        s.clock=gt4_license_motion_time(g->license_menu_motion,3);s.distance=g->license_menu_distance;
        if(!cycle)sequence[previous-1]=s;
        else{
            if(getenv("GT4_LICENSE_MENU_SYNC_NEGATIVE") && previous==600)s.pos[0]+=.1f;
            stamp *expected=&sequence[previous-1];
            CHECK(!memcmp(&s,expected,offsetof(stamp,eye)));
            CHECK(!memcmp(&s.clock,&expected->clock,2*sizeof(float)));
            unsigned kind=g->last_event_row[0x40]+1;
            int seconds=kind==2?8:kind==3?10:kind==4?0:g->last_event_row[0x41];
            if(previous<=(unsigned)(seconds*60+60))
                CHECK(!memcmp(s.eye,expected->eye,offsetof(stamp,clock)-offsetof(stamp,eye)));
            /* TV's random operator continues its RNG (0037C138), so
               different cycles can have different aiming/shake. */
            for(unsigned i=0;i<3;i++)CHECK(isfinite(s.eye[i]) && isfinite(s.lens[i]));
            for(unsigned i=0;i<9;i++)CHECK(isfinite(s.rows[i]));
        }
        samples++;if(previous==g->license_menu_steps){cycle++;}
    }
    CHECK(cycle==3 && samples==3*1470);free(sequence);
    puts("PASS 4410 playback samples plus 3 priming commands: car/wheels/distance/opening/clock repeat byte-identically at 60/120/240 Hz delivery; TV stays finite with original random operator. Native repeatability only.");
    race_game_return_pregrid(g);for(int i=0;i<120;i++)race_game_pregrid_update(g,1./60);
    visible=frame(g,&w,&h,0);CHECK(visible && r3d_frame_stats().wheel_passes+r3d_frame_stats().wheel_cache_hits>0);free(visible);
    if(argc==4 && !negative){
        uint8_t *reference=malloc(32u*1024*1024);CHECK(reference);FILE *in=fopen(argv[3],"rb");CHECK(in);
        CHECK(fread(reference,1,32u*1024*1024,in)==32u*1024*1024);fclose(in);
        #define WORD(a) (*(uint32_t *)(reference+(a)))
        #define FLOAT(a) (*(float *)(reference+(a)))
        uint32_t scene=0;for(uint32_t p=0x100000;p<0x2000000-0x25000;p+=4)
            if(WORD(p+0x64)==0x6807a8 && WORD(p+0x80)==0x67a078){scene=p;break;}
        CHECK(scene);uint32_t camera=scene+0x80,manager=WORD(camera+0x13c);
        CHECK(WORD(scene+0xcc8)==2 && WORD(WORD(scene+0x6c)+0xdc)==1);
        unsigned camera_kind=WORD(camera+0x138);CHECK(camera_kind==4 || camera_kind==1);
        /* 003EA8C8: F054/E0 select the first retained command recorder.
           00345D70 +30..38 advances its +40 counter before decoding.
           Saved opening camera is one tick behind this recorder. */
        uint32_t controller=scene+(WORD(scene+0xf054) || WORD(WORD(scene+0x6c)+0xe0)?0xe84c:0xe48c);
        uint32_t blob=WORD(controller+0x124);CHECK(blob && blob<0x2000000-0xeb04);
        uint8_t *original_tape=NULL;size_t original_steps=0;
        CHECK(gt4_disc_replay_commands(reference+blob,0xeb04,&original_tape,&original_steps));
        free(g->license_menu_tape);g->license_menu_tape=original_tape;g->license_menu_steps=original_steps;
        uint32_t race=WORD(scene+0x6c),entry=WORD(WORD(WORD(race+0x60)+8)),car=WORD(entry+0x18);
        CHECK(car && car<0x2000000-0x1000);
        /* Match the captured attempt's options, not the fixture defaults.
           00111B50/00111DC0/00110B70: entry gearbox +3448, ASM +3450,
           TCS +3454. This changes only this reference test's descriptor. */
        g->license_menu_desc.manual=WORD(entry+0x3448)!=0;
        g->license_menu_desc.assist_asm=WORD(entry+0x3450)!=0;
        g->license_menu_desc.assist_tcs=WORD(entry+0x3454)!=0;
        CHECK(license_menu_reset(g,error,sizeof error));memset(&g->license_menu_cycle,0,sizeof g->license_menu_cycle);
        CHECK(license_menu_prime(g));
        g->pregrid_acc=0;unsigned ticks=0,target_tick=WORD(controller+0x11c)-1;
        while(ticks<target_tick){race_game_pregrid_update(g,1./60);ticks++;}
        CHECK(g->license_menu_at==target_tick+1 && g->license_menu_ticks==target_tick);
        CHECK(!memcmp(g->license_menu_tape+target_tick*32,reference+controller+0xf4,32));
        float pose[3];CHECK(gt4_race_run_pose(g->run,0,pose,rot,wheels));
        for(unsigned i=0;i<3;i++){
            float t=FLOAT(car+0x554),expected=FLOAT(car+0x8b0+i*4)*t+FLOAT(car+0x894+i*4)*(1-t);
            printf("reference car %u at tick %u: native %.9g PS2 %.9g error %.9g\n",i,ticks,pose[i],expected,fabsf(pose[i]-expected));
            CHECK(fabsf(pose[i]-expected)<=.001f);
        }
        if(camera_kind==4){
        float clock=FLOAT(manager+0x188);
        CHECK(fabsf(gt4_license_motion_time(g->license_menu_motion,3)-clock)<.00001f);
        float eye[3],rows[9],lens[3];CHECK(race_view_get_camera(g->view,eye,rows,lens));
        if(getenv("GT4_LICENSE_MENU_REFERENCE_NEGATIVE"))eye[0]+=.1f;
        for(unsigned i=0;i<3;i++){
            float expected=FLOAT(manager+0x10+i*4);printf("reference eye %u: native %.9g PS2 %.9g error %.9g\n",i,eye[i],expected,fabsf(eye[i]-expected));
            CHECK(fabsf(eye[i]-expected)<=.001f);
        }
        CHECK(fabsf(lens[0]-FLOAT(manager+0x1c))<=.00001f && fabsf(lens[1]-FLOAT(manager+0x24))<=.00001f);
        }
        printf("PASS slot reference at tick %u: original command cursor and car within 1 mm; opening camera/lens checked when kind 4. TV matrix/rasterization outside comparison.\n",ticks);free(reference);
        race_game_return_pregrid(g);race_game_pregrid_update(g,1./60);
    }
    CHECK(g->tape==attempt && g->tape_n==saved_steps && g->thumb_on==2 && !memcmp(&g->license_log,&log,sizeof log));
    CHECK(!memcmp(attempt,g->license_menu_tape,saved_steps*32));
    CHECK(race_game_restart(g,error,sizeof error));ready=0;
    for(int i=0;i<2000 && !ready;i++){SDL_PumpEvents();SDL_Delay(5);ready=race_game_poll(g);}
    CHECK(ready==1 && !g->license_menu_playing && !g->tape_n);
    CHECK(gt4_race_run_pose(g->run,0,moved,rot,wheels) && !memcmp(initial_pos,moved,sizeof moved));
    /* Original pause.strb EXIT_MENU("retry") -> state 5, including 1P.
       The app consumes it and rebuilds the same prepared examination. */
    CHECK(!g->split);pause_toggle(g);CHECK(g->pause);
    for(int i=0;i<60;i++)draw_pause(g,1./60);
    race_pause_key(g->pause,40);race_pause_key(g->pause,13);
    for(int i=0;i<60;i++)draw_pause(g,1./60);
    CHECK(!g->pause);
    if(getenv("GT4_PAUSE_RETRY_NEGATIVE"))g->retry_requested=0;
    CHECK(race_game_take_retry(g) && !race_game_take_retry(g));
    CHECK(race_game_restart(g,error,sizeof error));ready=0;
    for(int i=0;i<2000 && !ready;i++){SDL_PumpEvents();SDL_Delay(5);ready=race_game_poll(g);}
    CHECK(ready==1 && !g->tape_n && !g->quit && !g->replaying);
    CHECK(gt4_race_run_pose(g->run,0,moved,rot,wheels) && !memcmp(initial_pos,moved,sizeof moved));
    printf("PASS full license menu draws wheels and %u differing color bytes; retained attempt loops, return preserves player tape/thumbnail/logger, Start restores initial exam.\n",changed);
    race_game_close(g);plat_video_shutdown();return 0;
}
