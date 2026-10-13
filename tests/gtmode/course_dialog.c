/* Authored dated-course dialog, using the production menu lifecycle.
   Rendering and input timing are explicit headless test boundaries. */
#define main purchase_fixture_main
#include "purchase_menu.c"
#undef main

static unsigned confirmations;
static int acknowledge(menu *m,void *user)
{
    (void)user;
    if(strcmp(ui_root_name(m->ui,menu_active_root(m)),"GetCourseDialog"))return 0;
    confirmations++;ticks(m);menu_send_input(m,MENU_ACCEPT);ticks(m);return 1;
}

int main(int argc,char **argv)
{
    CHECK(argc==2);const char *error=NULL;gt4_vol *vol=gt4_vol_open(argv[1],&error);
    CHECK(vol && gt4_game_load_disc(argv[1]));char directory[180],msg[256];
    snprintf(directory,sizeof directory,"build/career-native/course-dialog-%llu-%llu",(unsigned long long)time(NULL),(unsigned long long)clock());
    gt4_save_init(directory);
    menu *m=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(m);
    menu_set_car_loader(m,transaction_model_loader,NULL);
    menu_save_capture(m);CHECK(gt4_save_create(1,"Course dialog validation"));menu_save_refresh(m);
    /* Profile already owns a car, as at the dated unlock. TopRoot's GT3/
       GT4p import notices are outside this dialog fixture. */
    gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[1]);CHECK(factory);
    gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);
    ah_val car_args[]={ah_str(m->vm,"ka_01"),ah_int(0)},result,index=ah_int(0);
    ah_obj *garage=get(m->vm,game_object(m),"garage").u.o;
    CHECK(ah_call_member(m->vm,garage,"addNewCar",2,car_args,&result)>=0);
    CHECK(ah_call_member(m->vm,garage,"setRidingCar",1,&index,&result)>=0);
    menu_save_capture(m);
    ah_vm *vm=m->vm;ah_set_log(vm,log_script,NULL);menu_set_event_presenter(m,acknowledge,NULL);
    CHECK(enter_named(m,"HomeRoot"));ticks(m);CHECK(!script_errors);
    ah_obj *page=get(vm,m->project,"GetCourseDialog").u.o;
    ah_obj *dialog=get(vm,page,"Dialog").u.o;
    CHECK(get(vm,page,"actor").t!=AV_NIL && get(vm,dialog,"actor").t!=AV_NIL);
    if(getenv("GT4_COURSE_DIALOG_NEGATIVE"))ah_remove_member(vm,dialog,"actor");
    ah_val args[]={ah_objv(m->context),ah_str(vm,"Course available"),ah_str(vm,"deepforest")},ret;
    CHECK(ah_call_member(vm,page,"open",3,args,&ret)>=0);
    CHECK(!script_errors && confirmations==1 && !m->event_depth);
    CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"HomeRoot"));
    ah_val text=get(vm,get(vm,dialog,"Message").u.o,"text");
    CHECK(text.t==AV_NIL || (ah_cstr(text) && !*ah_cstr(text)));
    puts("PASS original course dialog actors prepared before open_motion, modal acknowledgment, close and return; negative removes required actor.");
    menu_close(m);gt4_save_shutdown();gt4_game_set_car_factory(NULL,NULL);
    gt4_career_vehicle_close(factory);gt4_vol_close(vol);return 0;
}
