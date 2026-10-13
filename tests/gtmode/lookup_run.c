/* Original complete record-label lookup and license aggregate returns.
   No VM/HLE substitutions in the EE path. Full callback/memory equivalence
   and license acquisition actions are outside this harness's coverage. */
#include "ee.h"
#include "gtmode_game.h"
#include "specdb.h"
#include "vol.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t address);
#define RECORD 0xa20a38u
#define LABEL 0x1ee0000u
static uint32_t r32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void w32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static int load(const char *p,uint8_t *out,size_t n){FILE *f=fopen(p,"rb");if(!f)return 0;int ok=fread(out,1,n,f)==n && fgetc(f)==EOF;fclose(f);return ok;}
typedef struct {gt4_career_race_lookup_data rules;uint8_t *labels,*db;size_t labels_size,db_size;} tables;
static int resolve(ah_vm *vm,ah_obj *self,void *user,int argc,const ah_val *argv,ah_val *ret){
 (void)vm;(void)self;tables *t=user;const char *name=argc?ah_cstr(argv[0]):NULL;
 *ret=ah_int(gt4_career_race_record_index(&t->rules,t->labels,t->labels_size,t->db,t->db_size,name));return 0;
}
static int direct(ee_ctx *c,uint32_t address,const uint64_t args[4],uint64_t *value){
 memset(c->r,0,sizeof c->r);for(unsigned i=0;i<4;i++)c->r[4+i].ud[0]=args[i];c->r[29].uw[0]=0x1ef0000;
 c->end_reason=0;jmp_buf jump;c->exit_jmp=&jump;
 if(setjmp(jump)){fprintf(stderr,"original %08x stopped: %s\n",address,c->end_detail);return 0;}
 recomp_lookup(address)(c);*value=c->r[2].ud[0];return 1;
}
static int original(ee_ctx *c,uint32_t address,const char *label,unsigned grade,uint64_t *value){
 size_t n=strlen(label);if(n>127)return 0;memcpy(c->ram+LABEL,label,n+1);memset(c->r,0,sizeof c->r);
 c->r[4].uw[0]=RECORD;c->r[5].uw[0]=LABEL;c->r[6].uw[0]=grade;c->r[29].uw[0]=0x1ef0000;
 c->end_reason=0;jmp_buf jump;c->exit_jmp=&jump;
 if(setjmp(jump)){fprintf(stderr,"original %08x %s stopped: %s\n",address,label,c->end_detail);return 0;}
 recomp_lookup(address)(c);*value=c->r[2].ud[0];return 1;
}
int main(int argc,char **argv){
 if(argc!=4)return 2;ee_ctx c;if(ee_init(&c))return 2;
 if(!load(argv[1],c.ram,EE_RAM_SIZE) || !load(argv[2],c.spr,EE_SPR_SIZE))return 2;
 const uint32_t entries[]={0x430e58,0x431260,0x4312b0,0x4312d8,0x431300,0x447188,0x431328,0x431c60,0x5a2f88,0x447598};
 FILE *code=fopen("extracted/core.bin","rb");if(!code)return 2;
 for(unsigned i=0;i<10;i++){uint8_t bytes[32];if(fseek(code,entries[i]-0x100000,SEEK_SET) || fread(bytes,1,32,code)!=32 || memcmp(bytes,c.ram+entries[i],32))return 2;}fclose(code);
 tables t={0};if(!gt4_career_race_lookup_data_load(argv[3],&t.rules))return 2;
 if(memcmp(t.rules.prefixes[0],c.ram+0x68bb20,3) || memcmp(t.rules.prefixes[1],c.ram+0x692800,3))return 2;
 const char *error=NULL;gt4_vol *v=gt4_vol_open(argv[3],&error);if(!v)return 2;
 t.labels=gt4_vol_read(v,"/specdb/GT4_US2560/RACE.idi",&t.labels_size);
 t.db=gt4_vol_read(v,"/specdb/GT4_US2560/RACE.dbt",&t.db_size);gt4_vol_close(v);
 if(!t.labels || !t.db || t.db_size<16)return 2;
 uint8_t records[3076];memcpy(records,c.ram+RECORD,sizeof records);c.lookup=recomp_lookup;
 unsigned lookups=0,rows=0,count=r32(t.db+8);
 for(unsigned i=0;i<count;i++){
  const char *name=gt4_specdb_label_for_id(t.labels,t.labels_size,r32(t.db+16+i*8));if(!name)return 2;
  const char *variants[]={name,strlen(name)>2?name+2:name};
  for(unsigned k=0;k<2;k++){
   uint64_t pointer;if(!original(&c,0x430e58,variants[k],0,&pointer))return 1;
   int index=gt4_career_race_record_index(&t.rules,t.labels,t.labels_size,t.db,t.db_size,variants[k]);
   uint64_t expected=index<0?0:RECORD+4+(uint32_t)index*4;
   if(pointer!=expected || memcmp(records,c.ram+RECORD,sizeof records)){fprintf(stderr,"lookup differs for %s: original=%llx C=%llx\n",variants[k],(unsigned long long)pointer,(unsigned long long)expected);return 1;}
   lookups++;
   /* Execute the original load helper for the exact label and the two
      fallback candidates used by 001A0E48. Compare decoded row bytes. */
   uint8_t row[151];int loaded=0;char candidate[132];
   for(unsigned pass=0;pass<3 && !loaded;pass++){
    if(!pass)snprintf(candidate,sizeof candidate,"%s",variants[k]);
    else snprintf(candidate,sizeof candidate,"%s%s",t.rules.prefixes[pass-1],variants[k]);
    size_t length=strlen(candidate);memcpy(c.ram+LABEL,candidate,length+1);
    memset(c.ram+0x1edf000,0,0xb0);memset(c.ram+0x1edf098,255,16);
    memset(c.r,0,sizeof c.r);c.r[4].uw[0]=0x1edf000;c.r[5].uw[0]=LABEL;c.r[29].uw[0]=0x1ef0000;
    c.end_reason=0;jmp_buf jump;c.exit_jmp=&jump;
    if(setjmp(jump)){fprintf(stderr,"RACE load stopped: %s\n",c.end_detail);return 1;}
    recomp_lookup(0x447188)(&c);loaded=c.r[2].ud[0]!=0;
   }
   int native_loaded=gt4_career_race_row(&t.rules,t.labels,t.labels_size,t.db,t.db_size,variants[k],row);
   if(loaded!=native_loaded || (loaded && memcmp(row,c.ram+0x1edf000,151)))return 1;rows++;
  }
 }
 for(unsigned n=0;n<=100;n++){
  char missing[128];memset(missing,'z',n);missing[n]=0;uint64_t pointer;
  if(!original(&c,0x430e58,missing,0,&pointer) || pointer || gt4_career_race_record_index(&t.rules,t.labels,t.labels_size,t.db,t.db_size,missing)!=-1)return 1;lookups++;
 }
 unsigned points=0;
 for(unsigned trial=0;trial<257;trial++){
  for(unsigned rank=0;rank<6;rank++){
   gt4_career_race_lookup_data rules=t.rules;
   if(trial)rules.driver_points[rank]=(uint8_t)(trial-1);
   memcpy(c.ram+0x6a6ef0,rules.driver_points,6);
   uint64_t args[4]={RECORD,rank,0,0},value;
   if(!direct(&c,0x447598,args,&value) || (int64_t)value!=gt4_career_driver_points(&rules,rank))return 1;points++;
  }
 }
 memcpy(c.ram+0x6a6ef0,t.rules.driver_points,6);
 unsigned enums=0;
 for(unsigned i=0;i<17;i++){
  const char *name=i<16?t.rules.grade_names[i]:"unknown_result";
  memcpy(c.ram+LABEL,name,strlen(name)+1);uint64_t args[4]={LABEL,0,0,0},value;
  if(!direct(&c,0x431c60,args,&value) || value!=gt4_career_result_grade(&t.rules,name))return 1;enums++;
 }
 gt4_game_set_race_lookup_data(&t.rules);
 ah_vm *vm=ah_vm_new();ah_obj *game=ah_module_new(vm,"game",ah_root(vm),NULL,NULL);ah_set_member(vm,ah_root(vm),"game",ah_objv(game));gt4_game_bind(vm,game);
 ah_val record;ah_get_member(vm,game,"race_record",&record);ah_obj *native_records=ah_array_new(vm);
 for(unsigned i=0;i<768;i++)ah_array_push(vm,native_records,ah_objv(ah_module_new(vm,"record",NULL,NULL,NULL)));
 ah_set_member(vm,record.u.o,"records",ah_objv(native_records));
 ah_set_member(vm,record.u.o,"_getRecordIndex",ah_native(vm,"_getRecordIndex",resolve,&t));
 const char *categories[]={"l0b","l0a","l0c","l0d","l0s","missing","usl0b"};
 const char *methods[]={"isLicensePerfect","isLicensePassed","canTryLastLicense","isLicenseAllBronze","isLicenseAllSilver"};
 const uint32_t functions[]={0x431260,0x4312b0,0x4312d8,0x431300,0x431300};
 unsigned aggregates=0,missions=0,conversions=0;uint32_t rng=0x32540001;
 for(unsigned trial=0;trial<193;trial++){
  for(unsigned i=0;i<768;i++){
   uint32_t word;
   if(trial==0)word=r32(records+4+i*4);
   else if(trial<=64)word=((trial-1)/16)|(((trial-1)%16)<<4)|0x78900000u;
   else {rng=rng*1664525u+1013904223u;word=rng;}
   w32(c.ram+RECORD+4+i*4,word);ah_set_member(vm,ah_array_get(native_records,i).u.o,"original_record",ah_int(word));
  }
  uint8_t before[3076];memcpy(before,c.ram+RECORD,sizeof before);
  for(unsigned cat=0;cat<7;cat++)for(unsigned method=0;method<5;method++){
   uint64_t result;unsigned grade=method==4?10:11;
   if(!original(&c,functions[method],categories[cat],grade,&result))return 1;
   ah_val arg=ah_str(vm,categories[cat]),actual;
   if(ah_call_member(vm,record.u.o,methods[method],1,&arg,&actual)<1 || (uint64_t)ah_truthy(actual)!=result || memcmp(before,c.ram+RECORD,sizeof before)){
    fprintf(stderr,"aggregate differs: trial %u %s %s original=%llu\n",trial,categories[cat],methods[method],(unsigned long long)result);return 1;
   }aggregates++;
  }
  const char *ranges[]={"ignored", "1,10", "11,20", "21,34", "1,40", "4,4", "5,4", "0,0", "-1,1", " 1 , 2junk", "2147483648,2147483648"};
  for(unsigned range=0;range<11;range++){
   int32_t first=1,last=40;char text[64];snprintf(text,sizeof text,"%s",ranges[range]);char *comma=strchr(text,',');
   if(comma){*comma=0;
    const char *number[]={text,comma+1};int32_t *outputs[]={&first,&last};
    for(unsigned part=0;part<2;part++){
     memcpy(c.ram+LABEL,number[part],strlen(number[part])+1);uint64_t args[4]={LABEL,0,0,0},value;
     if(!direct(&c,0x5a2f88,args,&value))return 1;
     int64_t converted=strtoll(number[part],NULL,10);uint32_t low=(uint32_t)converted;
     if((uint32_t)value!=low)return 1;memcpy(outputs[part],&low,4);conversions++;
    }
   }
   for(unsigned threshold=0;threshold<4;threshold++){
    const char *names[]={"1","2","unknown_result"};unsigned grade=threshold<3?gt4_career_result_grade(&t.rules,names[threshold]):0;
    uint64_t args[4]={RECORD,grade,(uint64_t)(int64_t)first,(uint64_t)(int64_t)last},value;
    if(!direct(&c,0x431328,args,&value))return 1;
    ah_val arguments[]={ah_str(vm,ranges[range]),threshold<3?ah_str(vm,names[threshold]):ah_nil()},actual;
    if(ah_call_member(vm,record.u.o,"isMissionClear",threshold<3?2:1,arguments,&actual)<1 || (uint64_t)ah_truthy(actual)!=value || memcmp(before,c.ram+RECORD,sizeof before)){
     fprintf(stderr,"mission range differs: trial %u %s grade %u original=%llu\n",trial,ranges[range],grade,(unsigned long long)value);return 1;
    }missions++;
   }
  }
 }
 printf("original record lookups: %u/%u identical pointers/ordinals, unchanged records\n",lookups,lookups);
 printf("original RACE load helpers and constructor fallback candidates: %u/%u identical loaded status and row bytes; boxed Adhoc constructor/full memory excluded\n",rows,rows);
 printf("original license aggregates vs bound native handler: %u/%u identical returns, unchanged records; captured state, all type/grade pairs and 128 random states; full memory/VM callbacks/actions excluded\n",aggregates,aggregates);
 printf("mission range queries: %u/%u identical returns and unchanged records; %u enum lookups, %u integer conversions identical; boxed callback/full memory/actions excluded\n",missions,missions,enums,conversions);
 printf("championship driver points: %u/%u identical signed-byte returns; supported ranks 0..5 only; full memory/callback excluded\n",points,points);
 gt4_game_set_race_lookup_data(NULL);ah_vm_free(vm);free(t.db);free(t.labels);ee_free(&c);return 0;
}
