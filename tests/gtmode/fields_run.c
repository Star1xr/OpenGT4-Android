/* Tests original scalar instruction spans, not full Adhoc callbacks.
   Original jal to boxing is an expected stop after its delay slot. */
#include "ee.h"
#include "career_original.h"
#include "specdb.h"
#include "vol.h"
#include <stdlib.h>
#include <string.h>
ee_fn gtmode_field_0_lookup(uint32_t address);
ee_fn gtmode_field_1_lookup(uint32_t address);
ee_fn gtmode_field_2_lookup(uint32_t address);
ee_fn gtmode_field_3_lookup(uint32_t address);
ee_fn gtmode_field_4_lookup(uint32_t address);
ee_fn gtmode_field_5_lookup(uint32_t address);
ee_fn gtmode_field_6_lookup(uint32_t address);
ee_fn gtmode_field_7_lookup(uint32_t address);
ee_fn gtmode_field_8_lookup(uint32_t address);
ee_fn gtmode_field_9_lookup(uint32_t address);
ee_fn gtmode_field_10_lookup(uint32_t address);
ee_fn gtmode_field_11_lookup(uint32_t address);
ee_fn gtmode_field_12_lookup(uint32_t address);
ee_fn gtmode_field_13_lookup(uint32_t address);
ee_fn gtmode_field_14_lookup(uint32_t address);
ee_fn gtmode_field_15_lookup(uint32_t address);
ee_fn gtmode_money_compare_lookup(uint32_t address);
typedef ee_fn (*lookup_fn)(uint32_t);
static const lookup_fn lookups[]={gtmode_field_0_lookup,gtmode_field_1_lookup,gtmode_field_2_lookup,gtmode_field_3_lookup,gtmode_field_4_lookup,gtmode_field_5_lookup,gtmode_field_6_lookup,gtmode_field_7_lookup,gtmode_field_8_lookup,gtmode_field_9_lookup,gtmode_field_10_lookup,gtmode_field_11_lookup,gtmode_field_12_lookup,gtmode_field_13_lookup,gtmode_field_14_lookup,gtmode_field_15_lookup};
static const uint32_t start[]={0x1a16f0,0x1a1788,0x1a1820,0x1a18b8,0x1a1950,
 0x1a19e4,0x1a1a84,0x1a1b34,0x1a1bdc,0x1a2668,0x1a2e60,0x1a2dc8,0x1a293c,0x1a2720,0x1a2784,0x1a139c};
static const unsigned size[]={4,4,4,4,4,20,20,20,28,4,4,4,28,4,4,8};
static uint32_t r32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int load(const char *p,uint8_t *out,size_t n){FILE *f=fopen(p,"rb");if(!f)return 0;int ok=fread(out,1,n,f)==n && fgetc(f)==EOF;fclose(f);return ok;}
int main(int argc,char **argv){
 if(argc!=4)return 2;ee_ctx c;if(ee_init(&c))return 2;
 if(!load(argv[1],c.ram,EE_RAM_SIZE) || !load(argv[2],c.spr,EE_SPR_SIZE))return 2;
 FILE *code=fopen("extracted/core.bin","rb");if(!code)return 2;
 for(unsigned i=0;i<16;i++){uint8_t bytes[32];
  if(fseek(code,start[i]-0x100000,SEEK_SET) || fread(bytes,1,size[i],code)!=size[i] || memcmp(bytes,c.ram+start[i],size[i]))return 2;
 }
 uint8_t money_code[16];if(fseek(code,0x639f8,SEEK_SET) || fread(money_code,1,16,code)!=16 || memcmp(money_code,c.ram+0x1639f8,16))return 2;
 fclose(code);
 const char *error=NULL;gt4_vol *v=gt4_vol_open(argv[3],&error);size_t n=0;
 if(!v)return 2;uint8_t *db=gt4_vol_read(v,"/specdb/GT4_US2560/RACE.dbt",&n);gt4_vol_close(v);
 if(!db || n<16 || r32(db+12)!=151)return 2;unsigned count=r32(db+8),checks=0;uint32_t rng=0x42c0ffee;
 jmp_buf jump;c.exit_jmp=&jump;
 for(unsigned trial=0;trial<count+1024;trial++){
  uint8_t row[151];
  if(trial<count){if(gt4_specdb_row_at(db,n,trial,row,sizeof row)!=151)return 2;}
  else for(unsigned k=0;k<151;k++){rng=rng*1664525u+1013904223u;row[k]=(uint8_t)(rng>>24);}
  memcpy(c.ram+0x1edf010,row,151);
  for(unsigned field=0;field<16;field++){
   memset(c.r,0,sizeof c.r);c.r[3].uw[0]=0x1edf000;c.r[21].uw[0]=0x1edf010;c.r[29].uw[0]=0x1ef0000;
   c.end_reason=0;c.lookup=lookups[field];
   if(setjmp(jump) && !(c.end_reason==EE_END_UNRECOMPILED && c.end_pc==start[field]+size[field]-8) &&
       !(c.end_reason==EE_END_UNSUPPORTED && c.end_pc==start[field]+size[field] && !strcmp(c.end_detail,"fell off end of function"))){fprintf(stderr,"unsupported field %u: %s\n",field,c.end_detail);return 1;}
   if(!c.end_reason)lookups[field](start[field])(&c);
   int32_t value=gt4_career_race_field(row,(gt4_career_race_field_id)field);
   if(c.r[5].sd[0]!=value || memcmp(c.ram+0x1edf010,row,151)){
    fprintf(stderr,"field %u trial %u original=%lld C=%d\n",field,trial,(long long)c.r[5].sd[0],value);return 1;
   }checks++;
  }
 }
 printf("MRaceData scalar spans: %u/%u identical values and unchanged 151-byte input rows; 16 fields, %u disc rows + 1024 random rows; VM boxing/full memory excluded\n",checks,checks,count);
 const uint64_t edges[]={0,1,UINT64_MAX,INT64_MAX,UINT64_C(0x8000000000000000),10000,UINT64_C(0x7fffffff),UINT64_C(0xffffffff80000000)};
 unsigned money_cases=0;
 for(unsigned trial=0;trial<1088;trial++){
  uint64_t balance,cost;
  if(trial<64){balance=edges[trial/8];cost=edges[trial%8];}
  else {rng=rng*1664525u+1013904223u;balance=(uint64_t)rng<<32;rng=rng*1664525u+1013904223u;balance|=rng;
        rng=rng*1664525u+1013904223u;cost=(uint64_t)rng<<32;rng=rng*1664525u+1013904223u;cost|=rng;}
  int64_t signed_balance,signed_cost;memcpy(&signed_balance,&balance,8);memcpy(&signed_cost,&cost,8);
  memset(c.r,0,sizeof c.r);c.r[16].ud[0]=balance;c.r[2].ud[0]=cost;c.end_reason=0;c.lookup=gtmode_money_compare_lookup;
  if(setjmp(jump) && !(c.end_reason==EE_END_UNSUPPORTED && c.end_pc==0x163a08 && !strcmp(c.end_detail,"fell off end of function")))return 1;
  if(!c.end_reason)gtmode_money_compare_lookup(0x1639f8)(&c);
  if(c.r[5].sd[0]!=gt4_career_money_enough(signed_balance,signed_cost))return 1;money_cases++;
 }
 printf("isEnoughMoney signed comparison span: %u/%u identical values; VM argument conversion/boxing excluded\n",money_cases,money_cases);
 free(db);ee_free(&c);return 0;
}
