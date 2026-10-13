/* Read-only real-disc race presentation/inspection probe. Local PNG output
   contains user-disc assets and must stay outside Git. Not a PS2 oracle. */
#include <SDL.h>
#include "plat_sdl.h"
#include "platform/platform.h"
#include "miniz.h"
#include "render3d.h"
#include "video_gl.h"
#include "career_license_disc.h"
#include "car_model.h"
static Uint8 fixture_key[SDL_NUM_SCANCODES];
static const Uint8 *fixture_keys(int *n){if(n)*n=SDL_NUM_SCANCODES;return fixture_key;}
static Uint32 fixture_window_flags(SDL_Window *w){(void)w;return SDL_WINDOW_INPUT_FOCUS;}
/* Exercise the app's actual keyboard code without sending OS input. */
#define SDL_GetKeyboardState fixture_keys
#define SDL_GetWindowFlags fixture_window_flags
#include "../../src/app/race_game.c"
#undef SDL_GetKeyboardState
#undef SDL_GetWindowFlags
#define CHECK(x) do{if(!(x)){fprintf(stderr,"inspection line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int shot(race_game *g,const char *prefix,const char *name)
{
    plat_video_begin_frame();if(!race_game_draw(g))return 0;
    int w,h;size_t len;uint8_t *p=plat_sdl_read_pixels(&w,&h);
    void *png=p?tdefl_write_image_to_png_file_in_memory(p,w,h,4,&len):NULL;
    char path[512];snprintf(path,sizeof path,"%s-%s.png",prefix,name);
    FILE *o=png?fopen(path,"wb"):NULL;if(o){fwrite(png,1,len,o);fclose(o);}
    free(p);mz_free(png);plat_video_end_frame();return o!=NULL;
}
int main(int argc,char **argv)
{
    if(argc<3)return 2;
    plat_video_desc desc={1280,896,0,0,0};CHECK(!plat_video_init(&desc));
    SDL_HideWindow(SDL_GetWindowFromID(1));plat_video_set_view(640,448);
    race_game *g=race_game_open(argv[1]);char msg[256];CHECK(g);
    const char *others[]={getenv("PROBE_RIVAL")?getenv("PROBE_RIVAL"):"ka_01","z_act_70","today_g_85","midget2_d_type_98","s500_63"};
    if(getenv("PROBE_LICENSE")){
        gt4_career_race_lookup_data lookup;gt4_career_license_exam exam;
        CHECK(gt4_career_race_lookup_data_load(argv[1],&lookup));
        CHECK(gt4_career_license_prepare(&exam,argv[1],&lookup,getenv("PROBE_LICENSE"),msg,sizeof msg));
        CHECK(race_game_start_prepared(g,&exam.desc,msg,sizeof msg));
    }else CHECK(race_game_start(g,"tsukuba",getenv("PROBE_CAR")?getenv("PROBE_CAR"):"ka_01",0,others,NULL,5,msg,sizeof msg));   /* PROBE_CAR: another car */
    int ready=0;for(int i=0;i<2000 && !ready;i++){SDL_PumpEvents();SDL_Delay(5);ready=race_game_poll(g);}CHECK(ready==1);
    if(getenv("PROBE_CINEMA_TEST")){
        race_game_update(g,0);
        uint8_t *before=malloc(0x2000000);CHECK(before);memcpy(before,gt4_phys_mem,0x2000000);
        float eye[3],rows[9],pr[3],seed[3];CHECK(race_view_get_camera(g->view,seed,rows,pr));
        race_game_cinematic_toggle(g);CHECK(race_game_cinematic(g));
        cinematic_camera(g,0);CHECK(race_view_get_camera(g->view,eye,rows,pr));
        for(int k=0;k<3;k++)CHECK(fabsf(eye[k]-seed[k])<1e-5f);
        fixture_key[SDL_SCANCODE_UP]=fixture_key[SDL_SCANCODE_A]=1;read_controls(g);
        CHECK(!command.throttle && !command.brake && !command.steer && !command.shift);
        memset(fixture_key,0,sizeof fixture_key);fixture_key[SDL_SCANCODE_W]=1;
        for(int i=0;i<60;i++)cinematic_camera(g,1./60);memset(fixture_key,0,sizeof fixture_key);
        CHECK(race_view_get_camera(g->view,eye,rows,pr));float distance=0;
        for(int k=0;k<3;k++)distance+=(eye[k]-seed[k])*(eye[k]-seed[k]);CHECK(distance>16 && distance<25);
        float old=pr[0];race_game_cinematic_mouse(g,80,-30,3);cinematic_camera(g,1./60);
        CHECK(g->cinema.projection[0]>old);CHECK(!g->cinema.aim);
        race_game_cinematic_key(g,SDLK_f);cinematic_camera(g,0);CHECK(g->cinema.mode==1 && g->cinema.aim);
        float pos[3],rot[9],wheels[4][3];CHECK(gt4_race_run_pose(g->run,0,pos,rot,wheels));
        float len=0,f[3];for(int k=0;k<3;k++){f[k]=pos[k]+rot[k*3+1]*.65f-g->cinema.eye[k];len+=f[k]*f[k];}
        len=sqrtf(len);for(int k=0;k<3;k++)CHECK(fabsf(-g->cinema.rows[6+k]-f[k]/len)<1e-5f);
        memcpy(seed,g->cinema.eye,sizeof seed);g->cinema.anchor[0]-=3;cinematic_camera(g,0);
        CHECK(fabsf(g->cinema.eye[0]-seed[0]-3)<1e-5f);
        race_game_cinematic_mouse(g,150,40,0);cinematic_camera(g,1./60);CHECK(g->cinema.aim);
        CHECK(fabsf(g->cinema.eye[2]-seed[2])>.001f);
        race_game_cinematic_key(g,SDLK_TAB);cinematic_camera(g,0);CHECK(g->cinema.target==1);
        memcpy(seed,g->cinema.eye,sizeof seed);race_game_cinematic_key(g,SDLK_g);cinematic_camera(g,0);
        CHECK(g->cinema.mode==2 && !g->cinema.aim);
        for(int k=0;k<3;k++)CHECK(fabsf(g->cinema.eye[k]-seed[k])<1e-4f);
        CHECK(gt4_race_run_pose(g->run,1,pos,rot,wheels));
        for(int k=0;k<3;k++){float expected=pos[k];for(int j=0;j<3;j++)expected+=rot[k*3+j]*g->cinema.mount[j];CHECK(fabsf(expected-g->cinema.eye[k])<1e-5f);}
        for(int a=0;a<3;a++){float norm=0;for(int k=0;k<3;k++)norm+=g->cinema.rows[a*3+k]*g->cinema.rows[a*3+k];CHECK(fabsf(norm-1)<1e-5f);}
        int original_hold=g->held;race_game_cinematic_key(g,SDLK_p);CHECK(g->held);
        fixture_key[SDL_SCANCODE_E]=1;cinematic_camera(g,1./60);memset(fixture_key,0,sizeof fixture_key);
        float focus=g->cinema.focus;race_game_cinematic_key(g,SDLK_LEFTBRACKET);cinematic_camera(g,1./60);
        CHECK(!g->cinema.focus_auto && g->cinema.focus<=focus);race_game_cinematic_key(g,SDLK_r);CHECK(g->cinema.focus_auto);
        /* Visible focus fixture, framing the selected car from outside. */
        float target[3],local[3]={3,1.5f,-5};CHECK(cinematic_pose(g,pos,rot));
        for(int k=0;k<3;k++){eye[k]=pos[k];for(int j=0;j<3;j++)eye[k]+=rot[k*3+j]*local[j];target[k]=pos[k]+rot[k*3+1]*.65f;}
        look(g,eye,target,0,3,3*1.30122f);
        CHECK(race_view_get_camera(g->view,g->cinema.eye,g->cinema.rows,g->cinema.projection));
        cinematic_mode(g,1);cinematic_camera(g,0);
        CHECK(!memcmp(before,gt4_phys_mem,0x2000000));
        if(getenv("PROBE_CINEMA_NEGATIVE"))GT4_U8(gt4_race_run_car(g->run,0)+0x554)^=1;
        CHECK(!memcmp(before,gt4_phys_mem,0x2000000));
        CHECK(shot(g,argv[2],"cinema-sharp"));int w,h,w2,h2;uint8_t *sharp=plat_sdl_read_pixels(&w,&h);CHECK(sharp);
        race_game_cinematic_key(g,SDLK_b);CHECK(g->cinema.aperture==1);
        CHECK(shot(g,argv[2],"cinema-focus"));uint8_t *blur=plat_sdl_read_pixels(&w2,&h2);CHECK(blur && w==w2 && h==h2);
        unsigned changed=0;for(int i=0;i<w*h;i++)if(memcmp(sharp+i*4,blur+i*4,3))changed++;
        free(sharp);free(blur);CHECK(changed>100);
        float samples[8][4];gt4_photo_samples(samples,8);
        for(int i=0;i<8;i++){
            float shift[2],jitter[2];cinematic_lens(g->cinema.projection,2.8f,g->cinema.focus,samples[i],shift,jitter);
            float sx=g->cinema.projection[0]*(448.f*4.f/3.f)/640*video_projection_x();
            CHECK(fabsf(-shift[0]/g->cinema.focus*sx+jitter[0])<1e-6f);
            CHECK(fabsf(-shift[1]/g->cinema.focus*g->cinema.projection[1]+jitter[1])<1e-6f);
        }
        CHECK(race_view_get_camera(g->view,eye,rows,pr));
        for(int k=0;k<3;k++)CHECK(fabsf(eye[k]-g->cinema.eye[k])<1e-5f);
        CHECK(!memcmp(before,gt4_phys_mem,0x2000000));
        race_game_cinematic_key(g,SDLK_HOME);cinematic_camera(g,0);CHECK(g->cinema.seeded && !g->cinema.mode);
        CHECK(!memcmp(before,gt4_phys_mem,0x2000000));
        race_game_cinematic_key(g,SDLK_ESCAPE);CHECK(!race_game_cinematic(g) && g->held==original_hold && !SDL_GetRelativeMouseMode());
        CHECK(race_view_get_camera(g->view,eye,rows,pr));
        CHECK(!memcmp(eye,g->cinema.original_eye,sizeof eye) && !memcmp(rows,g->cinema.original_rows,sizeof rows));
        race_game_cinematic_toggle(g);race_game_stop(g);CHECK(!race_game_cinematic(g) && !SDL_GetRelativeMouseMode());
        free(before);race_game_close(g);plat_video_shutdown();puts("cinema: seed, controls, lens/focus, follow/aim, target, mount, hold, cleanup and unchanged 32 MiB physics passed");return 0;
    }
    if(getenv("PROBE_FILM_TEST")){
        /* Filming must remain a read-only consumer of the complete physics
           address space, including all AI cars. This is a PC rig, not an
           original-camera fidelity comparison. */
        uint8_t *before=malloc(0x2000000);CHECK(before);memcpy(before,gt4_phys_mem,0x2000000);
        const char *modes[]={"low","orbit","crane"};
        for(int mode=0;mode<3;mode++)for(int step=0;step<3;step++){
#ifdef _WIN32
            _putenv_s("OPENGT4_RACE_FILM",modes[mode]);
#else
            setenv("OPENGT4_RACE_FILM",modes[mode],1);
#endif
            g->film_clock=10+step*12;
            film_camera(g,1./60);
            float eye[3],rows[9],projection[3];CHECK(race_view_get_camera(g->view,eye,rows,projection));
            for(int a=0;a<3;a++){
                float length=0;for(int k=0;k<3;k++){CHECK(isfinite(eye[k]));length+=rows[a*3+k]*rows[a*3+k];}
                CHECK(fabsf(length-1)<1e-5f);
            }
            CHECK(projection[0]>0 && projection[1]>0 && projection[2]>0);
            if(getenv("PROBE_FILM_NEGATIVE"))GT4_U8(gt4_race_run_car(g->run,0)+0x554)^=1;
            CHECK(!memcmp(before,gt4_phys_mem,0x2000000));
        }
        free(before);race_game_close(g);plat_video_shutdown();puts("film camera: orthonormal, finite, physics unchanged (32 MiB)");return 0;
    }
    if(getenv("PROBE_ALL_ENTRIES"))for(int entry=0;entry<6;entry++){
        const car_model *car=race_view_car_model(g->view,entry);CHECK(car);
        for(int wheel=0;wheel<4;wheel++){
            const gt4_model *model=car->wheel[wheel];CHECK(model && model->count);
            for(int triangle=0;triangle<model->count;triangle++)CHECK(model->triangles[triangle].source_model==0);
            r3d_mesh *mesh=r3d_mesh_create(model);CHECK(mesh);r3d_mesh_destroy(mesh);
        }
    }
    race_game_update(g,0);
    float eye[3],rows[9],projection[3],start[3];CHECK(race_view_get_camera(g->view,eye,rows,projection));memcpy(start,eye,sizeof eye);
    race_game_inspect_toggle(g);inspect_camera(g,0);CHECK(g->inspect_seeded);
    fixture_key[SDL_SCANCODE_UP]=fixture_key[SDL_SCANCODE_A]=1;read_controls(g);
    CHECK(!command.throttle && !command.brake && !command.steer && !command.shift);
    memset(fixture_key,0,sizeof fixture_key);
    CHECK(race_view_get_camera(g->view,eye,rows,projection));
    for(int k=0;k<3;k++)CHECK(fabsf(eye[k]-start[k])<1e-5f);
    fixture_key[SDL_SCANCODE_W]=1;
    for(int i=0;i<60;i++)inspect_camera(g,1./60);
    fixture_key[SDL_SCANCODE_W]=0;CHECK(race_view_get_camera(g->view,eye,rows,projection));
    for(int k=0;k<3;k++)CHECK(fabsf(eye[k]-start[k]+rows[6+k]*3)<1e-3f);
    float expected=1;
    if(argc>3 && !strcmp(argv[3],"--negative"))expected=2;
    for(int a=0;a<3;a++){float length=0;for(int k=0;k<3;k++)length+=rows[a*3+k]*rows[a*3+k];CHECK(fabsf(length-expected)<1e-5f);}
    fixture_key[SDL_SCANCODE_Q]=fixture_key[SDL_SCANCODE_LSHIFT]=1;
    for(int i=0;i<60;i++)inspect_camera(g,1./60);
    memset(fixture_key,0,sizeof fixture_key);CHECK(race_view_get_camera(g->view,eye,rows,projection));CHECK(fabsf(eye[1]-(start[1]-rows[7]*3-15))<1e-3f);
    race_game_inspect_toggle(g);race_game_update(g,0);CHECK(!g->inspect_on);
    /* Diagnostic only: inspect live wheel transforms after native ticks. */
    if(getenv("PROBE_STEPS"))for(int i=0;i<atoi(getenv("PROBE_STEPS"));i++){
        race_game_update(g,1./60);
        if(getenv("PROBE_DRAW_STEPS")){plat_video_begin_frame();CHECK(race_game_draw(g));plat_video_end_frame();}
    }
    if(getenv("PROBE_STEPS"))CHECK(shot(g,argv[2],"live"));
    /* Diagnostic views use native choices to inspect every visible wheel and
       the bonnet/lamps. The car pose itself is the real simulation's pose. */
    float pos[3],rot[9],wheels[4][3];CHECK(gt4_race_run_pose(g->run,getenv("PROBE_ENTRY")?atoi(getenv("PROBE_ENTRY")):0,pos,rot,wheels));
    if(getenv("PROBE_ISOLATE")){
        int entry=getenv("PROBE_ENTRY")?atoi(getenv("PROBE_ENTRY")):0;
        for(int i=0;i<5;i++)if(i!=entry-1)race_view_set_opponent(g->view,i,NULL,NULL,NULL);
    }
    static const float camera[4][3]={{3,2,-5},{-3,2,5},{4,1.2f,0},{-4,1.2f,0}};
    static const char *names[]={"front","rear","side","side-left"};
    float camera_scale=getenv("PROBE_CAMERA_SCALE")?(float)atof(getenv("PROBE_CAMERA_SCALE")):1;
    int all_entries=getenv("PROBE_ALL_ENTRIES")!=NULL;
    for(int entry=0;entry<(all_entries?6:1);entry++)for(int a=0;a<4;a++) {
        if(all_entries)CHECK(gt4_race_run_pose(g->run,entry,pos,rot,wheels));
        float target[3];for(int k=0;k<3;k++){eye[k]=target[k]=pos[k];for(int j=0;j<3;j++)eye[k]+=rot[k*3+j]*camera[a][j]*camera_scale;target[k]+=rot[k*3+1]*.6f;}
        float f[3],r[3],u[3],length=0;for(int k=0;k<3;k++){f[k]=target[k]-eye[k];length+=f[k]*f[k];}length=sqrtf(length);
        for(int k=0;k<3;k++)f[k]/=length;length=sqrtf(f[0]*f[0]+f[2]*f[2]);r[0]=-f[2]/length;r[1]=0;r[2]=f[0]/length;
        u[0]=-f[1]*r[2];u[1]=f[0]*r[2]-f[2]*r[0];u[2]=f[1]*r[0];
        for(int k=0;k<3;k++){rows[k]=r[k];rows[3+k]=u[k];rows[6+k]=-f[k];}
        char label[64];if(all_entries)snprintf(label,sizeof label,"car-%d-%s",entry,names[a]);
        if(!all_entries)snprintf(label,sizeof label,"%s",names[a]);
        race_view_set_camera(g->view,eye,rows,1.73205f,2.25378f,.1f);race_view_show_car(g->view,!getenv("PROBE_ISOLATE") || !getenv("PROBE_ENTRY") || !atoi(getenv("PROBE_ENTRY")));CHECK(shot(g,argv[2],label));
    }
    if(getenv("PROBE_DIST")) {   /* tests: the course seen from 3 m over its line at that distance, 20 m ahead */
        float d=(float)atof(getenv("PROBE_DIST")),p0[3],p1[3];
        if(gt4_race_run_course_point(g->run,d,p0) && gt4_race_run_course_point(g->run,d+20,p1)) {
            float f[3],r[3],u[3],length=0;eye[0]=p0[0];eye[1]=p0[1]+3;eye[2]=p0[2];
            for(int k=0;k<3;k++){f[k]=p1[k]-eye[k];length+=f[k]*f[k];}length=sqrtf(length);
            for(int k=0;k<3;k++)f[k]/=length;length=sqrtf(f[0]*f[0]+f[2]*f[2]);r[0]=-f[2]/length;r[1]=0;r[2]=f[0]/length;
            u[0]=-f[1]*r[2];u[1]=f[0]*r[2]-f[2]*r[0];u[2]=f[1]*r[0];
            for(int k=0;k<3;k++){rows[k]=r[k];rows[3+k]=u[k];rows[6+k]=-f[k];}
            race_view_set_camera(g->view,eye,rows,1.73205f,2.25378f,.1f);CHECK(shot(g,argv[2],"course"));
        }
    }
    if(argc>3 && (!strcmp(argv[3],"--wheel-test") || !strcmp(argv[3],"--wheel-negative"))) {
        /* Same camera/body/pose, synthetic angular exposure only. The player
           rim uses GPU accumulation; no physics state is written. */
        race_view_set_wheel_motion(g->view,0,NULL,NULL,0);CHECK(shot(g,argv[2],"sharp"));
        int w,h,w2,h2;uint8_t *sharp=plat_sdl_read_pixels(&w,&h);CHECK(sharp);
        r3d_stats before=r3d_frame_stats();
        if(strcmp(argv[3],"--wheel-negative")) {
            float current[4]={1.5f,1.5f,1.5f,1.5f},previous[4]={0};race_view_set_wheel_motion(g->view,0,current,previous,0);
        }
        CHECK(shot(g,argv[2],"blurred"));uint8_t *blur=plat_sdl_read_pixels(&w2,&h2);CHECK(blur && w==w2 && h==h2);
        r3d_stats after=r3d_frame_stats();CHECK(before.draws==after.draws && before.triangles==after.triangles);
        unsigned changed=0;for(int p=0;p<w*h;p++)if(memcmp(sharp+p*4,blur+p*4,4))changed++;
        free(sharp);CHECK(changed>0);CHECK(after.wheel_passes>0);
        CHECK(shot(g,argv[2],"cached"));uint8_t *cached=plat_sdl_read_pixels(&w,&h);CHECK(cached);
        CHECK(!memcmp(blur,cached,(size_t)w*h*4));free(blur);free(cached);
        r3d_stats reused=r3d_frame_stats();CHECK(!reused.wheel_passes && reused.wheel_cache_hits>0);
        CHECK(glGetError()==GL_NO_ERROR);
        printf("wheel accumulation: %u changed pixels, %u GPU passes, cached redraw 0 passes, unchanged scene draws/triangles.\n",changed,after.wheel_passes);
    }
    race_game_stop(g);CHECK(!g->inspect_on && !g->inspect_seeded);race_game_close(g);plat_video_shutdown();
    puts("race inspection: toggle, pose seed, movement, axes, no clipping and reset passed");return 0;
}
