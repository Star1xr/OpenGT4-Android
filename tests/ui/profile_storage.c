/* PC storage adaptation P: real authored garage/Trade helper contracts. */
#define main pregrid_regression_main
#include "pregrid_menu.c"
#undef main
#include "career_vehicle.h"
#include <time.h>
static int selected_profile=2;
static int picker(menu *m){(void)m;return selected_profile;}
static ui_widget *named(menu *m,const char *name){for(int i=0;i<ui_root_count(m->ui);i++)if(!strcmp(ui_root_name(m->ui,i),name))return ui_root_widget(m->ui,i);CHECK(0);return NULL;}
static ui_widget *find(ui_widget *w,const char *name){if(!strcmp(w->name,name))return w;for(int i=0;i<w->nkids;i++){ui_widget *v=find(w->kids[i],name);if(v)return v;}return NULL;}
static int info_dialog(menu*m,const char*t,const char*s,int kind,const char*y,const char*n){(void)m;(void)t;(void)s;(void)kind;(void)y;(void)n;return 1;}
int main(int argc,char **argv){
 CHECK(argc==2 || argc==3);const char *error=NULL;char msg[256],directory[240];
 snprintf(directory,sizeof directory,"build/profile-storage-%llu-%llu",(unsigned long long)time(NULL),(unsigned long long)clock());gt4_save_init(directory);
 CHECK(gt4_game_load_disc(argv[1]));gt4_vol *vol=gt4_vol_open(argv[1],&error);CHECK(vol);
 menu *m=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(m);ah_set_log(m->vm,log_script,NULL);
 gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[1]);CHECK(factory);gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);
 menu_save_capture(m);CHECK(gt4_save_create(2,"Garage source"));menu_save_refresh(m);
 ah_obj *source=game_object(m);ah_val garage,car;ah_get_member(m->vm,source,"garage",&garage);
 ah_val args[]={ah_str(m->vm,"ka_01"),ah_int(2)};CHECK(nat_car_garage(m->vm,NULL,m,2,args,&car)>=0);
 ah_val initial_cars;CHECK(ah_get_member(m->vm,garage.u.o,"cars",&initial_cars));ah_array_push(m->vm,initial_cars.u.o,car);
 uint8_t expected_body[0x4b8],actual_body[0x4b8];CHECK(gt4_game_car_body(m->vm,car.u.o,expected_body));
 ah_set_member(m->vm,source,"money",ah_int(12345));CHECK(gt4_save_capture(m->vm,source,1) && gt4_save_store());
 CHECK(gt4_save_create(1,"Active player"));menu_save_refresh(m);ah_obj *active=game_object(m);ah_set_member(m->vm,active,"money",ah_int(67890));CHECK(gt4_save_capture(m->vm,active,1));
 menu_set_profile_picker(m,picker);menu_set_dialog_presenter(m,info_dialog);
 ah_obj *manager=ah_module_new(m->vm,"manager",NULL,NULL,NULL);ah_val mode[]={ah_str(m->vm,"MODE_LOAD_GAME"),ah_int(1)},ret;
 CHECK(nat_pc_manager(m->vm,manager,m,2,mode,&ret)>=0);
 ah_val check[]={ah_objv(m->context),ah_int(0),ah_objv(manager),ah_int(1)};
 CHECK(ah_call_member(m->vm,m->project,"checkGameData",4,check,&ret)>=0 && ret.u.o==manager);
 ah_val create[]={ah_objv(m->context),ah_int(0),ah_nil(),ah_int(1)};
 CHECK(ah_call_member(m->vm,m->project,"checkGameData",4,create,&ret)>=0 && ret.t!=AV_NIL);
 ah_val load[]={ah_objv(m->context),ah_objv(manager)};
 CHECK(ah_call_member(m->vm,m->project,"loadGameDataForTrade",2,load,&ret)>=0);
 ah_obj *pair=ah_as_obj(ret,AO_ARRAY);CHECK(pair && !strcmp(ah_cstr(ah_array_get(pair,0)),"OK"));
 ah_obj *other=ah_array_get(pair,1).u.o;ah_val money,cars;CHECK(ah_get_member(m->vm,other,"money",&money));
 if(argc==3)ah_set_member(m->vm,other,"money",ah_int(67890));
 CHECK(ah_get_member(m->vm,other,"money",&money) && ah_num(money)==12345);
 CHECK(gt4_save_active()==1 && gt4_save_dirty());
 CHECK(ah_get_member(m->vm,other,"garage",&garage) && ah_get_member(m->vm,garage.u.o,"cars",&cars));CHECK(ah_array_size(cars.u.o)==1);
 CHECK(gt4_game_car_body(m->vm,ah_array_get(cars.u.o,0).u.o,actual_body) && !memcmp(actual_body,expected_body,sizeof actual_body));
 selected_profile=0;CHECK(ah_call_member(m->vm,m->project,"checkGameData",4,check,&ret)>=0 && ret.t==AV_NIL);
 CHECK(gt4_save_active()==1 && gt4_save_dirty());
 CHECK(nat_open_save(m->vm,NULL,m,0,NULL,&ret)>=0);CHECK(!gt4_save_dirty() && !m->next_project[0]);
 CHECK(gt4_save_load_slot(1));menu_save_refresh(m);CHECK(ah_get_member(m->vm,game_object(m),"money",&money) && ah_num(money)==67890);
 CHECK(ah_call_member(m->vm,m->project,"loadGameData",2,load,&ret)>=0 && !strcmp(ah_cstr(ret),"OK"));CHECK(gt4_save_active()==2);
 CHECK(gt4_save_load_slot(1));menu_save_refresh(m);
 selected_profile=2;ui_widget *home=named(m,"HomeRoot");menu_enter(m,home->root);advance(m);
 ui_widget *trade=find(home,"Trade");CHECK(trade);ah_val context=ah_objv(m->context);
 ui_widget *save=find(home,"Save");CHECK(save);ah_set_member(m->vm,game_object(m),"money",ah_int(67891));CHECK(gt4_save_capture(m->vm,game_object(m),1));
 CHECK(ah_call_member(m->vm,widget_module(m,save),"onActivate",1,&context,&ret)>=0);
 CHECK(!gt4_save_dirty() && !m->next_project[0]);

 CHECK(ah_call_member(m->vm,widget_module(m,trade),"onActivate",1,&context,&ret)>=0);apply_pages(m);advance(m);
 CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"GarageRoot") && !errors && gt4_save_active()==1);

 ui_widget *trade_root=named(m,"GarageRoot"),*trade_cars=find(trade_root,"Cars");CHECK(trade_cars);
 ah_val trade_args[]={ah_objv(m->context),ah_nil()};
 ah_val before_garage,before_cars,before_money;ah_get_member(m->vm,game_object(m),"garage",&before_garage);ah_get_member(m->vm,before_garage.u.o,"cars",&before_cars);ah_get_member(m->vm,game_object(m),"money",&before_money);
 int before_count=ah_array_size(before_cars.u.o);
 CHECK(ah_call_member(m->vm,widget_module(m,trade_cars),"onActivate",2,trade_args,&ret)>=0);apply_pages(m);advance(m);
 ah_val after_cars,after_money;ah_get_member(m->vm,before_garage.u.o,"cars",&after_cars);ah_get_member(m->vm,game_object(m),"money",&after_money);
 CHECK(ah_array_size(after_cars.u.o)==before_count+1 && ah_num(after_money)<ah_num(before_money) && gt4_save_active()==1 && !errors);
 CHECK(gt4_save_capture(m->vm,game_object(m),1) && gt4_save_store());CHECK(gt4_save_load_slot(1));menu_save_refresh(m);
 ah_obj *unchanged=ah_module_new(m->vm,"source_check",NULL,NULL,NULL);CHECK(gt4_save_read_profile(m->vm,unchanged,2));ah_val original_garage,original_cars;CHECK(ah_get_member(m->vm,unchanged,"garage",&original_garage) && ah_get_member(m->vm,original_garage.u.o,"cars",&original_cars));CHECK(ah_array_size(original_cars.u.o)==1);
 CHECK(gt4_game_car_body(m->vm,ah_array_get(original_cars.u.o,0).u.o,actual_body) && !memcmp(actual_body,expected_body,sizeof actual_body));
 menu_close(m);m=menu_open(vol,"arcade",msg,sizeof msg);CHECK(m);ah_set_log(m->vm,log_script,NULL);menu_set_profile_picker(m,picker);selected_profile=2;
 ui_widget *maker=named(m,"MakerRoot");menu_enter(m,maker->root);advance(m);
 ah_val open[]={ah_objv(m->context),ah_int(0)};selected_profile=0;
 CHECK(ah_call_member(m->vm,widget_module(m,maker),"openGarage",2,open,&ret)>=0);apply_pages(m);advance(m);
 CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"MakerRoot") && !errors && gt4_save_active()==1);
 selected_profile=2;CHECK(ah_call_member(m->vm,widget_module(m,maker),"openGarage",2,open,&ret)>=0);apply_pages(m);advance(m);
 CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"GarageRoot") && !errors && gt4_save_active()==1);
 menu_close(m);gt4_game_set_car_factory(NULL,NULL);gt4_career_vehicle_close(factory);gt4_vol_close(vol);gt4_save_shutdown();
 puts("PASS local profiles: authored Trade loader, independent source garage, cancel, authored Trade purchase/reload/source unchanged, Save writes active player, Arcade garage entry");return 0;
}
