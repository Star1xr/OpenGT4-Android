/* Original sequence-exit integer additions at 00114044..0011407c.
   Stop before world destruction: the B sum is in v0, whose jal delay
   instruction stores it. No race/menu arithmetic is stubbed. */
#define main pregrid_fixture_main
#include "../ui/pregrid_menu.c"
#undef main
#include "ee.h"
ee_fn recomp_lookup(uint32_t);
int main(int argc,char **argv){
 CHECK(argc==3);ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[2],0x100000)>0x54f3f8);c.lookup=recomp_lookup;
 const char *error=NULL;gt4_vol *v=gt4_vol_open(argv[1],&error);CHECK(v && gt4_game_load_disc(argv[1]));char msg[256];menu *m=menu_open(v,"gtmode",msg,sizeof msg);CHECK(m);
 ah_vm *vm=m->vm;ah_obj *game=game_object(m);CHECK(gt4_game_apply_event_distances(vm,game,0,0));ah_val stats;CHECK(ah_get_member(vm,game,"statistics",&stats));
 const uint32_t base=0x1800000,stack=0x1ef0000;memcpy(c.ram+0x622f4c,&base,4);uint32_t seed=123;
 for(unsigned test=0;test<4096;test++){
  uint64_t totals[2];uint32_t distances[2];for(unsigned i=0;i<2;i++){seed=seed*1664525u+1013904223u;totals[i]=(uint64_t)seed<<32;seed=seed*1664525u+1013904223u;totals[i]|=seed;seed=seed*1664525u+1013904223u;distances[i]=seed;if(test<4)totals[i]=UINT64_MAX-test;int64_t value;memcpy(&value,&totals[i],8);ah_set_member(vm,stats.u.o,i?"b_spec_distance":"a_spec_distance",ah_int(value));memcpy(c.ram+base+0x348+0x70+i*8,&totals[i],8);memcpy(c.ram+stack+4+i*4,&distances[i],4);}
  memset(c.r,0,sizeof c.r);c.r[7].uw[0]=0x622f4c;c.r[29].uw[0]=stack;jmp_buf jump;c.exit_jmp=&jump;if(!setjmp(jump))recomp_lookup(0x114044)(&c);CHECK(c.end_pc==0x114078);
  CHECK(menu_apply_event_distances(m,distances[0],distances[1]));uint64_t expected[2];memcpy(&expected[0],c.ram+base+0x348+0x70,8);expected[1]=c.r[2].ud[0];
  for(unsigned i=0;i<2;i++){ah_val value;CHECK(ah_get_member(vm,stats.u.o,i?"b_spec_distance":"a_spec_distance",&value));uint64_t actual=(uint64_t)value.u.i;if(getenv("GT4_EVENT_DISTANCE_NEGATIVE")&&!test&&!i)actual^=1;if(actual!=expected[i])fprintf(stderr,"case %u field %u actual %llx expected %llx\n",test,i,(unsigned long long)actual,(unsigned long long)expected[i]);CHECK(actual==expected[i]);}
 }
 puts("PASS 4096 sequence-exit A/B distance additions against original, including 64-bit wrap; negative changes one bit.");menu_close(m);gt4_vol_close(v);ee_free(&c);return 0;
}
