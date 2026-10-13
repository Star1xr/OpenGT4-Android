/* Disc-authored Setting panels across the complete car catalogue. This is
   an integration test, not an original GS draw-list oracle. */
#define main setting_menu_fixture_main
#include "setting_menu.c"
#undef main
#include "career_setting.h"
#include "career_parts.h"
static unsigned controls;
static int exercise(menu *m,int root,ui_widget *w){
 if(w->insensitive || !w->visible)return 1;
 if(w->slider_range_set && (!strcmp(w->type,"ScaleBar") || !strcmp(w->type,"SliderBar"))){
  int lo=w->slider_min,hi=w->slider_max;
  if(lo<hi){
   ah_obj *host=widget_module(m,w);CHECK(host);
   ah_set_member(m->vm,host,"index",ah_int(lo));m->focus[root]=w;
   menu_send_input(m,MENU_LEFT);CHECK(w->focus_index==lo);
   menu_send_input(m,MENU_RIGHT);CHECK(w->focus_index==lo+1);
   ah_set_member(m->vm,host,"index",ah_int(hi));
   menu_send_input(m,MENU_RIGHT);CHECK(w->focus_index==hi);
   menu_send_input(m,MENU_LEFT);CHECK(w->focus_index==hi-1);
   controls++;
  }
 }
 for(int i=0;i<w->nkids;i++)CHECK(exercise(m,root,w->kids[i]));return 1;
}
int main(int argc,char **argv){
 CHECK(argc==2 && gt4_game_load_disc(argv[1]));const char *why=NULL;
 gt4_vol *vol=gt4_vol_open(argv[1],&why);CHECK(vol);
 gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[1]);CHECK(factory);
 gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);gt4_game_set_auto_gears(gt4_career_vehicle_auto_gears,factory);
 gt4_game_set_stats_reader(gt4_career_vehicle_stats,factory);gt4_game_set_setting_reader(gt4_career_vehicle_setting,factory);
 gt4_save_init("");char error[256];menu *source=menu_open(vol,"gtmode",error,sizeof error);CHECK(source);menu_enter(source,0);ticks(source);
 menu *m=menu_open(vol,"setting",error,sizeof error);CHECK(m);ah_set_log(m->vm,log_script,NULL);menu_set_origin(m,"gtmode");
 size_t n;uint8_t *labels=gt4_vol_read(vol,"/specdb/GT4_US2560/GENERIC_CAR.idi",&n);CHECK(labels);
 unsigned count=car_u32(labels+4),panels=0;const char *limit=getenv("GT4_SETTING_CAR_LIMIT");if(limit && count>(unsigned)atoi(limit))count=(unsigned)atoi(limit);
 const char *names[]={"BrakeRoot","SuspensionRoot","LSDRoot","GearRoot","NosRoot","DriveTrainRoot","BallastRoot","AIDRoot","AIDRoot","AeroRoot"};
 for(unsigned i=0;i<count;i++){
  const char *label=gt4_specdb_label_for_id(labels,n,car_u32(labels+20+i*8));CHECK(label);
  ah_vm *vm=source->vm;ah_obj *car=ah_module_new(vm,"Car",NULL,NULL,NULL);CHECK(gt4_game_construct_car(vm,car,label,0,0));
  ah_val value=ah_objv(car),ret;CHECK(nat_bind_game_car(vm,NULL,source,1,&value,&ret)>=0);
  ah_obj *garage=get(vm,game_object(source),"garage").u.o,*cars=ah_array_new(vm);ah_array_push(vm,cars,value);
  ah_set_member(vm,garage,"cars",ah_objv(cars));ah_set_member(vm,garage,"riding_index",ah_int(0));
  ah_obj *owned=car;
  for(unsigned variant=0;variant<2;variant++){
  car=owned;
  if(variant){
   uint8_t initial[0x4b8];CHECK(gt4_game_car_body(vm,car,initial));char path[96];snprintf(path,sizeof path,"/specdb/GT4_US2560/CARS/%016llX",(unsigned long long)car_u32(initial));
   size_t size;uint8_t *parts=gt4_vol_read(vol,path,&size);CHECK(parts);const gt4_career_part_data *data=gt4_game_part_data();CHECK(data);
   /* Test-owned upgrades, not a change to shop ownership rules. */
   for(unsigned sheet=0;sheet<3;sheet++){
   ah_set_member(vm,car,"slot",ah_int(sheet));CHECK(ah_call_member(vm,car,"beginSetting",0,NULL,&ret)>=0);
   for(unsigned type=1;type<=31;type++)for(unsigned stage=1;stage<=data->type_max[type-1];stage++){
    if(gt4_career_part_key(parts,size,type,stage)==UINT64_MAX)continue;
    ah_val args[]={ah_int(type),ah_int(stage)};
    CHECK(ah_call_member(vm,car,"getParts",2,args,&ret)>=0);CHECK(ah_call_member(vm,car,"changeParts",2,args,&ret)>=0);
   }
   CHECK(ah_call_member(vm,car,"endSetting",0,NULL,&ret)>=0);
   }
   free(parts);
  }
  menu_save_capture(source);
  CHECK(menu_setting_begin(m));apply_pages(m);ticks(m);CHECK(!errors);
  car=get(m->vm,m->project,"current_car").u.o;CHECK(car);
  for(unsigned slot=0;slot<3;slot++){
   ah_set_member(m->vm,car,"slot",ah_int(slot));CHECK(ah_call_member(m->vm,car,"beginSetting",0,NULL,&ret)>=0);
   uint8_t body[0x4b8],rows[0x142],valid[0x142];CHECK(gt4_game_car_body(m->vm,car,body));
   CHECK(gt4_career_blob_get(m->vm,car,"_setting_parts_ps2",rows,sizeof rows));CHECK(gt4_career_blob_get(m->vm,car,"_setting_parts_valid_ps2",valid,sizeof valid));
   for(int group=0;group<GT4_SETTING_GROUPS;group++){
    int32_t range[42];gt4_setting_details details={0};
    /* An absent part has no panel in the original. Do not force an
       unavailable NOS/transmission into a fictitious range. */
    if(group==GT4_SETTING_GEAR){if(!gt4_career_vehicle_setting(factory,body,0,-1,&details))continue;}
    else if(gt4_setting_range(body,rows,valid,group,range)<0)continue;
    int root=page(m,names[group]);CHECK(root>=0);ticks(m);
    if(errors){fprintf(stderr,"catalog %s slot %u page %s\n",label,slot,names[group]);return 1;}
    uint8_t before[0x4b8],after[0x4b8];CHECK(gt4_game_car_body(m->vm,car,before));
    CHECK(exercise(m,root,ui_root_widget(m->ui,root)));
    CHECK(gt4_game_car_body(m->vm,car,after));if(getenv("GT4_SETTING_CATALOG_NEGATIVE"))after[0x15b]^=1;CHECK(!memcmp(before,after,sizeof before)); /* edits are pending until OK */
    CHECK(ah_call_member(m->vm,widget_module(m,ui_root_widget(m->ui,root)),"apply",0,NULL,&ret)>=0);CHECK(!errors);panels++;
   }
   /* Detailed engine graph was not included in the family-panel sweep.
      Opening it exercises MUnit torque conversion/labels as well as both
      normalized curves. It must work for every sheet and equipment set. */
   int graph=page(m,"EngineCurbePopup");CHECK(graph>=0);ticks(m);
   if(errors){fprintf(stderr,"catalog %s slot %u detailed engine graph\n",label,slot);return 1;}
   panels++;
  }
  CHECK(menu_setting_end(m));
  }
  if(i%50==0){printf("UI %u/%u %s\n",i+1,count,label);fflush(stdout);}
 }
 CHECK(controls>0);printf("PASS %u cars, %u authored panels in A/B/C, %u adjustable controls at both bounds; pending edits preserved until apply.\n",count,panels,controls);
 free(labels);menu_close(m);menu_close(source);gt4_save_shutdown();gt4_career_vehicle_close(factory);gt4_vol_close(vol);return 0;
}
