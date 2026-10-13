/* Full original championship result/points functions; data getters and
   result drawing are explicit boundaries, never replacement rule logic. */
#define main event_result_fixture
#include "event_result_run.c"
#undef main
#include "career_event.h"
#include "career_blob.h"
#include "gtmode_game.h"
#define SESSION 0x1860000u
#define SERIES_RECORD 0x1861000u
#define ROW 0x1862000u
static void championship_record(ee_ctx *c){c->r[2].uw[0]=c->r[5].uw[0]==WORLD+0x130?SERIES_RECORD:RECORD;}
static void signed_prize(ee_ctx *c){c->r[2].sd[0]=(int32_t)prize;}
static int grid_time_oracle;
static uint8_t transmission_types[3];
static int setting_buffer_oracle;
static int equipped_stage_oracle;
static int default_parts_oracle,default_exists,default_missing;
static uint64_t default_muffler;
#define DEFAULT_PARTS_KEY UINT64_C(0x2400000123)
static int default_fixture_lookup(void *ctx,uint64_t key,uint8_t *out,size_t cap){
 (void)ctx;if(key==DEFAULT_PARTS_KEY){if(!default_exists)return 0;CHECK(cap>=0xc0);memset(out,0,0xc0);memcpy(out+0x98,&default_muffler,8);return 0xc0;}
 CHECK(cap>=8);uint64_t next_key=default_missing?UINT64_MAX:DEFAULT_PARTS_KEY;memcpy(out,&next_key,8);return 8;
}
static void default_exists_hook(ee_ctx *c){c->r[2].ud[0]=(uint64_t)default_exists;}
static int default_fixture_bridge(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
 (void)self;(void)data;CHECK(argc==1);uint8_t row[256];int n=default_fixture_lookup(NULL,(uint64_t)argv[0].u.i,row,sizeof row);*ret=ah_nil();
 if(n){ah_obj *bytes=ah_array_new(vm);for(int j=0;j<n;j++)ah_array_push(vm,bytes,ah_int(row[j]));*ret=ah_objv(bytes);}return 0;
}
static uint64_t install_fixture_key=UINT64_MAX;
static int missing_part_key(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
 (void)vm;(void)self;(void)data;(void)argv;CHECK(argc==2);*ret=ah_int((int64_t)install_fixture_key);return 0;
}
static const unsigned stage_offsets[]={2,2,0xe,2,2,0,0xa,4,0,0xe,0x22,0,8,8,6,6,6,6,6,6,2,2,2,2,0x3a,0x38,4,6,0,0};
static uint8_t stage_seed;
static int stage_fixture_lookup(void *ctx,uint64_t key,uint8_t *out,size_t cap){
 (void)ctx;unsigned type=(unsigned)(key>>32);if(type<1 || type>30)return -1;
 unsigned size=stage_offsets[type-1]+1;CHECK(size<=cap);
 for(unsigned j=0;j<size;j++)out[j]=(uint8_t)(stage_seed+(uint32_t)key*13+j*7);return (int)size;
}
static int stage_fixture_bridge(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
 (void)self;(void)data;CHECK(argc==1);uint8_t row[256];int size=stage_fixture_lookup(NULL,(uint64_t)argv[0].u.i,row,sizeof row);CHECK(size>0);
 ah_obj *bytes=ah_array_new(vm);for(int j=0;j<size;j++)ah_array_push(vm,bytes,ah_int(row[j]));*ret=ah_objv(bytes);return 0;
}
static uint8_t setting_rows[11][0x70];
static unsigned setting_sizes[]={0xc,0x2d,0x16,0x20,0x14,0x70,0x1c,0xa,0x1a,6,8};
static unsigned setting_missing;
static int setting_fixture_lookup(void *ctx,uint64_t key,uint8_t *out,size_t cap){
 (void)ctx;unsigned i=(unsigned)key;if(i>=11 || (setting_missing&(1u<<i)))return 0;
 CHECK(setting_sizes[i]<=cap);memcpy(out,setting_rows[i],setting_sizes[i]);return (int)setting_sizes[i];
}
static int setting_fixture_bridge(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
 (void)self;(void)data;CHECK(argc==1);unsigned i=(unsigned)argv[0].u.i;*ret=ah_nil();
 if(i>=11 || (setting_missing&(1u<<i)))return 0;
 ah_obj *bytes=ah_array_new(vm);for(unsigned j=0;j<setting_sizes[i];j++)ah_array_push(vm,bytes,ah_int(setting_rows[i][j]));
 *ret=ah_objv(bytes);return 0;
}
static void transmission_row(ee_ctx *c){
 if(default_parts_oracle){default_fixture_lookup(NULL,c->r[5].ud[0],c->ram+c->r[6].uw[0],256);return;}
 if(equipped_stage_oracle){stage_fixture_lookup(NULL,c->r[5].ud[0],c->ram+c->r[6].uw[0],256);return;}
 if(setting_buffer_oracle){setting_fixture_lookup(NULL,c->r[5].ud[0],c->ram+c->r[6].uw[0],0x142);return;}
 c->ram[c->r[6].uw[0]+0x3a]=transmission_types[c->r[5].ud[0]%3];
}
static int transmission_bridge(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
 (void)vm;(void)self;(void)data;CHECK(argc==1);*ret=ah_int(transmission_types[(uint64_t)argv[0].u.i%3]);return 0;
}
static void metadata_label(ee_ctx *c){c->r[2].uw[0]=0x1867000;}
static ee_fn championship_boundary(uint32_t pc){
    if(pc==0x443f40)return default_exists_hook;
    if(pc==0x443ed0)return transmission_row;
    if(pc==0x114390 && grid_time_oracle)return NULL;
    if(pc==0x4472a0 || pc==0x1867100 || pc==0x445450)return metadata_label;
    if(pc==0x447ca0 || pc==0x110498 || pc==0x445450 || pc==0x445ae8)return noop;
    if(pc==0x430d20)return championship_record;
    if(pc==0x4475a8)return signed_prize;
    if(pc==0x11a168 || pc==0x11a2d8 || pc==0x3bb440 || pc==0x4472c0)return noop;
    return boundary(pc);
}
static ee_fn championship_lookup(uint32_t pc){ee_fn f=championship_boundary(pc);return f?f:recomp_lookup(pc);}
int main(int argc,char **argv){
 CHECK(argc==2);ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0x54f3f8);
 c.lookup=championship_lookup;c.hook=championship_boundary;
 w32(&c,0x622f4c,GAME);w32(&c,SESSION+0x6c,RACE);w32(&c,RACE+0x60,ENTRIES);
 w32(&c,RACE+0x70,ROW);w32(&c,ENTRIES+8,SLOTS);w32(&c,SLOTS,ENTRY);w32(&c,ENTRY+0x18,CAR);
 w32(&c,ENTRIES,6);uint32_t seed=0x20261004;
 for(unsigned test=0;test<8192;test++){
  gt4_career_championship_result native;memset(&native,0xa7,sizeof native);
  for(unsigned i=0;i<sizeof native.result.statistics;i++)native.result.statistics[i]=(uint8_t)(next(&seed)>>24);
  native.result.record=next(&seed);native.series_record=next(&seed);
  int32_t points[6];uint8_t order[6];int8_t awards[6];
  memcpy(awards,c.ram+0x6a6ef0,6);
  for(unsigned i=0;i<6;i++){points[i]=(int32_t)next(&seed);order[i]=(uint8_t)i;}
  for(unsigned i=5;i;i--){unsigned j=next(&seed)%(i+1);uint8_t tmp=order[i];order[i]=order[j];order[j]=tmp;}
  int finished=test%2,final_session=(test/2)%2,mode=(test/4)%4;
  rank=(int32_t)((test/16)%6);aspec=(int32_t)(next(&seed)%1024)-256;lap=next(&seed);prize=next(&seed);
  if(test%8==0)points[0]=2147483647; /* Strict victory. */
  if(test%8==1)for(unsigned i=0;i<6;i++)points[i]=0; /* Ties. */
  w32(&c,WORLD+0x170,final_session?2:0);w32(&c,WORLD+0x174,2);
  memcpy(c.ram+GAME+0x348,native.result.statistics,sizeof native.result.statistics);
  w32(&c,RECORD,native.result.record);w32(&c,SERIES_RECORD,native.series_record);
  c.ram[CAR+0x5b6]=(uint8_t)finished;w32(&c,SESSION+0xcc8,mode);
  int16_t units=(int16_t)next(&seed);memcpy(c.ram+ROW+0x2c,&units,2);
  native.result.awarded_prize=next(&seed);w32(&c,QUICK+0x4a4,native.result.awarded_prize);
  w32(&c,QUICK+0x4a8,0);w32(&c,QUICK+0x4ac,0);
  w32(&c,ENTRIES+0x14,ENTRY);
  for(unsigned i=0;i<6;i++){
    uint32_t node=ENTRY+i*0x40;w32(&c,node+0x14,order[i]);w32(&c,node+0x10,i==5?0:node+0x40);
    w32(&c,WORLD+0x2f8+i*0x188,(uint32_t)points[i]);
  }
  calendar_calls=0;default_history=0;
  memset(c.r,0,sizeof c.r);c.r[4].uw[0]=QUICK;c.r[5].uw[0]=WORLD;c.r[6].uw[0]=SESSION;c.r[29].uw[0]=0x1ef0000;
  jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));recomp_lookup(0x119cb8)(&c);
  gt4_career_championship_points(points,order,6,finished,awards);
  gt4_career_process_championship_result(&native,finished,final_session,rank,lap,mode,aspec,prize,points,units);
  if(getenv("GT4_CHAMPIONSHIP_NEGATIVE") && !test)native.result.statistics[0x54]^=1;
  if(memcmp(native.result.statistics,c.ram+GAME+0x348,sizeof native.result.statistics)){fprintf(stderr,"champ test=%u finished=%d final=%d rank=%d\n",test,finished,final_session,rank);for(unsigned j=0;j<sizeof native.result.statistics;j++)if(native.result.statistics[j]!=c.ram[GAME+0x348+j])fprintf(stderr,"stat+%x native=%02x original=%02x\n",j,native.result.statistics[j],c.ram[GAME+0x348+j]);return 1;}
  CHECK(native.result.record==r32(&c,RECORD));CHECK(native.series_record==r32(&c,SERIES_RECORD));
  CHECK(native.result.awarded_prize==r32(&c,QUICK+0x4a4));
  if(final_session){CHECK(native.grand_prize==r32(&c,QUICK+0x4a8));CHECK(native.series_rank==r32(&c,QUICK+0x4ac));}
  CHECK(calendar_calls==1 && native.result.calendar_grade==calendar[0] && native.result.calendar_method==calendar[1] && native.result.calendar_time==calendar[2]);
  for(unsigned i=0;i<6;i++)CHECK((uint32_t)points[i]==r32(&c,WORLD+0x2f8+i*0x188));
 }
 for(unsigned total=1;total<=99;total++){
  char series[32],label[32];snprintf(series,sizeof series,"fixture_%02u00",total);memcpy(c.ram+0x1863000,series,strlen(series)+1);
  memset(c.ram+WORLD,0,0xab0);memset(c.r,0,sizeof c.r);c.r[4].uw[0]=WORLD;c.r[5].uw[0]=0x1863000;c.r[29].uw[0]=0x1ef0000;
  jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));recomp_lookup(0x3ef0e8)(&c);
  CHECK(r32(&c,WORLD+0x174)==total-1 && r32(&c,WORLD+0x170)==0);
  for(unsigned round=0;round<total;round++){
   unsigned n;CHECK(gt4_career_championship_label(series,round,label,&n) && n==total);
   if(getenv("GT4_CHAMPIONSHIP_LABEL_NEGATIVE") && total==1)label[strlen(label)-1]^=1;
   CHECK(!strcmp(label,(const char *)c.ram+WORLD+0x150));
   if(round+1<total){memset(c.r,0,sizeof c.r);c.r[4].uw[0]=WORLD;c.r[29].uw[0]=0x1ef0000;CHECK(!setjmp(jump));recomp_lookup(0x3ef088)(&c);}
  }
 }
 for(unsigned test=0;test<4096;test++){
  uint8_t context[0x90];uint64_t keys[6];int32_t paints[6],totals[6];
  for(unsigned j=0;j<sizeof context;j++)context[j]=(uint8_t)next(&seed);
  memcpy(c.ram+GAME+0x38bf8,context,sizeof context);
  char series[33];unsigned length=test%33;
  for(unsigned j=0;j<length;j++)series[j]=(char)('a'+next(&seed)%26);
  series[length]=0;memcpy(c.ram+WORLD+0x130,series,length+1);
  uint32_t round=next(&seed),last=next(&seed);int active=test%3==0?0:(test%3==1?1:257);
  w32(&c,WORLD+0x170,round);w32(&c,WORLD+0x174,last);
  for(unsigned j=0;j<6;j++){
   uint32_t driver=WORLD+0x180+j*0x188;
   uint64_t owned=((uint64_t)next(&seed)<<32)|next(&seed),prepared=((uint64_t)next(&seed)<<32)|next(&seed);
   memcpy(c.ram+driver,&owned,8);memcpy(c.ram+driver+8,&prepared,8);
   keys[j]=j?prepared:owned;c.ram[driver+0x18]=(uint8_t)next(&seed);
   paints[j]=(int8_t)c.ram[driver+0x18];totals[j]=(int32_t)next(&seed);w32(&c,driver+0x178,(uint32_t)totals[j]);
  }
  memset(c.r,0,sizeof c.r);c.r[4].uw[0]=WORLD;c.r[5].uw[0]=active;c.r[29].uw[0]=0x1ef0000;
  jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));recomp_lookup(0x119bd8)(&c);
  gt4_career_championship_context(context,active,round,last,series,keys,paints,totals);
  if(getenv("GT4_CHAMPIONSHIP_CONTEXT_NEGATIVE") && !test)context[0x28]^=1;
  CHECK(!memcmp(context,c.ram+GAME+0x38bf8,sizeof context));
 }
 puts("PASS 4096 original championship suspension context blocks (0x90 live bytes), including clear-only flag, signed paints and truncated labels; resume/frontend integration outside this proof.");
 ah_vm *bridge_vm=ah_vm_new();CHECK(bridge_vm);ah_obj *bridge_game=ah_module_new(bridge_vm,"game",ah_root(bridge_vm),NULL,NULL);
 ah_set_member(bridge_vm,ah_root(bridge_vm),"game",ah_objv(bridge_game));
 ah_obj *bridge_champ=ah_module_new(bridge_vm,"championship",NULL,NULL,NULL);ah_set_member(bridge_vm,bridge_game,"championship",ah_objv(bridge_champ));
 for(unsigned test=0;test<8192;test++){
  gt4_career_menu_stack native;for(unsigned j=0;j<sizeof native;j++)((uint8_t *)&native)[j]=(uint8_t)next(&seed);
  native.length=0;native.saved_length=-1;native.entries[0]=native.saved[0]=0;
  for(unsigned j=0;j<test%40;j++)gt4_career_menu_push(&native,j%4?"menu_selection_fixture":"");
  if((test/6)%2)gt4_career_menu_save(&native);
  if((test/6)%3==0)native.saved_length=-1-(int32_t)(next(&seed)%32);
  memcpy(c.ram+WORLD+0xa68,&native,sizeof native);
  CHECK(gt4_career_blob_set(bridge_vm,bridge_game,"_menu_stack_ps2",(uint8_t *)&native,sizeof native));
  CHECK(gt4_career_blob_set(bridge_vm,bridge_champ,"menu_stack_ps2",(uint8_t *)&native.saved_length,260));
  char text[260];unsigned n=(test/6)%260;memset(text,'x',n);text[n]=0;memcpy(c.ram+0x1863000,text,n+1);
  uint32_t routine[]={0x42fb48,0x42fd00,0x42fd78,0x42fdf0,0x42fe88,0x42ff48};unsigned operation=test%6;
  int32_t depth=(int32_t)((test/6)%48)-3;
  memset(c.r,0,sizeof c.r);c.r[4].uw[0]=WORLD;c.r[5].uw[0]=operation==1?0x1863000:(uint32_t)depth;c.r[29].uw[0]=0x1ef0000;
  jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));recomp_lookup(routine[operation])(&c);
  static const char *operations[]={"menuClearStack","menuPush","menuPop","menuRef","saveMenuStack","loadMenuStack"};
  ah_val bridge_arg=operation==1?ah_str(bridge_vm,text):ah_int(depth),bridge_ret;
  CHECK(gt4_game_menu_stack_action(bridge_vm,bridge_game,operations[operation],operation==1 || operation==3,&bridge_arg,&bridge_ret)==0);
  gt4_career_menu_stack bridge_state;CHECK(gt4_career_blob_get(bridge_vm,bridge_game,"_menu_stack_ps2",(uint8_t *)&bridge_state,sizeof bridge_state));
  if(getenv("GT4_MENU_STACK_BRIDGE_NEGATIVE") && !test)bridge_state.entries[0]^=1;
  CHECK(!memcmp(&bridge_state,c.ram+WORLD+0xa68,sizeof bridge_state));
  if(operation==0 || operation==4){uint8_t saved[260];CHECK(gt4_career_blob_get(bridge_vm,bridge_champ,"menu_stack_ps2",saved,260));CHECK(!memcmp(saved,c.ram+WORLD+0xbac,260));}
  if(operation==2 || operation==3){CHECK((ah_cstr(bridge_ret)!=NULL)==(c.r[2].uw[0]!=0));if(ah_cstr(bridge_ret))CHECK(!strcmp(ah_cstr(bridge_ret),(const char *)c.ram+c.r[2].uw[0]));}
  if(operation==5)CHECK(ah_truthy(bridge_ret)==(c.r[2].uw[0]!=0));
  const char *text_result=NULL;int loaded=0;
  if(operation==0)gt4_career_menu_clear(&native);
  if(operation==1)gt4_career_menu_push(&native,text);
  if(operation==2)text_result=gt4_career_menu_pop(&native);
  if(operation==3)text_result=gt4_career_menu_ref(&native,depth);
  if(operation==4)gt4_career_menu_save(&native);
  if(operation==5)loaded=gt4_career_menu_load(&native);
  if(getenv("GT4_MENU_STACK_NEGATIVE") && !test)native.entries[0]^=1;
  CHECK(!memcmp(&native,c.ram+WORLD+0xa68,sizeof native));
  if(operation==5)CHECK((uint32_t)loaded==c.r[2].uw[0]);
  if(operation==2 || operation==3){CHECK((text_result!=NULL)==(c.r[2].uw[0]!=0));if(text_result)CHECK(!strcmp(text_result,(const char *)c.ram+c.r[2].uw[0]));}
  ah_gc(bridge_vm);
 }
 /* Owned car slot property must modify exactly the original sw word.
    Random bodies exercise all surrounding bytes and signed/wide values. */
 ah_obj *setting_car=ah_module_new(bridge_vm,"SettingCar",NULL,NULL,NULL);ah_pin(bridge_vm,ah_objv(setting_car));
 gt4_game_bind_car(bridge_vm,setting_car);
 for(unsigned test=0;test<4096;test++){
  uint8_t body[0x4b8],actual[0x4b8];for(unsigned j=0;j<sizeof body;j++)body[j]=(uint8_t)(next(&seed)>>24);
  uint64_t value=((uint64_t)next(&seed)<<32)|next(&seed);if(test<3)value=test;
  CHECK(gt4_career_blob_set(bridge_vm,setting_car,"full_car_ps2",body,sizeof body));
  CHECK(gt4_career_blob_set(bridge_vm,setting_car,"full_car_ps2",body,sizeof body));
  memcpy(c.ram+0x18a0008,body,sizeof body);memset(c.r,0,sizeof c.r);c.r[4].uw[0]=0x18a0000;c.r[5].ud[0]=value;
  recomp_lookup(0x43e2e8)(&c);ah_set_member(bridge_vm,setting_car,"slot",ah_int((int64_t)value));
  CHECK(gt4_game_car_body(bridge_vm,setting_car,actual));
  if(getenv("GT4_SETTING_SLOT_NEGATIVE") && !test)actual[0x488]^=1;
  CHECK(!memcmp(actual,c.ram+0x18a0008,sizeof actual));
  ah_val slot;CHECK(ah_get_member(bridge_vm,setting_car,"slot",&slot) && (uint32_t)slot.u.i==r32(&c,0x18a0490));
  if(test<3){uint8_t setup[0x178];memcpy(body+test*0x178,"fixture!",8);
   memcpy(body+0x488,&test,4);CHECK(gt4_career_blob_set(bridge_vm,setting_car,"full_car_ps2",body,sizeof body));
   CHECK(gt4_game_car_body(bridge_vm,setting_car,actual) && gt4_career_active_setup(actual,setup));
   CHECK(!memcmp(setup,body+test*0x178,0x178));
  }
  ah_gc(bridge_vm);
 }
 ah_set_member(bridge_vm,setting_car,"_getTransmissionType",ah_native(bridge_vm,"transmission fixture",transmission_bridge,NULL));
 for(unsigned test=0;test<4096;test++){
  uint8_t body[0x4b8],actual[0x4b8];int32_t types[3];
  for(unsigned j=0;j<sizeof body;j++)body[j]=(uint8_t)(next(&seed)>>24);
  for(unsigned j=0;j<3;j++){
   uint64_t key=j;transmission_types[j]=(uint8_t)((test+j)%256);
   if((test+j)%17==0)key=UINT64_MAX;
   memcpy(body+j*0x178+0x60,&key,8);types[j]=key==UINT64_MAX?-1:transmission_types[j];
  }
  memcpy(c.ram+0x18a0008,body,sizeof body);
  CHECK(gt4_career_blob_set(bridge_vm,setting_car,"full_car_ps2",body,sizeof body));
  memset(c.r,0,sizeof c.r);c.r[4].uw[0]=0x18a0000;c.r[29].uw[0]=0x1ef0000;
  jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));recomp_lookup(0x441350)(&c);
  gt4_career_end_setting(body,types);CHECK(!memcmp(body,c.ram+0x18a0008,sizeof body));
  ah_val fn,ret;CHECK(ah_get_member(bridge_vm,setting_car,"endSetting",&fn));CHECK(ah_call(bridge_vm,fn,0,NULL,&ret)==0 && ret.t==AV_NIL);
  CHECK(gt4_game_car_body(bridge_vm,setting_car,actual));
  if(getenv("GT4_END_SETTING_NEGATIVE") && !test)actual[0xf0]^=1;
  CHECK(!memcmp(actual,c.ram+0x18a0008,sizeof actual));ah_gc(bridge_vm);
 }
 puts("PASS 4096 original endSetting bodies and native script calls: three sheets, every byte transmission type, missing key, full body preservation; SpecDB lookup is explicit boundary.");
 setting_buffer_oracle=1;
 ah_set_member(bridge_vm,setting_car,"_getSettingPartRow",ah_native(bridge_vm,"setting row fixture",setting_fixture_bridge,NULL));
 for(unsigned test=0;test<4096;test++){
  static const unsigned keys[]={0x28,0x48,0x40,0x80,0xb0,0x50,0x58,0xe8,0xe0,0x100,0xf8};
  uint8_t body[0x4b8],buffer[0x142],actual[0x142];unsigned slot=test%3;
  for(unsigned j=0;j<sizeof body;j++)body[j]=(uint8_t)(next(&seed)>>24);
  for(unsigned j=0;j<sizeof buffer;j++)buffer[j]=(uint8_t)(next(&seed)>>24);
  memcpy(body+0x488,&slot,4);setting_missing=next(&seed)&0x7ff;
  for(unsigned i=0;i<11;i++){
   uint64_t key=i;memcpy(body+slot*0x178+keys[i],&key,8);
   for(unsigned j=0;j<setting_sizes[i];j++)setting_rows[i][j]=(uint8_t)(next(&seed)>>24);
  }
  memcpy(c.ram+0x18a0008,body,sizeof body);memcpy(c.ram+0x18a1000,buffer,sizeof buffer);
  CHECK(gt4_career_blob_set(bridge_vm,setting_car,"full_car_ps2",body,sizeof body));
  CHECK(gt4_career_blob_set(bridge_vm,setting_car,"_setting_parts_ps2",buffer,sizeof buffer));
  memset(c.r,0,sizeof c.r);c.r[4].uw[0]=0x18a0000;c.r[5].uw[0]=0x18a1000;c.r[29].uw[0]=0x1ef0000;
  jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));recomp_lookup(0x4401c0)(&c);
  CHECK(gt4_career_prepare_setting(body,buffer,sizeof buffer,setting_fixture_lookup,NULL));
  CHECK(!memcmp(buffer,c.ram+0x18a1000,sizeof buffer));
  ah_val fn,ret;CHECK(ah_get_member(bridge_vm,setting_car,"setupPartsBuffer",&fn));CHECK(ah_call(bridge_vm,fn,0,NULL,&ret)==0 && ret.t==AV_NIL);
  CHECK(gt4_career_blob_get(bridge_vm,setting_car,"_setting_parts_ps2",actual,sizeof actual));
  if(getenv("GT4_SETTING_BUFFER_NEGATIVE") && !test)actual[0]^=1;
  CHECK(!memcmp(actual,c.ram+0x18a1000,sizeof actual));
  uint8_t unchanged[0x4b8];CHECK(gt4_game_car_body(bridge_vm,setting_car,unchanged));CHECK(!memcmp(body,unchanged,sizeof body));
  ah_gc(bridge_vm);
 }
 setting_buffer_oracle=0;
 puts("PASS 4096 original setting part buffers and native script calls: eleven row sources/offsets, A/B/C selection, missing rows preserve bytes; database rows are explicit boundary.");
 for(unsigned test=0;test<4096;test++){
  uint8_t wrapper[0x190];for(unsigned j=0;j<sizeof wrapper;j++)wrapper[j]=(uint8_t)(next(&seed)>>24);
  memcpy(c.ram+0x18a2000,wrapper,sizeof wrapper);memset(c.r,0,sizeof c.r);c.r[4].uw[0]=0x18a2018;
  recomp_lookup(0x449d58)(&c);c.r[4].uw[0]=0x18a2000;recomp_lookup(0x146628)(&c);
  gt4_career_setting_wrapper_reset(wrapper,r32(&c,0x18a2028));
  if(getenv("GT4_SETTING_WRAPPER_NEGATIVE") && !test)wrapper[0x180]^=1;
  CHECK(!memcmp(wrapper,c.ram+0x18a2000,sizeof wrapper));
 }
 puts("PASS 4096 original setting wrapper resets: car cache and six auxiliary words, complete untouched-byte preservation; base class and owned car construction outside proof.");
 gt4_career_part_data part_rules={0};memcpy(part_rules.normal_flags,c.ram+0x6a7c08,32);gt4_game_set_part_data(&part_rules);
 equipped_stage_oracle=1;
 ah_set_member(bridge_vm,setting_car,"_getSettingPartRow",ah_native(bridge_vm,"stage fixture",stage_fixture_bridge,NULL));
 for(unsigned test=0;test<8192;test++){
  static const int keys[]={0x20,0x28,0x50,0xe0,0xe8,-1,0x80,0x78,-1,0x40,0x48,-1,0xa8,0xb0,0x88,0x90,0x98,0xa0,0xd8,0xd0,0xc0,0xb8,0xc8,0x58,0x60,0x68,0xf8,0x108,-1,-1};
  uint8_t body[0x4b8];for(unsigned j=0;j<sizeof body;j++)body[j]=(uint8_t)(next(&seed)>>24);
  unsigned slot=test%3;memcpy(body+0x488,&slot,4);stage_seed=(uint8_t)next(&seed);
  for(unsigned sheet=0;sheet<3;sheet++)for(unsigned i=0;i<30;i++)if(keys[i]>=0){
   uint64_t key=((uint64_t)(i+1)<<32)|(next(&seed)&65535u);if((test+i)%4==0)key=UINT64_MAX;
   memcpy(body+sheet*0x178+keys[i],&key,8);
  }
  int32_t type=test<4096?(int32_t)(test%37)-2:(int32_t)next(&seed);
  memcpy(c.ram+0x18a0008,body,sizeof body);CHECK(gt4_career_blob_set(bridge_vm,setting_car,"full_car_ps2",body,sizeof body));
  memset(c.r,0,sizeof c.r);c.r[4].uw[0]=0x18a0000;c.r[5].sd[0]=type;c.r[29].uw[0]=0x1ef0000;
  jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));recomp_lookup(0x43f4c8)(&c);
  int32_t stage;CHECK(gt4_career_equipped_stage(body,type,part_rules.normal_flags,stage_fixture_lookup,NULL,&stage));
  CHECK((uint32_t)stage==c.r[2].uw[0]);
  ah_val arg=ah_int((int64_t)type),fn,ret;CHECK(ah_get_member(bridge_vm,setting_car,"getEquipedParts",&fn));CHECK(ah_call(bridge_vm,fn,1,&arg,&ret)==0);
  if(getenv("GT4_EQUIPPED_STAGE_NEGATIVE") && !test)ret.u.i^=1;
  CHECK((uint32_t)ret.u.i==c.r[2].uw[0]);
  const char *names[]={"isRemovableType","isRemovedNormalType"};uint32_t routines[]={0x44a250,0x44a288};
  for(unsigned i=0;i<2;i++){
   memset(c.r,0,sizeof c.r);c.r[4].sd[0]=type;recomp_lookup(routines[i])(&c);
   CHECK(ah_get_member(bridge_vm,setting_car,names[i],&fn));CHECK(ah_call(bridge_vm,fn,1,&arg,&ret)==0);
   if(getenv("GT4_PART_PREDICATE_NEGATIVE") && !test && !i)ret.u.i^=1;
   CHECK((uint32_t)ret.u.i==c.r[2].uw[0]);
  }ah_gc(bridge_vm);
 }
 equipped_stage_oracle=0;gt4_game_set_part_data(NULL);
 puts("PASS 8192 equipped-stage and two type-predicate calls: 31 categories, three sheets, absent keys, signed suspension stages, normal fallback and invalid signed types; part rows are explicit boundary.");
 ah_set_member(bridge_vm,setting_car,"_getSettingPartKey",ah_native(bridge_vm,"missing part key",missing_part_key,NULL));
 ah_set_member(bridge_vm,setting_car,"_getSettingPartRow",ah_native(bridge_vm,"default parts fixture",default_fixture_bridge,NULL));
 for(unsigned test=0;test<4096;test++){
  uint8_t body[0x4b8],actual[0x4b8];for(unsigned j=0;j<sizeof body;j++)body[j]=(uint8_t)(next(&seed)>>24);
  unsigned slot=test%3;memcpy(body+0x488,&slot,4);uint32_t type=test%37;if(test>=512)type|=next(&seed)&0xffff0000u;
  memcpy(c.ram+0x18a0008,body,sizeof body);memset(c.r,0,sizeof c.r);
  c.r[4].uw[0]=0x18a0008+slot*0x178;c.r[5].uw[0]=type;recomp_lookup(0x446ae0)(&c);
  CHECK(gt4_career_remove_part(body,type));if(memcmp(body,c.ram+0x18a0008,sizeof body)){fprintf(stderr,"remove test=%u type=%u slot=%u\n",test,type,slot);for(unsigned j=0;j<sizeof body;j++)if(body[j]!=c.ram[0x18a0008+j])fprintf(stderr,"+%x native=%x original=%x\n",j,body[j],c.ram[0x18a0008+j]);}CHECK(!memcmp(body,c.ram+0x18a0008,sizeof body));
  for(unsigned j=0;j<sizeof body;j++)body[j]=(uint8_t)(next(&seed)>>24);memcpy(body+0x488,&slot,4);
  memcpy(c.ram+0x18a0008,body,sizeof body);CHECK(gt4_career_blob_set(bridge_vm,setting_car,"full_car_ps2",body,sizeof body));
  if(gt4_career_part_removable((int32_t)type)){
   memset(c.r,0,sizeof c.r);c.r[4].uw[0]=0x18a0008+slot*0x178;c.r[5].uw[0]=type;recomp_lookup(0x446ae0)(&c);
  }
  ah_val args[]={ah_int(type),ah_int(test%2?0:1)},fn,ret;
  if(args[1].u.i!=0)memcpy(c.ram+0x18a0008,body,sizeof body);
  CHECK(ah_get_member(bridge_vm,setting_car,"changeParts",&fn));CHECK(ah_call(bridge_vm,fn,2,args,&ret)==0 && ret.t==AV_NIL);
  CHECK(gt4_game_car_body(bridge_vm,setting_car,actual));
  if(getenv("GT4_REMOVE_PART_NEGATIVE") && !test)actual[0]^=1;
  CHECK(!memcmp(actual,c.ram+0x18a0008,sizeof actual));
  default_parts_oracle=1;default_exists=test%3!=0;default_missing=test%5==0;
  default_muffler=test%2?UINT64_MAX:((uint64_t)next(&seed)<<32)|next(&seed);
  int32_t query=test%4==0?22:20;memset(c.r,0,sizeof c.r);
  c.r[4].uw[0]=0x18a0008+slot*0x178;c.r[5].sd[0]=query;c.r[29].uw[0]=0x1ef0000;
  jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));recomp_lookup(0x4455b0)(&c);
  int result;CHECK(gt4_career_default_no_part(actual,query,default_fixture_lookup,NULL,&result));if((uint32_t)result!=c.r[2].uw[0])fprintf(stderr,"default test=%u type=%d exists=%d missing=%d muffler=%llx native=%d original=%u\n",test,query,default_exists,default_missing,(unsigned long long)default_muffler,result,c.r[2].uw[0]);CHECK((uint32_t)result==c.r[2].uw[0]);
  ah_val arg=ah_int(query);CHECK(ah_get_member(bridge_vm,setting_car,"isDefaultNoParts",&fn));CHECK(ah_call(bridge_vm,fn,1,&arg,&ret)==0);
  if(getenv("GT4_DEFAULT_PART_NEGATIVE") && !test)ret.u.i^=1;
  CHECK((uint32_t)ret.u.i==c.r[2].uw[0]);default_parts_oracle=0;ah_gc(bridge_vm);
 }
 puts("PASS 4096 original removals and default-stock-piece queries: three sheets, 30 key offsets, removable stage-zero guard, missing/default records and muffler sentinel; installation and database are outside proof.");

 for(unsigned test=0;test<8192;test++){
  const unsigned types[]={1,6,7,8,9,12,13,15,16,17,18,19,20,21,22,23,25,26,28,29,30,31,0,32,0xffffffffu};
  unsigned type=types[test%25],slot=test%3;uint64_t key=((uint64_t)type<<32)|next(&seed);
  uint8_t body[0x4b8];for(unsigned j=0;j<sizeof body;j++)body[j]=(uint8_t)(next(&seed)>>24);memcpy(body+0x488,&slot,4);
  CHECK(gt4_career_blob_set(bridge_vm,setting_car,"full_car_ps2",body,sizeof body));
  memcpy(c.ram+0x18a0008,body,sizeof body);memset(c.r,0,sizeof c.r);c.r[4].uw[0]=0x18a0000;c.r[5].ud[0]=key;c.r[29].uw[0]=0x1ef0000;
  jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));recomp_lookup(0x440b50)(&c);
  CHECK(gt4_career_install_key_only(body,key));
  if(getenv("GT4_INSTALL_KEY_NEGATIVE") && !test)body[0]^=1;
  if(getenv("GT4_RIGIDITY_NEGATIVE") && type==31)body[0x168]^=1;
  CHECK(!memcmp(body,c.ram+0x18a0008,sizeof body));
  install_fixture_key=key;ah_val args[]={ah_int(type),ah_int(test%4)},fn,ret;
  CHECK(ah_get_member(bridge_vm,setting_car,"changeParts",&fn));CHECK(ah_call(bridge_vm,fn,2,args,&ret)==0 && ret.t==AV_NIL);
  CHECK(gt4_game_car_body(bridge_vm,setting_car,body));CHECK(!memcmp(body,c.ram+0x18a0008,sizeof body));ah_gc(bridge_vm);
 }
 puts("PASS 8192 original key-only installations: 17 categories, three sheets, all-sheet writes, four no-op categories and invalid namespaces; defaults branches excluded.");

 setting_buffer_oracle=1;ah_set_member(bridge_vm,setting_car,"_getSettingPartRow",ah_native(bridge_vm,"parts fixture",setting_fixture_bridge,NULL));
 for(unsigned test=0;test<4096;test++){
  unsigned types[]={2,10,27,3,4,5,14,24},indices[]={0,2,10,5,8,7,4,6};unsigned choice=test%8,slot=(test/8)%3;
  uint64_t key=((uint64_t)types[choice]<<32)|indices[choice];uint8_t body[0x4b8],buffer[0x142],actual[0x4b8],actual_buffer[0x142];
  for(unsigned j=0;j<sizeof body;j++)body[j]=(uint8_t)(next(&seed)>>24);memcpy(body+0x488,&slot,4);
  for(unsigned j=0;j<sizeof buffer;j++)buffer[j]=(uint8_t)(next(&seed)>>24);
  for(unsigned i=0;i<11;i++)for(unsigned j=0;j<sizeof setting_rows[i];j++)setting_rows[i][j]=(uint8_t)(next(&seed)>>24);
  setting_missing=test%4==0?1u<<indices[choice]:0;
  CHECK(gt4_career_blob_set(bridge_vm,setting_car,"full_car_ps2",body,sizeof body));CHECK(gt4_career_blob_set(bridge_vm,setting_car,"_setting_parts_ps2",buffer,sizeof buffer));
  memcpy(c.ram+0x18a0008,body,sizeof body);memcpy(c.ram+0x18b0000,buffer,sizeof buffer);memset(c.r,0,sizeof c.r);
  c.r[4].uw[0]=0x18a0000;c.r[5].ud[0]=key;c.r[6].uw[0]=0x18b0000;c.r[29].uw[0]=0x1ef0000;
  jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));recomp_lookup(0x440b50)(&c);
  CHECK(gt4_career_install_defaults(body,key,buffer,setting_fixture_lookup,NULL));
  if(getenv("GT4_INSTALL_DEFAULT_NEGATIVE") && !test)body[slot*0x178+0x12f]^=1;
  if(memcmp(body,c.ram+0x18a0008,sizeof body)){fprintf(stderr,"defaults test=%u type=%u slot=%u\n",test,types[choice],slot);for(unsigned j=0;j<sizeof body;j++)if(body[j]!=c.ram[0x18a0008+j])fprintf(stderr,"+%x native=%x original=%x\n",j,body[j],c.ram[0x18a0008+j]);}CHECK(!memcmp(body,c.ram+0x18a0008,sizeof body));CHECK(!memcmp(buffer,c.ram+0x18b0000,sizeof buffer));
  install_fixture_key=key;ah_val args[]={ah_int(types[choice]),ah_int(test%4)},fn,ret;
  CHECK(ah_get_member(bridge_vm,setting_car,"changeParts",&fn));CHECK(ah_call(bridge_vm,fn,2,args,&ret)==0 && ret.t==AV_NIL);
  CHECK(gt4_game_car_body(bridge_vm,setting_car,actual));CHECK(gt4_career_blob_get(bridge_vm,setting_car,"_setting_parts_ps2",actual_buffer,sizeof actual_buffer));
  CHECK(!memcmp(actual,body,sizeof body));CHECK(!memcmp(actual_buffer,buffer,sizeof buffer));ah_gc(bridge_vm);
 }
 for(unsigned test=0;test<1024;test++){
  uint8_t body[0x4b8],buffer[0x142];unsigned slot=test%3;uint64_t key=UINT64_C(0xb00000001);
  for(unsigned j=0;j<sizeof body;j++)body[j]=(uint8_t)(next(&seed)>>24);
  for(unsigned j=0;j<sizeof buffer;j++)buffer[j]=(uint8_t)(next(&seed)>>24);
  for(unsigned j=0;j<sizeof setting_rows[1];j++)setting_rows[1][j]=(uint8_t)(next(&seed)>>24);
  setting_missing=test%2?2:0;memcpy(body+0x488,&slot,4);
  memcpy(c.ram+0x18a0008,body,sizeof body);memcpy(c.ram+0x18b0000,buffer,sizeof buffer);memset(c.r,0,sizeof c.r);
  c.r[4].uw[0]=0x18a0000;c.r[5].ud[0]=key;c.r[6].uw[0]=0x18b0000;c.r[29].uw[0]=0x1ef0000;
  jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));recomp_lookup(0x4402f0)(&c);
  CHECK(gt4_career_install_defaults(body,key,buffer,setting_fixture_lookup,NULL));
  if(getenv("GT4_GEAR_DEFAULT_NEGATIVE") && !test)body[slot*0x178+0x110]^=1;
  CHECK(!memcmp(body,c.ram+0x18a0008,sizeof body) && !memcmp(buffer,c.ram+0x18b0000,sizeof buffer));
 }
 puts("PASS 1024 original gearbox defaults, full body/buffer, missing rows; automatic ratio generation outside this proof");
 setting_buffer_oracle=0;
 puts("PASS 4096 original installs with defaults: brake controller, drivetrain, NOS, suspension, TCSC, ASCC, LSD and steering, three sheets, full body and wrapper buffer; row bytes are explicit boundary.");

 gt4_game_bind(bridge_vm,bridge_game);ah_val used_value;CHECK(ah_get_member(bridge_vm,bridge_game,"used_car",&used_value));ah_obj *used=used_value.u.o;
 for(unsigned test=0;test<8192;test++){
  uint8_t bits[32],actual[32];for(unsigned j=0;j<32;j++)bits[j]=(uint8_t)(next(&seed)>>24);
  int32_t old=(int32_t)next(&seed),week=test%2?old:(int32_t)next(&seed);uint32_t id=test%256;
  CHECK(gt4_career_blob_set(bridge_vm,used,"bits_ps2",bits,sizeof bits));ah_set_member(bridge_vm,used,"week",ah_int(old));
  memcpy(c.ram+0x18c0004,bits,32);w32(&c,0x18c0024,(uint32_t)old);memset(c.r,0,sizeof c.r);
  c.r[4].uw[0]=0x18c0000;c.r[5].sd[0]=week;c.r[6].uw[0]=id;c.r[29].uw[0]=0x1ef0000;
  jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));recomp_lookup(test%3?0x43b638:0x43b690)(&c);
  ah_val args[]={ah_int(week),ah_int(id)},ret;CHECK(ah_call_member(bridge_vm,used,test%3?"get":"set",2,args,&ret)==1);
  CHECK(gt4_career_blob_get(bridge_vm,used,"bits_ps2",actual,sizeof actual));
  if(getenv("GT4_USED_STOCK_NEGATIVE") && !test)actual[0]^=1;
  CHECK(!memcmp(actual,c.ram+0x18c0004,32));ah_val saved;CHECK(ah_get_member(bridge_vm,used,"week",&saved));CHECK((uint32_t)saved.u.i==(*(uint32_t *)(c.ram+0x18c0024)));
  if(test%3)CHECK((uint32_t)ret.u.i==c.r[2].uw[0]);ah_gc(bridge_vm);
 }
 puts("PASS 8192 used stock sold bits and week changes against original: all 256 ids, query/set/reset and complete bitset preservation.");
 ah_vm_free(bridge_vm);
 puts("PASS 4096 original owned-car slot writes and native property reads: signed/low-word values, full body preservation and three active setup selections.");
 /* 00114390 traverses the already sorted linked list. Original sorting
    is an explicit boundary here; source/marker/player-index are compared. */
 grid_time_oracle=1;w32(&c,0x6187a8,QUICK);c.ram[0x1867000]=0;
 for(unsigned test=0;test<4096;test++){
  unsigned count=1+test%6,player=next(&seed)%count;uint32_t expected[6];
  w32(&c,ENTRIES,count);w32(&c,ENTRIES+0x14,ENTRY);
  for(unsigned i=0;i<count;i++){
   uint32_t node=ENTRY+i*0x40,car=0x1870000+i*0x1000,time=next(&seed);unsigned finished=(next(&seed)>>24);
   w32(&c,node+0x18,car);w32(&c,node+0x10,i+1<count?node+0x40:0);w32(&c,node+0x14,i==player?0:i+1);
   c.ram[car+0x5b6]=(uint8_t)finished;w32(&c,car+0x718,time);expected[i]=gt4_career_grid_time(finished,time);
  }
  memset(c.r,0,sizeof c.r);c.r[4].uw[0]=QUICK;c.r[5].uw[0]=SESSION;c.r[29].uw[0]=0x1ef0000;
  jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));recomp_lookup(0x114390)(&c);
  CHECK(r32(&c,QUICK+0x4a0)==player);
  for(unsigned i=0;i<count;i++){
   if(getenv("GT4_GRID_TIME_NEGATIVE") && !test)expected[i]^=1;
   CHECK(r32(&c,QUICK+0x454+i*4)==expected[i]);
   memset(c.r,0,sizeof c.r);c.r[5].uw[0]=i;recomp_lookup(0x127df8)(&c);CHECK(c.r[2].uw[0]==expected[i]);
  }
 }
 grid_time_oracle=0;
 puts("PASS 4096 original post-race grids: 1..6 entrants, finished marker/line time, player result slot and getGridTime words; sorting/catalog are explicit boundaries.");
 /* Execute the whole 00119918, retaining its 00395400/00396800 getters.
    Zero entrants isolates metadata from catalog/points boundaries; slot
    zero still supplies the required player entry. Labels/mode are hooks. */
 w32(&c,ENTRIES,0);w32(&c,ENTRIES+8,SLOTS);w32(&c,SLOTS,ENTRY);
 w32(&c,WORLD+0x12c,0x1867200);w32(&c,0x1867210,0);w32(&c,0x1867214,0x1867100);
 w32(&c,RACE+0x80,0x1867300);w32(&c,0x1867304,0x1867400);w32(&c,0x18674dc,0x1867600);
 c.ram[0x1867000]=0;
 static const uint32_t edges[]={0,0x80000000,1,0x80000001,0x3f000000,0x3fc00000,0xbfc00000,
   0x4effffff,0xceffffff,0x4f000000,0xcf000000,0x7f800000,0xff800000,0x7fc00001,0xffc00001};
 for(unsigned test=0;test<16384;test++){
  uint32_t bits=test<sizeof edges/sizeof *edges?edges[test]:next(&seed),laps=next(&seed);float metres;memcpy(&metres,&bits,4);
  w32(&c,0x1867614,bits);w32(&c,RACE,laps);
  memset(c.r,0,sizeof c.r);c.r[4].uw[0]=QUICK;c.r[5].uw[0]=WORLD;c.r[6].uw[0]=SESSION;c.r[29].uw[0]=0x1ef0000;
  jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));recomp_lookup(0x119918)(&c);
  int32_t length=gt4_career_course_length(metres);
  if(getenv("GT4_COURSE_LENGTH_NEGATIVE") && !test)length^=1;
  CHECK((uint32_t)length==r32(&c,QUICK+0x4f0));CHECK(laps==r32(&c,QUICK+0x4f4));
 }
 puts("PASS 16384 original championship metadata cases: laps and course metres, fractional/sign/overflow/subnormal/NaN/Inf boundaries; course pointer getters execute unchanged.");
 puts("PASS 8192 original menu-stack operations: clear, push byte limit, empty strings, pop/ref, saved full buffer tails, load/failure; native Adhoc bridge buffers/strings/checkpoints/returns compared.");
 puts("PASS 4950 original championship round labels and 99 series initializations; RACE lookup/data transfer is an explicit boundary.");
 puts("PASS 8192 original championship rounds/finals: signed points, abandonment, ties, strict victory, records, grand prize, statistics and calendar arguments; data/drawing boundaries explicit.");ee_free(&c);return 0;
}
