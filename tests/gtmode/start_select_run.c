/* Original 003883EC..00388438 and 003475B8: no callback substitutions. */
#include "ee.h"
#include "car.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"start selection line %d: %s\n",__LINE__,#x); return 1; } } while (0)
#define RACE 0x1800000u
#define COND 0x1802000u
static void word(uint8_t *p,uint32_t v){memcpy(p,&v,4);}
static int run(ee_ctx *c,uint32_t pc){
 jmp_buf j;c->exit_jmp=&j;c->end_reason=EE_END_NONE;
 if(setjmp(j))return c->end_reason==EE_END_STOP_PC;
 recomp_lookup(pc)(c);return 1;
}
int main(int argc,char **argv){
 CHECK(argc==2);ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0x54f3f8);c.lookup=recomp_lookup;
 uint8_t *native=malloc(EE_RAM_SIZE);CHECK(native);memcpy(native,c.ram,EE_RAM_SIZE);gt4_phys_mem=native;
 unsigned cases=0;
 const uint32_t kinds[]={0,1,2,3,4,5,UINT32_MAX,0x80000000u};
 for(unsigned k=0;k<sizeof kinds/sizeof kinds[0];k++)for(unsigned initial=0;initial<256;initial++){
  memset(c.ram+RACE,0xa7,0x1000);memcpy(native+RACE,c.ram+RACE,0x1000);
  word(c.ram+RACE+0xb4,initial);word(native+RACE+0xb4,initial);
  memset(c.r,0,sizeof c.r);c.r[2].uw[0]=kinds[k];c.r[20].uw[0]=RACE;c.r[29].uw[0]=0x1ef0000;
  CHECK(run(&c,0x3883ec));gt4_race_select_start(RACE,kinds[k]);
  if(getenv("GT4_START_SELECT_NEGATIVE")&&!cases)native[RACE+0xb4]^=1;
  CHECK(!memcmp(c.ram+RACE,native+RACE,0x1000));cases++;
 }
 for(unsigned flag=0;flag<256;flag++)for(unsigned rolling=0;rolling<256;rolling++)for(unsigned sign=0;sign<2;sign++){
  c.ram[COND+0xf866]=native[COND+0xf866]=(uint8_t)flag;
  c.ram[COND+0xf886]=native[COND+0xf886]=(uint8_t)rolling;
  int16_t grid=sign?-1:0;memcpy(c.ram+COND+0xf884,&grid,2);memcpy(native+COND+0xf884,&grid,2);
  word(native+COND+0x10140,0x679ce8);
  memset(c.r,0,sizeof c.r);c.r[4].uw[0]=COND;c.r[29].uw[0]=0x1ef0000;
  CHECK(run(&c,0x3475b8));uint32_t actual=gt4_session(COND,0x50,0,0,0,0,0,NULL);
  if(getenv("GT4_START_GETTER_NEGATIVE")&&!flag&&!rolling&&!sign)actual^=1;
  CHECK(c.r[2].uw[0]==actual);cases++;
 }
 memset(c.r,0,sizeof c.r);c.r[29].uw[0]=0x1ef0000;CHECK(run(&c,0x3f5528));
 word(native+COND+0x10140,0x685890);CHECK(c.r[2].uw[0]==gt4_session(COND,0x50,0,0,0,0,0,NULL));
 gt4_phys_mem=NULL;free(native);ee_free(&c);
 printf("PASS %u original start selection/getter cases and Time Trial mode; complete 0x1000-byte race block.\n",cases);return 0;
}
