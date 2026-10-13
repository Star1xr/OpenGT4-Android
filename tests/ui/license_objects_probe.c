/* Native presentation integration, independent from solver fidelity. */
#include <SDL.h>
#include "plat_sdl.h"
#include "platform/platform.h"
#include "miniz.h"
#include "career_license_disc.h"
#include "../../src/app/race_game.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"license objects line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint8_t *frame(race_game *g,int *w,int *h)
{plat_video_begin_frame();race_game_draw(g);uint8_t *p=plat_sdl_read_pixels(w,h);plat_video_end_frame();return p;}
static int screenshot(race_game *g,const char *path)
{int w,h;uint8_t *p=frame(g,&w,&h);size_t n;void *png=p?tdefl_write_image_to_png_file_in_memory(p,w,h,4,&n):NULL;
 FILE *f=png?fopen(path,"wb"):NULL;int ok=f && fwrite(png,1,n,f)==n;
 if(f)fclose(f);mz_free(png);free(p);return ok;}
static uint32_t word(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
static float scalar(const uint8_t *p){float v;memcpy(&v,p,4);return v;}
static void object_hide(int on)
{
#ifdef _WIN32
 _putenv_s("OPENGT4_TEST_HIDE_GADGETS",on?"1":"");
#else
 if(on)setenv("OPENGT4_TEST_HIDE_GADGETS","1",1);else unsetenv("OPENGT4_TEST_HIDE_GADGETS");
#endif
}
int main(int argc,char **argv)
{
    CHECK(argc==3 || argc==5);CHECK(!plat_video_init(&(plat_video_desc){960,672,0,0,0}));
    SDL_HideWindow(SDL_GetWindowFromID(1));plat_video_set_view(640,448);
    race_game *g=race_game_open(argv[1]);CHECK(g);gt4_career_race_lookup_data lookup;
    CHECK(gt4_career_race_lookup_data_load(argv[1],&lookup));
    const char *tests[]={"l0b0001","l0b0003"};char error[256],path[1024];unsigned visible=0;
    for(unsigned test=0;test<2;test++){
        gt4_career_license_exam exam;CHECK(gt4_career_license_prepare(&exam,argv[1],&lookup,tests[test],error,sizeof error));
        CHECK(race_game_start_prepared(g,&exam.desc,error,sizeof error));int ready=0;
        for(int i=0;i<2000 && !ready;i++){SDL_PumpEvents();SDL_Delay(5);ready=race_game_poll(g);}CHECK(ready==1);
        CHECK(g->view && g->run);printf("%s course=%s\n",tests[test],exam.desc.course);
        for(unsigned tick=0;tick<120;tick++){
            race_game_update(g,1./60);if(tick%30!=29)continue;
            if(getenv("GT4_LICENSE_START_CAR_NEGATIVE"))race_view_show_car(g->view,0);
            int w,h,w2,h2;uint8_t *shown=frame(g,&w,&h);CHECK(shown);
            race_view_show_car(g->view,0);uint8_t *hidden=frame(g,&w2,&h2);CHECK(hidden && w==w2 && h==h2);
            unsigned changed=0;for(size_t i=0;i<(size_t)w*h*4;i++)changed+=shown[i]!=hidden[i];
            printf("opening %s tick=%u on=%d left=%d pixels=%u\n",tests[test],tick+1,g->start_cam_on,g->license_start_left,changed);CHECK(g->start_cam_on && changed>0);visible++;free(shown);free(hidden);race_view_show_car(g->view,1);
        }
        snprintf(path,sizeof path,"%s-%s-start.png",argv[2],tests[test]);CHECK(screenshot(g,path));
        /* 00374720 selects kind 4 only while BC > 0. Its internal
           clock (0037D338 +F8..110) still has the additional 60 frames. */
        gt4_race_hud hud;CHECK(gt4_race_run_hud(g->run,0,&hud));
        CHECK(g->license_start_left==hud.start_frames+60);
        for(unsigned tick=120;tick<480;tick++){
            race_game_update(g,1./60);CHECK(gt4_race_run_hud(g->run,0,&hud));
            if(getenv("GT4_LICENSE_START_CLOCK_NEGATIVE"))g->start_cam_on=1;
            CHECK(g->start_cam_on==(hud.start_frames>0));CHECK(g->license_start_left==hud.start_frames+60);
        }CHECK(!g->start_cam_on && hud.start_frames==0);

    }
    if(argc==5)for(unsigned state=0;state<2;state++){
        uint8_t *ram=malloc(0x2000000);CHECK(ram);FILE *f=fopen(argv[3+state],"rb");CHECK(f);
        CHECK(fread(ram,1,0x2000000,f)==0x2000000);fclose(f);
        uint32_t rc=word(ram+0x6d6060),manager=word(ram+rc+12),section=word(ram+rc+0xb0);
        gt4_course_gadgets *objects=race_view_gadgets(g->view,(int)section);CHECK(objects && section<objects->sections);
        unsigned seen[256]={0},matched=0;float maximum=0,rotation_error=0;
        for(uint32_t p=word(ram+manager+8);p;p=word(ram+p)){
            CHECK(p<0x2000000-96);unsigned type=ram[p+0x44],found=0;
            for(unsigned j=0;j<objects->length[section+1];j++){
                gt4_course_gadget *o=objects->instance+objects->first[section+1]+j;
                if(seen[j] || o->type!=type)continue;float pos[3];memcpy(pos,ram+p+16,12);
                if(fabsf(pos[0]-o->pos[0])>.002f || fabsf(pos[2]-o->pos[2])>.002f)continue;
                if(getenv("GT4_GADGET_RAM_NEGATIVE"))pos[1]+=.02f;
                float dy=fabsf(pos[1]-o->pos[1]);if(o->fixed)CHECK(dy==0);else CHECK(dy<.00001f);
                float a[3],r[9];memcpy(a,ram+p+4,12);gt4_course_gadget_rotation(a,r);
                for(unsigned k=0;k<9;k++){rotation_error=fmaxf(rotation_error,fabsf(r[k]-o->rot[k]));CHECK(fabsf(r[k]-o->rot[k])<.00001f);}
                CHECK(fabsf(o->sun-ram[p+0x46]/255.f)<.000001f && fabsf(o->shade-ram[p+0x47]/255.f)<.000001f);
                maximum=fmaxf(maximum,dy);seen[j]=1;found=1;matched++;break;
            }CHECK(found);
        }CHECK(matched==objects->length[section+1]);
        /* Inspection fixture: captured eye/lens, looking at the authored
           Start/Finish banner. This is a draw-coverage view, not an oracle
           of the PS2 camera. The saved Screenshot is earlier than its RAM. */
        uint32_t scene=0;for(uint32_t p=0x100000;p<0x2000000-0x25000;p+=4)
            if(word(ram+p+0x64)==0x6807a8 && word(ram+p+0x80)==0x67a078){scene=p;break;}
        CHECK(scene);uint32_t camera=word(ram+scene+0x80+0x13c);float eye[3],rows[9];
        memcpy(eye,ram+camera+0x10,12);gt4_course_gadget *banner=NULL;
        for(unsigned j=0;j<objects->length[section+1];j++){
            gt4_course_gadget *o=objects->instance+objects->first[section+1]+j;
            if(o->type==(state?1:5)){banner=o;break;}
        }CHECK(banner);float back[3];float len=0;
        for(unsigned i=0;i<3;i++){back[i]=eye[i]-banner->pos[i];len+=back[i]*back[i];}
        CHECK(len>0);len=sqrtf(len);for(unsigned i=0;i<3;i++)back[i]/=len;
        len=sqrtf(back[0]*back[0]+back[2]*back[2]);CHECK(len>0);
        rows[0]=back[2]/len;rows[1]=0;rows[2]=-back[0]/len;
        rows[3]=back[1]*rows[2];rows[4]=back[2]*rows[0]-back[0]*rows[2];rows[5]=-back[1]*rows[0];memcpy(rows+6,back,12);
        race_view_set_camera(g->view,eye,rows,scalar(ram+camera+0x1c),scalar(ram+camera+0x24),.1f);
        race_view_show_car(g->view,0);int w,h,w2,h2;
        if(getenv("GT4_GADGET_DRAW_NEGATIVE"))object_hide(1);
        uint8_t *shown=frame(g,&w,&h);CHECK(shown);
        object_hide(1);uint8_t *hidden=frame(g,&w2,&h2);object_hide(0);CHECK(hidden && w==w2 && h==h2);
        unsigned changed=0;for(size_t j=0;j<(size_t)w*h*4;j++)changed+=shown[j]!=hidden[j];CHECK(changed>0);free(shown);free(hidden);
        snprintf(path,sizeof path,"%s-slot%u-objects.png",argv[2],state+5);CHECK(screenshot(g,path));
        printf("slot %u: %u object poses, max Y %.9g m, max matrix %.9g, exact light bytes; %u object color bytes\n",state+5,matched,maximum,rotation_error,changed);free(ram);
    }
    printf("PASS %u opening samples with visible car pixels.\n",visible);race_game_close(g);plat_video_shutdown();return 0;
}
