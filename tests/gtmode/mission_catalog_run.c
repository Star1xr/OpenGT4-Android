/* C: all mission result-to-menu/save bridges, forcing outcomes at that
   boundary as FIDELITY rule 5 permits. This is not 34 physical victories;
   mission_cycle_run separately completes a race with controller inputs. */
#define main pregrid_fixture_main
#define plat_draw_quad4 pregrid_arcade_quad4
#include "../ui/pregrid_menu.c"
#undef plat_draw_quad4
#undef main
#include "career_license_disc.h"
#include <time.h>
void plat_draw_quad4(const plat_texture*t,float x,float y,float w,float h,float u,float v,float a,float b,const uint8_t c[4][4]){
 (void)t;(void)x;(void)y;(void)w;(void)h;(void)u;(void)v;(void)a;(void)b;(void)c;quads++;
}
static ah_val member_value(menu *m,ah_obj *o,const char *name){ah_val v;CHECK(ah_get_member(m->vm,o,name,&v));return v;}
static unsigned record_grade(menu *m,const char *code){
 ah_val key=ah_str(m->vm,code),unit;ah_obj *records=member_value(m,game_object(m),"race_record").u.o;
 CHECK(ah_call_member(m->vm,records,"getUnit",1,&key,&unit)>=0);
 return ((uint32_t)member_value(m,unit.u.o,"original_record").u.i>>4)&15;
}
int main(int argc,char **argv){
 CHECK(argc==2);const char *error=NULL;gt4_vol *vol=gt4_vol_open(argv[1],&error);CHECK(vol && gt4_game_load_disc(argv[1]));
 char directory[200],msg[256];snprintf(directory,sizeof directory,"build/mission-catalog-%llu-%llu",(unsigned long long)time(NULL),(unsigned long long)clock());
 gt4_save_init(directory);menu *career=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(career);ah_set_log(career->vm,log_script,NULL);
 menu_save_capture(career);CHECK(gt4_save_create(1,"Mission catalog"));
 gt4_career_race_lookup_data lookup;CHECK(gt4_career_race_lookup_data_load(argv[1],&lookup));
 unsigned cycles=0;uint64_t paid=0;int64_t initial_money=member_value(career,game_object(career),"money").u.i;
 for(unsigned n=1;n<=34;n++){
  char code[32];snprintf(code,sizeof code,"l0m%04u",n);gt4_career_license_exam exam;
  CHECK(gt4_career_mission_prepare(&exam,argv[1],&lookup,code,msg,sizeof msg));
  uint32_t winning_prize=gt4_career_race_prize(exam.row,0);CHECK(winning_prize>0);
  CHECK(record_grade(career,code)==15);
  /* Abort, losing finish, win, then loss after that win. Best record must
     survive; the original script pays only the supplied prize amount. */
  for(unsigned attempt=0;attempt<4;attempt++){
   int finished=attempt!=0,position=attempt==2?0:exam.desc.cars-1;
   uint32_t prize=finished?gt4_career_race_prize(exam.row,position):0,award=0;
   CHECK(menu_apply_mission_result(career,code,finished,position,60000+n,250,prize,&award));CHECK(award==prize);
   unsigned expected=attempt==0?15:attempt==1?(unsigned)position:0;
   if(getenv("GT4_MISSION_CATALOG_NEGATIVE") && n==1 && attempt==2)expected=1;
   CHECK(record_grade(career,code)==expected);
   menu_quick_race q={0};strcpy(q.race,code);strcpy(q.course,exam.desc.course);q.entries=exam.desc.cars;q.finished=1;q.prize=award;q.player=position;
   for(int i=0;i<q.entries;i++){strcpy(q.grid[i].car,"ka_01");q.grid[i].time=60000+n+i;}
   menu_set_quick_race(&q);menu *quick=menu_open(vol,"quick-mission",msg,sizeof msg);CHECK(quick);ah_set_log(quick->vm,log_script,NULL);
   CHECK(menu_enter_quick_start(quick));advance(quick);
   CHECK(!strcmp(ui_root_name(quick->ui,menu_active_root(quick)),prize?"PrizeRoot":"AfterRoot"));
   if(prize){
    /* prize_action.ad L27..35 / quick-mission.ad L562: the normal
       confirm key speeds up the authored credit counter. Large prizes
       cannot be assumed to finish after mission 9's ten-second wait. */
    menu_send_input(quick,MENU_ACCEPT);
    for(unsigned i=0;i<30;i++){
     if(member_value(quick,game_object(quick),"money").u.i==initial_money+(int64_t)(paid+prize) &&
        (!strcmp(menu_focus_name(quick),"OK") || !strcmp(ui_root_name(quick->ui,menu_active_root(quick)),"AfterRoot")))break;
     advance(quick);
    }
    if(strcmp(ui_root_name(quick->ui,menu_active_root(quick)),"AfterRoot")){menu_send_input(quick,MENU_ACCEPT);advance(quick);}
    if(strcmp(ui_root_name(quick->ui,menu_active_root(quick)),"AfterRoot"))fprintf(stderr,"mission %u attempt %u prize %u page %s focus %s\n",n,attempt,prize,ui_root_name(quick->ui,menu_active_root(quick)),menu_focus_name(quick));
    CHECK(!strcmp(ui_root_name(quick->ui,menu_active_root(quick)),"AfterRoot"));paid+=prize;
   }
   ui_draw_root(quick->ui,menu_active_root(quick),NULL,NULL);CHECK(!errors);
   CHECK(member_value(quick,game_object(quick),"money").u.i==initial_money+(int64_t)paid);
   menu_save_capture(quick);CHECK(gt4_save_store() && gt4_save_load_slot(1));menu_save_refresh(career);
   CHECK(record_grade(career,code)==expected);
   CHECK(member_value(career,game_object(career),"money").u.i==initial_money+(int64_t)paid);
   menu_close(quick);cycles++;
  }
 }
 for(unsigned n=1;n<=34;n++){char code[32];snprintf(code,sizeof code,"l0m%04u",n);CHECK(record_grade(career,code)==0);}
 printf("PASS 34 authored mission catalogs, %u forced outcome/menu/save cycles, credits %llu; abort/loss/win/repeated loss, best-record preservation. No physical wins asserted.\n",cycles,(unsigned long long)paid);
 menu_close(career);gt4_save_shutdown();gt4_vol_close(vol);return 0;
}
