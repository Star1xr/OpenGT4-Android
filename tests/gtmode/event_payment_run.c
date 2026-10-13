/* Reuse the native menu harness's platform boundaries. This test executes
   the disc's authored prize callback and compares each credit increment
   with the complete original 0043AFB0, not a replacement payment formula. */
#define main award_consumption_fixture
#include "award_menu.c"
#undef main
#include "ee.h"
ee_fn recomp_lookup(uint32_t);
#define PAYMENT_GAME 0x1800000u
static uint64_t read_balance(ee_ctx *c){return gt4_career_money_get(c->ram+PAYMENT_GAME);}
static int increment_original(ee_ctx *c,int64_t amount){
 memset(c->r,0,sizeof c->r);c->r[4].uw[0]=PAYMENT_GAME;c->r[5].sd[0]=amount;c->r[29].uw[0]=0x1ef0000;
 jmp_buf jump;c->exit_jmp=&jump;if(setjmp(jump)){fprintf(stderr,"original payment: %s\n",c->end_detail);return 0;}
 recomp_lookup(0x43afb0)(c);return 1;
}
int main(int argc,char **argv){
 CHECK(argc==3 || argc==4);ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[2],0x100000)>0x54f3f8);c.lookup=recomp_lookup;
 const char *error=NULL;gt4_vol *v=gt4_vol_open(argv[1],&error);CHECK(v && gt4_game_load_disc(argv[1]));
 char directory[180];snprintf(directory,sizeof directory,"build/career-native/event-payment-%llu-%llu",(unsigned long long)time(NULL),(unsigned long long)clock());
 gt4_save_init(directory);char message[256];menu *career=menu_open(v,"gtmode",message,sizeof message);CHECK(career);
 menu_save_capture(career);CHECK(gt4_save_create(1,"Event payment validation"));
 const char *project=argc==4?argv[3]:"quick-event";
 CHECK(!strcmp(project,"quick-event") || !strcmp(project,"quick-championship"));
 menu *m=menu_open(v,project,message,sizeof message);CHECK(m);
 menu_save_refresh(m);
 ah_vm *vm=m->vm;ah_set_log(vm,log_script,NULL);ah_obj *game=game_object(m);
 ah_obj *amount_face=ah_module_new(vm,"amount",NULL,NULL,NULL),*money_face=ah_module_new(vm,"balance",NULL,NULL,NULL);
 ah_set_member(vm,m->project,"prprize",ah_objv(amount_face));ah_set_member(vm,m->project,"prmoney",ah_objv(money_face));
 static const uint64_t balances[]={0,5939,999999999,UINT64_C(0x7fffffffffffffff),UINT64_C(0xfffffffffffffff0),UINT64_MAX};
 static const int64_t prizes[]={1,9,10,11,99,100,101,600,1000,1001,30000,2147483647};
 unsigned cases=0,steps=0;
 for(unsigned b=0;b<sizeof balances/sizeof *balances;b++)for(unsigned p=0;p<sizeof prizes/sizeof *prizes;p++)for(int fast=0;fast<2;fast++){
  /* Large awards use the actual authored fast selector to avoid millions
     of callback ticks. Small awards exercise decimal decline to one. */
  if(!fast && (prizes[p]>30000 || prizes[p]<10))continue;
  int64_t balance;memcpy(&balance,&balances[b],8);ah_set_member(vm,game,"money",ah_int(balance));ah_set_member(vm,game,"need_save",ah_int(0));
  gt4_career_money_set(c.ram+PAYMENT_GAME,balances[b]);
  ah_set_member(vm,m->project,"rest_prize",ah_int(prizes[p]));ah_set_member(vm,m->project,"decline_unit",ah_int(10));
  ah_val result;if(fast)CHECK(ah_call_member(vm,m->project,"makePrizeFast",0,NULL,&result)==1);
  unsigned ticks=0;
  while(ah_num(get(vm,m->project,"rest_prize"))>0){
   ah_val unit=get(vm,m->project,"decline_unit");CHECK(unit.t==AV_INT || unit.t==AV_LONG);CHECK(unit.u.i>0);
   ah_val context=ah_objv(m->context);CHECK(ah_call_member(vm,m->project,"onPrizeCheck",1,&context,&result)==1);
   CHECK(increment_original(&c,unit.u.i));
   ah_val actual=get(vm,game,"money");CHECK(actual.t==AV_INT || actual.t==AV_LONG);
   uint64_t native=(uint64_t)actual.u.i;if(getenv("GT4_CAREER_EVENT_PAYMENT_NEGATIVE") && !steps)native^=1;
   CHECK(native==read_balance(&c));CHECK(ah_truthy(get(vm,game,"need_save")));CHECK(!script_errors);
   steps++;CHECK(++ticks<10000);
  }
  CHECK(ah_num(get(vm,m->project,"rest_prize"))==0);cases++;
 }
 uint64_t expected=(uint64_t)get(vm,game,"money").u.i;
 CHECK(gt4_save_capture(vm,game,1));CHECK(gt4_save_store());CHECK(gt4_save_load_slot(1));
 ah_set_member(vm,game,"money",ah_int(0));menu_save_refresh(m);
 CHECK((uint64_t)get(vm,game,"money").u.i==expected);
 printf("PASS original authored %s prize payment: %u balance/prize/speed cases, %u callback increments against 0043AFB0, including 64-bit wrap, dirty-save flag and native file reload. Presentation/completion flow is outside this proof.\n",project,cases,steps);
 menu_close(m);menu_close(career);gt4_save_shutdown();gt4_vol_close(v);ee_free(&c);return 0;
}
