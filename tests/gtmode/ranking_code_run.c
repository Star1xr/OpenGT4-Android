#include "ranking_code.h"
#include "../../src/core/ranking_code.c"
#include "ee.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
static uint32_t clock_value;
static void clock_hook(ee_ctx *e){e->r[2].ud[0]=clock_value;}
static ee_fn hook(uint32_t a){return a==0x548918?clock_hook:NULL;}
static ee_fn lookup(uint32_t a){ee_fn f=hook(a);return f?f:recomp_lookup(a);}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"ranking line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
 CHECK(argc>=3);ee_ctx e;CHECK(!ee_init(&e));CHECK(ee_load_image(&e,argv[1],0x100000)>0x5a7c98);e.lookup=lookup;e.hook=hook;
 const char *err;gt4_vol *v=gt4_vol_open(argv[2],&err);CHECK(v);uint8_t key[16];CHECK(gt4_ranking_code_key(v,key));CHECK(!memcmp(key,e.ram+0x6a7c88,16));
 uint32_t rng=12345;
 /* Compare stages as well as the public output, retaining the original
    round arithmetic as the independent source of expected bytes. */
 uint8_t ex[256],lg[256],ks[217],src[8]={1,2,3,4,5,6,7,8},dst[8];tables(ex,lg);
 schedule(ks,key,key+8,13,ex);memcpy(e.ram+0x1802000,key,16);
 e.r[29].uw[0]=0x1eff000;e.r[4].uw[0]=0x1efc000;recomp_lookup(0x44b3a8)(&e);
 e.r[4].uw[0]=0x1803000;e.r[5].uw[0]=0x1efc000;e.r[6].uw[0]=0x1802000;e.r[7].uw[0]=0x1802008;e.r[8].uw[0]=13;
 recomp_lookup(0x44b680)(&e);CHECK(!memcmp(ks,e.ram+0x1803000,217));
 memcpy(e.ram+0x1804000,src,8);block(dst,src,ks,ex,lg);
 e.r[4].uw[0]=0x1804100;e.r[5].uw[0]=0x1804000;e.r[6].uw[0]=0x1efc000;e.r[7].uw[0]=0x1803000;
 recomp_lookup(0x44b900)(&e);CHECK(!memcmp(dst,e.ram+0x1804100,8));
 uint8_t st[16]={0};digest_block(st,src,ex,lg);
 memset(e.ram+0x1805000,0,32);memcpy(e.ram+0x1805000,&(uint32_t){0x1efc000},4);
 e.r[4].uw[0]=0x1805000;e.r[5].uw[0]=0x1804000;recomp_lookup(0x44acd8)(&e);CHECK(!memcmp(st,e.ram+0x1805004,16));
 for(int t=0;t<1024;t++){
  uint32_t fields[10];for(int i=0;i<10;i++){rng=rng*1664525u+1013904223u;fields[i]=rng;}
  clock_value=t*777u;char actual[33];gt4_ranking_code(fields,key,clock_value,actual);
  memcpy(e.ram+0x1800000,fields,sizeof fields);memset(e.ram+0x1801000,0,33);
  e.r[4].uw[0]=0x1800000;e.r[5].uw[0]=0x1801000;e.r[6].uw[0]=32;e.r[29].uw[0]=0x1eff000;
  jmp_buf j;e.exit_jmp=&j;CHECK(!setjmp(j));recomp_lookup(0x44b258)(&e);
  if(argc>3 && !t)actual[0]=actual[0]=='0'?'1':'0';
  if(memcmp(actual,e.ram+0x1801000,32)){fprintf(stderr,"case %d actual %s original %.32s\n",t,actual,e.ram+0x1801000);return 1;}
 }
 puts("ranking: 1024 original passcodes, all 32 decimal digits exact");gt4_vol_close(v);ee_free(&e);return 0;
}
