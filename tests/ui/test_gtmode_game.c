#include "gtmode_game.h"
#include "save.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static ah_val get(ah_vm *vm,ah_obj *o,const char *key){ah_val v;CHECK(ah_get_member(vm,o,key,&v));return v;}
static ah_val call(ah_vm *vm,ah_obj *o,const char *key,int argc,ah_val *args){ah_val v;CHECK(ah_call_member(vm,o,key,argc,args,&v)>0);return v;}
static int dirtied;
static int dirty(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){(void)vm;(void)self;(void)data;(void)argc;(void)argv;dirtied++;*ret=ah_nil();return 0;}
int main(void){
    ah_vm *datevm=ah_vm_new();ah_obj *utility=ah_module_new(datevm,"calendar",ah_root(datevm),NULL,NULL);ah_set_member(datevm,ah_root(datevm),"calendar",ah_objv(utility));gt4_calendar_bind(datevm,utility);
    ah_val dateargs[]={ah_int(2005),ah_int(4),ah_int(2)};CHECK(call(datevm,utility,"dateToDay",3,dateargs).u.i==2453463);
    dateargs[0]=ah_int(2453463);CHECK(call(datevm,utility,"dayToGameDay",1,dateargs).u.i==0);
    ah_obj *origin=ah_as_obj(call(datevm,utility,"dayToDate",1,dateargs),AO_ARRAY);CHECK(ah_array_get(origin,0).u.i==2005 && ah_array_get(origin,1).u.i==4 && ah_array_get(origin,2).u.i==2);
    dateargs[0]=ah_int(2008);dateargs[1]=ah_int(2);dateargs[2]=ah_int(29);dateargs[0]=call(datevm,utility,"dateToDay",3,dateargs);origin=ah_as_obj(call(datevm,utility,"dayToDate",1,dateargs),AO_ARRAY);CHECK(ah_array_get(origin,2).u.i==29);ah_vm_free(datevm);
    ah_vm *vm=ah_vm_new();ah_obj *game=ah_module_new(vm,"game",ah_root(vm),NULL,NULL);ah_set_member(vm,ah_root(vm),"game",ah_objv(game));ah_set_member(vm,game,"money",ah_int(10000));gt4_game_bind(vm,game);
    ah_val args[2]={ah_int(-9000),ah_nil()};CHECK(ah_num(call(vm,game,"incMoney",1,args))==1000);
    ah_val ignored;args[0]=ah_int(-1001);call(vm,game,"incMoney",1,args);CHECK(get(vm,game,"money").u.i==-1);
    ah_set_member(vm,game,"money",ah_int(INT64_MAX));args[0]=ah_int(1);CHECK(call(vm,game,"incMoney",1,args).u.i==INT64_MIN);
    args[0]=ah_int(INT64_MIN);CHECK(call(vm,game,"isEnoughMoney",1,args).u.i==1);
    args[0]=ah_int(0);CHECK(call(vm,game,"isEnoughMoney",1,args).u.i==0);
    /* PC serialization must retain the original unsigned balance's high bit. */
    {uint8_t *packed;size_t packed_size;CHECK(gt4_state_pack(vm,game,&packed,&packed_size));
     ah_vm *checkvm=ah_vm_new();ah_obj *checkgame=ah_module_new(checkvm,"game",ah_root(checkvm),NULL,NULL);
     ah_set_member(checkvm,ah_root(checkvm),"game",ah_objv(checkgame));CHECK(gt4_state_merge(checkvm,checkgame,packed,packed_size));
     CHECK(get(checkvm,checkgame,"money").u.i==INT64_MIN);free(packed);ah_vm_free(checkvm);}
    ah_set_member(vm,game,"money",ah_int(1000));
    ah_obj *garage=ah_as_obj(get(vm,game,"garage"),AO_MODULE);CHECK(!ah_truthy(call(vm,garage,"hasRidingCar",0,NULL)));
    args[0]=ah_str(vm,"fixture_car");args[1]=ah_int(2);CHECK(ah_num(call(vm,garage,"addNewCar",2,args))==0);CHECK(ah_num(get(vm,garage,"count"))==1);
    args[0]=ah_int(0);call(vm,garage,"setRidingCar",1,args);CHECK(ah_truthy(call(vm,garage,"hasRidingCar",0,NULL)));
    ah_obj *car=ah_as_obj(get(vm,garage,"riding_car"),AO_MODULE);CHECK(ah_num(call(vm,car,"getGarageIndex",0,NULL))==0);
    args[0]=ah_int(1);CHECK(ah_call_member(vm,garage,"setRidingCar",1,args,&ignored)<0);CHECK(ah_num(get(vm,garage,"riding_index"))==0);
    args[0]=ah_float(0.5);CHECK(ah_call_member(vm,garage,"setRidingCar",1,args,&ignored)<0);
    ah_set_member(vm,garage,"_markDirty",ah_native(vm,"dirty",dirty,NULL));
    call(vm,garage,"hasRidingCar",0,NULL);CHECK(dirtied==0);
    args[0]=ah_int(0);CHECK(ah_truthy(call(vm,garage,"delCar",1,args)));CHECK(dirtied==1);CHECK(!ah_truthy(call(vm,garage,"hasRidingCar",0,NULL)));
    CHECK(ah_num(get(vm,garage,"count"))==0);CHECK(!ah_truthy(call(vm,garage,"delCar",1,args)));CHECK(dirtied==2);
    args[0]=ah_str(vm,"fixture_car");args[1]=ah_int(2);CHECK(ah_num(call(vm,garage,"addNewCar",2,args))==0);
    args[0]=ah_int(0);call(vm,garage,"setRidingCar",1,args);
    args[0]=ah_str(vm,"nike_one_04");CHECK(!ah_truthy(call(vm,game,"isCarAvailable",1,args)));call(vm,game,"setCarAvailable",1,args);CHECK(ah_truthy(call(vm,game,"isCarAvailable",1,args)));
    ah_obj *presents=ah_as_obj(get(vm,game,"presents"),AO_MODULE);ah_obj *legacy_prizes=ah_module_new(vm,"by_race",NULL,NULL,NULL);ah_set_member(vm,legacy_prizes,"11",ah_bool(1));ah_set_member(vm,presents,"by_race",ah_objv(legacy_prizes));
    args[0]=ah_int(11);CHECK(ah_truthy(call(vm,presents,"getByRace",1,args)));
    args[0]=ah_int(12);CHECK(!ah_truthy(call(vm,presents,"getByRace",1,args)));call(vm,presents,"setByRace",1,args);CHECK(ah_truthy(call(vm,presents,"getByRace",1,args)));
    ah_obj *record=ah_as_obj(get(vm,game,"race_record"),AO_MODULE);args[0]=ah_str(vm,"l0b");CHECK(!ah_truthy(call(vm,record,"isLicensePassed",1,args)));
    for(int i=1;i<=16;i++){char key[32];snprintf(key,sizeof key,"l0b%04d",i);args[0]=ah_str(vm,key);ah_obj *unit=ah_as_obj(call(vm,record,"getUnit",1,args),AO_MODULE);ah_set_member(vm,unit,"result",ah_str(vm,i==16?"NONE":"gold"));}
    args[0]=ah_str(vm,"l0b");CHECK(ah_truthy(call(vm,record,"canTryLastLicense",1,args)));CHECK(!ah_truthy(call(vm,record,"isLicensePassed",1,args)));
    args[0]=ah_str(vm,"l0b0016");ah_obj *last=ah_as_obj(call(vm,record,"getUnit",1,args),AO_MODULE);ah_set_member(vm,last,"result",ah_str(vm,"bronze"));
    args[0]=ah_str(vm,"l0b");CHECK(ah_truthy(call(vm,record,"isLicensePassed",1,args)));CHECK(!ah_truthy(call(vm,record,"isLicenseAllSilver",1,args)));
    ah_set_member(vm,last,"result",ah_str(vm,"red"));args[0]=ah_str(vm,"l0b");
    CHECK(ah_truthy(call(vm,record,"isLicensePassed",1,args)));
    CHECK(!ah_truthy(call(vm,record,"isLicenseAllBronze",1,args)));
    ah_set_member(vm,last,"original_record",ah_int(1u|(15u<<4)));
    CHECK(!ah_truthy(call(vm,record,"isLicensePassed",1,args)));
    ah_set_member(vm,last,"original_record",ah_int(1u|(9u<<4)));
    CHECK(ah_truthy(call(vm,record,"isLicensePerfect",1,args)));
    ah_remove_member(vm,last,"original_record");ah_set_member(vm,last,"result",ah_str(vm,"bronze"));
    ah_obj *calendar=ah_as_obj(get(vm,game,"calendar"),AO_MODULE),*date=ah_array_new(vm);
    ah_array_push(vm,date,ah_int(2008));ah_array_push(vm,date,ah_int(2));ah_array_push(vm,date,ah_int(28));ah_set_member(vm,calendar,"today",ah_objv(date));
    args[0]=ah_str(vm,"fixture_car");args[1]=ah_int(2);call(vm,calendar,"putBuyNewCarEvent",2,args);
    date=ah_as_obj(get(vm,calendar,"today"),AO_ARRAY);CHECK(ah_num(ah_array_get(date,2))==29);
    call(vm,calendar,"putBuyNewCarEvent",2,args);date=ah_as_obj(get(vm,calendar,"today"),AO_ARRAY);CHECK(ah_num(ah_array_get(date,1))==3 && ah_num(ah_array_get(date,2))==1);
    CHECK(ah_num(call(vm,calendar,"getElapsedDate",0,NULL))==2);
    /* Restoring actual serialized state retains ownership/records and rebinds methods. */
    ah_set_member(vm,game,"username",ah_str(vm,"Fixture"));uint8_t *bytes;size_t size;CHECK(gt4_state_pack(vm,game,&bytes,&size));
    ah_vm *restored=ah_vm_new();ah_obj *copy=ah_module_new(restored,"game",ah_root(restored),NULL,NULL);ah_set_member(restored,ah_root(restored),"game",ah_objv(copy));CHECK(gt4_state_merge(restored,copy,bytes,size));free(bytes);gt4_game_bind(restored,copy);
    ah_obj *cg=ah_as_obj(get(restored,copy,"garage"),AO_MODULE);CHECK(ah_truthy(call(restored,cg,"hasRidingCar",0,NULL)));CHECK(ah_num(get(restored,cg,"count"))==1);
    ah_obj *cr=ah_as_obj(get(restored,copy,"race_record"),AO_MODULE);args[0]=ah_str(restored,"l0b");CHECK(ah_truthy(call(restored,cr,"isLicensePassed",1,args)));
    args[0]=ah_str(restored,"nike_one_04");CHECK(ah_truthy(call(restored,copy,"isCarAvailable",1,args)));
    args[0]=ah_int(12);CHECK(ah_truthy(call(restored,ah_as_obj(get(restored,copy,"present"),AO_MODULE),"getByRace",1,args)));
    /* A sparse PS2 garage must not resolve an original slot as a dense index. */
    ah_obj *imported=ah_array_new(restored),*importcar=ah_module_new(restored,"car",NULL,NULL,NULL);
    ah_set_member(restored,importcar,"slot",ah_int(7));ah_array_push(restored,imported,ah_objv(importcar));
    ah_set_member(restored,cg,"cars",ah_objv(imported));ah_set_member(restored,cg,"riding_index",ah_int(7));gt4_game_bind(restored,copy);
    CHECK(ah_truthy(call(restored,cg,"hasRidingCar",0,NULL)));CHECK(get(restored,cg,"riding_car").u.o==importcar);
    CHECK(call(restored,importcar,"getGarageIndex",0,NULL).u.i==7);CHECK(get(restored,cg,"count").u.i==1);
    ah_vm_free(restored);ah_vm_free(vm);puts("GT Mode state contracts: passed");return 0;
}
