/* Original part selector/pricing vs disc-backed C. No original file-I/O HLE;
   the original counted CARS array is supplied at its loader output boundary. */
#include "ee.h"
#include "career_parts.h"
#include "career_original.h"
#include "specdb.h"
#include "vol.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t address);
#define OBJECT 0x1edc000u
#define LIST 0x1ee0000u
static uint32_t r32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t r64(const uint8_t *p){return r32(p)|(uint64_t)r32(p+4)<<32;}
static void w32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static int load(const char *p,uint8_t *out,size_t n){FILE *f=fopen(p,"rb");if(!f)return 0;int ok=fread(out,1,n,f)==n && fgetc(f)==EOF;fclose(f);return ok;}
static int call(ee_ctx *c,uint32_t address,int64_t type,int64_t stage,uint64_t *value){
 memset(c->r,0,sizeof c->r);c->r[4].uw[0]=OBJECT;c->r[5].sd[0]=type;c->r[6].sd[0]=stage;c->r[29].uw[0]=0x1ef0000;c->end_reason=0;
 jmp_buf jump;c->exit_jmp=&jump;
 if(setjmp(jump)){fprintf(stderr,"original %08x stopped at %08x: %s\n",address,c->end_pc,c->end_detail);return 0;}
 recomp_lookup(address)(c);*value=c->r[2].ud[0];return 1;
}
typedef struct {uint8_t *db[32];size_t sizes[32];gt4_career_part_data rules;} tables;
static int32_t price(tables *t,const uint8_t *list,size_t size,int64_t type,int64_t stage){
 uint64_t key=gt4_career_part_key(list,size,type,stage);if(key==UINT64_MAX || type<1 || type>31)return -1;
 uint32_t branch=t->rules.price_branches[(unsigned)type-1];if(branch==0x44a458 || branch==0x44a45c)return gt4_career_part_row_price(branch,NULL,0);
 unsigned ns=(unsigned)(key>>32);if(ns>=32)return -1;uint8_t row[4096];int n=gt4_specdb_row(t->db[ns],t->sizes[ns],(uint32_t)key&65535,row,sizeof row);
 return n>0?gt4_career_part_row_price(branch,row,(size_t)n):-1;
}
static int compare(ee_ctx *c,tables *t,const uint8_t *list,size_t size,int64_t type,int64_t stage,unsigned *keys,unsigned *prices){
 uint64_t original;if(!call(c,0x44a048,type,stage,&original) || original!=gt4_career_part_key(list,size,type,stage) || memcmp(c->ram+LIST,list,size)){
  fprintf(stderr,"selector differs type=%lld stage=%lld\n",(long long)type,(long long)stage);return 0;
 }(*keys)++;
 if(!call(c,0x44a2e0,type,stage,&original) || (int64_t)original!=price(t,list,size,type,stage) || memcmp(c->ram+LIST,list,size)){
  fprintf(stderr,"price differs type=%lld stage=%lld original=%lld C=%d\n",(long long)type,(long long)stage,(long long)original,price(t,list,size,type,stage));return 0;
 }(*prices)++;return 1;
}
static int metadata(ee_ctx *c,const uint8_t row[24],const gt4_career_part_data *rules,unsigned *checks){
 static const uint32_t entries[]={0x4488a0,0x448968,0x448908};
 const int64_t expected[]={gt4_career_car_year(row),gt4_career_car_open_type(row),gt4_career_car_category(row)};
 memcpy(c->ram+OBJECT,row,24);
 for(unsigned i=0;i<3;i++){
  uint64_t value;
  if(!call(c,entries[i],0,0,&value) || (int64_t)value!=expected[i] || memcmp(c->ram+OBJECT,row,24)){
   fprintf(stderr,"car metadata differs function=%08x\n",entries[i]);return 0;
  }
  (*checks)++;
 }
 uint64_t pointer;if(!call(c,0x448928,0,0,&pointer))return 0;
 const char *name=gt4_career_car_category_name(rules,row);
 if((pointer==0)!=(name==NULL) || (pointer && (pointer>=EE_RAM_SIZE-24 || strcmp((char *)c->ram+pointer,name))) || memcmp(c->ram+OBJECT,row,24)){
  fprintf(stderr,"category name differs\n");return 0;
 }
 (*checks)++;return 1;
}
int main(int argc,char **argv){
 if(argc!=4)return 2;ee_ctx c;if(ee_init(&c))return 2;if(!load(argv[1],c.ram,EE_RAM_SIZE) || !load(argv[2],c.spr,EE_SPR_SIZE))return 2;
 FILE *code=fopen("extracted/core.bin","rb");if(!code)return 2;
 const uint32_t entries[]={0x44a048,0x44a2e0,0x4488a0,0x448968,0x448908,0x448928};for(unsigned i=0;i<6;i++){uint8_t bytes[32];if(fseek(code,entries[i]-0x100000,SEEK_SET) || fread(bytes,1,32,code)!=32 || memcmp(bytes,c.ram+entries[i],32))return 2;}fclose(code);
 tables t={0};if(!gt4_career_part_data_load(argv[3],&t.rules))return 2;
 for(unsigned i=0;i<31;i++)if(t.rules.price_branches[i]!=r32(c.ram+0x6a7b60+i*4))return 2;
 const char *error=NULL;gt4_vol *v=gt4_vol_open(argv[3],&error);if(!v)return 2;
 for(unsigned ns=0;ns<32;ns++){char path[160];snprintf(path,sizeof path,"/specdb/GT4_US2560/%s.dbt",t.rules.table_names[ns]);t.db[ns]=gt4_vol_read(v,path,&t.sizes[ns]);if(!t.db[ns])return 2;}
 if(t.sizes[0]<16)return 2;unsigned cars=r32(t.db[0]+8),loaded=0,missing=0,keys=0,prices=0;
 c.lookup=recomp_lookup;
 unsigned fields=0;
 for(unsigned car=0;car<cars;car++){
  uint8_t row[24];uint32_t id=r32(t.db[0]+16+car*8);
  if(gt4_specdb_row(t.db[0],t.sizes[0],id,row,24)!=24 || !metadata(&c,row,&t.rules,&fields))return 1;
 }
 /* Exhaust all signed-halfword bit patterns; bytes span their entire domain. */
 for(unsigned bits=0;bits<65536;bits++){
  uint8_t row[24]={0};row[12]=(uint8_t)bits;row[13]=(uint8_t)(bits>>8);
  row[17]=(uint8_t)bits;row[20]=(uint8_t)(bits>>8);
  if(!metadata(&c,row,&t.rules,&fields))return 1;
 }
 printf("car year/open type/category key/name: %u/%u identical; unchanged 24-byte rows. Boxed callbacks excluded.\n",fields,fields);
 w32(c.ram+OBJECT+8,LIST);
 for(unsigned car=0;car<cars;car++){
  uint64_t key=r32(t.db[0]+16+car*8);char path[160];snprintf(path,sizeof path,"/specdb/GT4_US2560/CARS/%016llX",(unsigned long long)key);
  size_t n=0;uint8_t *list=gt4_vol_read(v,path,&n);if(!list){missing++;continue;}
  if(n<8 || n>0x10000 || r64(list)>INT32_MAX || r64(list)>(n-8)/16)return 2;loaded++;memcpy(c.ram+LIST,list,n);
  unsigned count=r32(list);for(unsigned i=0;i<count;i++){
   uint64_t part=r64(list+8+i*16);int32_t type;uint32_t bits=(uint32_t)(part>>32);memcpy(&type,&bits,4);
   if(!compare(&c,&t,list,n,type,list[16+i*16],&keys,&prices))return 1;
  }
  const int64_t edges[]={-1,0,1,30,31,32,INT32_MIN,INT32_MAX,INT64_MIN,INT64_MAX,INT64_C(0x10000001f)};
  for(unsigned i=0;i<11;i++)if(!compare(&c,&t,list,n,edges[i],255,&keys,&prices))return 1;
  free(list);
 }
 /* Empty list and full64 stage values must not wrap to a byte. */
 uint8_t empty[8]={0};memcpy(c.ram+LIST,empty,8);
 for(int64_t type=-1;type<=33;type++)if(!compare(&c,&t,empty,8,type,INT64_MAX,&keys,&prices))return 1;
 printf("parts selector %u/%u; price returns %u/%u identical; %u disc car lists, %u missing lists; unchanged counted arrays. Original file-I/O, boxed callback, full memory, purchase/effects excluded.\n",keys,keys,prices,prices,loaded,missing);
 for(unsigned i=0;i<32;i++)free(t.db[i]);gt4_vol_close(v);ee_free(&c);return 0;
}
