/* Integration boundary: original 00114498 -> real profile VM -> authored
   quick-event payment -> native save/reload. Race inputs are injected;
   this does not certify the full physical session or presentation. */
#define main result_fixture_main
#include "event_result_run.c"
#undef main
#undef CHECK
#define main pregrid_fixture_main
#include "../ui/pregrid_menu.c"
#undef main
static ah_val field(ah_vm *vm,ah_obj *obj,const char *name){ah_val v;if(!ah_get_member(vm,obj,name,&v)){fprintf(stderr,"missing field: %s\n",name);exit(1);}return v;}
int main(int argc,char **argv){
 CHECK(argc==3);ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[2],0x100000)>0x54f3f8);c.lookup=lookup;c.hook=boundary;
 w32(&c,0x622f4c,GAME);w32(&c,WORLD+0x6c,RACE);w32(&c,RACE+0x60,ENTRIES);w32(&c,ENTRIES+8,SLOTS);w32(&c,SLOTS,ENTRY);w32(&c,ENTRY+0x18,CAR);
 const char *error=NULL;gt4_vol *v=gt4_vol_open(argv[1],&error);CHECK(v && gt4_game_load_disc(argv[1]));
 char directory[200],message[256];snprintf(directory,sizeof directory,"build/career-native/event-cycle-%llu-%llu",(unsigned long long)time(NULL),(unsigned long long)clock());gt4_save_init(directory);
 menu *m=menu_open(v,"gtmode",message,sizeof message);CHECK(m);ah_set_log(m->vm,log_script,NULL);
 ah_obj *initial_stats=field(m->vm,game_object(m),"statistics").u.o,*initial_counts=ah_array_new(m->vm);
 ah_set_member(m->vm,initial_stats,"prize_money",ah_int(0));for(int i=0;i<7;i++)ah_array_push(m->vm,initial_counts,ah_int(0));ah_set_member(m->vm,initial_stats,"race_positions",ah_objv(initial_counts));
 uint8_t owned_body[0x4b8]={0};ah_obj *owned=ah_module_new(m->vm,"owned_fixture",NULL,NULL,NULL);CHECK(gt4_career_blob_set(m->vm,owned,"full_car_ps2",owned_body,sizeof owned_body));ah_set_member(m->vm,field(m->vm,game_object(m),"garage").u.o,"riding_car",ah_objv(owned));
 ah_obj *fixture_garage=field(m->vm,game_object(m),"garage").u.o;ah_obj *fixture_cars=ah_array_new(m->vm);ah_array_push(m->vm,fixture_cars,ah_objv(owned));ah_set_member(m->vm,fixture_garage,"cars",ah_objv(fixture_cars));ah_set_member(m->vm,fixture_garage,"riding_index",ah_int(0));
 ah_set_member(m->vm,owned,"car_code",ah_str(m->vm,"ka_01"));
 menu_save_capture(m);CHECK(gt4_save_create(1,"Event cycle validation"));
 menu *q=menu_open(v,"quick-event",message,sizeof message);CHECK(q);ah_set_log(q->vm,log_script,NULL);
 uint8_t previous=15;uint32_t award=0;unsigned cases=0;
 for(int mode=0;mode<4;mode++)for(int finished=0;finished<2;finished++)for(int pos=0;pos<7;pos++){
  ah_vm *vm=m->vm;ah_obj *game=game_object(m),*stats=field(vm,game,"statistics").u.o;
  ah_val labelv=ah_str(vm,"am_sunday_0001"),unitv;CHECK(ah_call_member(vm,field(vm,game,"race_record").u.o,"getUnit",1,&labelv,&unitv)>=0);
  memset(c.ram+GAME+0x348,0,0x70);uint64_t total=(uint64_t)field(vm,stats,"prize_money").u.i;memcpy(c.ram+GAME+0x348+0x48,&total,8);
  ah_val counts=field(vm,stats,"race_positions");for(int i=0;i<7;i++)w32(&c,GAME+0x348+0x54+4*i,(uint32_t)ah_num(ah_array_get(counts.u.o,i)));
  w32(&c,RECORD,(uint32_t)ah_num(field(vm,unitv.u.o,"original_record")));c.ram[BEST]=previous;w32(&c,QUICK+0x4a4,award);
  rank=pos;aspec=17+pos;lap=60000+pos;prize=600+100*pos;default_history=0;calendar_calls=0;c.ram[CAR+0x5b6]=(uint8_t)finished;w32(&c,WORLD+0xcc8,mode);
  memset(c.r,0,sizeof c.r);c.r[4].uw[0]=QUICK;c.r[5].uw[0]=WORLD;c.r[6].uw[0]=BEST;c.r[29].uw[0]=0x1ef0000;jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));recomp_lookup(0x114498)(&c);
  CHECK(menu_apply_career_result(m,"am_sunday_0001",finished,pos,lap,mode,aspec,prize,&previous,&award));
  uint8_t wear[32];for(unsigned i=0;i<32;i++)wear[i]=(uint8_t)(cases+i);CHECK(menu_apply_car_wear(m,wear));
  if(getenv("GT4_EVENT_CYCLE_NEGATIVE") && !cases)previous^=1;
  CHECK(previous==c.ram[BEST] && award==r32(&c,QUICK+0x4a4));CHECK((uint32_t)ah_num(field(vm,unitv.u.o,"original_record"))==r32(&c,RECORD));
  total=(uint64_t)field(vm,stats,"prize_money").u.i;CHECK(!memcmp(&total,c.ram+GAME+0x348+0x48,8));counts=field(vm,stats,"race_positions");for(int i=0;i<7;i++)CHECK((uint32_t)ah_num(ah_array_get(counts.u.o,i))==r32(&c,GAME+0x348+0x54+4*i));CHECK(calendar_calls==1);
  menu_save_refresh(q);menu_quick_event_context(q,"am_sunday_0001");menu_quick_event_result(q,1,award);
  ah_val page;CHECK(ah_call_member(q->vm,q->project,"getStartPage",0,NULL,&page)>=0);CHECK(page.t==AV_OBJ);
  if(cases==0 || cases==7){CHECK(menu_enter_quick_start(q));advance(q);CHECK(!errors);}
  ah_obj *face=ah_module_new(q->vm,"amount_fixture",NULL,NULL,NULL);ah_set_member(q->vm,q->project,"prprize",ah_objv(face));ah_set_member(q->vm,q->project,"prmoney",ah_objv(face));
  ah_obj *qgame=game_object(q);uint64_t balance=(uint64_t)field(q->vm,qgame,"money").u.i;
  ah_set_member(q->vm,q->project,"rest_prize",ah_int(award));ah_set_member(q->vm,q->project,"decline_unit",ah_int(10));ah_val result;
  CHECK(ah_call_member(q->vm,q->project,"makePrizeFast",0,NULL,&result)>=0);ah_val context=ah_objv(q->context);unsigned ticks=0;while(ah_num(field(q->vm,q->project,"rest_prize"))>0){CHECK(ah_call_member(q->vm,q->project,"onPrizeCheck",1,&context,&result)>=0);CHECK(++ticks<10000);}
  CHECK((uint64_t)field(q->vm,qgame,"money").u.i==balance+award);menu_save_capture(q);CHECK(gt4_save_store());CHECK(gt4_save_load_slot(1));menu_save_refresh(m);
  game=game_object(m);CHECK((uint64_t)field(vm,game,"money").u.i==balance+award);stats=field(vm,game,"statistics").u.o;CHECK((uint64_t)field(vm,stats,"prize_money").u.i==total);
  uint8_t restored[0x4b8];CHECK(gt4_game_car_body(vm,field(vm,field(vm,game,"garage").u.o,"riding_car").u.o,restored));CHECK(!memcmp(restored,owned_body,0x498) && !memcmp(restored+0x498,wear,32));CHECK(!errors);cases++;
 }
 CHECK(menu_apply_event_distances(m,123,456));CHECK(gt4_save_store());CHECK(gt4_save_load_slot(1));menu_save_refresh(m);ah_obj *final_stats=field(m->vm,game_object(m),"statistics").u.o;CHECK(field(m->vm,final_stats,"a_spec_distance").u.i==123 && field(m->vm,final_stats,"b_spec_distance").u.i==456);
 printf("PASS %u original result/profile/quick-event payment/save-reload integration cases; injected finish/rank boundary.\n",cases);menu_close(q);menu_close(m);gt4_save_shutdown();gt4_vol_close(v);ee_free(&c);return 0;
}
