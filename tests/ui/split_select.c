/* Execute arcade.ad's real P1/P2 Select -> MakerRoot -> CarRoot ->
   colour -> SettingPopup.OK -> SplitRoot. No race code is injected. */
#define plat_draw_rotated fixture_rotated
#define main pregrid_regression_main
#include "pregrid_menu.c"
#undef main
#undef plat_draw_rotated
static float cursor_x;
void plat_draw_rotated(const plat_texture*t,float x,float y,float w,float h,float cx,float cy,float a,const uint8_t c[4]){cursor_x=cx;fixture_rotated(t,x,y,w,h,cx,cy,a,c);}
#include "career_vehicle.h"
static ui_widget *root_named(menu *m,const char *name)
{
    for(int i=0;i<ui_root_count(m->ui);i++)if(!strcmp(ui_root_name(m->ui,i),name))return ui_root_widget(m->ui,i);
    CHECK(0);return NULL;
}
static void page(menu *m,const char *name)
{
    advance(m);
    const char *actual=ui_root_name(m->ui,menu_active_root(m));
    if(strcmp(actual,name))fprintf(stderr,"page %s, expected %s, focus %s\n",actual,name,menu_focus_name(m));
    CHECK(!strcmp(actual,name) && !errors);
}
static void selection_tick(menu *m)
{
    for(int i=0;i<60;i++){menu_tick(m);menu_animate(m,1.f/60);ui_draw_root(m->ui,menu_active_root(m),NULL,NULL);}
}
static void call_context(menu *m,ui_widget *w,const char *method)
{
    ah_val context=ah_objv(m->context),ret;
    CHECK(ah_call_member(m->vm,widget_module(m,w),method,1,&context,&ret)>=0);
}
static void choose(menu *m,int port,const char *code,int paint,int manual,int assist)
{
    ui_widget *split=root_named(m,"SplitRoot");
    call_context(m,child(child(child(split,port?"P2":"P1"),"Buttons"),"Select"),"onActivate");
    page(m,"MakerRoot");
    ah_val now;CHECK(ah_get_member(m->vm,widget_module(m,split),"now1p",&now));
    CHECK(ah_num(now)==!port);
    /* Use the real maker and car navigation, not assigned car_code fields. */
    for(int i=0;i<100 && strcmp(menu_focus_name(m),"ford");i++){
        menu_send_input(m,MENU_DOWN);selection_tick(m);
    }
    CHECK(!strcmp(menu_focus_name(m),"ford"));
    menu_send_input(m,MENU_ACCEPT);page(m,"CarRoot");
    for(int i=0;i<50 && strcmp(menu_selected_car(m),code);i++){
        menu_send_input(m,MENU_RIGHT);advance(m);
    }
    if(strcmp(menu_selected_car(m),code))fprintf(stderr,"selected %s, expected %s\n",menu_selected_car(m),code);
    CHECK(!strcmp(menu_selected_car(m),code));
    menu_send_input(m,MENU_ACCEPT);advance(m);
    CHECK(!strcmp(menu_focus_name(m),"ColItem"));
    /* Reopening the shared catalog can retain the current colour strip. */
    for(int i=0;i<32;i++){
        ah_val color;CHECK(ah_call_member(m->vm,widget_module(m,root_named(m,"CarRoot")),"getCurrentColor",0,NULL,&color)>=0);
        if(ah_num(color)==paint)break;
        menu_send_input(m,ah_num(color)<paint?MENU_RIGHT:MENU_LEFT);advance(m);
    }
    menu_send_input(m,MENU_ACCEPT);page(m,"SettingPopup");
    ui_widget *pane=child(root_named(m,"SettingPopup"),"Pane");
    const char *rows[]={"Mission","Assist"};int indices[]={manual,assist};
    for(int i=0;i<2;i++){
        ah_val args[]={ah_objv(m->context),ah_int(indices[i])},ret;
        CHECK(ah_call_member(m->vm,widget_module(m,child(child(pane,rows[i]),"OptionMenu")),"setIndex",2,args,&ret)>=0);
    }
    call_context(m,child(child(pane,"Button"),"OK"),"onActivate");page(m,"SplitRoot");
    CHECK(!strcmp(menu_split_car(m,port),code));
    if(menu_game_number(m,port?"car_color_2p":"car_color",-1)!=paint)fprintf(stderr,"port%d paint %.0f expected %d\n",port,menu_game_number(m,port?"car_color_2p":"car_color",-1),paint);
    CHECK(menu_game_number(m,port?"car_color_2p":"car_color",-1)==paint);
    CHECK(menu_game_number(m,port?"manual_2p":"manual",-1)==manual);
    CHECK(menu_game_number(m,port?"assist_asm_2p":"assist_asm",-1)==(assist==2));
    CHECK(menu_game_number(m,port?"assist_tcs_2p":"assist_tcs",-1)==(assist>0));
}
int main(int argc,char **argv)
{
    CHECK(argc==2 || argc==3);const char *error=NULL;char msg[256];
    gt4_vol *vol=gt4_vol_open(argv[1],&error);CHECK(vol);
    CHECK(gt4_game_load_disc(argv[1]));
    menu *m=menu_open(vol,"arcade",msg,sizeof msg);CHECK(m);ah_set_log(m->vm,log_script,NULL);
    ah_set_member(m->vm,widget_module(m,root_named(m,"GameRoot")),"current_game",ah_str(m->vm,"SplitBattle"));
    ah_set_member(m->vm,widget_module(m,root_named(m,"CourseRoot")),"current_crs",ah_str(m->vm,"tsukuba"));
    ah_obj *game=game_object(m);
    ah_val arg=ah_str(m->vm,"ka_01"),ret;
    CHECK(ah_call_member(m->vm,game,"isCarAvailable",1,&arg,&ret)>=0 && ah_truthy(ret));
    arg=ah_str(m->vm,"model_t_tourer_15");
    CHECK(ah_call_member(m->vm,game,"isCarAvailable",1,&arg,&ret)>=0 && !ah_truthy(ret));
    ah_val available;CHECK(ah_get_member(m->vm,game,"available",&available));
    ah_set_member(m->vm,available.u.o,"all",ah_int(1));
    CHECK(ah_call_member(m->vm,game,"isCarAvailable",1,&arg,&ret)>=0 && ah_truthy(ret));
    ah_set_member(m->vm,available.u.o,"all",ah_int(0));
    if(argc==3 && !strcmp(argv[2],"--availability-negative"))ah_set_member(m->vm,available.u.o,"all",ah_int(1));
    CHECK(ah_call_member(m->vm,game,"isCarAvailable",1,&arg,&ret)>=0 && !ah_truthy(ret));
    ah_set_member(m->vm,game,"username",ah_str(m->vm,"Test"));
    ah_set_member(m->vm,widget_module(m,root_named(m,"MethodRoot")),"current_method",ah_str(m->vm,"ByMaker"));
    ah_set_member(m->vm,game,"car_code",ah_str(m->vm,""));ah_set_member(m->vm,game,"car_code_2p",ah_str(m->vm,""));
    /* CourseRoot.onActivate resets these before entering SplitRoot. */
    ah_set_member(m->vm,game,"car_color",ah_int(0));ah_set_member(m->vm,game,"car_color_2p",ah_int(0));
    ah_set_member(m->vm,game,"car_garage",ah_nil());ah_set_member(m->vm,game,"car_garage_2p",ah_nil());
    ui_widget *split=root_named(m,"SplitRoot");menu_enter(m,split->root);page(m,"SplitRoot");
    CHECK(!child(split,"RaceStart")->visible);
    ui_widget *select1=child(child(child(split,"P1"),"Buttons"),"Select");
    ui_widget *select2=child(child(child(split,"P2"),"Buttons"),"Select");
    CHECK(m->focus[split->root]==select1);
    selection_tick(m);float left_cursor=cursor_x;
    ui_cursor_hold(m->ui,1); /* outgoing popup held the previous focus */
    menu_send_input(m,MENU_RIGHT);
    if(argc==3 && !strcmp(argv[2],"--cursor-negative"))ui_cursor_hold(m->ui,1);
    selection_tick(m);CHECK(cursor_x>left_cursor+100);
    CHECK(m->focus[split->root]==select2);
    menu_send_input(m,MENU_LEFT);selection_tick(m);
    CHECK(m->focus[split->root]==select1);
    menu_send_input(m,MENU_ACCEPT);page(m,"MakerRoot");
    menu_send_input(m,MENU_BACK);page(m,"SplitRoot");
    choose(m,0,"ka_01",2,1,0);
    CHECK(!child(split,"RaceStart")->visible && !*menu_split_car(m,1));
    CHECK(m->focus[split->root]==child(child(child(split,"P2"),"Buttons"),"Select"));
    choose(m,1,"focus_st170_jp_03",1,0,2);
    CHECK(child(split,"RaceStart")->visible);
    CHECK(m->focus[split->root]==child(split,"RaceStart"));
    CHECK(!strcmp(menu_split_car(m,0),"ka_01") && menu_game_number(m,"car_color",-1)==2);
    ui_widget *face1=child(child(split,"P1"),"CarFace"),*face2=child(child(split,"P2"),"CarFace");
    CHECK(face1!=face2 && !strcmp(face1->car_code,"ka_01") && !strcmp(face2->car_code,"focus_st170_jp_03"));
    if(argc==3 && !strcmp(argv[2],"--paint-negative"))face1->car_color_index=face2->car_color_index;
    CHECK(face1->car_color_index==2 && face2->car_color_index==1);
    CHECK(face1->car_normal && face2->car_normal && face1->car_motion==1 && face2->car_motion==1);
    float yaw1=face1->model_angles[1],yaw2=face2->model_angles[1];advance(m);
    CHECK(face1->model_angles[1]!=yaw1 && face2->model_angles[1]!=yaw2);
    /* Cancel changing P1, without losing either player's confirmed choice. */
    call_context(m,child(child(child(split,"P1"),"Buttons"),"Select"),"onActivate");page(m,"MakerRoot");
    menu_send_input(m,MENU_BACK);page(m,"SplitRoot");
    CHECK(!strcmp(menu_split_car(m,0),"ka_01") && !strcmp(menu_split_car(m,1),"focus_st170_jp_03"));
    CHECK(face1->car_color_index==2 && face2->car_color_index==1);
    /* Changing only P1 also permits the same model with another paint. */
    choose(m,0,"focus_st170_jp_03",0,1,0);
    CHECK(!strcmp(menu_split_car(m,0),"focus_st170_jp_03"));
    CHECK(face1->car_color_index==0 && face2->car_color_index==1);
    CHECK(menu_game_number(m,"manual_2p",-1)==0 && menu_game_number(m,"assist_asm_2p",-1)==1);
    if(argc==3 && !strcmp(argv[2],"--negative"))ah_set_member(m->vm,game,"car_code_2p",ah_str(m->vm,"ka_01"));
    CHECK(!strcmp(menu_split_car(m,1),"focus_st170_jp_03"));
    /* Same menu boundary when either port chooses GarageRoot: the
       original MCarGarage carries all three sheets, parts and wear. */
    gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[1]);CHECK(factory);
    gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);
    ah_obj *owned[2];uint8_t reference_body[2][0x4b8],read_body[0x4b8];
    for(int port=0;port<2;port++){
        ah_val args[]={ah_str(m->vm,"focus_st170_jp_03"),ah_int(port)},value;
        CHECK(nat_car_garage(m->vm,NULL,m,2,args,&value)>=0);owned[port]=value.u.o;
        CHECK(gt4_game_car_body(m->vm,owned[port],reference_body[port]));
        ah_set_member(m->vm,game,port?"car_code_2p":"car_code",ah_str(m->vm,""));
        ah_set_member(m->vm,game,port?"car_garage_2p":"car_garage",value);
        int color;CHECK(menu_split_owned(m,port,read_body,&color)==1);
        CHECK(color==port && !memcmp(read_body,reference_body[port],sizeof read_body));
        CHECK(!strcmp(menu_split_car(m,port),"focus_st170_jp_03"));
    }
    if(argc==3 && !strcmp(argv[2],"--owned-negative"))ah_set_member(m->vm,game,"car_garage_2p",ah_objv(owned[0]));
    int paint;CHECK(menu_split_owned(m,1,read_body,&paint)==1 && paint==1);
    CHECK(!memcmp(read_body,reference_body[1],sizeof read_body));
    /* GameRoot.doGame also fills car_code in SingleRace/TimeTrial. */
    ah_set_member(m->vm,game,"car_code",ah_str(m->vm,"focus_st170_jp_03"));
    CHECK(menu_split_owned(m,0,read_body,&paint)==1 && paint==0);
    CHECK(!memcmp(read_body,reference_body[0],sizeof read_body));
    call_context(m,child(split,"RaceStart"),"onActivate");
    CHECK(menu_take_race_request(m) && menu_race_is_split(m) && !errors);
    /* arcade.ad L4675/L4690 writes the live main.game option without
       setNeedSave; the next project must still see that global option. */
    ah_set_member(m->vm,widget_module(m,root_named(m,"GameRoot")),"current_game",ah_str(m->vm,"SingleRace"));
    menu *driving_quick=NULL;
    for(int pass=0;pass<4;pass++){
        int manual=pass&1;
        ah_val option;CHECK(ah_get_member(m->vm,game,"option",&option));
        ah_set_member(m->vm,option.u.o,"automatic_gear",ah_int(manual));
        options_capture(m); /* preceding preference deliberately opposite */
        ah_val args[]={ah_objv(m->context),ah_int(manual)},result;
        ui_widget *setting=root_named(m,"SettingPopup");
        CHECK(ah_call_member(m->vm,widget_module(m,child(child(child(setting,"Pane"),"Mission"),"OptionMenu")),"setIndex",2,args,&result)>=0);
        CHECK(ah_call_member(m->vm,widget_module(m,setting),"apply_setting",0,NULL,&result)>=0);
        CHECK(menu_game_number(m,"manual",-1)==manual);
        CHECK(menu_option_number(m,"automatic_gear",-1)==1-manual);
        ah_set_member(m->vm,game,"next_sequence",ah_str(m->vm,"single_race"));
        CHECK(nat_finish(m->vm,NULL,m,0,NULL,&result)>=0 && menu_take_race_request(m));
        if(argc==3 && !strcmp(argv[2],"--driving-negative") && manual){
            ah_set_member(m->vm,option.u.o,"automatic_gear",ah_int(1));
            options_capture(m);gt4_save_capture(m->vm,game,1);
        }
        menu_quick_race work={0};strcpy(work.course,"tsukuba");work.entries=1;work.player=0;work.dry=1;
        strcpy(work.grid[0].car,"ka_01");work.grid[0].power=61;work.grid[0].weight=965;menu_set_quick_race(&work);
        if(!driving_quick)driving_quick=menu_open(vol,"quick-arcade",msg,sizeof msg);
        menu *quick=driving_quick;CHECK(quick);ah_set_log(quick->vm,log_script,NULL);
        CHECK(menu_enter_quick_start(quick));advance(quick);
        menu_quick_tune_values tune;CHECK(menu_quick_tune(quick,&tune));
        fprintf(stderr,"Arcade choice %d reaches pre-race %d\n",manual,tune.transmission);
        CHECK(tune.transmission==manual && !errors);
        CHECK(!strcmp(menu_focus_name(quick),"Start"));
        menu_send_input(quick,MENU_ACCEPT);advance(quick);char start_command[64];
        CHECK(menu_take_quick_command(quick,start_command,sizeof start_command));
        CHECK(!strcmp(start_command,"COMMAND_START") && !errors);
    }
    menu_close(driving_quick);
    gt4_game_set_car_factory(NULL,NULL);gt4_career_vehicle_close(factory);
    menu_close(m);gt4_vol_close(vol);
    puts("PASS original two-player selection: cars, paints, settings, cancel, gated start");return 0;
}
