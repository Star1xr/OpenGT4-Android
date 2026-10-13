/* Original option.ad RaceRoot -> GT Mode/prepared player/Retry. */
#define main fitted_parts_original_main
#include "fitted_parts.c"
#undef main
#include "career_event.h"
static int named_root(menu*m,const char*n){for(int i=0;i<ui_root_count(m->ui);i++)if(!strcmp(ui_root_name(m->ui,i),n))return i;CHECK(0);return 0;}
static void selected(menu*m,int manual){
 int root=named_root(m,"RaceRoot");menu_enter(m,root);
 ui_widget*page=ui_root_widget(m->ui,root),*selector=ui_widget_child(ui_widget_child(ui_widget_child(page,"Pane"),"Transmission"),"OptionMenu");CHECK(selector);
 ah_val args[]={ah_objv(m->context),ah_int(manual)},result;
 CHECK(ah_call_member(m->vm,widget_module(m,selector),"setIndex",2,args,&result)>=0);
 /* option.ad apply L2106 also marks setNeedSave before changing fields. */
 CHECK(ah_call_member(m->vm,widget_module(m,page),"apply",0,NULL,&result)>=0);
 menu_tick(m);menu_save_capture(m);
 CHECK(menu_option_number(m,"automatic_gear",-1)==1-manual);
}
static void gearbox(race_game*g,int manual){
 unsigned cond=gt4_race_run_conductor(g->run),race=GT4_U32(cond),entry=GT4_U32(GT4_U32(GT4_U32(race+0x60)+8));
 CHECK(GT4_U32(entry+0x3448)==manual);
 CHECK(g->last_desc.manual==manual);
 /* Run through the original start so the live player's gearbox consumes
    entry+3448; checking the menu/descriptor alone hid the earlier bug. */
 GT4_U32(race+0xbc)=0;GT4_U32(cond+0xcbfc)=0;
 for(int i=0;i<1200;i++)race_game_update(g,STEP+1e-12);
 unsigned car=gt4_race_run_car(g->run,0);
 fprintf(stderr,"GT transmission %d live kind %u\n",manual,GT4_U8(car+0x546));
 CHECK(GT4_U8(car+0x546)==manual);
}
int main(int argc,char**argv){
 CHECK(argc>=3);int negative=argc>3;const char*why;char msg[256];
 CHECK(!plat_video_init(&(plat_video_desc){640,448,0,0,0}));SDL_HideWindow(plat_sdl_window());
 menu_options_storage(NULL);gt4_save_init(argv[2]);gt4_vol*vol=gt4_vol_open(argv[1],&why);CHECK(vol && gt4_game_load_disc(argv[1]));
 gt4_career_vehicle*f=gt4_career_vehicle_open(argv[1]);CHECK(f);gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,f);
 menu*gt=menu_open(vol,"gtmode",msg,sizeof msg),*options=menu_open(vol,"option",msg,sizeof msg);CHECK(gt && options);
 ah_obj*garage=field(gt->vm,game_object(gt),"garage").u.o;ah_val args[]={ah_str(gt->vm,"ka_01"),ah_int(0)},result;
 CHECK(ah_call_member(gt->vm,garage,"addNewCar",2,args,&result)==1);ah_val index=ah_int(0);CHECK(ah_call_member(gt->vm,garage,"setRidingCar",1,&index,&result)==1);
 uint8_t body[0x4b8];CHECK(gt4_game_car_body(gt->vm,field(gt->vm,garage,"riding_car").u.o,body));menu_save_capture(gt);
 race_game*g=race_game_open(argv[1]);CHECK(g);uint32_t rng;CHECK(gt4_career_event_rng_load(argv[1],&rng));
 for(int pass=0;pass<3;pass++){
  int manual=pass==1;selected(options,manual);menu_save_refresh(gt);CHECK(menu_driving_manual(gt)==manual);
  gt4_career_event event;CHECK(gt4_career_event_prepare(&event,argv[1],gt4_game_race_lookup_data(),"am_sunday_0001",body,0,&rng,msg,sizeof msg));
  event.desc.manual=negative?(int)menu_game_number(gt,"manual",0):menu_driving_manual(gt);
  CHECK(race_game_start_prepared(g,&event.desc,msg,sizeof msg));ready(g);gearbox(g,manual);
  CHECK(race_game_restart(g,msg,sizeof msg));ready(g);gearbox(g,manual);
  /* Reopening Options after loading a grid must update the cached desc. */
  selected(options,!manual);menu_save_refresh(gt);
  CHECK(race_game_set_transmission(g,menu_driving_manual(gt)));
  CHECK(race_game_restart(g,msg,sizeof msg));ready(g);gearbox(g,!manual);
  CHECK(!race_game_set_transmission(g,!manual));
  printf("PASS GT option %d: original apply, prepared Sunday Cup, Retry, pregrid change\n",manual);
 }
 race_game_close(g);menu_close(options);menu_close(gt);gt4_game_set_car_factory(NULL,NULL);gt4_career_vehicle_close(f);gt4_vol_close(vol);gt4_save_shutdown();plat_video_shutdown();return 0;
}
