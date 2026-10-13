/* Test-only original calls versus the native Adhoc service flow and PS2 import. */
#include "ee.h"
#include "fpu.h"
#include "gtmode_game.h"
#include "career_float.h"
#include "save_ps2.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t address);
static int load(const char *path,uint8_t *p,size_t n){FILE *f=fopen(path,"rb");if(!f)return 0;int ok=fread(p,1,n,f)==n && fgetc(f)==EOF;fclose(f);return ok;}
static ah_val member(ah_vm *vm,ah_obj *o,const char *key){ah_val v=ah_nil();ah_get_member(vm,o,key,&v);return v;}
static uint32_t u32(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
static void sd(uint8_t *p,uint64_t value){memcpy(p,&value,8);}
int main(int argc,char **argv){
    if(argc!=5){fprintf(stderr,"usage: gtmode_service_oracle RAM SPR ISO PS2-CARD\n");return 2;}
    const uint32_t profile=0xa0cf58,wear=0xa209e8,stack=0x1ef0000,params=0x620320,settings=0x620cd8;
    ee_ctx c;if(ee_init(&c))return 2;
    uint8_t *ram=malloc(EE_RAM_SIZE),*spr=malloc(EE_SPR_SIZE),*expected=malloc(EE_RAM_SIZE);
    if(!ram || !spr || !expected || !load(argv[1],ram,EE_RAM_SIZE) || !load(argv[2],spr,EE_SPR_SIZE))return 2;
    const uint32_t ranges[][2]={{0x4359b0,0x4359e0},{0x4359e0,0x435a0c},{0x43af68,0x43af84},{0x43afb0,0x43aff0},{0x343928,0x343974},{0x343698,0x343704},{0x350678,0x350780},{0x3507d0,0x350804},{0x350808,0x350830},{0x435860,0x4358c4},{0x434e08,0x434e1c}};
    FILE *f=fopen("extracted/core.bin","rb");if(!f)return 2;
    for(unsigned i=0;i<sizeof ranges/sizeof *ranges;i++){uint8_t code[512];size_t n=ranges[i][1]-ranges[i][0];if(fseek(f,ranges[i][0]-0x100000,SEEK_SET) || fread(code,1,n,f)!=n || memcmp(code,ram+ranges[i][0],n))return 2;}fclose(f);
    if(!gt4_game_load_disc(argv[3])){fprintf(stderr,"disc career data unavailable\n");return 2;}
    ah_vm *vm=ah_vm_new();ah_obj *game=ah_module_new(vm,"game",ah_root(vm),NULL,NULL);char error[256];
    if(!gt4_ps2_read(argv[4],vm,game,error,sizeof error)){fprintf(stderr,"import: %s\n",error);return 2;}
    gt4_game_bind(vm,game);ah_obj *garage=member(vm,game,"garage").u.o,*car=ah_as_obj(member(vm,garage,"riding_car"),AO_MODULE);
    if(!car){fprintf(stderr,"save has no selected car\n");return 2;}
    if((uint32_t)member(vm,car,"code").u.i!=u32(ram+0xa20548+8) || member(vm,car,"table").u.i!=0 || (uint32_t)member(vm,car,"odometer").u.i!=u32(ram+wear)){
        fprintf(stderr,"imported car key/mileage differs from captured current car\n");return 1;
    }
    uint64_t balance=(uint64_t)member(vm,game,"money").u.i;ah_obj *state=ah_as_obj(member(vm,car,"oil_state"),AO_ARRAY);
    if(balance!=gt4_career_money_get(ram+profile) || ah_array_size(state)!=32){fprintf(stderr,"card and capture do not match (money %llu versus %llu)\n",(unsigned long long)balance,(unsigned long long)gt4_career_money_get(ram+profile));return 2;}
    for(int i=0;i<32;i++)if(ah_array_get(state,i).u.i!=ram[wear+i]){fprintf(stderr,"imported oil state differs at byte %d\n",i);return 1;}
    ah_obj *race_records=ah_as_obj(member(vm,member(vm,game,"race_record").u.o,"records"),AO_ARRAY);
    if(ah_array_size(race_records)!=768)return 1;
    for(int i=0;i<768;i++){
        ah_obj *record=ah_as_obj(ah_array_get(race_records,i),AO_MODULE);
        ah_val word=member(vm,record,"original_record");
        if((word.t!=AV_INT && word.t!=AV_LONG) || (uint32_t)word.u.i!=u32(ram+profile+0x13ae0+4+i*4)){
            fprintf(stderr,"imported race record %d differs from live captured record: %08x/%08x\n",i,(uint32_t)word.u.i,u32(ram+profile+0x13ae0+4+i*4));return 1;
        }
    }
    puts("PS2 import: 768/768 complete 32-bit race records identical to captured GT Mode state");
    memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);memcpy(expected,ram,EE_RAM_SIZE);c.lookup=recomp_lookup;jmp_buf jump;c.exit_jmp=&jump;
    /* Authored OilChangeRoot: incMoney(-50), then riding_car.replaceOil(). */
    ah_val cost=ah_int(-50),ret;
    if(ah_call_member(vm,game,"incMoney",1,&cost,&ret)<1 || ah_call_member(vm,car,"replaceOil",0,NULL,&ret)<1)return 1;
    gt4_career_money_set(expected+profile,(uint64_t)member(vm,game,"money").u.i);
    state=ah_as_obj(member(vm,car,"oil_state"),AO_ARRAY);for(int i=0;i<32;i++)expected[wear+i]=(uint8_t)ah_array_get(state,i).u.i;
    /* Compare the original lazy global initialization separately from the
       native disc-backed cache. Neither supplies bytes from original output. */
    uint8_t scratch_wear[32];memcpy(scratch_wear,ram+wear,32);gt4_career_replace_oil(scratch_wear,expected+params,expected+settings);
    if(memcmp(scratch_wear,expected+wear,32)){fprintf(stderr,"disc-backed oil settings differ from captured settings\n");return 1;}
    memset(c.r,0,sizeof c.r);c.r[4].uw[0]=profile;c.r[5].ud[0]=(uint64_t)(int64_t)-50;c.r[29].uw[0]=stack;
    if(setjmp(jump))return 1;recomp_lookup(0x43afb0)(&c);
    sd(expected+stack-32,0);sd(expected+stack-24,0);sd(expected+stack-16,0);sd(expected+stack-48,0x43afd0);
    memset(c.r,0,sizeof c.r);c.r[4].uw[0]=wear;c.r[29].uw[0]=stack;c.fpr[20].u=0;
    sd(expected+stack-16,0);sd(expected+stack-8,0);
    if(!ram[wear+0x14]){
        uint32_t value=career_f32_mul(u32(expected+params+20),0x40400000u);int clamped=career_f32_lt(u32(expected+params+28),value);
        sd(expected+stack-32,0);sd(expected+stack-24,0);sd(expected+stack-16,0);sd(expected+stack-48,params);sd(expected+stack-40,clamped?0x3436e0:0x3436cc);
    }
    ee_fpu_enter();if(setjmp(jump)){ee_fpu_leave();return 1;}recomp_lookup(0x343928)(&c);ee_fpu_leave();
    if(memcmp(c.ram,expected,EE_RAM_SIZE) || memcmp(c.spr,spr,EE_SPR_SIZE)){
        for(uint32_t i=0;i<EE_RAM_SIZE;i++)if(c.ram[i]!=expected[i]){fprintf(stderr,"service mismatch at %08x: %02x/%02x\n",i,c.ram[i],expected[i]);break;}return 1;
    }
    puts("PS2 import + native Adhoc oil service: 1/1 identical full RAM/SPR; money and 32 wear bytes match original calls");
    uint8_t *packed=NULL;size_t packed_size=0;
    if(!gt4_state_pack(vm,game,&packed,&packed_size))return 1;
    ah_obj *restored=ah_module_new(vm,"restored",ah_root(vm),NULL,NULL);
    if(!gt4_state_replace(vm,restored,packed,packed_size))return 1;gt4_game_bind(vm,restored);
    ah_obj *restored_garage=member(vm,restored,"garage").u.o,*restored_car=member(vm,restored_garage,"riding_car").u.o;
    ah_obj *restored_wear=ah_as_obj(member(vm,restored_car,"oil_state"),AO_ARRAY);
    if((uint64_t)member(vm,restored,"money").u.i!=gt4_career_money_get(c.ram+profile) || ah_array_size(restored_wear)!=32)return 1;
    for(int i=0;i<32;i++)if(ah_array_get(restored_wear,i).u.i!=c.ram[wear+i])return 1;
    ah_obj *restored_records=ah_as_obj(member(vm,member(vm,restored,"race_record").u.o,"records"),AO_ARRAY);
    if(ah_array_size(restored_records)!=768)return 1;
    for(int i=0;i<768;i++)if((uint32_t)member(vm,ah_array_get(restored_records,i).u.o,"original_record").u.i!=u32(ram+profile+0x13ae0+4+i*4))return 1;
    free(packed);puts("native save round-trip: post-service money, all 32 wear bytes and all 768 original record words preserved");
    int32_t selected=(int32_t)member(vm,garage,"riding_index").u.i;ah_val slot=ah_int(selected);
    if(ah_call_member(vm,garage,"delCar",1,&slot,&ret)<1 || !ah_truthy(ret))return 1;
    ah_obj *vacant=ah_as_obj(member(vm,garage,"vacant_slots"),AO_MODULE);char key[16];snprintf(key,sizeof key,"%d",selected);
    ah_obj *removed=member(vm,vacant,key).u.o;uint64_t flags=(uint64_t)member(vm,removed,"flags").u.i;
    memcpy(expected+profile+0xb8e8+0x10+(uint32_t)selected*32,&flags,8);memset(expected+profile+0xb8e8+0x81c8,255,4);
    memset(c.r,0,sizeof c.r);c.r[4].uw[0]=profile+0xb8e8;c.r[5].uw[0]=(uint32_t)selected;c.r[29].uw[0]=stack;
    sd(expected+stack-32,0);sd(expected+stack-24,0);sd(expected+stack-16,0);
    if(setjmp(jump))return 1;recomp_lookup(0x435860)(&c);
    if(c.r[2].uw[0]!=1 || memcmp(c.ram,expected,EE_RAM_SIZE) || memcmp(c.spr,spr,EE_SPR_SIZE))return 1;
    if(ah_call_member(vm,garage,"getCount",0,NULL,&ret)<1 || ret.u.i!=0 || member(vm,garage,"riding_index").u.i!=-1)return 1;
    puts("native Adhoc selected-car deletion: 1/1 identical full RAM/SPR, retained metadata and cleared selection");
    ah_vm_free(vm);free(expected);free(spr);free(ram);ee_free(&c);return 0;
}
