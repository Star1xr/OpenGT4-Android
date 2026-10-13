/* Exercise the disc's Arcade exit and UnitRoot apply callbacks through the
   native project cache. No user profile or physics is involved. */
#define main setting_fixture_main
#include "../gtmode/setting_menu.c"
#undef main

static int choose(menu *m,ui_widget *w,int index){
 CHECK(w && index>=0 && index<ui_option_count(w));
 set_focus(m,w);menu_send_input(m,MENU_ACCEPT);
 for(unsigned n=0;w->option_pending!=index && n<16;n++)menu_send_input(m,MENU_DOWN);
 CHECK(w->option_pending==index);menu_send_input(m,MENU_ACCEPT);return 0;
}
int main(int argc,char **argv){
 CHECK(argc==2);const char *err=NULL;char msg[256];
 gt4_vol *vol=gt4_vol_open(argv[1],&err);CHECK(vol && gt4_game_load_disc(argv[1]));
 gt4_save_init("");menu_options_storage(getenv("GT4_OPTIONS_FILE"));
 if(getenv("GT4_OPTIONS_SAVE_DIR"))gt4_save_init(getenv("GT4_OPTIONS_SAVE_DIR"));
 menu *arcade=menu_open(vol,"arcade",msg,sizeof msg);CHECK(arcade);page(arcade,"TopRoot");ticks(arcade);
 ah_val ret;ah_set_member(arcade->vm,game_object(arcade),"next_sequence",ah_str(arcade->vm,"single_race"));
 CHECK(nat_finish(arcade->vm,NULL,arcade,0,NULL,&ret)==0 && menu_take_race_request(arcade));
 menu_arcade_return(arcade);menu_set_origin(arcade,"quick-arcade");page(arcade,"TopRoot");ticks(arcade);
 ui_widget *top=ui_root_widget(arcade->ui,menu_active_root(arcade));
 ui_widget *button=child(child(top,"hidden"),"Option");CHECK(button);
 ah_val args[]={ah_objv(arcade->context),ah_nil(),ah_nil()};
 CHECK(ah_call_member(arcade->vm,widget_module(arcade,button),"onActivate",3,args,&ret)>=0);ticks(arcade);
 char route[64];if(getenv("GT4_OPTIONS_ROUTE_NEGATIVE"))arcade->race_requested=1;
 CHECK(!menu_take_race_request(arcade));
 CHECK(menu_take_project(arcade,route,sizeof route) && !strcmp(route,"option"));
 menu *option=menu_open(vol,"option",msg,sizeof msg);CHECK(option);ah_set_log(option->vm,log_script,NULL);menu_set_origin(option,"arcade");
 menu *cached=menu_open(vol,"arcade",msg,sizeof msg);CHECK(cached);
 if(getenv("GT4_OPTIONS_SAVE_DIR"))CHECK(gt4_save_create(1,"Options validation"));
 const char *power[]={"PS","HP","BHP","CH","KW","CV","PF"};
 const char *power_label[]={"PS","HP","BHP","CH","kW","CV","PF"};
 const char *torque[]={"KGFM","FTLB","NM"},*torque_label[]={"kgf.m","ft.lb","NM"};
 for(int i=0;i<7;i++){
  CHECK(page(option,"UnitRoot")>=0);ticks(option);
  ui_widget *root=ui_root_widget(option->ui,menu_active_root(option)),*pane=child(root,"Pane");CHECK(pane);
  CHECK(!choose(option,child(child(pane,"Velocity"),"OptionMenu"),i%2));
  CHECK(!choose(option,child(child(pane,"Power"),"OptionMenu"),i));
  CHECK(!choose(option,child(child(pane,"Torque"),"OptionMenu"),i%3));
  set_focus(option,child(child(pane,"Buttons"),"OK"));menu_send_input(option,MENU_ACCEPT);ticks(option);
  CHECK(!errors);
  if(getenv("GT4_OPTIONS_FILE"))menu_options_storage(getenv("GT4_OPTIONS_FILE"));
  CHECK(!strcmp(menu_option_text(option,"unit_power","?"),power[i]));
  CHECK(!strcmp(menu_option_text(option,"unit_torque","?"),torque[i%3]));
  /* The cached destination must see applied preferences on entry. */
  page(cached,"TopRoot");ticks(cached);
  if(getenv("GT4_OPTIONS_UNITS_NEGATIVE"))ah_set_member(cached->vm,get(cached->vm,game_object(cached),"option").u.o,"unit_power",ah_str(cached->vm,"bad"));
  CHECK(!strcmp(menu_option_text(cached,"unit_power","?"),power[i]));
  CHECK(!strcmp(menu_option_text(cached,"unit_torque","?"),torque[i%3]));
  CHECK(!strcmp(menu_option_text(cached,"unit_velocity","?"),i%2?"MPH":"KMPH"));
  CHECK(nat_power_unit(cached->vm,NULL,NULL,0,NULL,&ret)==0 && !strcmp(ah_cstr(ret),power_label[i]));
  ah_val ps=ah_float(200);CHECK(nat_power_unit(cached->vm,NULL,(void *)1,1,&ps,&ret)==0);
  CHECK(ret.u.i==(i==1 || i==2?197:i==4?147:200));
  CHECK(nat_torque_unit(cached->vm,NULL,NULL,0,NULL,&ret)==0 && !strcmp(ah_cstr(ret),torque_label[i%3]));
  ah_val kg=ah_float(100);CHECK(nat_torque_unit(cached->vm,NULL,(void *)1,1,&kg,&ret)==0);
  CHECK(ret.u.i==(i%3==1?723:i%3==2?980:100));
  /* Refreshing an untouched cached project must not undo these choices. */
  menu_save_capture(arcade);menu_save_refresh(option);
  CHECK(!strcmp(menu_option_text(option,"unit_power","?"),power[i]));
  /* Reopen reflects the applied indices; Cancel does not commit edits. */
  CHECK(page(option,"UnitRoot")>=0);ticks(option);
  root=ui_root_widget(option->ui,menu_active_root(option));pane=child(root,"Pane");
  CHECK(child(child(pane,"Power"),"OptionMenu")->focus_index==i);
  CHECK(child(child(pane,"Torque"),"OptionMenu")->focus_index==i%3);
  CHECK(!choose(option,child(child(pane,"Power"),"OptionMenu"),(i+1)%7));
  set_focus(option,child(child(pane,"Buttons"),"Cancel"));menu_send_input(option,MENU_ACCEPT);ticks(option);
  CHECK(!strcmp(menu_option_text(option,"unit_power","?"),power[i]));
  if(getenv("GT4_OPTIONS_SAVE_DIR")){
   CHECK(gt4_save_store() && gt4_save_load_slot(1));menu_save_refresh(cached);
   CHECK(!strcmp(menu_option_text(cached,"unit_power","?"),power[i]));
   CHECK(!strcmp(menu_option_text(cached,"unit_torque","?"),torque[i%3]));
  }
 }
 CHECK(!errors);
 /* A new authored race request still works after the neutral menu entry. */
 ah_set_member(arcade->vm,game_object(arcade),"next_sequence",ah_str(arcade->vm,"time_trial"));
 CHECK(nat_finish(arcade->vm,NULL,arcade,0,NULL,&ret)==0 && menu_take_race_request(arcade));
 menu_close(cached);menu_close(option);menu_close(arcade);gt4_save_shutdown();gt4_vol_close(vol);
 puts("PASS Arcade return opens Options; seven power/three torque/two speed units apply across cached projects.");return 0;
}
