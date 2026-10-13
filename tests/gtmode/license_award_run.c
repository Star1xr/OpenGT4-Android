/* C: authored licence prize eligibility/delivery, with injected medals.
   gtmode.ad PresentRoot L14176..14206 checks the original profile records;
   all car identities and conditions come from PresentData on the disc. */
#define main pregrid_fixture_main
#include "../ui/pregrid_menu.c"
#undef main
#include "career_vehicle.h"
static ah_val get(ah_vm *vm,ah_obj *o,const char *name){ah_val v;CHECK(ah_get_member(vm,o,name,&v));return v;}
int main(int argc,char **argv){
 CHECK(argc==2);gt4_vol *vol=gt4_vol_open(argv[1],NULL);CHECK(vol && gt4_game_load_disc(argv[1]));
 gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[1]);CHECK(factory);gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);
 char dir[200],error[256];snprintf(dir,sizeof dir,"build/career-native/license-awards-%llu-%llu",(unsigned long long)time(NULL),(unsigned long long)clock());
 gt4_save_init(dir);menu *m=menu_open(vol,"gtmode",error,sizeof error);CHECK(m);ah_vm *vm=m->vm;ah_set_log(vm,log_script,NULL);
 menu_save_capture(m);CHECK(gt4_save_create(1,"Licence prize validation"));
 const char *groups[]={"l0b","l0a","lib","lia","l0s"};
 const char *conditions[]={"license","all_silver","all_gold"};
 ah_obj *page=get(vm,m->project,"PresentRoot").u.o,*data=get(vm,m->project,"PresentData").u.o;
 ah_obj *details=get(vm,data,"detail_carlist").u.o,*kinds=get(vm,data,"condition_carlist").u.o;
 int ids[3][5],paper_ids[5]={-1,-1,-1,-1,-1};unsigned found=0,delivered=0;
 for(unsigned grade=0;grade<3;grade++)for(unsigned g=0;g<5;g++){
  ids[grade][g]=-1;
  for(int i=0;i<ah_array_size(details);i++){
   const char *category=ah_cstr(ah_array_get(details,i)),*kind=ah_cstr(ah_array_get(kinds,i));
   const char *type=ah_cstr(ah_array_get(get(vm,data,"type_carlist").u.o,i));
   if(category && kind && type && !strcmp(category,groups[g]) && !strcmp(kind,conditions[grade])){
    if(!strcmp(type,"paper")){CHECK(!grade && paper_ids[g]<0);paper_ids[g]=i;}
    else{CHECK(!strcmp(type,"car") && ids[grade][g]<0);ids[grade][g]=i;found++;}
   }
  }
  CHECK(ids[grade][g]>=0);
 }
 CHECK(found==15);
 for(unsigned grade=0;grade<3;grade++){
  for(unsigned g=0;g<5;g++){
   int id=ids[grade][g];ah_val arg=ah_int(id),column,ret;
   CHECK(ah_call_member(vm,page,"makeColumn",1,&arg,&column)>=0);ah_val args[]={arg,column};
   CHECK(ah_call_member(vm,page,conditions[grade],2,args,&ret)>=0 && !ah_truthy(ret));
   for(unsigned n=1;n<=16;n++){
    char code[32];snprintf(code,sizeof code,"%s%04u",groups[g],n);gt4_career_license_attempts visit;
    CHECK(menu_license_begin(m,code,&visit));
    CHECK(menu_license_attempt(m,code,&visit,4-(int)grade,60000-grade*1000,0x060a07eau));
    CHECK(menu_license_leave(m,code,&visit));
   }
   CHECK(ah_call_member(vm,page,conditions[grade],2,args,&ret)>=0 && ah_truthy(ret));
   CHECK(menu_enter_career_return(m));CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"PresentRoot"));
   ah_obj *list=get(vm,page,"current_list").u.o;int entitled=0;
   for(int i=0;i<ah_array_size(list);i++)entitled|=ah_num(ah_array_get(list,i))==id;
   CHECK(entitled);
   if(!grade){
    CHECK(paper_ids[g]>=0);ah_val paper_id=ah_int(paper_ids[g]),paper_column;
    CHECK(ah_call_member(vm,page,"makeColumn",1,&paper_id,&paper_column)>=0);
    ah_val paper_args[]={paper_id,paper_column};
    /* PresentRoot.get_next marks the paper before presenting it (L14456).
       The headless presenter calls the original pane as a separate step. */
    CHECK(ah_call_member(vm,page,"license",2,paper_args,&ret)>=0 && !ah_truthy(ret));
    ah_val paper_delivery[]={ah_objv(m->context),ah_str(vm,groups[g])};
    CHECK(ah_call_member(vm,get(vm,page,"PaperPane").u.o,"get_paper",2,paper_delivery,&ret)>=0 && !errors);
    menu_save_capture(m);CHECK(menu_enter_career_return(m));
   }
   ah_obj *pane=get(vm,page,"CarPane").u.o;
   ah_val delivery[]={ah_objv(m->context),ah_array_get(column.u.o,0),ah_array_get(column.u.o,1),ah_array_get(column.u.o,2),ah_array_get(column.u.o,3),ah_array_get(column.u.o,4)};
   CHECK(ah_call_member(vm,pane,"get_car",6,delivery,&ret)>=0 && !errors);delivered++;
   ah_obj *game=game_object(m),*cars=get(vm,get(vm,game,"garage").u.o,"cars").u.o;CHECK(ah_array_size(cars)==(int)delivered);
   ah_obj *car=ah_array_get(cars,(int)delivered-1).u.o;const char *label=ah_cstr(get(vm,car,"car_code"));CHECK(label);
   uint8_t unit[32]={0},expected[0x4b8],actual[0x4b8];int color=(int)ah_num(get(vm,car,"color_index"));
   CHECK(gt4_career_vehicle_create(factory,label,color,0,unit,expected) && gt4_game_car_body(vm,car,actual));
   if(getenv("GT4_LICENSE_AWARD_NEGATIVE"))actual[0x498]^=1;
   CHECK(!memcmp(actual,expected,sizeof actual));
   CHECK(ah_num(get(vm,get(vm,game,"statistics").u.o,"prize_cars"))==delivered);
   CHECK(ah_call_member(vm,page,conditions[grade],2,args,&ret)>=0 && !ah_truthy(ret));
   menu_save_capture(m);CHECK(gt4_save_store() && gt4_save_load_slot(1));menu_save_refresh(m);
   game=game_object(m);cars=get(vm,get(vm,game,"garage").u.o,"cars").u.o;CHECK(ah_array_size(cars)==(int)delivered);
   CHECK(gt4_game_car_body(vm,ah_array_get(cars,(int)delivered-1).u.o,actual) && !memcmp(actual,expected,sizeof actual));
   CHECK(ah_call_member(vm,page,conditions[grade],2,args,&ret)>=0 && !ah_truthy(ret));
  }
 }
 CHECK(!errors);printf("PASS 15 authored licence prize cars, medal gates, complete vehicle bodies, save/reload and duplicate prevention (injected attempt results).\n");
 menu_close(m);gt4_save_shutdown();gt4_game_set_car_factory(NULL,NULL);gt4_career_vehicle_close(factory);gt4_vol_close(vol);return 0;
}
