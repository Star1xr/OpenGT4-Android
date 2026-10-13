/* Disc-wide championship/profile/authored-menu integration. Race finishes
   are injected; the complete physical session is outside this test. */
#define main pregrid_fixture_main
#include "../ui/pregrid_menu.c"
#undef main
#include "career_event.h"
static ah_val value(ah_vm *vm,ah_obj *obj,const char *name){ah_val v;CHECK(ah_get_member(vm,obj,name,&v));return v;}
static uint32_t word(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
static void pay(menu *q,uint32_t amount){
    ah_obj *face=ah_module_new(q->vm,"payment_fixture",NULL,NULL,NULL);
    ah_set_member(q->vm,q->project,"prprize",ah_objv(face));ah_set_member(q->vm,q->project,"prmoney",ah_objv(face));
    ah_set_member(q->vm,q->project,"rest_prize",ah_int(amount));ah_set_member(q->vm,q->project,"decline_unit",ah_int(10));
    ah_val ret,context=ah_objv(q->context);CHECK(ah_call_member(q->vm,q->project,"makePrizeFast",0,NULL,&ret)>=0);
    while(ah_num(value(q->vm,q->project,"rest_prize"))>0)CHECK(ah_call_member(q->vm,q->project,"onPrizeCheck",1,&context,&ret)>=0);
}
int main(int argc,char **argv){
 CHECK(argc==2);const char *error=NULL;gt4_vol *v=gt4_vol_open(argv[1],&error);CHECK(v && gt4_game_load_disc(argv[1]));
 char directory[180],msg[256];snprintf(directory,sizeof directory,"build/career-native/championship-cycle-%llu-%llu",(unsigned long long)time(NULL),(unsigned long long)clock());gt4_save_init(directory);
 menu_quick_race initial={0};initial.entries=6;initial.championship=1;menu_set_quick_race(&initial);
 menu *m=menu_open(v,"gtmode",msg,sizeof msg),*q=menu_open(v,"quick-championship",msg,sizeof msg);CHECK(m && q);
 ah_set_log(m->vm,log_script,NULL);ah_set_log(q->vm,log_script,NULL);menu_save_capture(m);CHECK(gt4_save_create(1,"Champ validation"));load_race_tables(m);
 unsigned series_cases=0,round_cases=0;const gt4_career_race_lookup_data *rules=gt4_game_race_lookup_data();
 for(unsigned at=0;at<word(m->race_labels+4);at++){
  const char *series=gt4_specdb_label_for_id(m->race_labels,m->race_labels_size,word(m->race_labels+20+at*8));CHECK(series);
  size_t n=strlen(series);if(n<4 || strcmp(series+n-2,"00") || (series[n-4]=='0' && series[n-3]=='0'))continue;
  char label[32];unsigned count;if(!gt4_career_championship_label(series,0,label,&count))continue;
  ah_val stack_ret,selection=ah_str(m->vm,series),number=ah_int(17);
  CHECK(ah_call_member(m->vm,game_object(m),"menuClearStack",0,NULL,&stack_ret)>=0);
  CHECK(ah_call_member(m->vm,game_object(m),"menuPush",1,&selection,&stack_ret)>=0);
  CHECK(ah_call_member(m->vm,game_object(m),"menuPush",1,&number,&stack_ret)>=0);
  CHECK(ah_call_member(m->vm,game_object(m),"loadMenuStack",0,NULL,&stack_ret)>=0 && !ah_truthy(stack_ret));
  CHECK(ah_call_member(m->vm,game_object(m),"menuRef",0,NULL,&stack_ret)>=0 && !strcmp(ah_cstr(stack_ret),"17"));
  CHECK(ah_call_member(m->vm,game_object(m),"saveMenuStack",0,NULL,&stack_ret)>=0 && stack_ret.t==AV_NIL);menu_save_capture(m);
  int32_t points[6]={0};uint64_t balance=(uint64_t)value(m->vm,game_object(m),"money").u.i;
  for(unsigned round=0;round<count;round++){
    CHECK(gt4_career_championship_label(series,round,label,&count));uint8_t row[151];
    CHECK(gt4_career_race_row(rules,m->race_labels,m->race_labels_size,m->race_db,m->race_db_size,label,row));
    uint8_t order[6]={0,1,2,3,4,5};gt4_career_championship_points(points,order,6,1,(const int8_t *)rules->driver_points);
    uint32_t award=0,grand=0;uint32_t prize=gt4_career_race_prize(row,0);int16_t units;memcpy(&units,row+0x2c,2);
    CHECK(menu_apply_championship_result(m,label,series,1,round+1==count,0,60000,0,20,prize,points,units,&award,&grand));
    CHECK(award==prize && grand==(round+1==count?(uint32_t)((int32_t)units*100):0));
    if(getenv("GT4_CHAMPIONSHIP_CYCLE_NEGATIVE") && !round_cases)award^=1;CHECK(award==prize);
    menu_quick_race work={0};work.entries=6;work.player=0;work.championship=1;work.session_number=(int)round;work.final_session=round+1==count;
    work.finished=1;work.prize=award;work.grand_prize=grand;snprintf(work.race,sizeof work.race,"%s",label);snprintf(work.course,sizeof work.course,"tsukuba");
    memcpy(work.total_points,points,sizeof points);for(unsigned i=0;i<6;i++)strcpy(work.grid[i].car,"ka_01");menu_set_quick_race(&work);
    menu_save_refresh(q);menu_quick_event_context(q,label);ah_val quick,ret;CHECK(nat_quick_work(q->vm,NULL,NULL,0,NULL,&quick)==0);
    CHECK(ah_num(value(q->vm,quick.u.o,"SessionNumber"))==round);CHECK(ah_truthy(value(q->vm,quick.u.o,"IsFinalSession"))==(round+1==count));
    for(unsigned i=0;i<6;i++){ah_val index=ah_int(i);CHECK(ah_call_member(q->vm,quick.u.o,"getPower",1,&index,&ret)>=0 && ah_num(ret)==points[i]);}
    pay(q,award);if(grand)pay(q,grand);balance+=award+grand;
    CHECK((uint64_t)value(q->vm,game_object(q),"money").u.i==balance);
    /* Original goNext's final payment branch consumes both work fields. */
    ah_obj *page=value(q->vm,q->project,"PrizeRoot").u.o;ah_set_member(q->vm,page,"grand_prize_mode",ah_int(1));
    ah_val context=ah_objv(q->context);CHECK(ah_call_member(q->vm,page,"goNext",1,&context,&ret)>=0);advance(q);
    CHECK(nat_quick_work(q->vm,NULL,NULL,0,NULL,&quick)==0);CHECK(!ah_num(value(q->vm,quick.u.o,"prize")) && !ah_num(value(q->vm,quick.u.o,"GrandPrize")));
    CHECK(ah_call_member(q->vm,q->project,"getStartPage",0,NULL,&ret)>=0);CHECK(ret.u.o==value(q->vm,q->project,"AfterRoot").u.o);
    uint8_t live[0x90]={0};uint64_t keys[6];int32_t paints[6];
    for(unsigned j=0;j<6;j++){keys[j]=(UINT64_C(0x26)<<32)|j;paints[j]=(int32_t)j;}
    gt4_career_championship_context(live,round+1<count,round+1,count-1,series,keys,paints,points);
    CHECK(menu_championship_context_set(q,live));
    menu_save_capture(q);CHECK(gt4_save_store() && gt4_save_load_slot(1));menu_save_refresh(m);
    uint8_t restored[0x90];CHECK(menu_championship_context_get(m,restored));
    if(getenv("GT4_CHAMPIONSHIP_RESUME_NEGATIVE") && !round_cases)restored[0x3c]^=1;
    CHECK(!memcmp(live+8,restored+8,0x88));
    ah_obj *profile=game_object(m);CHECK(ah_call_member(m->vm,profile,"isSuspended",0,NULL,&ret)>=0);
    CHECK(ah_truthy(ret)==(round+1<count));
    CHECK(ah_call_member(m->vm,profile,"loadMenuStack",0,NULL,&ret)>=0 && ah_truthy(ret));
    CHECK(ah_call_member(m->vm,profile,"menuRef",0,NULL,&ret)>=0);
    if(getenv("GT4_CHAMPIONSHIP_MENU_STACK_NEGATIVE") && !round_cases)ret=ah_str(m->vm,"18");
    CHECK(!strcmp(ah_cstr(ret),"17"));ah_val depth=ah_int(1);
    CHECK(ah_call_member(m->vm,profile,"menuRef",1,&depth,&ret)>=0 && !strcmp(ah_cstr(ret),series));
    if(!round_cases && round+1<count){
        ah_val args[2]={ah_objv(m->context),ah_int(1)};
        CHECK(ah_call_member(m->vm,m->project,"sureNoSuspended",2,args,&ret)>=0);
        advance(m);CHECK(menu_take_race_request(m));
        CHECK(!strcmp(ah_cstr(value(m->vm,profile,"next_sequence")),"championship"));
        CHECK(!strcmp(ah_cstr(value(m->vm,profile,"race_code")),series));CHECK(!errors);
    }
    if(round+1<count){CHECK(!strcmp(ah_cstr(value(m->vm,profile,"suspended_racecode")),series));CHECK(ah_num(value(m->vm,profile,"suspended_session"))==round+1);}
    else {CHECK(ah_call_member(m->vm,profile,"abandonRace",0,NULL,&ret)>=0);CHECK(menu_championship_context_get(m,restored));CHECK(!restored[0x28] && !memcmp(restored+8,live+8,0x20));}

    CHECK((uint64_t)value(m->vm,game_object(m),"money").u.i==balance);CHECK(!errors);round_cases++;
  }
  ah_val code=ah_str(m->vm,series),record;CHECK(ah_call_member(m->vm,value(m->vm,game_object(m),"race_record").u.o,"getUnit",1,&code,&record)>=0);
  CHECK(!strcmp(ah_cstr(value(m->vm,record.u.o,"present_result")),"1"));series_cases++;
 }
 CHECK(series_cases && round_cases);printf("PASS %u disc championships / %u rounds: points, series victory, round/grand payment, original goNext consumption, replay selector, suspension contexts and save/reload; injected race finish boundary.\n",series_cases,round_cases);
 menu_close(q);menu_close(m);gt4_save_shutdown();gt4_vol_close(v);return 0;
}
