/* Countdown initialization/first ticks against original 003C0190/003C0208.
   Only the course light presentation call is excluded on both sides. */
#include "ee.h"
#include "car.h"
#include "career_race_lookup.h"
#include "specdb.h"
#include "vol.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"GT start line %d: %s\n",__LINE__,#x);return 1;}}while(0)
#define RACE 0x1800000u
#define SETTINGS 0x1802000u
static void lights(ee_ctx *c){(void)c;}
static ee_fn boundary(uint32_t pc){return pc==0x3973d0?lights:NULL;}
static ee_fn lookup(uint32_t pc){return pc==0x3973d0?lights:recomp_lookup(pc);}
static void word(uint8_t *p,uint32_t v){memcpy(p,&v,4);}
static int original(ee_ctx *c,uint32_t pc){
 memset(c->r,0,sizeof c->r);c->r[4].uw[0]=RACE;c->r[29].uw[0]=0x1ef0000;
 jmp_buf j;c->exit_jmp=&j;if(setjmp(j))return 0;recomp_lookup(pc)(c);return 1;
}
int main(int argc,char **argv){
 CHECK(argc==3);ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0x54f3f8);c.lookup=lookup;c.hook=boundary;c.trace=stderr;
 uint8_t *native=calloc(1,EE_RAM_SIZE);CHECK(native);
 const char *error=NULL;gt4_vol *v=gt4_vol_open(argv[2],&error);CHECK(v);
 size_t ln,rn;uint8_t *labels=gt4_vol_read(v,"/specdb/GT4_US2560/RACE.idi",&ln),*rows=gt4_vol_read(v,"/specdb/GT4_US2560/RACE.dbt",&rn);CHECK(labels && rows);
 gt4_career_race_lookup_data data;CHECK(gt4_career_race_lookup_data_load(argv[2],&data));
 const char *events[]={"am_sunday_0001","rh_paris_rally_e_0001","rh_nursnow_e_0001","rh_icerace_e_0001","rh_grandcanyon_e_0001"};
 for(unsigned test=0;test<2053;test++){
  uint8_t row[151]={0};
  if(test<5)CHECK(gt4_career_race_row(&data,labels,ln,rows,rn,events[test],row));
  else{row[0x40]=(uint8_t)(test-5);row[0x41]=(uint8_t)((test-5)/256);}
  memset(c.ram+RACE,0,0x3000);memset(native+RACE,0,0x3000);
  word(c.ram+RACE+0x70,SETTINGS);word(native+RACE+0x70,SETTINGS);
  /* Presentation's nested course pointer chain stays within initialized
     guest RAM; excluded light callback does not inspect those pointers. */
  memcpy(c.ram+SETTINGS,row,151);memcpy(native+SETTINGS,row,151);
  word(c.ram+RACE+0xc4,test%7);word(native+RACE+0xc4,test%7);
  CHECK(original(&c,0x3c0190));gt4_phys_mem=native;gt4_phys_headless=1;gt4_race_begin(RACE);
  CHECK(!memcmp(c.ram+RACE,native+RACE,0x1000));
  for(int tick=0;tick<3;tick++){
   CHECK(original(&c,0x3c0208));gt4_phys_mem=native;gt4_race_before_step(RACE);
   if(getenv("GT4_CAREER_START_NEGATIVE") && !test && !tick)native[RACE+0xb8]^=1;
   CHECK(!memcmp(c.ram+RACE,native+RACE,0x1000));
  }
  if(test<5)printf("%s: kind=%u initial seconds=%u frames after three ticks=%u\n",events[test],row[0x40],row[0x41],GT4_U32(RACE+0xbc));
 }
 gt4_phys_mem=NULL;free(native);free(labels);free(rows);gt4_vol_close(v);ee_free(&c);
 puts("PASS 2053 original start initialization/three-tick cases; complete 0x1000-byte race state, countdown/records/queue/time limit; light presentation excluded.");return 0;
}
