#include "ee.h"
#include "career_home.h"
#include "career_calendar.h"
#include "career_blob.h"
#include "gtmode_game.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
static const uint32_t BASE=0x1800000,STACK=0x1ef0000;
static void experience(ee_ctx *c){c->r[2].uw[0]=BASE;}
static ee_fn boundary(uint32_t pc){return pc==0x4683a8?experience:NULL;}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
static uint32_t random_state=0x09102026;
static uint32_t next(void){random_state=random_state*1664525u+1013904223u;return random_state;}
static int run(ee_ctx *c,uint32_t pc,uint64_t a,uint64_t b,uint64_t d,uint64_t e){
 memset(c->r,0,sizeof c->r);c->r[4].ud[0]=a;c->r[5].ud[0]=b;c->r[6].ud[0]=d;c->r[7].ud[0]=e;c->r[29].uw[0]=STACK;
 jmp_buf jump;c->exit_jmp=&jump;if(setjmp(jump)){fprintf(stderr,"stop %x %s\n",c->end_pc,c->end_detail);return 0;}recomp_lookup(pc)(c);return 1;
}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"home line %d case %u\n",__LINE__,test);return 1;}}while(0)
int main(int argc,char **argv){
 if(argc<2 || argc>3)return 2;ee_ctx c;if(ee_init(&c) || ee_load_image(&c,argv[1],0x100000)<0x500000)return 2;c.lookup=lookup;c.hook=boundary;
 gt4_career_home_data data;gt4_career_calendar_data calendar;unsigned test=0;
 if(argc==3){CHECK(gt4_career_home_data_load(argv[2],&data));CHECK(!memcmp(data.weights,c.ram+0x623e40,8));CHECK(gt4_career_calendar_data_load(argv[2],&calendar));}
 else {memset(&data,0,sizeof data);memcpy(data.weights,c.ram+0x623e40,8);memcpy(calendar.year_offsets,c.ram+0x6cc718,16);memcpy(calendar.month_offsets,c.ram+0x6cc728,52);}
 gt4_game_set_home_data(&data);ah_vm *vm=ah_vm_new();ah_obj *game=ah_module_new(vm,"game",ah_root(vm),NULL,NULL);gt4_game_bind(vm,game);ah_val stats,ret;CHECK(ah_call_member(vm,game,"getTotalRaceCount",0,NULL,&ret)==1);CHECK(ah_get_member(vm,game,"statistics",&stats));
 for(test=0;test<4096;test++){
  uint8_t e[0x180];for(unsigned i=0;i<sizeof e;i+=2){unsigned value=test==0?0:test==1?65535:next()%(test%2?65536:1025);e[i]=(uint8_t)value;e[i+1]=(uint8_t)(value>>8);}memcpy(c.ram+BASE,e,sizeof e);
  int32_t native[4],original[4];gt4_career_bspec_status(&data,e,native);
  CHECK(run(&c,0x468b58,BASE+0x200,BASE+0x204,BASE+0x208,0));original[0]=c.r[2].sw[0];memcpy(original+1,c.ram+BASE+0x200,12);
  if(getenv("GT4_HOME_NEGATIVE") && !test)native[0]++;
  if(memcmp(native,original,sizeof native))fprintf(stderr,"skills native %d %d %d %d original %d %d %d %d\n",native[0],native[1],native[2],native[3],original[0],original[1],original[2],original[3]);CHECK(!memcmp(native,original,sizeof native));
  CHECK(gt4_career_blob_set(vm,stats.u.o,"b_spec_experience_ps2",e,sizeof e));
  const char *names[]={"getBspecTotalExperience","getBspecCarExperience","getBspecCourseExperience","getBspecBattleExperience"};
  for(unsigned i=0;i<4;i++){CHECK(ah_call_member(vm,game,names[i],0,NULL,&ret)==1);CHECK(ret.u.i==original[i]);}ah_gc(vm);
  uint32_t p[7];for(unsigned i=0;i<7;i++)p[i]=next();memcpy(c.ram+BASE+0x54,p,sizeof p);CHECK(run(&c,0x43b018,BASE,0,0,0));CHECK(gt4_career_home_race_count(p)==c.r[2].sw[0]);
  int32_t today=2453462+(int32_t)(next()%100000),offset=(int32_t)(next()%4000)-500;unsigned count=next()%2923;
  unsigned first=next()%2922;uint32_t packed=gt4_career_day_to_date(&calendar,today-offset);memcpy(c.ram+BASE+0xb6c0,&today,4);memcpy(c.ram+BASE+0x10,&count,4);memcpy(c.ram+BASE+0xc,&first,4);
  CHECK(run(&c,0x42f060,BASE,packed&65535,(packed>>16)&255,packed>>24));uint32_t ptr=c.r[2].uw[0];int32_t index=ptr?(int32_t)(((ptr-BASE-0x18)/16+2922-first)%2922):-1;CHECK(gt4_career_history_index(today,today-offset,count)==index);
  uint64_t courses[128];for(unsigned i=0;i<128;i++)courses[i]=test%2?(UINT64_C(0x2200000000)|4*i+1):UINT64_MAX;memcpy(c.ram+BASE+0x2008,courses,sizeof courses);memset(c.ram+BASE+0x2408,0,4);
  for(unsigned j=0;j<20;j++){uint64_t key=UINT64_C(0x2200000000)|(next()%180);gt4_career_course_unlock(courses,key);CHECK(run(&c,0x42ea38,BASE,key,0,0));CHECK(!memcmp(courses,c.ram+BASE+0x2008,sizeof courses));CHECK(run(&c,0x42eae8,BASE,key,0,0));CHECK(gt4_career_course_available(courses,key,0)==c.r[2].sw[0]);}
  CHECK(run(&c,0x42eae8,BASE,UINT64_C(0x220000fffe),0,0));CHECK(c.r[2].sw[0]==0);
  unsigned all=1;memcpy(c.ram+BASE+0x2408,&all,4);CHECK(run(&c,0x42eae8,BASE,UINT64_C(0x220000fffe),0,0));CHECK(gt4_career_course_available(courses,UINT64_C(0x220000fffe),1)==c.r[2].sw[0]);
  uint64_t metres=test%2?((uint64_t)next()<<32)|next():next();
  CHECK(run(&c,0x57f480,metres,0,0,0));float distance=c.fpr[0].f;
  for(unsigned miles=0;miles<2;miles++){memcpy(c.ram+BASE,&miles,4);c.fpr[12].f=distance;CHECK(run(&c,0x472c28,BASE,0,0,0));c.fpr[12].f=c.fpr[0].f;CHECK(run(&c,0x57f360,0,0,0,0));
   int64_t native_distance=gt4_career_home_distance(metres,miles);if(native_distance!=c.r[2].sd[0])fprintf(stderr,"distance %llx unit %u native %lld original %lld\n",(unsigned long long)metres,miles,(long long)native_distance,(long long)c.r[2].sd[0]);CHECK(native_distance==c.r[2].sd[0]);}
 }
 puts("PASS 4096 original Home cases: B-Spec total/three skills and native bindings, seven race counters, dated history lookup, sorted course unlock/query/override, km/miles signed distance conversion; negative detector included.");ah_vm_free(vm);ee_free(&c);return 0;
}
