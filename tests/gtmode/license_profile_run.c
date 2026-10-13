/* C: persistent licence records/calendar/save, injected physical results.
   The examination judgement has its separate original EE oracle. */
#define main pregrid_fixture_main
#define plat_draw_rotated fixture_draw_rotated
#include "../ui/pregrid_menu.c"
#undef plat_draw_rotated
#undef main
static unsigned cursor_draws;
void plat_draw_rotated(const plat_texture*t,float x,float y,float w,float h,float cx,float cy,float a,const uint8_t c[4]){
 if(w==32 && h==32)cursor_draws++;
 fixture_draw_rotated(t,x,y,w,h,cx,cy,a,c);
}
static ah_val field(ah_vm *vm,ah_obj *obj,const char *name){ah_val value;CHECK(ah_get_member(vm,obj,name,&value));return value;}
int main(int argc,char **argv){
 CHECK(argc==2);gt4_vol *v=gt4_vol_open(argv[1],NULL);CHECK(v && gt4_game_load_disc(argv[1]));
 char directory[200],error[256];snprintf(directory,sizeof directory,"build/career-native/license-profile-%llu-%llu",(unsigned long long)time(NULL),(unsigned long long)clock());
 gt4_save_init(directory);menu *m=menu_open(v,"gtmode",error,sizeof error);CHECK(m);ah_set_log(m->vm,log_script,NULL);
 menu_save_capture(m);CHECK(gt4_save_create(1,"Licence profile"));
 const char *groups[]={"l0b","l0a","lib","lia","l0s"};unsigned attempts=0;
 for(unsigned g=0;g<5;g++){
  ah_val category=ah_str(m->vm,groups[g]),passed;
  CHECK(ah_call_member(m->vm,field(m->vm,game_object(m),"race_record").u.o,"isLicensePassed",1,&category,&passed)>=0 && !ah_truthy(passed));
  for(unsigned n=1;n<=16;n++){
   char code[32];snprintf(code,sizeof code,"%s%04u",groups[g],n);gt4_career_license_attempts visit;
   CHECK(gt4_game_license_begin(m->vm,game_object(m),code,&visit));
   CHECK(gt4_game_license_attempt(m->vm,game_object(m),code,&visit,-1,0x157529ffu,0));CHECK(visit.best_grade==13);
   CHECK(gt4_game_license_attempt(m->vm,game_object(m),code,&visit,4,60000,0x060a07eau));
   CHECK(gt4_game_license_attempt(m->vm,game_object(m),code,&visit,3,59000,0x060a07eau));
   CHECK(gt4_game_license_attempt(m->vm,game_object(m),code,&visit,2,58000,0x060a07eau));
   CHECK(gt4_game_license_attempt(m->vm,game_object(m),code,&visit,4,61000,0x060a07eau));attempts+=5;
   CHECK(visit.best_grade==9 && visit.best_time==58000);
   CHECK(gt4_game_license_leave(m->vm,game_object(m),code,&visit));
   menu_save_capture(m);int stored=gt4_save_store();
   if(!stored)fprintf(stderr,"save %s: %s\n",code,gt4_save_error());CHECK(stored);
   int loaded=gt4_save_load_slot(1);if(!loaded)fprintf(stderr,"reload %s: %s\n",code,gt4_save_error());CHECK(loaded);menu_save_refresh(m);
   ah_val record,value,arg=ah_str(m->vm,code);
   CHECK(ah_call_member(m->vm,field(m->vm,game_object(m),"license_record").u.o,"getUnit",1,&arg,&record)>=0);
   uint32_t best=gt4_game_license_best(m->vm,game_object(m),code);
   if(getenv("GT4_LICENSE_PROFILE_NEGATIVE") && !g && n==1)best++;
   CHECK(best==58000);
   CHECK(ah_call_member(m->vm,record.u.o,"getRankNum",0,NULL,&value)>=0 && ah_num(value)==4);
   CHECK(ah_call_member(m->vm,record.u.o,"getBestTime",0,NULL,&value)>=0 && !strcmp(ah_cstr(value),"0'58.000"));
   CHECK(ah_call_member(m->vm,record.u.o,"getResult",0,NULL,&value)>=0 && !strcmp(ah_cstr(value),"gold"));
   CHECK(ah_call_member(m->vm,record.u.o,"getDate",0,NULL,&value)>=0 && !strcmp(ah_cstr(value),"2026/10/6"));
   if(n==1){
    /* gtmode.ad L8227/L8394: cached return must initialize and hide the veil. */
    int detail=-1;for(int i=0;i<ui_root_count(m->ui);i++)if(!strcmp(ui_root_name(m->ui,i),"LicenseDetailRoot"))detail=i;
    CHECK(detail>=0);menu_enter(m,detail);advance(m);
    ui_widget *veil=child(child(ui_root_widget(m->ui,detail),"Pane"),"Veil");CHECK(veil);
    /* Execute the disc's real outgoing callback (L8394), rather than only
       imitating its veil: the hidden context must not survive the return. */
    CHECK(m->focus[detail]);menu_send_input(m,MENU_ACCEPT);advance(m);
    CHECK(veil->visible && !ah_truthy(field(m->vm,m->context,"cursor_visible")));
    cursor_draws=0;ui_draw_root(m->ui,detail,NULL,NULL);CHECK(!cursor_draws);
    if(!getenv("GT4_LICENSE_PROFILE_RETURN_NEGATIVE"))menu_license_return_prepare(m);
    if(getenv("GT4_LICENSE_CURSOR_RETURN_NEGATIVE"))ah_set_member(m->vm,m->context,"cursor_visible",ah_int(0));
    menu_enter(m,detail);advance(m);CHECK(!veil->visible && !errors);
    CHECK(ah_truthy(field(m->vm,m->context,"cursor_visible")));
    cursor_draws=0;ui_draw_root(m->ui,detail,NULL,NULL);CHECK(cursor_draws==1);
    ui_widget *before=m->focus[detail];menu_send_input(m,MENU_RIGHT);advance(m);
    CHECK(m->focus[detail] && m->focus[detail]!=before);
    cursor_draws=0;ui_draw_root(m->ui,detail,NULL,NULL);CHECK(cursor_draws==1);
   }
  }
  category=ah_str(m->vm,groups[g]);CHECK(ah_call_member(m->vm,field(m->vm,game_object(m),"race_record").u.o,"isLicensePerfect",1,&category,&passed)>=0 && ah_truthy(passed));
 }
 /* 00432040: capacity ten, ties follow earlier attempts, discard slow tails. */
 gt4_career_license_attempts visit;CHECK(gt4_game_license_begin(m->vm,game_object(m),"l0b0001",&visit));
 CHECK(gt4_game_license_attempt(m->vm,game_object(m),"l0b0001",&visit,4,58000,0x070a07eau));
 ah_val record,value,arg=ah_str(m->vm,"l0b0001"),index=ah_int(1);
 CHECK(ah_call_member(m->vm,field(m->vm,game_object(m),"license_record").u.o,"getUnit",1,&arg,&record)>=0);
 CHECK(ah_call_member(m->vm,record.u.o,"getResult",1,&index,&value)>=0 && !strcmp(ah_cstr(value),"bronze"));
 CHECK(ah_call_member(m->vm,record.u.o,"getDate",1,&index,&value)>=0 && !strcmp(ah_cstr(value),"2026/10/7"));
 for(unsigned j=0;j<12;j++)CHECK(gt4_game_license_attempt(m->vm,game_object(m),"l0b0001",&visit,4,60000-j*50,0x080a07eau));
 menu_save_capture(m);CHECK(gt4_save_store() && gt4_save_load_slot(1));menu_save_refresh(m);
 CHECK(ah_call_member(m->vm,field(m->vm,game_object(m),"license_record").u.o,"getUnit",1,&arg,&record)>=0);
 CHECK(ah_call_member(m->vm,record.u.o,"getRankNum",0,NULL,&value)>=0 && ah_num(value)==10);
 ah_obj *entries=field(m->vm,record.u.o,"entries").u.o;
 const uint32_t expected[]={58000,58000,59000,59450,59500,59550,59600,59650,59700,59750};
 for(int i=0;i<10;i++)CHECK((uint32_t)ah_num(field(m->vm,ah_array_get(entries,i).u.o,"time"))==expected[i]);
 CHECK(gt4_game_license_best(m->vm,game_object(m),"l0b0001")==58000);
 CHECK(ah_num(field(m->vm,field(m->vm,game_object(m),"calendar").u.o,"elapsed_days"))==80);
 CHECK(!errors);printf("PASS %u injected visit attempts, 80 medal/calendar saves; 13 extra ranking insertions/ties/capacity (zero tolerance).\n",attempts);
 menu_close(m);gt4_save_shutdown();gt4_vol_close(v);return 0;
}
