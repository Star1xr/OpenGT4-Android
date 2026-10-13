/* Native 0-400 m: authored quick-mt Start, physical finish, ResultArcade
   mode 5 (003DFFF8 entry+1B0), recorded replay, then refreshed quick-mt.
   Mode 4 formatting follows 003E0490. No finish or time is injected.
   --negative substitutes car+718, reproducing the zero-time regression. */
#include <SDL.h>
#include "plat_sdl.h"
#include "platform/platform.h"
#include "miniz.h"
#include "../../src/core/race_result.c"
#include "../../src/app/race_game.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"machine flow line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void ticks(menu *m){for(int i=0;i<60;i++){menu_tick(m);menu_animate(m,1.f/60);}}
static int shot(const char *dir,const char *name)
{
    char path[1200];snprintf(path,sizeof path,"%s/%s.png",dir,name);
    int w,h;size_t size;uint8_t *pixels=plat_sdl_read_pixels(&w,&h);
    void *png=pixels?tdefl_write_image_to_png_file_in_memory(pixels,w,h,4,&size):NULL;
    FILE *f=png?fopen(path,"wb"):NULL;if(f){fwrite(png,1,size,f);fclose(f);}
    free(pixels);mz_free(png);return f!=NULL;
}
int main(int argc,char **argv)
{
    CHECK(argc>=3);int negative=argc>3 && !strcmp(argv[3],"--negative");
#ifdef _WIN32
    _putenv_s("OPENGT4_RACE_AUTOPILOT","1");
    _putenv_s("OPENGT4_RACE_SPEED","1");
#else
    setenv("OPENGT4_RACE_AUTOPILOT","1",1);
    setenv("OPENGT4_RACE_SPEED","1",1);
#endif
    plat_video_desc video={640,480,0,0,0};CHECK(!plat_video_init(&video));SDL_HideWindow(SDL_GetWindowFromID(1));
    const char *why;gt4_vol *vol=gt4_vol_open(argv[1],&why);CHECK(vol);
    menu_quick_race q={0};strcpy(q.course,"testcourse");strcpy(q.race,"mt400_testcourse");
    q.entries=1;q.laps=1;q.dry=1;strcpy(q.grid[0].car,"ka_01");
    menu_set_quick_race(&q);menu_set_quick_best(0x157529ffu);
    char msg[256];menu *m=menu_open(vol,"quick-mt",msg,sizeof msg);CHECK(m);
    CHECK(menu_enter_quick_start(m));ticks(m);
    CHECK(menu_test_activate(m,"Start"));ticks(m);char command_name[64];
    CHECK(menu_take_quick_command(m,command_name,sizeof command_name) && !strcmp(command_name,"COMMAND_START"));
    race_game *g=race_game_open(argv[1]);CHECK(g);race_game_set_world_mode(g,15);race_game_set_laps(g,1);
    CHECK(race_game_start(g,"testcourse","ka_01",0,NULL,NULL,0,msg,sizeof msg));
    int ready=0;for(int i=0;i<4000 && !ready;i++){SDL_PumpEvents();SDL_Delay(5);ready=race_game_poll(g);}CHECK(ready==1);
    unsigned frame;for(frame=0;frame<60*60*2 && !gt4_race_run_finished(g->run);frame++)race_game_update(g,STEP);
    CHECK(gt4_race_run_finished(g->run));uint32_t elapsed;float speed;int test;
    CHECK(race_game_machine_result(g,&test,&elapsed,&speed));CHECK(test==1 && elapsed>0 && elapsed!=0x157529ffu);
    uint32_t race=GT4_U32(gt4_race_run_conductor(g->run)),entry=GT4_U32(GT4_U32(GT4_U32(race+0x60)+8));
    CHECK(GT4_U32(entry+0x1b0)==elapsed);
    plat_video_set_view(640,448);plat_video_begin_frame();CHECK(race_game_draw(g));draw_result(g,1,STEP);
    CHECK(g->result && g->result->nrows==1);
    char expected[24];snprintf(expected,sizeof expected," %u'%02u.%03u",elapsed/60000,elapsed/1000%60,elapsed%1000);
    if(negative)snprintf(g->result->rows[0].time,sizeof g->result->rows[0].time," 0'00.000");
    CHECK(!strcmp(g->result->rows[0].time,expected));
    /* Let the original STRB reveal its board before the arrival capture. */
    plat_video_end_frame();
    for(int i=0;i<300;i++){
        plat_video_begin_frame();race_game_draw(g);draw_result(g,1,STEP);
        if(i==299)CHECK(shot(argv[2],"finish"));plat_video_end_frame();
    }
    for(int i=0;i<1000 && !race_result_done(g->result);i++){race_result_press(g->result);draw_result(g,1,STEP);}
    CHECK(race_result_done(g->result));CHECK(race_game_can_replay(g));
    CHECK(race_game_replay(g,msg,sizeof msg));
    for(int i=0;i<4000 && race_game_poll(g)==0;i++){SDL_PumpEvents();SDL_Delay(5);}CHECK(g->ready);
    for(unsigned i=0;i<frame+600 && !race_game_over(g);i++)race_game_update(g,STEP);
    CHECK(race_game_over(g) && g->tape_at==g->tape_n);
    race_game_return_pregrid(g);menu_set_quick_best(elapsed);menu_set_quick_completed(m,1);
    CHECK(menu_enter_quick_start(m));ticks(m);
    ui_widget *root=ui_root_widget(menu_ui(m),menu_active_root(m));
    ui_widget *buttons=ui_widget_child(root,"Buttons");CHECK(ui_widget_child(buttons,"Replay")->visible && ui_widget_child(buttons,"SaveReplay")->visible);
    CHECK(!strcmp(ui_widget_text(menu_ui(m),ui_widget_child(root,"Sessionbest_Time")),expected+1));
    race_game_pregrid_update(g,STEP);
    plat_video_set_view(640,480);plat_video_begin_frame();CHECK(race_game_draw_pregrid(g));ui_draw_root(menu_ui(m),menu_active_root(m),NULL,NULL);
    CHECK(shot(argv[2],"returned-menu"));plat_video_end_frame();
    /* Mode 4's two supported units; deliberately separate from 400 m time. */
    for(int mph=0;mph<2;mph++){
        race_result *r=race_result_open(vol,NULL,NULL);CHECK(r);
        race_result_state s={0};s.cars=1;s.rank[0]=1;s.finished[0]=1;s.mode=4;s.top_speed[0]=50;s.mph=mph;
        race_result_update(r,&s,0);printf("speed unit %d: %s\n",mph,r->rows[0].time);CHECK(!strcmp(r->rows[0].time,mph?"111.85mph":"180.00km/h"));race_result_close(r);
    }
    printf("PASS physical 400 m in %u frames: %s; result, complete replay and quick-mt return; speed units.\n",frame,expected);
    race_game_close(g);menu_close(m);gt4_vol_close(vol);plat_video_shutdown();return 0;
}
