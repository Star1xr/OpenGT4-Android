/* Purchase construction oracle. Captures and generated code stay test-only. */
#include "ee.h"
#include "fpu.h"
#include "thread.h"
#include "car.h"
#include "spectables.h"
#include "specdb.h"
#include "vol.h"
#include "career_original.h"
#include "gtmode_game.h"
#include "career_vehicle.h"
#include "save.h"
#include "career_blob.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define SETUP 0x1ec0000u
#define SCRATCH 0x1ec1000u
#define STACK 0x1ef0000u
static int load(const char *path,uint8_t *out,size_t n){FILE *f=fopen(path,"rb");if(!f)return 0;int ok=fread(out,1,n,f)==n && fgetc(f)==EOF;fclose(f);return ok;}
static uint32_t r32(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
static uint64_t original_t0;
static ee_fn scheduled_function;
static void scheduled_entry(ee_ctx *c){ee_fpu_enter();scheduled_function(c);ee_fpu_leave();}
static int original(ee_ctx *c,uint32_t fn,uint64_t a0,uint64_t a1,uint64_t a2,uint64_t a3){
 FILE *code=fopen("extracted/core.bin","rb");uint8_t entry[32];
 if(!code)return 0;int matches=fn>=0x100000 && fn<EE_RAM_SIZE-32 && !fseek(code,fn-0x100000,SEEK_SET) && fread(entry,1,32,code)==32 && !memcmp(entry,c->ram+fn,32);fclose(code);if(!matches){fprintf(stderr,"original entry mismatch %08x\n",fn);return 0;}
 memset(c->r,0,sizeof c->r);c->r[4].ud[0]=a0;c->r[5].ud[0]=a1;c->r[6].ud[0]=a2;c->r[7].ud[0]=a3;c->r[8].ud[0]=original_t0;c->r[29].uw[0]=STACK;c->end_reason=0;
 c->lookup=recomp_lookup;jmp_buf jump;c->exit_jmp=&jump;ee_fpu_enter();
 if(setjmp(jump)){ee_fpu_leave();fprintf(stderr,"original %08x stopped: %s\n",fn,c->end_detail);return 0;}
 scheduled_function=recomp_lookup(fn);
 if(ee_sched_run(c,scheduled_entry) || c->end_reason!=EE_END_RETURNED){ee_fpu_leave();fprintf(stderr,"original %08x stopped: %s\n",fn,c->end_detail);ee_sched_free(c);return 0;}
 ee_sched_free(c);ee_fpu_leave();return 1;
}

static int fixture_key(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
 (void)vm;(void)self;(void)argc;(void)argv;*ret=ah_int(*(uint32_t *)data);return 0;
}
static ah_val member(ah_vm *vm,ah_obj *o,const char *key){ah_val v=ah_nil();ah_get_member(vm,o,key,&v);return v;}
static int native_event(const uint8_t *before,const uint8_t *after,uint32_t id,int64_t color,int subtype,int use_default){
 ah_vm *vm=ah_vm_new();ah_obj *game=ah_module_new(vm,"game",ah_root(vm),NULL,NULL);gt4_game_bind(vm,game);
 ah_obj *cal=member(vm,game,"calendar").u.o,*date=ah_array_new(vm),*events=ah_array_new(vm);int fields[3];
 if(!gt4_game_decode_calendar_day((int32_t)r32(before+0xb6c0),fields))return 0;
 for(unsigned i=0;i<3;i++)ah_array_push(vm,date,ah_int(fields[i]));ah_set_member(vm,cal,"today",ah_objv(date));
 for(unsigned i=0;i<r32(before+0x10);i++){
  ah_obj *record=ah_array_new(vm);unsigned offset=0x18+((r32(before+0xc)+i)%2922)*16;
  for(unsigned j=0;j<16;j++)ah_array_push(vm,record,ah_int(before[offset+j]));ah_array_push(vm,events,ah_objv(record));
 }
 ah_set_member(vm,cal,"original_events",ah_objv(events));ah_set_member(vm,cal,"_getCarKey",ah_native(vm,"fixture_key",fixture_key,&id));
 ah_val args[]={ah_str(vm,"oracle-resolved-car"),ah_int(color)},ret;
 const char *methods[]={"putBuyNewCarEvent","putBuyUsedCarEvent","putBuyTradeCarEvent","putPresentCarEvent"};
 int status=ah_call_member(vm,cal,methods[subtype-1],use_default?1:2,args,&ret);if(status<1 || ret.t!=AV_VOID){fprintf(stderr,"calendar call status=%d ret=%d\n",status,ret.t);return 0;}
 uint8_t *packed=NULL;size_t size=0;
 if(!gt4_state_pack(vm,game,&packed,&size) || !gt4_state_replace(vm,game,packed,size))return 0;free(packed);
 cal=member(vm,game,"calendar").u.o;events=member(vm,cal,"original_events").u.o;
 if((unsigned)ah_array_size(events)!=r32(after+0x10)){fprintf(stderr,"events %d/%u after pack\n",ah_array_size(events),r32(after+0x10));return 0;}
 for(unsigned i=0;i<r32(after+0x10);i++){
  ah_obj *record=ah_array_get(events,(int)i).u.o;unsigned offset=0x18+((r32(after+0xc)+i)%2922)*16;
  for(unsigned j=0;j<16;j++)if(ah_array_get(record,(int)j).u.i!=after[offset+j]){fprintf(stderr,"event %u byte %u native=%lld orig=%u\n",i,j,(long long)ah_array_get(record,(int)j).u.i,after[offset+j]);return 0;}
 }
 if(!gt4_game_decode_calendar_day((int32_t)r32(after+0xb6c0),fields))return 0;date=member(vm,cal,"today").u.o;
 for(unsigned i=0;i<3;i++)if(ah_array_get(date,(int)i).u.i!=fields[i])return 0;
 ah_vm_free(vm);return 1;
}
static int native_selection(const uint8_t *before,const uint8_t *after,unsigned index,int change){
 ah_vm *vm=ah_vm_new();ah_obj *game=ah_module_new(vm,"game",ah_root(vm),NULL,NULL);gt4_game_bind(vm,game);
 ah_obj *garage=member(vm,game,"garage").u.o,*cars=ah_array_new(vm),*history=ah_array_new(vm);
 for(unsigned i=0;i<1000;i++){
  uint64_t flags,position;memcpy(&flags,before+0x10+i*32,8);memcpy(&position,before+0x20+i*32,8);
  int64_t f,h;memcpy(&f,&flags,8);memcpy(&h,&position,8);ah_array_push(vm,history,ah_int(h));
  if(flags&1){ah_obj *car=ah_module_new(vm,"car",NULL,NULL,NULL);ah_set_member(vm,car,"flags",ah_int(f));ah_set_member(vm,car,"history",ah_int(h));ah_array_push(vm,cars,ah_objv(car));}
  else ah_array_push(vm,cars,ah_nil());
 }
 /* The destination must be occupied for this authored native contract. */
 if(ah_array_get(cars,(int)index).t==AV_NIL){ah_vm_free(vm);return 0;}
 ah_set_member(vm,garage,"cars",ah_objv(cars));ah_set_member(vm,garage,"original_unit_history",ah_objv(history));
 ah_set_member(vm,garage,"riding_index",ah_int((int32_t)r32(before+0x81c8)));
 ah_set_member(vm,garage,"original_pending_change",ah_int((int32_t)r32(before+0x81f4)));
 ah_val args[]={ah_int(index),ah_array_get(cars,(int)index)},ret;
 if(ah_call_member(vm,garage,change?"changeRidingCar":"setRidingCar",2,args,&ret)<1 || ret.t!=AV_VOID)return 0;
 uint8_t *packed;size_t size;if(!gt4_state_pack(vm,game,&packed,&size) || !gt4_state_replace(vm,game,packed,size))return 0;free(packed);
 garage=member(vm,game,"garage").u.o;cars=member(vm,garage,"cars").u.o;history=member(vm,garage,"original_unit_history").u.o;
 if(member(vm,garage,"riding_index").u.i!=index)return 0;
 for(unsigned i=0;i<1000;i++){
  uint64_t flags,h;memcpy(&flags,after+0x10+i*32,8);memcpy(&h,after+0x20+i*32,8);
  if((uint64_t)ah_array_get(history,(int)i).u.i!=h)return 0;
  ah_obj *car=ah_as_obj(ah_array_get(cars,(int)i),AO_MODULE);
  if(car && ((uint64_t)member(vm,car,"flags").u.i!=flags || (uint64_t)member(vm,car,"history").u.i!=h))return 0;
 }
 ah_vm_free(vm);return 1;
}
/* Complete native transaction. Only pre-purchase garage/calendar bytes and
   the user's label/color/mileage choice initialize this VM. AFTER is oracle-only. */
static int purchase_check(ah_vm *vm,ah_obj *game,const uint8_t *before,const uint8_t *after){
 const uint32_t g=0xa18840,cal=0xa0d178,profile=0xa0cf58;
 ah_obj *garage=member(vm,game,"garage").u.o,*cars=member(vm,garage,"cars").u.o;
 uint8_t actual[0x81d0];memcpy(actual,before+g,sizeof actual);
 if(!gt4_career_blob_get(vm,garage,"original_units_ps2",actual+8,32000))return 0;
 ah_obj *history=member(vm,garage,"original_unit_history").u.o;
 for(unsigned i=0;i<1000;i++){
  ah_obj *car=ah_as_obj(ah_array_get(cars,(int)i),AO_MODULE);
  if(car){if(!gt4_game_car_unit(vm,car,actual+8+i*32))return 0;}
  else {uint64_t h=(uint64_t)ah_array_get(history,(int)i).u.i;memcpy(actual+0x20+i*32,&h,8);}
 }
 int32_t riding=(int32_t)member(vm,garage,"riding_index").u.i;memcpy(actual+0x81c8,&riding,4);
 if(riding>=0){ah_obj *car=ah_as_obj(ah_array_get(cars,riding),AO_MODULE);if(!gt4_game_car_body(vm,car,actual+0x7d10))return 0;
  uint8_t current[0x4b8];if(!gt4_career_blob_get(vm,garage,"current_car_ps2",current,sizeof current) || memcmp(current,actual+0x7d10,sizeof current))return 0;
 }
 if(memcmp(actual,after+g,sizeof actual)){for(unsigned i=0;i<sizeof actual;i++)if(actual[i]!=after[g+i]){fprintf(stderr,"native garage +%x actual=%02x expected=%02x\n",i,actual[i],after[g+i]);break;}return 0;}
 if((uint64_t)member(vm,game,"money").u.i!=gt4_career_money_get(after+profile))return 0;
 uint8_t calendar[0xb6c8];memcpy(calendar,before+cal,sizeof calendar);
 ah_obj *c=member(vm,game,"calendar").u.o,*events=member(vm,c,"original_events").u.o;
 unsigned initial=r32(before+cal+0x10),count=(unsigned)ah_array_size(events),next=r32(before+cal+8),first=r32(before+cal+0xc);
 if(count!=initial+1)return 0;
 for(unsigned i=0;i<count;i++){ah_obj *event=ah_array_get(events,(int)i).u.o;unsigned slot=(first+i)%2922;for(unsigned j=0;j<16;j++)calendar[0x18+slot*16+j]=(uint8_t)ah_array_get(event,j).u.i;}
 next=(next+1)%2922;memcpy(calendar+8,&next,4);memcpy(calendar+0x10,&count,4);
 ah_obj *date=member(vm,c,"today").u.o;int expected_date[3];if(!gt4_game_decode_calendar_day((int32_t)r32(after+cal+0xb6c0),expected_date))return 0;
 for(unsigned i=0;i<3;i++)if(ah_array_get(date,(int)i).u.i!=expected_date[i])return 0;
 /* The menu refresh consumes dirty; this is a presentation cache flag. */
 memcpy(calendar+0xb6c0,after+cal+0xb6c0,8);
 if(memcmp(calendar,after+cal,sizeof calendar))return 0;
 return 1;
}
static int native_purchase(const uint8_t *before,const uint8_t *inserted,const uint8_t *after,
                           const char *label,uint32_t id,int32_t color,uint32_t mileage,int32_t price,
                           gt4_career_vehicle *factory){
 const uint32_t g=0xa18840,cal=0xa0d178;
 ah_vm *vm=ah_vm_new();ah_obj *game=ah_module_new(vm,"game",ah_root(vm),NULL,NULL);gt4_game_bind(vm,game);
 ah_obj *garage=member(vm,game,"garage").u.o,*calendar=member(vm,game,"calendar").u.o,*history=ah_array_new(vm),*events=ah_array_new(vm),*date=ah_array_new(vm);
 ah_set_member(vm,game,"money",ah_int((int64_t)gt4_career_money_get(before+0xa0cf58)));
 gt4_career_blob_set(vm,garage,"original_units_ps2",before+g+8,32000);
 for(unsigned i=0;i<1000;i++){int64_t h;memcpy(&h,before+g+0x20+i*32,8);ah_array_push(vm,history,ah_int(h));}
 ah_set_member(vm,garage,"original_unit_history",ah_objv(history));
 int fields[3];if(!gt4_game_decode_calendar_day((int32_t)r32(before+cal+0xb6c0),fields))return 0;
 for(unsigned i=0;i<3;i++)ah_array_push(vm,date,ah_int(fields[i]));ah_set_member(vm,calendar,"today",ah_objv(date));
 for(unsigned i=0;i<r32(before+cal+0x10);i++){ah_obj *event=ah_array_new(vm);unsigned slot=(r32(before+cal+0xc)+i)%2922;for(unsigned j=0;j<16;j++)ah_array_push(vm,event,ah_int(before[cal+0x18+slot*16+j]));ah_array_push(vm,events,ah_objv(event));}
 ah_set_member(vm,calendar,"original_events",ah_objv(events));ah_set_member(vm,calendar,"_getCarKey",ah_native(vm,"fixture_key",fixture_key,&id));
 ah_obj *source=ah_module_new(vm,"chosen_used_car",NULL,NULL,NULL);
 ah_set_member(vm,source,"car_code",ah_str(vm,label));ah_set_member(vm,source,"color_index",ah_int(color));ah_set_member(vm,source,"odometer",ah_int(mileage));
 gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);
 if(!gt4_game_construct_car(vm,source,label,0,0))return 0;
 ah_val used_args[]={ah_int(123),ah_int(color),ah_int(mileage)},preview_ret;
 if(ah_call_member(vm,source,"makeUsed",3,used_args,&preview_ret)<1)return 0;
 ah_val input=ah_objv(source),ret;if(ah_call_member(vm,garage,"addUsedCar",1,&input,&ret)<1 || ret.u.i!=0){fprintf(stderr,"native addUsedCar failed\n");return 0;}
 ah_obj *car=ah_array_get(member(vm,garage,"cars").u.o,0).u.o;uint8_t unit[32];
 if(!gt4_game_car_unit(vm,car,unit) || memcmp(unit,inserted+g+8,32))return 0;
 ah_val args[]={ah_str(vm,label),ah_int(color)};
 if(ah_call_member(vm,calendar,"putBuyUsedCarEvent",2,args,&ret)<1)return 0;
 ah_val debit=ah_int(-(int64_t)price);if(ah_call_member(vm,game,"incMoney",1,&debit,&ret)<1)return 0;
 ah_val selection[]={ah_int(0),ah_objv(source)}; /* The menu passes its native constructed used preview. */
 if(ah_call_member(vm,garage,"setRidingCar",2,selection,&ret)<1 || !purchase_check(vm,game,before,after)){fprintf(stderr,"complete native transaction differs before save\n");return 0;}
 uint8_t *packed=NULL;size_t size=0;const char *path="build/career-native/purchase-roundtrip.sav";
 if(!gt4_state_pack(vm,game,&packed,&size) || !gt4_save_write(path,packed,size))return 0;free(packed);
 uint64_t stamp;if(gt4_save_read(path,&packed,&size,&stamp)<1)return 0;
 ah_vm *reload=ah_vm_new();ah_obj *copy=ah_module_new(reload,"game",ah_root(reload),NULL,NULL);
 if(!gt4_state_replace(reload,copy,packed,size))return 0;free(packed);gt4_game_bind(reload,copy);
 int ok=purchase_check(reload,copy,before,after);if(!ok)fprintf(stderr,"native transaction differs after file reload\n");
 gt4_game_set_car_factory(NULL,NULL);ah_vm_free(reload);ah_vm_free(vm);return ok;
}
int main(int argc,char **argv){
 if(argc!=4 && argc!=6){fprintf(stderr,"usage: gtmode_purchase_oracle RAM SPR ISO [AFTER-INSERT-RAM AFTER-COMPLETE-RAM]\n");return 2;}
 ee_ctx c;if(ee_init(&c))return 2;
 uint8_t *ram=malloc(EE_RAM_SIZE),*native=malloc(EE_RAM_SIZE),*spr=malloc(EE_SPR_SIZE);
 if(!ram || !native || !spr || !load(argv[1],ram,EE_RAM_SIZE) || !load(argv[2],spr,EE_SPR_SIZE))return 2;
 const char *err=NULL;gt4_vol *vol=gt4_vol_open(argv[3],&err);gt4_spec_tables *tables=vol?gt4_spec_tables_open(vol,NULL):NULL;if(!tables)return 2;
 gt4_specdb_src db={gt4_spec_tables_row,tables,gt4_spec_tables_str,gt4_spec_tables_car_parts};
 size_t size;const uint8_t *generic=gt4_spec_tables_data(tables,0,&size);if(!generic || size<16)return 2;
 unsigned cases=0;uint8_t *old_mem=gt4_phys_mem;
 for(unsigned i=0;i<r32(generic+8);i++){
  uint32_t id=r32(generic+16+i*8);
  memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);
  for(unsigned j=0;j<0x178;j++)c.ram[SETUP+j]=(uint8_t)(j*37+i);
  /* func_00443240 copies all 12 ratio words although 00354FF8 only
     writes the active gears. Seed its untouched caller frame explicitly. */
  memset(c.ram+STACK-0x120-0x670,0,0x670);
  memcpy(native,c.ram,EE_RAM_SIZE);
  if(!original(&c,0x444480,SETUP,id,0,0))return 1;
  gt4_phys_mem=native;ee_fpu_enter();int result=gt4_garage_new(SETUP,id,&db,SCRATCH);ee_fpu_leave();gt4_phys_mem=old_mem;
  if(result!=(int)c.r[2].sw[0] || memcmp(native+SETUP,c.ram+SETUP,0x178)){
   fprintf(stderr,"setup mismatch car=%u return=%d/%d\n",id,result,c.r[2].sw[0]);
   for(unsigned j=0;j<0x178;j++)if(native[SETUP+j]!=c.ram[SETUP+j])fprintf(stderr," +%03x original=%02x native=%02x\n",j,c.ram[SETUP+j],native[SETUP+j]);return 1;
  }
  cases++;
 }
 printf("func_00444480: %u/%u identical setup blocks (0x178 bytes); runtime SpecDB; setup-only component suite\n",cases,cases);


 /* Exercise full replacement, vtable preservation, occupied and vacant
    history slots, and the conditional previous-car flag in 00435418. */
 unsigned selection_cases=0;
 const uint32_t g=0xa18840;
 for(unsigned variant=0;variant<64;variant++)for(unsigned change=0;change<2;change++)for(unsigned pending=0;pending<2;pending++){
  memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);
  uint32_t index=variant==63?999:variant*15,previous=(index+37)%1000;
  memcpy(c.ram+g+0x81c8,&previous,4);memcpy(c.ram+g+0x81f4,&pending,4);
  for(unsigned j=0;j<1000;j++){
   uint64_t h=0x9abcdef012345400ull|((j*17+variant)%1024),f=0xfedcba9876543210ull|(j%4);
   memcpy(c.ram+g+8+j*32+0x18,&h,8);memcpy(c.ram+g+0x10+j*32,&f,8);
  }
  c.ram[g+0x10+index*32]|=1;
  for(unsigned j=0;j<0x4c0;j++)c.ram[SETUP+j]=(uint8_t)(j*31+variant);
  uint8_t before_garage[0x81f8];memcpy(before_garage,c.ram+g,sizeof before_garage);
  memcpy(native,c.ram,EE_RAM_SIZE);
  if(!original(&c,change?0x435418:0x435388,g,index,SETUP,0))return 1;
  if(change)gt4_career_garage_change_riding(native+g,index,native+SETUP);
  else gt4_career_garage_set_riding(native+g,index,native+SETUP);
  if(memcmp(native+g,c.ram+g,0x81f8)){
   fprintf(stderr,"riding selection mismatch index=%u change=%u pending=%u\n",index,change,pending);return 1;
  }
  if(variant<2 && !native_selection(before_garage,c.ram+g,index,(int)change)){
   fprintf(stderr,"native selection/save mismatch variant=%u change=%u pending=%u\n",variant,change,pending);return 1;
  }
  selection_cases++;
 }
 printf("func_00435388 / 00435418 / 00435690: %u/%u identical complete garage objects (0x81f8 bytes); 8 native flag/history/save comparisons\n",selection_cases,selection_cases);

 const uint64_t mileages[]={0,1,16777215,16777216,16777217,85574576,0x5f5e0fff,0x5f5e1000,0x5f5e1001,0x7fffffff,0x80000000,0xffffffff,0x100000000,0x7fffffffffffffff};
 gt4_career_oil_data oil;if(!gt4_career_oil_data_load(argv[3],&oil))return 2;
 /* Validate complete standard constructors, including compact power/weight,
    owned parts, three setups and wear, for every disc catalog row. */
 unsigned vehicle_cases=0;
 for(unsigned i=0;i<r32(generic+8);i++){
  uint32_t id=r32(generic+16+i*8);uint64_t mileage=i%2?85574576:0;int32_t color=0;
  memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);
  memset(c.ram+STACK-0x10000,0,0x10000);
  uint8_t unit[32],body[0x4b8];for(unsigned j=0;j<32;j++)unit[j]=(uint8_t)(i+j*17);
  memcpy(c.ram+SETUP,unit,32);memcpy(native,c.ram,EE_RAM_SIZE);
  if(!original(&c,0x434e38,SETUP,id,color,mileage))return 1;
  gt4_career_oil_data data=oil;
  memcpy(data.params,ram+0x620320,36);memcpy(data.settings,ram+0x620cd8,sizeof data.settings);memcpy(data.clutch_maximum,ram+0x620b18,4);
  gt4_phys_mem=native;ee_fpu_enter();int ok=gt4_career_vehicle_build(&db,id,color,mileage,&data,unit,body);ee_fpu_leave();gt4_phys_mem=old_mem;
  if(!ok || memcmp(unit,c.ram+SETUP,32)){
   fprintf(stderr,"complete unit mismatch id=%u mileage=%llu\n",id,(unsigned long long)mileage);
   for(unsigned j=0;j<32;j++)if(unit[j]!=c.ram[SETUP+j])fprintf(stderr,"unit +%x C=%02x original=%02x\n",j,unit[j],c.ram[SETUP+j]);return 1;
  }
  memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);memset(c.ram+STACK-0x10000,0,0x10000);memset(c.ram+SETUP,0,0x4c0);
  if(!original(&c,0x43dc20,SETUP,0,0,0) || !original(&c,0x440ec8,SETUP,id,0,0) || !original(&c,0x440fb8,SETUP,0,color,mileage))return 1;
  if(memcmp(body,c.ram+SETUP+8,sizeof body)){
   fprintf(stderr,"complete body mismatch id=%u mileage=%llu\n",id,(unsigned long long)mileage);
   unsigned diffs=0;for(unsigned j=0;j<sizeof body;j++)if(body[j]!=c.ram[SETUP+8+j] && diffs++<16)fprintf(stderr,"body +%x C=%02x original=%02x\n",j,body[j],c.ram[SETUP+8+j]);return 1;
  }
  vehicle_cases++;
 }
 printf("complete standard vehicle constructors: %u/%u identical compact units (32 bytes) and full bodies (0x4b8); all runtime catalog cars, new/used mileage\n",vehicle_cases,vehicle_cases);
 unsigned used_cases=0;
 for(unsigned i=0;i<r32(generic+8);i++){
  uint32_t id=r32(generic+16+i*8),seed=i*0x9e3779b9u;int32_t color=i%3?0:-1;uint64_t mileage=UINT64_MAX;
  memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);memset(c.ram+STACK-0x10000,0,0x10000);memset(c.ram+SETUP,0,0x4c0);
  memcpy(native,c.ram,EE_RAM_SIZE);
  if(!original(&c,0x43dc20,SETUP,0,0,0) || !original(&c,0x440ec8,SETUP,id,0,0) || !original(&c,0x440fb8,SETUP,seed,(uint64_t)(int64_t)color,mileage))return 1;
  gt4_career_oil_data data=oil;memcpy(data.params,ram+0x620320,36);memcpy(data.settings,ram+0x620cd8,sizeof data.settings);memcpy(data.clutch_maximum,ram+0x620b18,4);
  uint8_t unit[32]={0},body[0x4b8];gt4_phys_mem=native;ee_fpu_enter();
  int ok=gt4_career_vehicle_used_choices(&db,id,(int32_t)seed,&color,&mileage) && gt4_career_vehicle_build(&db,id,color,mileage,&data,unit,body);
  ee_fpu_leave();gt4_phys_mem=old_mem;
  if(!ok || memcmp(body,c.ram+SETUP+8,sizeof body)){
   fprintf(stderr,"seeded used constructor mismatch id=%u seed=%u color=%d mileage=%llu\n",id,seed,color,(unsigned long long)mileage);
   unsigned diffs=0;for(unsigned j=0;j<sizeof body;j++)if(body[j]!=c.ram[SETUP+8+j] && diffs++<16)fprintf(stderr,"body +%x C=%02x original=%02x\n",j,body[j],c.ram[SETUP+8+j]);return 1;
  }
  used_cases++;
 }
 printf("seeded makeUsed: %u/%u identical full bodies, original age ranges/RNG and random paint across all catalog cars\n",used_cases,used_cases);
 unsigned wear_cases=0;
 for(unsigned exempt=0;exempt<2;exempt++)for(unsigned i=0;i<sizeof mileages/sizeof *mileages;i++){
  memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);
  if(!original(&c,0x343490,0xa20548+0x4a0,0,0,0) || !original(&c,0x343420,0xa20548+0x4a0,exempt,0,0) || !original(&c,0x440fb8,0xa20548,0,0,mileages[i]))return 1;
  uint8_t expected[32];gt4_career_oil_data data=oil;
  /* Lazy params/settings are execution state; use the same initial globals
     for both implementations, rather than silently resetting only one. */
  memcpy(data.params,ram+0x620320,36);memcpy(data.settings,ram+0x620cd8,sizeof data.settings);memcpy(data.clutch_maximum,ram+0x620b18,4);
  gt4_career_car_wear_init(expected,mileages[i],(uint8_t)exempt,&data);
  if(memcmp(expected,c.ram+0xa20548+0x4a0,32) || memcmp(data.params,c.ram+0x620320,36) || memcmp(data.settings,c.ram+0x620cd8,sizeof data.settings)){
   fprintf(stderr,"wear mismatch exempt=%u mileage=%llu\n",exempt,(unsigned long long)mileages[i]);return 1;
  }
  wear_cases++;
 }
 printf("func_00440FB8 nonnegative explicit mileage: %u/%u identical 32-byte wear blocks and lazy oil globals\n",wear_cases,wear_cases);


 if(!gt4_game_load_disc(argv[3]))return 2;
 const int64_t colors[]={-129,-128,-1,0,127,128,255,256};unsigned event_cases=0;
 for(int subtype=1;subtype<=4;subtype++)for(unsigned i=0;i<=sizeof colors/sizeof *colors;i++){
  int use_default=i==sizeof colors/sizeof *colors;int64_t color=use_default?0:colors[i];
  memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);
  /* Resolve the label through the runtime SpecDB, using a real key. */
  size_t labels_size;uint8_t *labels=gt4_vol_read(vol,"/specdb/GT4_US2560/GENERIC_CAR.idi",&labels_size);
  uint32_t id=r32(generic+16);const char *label=gt4_specdb_label_for_id(labels,labels_size,id);if(!label)return 2;
  strcpy((char *)c.ram+0x1ee0000,label);free(labels);memset(c.ram+STACK-0x30,0,8);
  if(!original(&c,0x42f1b8,0xa0d178,0x1ee0000,(uint64_t)color,subtype) || !native_event(ram+0xa0d178,c.ram+0xa0d178,id,color,subtype,use_default)){
   fprintf(stderr,"native purchase calendar mismatch subtype=%d color=%lld\n",subtype,(long long)color);return 1;
  }
  event_cases++;
 }
 printf("purchase calendar natives: %u/%u identical records/dates after native save serialization round trip\n",event_cases,event_cases);
 if(argc==6){
  const uint32_t garage=0xa18840,calendar=0xa0d178,profile=0xa0cf58,label_at=0x1ee0000;
  uint8_t *inserted=malloc(EE_RAM_SIZE),*completed=malloc(EE_RAM_SIZE),*expected=malloc(EE_RAM_SIZE);
  if(!inserted || !completed || !expected || !load(argv[4],inserted,EE_RAM_SIZE) || !load(argv[5],completed,EE_RAM_SIZE) || !gt4_game_load_disc(argv[3]))return 2;
  uint32_t id=r32(inserted+garage+8),mileage=r32(inserted+garage+8+20),color=(r32(inserted+garage+16)>>2)&127;
  size_t labels_size;uint8_t *labels=gt4_vol_read(vol,"/specdb/GT4_US2560/GENERIC_CAR.idi",&labels_size);
  const char *label=gt4_specdb_label_for_id(labels,labels_size,id);if(!label)return 2;
  gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[3]);uint8_t built_unit[32],built_body[0x4b8];
  memcpy(built_unit,ram+garage+8,32);
  if(!factory || !gt4_career_vehicle_create(factory,label,(int32_t)color,mileage,built_unit,built_body)){fprintf(stderr,"native factory failed\n");return 1;}
  gt4_career_calendar_data dates;if(!gt4_career_calendar_data_load(argv[3],&dates))return 2;
  gt4_career_car_acquisition_day(built_unit,&dates,(int32_t)r32(ram+calendar+0xb6c0));
  if(memcmp(built_unit,inserted+garage+8,32) || memcmp(built_body,completed+garage+0x7d10,0x4b8)){
   fprintf(stderr,"native constructed actual car mismatch\n");
   for(unsigned j=0;j<32;j++)if(built_unit[j]!=inserted[garage+8+j])fprintf(stderr,"unit +%x C=%02x actual=%02x\n",j,built_unit[j],inserted[garage+8+j]);
   unsigned differences=0;for(unsigned j=0;j<0x4b8;j++)if(built_body[j]!=completed[garage+0x7d10+j] && differences++<40)fprintf(stderr,"body +%x C=%02x actual=%02x\n",j,built_body[j],completed[garage+0x7d10+j]);
   fprintf(stderr,"body differences %u\n",differences);return 1;
  }
  printf("native factory: actual bought BEAT compact unit and full body match, with no completed-car input\n");
  /* Keep the factory alive for the integrated native transaction below. */
  memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);strcpy((char *)c.ram+label_at,label);
  original_t0=mileage;
  if(!original(&c,0x435800,garage,label_at,color,r32(ram+calendar+0xb6c0)))return 1;
  original_t0=0;
  if(c.r[2].sw[0]!=0 || memcmp(c.ram+garage,inserted+garage,0x81d0)){
   fprintf(stderr,"actual purchase insertion mismatch (32-byte unit and whole garage)\n");return 1;
  }
  uint8_t row[24];int32_t price;
  if(gt4_specdb_row(generic,size,id,row,24)!=24 || !gt4_game_car_price((int32_t)r32(row+8),mileage,0,&price))return 2;
  memcpy(expected,ram,EE_RAM_SIZE);
  gt4_career_money_inc(expected+profile,0u-(uint64_t)(int64_t)price);
  uint8_t event[16]={0};gt4_career_calendar_car_event(event,0,id,color,2);gt4_career_calendar_append(expected+calendar,event);
  memset(c.ram+STACK-0x30,0,8);
  if(!original(&c,0x42f1b8,calendar,label_at,color,2) || !original(&c,0x43afb0,profile,0u-(uint64_t)(int64_t)price,0,0))return 1;
  if(memcmp(c.ram+calendar,expected+calendar,0xb6c8)){fprintf(stderr,"C purchase calendar differs from original before refresh\n");return 1;}
  int32_t dirty=gt4_career_calendar_consume_dirty(expected+calendar);
  if(!original(&c,0x600b88,calendar,0,0,0) || c.r[2].sw[0]!=dirty)return 1;
  if(memcmp(c.ram+calendar,expected+calendar,0xb6c8) || memcmp(c.ram+calendar,completed+calendar,0xb6c8) ||
     gt4_career_money_get(c.ram+profile)!=gt4_career_money_get(expected+profile) || gt4_career_money_get(c.ram+profile)!=gt4_career_money_get(completed+profile)){
   fprintf(stderr,"actual purchase price/money/calendar mismatch price=%d money=%llu/%llu/%llu\n",price,(unsigned long long)gt4_career_money_get(c.ram+profile),(unsigned long long)gt4_career_money_get(expected+profile),(unsigned long long)gt4_career_money_get(completed+profile));
   for(unsigned j=0;j<0xb6c8;j++)if(c.ram[calendar+j]!=expected[calendar+j] || c.ram[calendar+j]!=completed[calendar+j])fprintf(stderr,"calendar +%x original=%02x C=%02x actual=%02x\n",j,c.ram[calendar+j],expected[calendar+j],completed[calendar+j]);return 1;
  }
  gt4_career_oil_data data=oil;uint8_t wear[32];
  uint8_t exempt=row[0x11]>=100;
  gt4_career_car_wear_init(wear,mileage,exempt,&data);
  if(memcmp(wear,completed+0xa20548+0x4a0,32)){fprintf(stderr,"actual purchased-car wear mismatch exempt=%u\n",exempt);for(unsigned j=0;j<32;j++)if(wear[j]!=completed[0xa20548+0x4a0+j])fprintf(stderr,"wear +%x C=%02x actual=%02x\n",j,wear[j],completed[0xa20548+0x4a0+j]);return 1;}
  /* Isolate the selection action: its full-car argument is the captured
     BEAT state, not a claim that our purchase constructor produced it. */
  memcpy(c.ram,inserted,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);
  memcpy(c.ram+SETUP,completed+garage+0x7d08,0x4c0);
  memcpy(expected, c.ram, EE_RAM_SIZE);
  if(!original(&c,0x435388,garage,0,SETUP,0))return 1;
  gt4_career_garage_set_riding(expected+garage,0,expected+SETUP);
  if(memcmp(c.ram+garage,completed+garage,0x81d0) || memcmp(expected+garage,completed+garage,0x81d0) ||
     !native_selection(inserted+garage,completed+garage,0,0)){
   fprintf(stderr,"actual selected-car garage/history mismatch\n");return 1;
  }
  printf("actual PS2 purchase selection: original and C match complete garage (0x81d0 bytes), given the same captured full-car argument; native flags/history survive save reload\n");
  if(!native_purchase(ram,inserted,completed,label,id,(int32_t)color,mileage,price,factory))return 1;
  gt4_career_vehicle_close(factory);
  printf("PASS complete native PS2 used purchase: %s mileage=%u color=%u price=%d balance=%llu; whole garage 0x81d0, calendar 0xb6c8 and money identical before and after disk save/new-VM reload; no completed-car input\n",label,mileage,color,price,(unsigned long long)gt4_career_money_get(completed+profile));
  free(labels);free(inserted);free(completed);free(expected);
 }
 gt4_spec_tables_close(tables);gt4_vol_close(vol);free(ram);free(native);free(spr);ee_free(&c);return 0;
}
