/* Arcade's authored SplitRoot::RaceStart -> GameRoot::doGame -> finish.
   The negative removes the second selected car, so Start must not fire. */
#define main pregrid_regression_main
#include "pregrid_menu.c"
#undef main
#include "race_result.h"
#include "race_pause.h"
void plat_video_set_view(float w,float h){(void)w;(void)h;}
static ui_widget *root_named(menu *m,const char *name)
{
    for(int i=0;i<ui_root_count(m->ui);i++)if(!strcmp(ui_root_name(m->ui,i),name))return ui_root_widget(m->ui,i);
    return NULL;
}
int main(int argc,char **argv)
{
    CHECK(argc==2 || argc==3);const char *error=NULL;char msg[256];
    gt4_vol *vol=gt4_vol_open(argv[1],&error);CHECK(vol);
    menu *m=menu_open(vol,"arcade",msg,sizeof msg);CHECK(m);ah_set_log(m->vm,log_script,NULL);
    ah_obj *game_root=widget_module(m,root_named(m,"GameRoot"));CHECK(game_root);
    ah_obj *course_root=widget_module(m,root_named(m,"CourseRoot"));CHECK(course_root);
    ah_obj *split_root=widget_module(m,root_named(m,"SplitRoot"));CHECK(split_root);
    ah_set_member(m->vm,game_root,"current_game",ah_str(m->vm,"SplitBattle"));
    ah_set_member(m->vm,course_root,"current_crs",ah_str(m->vm,"tsukuba"));
    ah_obj *game=game_object(m);CHECK(game);
    ah_set_member(m->vm,game,"car_code",ah_str(m->vm,"ka_01"));
    ah_set_member(m->vm,game,"car_code_2p",ah_str(m->vm,argc==3 && !strcmp(argv[2],"--negative")?"":"today_g_85"));
    ah_set_member(m->vm,game,"car_garage",ah_nil());ah_set_member(m->vm,game,"car_garage_2p",ah_nil());
    ah_set_member(m->vm,game,"car_color",ah_int(2));ah_set_member(m->vm,game,"car_color_2p",ah_int(3));
    ah_obj *start=widget_module(m,child(root_named(m,"SplitRoot"),"RaceStart"));CHECK(start);
    ah_val context=ah_objv(m->context),ret;CHECK(ah_call_member(m->vm,start,"onActivate",1,&context,&ret)>=0);
    CHECK(!errors && menu_take_race_request(m) && menu_race_is_split(m) && !menu_race_is_time_trial(m));
    CHECK(!strcmp(menu_split_car(m,0),"ka_01") && !strcmp(menu_split_car(m,1),"today_g_85"));
    CHECK(!strcmp(menu_selected_course(m),"tsukuba"));
    CHECK(menu_game_number(m,"car_color",0)==2 && menu_game_number(m,"car_color_2p",0)==3);
    /* The same authored results movie bound by 0032F028 +0x1C0..1D4,
       fed the two actual finish rows. A: full world activation not checked. */
    race_result *result=race_result_open(vol,NULL,NULL);CHECK(result);
    race_result_state state={0};state.cars=2;state.rank[0]=2;state.rank[1]=1;
    state.name[0]="Ford Ka";state.name[1]="Honda TODAY";
    state.finished[0]=state.finished[1]=1;state.laps[0]=state.laps[1]=1;
    state.time[0]=65000;state.time[1]=60000;
    for(int i=0;i<600 && !race_result_done(result);i++){
        race_result_update(result,&state,1./60);race_result_draw(result);
        if(i>120 && !(i%30))race_result_press(result);
    }
    CHECK(race_result_done(result));race_result_close(result);
    for(int leave=0;leave<2;leave++){
        race_pause *pause=race_pause_open(vol,1,0,0,NULL,NULL);CHECK(pause);
        for(int i=0;i<60;i++)race_pause_update(pause,1./60);
        race_pause_key(pause,40);
        if(leave || (argc==3 && !strcmp(argv[2],"--pause-negative")))race_pause_key(pause,40);
        race_pause_key(pause,13);
        for(int i=0;i<60;i++)race_pause_update(pause,1./60);
        CHECK(race_pause_state(pause)==(leave?RACE_PAUSE_EXIT:RACE_PAUSE_RETRY));
        race_pause_close(pause);
    }
    menu_close(m);gt4_vol_close(vol);puts("PASS authored SplitRoot Start: both cars/paints, selected course, split sequence, two-car result exit");return 0;
}
