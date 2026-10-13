/* Real native record attributes/methods vs original EE primitives/spans.
   Scope: packed word, getter values, native save round trip; not boxed EE VM. */
#include "ee.h"
#include "gtmode_game.h"
#include "save.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t address);
ee_fn gtmode_attribute_0_lookup(uint32_t address);
ee_fn gtmode_attribute_1_lookup(uint32_t address);
ee_fn gtmode_attribute_2_lookup(uint32_t address);
ee_fn gtmode_attribute_3_lookup(uint32_t address);
#define WORD 0x1edf000u
#define LABEL 0x1ee0000u
typedef ee_fn (*lookup_fn)(uint32_t);
static const lookup_fn spans[]={gtmode_attribute_0_lookup,gtmode_attribute_1_lookup,gtmode_attribute_2_lookup,gtmode_attribute_3_lookup};
static const uint32_t begins[]={0x4309e8,0x1a4d88,0x1a4fa0,0x1a51b8};
static const unsigned sizes[]={28,28,28,32};
static int load(const char *p,uint8_t *out,size_t n){FILE *f=fopen(p,"rb");if(!f)return 0;int ok=fread(out,1,n,f)==n && fgetc(f)==EOF;fclose(f);return ok;}
static uint32_t r32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void w32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static int call(ee_ctx *c,uint32_t address,uint64_t a0,uint64_t a1,uint64_t a2,uint64_t *result){
 memset(c->r,0,sizeof c->r);c->r[4].ud[0]=a0;c->r[5].ud[0]=a1;c->r[6].ud[0]=a2;c->r[29].uw[0]=0x1ef0000;
 c->lookup=recomp_lookup;c->end_reason=0;jmp_buf jump;c->exit_jmp=&jump;
 if(setjmp(jump)){fprintf(stderr,"original %08x stopped: %s\n",address,c->end_detail);return 0;}
 recomp_lookup(address)(c);*result=c->r[2].ud[0];return 1;
}
static ah_val get(ah_vm *vm,ah_obj *unit,const char *field){ah_val v;ah_get_member(vm,unit,field,&v);return v;}
static int original_name(ee_ctx *c,uint32_t address,uint64_t input,ah_val actual){
 uint64_t pointer;if(!call(c,address,input,0,0,&pointer))return 0;
 const char *name=ah_cstr(actual);return pointer?pointer<EE_RAM_SIZE && name && !strcmp((const char *)c->ram+(uint32_t)pointer,name):actual.t==AV_NIL;
}
static unsigned dirtied;
static int mark_dirty(ah_vm *vm,ah_obj *self,void *user,int argc,const ah_val *argv,ah_val *ret){
 (void)vm;(void)self;(void)user;(void)argc;(void)argv;dirtied++;*ret=ah_nil();return 0;
}
static int fixture_index(ah_vm *vm,ah_obj *self,void *user,int argc,const ah_val *argv,ah_val *ret){
 (void)vm;(void)self;(void)user;const char *name=argc?ah_cstr(argv[0]):NULL;*ret=ah_int(name && !strcmp(name,"fixture")?0:-1);return 0;
}
int main(int argc,char **argv){
 if(argc!=4)return 2;ee_ctx c;if(ee_init(&c))return 2;
 if(!load(argv[1],c.ram,EE_RAM_SIZE) || !load(argv[2],c.spr,EE_SPR_SIZE))return 2;
 FILE *code=fopen("extracted/core.bin","rb");if(!code)return 2;
 for(unsigned i=0;i<4;i++){uint8_t bytes[32];if(fseek(code,begins[i]-0x100000,SEEK_SET) || fread(bytes,1,sizes[i],code)!=sizes[i] || memcmp(bytes,c.ram+begins[i],sizes[i]))return 2;}
 const uint32_t entries[]={0x430990,0x4309b0,0x431c38,0x430a58,0x430a20,0x430a10};
 for(unsigned i=0;i<6;i++){uint8_t bytes[16];if(fseek(code,entries[i]-0x100000,SEEK_SET) || fread(bytes,1,16,code)!=16 || memcmp(bytes,c.ram+entries[i],16))return 2;}fclose(code);
 gt4_career_race_lookup_data rules;if(!gt4_career_race_lookup_data_load(argv[3],&rules))return 2;gt4_game_set_race_lookup_data(&rules);
 ah_vm *vm=ah_vm_new();ah_obj *game=ah_module_new(vm,"game",ah_root(vm),NULL,NULL);ah_set_member(vm,ah_root(vm),"game",ah_objv(game));gt4_game_bind(vm,game);
 ah_obj *record=get(vm,game,"race_record").u.o,*records=get(vm,record,"records").u.o;
 if(ah_array_size(records)!=768)return 1;
 for(unsigned i=0;i<768;i++)if(get(vm,ah_array_get(records,i).u.o,"original_record").u.i!=0xfff0)return 1;
 ah_set_member(vm,record,"_getRecordIndex",ah_native(vm,"index",fixture_index,NULL));
 ah_set_member(vm,record,"_markDirty",ah_native(vm,"dirty",mark_dirty,NULL));
 ah_val label=ah_str(vm,"fixture"),found;if(ah_call_member(vm,record,"getUnit",1,&label,&found)<1 || found.u.o!=ah_array_get(records,0).u.o)return 1;
 ah_obj *unit=found.u.o;
 label=ah_str(vm,"missing");if(ah_call_member(vm,record,"getUnit",1,&label,&found)<1 || found.t!=AV_NIL)return 1;
 ah_set_member(vm,unit,"result",ah_str(vm,rules.grade_names[0]));if(dirtied!=1)return 1;
 ah_set_member(vm,unit,"result",ah_str(vm,rules.grade_names[0]));if(dirtied!=1)return 1;
 ah_set_member(vm,unit,"best_score",ah_int(200));if(get(vm,unit,"best_score").u.i!=0 || dirtied!=1)return 1;
 /* Legacy PC license objects migrate without overwriting imported words. */
 ah_obj *legacy=ah_module_new(vm,"legacy",NULL,NULL,NULL);ah_set_member(vm,legacy,"result",ah_str(vm,rules.grade_names[9]));gt4_game_bind_record_unit(vm,legacy);
 if((uint32_t)get(vm,legacy,"original_record").u.i!=0xff91)return 1;
 const char *fields[]={"type","result","present_result","spot_result"};uint32_t rng=0x4309d0;unsigned attrs=0,getters=0,updates=0,scores=0,refreshes=0;
 const int64_t edges[]={INT64_MIN,INT64_MAX,INT32_MIN,INT32_MAX,-256,-255,-1,0,254,255,256,INT64_C(0x100000000),-INT64_C(0x100000000)};
 for(unsigned trial=0;trial<4096;trial++){
  rng=rng*1664525u+1013904223u;uint32_t word=rng;ah_set_member(vm,unit,"original_record",ah_int(word&~12u));w32(c.ram+WORD,word&~12u);
  if(!original_name(&c,0x4309b0,WORD,get(vm,unit,"type")))return 1;getters++;
  ah_set_member(vm,unit,"original_record",ah_int(word));
  for(unsigned field=1;field<4;field++){if(!original_name(&c,0x431c38,(word>>(field*4))&15u,get(vm,unit,fields[field])))return 1;getters++;}
  if(get(vm,unit,"best_score").u.i!=((word>>16)&255u))return 1;
  for(unsigned field=0;field<4;field++){
   const char *name=field?(trial%17<16?rules.grade_names[trial%17]:"unknown"):(trial%5<4?rules.type_names[trial%5]:"unknown");
   uint32_t value=field?gt4_career_result_grade(&rules,name):(uint32_t)gt4_career_record_type(&rules,name);
   uint8_t before[16];memset(before,0xa5,16);w32(before,word);memcpy(c.ram+WORD,before,16);
   memset(c.r,0,sizeof c.r);c.r[2].sd[0]=field?(int64_t)value:(int64_t)(int32_t)value;c.r[16].uw[0]=WORD;c.r[17].uw[0]=WORD;c.r[29].uw[0]=0x1ef0000;
   c.lookup=spans[field];c.end_reason=0;jmp_buf jump;c.exit_jmp=&jump;
   if(setjmp(jump) && !(c.end_reason==EE_END_UNSUPPORTED && c.end_pc==begins[field]+sizes[field] && !strcmp(c.end_detail,"fell off end of function")))return 1;
   if(!c.end_reason)spans[field](begins[field])(&c);
   ah_set_member(vm,unit,"original_record",ah_int(word));ah_set_member(vm,unit,fields[field],ah_str(vm,name));
   w32(before,(uint32_t)get(vm,unit,"original_record").u.i);
   if(memcmp(before,c.ram+WORD,16)){fprintf(stderr,"attribute differs field %u trial %u\n",field,trial);return 1;}attrs++;
  }
  const char *type=trial%5<4?rules.type_names[trial%5]:"unknown",*grade=trial%17<16?rules.grade_names[trial%17]:"unknown";
  memcpy(c.ram+LABEL,type,strlen(type)+1);uint64_t kind,returned;
  if(!call(&c,0x430990,LABEL,0,0,&kind) || (int64_t)kind!=gt4_career_record_type(&rules,type))return 1;
  w32(c.ram+WORD,word);if(!call(&c,0x430a58,WORD,kind,gt4_career_result_grade(&rules,grade),&returned))return 1;
  ah_set_member(vm,unit,"original_record",ah_int(word));ah_val args[]={ah_str(vm,type),ah_str(vm,grade)},ret;
  if(ah_call_member(vm,unit,"update",2,args,&ret)<1 || (uint32_t)get(vm,unit,"original_record").u.i!=r32(c.ram+WORD))return 1;updates++;
  int64_t score=edges[trial%13];w32(c.ram+WORD,word);if(!call(&c,0x430a20,WORD,(uint64_t)score,0,&returned))return 1;
  ah_set_member(vm,unit,"original_record",ah_int(word));ah_val arg=ah_int(score);
  if(ah_call_member(vm,unit,"updateBestScore",1,&arg,&ret)<1 || (uint32_t)get(vm,unit,"original_record").u.i!=r32(c.ram+WORD))return 1;scores++;
  w32(c.ram+WORD,word);if(!call(&c,0x430a10,WORD,0,0,&returned))return 1;
  ah_set_member(vm,unit,"original_record",ah_int(word));if(ah_call_member(vm,unit,"refreshResult",0,NULL,&ret)<1 || (uint32_t)get(vm,unit,"original_record").u.i!=r32(c.ram+WORD))return 1;refreshes++;
 }
 /* Owned raw words must survive hooks, GC and save reconstruction. */
 uint8_t *bytes;size_t size;uint32_t expected=(uint32_t)get(vm,unit,"original_record").u.i;
 ah_gc(vm);if(!gt4_state_pack(vm,game,&bytes,&size))return 1;
 ah_vm *loaded=ah_vm_new();ah_obj *copy=ah_module_new(loaded,"game",ah_root(loaded),NULL,NULL);ah_set_member(loaded,ah_root(loaded),"game",ah_objv(copy));
 if(!gt4_state_merge(loaded,copy,bytes,size))return 1;free(bytes);gt4_game_bind(loaded,copy);
 ah_obj *loaded_record=get(loaded,copy,"race_record").u.o,*loaded_records=get(loaded,loaded_record,"records").u.o,*loaded_unit=ah_array_get(loaded_records,0).u.o;
 if(ah_array_size(loaded_records)!=768)return 1;
 if((uint32_t)get(loaded,loaded_unit,"original_record").u.i!=expected || get(loaded,loaded_unit,"best_score").u.i!=((expected>>16)&255u))return 1;
 ah_val result;if(ah_call_member(loaded,loaded_unit,"refreshResult",0,NULL,&result)<1)return 1;
 printf("record native attributes: %u/%u packed writes; %u/%u enum getter values; methods update %u/%u, score %u/%u, refresh %u/%u; GC/save round trip passed. Full boxed EE callbacks/full RAM/actions excluded.\n",attrs,attrs,getters,getters,updates,updates,scores,scores,refreshes,refreshes);
 ah_vm_free(loaded);ah_vm_free(vm);gt4_game_set_race_lookup_data(NULL);ee_free(&c);return 0;
}
