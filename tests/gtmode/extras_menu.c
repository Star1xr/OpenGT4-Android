/* Disc-authored secret commands and dated shop selectors. */
#define main purchase_fixture_main
#include "purchase_menu.c"
#undef main
static int date_at(menu *m,int days){
 int fields[3];CHECK(gt4_game_decode_calendar_day(2453462+days,fields));
 ah_obj *date=ah_array_new(m->vm);for(int i=0;i<3;i++)ah_array_push(m->vm,date,ah_int(fields[i]));
 ah_set_member(m->vm,get(m->vm,game_object(m),"calendar").u.o,"today",ah_objv(date));return 0;
}
static ah_val original_day;
static int no_cycle(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){(void)data;(void)argc;(void)argv;(void)self;return ah_call(vm,original_day,0,NULL,ret);}
int main(int argc,char **argv){
 CHECK(argc>=2);const char *error=NULL;gt4_vol *v=gt4_vol_open(argv[1],&error);CHECK(v && gt4_game_load_disc(argv[1]));gt4_save_init("");
 char message[256];menu *m=menu_open(v,"gtmode",message,sizeof message);CHECK(m);ah_vm *vm=m->vm;ah_val ret;
 ah_obj *calendar=get(vm,game_object(m),"calendar").u.o;
 for(int day=0;day<1401;day++){
  date_at(m,day);ah_val modulus=ah_int(100);CHECK(ah_call_member(vm,calendar,"getElapsedWeek",1,&modulus,&ret)>0);CHECK(ah_num(ret)==day/7%100);
 }

 gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[1]);CHECK(factory);gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);
 ah_obj *used=get(vm,m->project,"UsedCarRoot").u.o;CHECK(used);ah_set_member(vm,used,"current_region",ah_str(vm,"JP"));
 const int boundary_days[]={0,6,7,692,693,699,700,1393,1400};
 for(unsigned trial=0;trial<sizeof boundary_days/sizeof *boundary_days;trial++){
  int day=boundary_days[trial];if(m->depth)finalize_page(m,m->pages[m->depth-1]);date_at(m,day);menu_save_capture(m);m->depth=0;
  for(int r=0;r<ui_root_count(m->ui);r++)if(!strcmp(ui_root_name(m->ui,r),"UsedCarRoot"))menu_enter(m,r);ticks(m);
  ah_obj *stock=get(vm,get(vm,used,"UsedCarModule").u.o,"UsedCarData").u.o;CHECK(stock);
  char filename[64];snprintf(filename,sizeof filename,"usedcar/us_used%04d",day/7%100);ah_val arg=ah_str(vm,filename);ah_obj *reference=ah_module_new(vm,"reference",NULL,NULL,NULL);CHECK(nat_project_load(vm,reference,m,1,&arg,&ret)==0);reference=get(vm,reference,"UsedCarData").u.o;if(argc>2 && !strcmp(argv[2],"--stock-negative"))ah_set_member(vm,reference,"usedcar_00_carlist",ah_objv(ah_array_new(vm)));
  int black=0;for(int region=0;region<3;region++){char key[64];snprintf(key,sizeof key,"usedcar_%02d_carlist",region);ah_val actual=get(vm,stock,key),expected=get(vm,reference,key);if(ah_array_size(actual.u.o)!=ah_array_size(expected.u.o))fprintf(stderr,"used day%d region%d count%d/%d\n",day,region,ah_array_size(actual.u.o),ah_array_size(expected.u.o));CHECK(ah_array_size(actual.u.o)==ah_array_size(expected.u.o));for(int i=0;i<ah_array_size(actual.u.o);i++){const char *label=ah_cstr(ah_array_get(actual.u.o,i));CHECK(!strcmp(label,ah_cstr(ah_array_get(expected.u.o,i))));black+=!strcmp(label,"r390_race_98") || !strcmp(label,"gt_one_race_exxon_99") || !strcmp(label,"_787b_91") || !strcmp(label,"r92cp_92");}}
  CHECK(black==(day/7%100==99?4:0));
 }
 ah_obj *wheel=get(vm,m->project,"WheelShopRoot").u.o;CHECK(wheel);
 CHECK(ah_call_member(vm,wheel,"getUsedWheelPeriod",0,NULL,&ret)>=0);int period=ah_num(ret);CHECK(period>1);ah_set_member(vm,wheel,"period",ah_int(period));ah_set_member(vm,wheel,"current_wheel_maker",ah_str(vm,"used"));
 ah_obj *data=get(vm,m->project,"WheelData").u.o;CHECK(data);
 original_day=get(vm,calendar,"getElapsedDate");if(argc>2 && !strcmp(argv[2],"--wheel-negative"))ah_set_member(vm,calendar,"getElapsedDate",ah_bind(vm,calendar,ah_native(vm,"getElapsedDate",no_cycle,NULL)));
 for(int day=0;day<period*2;day++){
  date_at(m,day);CHECK(ah_call_member(vm,wheel,"getWheelList",0,NULL,&ret)>=0);char name[64];snprintf(name,sizeof name,"used%02d_carlist",day%period);ah_val expected=get(vm,data,name);if(ret.t!=expected.t || ret.u.o!=expected.u.o)fprintf(stderr,"wheel day=%d period=%d ret=%d/%p expected=%d/%p sizes=%d/%d\n",day,period,ret.t,ret.u.o,expected.t,expected.u.o,ah_array_size(ret.u.o),ah_array_size(expected.u.o));CHECK(ret.t==expected.t && ret.u.o==expected.u.o);
 }
 ah_obj *top=get(vm,m->project,"TopRoot").u.o;CHECK(top);ah_val commands=get(vm,top,"craft_command");CHECK(ah_as_obj(commands,AO_ARRAY));int count=ah_array_size(commands.u.o);CHECK(count==16);
 date_at(m,0);menu_save_capture(m);m->depth=0;menu_enter(m,0);ticks(m);ah_set_log(vm,log_script,NULL);script_errors=0;
 for(int pass=0;pass<3;pass++){
  date_at(m,pass==0?365:366);ah_set_member(vm,game_object(m),"money",ah_int(123));
  for(int i=0;i<count;i++){int key=ah_num(ah_array_get(commands.u.o,i));if(pass==1 && i==count-1)key=0;menu_send_key(m,key,0);}
  CHECK(get(vm,game_object(m),"money").u.i==(pass==2?10000123:123));
 }

 /* Focused target is explicit; execute the authored code and calendar update. */
 const char *pages[]={"LicenseDetailRoot","EventCourseRoot","LicenseRoot"};
 for(int kind=0;kind<3;kind++){
  ah_obj *page=get(vm,m->project,pages[kind]).u.o;CHECK(page);ui_widget *root=NULL;
  for(int r=0;r<ui_root_count(m->ui);r++)if(!strcmp(ui_root_name(m->ui,r),pages[kind]))root=ui_root_widget(m->ui,r);CHECK(root);
  ui_widget *target=root->kids[0];if(kind==1)target=ui_widget_child(ui_widget_child(ui_widget_child(root,"RaceSelect"),"ScrollBox"),"SelectBox");CHECK(target);m->depth=1;m->pages[0]=root->root;const char *target_name=target->name;m->focus[root->root]=target;
  const char *code=kind==1?"am_sunday_0001":"l0b0001";if(kind==2){target->name="l0b";}
  ah_set_member(vm,widget_module(m,target),kind==1?"racecode":"license",ah_str(vm,code));
  ah_obj *handler=page;
  if(kind==1){handler=get(vm,handler,"RaceSelect").u.o;handler=get(vm,handler,"ScrollBox").u.o;handler=get(vm,handler,"SelectBox").u.o;}
  ah_val sequence=get(vm,handler,"craft_command");CHECK(ah_as_obj(sequence,AO_ARRAY));
  ah_set_member(vm,m->project,"craft_index",ah_int(0));ah_obj *event=ah_module_new(vm,"event",NULL,NULL,NULL);ah_val args[]={ah_objv(m->context),ah_objv(event)};
  date_at(m,366);
  for(int i=0;i<ah_array_size(sequence.u.o);i++){
   ah_set_member(vm,event,"keysym",ah_array_get(sequence.u.o,i));menu_send_key(m,(int)ah_num(ah_array_get(sequence.u.o,i)),0);
  }
  ah_val codearg=ah_str(vm,kind==2?"l0b0002":code),unit;CHECK(ah_call_member(vm,get(vm,game_object(m),"race_record").u.o,"getUnit",1,&codearg,&unit)>=0);
  fprintf(stderr,"kind%d code%s result=%s sequence=%d\n",kind,code,ah_cstr(get(vm,unit.u.o,"result")),ah_array_size(sequence.u.o));CHECK(!strcmp(ah_cstr(get(vm,unit.u.o,"result")),kind==1?"1":kind==2?"red":"gold"));target->name=target_name;
 }

 char directory[240],imports[320],source_dir[400],path[480];snprintf(directory,sizeof directory,"build/legacy-inherit-%llu-%llu",(unsigned long long)time(NULL),(unsigned long long)clock());gt4_save_init(directory);menu_save_capture(m);CHECK(gt4_save_create(1,"Inheritance test"));menu_save_refresh(m);
 snprintf(imports,sizeof imports,"%s/imports",directory);CHECK(gt4_save_mkdir(imports));snprintf(source_dir,sizeof source_dir,"%s/BASCUS-97102GAMEDATA",imports);CHECK(gt4_save_mkdir(source_dir));snprintf(path,sizeof path,"%s/BASCUS-97102GAMEDATA",source_dir);
 ah_obj *manager=ah_module_new(vm,"inherit manager",NULL,NULL,NULL);ah_val mode=ah_str(vm,"MODE_OPERATION");CHECK(nat_pc_manager(vm,manager,m,1,&mode,&ret)>=0);
 CHECK(ah_call_member(vm,manager,"isExistGT3Data",0,NULL,&ret)>=0 && !strcmp(ah_cstr(ret),"FILE_NOT_FOUND"));
 m->depth=0;for(int r=0;r<ui_root_count(m->ui);r++)if(!strcmp(ui_root_name(m->ui,r),"StatusRoot"))menu_enter(m,r);ticks(m);
 uint8_t source[8192];memset(source,255,sizeof source);uint64_t raw=~UINT64_C(150000);memcpy(source+0x50,&raw,8);CHECK(plat_file_write(path,source,sizeof source));
 ah_set_member(vm,game_object(m),"money",ah_int(123));
 confirm_choice=1;menu_set_dialog_presenter(m,purchase_dialog);ah_val inherit_args[]={ah_objv(m->context),ah_objv(manager),ah_int(0)};CHECK(ah_call_member(vm,get(vm,m->project,"StatusRoot").u.o,"inheritDataCheckGT3",3,inherit_args,&ret)==1);if(argc>2 && !strcmp(argv[2],"--inherit-negative"))ah_set_member(vm,game_object(m),"money",ah_int(100124));CHECK(ah_num(get(vm,game_object(m),"money"))==100123);
 CHECK(ah_call_member(vm,manager,"inheritGT3Data",0,NULL,&ret)>=0 && !strcmp(ah_cstr(ret),"OK"));CHECK(ah_num(get(vm,game_object(m),"money"))==100123);
 CHECK(ah_call_member(vm,get(vm,m->project,"StatusRoot").u.o,"inheritDataCheckGT4p",3,inherit_args,&ret)==1);
 for(int group=0;group<2;group++)for(int i=1;i<=16;i++){char label[32];snprintf(label,sizeof label,"l0%c%04d",group?'a':'b',i);ah_val code=ah_str(vm,label),unit;CHECK(ah_call_member(vm,get(vm,game_object(m),"race_record").u.o,"getUnit",1,&code,&unit)>=0);CHECK(gt4_career_record_meets_grade((uint8_t)ah_num(get(vm,unit.u.o,"original_record")),12));}
 CHECK(gt4_save_load_slot(1));menu_save_refresh(m);CHECK(ah_num(get(vm,game_object(m),"money"))==100123 && ah_truthy(get(vm,game_object(m),"withdrawn")) && ah_truthy(get(vm,game_object(m),"withdrawnGT4p")));
 ah_set_member(vm,game_object(m),"withdrawnGT4p",ah_int(0));CHECK(plat_file_write(path,source,0x80));CHECK(ah_call_member(vm,manager,"inheritGT4pData",0,NULL,&ret)>=0 && !strcmp(ah_cstr(ret),"DEVICE_ERROR"));CHECK(!ah_truthy(get(vm,game_object(m),"withdrawnGT4p")));
 /* Invalid source data must not grant credits or mark an import complete. */
 ah_set_member(vm,game_object(m),"withdrawn",ah_int(0));memset(source,255,sizeof source);CHECK(plat_file_write(path,source,sizeof source));
 CHECK(ah_call_member(vm,manager,"inheritGT3Data",0,NULL,&ret)==1 && !strcmp(ah_cstr(ret),"NO_DATA"));CHECK(!ah_truthy(get(vm,game_object(m),"withdrawn")) && ah_num(get(vm,game_object(m),"money"))==100123);
 source[0x170]=0;CHECK(plat_file_write(path,source,sizeof source));CHECK(ah_call_member(vm,manager,"inheritGT4pData",0,NULL,&ret)==1 && !strcmp(ah_cstr(ret),"NO_DATA"));CHECK(!ah_truthy(get(vm,game_object(m),"withdrawnGT4p")));
 memset(source+0x70,0,8);CHECK(plat_file_write(path,source,sizeof source));CHECK(ah_call_member(vm,manager,"inheritGT4pData",0,NULL,&ret)==1 && !strcmp(ah_cstr(ret),"DEVICE_ERROR"));CHECK(!ah_truthy(get(vm,game_object(m),"withdrawnGT4p")));
 CHECK(!script_errors);printf("PASS local US GT3 credits cap/one-time/reload and B/A inheritance, missing/truncated files; synthetic source, no real source save\n");
 CHECK(!script_errors);
 if(argc>2 && !strcmp(argv[2],"--secret-negative")){ah_val code=ah_str(vm,"l0b0001"),unit;CHECK(ah_call_member(vm,get(vm,game_object(m),"race_record").u.o,"getUnit",1,&code,&unit)>=0);ah_set_member(vm,unit.u.o,"original_record",ah_int(0));CHECK(!strcmp(ah_cstr(get(vm,unit.u.o,"result")),"gold"));}
 printf("PASS 1401 calendar days, nine weekly stock boundaries including black cars, %d dated wheel lists, original money command/day gate/wrong command, license gold/category and event records\n",period*2);
 menu_close(m);gt4_game_set_car_factory(NULL,NULL);gt4_career_vehicle_close(factory);gt4_vol_close(v);gt4_save_shutdown();return 0;
}
