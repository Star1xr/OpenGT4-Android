/* Execute original writeback instructions, including all unaligned
   loads/stores. Lookup selects the current car body, as 00441248 does. */
#define main pregrid_fixture_main
#include "../ui/pregrid_menu.c"
#undef main
#include "career_blob.h"
#include "ee.h"
ee_fn recomp_lookup(uint32_t);
static void equipped_boundary(ee_ctx *c){(void)c;}
static ee_fn wear_hook(uint32_t pc){return pc==0x441248?equipped_boundary:NULL;}
static ee_fn wear_lookup(uint32_t pc){ee_fn f=wear_hook(pc);return f?f:recomp_lookup(pc);}
int main(int argc,char **argv){
 CHECK(argc==3);ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[2],0x100000)>0x54f3f8);c.lookup=wear_lookup;c.hook=wear_hook;
 const char *error=NULL;gt4_vol *v=gt4_vol_open(argv[1],&error);CHECK(v && gt4_game_load_disc(argv[1]));char msg[256];menu *m=menu_open(v,"gtmode",msg,sizeof msg);CHECK(m);
 ah_vm *vm=m->vm;ah_obj *game=game_object(m);ah_val garage;CHECK(ah_get_member(vm,game,"garage",&garage));ah_obj *car=ah_module_new(vm,"owned",NULL,NULL,NULL);
 ah_obj *cars=ah_array_new(vm);ah_array_push(vm,cars,ah_objv(car));ah_set_member(vm,garage.u.o,"cars",ah_objv(cars));ah_set_member(vm,garage.u.o,"riding_index",ah_int(0));
 const uint32_t base=0x1800000,world=0x1840000,entries=0x1841000,slots=0x1842000,entry=0x1843000,vehicle=0x1850000;uint8_t body[0x4b8],wear[32],actual[0x4b8];
 memcpy(c.ram+0x622f4c,&base,4);memcpy(c.ram+world+0x60,&entries,4);memcpy(c.ram+entries+8,&slots,4);memcpy(c.ram+slots,&entry,4);memcpy(c.ram+entry+0x18,&vehicle,4);
 uint32_t seed=12345;for(unsigned test=0;test<4096;test++){
  for(unsigned i=0;i<sizeof body;i++){seed=seed*1664525u+1013904223u;body[i]=(uint8_t)(seed>>24);}for(unsigned i=0;i<32;i++){seed=seed*1664525u+1013904223u;wear[i]=(uint8_t)(seed>>24);}
  CHECK(gt4_career_blob_set(vm,car,"full_car_ps2",body,sizeof body));ah_set_member(vm,car,"oil_state",ah_nil());memcpy(c.ram+base+0x13940,body,sizeof body);memcpy(c.ram+vehicle+0x6dc,wear,32);
  float total=(float)(seed&0xffffffu)*(test%3==0?512.0f:0.25f),bdistance=(float)(seed&0xffffu)*0.125f;
  memcpy(c.ram+vehicle+0x6d4,&total,4);memcpy(c.ram+vehicle+0x7b0,&bdistance,4);
  memset(c.r,0,sizeof c.r);c.r[4].uw[0]=world;c.r[5].uw[0]=0x1854000;c.r[6].uw[0]=0x1854004;c.r[29].uw[0]=0x1ef0000;
  jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));recomp_lookup(0x10fa58)(&c);
  uint32_t expected_a,expected_b;memcpy(&expected_a,c.ram+0x1854000,4);memcpy(&expected_b,c.ram+0x1854004,4);
  CHECK(gt4_career_distance_integer(total-bdistance)==expected_a);CHECK(gt4_career_distance_integer(bdistance)==expected_b);
  CHECK(gt4_game_apply_car_wear(vm,game,wear));CHECK(gt4_game_car_body(vm,car,actual));if(getenv("GT4_EVENT_WEAR_NEGATIVE") && !test)actual[0x498]^=1;
  CHECK(!memcmp(actual,c.ram+base+0x13940,sizeof actual));
  gt4_game_bind_car(vm,car);uint32_t metres;memcpy(&metres,wear,4);
  /* A stale purchase-time cache must not hide the actual race mileage. */
  ah_set_member(vm,car,"odometer",ah_int(metres^1u));
  ah_set_member(vm,car,"original_base_price",ah_int(15000));ah_val meter,price;
  CHECK(ah_call_member(vm,car,"getOdometer",0,NULL,&meter)>=0);
  CHECK((uint32_t)meter.u.i==metres);
  int32_t expected;CHECK(gt4_game_car_price(15000,metres,1,&expected));
  CHECK(ah_call_member(vm,car,"getUsedSellPrice",0,NULL,&price)>=0);
  if(getenv("GT4_LIVE_MILEAGE_NEGATIVE") && !test)price.u.i++;
  CHECK(price.u.i==expected);
 }
 puts("PASS 4096 original wear writeback and A/B mileage conversion cases, complete owned body including unchanged setups; live odometer/sale price ignores stale cache. Negative detectors included.");menu_close(m);gt4_vol_close(v);ee_free(&c);return 0;
}
