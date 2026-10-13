/* One authored sale action: quote, deletion, credit, calendar, native file save.
   Original functions are test-only. Reserved event stack bytes are seeded zero. */
#include "ee.h"
#include "fpu.h"
#include "gtmode_game.h"
#include "save_ps2.h"
#include "save.h"
#include "specdb.h"
#include "vol.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define PROFILE 0xa0cf58u
#define GARAGE (PROFILE+0xb8e8u)
#define CALENDAR (PROFILE+0x220u)
#define CAR 0xa20548u
#define STACK 0x1ef0000u
#define LABEL 0x1ee0000u
static uint32_t r32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t r64(const uint8_t *p){return r32(p)|(uint64_t)r32(p+4)<<32;}
static void w32(uint8_t *p,uint32_t v){for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static void w64(uint8_t *p,uint64_t v){w32(p,(uint32_t)v);w32(p+4,(uint32_t)(v>>32));}
static int load(const char *path,uint8_t *p,size_t n){FILE *f=fopen(path,"rb");if(!f)return 0;int ok=fread(p,1,n,f)==n && fgetc(f)==EOF;fclose(f);return ok;}
static ah_val get(ah_vm *vm,ah_obj *o,const char *name){ah_val v=ah_nil();ah_get_member(vm,o,name,&v);return v;}
typedef struct {uint8_t *labels;size_t size;} catalog;
static int key(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
 (void)self;catalog *cat=data;uint32_t id;if(argc!=1 || !gt4_specdb_label(cat->labels,cat->size,ah_cstr(argv[0]),&id))return ah_error(vm,"test car label missing");*ret=ah_int(id);return 0;
}
static int call(ee_ctx *c,uint32_t address,uint64_t a0,uint64_t a1,uint64_t a2,uint64_t a3,uint64_t *result){
 memset(c->r,0,sizeof c->r);c->r[4].ud[0]=a0;c->r[5].ud[0]=a1;c->r[6].ud[0]=a2;c->r[7].ud[0]=a3;c->r[29].uw[0]=STACK;
 c->lookup=recomp_lookup;c->end_reason=0;jmp_buf jump;c->exit_jmp=&jump;ee_fpu_enter();
 if(setjmp(jump)){ee_fpu_leave();fprintf(stderr,"original %08x stopped: %s\n",address,c->end_detail);return 0;}
 ee_fn fn=recomp_lookup(address);if(!fn){ee_fpu_leave();return 0;}fn(c);ee_fpu_leave();*result=c->r[2].ud[0];return 1;
}
int main(int argc,char **argv){
 if(argc!=6 && argc!=7){fprintf(stderr,"usage: gtmode_sale_oracle RAM SPR ISO PS2-CARD NATIVE-SAVE [ACTUAL-AFTER-RAM]\n");return 2;}
 ee_ctx c;if(ee_init(&c) || !load(argv[1],c.ram,EE_RAM_SIZE) || !load(argv[2],c.spr,EE_SPR_SIZE) || !gt4_game_load_disc(argv[3]))return 2;
 const uint32_t entries[]={0x4411f0,0x435860,0x43afb0,0x42f218,0x4458c8};FILE *code=fopen("extracted/core.bin","rb");if(!code)return 2;
 for(unsigned i=0;i<5;i++){uint8_t bytes[32];if(fseek(code,entries[i]-0x100000,SEEK_SET) || fread(bytes,1,32,code)!=32 || memcmp(bytes,c.ram+entries[i],32))return 2;}fclose(code);
 ah_vm *vm=ah_vm_new();ah_obj *game=ah_module_new(vm,"game",ah_root(vm),NULL,NULL);char error[256];
 if(!gt4_ps2_read(argv[4],vm,game,error,sizeof error)){fprintf(stderr,"import: %s\n",error);return 2;}gt4_game_bind(vm,game);
 ah_obj *garage=get(vm,game,"garage").u.o,*car=ah_as_obj(get(vm,garage,"riding_car"),AO_MODULE),*calendar=get(vm,game,"calendar").u.o;
 if(!car)return 2;int64_t slot=get(vm,garage,"riding_index").u.i;uint32_t id=(uint32_t)get(vm,car,"code").u.i;
 if(id!=r32(c.ram+CAR+8) || (uint64_t)get(vm,game,"money").u.i!=gt4_career_money_get(c.ram+PROFILE) || (uint32_t)get(vm,car,"odometer").u.i!=r32(c.ram+CAR+0x4a0))return 2;
 const char *err=NULL;gt4_vol *vol=gt4_vol_open(argv[3],&err);if(!vol)return 2;catalog cat={0};size_t dbsize=0;
 cat.labels=gt4_vol_read(vol,"/specdb/GT4_US2560/GENERIC_CAR.idi",&cat.size);uint8_t *db=gt4_vol_read(vol,"/specdb/GT4_US2560/GENERIC_CAR.dbt",&dbsize),row[24];
 const char *label=gt4_specdb_label_for_id(cat.labels,cat.size,id);if(!label || gt4_specdb_row(db,dbsize,id,row,24)!=24)return 2;
 ah_set_member(vm,car,"car_code",ah_str(vm,label));uint32_t bits=r32(row+8);int32_t base;memcpy(&base,&bits,4);ah_set_member(vm,car,"original_base_price",ah_int(base));gt4_game_bind_car(vm,car);
 ah_set_member(vm,calendar,"_getCarKey",ah_native(vm,"key",key,&cat));
 uint8_t *expected=malloc(EE_RAM_SIZE);if(!expected)return 2;memcpy(expected,c.ram,EE_RAM_SIZE);
 ah_obj *history=get(vm,calendar,"original_events").u.o;unsigned initial=r32(c.ram+CALENDAR+0x10),first=r32(c.ram+CALENDAR+0xc);
 if((unsigned)ah_array_size(history)!=initial)return 2;
 for(unsigned i=0;i<initial;i++){ah_obj *event=ah_array_get(history,i).u.o;for(unsigned j=0;j<16;j++)if(ah_array_get(event,j).u.i!=c.ram[CALENDAR+0x18+((first+i)%2922)*16+j])return 1;}
 ah_val price,color,ret,index=ah_int(slot);
 if(ah_call_member(vm,car,"getUsedSellPrice",0,NULL,&price)<1 || ah_call_member(vm,car,"getColorIndex",0,NULL,&color)<1)return 1;
 uint64_t original;if(!call(&c,0x4411f0,CAR,0,0,0,&original) || (int64_t)original!=price.u.i)return 1;
 uint32_t setup=r32(c.ram+CAR+0x490);if(setup>=3 || !call(&c,0x4458c8,CAR+8+setup*0x178,0,0,0,&original) || (int64_t)original!=color.u.i)return 1;
 /* The exact order in GTmodeProject CarViewRoot's sell handler. */
 if(ah_call_member(vm,garage,"delCar",1,&index,&ret)<1 || !ah_truthy(ret))return 1;
 if(!call(&c,0x435860,GARAGE,slot,0,0,&original) || original!=1)return 1;
 if(ah_call_member(vm,game,"incMoney",1,&price,&ret)<1 || !call(&c,0x43afb0,PROFILE,(uint64_t)price.u.i,0,0,&original))return 1;
 ah_val args[]={get(vm,car,"car_code"),color,price};
 if(ah_call_member(vm,calendar,"putSellCarEvent",3,args,&ret)<1)return 1;
 strcpy((char *)c.ram+LABEL,label);memset(c.ram+STACK-32,0,8);
 if(!call(&c,0x42f218,CALENDAR,LABEL,(uint64_t)color.u.i,(uint64_t)price.u.i,&original))return 1;
 /* Overlay only native state changes on the original baseline; every other
    garage/calendar byte must remain unchanged. ABI scratch stack excluded. */
 ah_obj *vacant=get(vm,garage,"vacant_slots").u.o;char slot_name[16];snprintf(slot_name,sizeof slot_name,"%lld",(long long)slot);
 ah_obj *removed=get(vm,vacant,slot_name).u.o;w64(expected+GARAGE+0x10+slot*32,(uint64_t)get(vm,removed,"flags").u.i);w32(expected+GARAGE+0x81c8,(uint32_t)get(vm,garage,"riding_index").u.i);
 if(memcmp(c.ram+GARAGE,expected+GARAGE,0x81d0) || get(vm,garage,"riding_car").t!=AV_NIL || ah_array_get(get(vm,garage,"cars").u.o,(int)slot).t!=AV_NIL)return 1;
 /* Comparator control: a one-credit error must reject the transaction. */
 if(getenv("GT4_CAREER_TEST_NEGATIVE"))ah_set_member(vm,game,"money",ah_int(get(vm,game,"money").u.i+1));
 if((uint64_t)get(vm,game,"money").u.i!=gt4_career_money_get(c.ram+PROFILE))return 1;
 history=get(vm,calendar,"original_events").u.o;unsigned count=r32(c.ram+CALENDAR+0x10),oldnext=r32(expected+CALENDAR+8),oldfirst=r32(expected+CALENDAR+0xc);
 if((unsigned)ah_array_size(history)!=count)return 1;
 ah_obj *last=ah_array_get(history,ah_array_size(history)-1).u.o;for(unsigned j=0;j<16;j++)expected[CALENDAR+0x18+oldnext*16+j]=(uint8_t)ah_array_get(last,j).u.i;
 w32(expected+CALENDAR+8,(oldnext+1)%2922);w32(expected+CALENDAR+0xc,initial==2922?(oldfirst+1)%2922:oldfirst);w32(expected+CALENDAR+0x10,count);
 ah_obj *date=get(vm,calendar,"today").u.o;gt4_career_calendar_data dates;if(!gt4_career_calendar_data_load(argv[3],&dates))return 2;
 int32_t day=gt4_career_date_to_day(&dates,(int32_t)ah_array_get(date,0).u.i,(int32_t)ah_array_get(date,1).u.i,(int32_t)ah_array_get(date,2).u.i);
 w32(expected+CALENDAR+0xb6c0,(uint32_t)day);w32(expected+CALENDAR+0xb6c4,1);
 if(memcmp(c.ram+CALENDAR,expected+CALENDAR,0xb6c8))return 1;
 if(ah_call_member(vm,garage,"getCount",0,NULL,&ret)<1 || ret.u.i!=gt4_career_garage_count(c.ram+GARAGE))return 1;
 if(argc==7){
  uint8_t *after=malloc(EE_RAM_SIZE);if(!after || !load(argv[6],after,EE_RAM_SIZE))return 2;
  gt4_career_money_set(expected+PROFILE,(uint64_t)get(vm,game,"money").u.i);
  if(memcmp(after+GARAGE,expected+GARAGE,0x81d0) || memcmp(after+CALENDAR,expected+CALENDAR,0xb6c8) || gt4_career_money_get(after+PROFILE)!=(uint64_t)get(vm,game,"money").u.i){
   fprintf(stderr,"actual PCSX2 after-sale state differs\n");return 1;
  }
  free(after);puts("ACTUAL PCSX2 sale: native price/money, all garage 0x81d0 bytes, all calendar 0xb6c8 bytes identical to after-action snapshot");
 }
 uint8_t *packed=NULL;size_t size=0;if(!gt4_state_pack(vm,game,&packed,&size) || !gt4_save_write(argv[5],packed,size))return 1;free(packed);
 uint64_t stamp;if(gt4_save_read(argv[5],&packed,&size,&stamp)<1)return 1;
 ah_vm *restored=ah_vm_new();ah_obj *copy=ah_module_new(restored,"game",ah_root(restored),NULL,NULL);if(!gt4_state_replace(restored,copy,packed,size))return 1;free(packed);gt4_game_bind(restored,copy);
 ah_obj *cg=get(restored,copy,"garage").u.o,*cc=get(restored,copy,"calendar").u.o,*saved=get(restored,cc,"original_events").u.o;
 if(get(restored,copy,"money").u.i!=get(vm,game,"money").u.i || get(restored,cg,"riding_index").u.i!=-1 || get(restored,cg,"riding_car").t!=AV_NIL || ah_array_size(saved)!=ah_array_size(history))return 1;
 ah_obj *saved_date=get(restored,cc,"today").u.o;
 for(int i=0;i<3;i++)if(ah_array_get(saved_date,i).u.i!=ah_array_get(date,i).u.i)return 1;
 if(get(restored,cc,"elapsed_days").u.i!=get(vm,calendar,"elapsed_days").u.i || ah_call_member(restored,cg,"getCount",0,NULL,&ret)<1 || ret.u.i!=gt4_career_garage_count(expected+GARAGE))return 1;
 ah_obj *saved_vacant=get(restored,cg,"vacant_slots").u.o,*saved_removed=get(restored,saved_vacant,slot_name).u.o;
 const char *retained[]={"code","table","flags","odometer","variation","history"};
 for(unsigned i=0;i<6;i++)if(get(restored,saved_removed,retained[i]).u.i!=get(vm,removed,retained[i]).u.i)return 1;
 for(int i=0;i<ah_array_size(saved);i++)for(int j=0;j<16;j++)if(ah_array_get(ah_array_get(saved,i).u.o,j).u.i!=ah_array_get(ah_array_get(history,i).u.o,j).u.i)return 1;
 /* Full producer comparisons, including the exact retained stack padding.
    These are additional component cases, not additional actual user actions. */
 uint8_t *cal_seed=malloc(0xb6c8),*cal_expected=malloc(0xb6c8);if(!cal_seed || !cal_expected)return 2;
 memcpy(cal_seed,c.ram+CALENDAR,0xb6c8);uint32_t random=0x51aebabe;unsigned event_cases=0,color_cases=0;
 const int64_t limits[]={INT64_MIN,INT64_MAX,-1,0,1,65534,65535,65536,INT64_C(0x100000000)};
 for(unsigned i=0;i<1024;i++)for(int sell=0;sell<2;sell++){
  uint8_t event[16];for(unsigned j=0;j<16;j++){random=random*1664525u+1013904223u;event[j]=(uint8_t)(random>>24);}
  int64_t color_value=limits[i%9],detail=limits[(i/9)%9];
  memcpy(c.ram+CALENDAR,cal_seed,0xb6c8);memcpy(cal_expected,cal_seed,0xb6c8);
  memcpy(c.ram+STACK-(sell?32:48),event,16);
  gt4_career_calendar_car_event(event,sell,id,color_value,detail);gt4_career_calendar_append(cal_expected,event);
  if(!call(&c,sell?0x42f218:0x42f1b8,CALENDAR,LABEL,(uint64_t)color_value,(uint64_t)detail,&original) || memcmp(c.ram+CALENDAR,cal_expected,0xb6c8)){
   fprintf(stderr,"car calendar producer differs trial=%u sell=%d\n",i,sell);return 1;
  }
  event_cases++;
 }
 for(unsigned i=0;i<256;i++){
  c.ram[LABEL+0x100+0x18]=(uint8_t)i;ah_set_member(vm,car,"color_index",ah_int(i));
  if(ah_call_member(vm,car,"getColorIndex",0,NULL,&ret)<1 || !call(&c,0x4458c8,LABEL+0x100,0,0,0,&original) || ret.u.i!=(int64_t)original)return 1;color_cases++;
 }
 printf("car event producers %u/%u full calendar bytes; native color getter %u/%u identical.\n",event_cases,event_cases,color_cases,color_cases);
 free(cal_seed);free(cal_expected);
 printf("sale action identical: price %lld, color %lld, slot %lld, money %lld, garage bytes 0x81d0, calendar bytes 0xb6c8; native file save/load preserved. Reserved event bytes seeded zero; original boxed callbacks/UI execution excluded.\n",(long long)price.u.i,(long long)color.u.i,(long long)slot,(long long)get(vm,game,"money").u.i);
 ah_vm_free(restored);ah_vm_free(vm);free(expected);free(cat.labels);free(db);gt4_vol_close(vol);ee_free(&c);return 0;
}
