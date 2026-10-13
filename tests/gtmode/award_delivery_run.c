/* Actual authored project-prize eligibility and get_car delivery, from a
   fresh native profile through save/reload. No completed-car fixture. */
#define main pregrid_fixture_main
#include "../ui/pregrid_menu.c"
#undef main
#include "career_vehicle.h"
#include "career_blob.h"
static ah_val value(ah_vm *vm,ah_obj *obj,const char *name){ah_val v;CHECK(ah_get_member(vm,obj,name,&v));return v;}
int main(int argc,char **argv){
 CHECK(argc==2);const char *error=NULL;gt4_vol *v=gt4_vol_open(argv[1],&error);CHECK(v && gt4_game_load_disc(argv[1]));
 gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[1]);CHECK(factory);gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);
 char directory[180],msg[256];snprintf(directory,sizeof directory,"build/career-native/award-delivery-%llu-%llu",(unsigned long long)time(NULL),(unsigned long long)clock());gt4_save_init(directory);
 menu *m=menu_open(v,"gtmode",msg,sizeof msg);CHECK(m);ah_vm *vm=m->vm;ah_set_log(vm,log_script,NULL);menu_save_capture(m);if(!gt4_save_create(1,"Award validation")){fprintf(stderr,"save: %s\n",gt4_save_error());return 1;}
 ah_obj *game=game_object(m),*page=value(vm,m->project,"PresentRoot").u.o,*data=value(vm,m->project,"PresentData").u.o;
 ah_obj *details=value(vm,data,"detail_carlist").u.o;int id=-1;for(int i=0;i<ah_array_size(details);i++){const char *s=ah_cstr(ah_array_get(details,i));if(s && !strcmp(s,"am_sunday_0000")){id=i;break;}}CHECK(id>=0);
 ah_val prize_id=ah_int(id),column,ret;CHECK(ah_call_member(vm,page,"makeColumn",1,&prize_id,&column)>=0);ah_val args[]={prize_id,column};CHECK(ah_call_member(vm,page,"win",2,args,&ret)>=0 && !ah_truthy(ret));
 /* race_util.ad:57..64 enumerates consecutive labels until absent. */
 unsigned races=0;for(unsigned i=1;i<99;i++){char race[32];snprintf(race,sizeof race,"am_sunday_%04u",i);uint32_t key;if(!gt4_specdb_label(m->race_labels,m->race_labels_size,race,&key))break;uint8_t best=15;uint32_t prize=0;CHECK(menu_apply_career_result(m,race,1,0,60000,0,20,600,&best,&prize));races++;char next[32];snprintf(next,sizeof next,"am_sunday_%04u",i+1);if(gt4_specdb_label(m->race_labels,m->race_labels_size,next,&key)){CHECK(menu_enter_career_return(m));ah_val label=ah_str(vm,race),record;CHECK(ah_call_member(vm,value(vm,game,"race_record").u.o,"getUnit",1,&label,&record)>=0);CHECK(!strcmp(ah_cstr(value(vm,record.u.o,"present_result")),"NONE"));CHECK(!strcmp(ah_cstr(value(vm,record.u.o,"spot_result")),"1"));}}CHECK(races);
 CHECK(menu_enter_career_return(m));CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"PresentRoot"));ah_obj *entitled=value(vm,page,"current_list").u.o;int found=0;for(int i=0;i<ah_array_size(entitled);i++)found|=(int)ah_num(ah_array_get(entitled,i))==id;CHECK(found && !errors);
 ah_obj *pane=value(vm,page,"CarPane").u.o;ah_val delivery[]={ah_objv(m->context),ah_array_get(column.u.o,0),ah_array_get(column.u.o,1),ah_array_get(column.u.o,2),ah_array_get(column.u.o,3),ah_array_get(column.u.o,4)};
 int days_before=(int)ah_num(value(vm,value(vm,game,"calendar").u.o,"elapsed_days"));
 CHECK(ah_call_member(vm,pane,"get_car",6,delivery,&ret)>=0);CHECK(!errors);
 ah_obj *garage=value(vm,game,"garage").u.o,*cars=value(vm,garage,"cars").u.o;CHECK(ah_array_size(cars)==1);ah_obj *car=ah_array_get(cars,0).u.o;
 const char *label=ah_cstr(value(vm,car,"car_code"));CHECK(label);char saved_label[65];snprintf(saved_label,sizeof saved_label,"%s",label);
 CHECK(ah_num(value(vm,value(vm,game,"calendar").u.o,"elapsed_days"))==days_before+1);ah_val car_label=ah_str(vm,label);CHECK(ah_call_member(vm,game,"isCarAvailable",1,&car_label,&ret)>=0 && ah_truthy(ret));CHECK(!strcmp(menu_pcm_request(m,NULL),"get_car"));
 uint8_t unit[32]={0},expected[0x4b8],actual[0x4b8];int color=(int)ah_num(value(vm,car,"color_index"));CHECK(gt4_career_vehicle_create(factory,label,color,0,unit,expected));CHECK(gt4_game_car_body(vm,car,actual));
 if(getenv("GT4_AWARD_DELIVERY_NEGATIVE"))actual[0x498]^=1;CHECK(!memcmp(actual,expected,sizeof actual));
 CHECK(ah_num(value(vm,value(vm,game,"statistics").u.o,"prize_cars"))==1);menu_save_capture(m);CHECK(gt4_save_store() && gt4_save_load_slot(1));menu_save_refresh(m);
 game=game_object(m);cars=value(vm,value(vm,game,"garage").u.o,"cars").u.o;CHECK(ah_array_size(cars)==1);car=ah_array_get(cars,0).u.o;CHECK(!strcmp(ah_cstr(value(vm,car,"car_code")),saved_label));CHECK(gt4_game_car_body(vm,car,actual) && !memcmp(actual,expected,sizeof actual));
 CHECK(ah_num(value(vm,value(vm,game,"calendar").u.o,"elapsed_days"))==days_before+1);car_label=ah_str(vm,saved_label);CHECK(ah_call_member(vm,game,"isCarAvailable",1,&car_label,&ret)>=0 && ah_truthy(ret));CHECK(ah_num(value(vm,value(vm,game,"statistics").u.o,"prize_cars"))==1);
 CHECK(ah_call_member(vm,page,"win",2,args,&ret)>=0 && !ah_truthy(ret));CHECK(!errors);
 puts("PASS actual Sunday Cup eligibility, authored car delivery, complete native vehicle body, prize statistics, save/reload and prevention of duplicate award.");menu_close(m);gt4_save_shutdown();gt4_game_set_car_factory(NULL,NULL);gt4_career_vehicle_close(factory);gt4_vol_close(v);return 0;
}
