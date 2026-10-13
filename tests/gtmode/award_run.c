/* Original calendar/present functions versus production C and save bindings.
   Generated code is confined to this test oracle. */
#include "ee.h"
#include "career_calendar.h"
#include "career_race_lookup.h"
#include "career_blob.h"
#include "gtmode_game.h"
#include "save.h"
#include "specdb.h"
#include "vol.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
ee_fn award_stat_lookup_0(uint32_t);
ee_fn award_stat_lookup_1(uint32_t);
static void boxed_cleanup(ee_ctx *c){(void)c;}
static ee_fn lookup(uint32_t pc){
    if(pc==0x15f388)return boxed_cleanup;
    ee_fn f=award_stat_lookup_0(pc);if(!f)f=award_stat_lookup_1(pc);return f?f:recomp_lookup(pc);
}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"award oracle line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t rd(const uint8_t *p){uint32_t x;memcpy(&x,p,4);return x;}
static void wr(uint8_t *p,uint32_t x){memcpy(p,&x,4);}
static int load(const char *name,uint8_t *p,size_t n){FILE*f=fopen(name,"rb");if(!f)return 0;int ok=fread(p,1,n,f)==n && fgetc(f)==EOF;fclose(f);return ok;}
static int invoke(ee_ctx *c,uint32_t fn,uint64_t a,uint64_t b,uint64_t d,uint64_t e,uint64_t f){
    memset(c->r,0,sizeof c->r);c->r[4].ud[0]=a;c->r[5].ud[0]=b;c->r[6].ud[0]=d;c->r[7].ud[0]=e;c->r[8].ud[0]=f;c->r[29].uw[0]=0x1ef0000;
    jmp_buf jump;c->exit_jmp=&jump;c->end_reason=0;if(setjmp(jump)){fprintf(stderr,"original %08x: %s\n",fn,c->end_detail);return 0;}
    ee_fn p=c->lookup(fn);if(!p)return 0;p(c);return 1;
}
static ah_val get(ah_vm *vm,ah_obj *o,const char *name){ah_val v=ah_nil();ah_get_member(vm,o,name,&v);return v;}
static int resolve(ah_vm *vm,ah_obj *self,void *data,int n,const ah_val *args,ah_val *ret){(void)vm;(void)self;(void)args;if(n!=1)return -1;*ret=ah_int((int64_t)*(uint64_t*)data);return 0;}
int main(int argc,char **argv){
    if(argc!=4){fprintf(stderr,"usage: gtmode_award_oracle RAM SPR ISO\n");return 2;}
    ee_ctx c;CHECK(!ee_init(&c));uint8_t *ram=malloc(EE_RAM_SIZE),*spr=malloc(EE_SPR_SIZE);CHECK(ram && spr && load(argv[1],ram,EE_RAM_SIZE) && load(argv[2],spr,EE_SPR_SIZE));c.lookup=lookup;
    const uint32_t funcs[]={0x42f270,0x42f2e0,0x439d70,0x439d98,0x439de0,0x439e00,0x439e18,0x431cb8};
    FILE *original=fopen("extracted/core.bin","rb");CHECK(original);
    for(unsigned i=0;i<sizeof funcs/sizeof *funcs;i++){uint8_t instructions[24];CHECK(!fseek(original,funcs[i]-0x100000,SEEK_SET) && fread(instructions,1,24,original)==24 && !memcmp(ram+funcs[i],instructions,24));}fclose(original);
    gt4_career_calendar_data dates;gt4_career_race_lookup_data rules;CHECK(gt4_career_calendar_data_load(argv[3],&dates) && gt4_career_race_lookup_data_load(argv[3],&rules) && gt4_game_load_disc(argv[3]));
    const char *err=NULL;gt4_vol *vol=gt4_vol_open(argv[3],&err);CHECK(vol);size_t ln=0,dn=0;uint8_t *labels=gt4_vol_read(vol,"/specdb/GT4_US2560/RACE.idi",&ln),*db=gt4_vol_read(vol,"/specdb/GT4_US2560/RACE.dbt",&dn);CHECK(labels && db);
    uint8_t *expected=malloc(0xb6c8);CHECK(expected);const uint32_t cal=0xa0d178,text=0x1ee0000,flags=0x1edd000;unsigned events=0,bits=0,enums=0;uint32_t random=0x927300;
    memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);
    for(unsigned i=0;i<17;i++){
        const char *name=i<15?rules.method_names[i]:i==15?"unknown":"";strcpy((char*)c.ram+text,name);
        CHECK(invoke(&c,0x431cb8,text,0,0,0,0) && c.r[2].uw[0]==gt4_career_race_method(&rules,name));enums++;
    }
    for(unsigned row=0;row<rd(db+8);row++){
        uint32_t id=rd(db+16+row*8);const char *name=gt4_specdb_label_for_id(labels,ln,id);CHECK(name);strcpy((char*)c.ram+text,name);
        uint64_t key=UINT64_C(0x2300000000)|id;
        for(unsigned license=0;license<2;license++){
            random=random*1664525u+1013904223u;uint32_t grade=row%16,method=row%15,prize=random;
            memcpy(c.ram+cal,ram+cal,0xb6c8);wr(c.ram+cal+8,row%2922);wr(c.ram+cal+12,(row+1)%2922);wr(c.ram+cal+16,row%3==0?2922:row%2922);
            memcpy(expected,c.ram+cal,0xb6c8);uint8_t event[16];memset(event,0xa7,16);c.ram[0x1ef0000-64+3]=0xa7;
            gt4_career_calendar_race_event(event,license,key,grade,method,prize);gt4_career_calendar_append(expected,event);
            CHECK(invoke(&c,license?0x42f2e0:0x42f270,cal,text,grade,method,prize));
            CHECK(!memcmp(expected,c.ram+cal,0xb6c8) && !memcmp(c.spr,spr,EE_SPR_SIZE));events++;
        }
    }
    for(unsigned bit=0;bit<928;bit++){
        uint8_t local[116];for(unsigned j=0;j<sizeof local;j++){random=random*1664525u+1013904223u;local[j]=(uint8_t)(random>>24);}
        memcpy(c.ram+flags+4,local,116);CHECK(invoke(&c,0x439d70,flags,bit,0,0,0));CHECK(c.r[2].uw[0]==(unsigned)gt4_career_present_get(local,bit));bits++;
        gt4_career_present_set(local,bit);CHECK(invoke(&c,bit<868?0x439de0:0x439e18,flags,bit<868?bit:bit-868,0,0,0));CHECK(!memcmp(c.ram+flags+4,local,116));bits++;
        CHECK(invoke(&c,bit<868?0x439d70:0x439e00,flags,bit<868?bit:bit-868,0,0,0) && c.r[2].uw[0]==1);bits++;
    }
    unsigned statistics=0;
    for(unsigned mode=0;mode<2;mode++)for(unsigned i=0;i<2048;i++){
        random=random*1664525u+1013904223u;uint64_t before=(uint64_t)random<<32;
        random=random*1664525u+1013904223u;before|=random;
        uint64_t delta=i<4?(i==0?UINT64_MAX:i==1?1:i==2?UINT64_C(0x8000000000000000):0):(uint64_t)random*UINT64_C(0xdeadbeef12345);
        memcpy(c.ram+flags+0x48,&before,8);wr(c.ram+flags+0x50,(uint32_t)before);
        memset(c.r,0,sizeof c.r);c.r[16].uw[0]=flags;c.r[2].ud[0]=delta;c.r[29].uw[0]=0x1ef0000;
        jmp_buf jump;c.exit_jmp=&jump;
        if(!setjmp(jump))lookup(mode?0x163ec4:0x163bfc)(&c);
        /* The original store executes in the jal delay slot. Stop at the
           explicitly excluded Adhoc cleanup call after that store. */
        if(c.end_pc!=(mode?0x163ed4u:0x163c0cu)){fprintf(stderr,"statistics stop %08x: %s\n",c.end_pc,c.end_detail);return 1;}
        if(mode)CHECK(rd(c.ram+flags+0x50)==gt4_career_prize_cars_add((uint32_t)before,delta));
        else {uint64_t actual;memcpy(&actual,c.ram+flags+0x48,8);CHECK(actual==gt4_career_credit_add(before,delta));}
        statistics++;
    }
    ah_vm *vm=ah_vm_new();ah_obj *game=ah_module_new(vm,"game",ah_root(vm),NULL,NULL);gt4_game_bind(vm,game);ah_obj *calendar=get(vm,game,"calendar").u.o,*present=get(vm,game,"present").u.o;
    uint64_t key=UINT64_C(0x2300000000)|rd(db+16);ah_set_member(vm,calendar,"_getRaceKey",ah_native(vm,"race key",resolve,&key));
    ah_obj *date=ah_array_new(vm);ah_array_push(vm,date,ah_int(2005));ah_array_push(vm,date,ah_int(4));ah_array_push(vm,date,ah_int(2));ah_set_member(vm,calendar,"today",ah_objv(date));
    uint8_t present_seed[116];memcpy(present_seed,ram+flags+4,116);CHECK(gt4_career_blob_set(vm,present,"bits_ps2",present_seed,116));
    ah_val result,args[4];args[0]=ah_int(867);CHECK(ah_call_member(vm,present,"setByRace",1,args,&result)==1);gt4_career_present_set(present_seed,867);
    args[0]=ah_int(59);CHECK(ah_call_member(vm,present,"setByTime",1,args,&result)==1);gt4_career_present_set(present_seed,927);
    args[0]=ah_int(-1);CHECK(ah_call_member(vm,game,"incTotalPrizeMoney",1,args,&result)==1 && ah_call_member(vm,game,"incTotalPrizeCars",1,args,&result)==1);
    args[0]=ah_int(2);CHECK(ah_call_member(vm,game,"incTotalPrizeMoney",1,args,&result)==1 && ah_call_member(vm,game,"incTotalPrizeCars",1,args,&result)==1);
    args[0]=ah_str(vm,gt4_specdb_label_for_id(labels,ln,(uint32_t)key));args[1]=ah_str(vm,rules.grade_names[9]);args[2]=ah_nil();args[3]=ah_int(-1);
    CHECK(ah_call_member(vm,calendar,"putRunRaceEvent",4,args,&result)==1);
    ah_obj *history=get(vm,calendar,"original_events").u.o;CHECK(ah_array_size(history)==1);uint8_t event[16]={0};gt4_career_calendar_race_event(event,0,key,rules.grade_values[9],14,UINT64_MAX);
    for(unsigned j=0;j<16;j++)CHECK(ah_array_get(ah_array_get(history,0).u.o,j).u.i==event[j]);
    CHECK(ah_call_member(vm,game,"getTotalPrizeMoney",0,NULL,&result)==1 && !strcmp(ah_cstr(result),"1"));CHECK(ah_call_member(vm,game,"getTotalPrizeCars",0,NULL,&result)==1 && result.u.i==1);
    uint8_t *packed=NULL;size_t size=0;CHECK(gt4_state_pack(vm,game,&packed,&size));ah_vm *copyvm=ah_vm_new();ah_obj *copy=ah_module_new(copyvm,"game",ah_root(copyvm),NULL,NULL);CHECK(gt4_state_replace(copyvm,copy,packed,size));gt4_game_bind(copyvm,copy);uint8_t saved[116];CHECK(gt4_career_blob_get(copyvm,get(copyvm,copy,"present").u.o,"bits_ps2",saved,116) && !memcmp(saved,present_seed,116));CHECK(ah_array_size(get(copyvm,get(copyvm,copy,"calendar").u.o,"original_events").u.o)==1);
    CHECK(ah_call_member(copyvm,copy,"getTotalPrizeMoney",0,NULL,&result)==1 && !strcmp(ah_cstr(result),"1"));CHECK(ah_call_member(copyvm,copy,"getTotalPrizeCars",0,NULL,&result)==1 && result.u.i==1);
    printf("PASS race/license calendar %u/%u whole blocks; present flags %u/%u; method enum %u/%u; statistics %u/%u original instruction spans; native binding and save round trip.\n",events,events,bits,bits,enums,enums,statistics,statistics);
    free(packed);ah_vm_free(copyvm);ah_vm_free(vm);free(expected);free(db);free(labels);gt4_vol_close(vol);free(spr);free(ram);ee_free(&c);return 0;
}
