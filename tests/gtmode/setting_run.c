/* Original accessors execute from the user's CORE; VM allocation/refcount
   are test boundaries for range constructors, not replacement range logic. */
#include "ee.h"
#include "career_setting.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define CAR 0x1800000u
#define WRAP 0x1801000u
#define POOL 0x1803000u
#define CHECK(x) do{if(!(x)){fprintf(stderr,"setting oracle line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static const uint32_t getters[10][18]={{0x43ec30,0x43ec58},{0x43edc0,0x43ede8,0x43ee10,0x43ee38,0x43ee60,0x43ee88,0x43eeb0,0x43eed8,0x43ef00,0x43ef28,0x43ef50,0x43ef78,0x43efa0,0x43efc8,0x43eff0,0x43f018,0x43f040,0x43f068},{0x43f090,0x43f0b8,0x43f0e0,0x43f108,0x43f130,0x43f158},{0x43eb60,0x43eb60,0x43eb60,0x43eb60,0x43eb60,0x43eb60,0x43eb60,0x43eb60,0x43eb90,0x43ebb8,0x43ebe0},{0x43f248},{0x43ec08},{0x43f1f8,0x43f220},{0x43f1a8,0x43f1d0},{0x43f180},{0x43ec80,0x43eca8}};
static const uint32_t setters[10][18]={{0x43e3c0,0x43e3e8},{0x43e598,0x43e5c0,0x43e630,0x43e658,0x43e6c8,0x43e6f0,0x43e760,0x43e788,0x43e7b0,0x43e7d8,0x43e800,0x43e828,0x43e850,0x43e878,0x43e8a0,0x43e8c8,0x43e8f0,0x43e918},{0x43e940,0x43e968,0x43e990,0x43e9b8,0x43e9e0,0x43ea08},{0x43e2f0,0x43e2f0,0x43e2f0,0x43e2f0,0x43e2f0,0x43e2f0,0x43e2f0,0x43e2f0,0x43e320,0x43e348,0x43e370},{0x43eb38},{0x43e398},{0x43eae8,0x43eb10},{0x43ea58,0x43eaa0},{0x43ea30},{0x43e410,0x43e438}};
static const int counts[]={2,18,6,11,1,1,2,2,1,2};
static const uint32_t range_callbacks[]={0x1490b0,0x14b478,0x14aad8,0,0x14d638,0x14a3d0,0x14d278,0x14cbc8,0x14c898,0x14a6a0};
static void w32(uint8_t *p,uint32_t v){memcpy(p,&v,4);}
static uint32_t r32(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
static unsigned pool_at;
static void ref(ee_ctx *c){w32(c->ram+c->r[4].uw[0],WRAP);}
static void noop(ee_ctx *c){(void)c;}
static void arr(ee_ctx *c){memset(c->ram+POOL,0,0x300);w32(c->ram+c->r[4].uw[0],POOL);pool_at=POOL+0x300;}
static void elem(ee_ctx *c){c->r[2].uw[0]=POOL+c->r[5].uw[0]*4;}
static void integer(ee_ctx *c){w32(c->ram+pool_at,c->r[5].uw[0]);w32(c->ram+c->r[4].uw[0],pool_at);pool_at+=4;}
static ee_fn boundary(uint32_t pc){switch(pc){case 0x13bdc0:return ref;case 0x2efc48:return arr;case 0x2efce0:return elem;case 0x2fe278:return integer;
 case 0x13bd68:case 0x3285a8:case 0x3285f8:case 0x2fc870:case 0x2ed5c0:return noop;default:return NULL;}}
static ee_fn lookup(uint32_t pc){ee_fn fn=boundary(pc);return fn?fn:recomp_lookup(pc);}
static int call(ee_ctx *c,uint32_t pc,uint32_t a,uint32_t b,uint32_t d){
 memset(c->r,0,sizeof c->r);c->r[4].uw[0]=a;c->r[5].uw[0]=b;c->r[6].uw[0]=d;c->r[29].uw[0]=0x1ef0000;
 jmp_buf j;c->exit_jmp=&j;if(setjmp(j))return 0;ee_fn fn=recomp_lookup(pc);if(!fn)return 0;fn(c);return 1;
}
int main(int argc,char **argv){
 CHECK(argc==2);ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0x54f3f8);c.lookup=lookup;c.hook=boundary;c.trace=stderr;
 uint8_t body[0x4b8],rows[0x142],valid[0x142];unsigned checks=0;uint32_t seed=12345678;
 for(unsigned test=0;test<180;test++){
  for(unsigned i=0;i<sizeof body;i++){seed=seed*1664525u+1013904223u;body[i]=(uint8_t)(seed>>24);}w32(body+0x488,test%3);
  for(int g=0;g<10;g++)for(int index=0;index<counts[g];index++){
   memcpy(c.ram+CAR+8,body,sizeof body);int32_t value;
   CHECK(call(&c,getters[g][index],CAR,g==3?(uint32_t)index:0,0));CHECK(gt4_setting_get(body,g,index,&value));CHECK(value==c.r[2].sw[0]);
   int32_t change=(int32_t)(test*997u-19000u);uint8_t native[0x4b8];memcpy(native,body,sizeof body);
   CHECK(call(&c,setters[g][index],CAR,g==3&&index<8?(uint32_t)index:(uint32_t)change,(uint32_t)change));CHECK(gt4_setting_set(native,g,index,change));
   if(getenv("GT4_SETTING_ORACLE_NEGATIVE") && !test && !g && !index)native[0x12f]^=1;
   CHECK(!memcmp(native,c.ram+CAR+8,sizeof native));checks+=2;
  }
  for(unsigned i=0;i<sizeof rows;i++){seed=seed*1664525u+1013904223u;rows[i]=(uint8_t)(seed>>24);}memset(valid,1,sizeof valid);
  w32(c.ram+WRAP+0x14,CAR);memcpy(c.ram+WRAP+0x30,rows,sizeof rows);memcpy(c.ram+CAR+8,body,sizeof body);
  for(int g=0;g<10;g++)if(range_callbacks[g]){
   int32_t result[42];int n=gt4_setting_range(body,rows,valid,g,result);CHECK(n>0);CHECK(call(&c,range_callbacks[g],0x1802000,0x1802004,0));
   if(getenv("GT4_SETTING_RANGE_NEGATIVE"))result[0]^=1;
   for(int i=0;i<n;i++){int32_t original;uint32_t ptr=r32(c.ram+POOL+i*4);memcpy(&original,c.ram+ptr,4);CHECK(original==result[i]);checks++;}
  }
 }
 for(int n=1;n<=9;n++)for(unsigned test=0;test<512;test++){
  int16_t ratios[16],lo[16],hi[16];for(int i=0;i<16;i++){seed=seed*1664525u+1013904223u;ratios[i]=(int16_t)(seed>>16);lo[i]=(int16_t)0x5a5a;hi[i]=(int16_t)0x5a5a;}
  memcpy(c.ram+CAR,ratios,32);memset(c.ram+CAR+64,0x5a,64);memset(c.r,0,sizeof c.r);
  c.r[4].uw[0]=n;c.r[5].uw[0]=CAR;c.r[6].uw[0]=CAR+64;c.r[7].uw[0]=CAR+96;
  recomp_lookup(0x355278)(&c);gt4_setting_ratio_bounds(n,ratios,lo,hi);
  if(getenv("GT4_SETTING_RATIO_NEGATIVE"))hi[1]^=1;
  CHECK(!memcmp(lo,c.ram+CAR+64,32) && !memcmp(hi,c.ram+CAR+96,32));checks+=32;
 }
 memset(body,0,sizeof body);w32(body+0x488,3);int32_t rejected;
 CHECK(!gt4_setting_get(body,0,0,&rejected) && !gt4_setting_set(body,0,0,1));
 w32(body+0x488,0);CHECK(!gt4_setting_get(body,-1,0,&rejected) && !gt4_setting_get(body,0,2,&rejected));
 memset(valid,0,sizeof valid);int32_t limits[42];CHECK(gt4_setting_range(body,rows,valid,GT4_SETTING_BRAKE,limits)==-1);
 ee_free(&c);printf("PASS %u original setting reads/writes/range elements, all A/B/C sheets, signed ballast and paired ASM stores\n",checks);return 0;
}
