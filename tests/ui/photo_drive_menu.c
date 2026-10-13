/* The actual authored PhotoDrive button, disc rules and equipped-car bridge. */
#define main purchase_fixture_main
#include "../gtmode/purchase_menu.c"
#undef main
int main(int argc,char **argv){
 CHECK(argc>=2);gt4_save_init("");gt4_vol *v=gt4_vol_open(argv[1],NULL);CHECK(v && gt4_game_load_disc(argv[1]));
 gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[1]);CHECK(factory);gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);
 char msg[256];menu *m=menu_open(v,"gtmode",msg,sizeof msg);CHECK(m);menu_enter(m,0);ticks(m);ah_vm *vm=m->vm;ah_obj *game=game_object(m);
 ah_obj *garage=get(vm,game,"garage").u.o;ah_val buy[]={ah_str(vm,"ka_01"),ah_int(2)},ret,index=ah_int(0);
 CHECK(ah_call_member(vm,garage,"addNewCar",2,buy,&ret)==1);CHECK(ah_call_member(vm,garage,"setRidingCar",1,&index,&ret)==1);
 uint8_t expected[0x4b8],body[0x4b8];CHECK(menu_riding_car_body(m,expected));
 ah_set_member(vm,game,"course_code",ah_str(vm,"tsukuba"));
 ah_obj *page=get(vm,m->project,"CourseEntryRoot").u.o;CHECK(page);
 for(int i=0;i<ui_root_count(m->ui);i++)if(!strcmp(ui_root_name(m->ui,i),"CourseEntryRoot"))ah_set_member(vm,page,"name",ah_str(vm,ui_root_name(m->ui,i)));
 ah_obj *info=ah_module_new(vm,"Photo Drive info",NULL,NULL,NULL);
 ah_set_member(vm,info,"race_code",ah_str(vm,"photo_drive"));ah_set_member(vm,info,"entrance_fee",ah_int(0));
 ah_val infoarg=ah_objv(info);CHECK(ah_call_member(vm,page,"setPhotoDriveInfo",1,&infoarg,&ret)>=0);
 ah_obj *button=get(vm,get(vm,get(vm,page,"Buttons").u.o,"Unit").u.o,"PhotoDrive").u.o;CHECK(button);
 ah_val args[]={ah_objv(m->context),ah_nil(),ah_nil()};CHECK(ah_call_member(vm,button,"onActivate",3,args,&ret)>=0);
 ticks(m);CHECK(menu_take_race_request(m));int route=menu_race_is_free(m);if(argc>2)route=1;CHECK(route==3);
 CHECK(!strcmp(ah_cstr(get(vm,game,"race_code")),"photo_drive"));
 CHECK(!strcmp(ah_cstr(get(vm,game,"major_menu_project")),"gtmode"));
 CHECK(!strcmp(ah_cstr(get(vm,game,"major_menu_page")),"CourseEntryRoot"));
 char course[32],car[96];int paint,laps;CHECK(menu_free_request(m,course,car,&paint,body,&laps));
 CHECK(!strcmp(course,"tsukuba") && !strcmp(car,"ka_01") && paint==2 && laps==0 && !memcmp(body,expected,sizeof body));
 uint8_t row[151],reference[151];CHECK(menu_photo_drive_rules(m,row));
 CHECK(gt4_career_race_row(gt4_game_race_lookup_data(),m->race_labels,m->race_labels_size,m->race_db,m->race_db_size,"photo_drive",reference));
 CHECK(!memcmp(row,reference,151) && row[0x3e]==1 && row[0x43]==0);
 ah_set_member(vm,game,"next_sequence",ah_str(vm,"freerun"));CHECK(!menu_photo_drive_rules(m,row));
 puts("PASS authored PhotoDrive entry, GT return page, single-car unlimited disc rules and equipped garage body/paint");
 menu_close(m);gt4_game_set_car_factory(NULL,NULL);gt4_career_vehicle_close(factory);gt4_vol_close(v);gt4_save_shutdown();return 0;
}
