#include "gtmode_game.h"
#include "career_original.h"
#include "career_calendar.h"
#include "career_blob.h"
#include "career_setting.h"
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <math.h>
static gt4_game_car_factory car_factory;
static gt4_game_service_reader service_reader;static void *service_reader_ctx;
void gt4_game_set_service_reader(gt4_game_service_reader fn,void *ctx){service_reader=fn;service_reader_ctx=ctx;}
int gt4_game_car_service_geometry(ah_vm *vm,ah_obj *car,float out[12]){uint8_t body[0x4b8];return service_reader && gt4_game_car_body(vm,car,body) && service_reader(service_reader_ctx,body,out);}
static gt4_game_setting_reader setting_reader;
static void *setting_reader_ctx;
void gt4_game_set_setting_reader(gt4_game_setting_reader fn,void *ctx){setting_reader=fn;setting_reader_ctx=ctx;}
static gt4_game_stats_reader stats_reader;
static void *stats_reader_ctx;
void gt4_game_set_stats_reader(gt4_game_stats_reader fn,void *ctx){stats_reader=fn;stats_reader_ctx=ctx;}
int gt4_game_car_stats(ah_vm *vm,ah_obj *car,int32_t out[3]){uint8_t body[0x4b8];return stats_reader && gt4_game_car_body(vm,car,body) && stats_reader(stats_reader_ctx,body,out);}
static void *car_factory_ctx;
static int car_preview(ah_vm*,ah_obj*,void*,int,const ah_val*,ah_val*);
static int car_index(ah_vm*,ah_obj*,void*,int,const ah_val*,ah_val*);
int gt4_game_has_car_factory(void){return car_factory!=NULL;}
void gt4_game_set_car_factory(gt4_game_car_factory factory,void *ctx){car_factory=factory;car_factory_ctx=ctx;}
static gt4_game_auto_gears auto_gears;static void *auto_gears_ctx;
void gt4_game_set_auto_gears(gt4_game_auto_gears fn,void *ctx){auto_gears=fn;auto_gears_ctx=ctx;}
static gt4_career_part_data original_parts;
static int original_parts_ready;
void gt4_game_set_part_data(const gt4_career_part_data *data){original_parts_ready=data!=NULL;if(data)original_parts=*data;}
const gt4_career_part_data *gt4_game_part_data(void){return original_parts_ready?&original_parts:NULL;}
static gt4_career_race_lookup_data original_race_lookup;
static int original_race_lookup_ready;
void gt4_game_set_race_lookup_data(const gt4_career_race_lookup_data *data){original_race_lookup_ready=data!=NULL;if(data)original_race_lookup=*data;}
const gt4_career_race_lookup_data *gt4_game_race_lookup_data(void){return original_race_lookup_ready?&original_race_lookup:NULL;}
static gt4_career_calendar_data original_calendar;
static int original_calendar_ready;
static gt4_career_home_data original_home;static int original_home_ready;
void gt4_game_set_home_data(const gt4_career_home_data *data){original_home_ready=data!=NULL;if(data)original_home=*data;}
const gt4_career_home_data *gt4_game_home_data(void){return original_home_ready?&original_home:NULL;}
static gt4_career_oil_data original_oil;
static gt4_career_price_data original_prices;
static int original_oil_ready,original_prices_ready;
void gt4_game_set_oil_data(const gt4_career_oil_data *data){original_oil_ready=data!=NULL;if(data)original_oil=*data;}
void gt4_game_set_price_data(const gt4_career_price_data *data){original_prices_ready=data!=NULL;if(data)original_prices=*data;}
int gt4_game_car_price(int32_t base,uint32_t odometer,int sell,int32_t *price){
    if(!original_prices_ready || !price)return 0;
    int32_t used=gt4_career_used_price_value(&original_prices,base,odometer);
    *price=sell?gt4_career_sell_price_value(used):used;return 1;
}
void gt4_game_set_calendar_data(const gt4_career_calendar_data *data){original_calendar_ready=data!=NULL;if(data)original_calendar=*data;}
int gt4_game_decode_calendar_day(int32_t day,int date[3]){
    if(!original_calendar_ready)return 0;
    uint32_t packed=gt4_career_day_to_date(&original_calendar,day);
    date[0]=packed&65535;date[1]=(packed>>16)&255;date[2]=packed>>24;return 1;
}
static int32_t low32(ah_val v){uint32_t bits=(uint32_t)v.u.i;int32_t result;memcpy(&result,&bits,4);return result;}

static ah_val member(ah_vm *vm,ah_obj *o,const char *key){ah_val v=ah_nil();if(o)ah_get_member(vm,o,key,&v);return v;}
static ah_obj *module(ah_vm *vm,ah_obj *o,const char *key){ah_obj *v=ah_as_obj(member(vm,o,key),AO_MODULE);if(!v){v=ah_module_new(vm,key,NULL,NULL,NULL);ah_set_member(vm,o,key,ah_objv(v));}return v;}
static ah_obj *array(ah_vm *vm,ah_obj *o,const char *key){ah_obj *v=ah_as_obj(member(vm,o,key),AO_ARRAY);if(!v){v=ah_array_new(vm);ah_set_member(vm,o,key,ah_objv(v));}return v;}
static void method(ah_vm *vm,ah_obj *o,const char *name,ah_native_fn fn){ah_set_member(vm,o,name,ah_bind(vm,o,ah_native(vm,name,fn,(void *)name)));}
static void changed(ah_vm *vm,ah_obj *self){ah_val callback=member(vm,self,"_markDirty"),ret;if(callback.t!=AV_NIL)ah_call(vm,callback,0,NULL,&ret);}
int gt4_game_apply_car_wear(ah_vm *vm,ah_obj *game,const uint8_t wear[32]){
    if(!vm || !game || !wear)return 0;
    ah_obj *car=ah_as_obj(member(vm,module(vm,game,"garage"),"riding_car"),AO_MODULE);
    uint8_t body[0x4b8];if(!car || !gt4_game_car_body(vm,car,body))return 0;
    /* 0010FA58 writes game+0x13dd8 = riding body+0x498, all 32 bytes. */
    memcpy(body+0x498,wear,32);
    if(!gt4_career_blob_set(vm,car,"full_car_ps2",body,sizeof body))return 0;
    ah_obj *state=ah_array_new(vm);for(unsigned i=0;i<32;i++)ah_array_push(vm,state,ah_int(wear[i]));
    ah_set_member(vm,car,"oil_state",ah_objv(state));changed(vm,game);return 1;
}
int gt4_game_apply_event_distances(ah_vm *vm,ah_obj *game,uint32_t a,uint32_t b){
    if(!vm || !game)return 0;ah_obj *stats=module(vm,game,"statistics");
    const char *names[]={"a_spec_distance","b_spec_distance"};uint32_t add[]={a,b};
    for(unsigned i=0;i<2;i++){ah_val old=member(vm,stats,names[i]);if(old.t!=AV_NIL && old.t!=AV_INT && old.t!=AV_LONG)return 0;}
    for(unsigned i=0;i<2;i++){ah_val old=member(vm,stats,names[i]);uint64_t total=(old.t==AV_NIL?0:(uint64_t)old.u.i)+add[i];int64_t value;memcpy(&value,&total,8);ah_set_member(vm,stats,names[i],ah_int(value));}
    changed(vm,game);return 1;
}
/* func_0014EB38 -> func_00343928; cost is charged by the original script.
   Do not infer wear for a car whose original state has not been imported. */
static int car_oil(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
    (void)data;(void)argv;if(argc || !original_oil_ready)return ah_error(vm,"original oil settings unavailable");
    ah_obj *state=ah_as_obj(member(vm,self,"oil_state"),AO_ARRAY);uint8_t wear[32];
    if(ah_array_size(state)!=32)return ah_error(vm,"original car oil state unavailable");
    for(int i=0;i<32;i++){ah_val v=ah_array_get(state,i);if((v.t!=AV_INT && v.t!=AV_LONG) || v.u.i<0 || v.u.i>255)return ah_error(vm,"invalid car oil state");wear[i]=(uint8_t)v.u.i;}
    gt4_career_replace_oil(wear,original_oil.params,original_oil.settings);
    ah_obj *updated=ah_array_new(vm);for(int i=0;i<32;i++)ah_array_push(vm,updated,ah_int(wear[i]));
    ah_set_member(vm,self,"oil_state",ah_objv(updated));changed(vm,self);*ret=ah_nil();return 0;
}
static int car_oil_life(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
 (void)data;(void)argv;uint8_t body[0x4b8];if(argc || !original_oil_ready || !gt4_game_car_body(vm,self,body))return ah_error(vm,"original oil life unavailable");
 *ret=ah_float(gt4_career_oil_life(body+0x498,original_oil.params,original_oil.settings));return 0;
}
/* func_0013C9D8 -> 00441248 / 004458C8: signed color byte from
   the current setup. Imported selected cars carry that setup's exact byte. */
static int car_color(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
    (void)data;(void)argv;if(argc)return ah_error(vm,"getColorIndex takes no arguments");
    ah_val value=member(vm,self,"color_index");if(value.t==AV_NIL)value=member(vm,self,"variation");
    if(value.t!=AV_INT && value.t!=AV_LONG)return ah_error(vm,"original car color unavailable");
    unsigned bits=(uint8_t)value.u.i;*ret=ah_int(bits<128?(int)bits:(int)bits-256);return 0;
}
/* func_00441128 / 004411F0. original_base_price is resolved from the
   owning car's original SpecDB row by the menu catalog binder. */
static int car_price(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
    (void)argv;ah_val base=member(vm,self,"original_base_price"),mileage=member(vm,self,"odometer");int32_t price;
    /* 00441128/004411F0 -> 003434C8: price reads the live wear
       odometer (+4a0), not the compact purchase-time garage cache. */
    uint8_t body[0x4b8];if(gt4_game_car_body(vm,self,body)){
        uint32_t metres;memcpy(&metres,body+0x498,4);mileage=ah_int(metres);
    }
    if(argc || (base.t!=AV_INT && base.t!=AV_LONG) || (mileage.t!=AV_INT && mileage.t!=AV_LONG) ||
       !gt4_game_car_price(low32(base),(uint32_t)mileage.u.i,!strcmp(data,"getUsedSellPrice"),&price))return ah_error(vm,"original car price data unavailable");
    *ret=ah_int(price);return 0;
}
/* MCarData.slot: 00140C70 getter reads live +0x490; setter calls
   0043E2E8, a single sw. Serialized native body omits class header +8. */
static int car_setting_get(ah_vm *vm,ah_obj *self,const char *name,ah_val *out){
    if(!strcmp(name,"dirtiness")){uint8_t body[0x4b8];if(!original_oil_ready || !gt4_game_car_body(vm,self,body)){*out=ah_nil();return 1;}*out=ah_float(gt4_career_dirtiness(body+0x498,original_oil.dirt_params));return 1;}
    /* 00141548 -> 00145B20 +0x64: compact unit +0x18, low ten bits. */
    if(!strcmp(name,"rideHistory")){*out=ah_int(low32(member(vm,self,"history"))&1023);return 1;}
    if(strcmp(name,"slot"))return 0;
    uint8_t body[0x4b8];if(!gt4_game_car_body(vm,self,body)){*out=ah_nil();return 1;}
    uint32_t bits;int32_t slot;memcpy(&bits,body+0x488,4);memcpy(&slot,&bits,4);*out=ah_int(slot);return 1;
}
static int car_setting_set(ah_vm *vm,ah_obj *self,const char *name,ah_val value){
    if(!strcmp(name,"dirtiness")){
        uint8_t body[0x4b8];if(!ah_is_num(value) || !gt4_game_car_body(vm,self,body)){ah_error(vm,"dirtiness requires an owned car");return 1;}
        gt4_career_set_dirtiness(body+0x498,(float)ah_num(value));
        ah_obj *state=ah_array_new(vm);for(unsigned i=0;i<32;i++)ah_array_push(vm,state,ah_int(body[0x498+i]));
        ah_set_member(vm,self,"oil_state",ah_objv(state));gt4_career_blob_set(vm,self,"full_car_ps2",body,sizeof body);changed(vm,self);return 1;
    }
    if(strcmp(name,"slot"))return 0;
    uint8_t body[0x4b8];
    if((value.t!=AV_INT && value.t!=AV_LONG) || !gt4_game_car_body(vm,self,body)){
        ah_error(vm,"slot requires an integer and an owned car body");return 1;
    }
    uint32_t bits=(uint32_t)value.u.i;memcpy(body+0x488,&bits,4);
    gt4_career_blob_set(vm,self,"full_car_ps2",body,sizeof body);changed(vm,self);return 1;
}
typedef struct {ah_vm *vm;ah_obj *car;uint8_t *buffer,*valid;} setting_lookup_context;
static int setting_row_lookup(void *context,uint64_t key,uint8_t *out,size_t capacity){
    setting_lookup_context *ctx=context;ah_val fn=member(ctx->vm,ctx->car,"_getSettingPartRow"),row;
    int64_t signed_key;memcpy(&signed_key,&key,8);ah_val arg=ah_int(signed_key);
    if(fn.t==AV_NIL || ah_call(ctx->vm,fn,1,&arg,&row)<0)return -1;
    if(row.t==AV_NIL)return 0;
    ah_obj *bytes=ah_as_obj(row,AO_ARRAY);int count=ah_array_size(bytes);
    if(!bytes || count<0 || (size_t)count>capacity)return -1;
    for(int i=0;i<count;i++){
        ah_val byte=ah_array_get(bytes,i);
        if((byte.t!=AV_INT && byte.t!=AV_LONG) || byte.u.i<0 || byte.u.i>255)return -1;
        out[i]=(uint8_t)byte.u.i;
    }
    if(ctx->buffer && ctx->valid && (uintptr_t)out>=(uintptr_t)ctx->buffer &&
       (uintptr_t)out+(size_t)count<=(uintptr_t)ctx->buffer+0x142)
        memset(ctx->valid+((uintptr_t)out-(uintptr_t)ctx->buffer),1,(size_t)count);
    return count;
}
/* 0013FEA8 -> 00147D50 -> 004401C0. NOS.dbt row width 8 at destination +0x13a ends +0x142.
   Existing wrapper bytes must
   be retained for rows that the original database lookup does not fill. */
static int car_setup_parts_buffer(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
    (void)data;(void)argv;uint8_t body[0x4b8],buffer[0x142],valid[0x142];setting_lookup_context ctx={vm,self,buffer,valid};
    if(argc || !gt4_game_car_body(vm,self,body))
        return ah_error(vm,"original setting buffer unavailable");
    /* 00147D88 does not initialize the temporary row buffer. Represent
       previously unwritten cells with validity, not guessed part values.
       Zero backing storage is never evidence of an original zero default. */
    if(!gt4_career_blob_get(vm,self,"_setting_parts_ps2",buffer,sizeof buffer)){
        memset(buffer,0,sizeof buffer);memset(valid,0,sizeof valid);
    }else if(!gt4_career_blob_get(vm,self,"_setting_parts_valid_ps2",valid,sizeof valid))memset(valid,1,sizeof valid);
    if(!gt4_career_prepare_setting(body,buffer,sizeof buffer,setting_row_lookup,&ctx))
        return ah_error(vm,"original equipped part row unavailable");
    if(!gt4_career_blob_set(vm,self,"_setting_parts_ps2",buffer,sizeof buffer))return ah_error(vm,"setting buffer write failed");
    if(!gt4_career_blob_set(vm,self,"_setting_parts_valid_ps2",valid,sizeof valid))return ah_error(vm,"setting buffer validity write failed");
    *ret=ah_nil();return 0;
}
/* 0013FF38 -> 00441350. Resolve each original TRANSMISSION row before
   mutating the serialized owned body; no guessed default type. */
static int car_end_setting(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
    (void)data;(void)argv;uint8_t body[0x4b8];int32_t types[3];
    if(argc || !gt4_game_car_body(vm,self,body))return ah_error(vm,"endSetting requires an owned car");
    ah_val lookup=member(vm,self,"_getTransmissionType");
    for(unsigned i=0;i<3;i++){
        uint64_t key;memcpy(&key,body+i*0x178+0x60,8);types[i]=-1;
        if(key==UINT64_MAX)continue;
        int64_t signed_key;memcpy(&signed_key,&key,8);ah_val arg=ah_int(signed_key),type;
        if(lookup.t==AV_NIL || ah_call(vm,lookup,1,&arg,&type)<0 || (type.t!=AV_INT && type.t!=AV_LONG))
            return ah_error(vm,"original transmission type unavailable");
        types[i]=low32(type);
    }
    gt4_career_end_setting(body,types);
    if(!gt4_career_blob_set(vm,self,"full_car_ps2",body,sizeof body))return ah_error(vm,"endSetting write failed");
    changed(vm,self);*ret=ah_nil();return 0;
}
/* Original MCarData type predicates and 00140530 equipped-stage getter. */
static int car_part_query(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
    if(argc<1 || (argv[0].t!=AV_INT && argv[0].t!=AV_LONG))return ah_error(vm,"part query requires integer type");
    int32_t type=low32(argv[0]);const char *name=data;
    if(!strcmp(name,"isRemovableType")){*ret=ah_int(gt4_career_part_removable(type));return 0;}
    if(!strcmp(name,"isRemovedNormalType")){*ret=ah_int(gt4_career_part_removed_normal(type));return 0;}
    uint8_t body[0x4b8];int32_t stage;setting_lookup_context ctx={vm,self};
    if(!original_parts_ready || !gt4_game_car_body(vm,self,body) ||
       !gt4_career_equipped_stage(body,type,original_parts.normal_flags,setting_row_lookup,&ctx,&stage))
        return ah_error(vm,"original equipped stage unavailable");
    *ret=ah_int(stage);return 0;
}
/* car.isReversibilityType(type) (00140A18 -> 0044A218): types 1..28
   through jtbl_006A7AB0, a part that can be swapped back to the stock one:
   1..5, 10, 11, 13, 14, 18..28; 6..9, 12, 15..17 and others not. */
static int car_reversibility(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
    (void)self;(void)data;
    if(argc<1 || (argv[0].t!=AV_INT && argv[0].t!=AV_LONG))return ah_error(vm,"isReversibilityType requires an integer type");
    static const uint8_t table[28]={1,1,1,1,1,0,0,0,0,1,1,0,1,1,0,0,0,1,1,1,1,1,1,1,1,1,1,1};
    uint32_t type=(uint32_t)low32(argv[0])-1u;
    *ret=ah_int(type<28?table[type]:0);return 0;
}
/* 0013F3A8: original muffler may have no stock piece. */
static int car_default_no_parts(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
    (void)data;uint8_t body[0x4b8];setting_lookup_context ctx={vm,self};int result;
    if(argc<1 || (argv[0].t!=AV_INT && argv[0].t!=AV_LONG) || !gt4_game_car_body(vm,self,body) ||
       !gt4_career_default_no_part(body,low32(argv[0]),setting_row_lookup,&ctx,&result))return ah_error(vm,"original default parts unavailable");
    *ret=ah_int(result);return 0;
}
/* 001403A0 +0x40438..0x40468: missing stage zero and removable type
   clears its key. Installation branch 00440B50 still needs defaults. */
static int car_change_parts(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
    (void)data;uint8_t body[0x4b8];
    if(argc<2 || (argv[0].t!=AV_INT && argv[0].t!=AV_LONG) || (argv[1].t!=AV_INT && argv[1].t!=AV_LONG) || !gt4_game_car_body(vm,self,body))return ah_error(vm,"changeParts requires owned car/type/stage");
    ah_val fn=member(vm,self,"_getSettingPartKey"),key;
    if(fn.t==AV_NIL || ah_call(vm,fn,2,argv,&key)<0 || (key.t!=AV_INT && key.t!=AV_LONG))return ah_error(vm,"original part key unavailable");
    if((uint64_t)key.u.i!=UINT64_MAX){
        if(!gt4_career_install_key_only(body,(uint64_t)key.u.i)){
            uint8_t buffer[0x142],valid[0x142],row[256];setting_lookup_context ctx={vm,self,buffer,valid};
            /* 001403A0 passes the context's buffer (+0x30), there whether
               or not a setting page prepared it: never written cells are
               unknown, and a missing row needing them stays an error. */
            if(!gt4_career_blob_get(vm,self,"_setting_parts_ps2",buffer,sizeof buffer)){memset(buffer,0,sizeof buffer);memset(valid,0,sizeof valid);}
            else if(!gt4_career_blob_get(vm,self,"_setting_parts_valid_ps2",valid,sizeof valid))memset(valid,1,sizeof valid);
            unsigned type=(unsigned)((uint64_t)key.u.i>>32),dest,width;
            switch(type){case 11:dest=0xc;width=0x2d;break;case 2:dest=0;width=0xc;break;case 3:dest=0x84;width=0x70;break;case 4:dest=0x11a;width=0x1a;break;case 5:dest=0x110;width=0xa;break;case 14:dest=0x70;width=0x14;break;case 24:dest=0xf4;width=0x1c;break;case 10:dest=0x3a;width=0x16;break;case 27:dest=0x13a;width=8;break;default:return ah_error(vm,"part installation defaults not ported");}
            int bytes=setting_row_lookup(&ctx,(uint64_t)key.u.i,row,sizeof row);
            if(bytes<0)return ah_error(vm,"part row unavailable");
            if(!bytes)for(unsigned j=0;j<width;j++)if(!valid[dest+j])return ah_error(vm,"original unwritten part defaults unavailable");
            if(
               !gt4_career_install_defaults(body,(uint64_t)key.u.i,buffer,setting_row_lookup,&ctx))return ah_error(vm,"part installation defaults unavailable");
            /* 00443ED0 writes actual decoded bytes; padding stays as it was. */
            if(bytes>0)memset(valid+dest,1,(size_t)bytes);
            if(type==11 && (!auto_gears || !auto_gears(auto_gears_ctx,body)))return ah_error(vm,"original automatic gear ratios unavailable");
            if(!gt4_career_blob_set(vm,self,"_setting_parts_ps2",buffer,sizeof buffer))return ah_error(vm,"part buffer write failed");
            if(!gt4_career_blob_set(vm,self,"_setting_parts_valid_ps2",valid,sizeof valid))return ah_error(vm,"part validity write failed");
        }
        if(!gt4_career_blob_set(vm,self,"full_car_ps2",body,sizeof body))return ah_error(vm,"part installation write failed");
        changed(vm,self);*ret=ah_nil();return 0;
    }
    if(low32(argv[1])==0 && gt4_career_part_removable(low32(argv[0]))){
        if(!gt4_career_remove_part(body,(uint16_t)low32(argv[0])))return ah_error(vm,"invalid setup slot");
        if(!gt4_career_blob_set(vm,self,"full_car_ps2",body,sizeof body))return ah_error(vm,"part removal write failed");changed(vm,self);
    }*ret=ah_nil();return 0;
}
static const ah_host car_setting_host={"owned car",car_setting_get,car_setting_set,NULL,NULL,1};
static const char *const setting_names[]={"BrakeController","Suspension","LSD","Gear","NOS","DriveTrain","Ballast","ASCC","TCSC","RacingModify"};
static int car_tuning(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
 const char *name=data;uint8_t body[0x4b8];int group=-1;
 if(!gt4_game_car_body(vm,self,body))return ah_error(vm,"setting requires an owned car");
 for(int i=0;i<GT4_SETTING_GROUPS;i++)if(strstr(name,setting_names[i])){group=i;break;}
 if(group<0)return ah_error(vm,"unknown setting method");
 if(strstr(name,"Range")){
  int32_t values[42];int n;
  if(group==GT4_SETTING_GEAR){gt4_setting_details details={0};
   if(!setting_reader || !setting_reader(setting_reader_ctx,body,0,argc?(int)ah_num(argv[0]):-1,&details))return ah_error(vm,"original gear limits unavailable");
   memcpy(values,details.gear,sizeof details.gear);n=23;
  }else{uint8_t rows[0x142],valid[0x142];
   if(!gt4_career_blob_get(vm,self,"_setting_parts_ps2",rows,sizeof rows) || !gt4_career_blob_get(vm,self,"_setting_parts_valid_ps2",valid,sizeof valid))return ah_error(vm,"setting rows unavailable");
   n=gt4_setting_range(body,rows,valid,group,values);if(n<0)return ah_error(vm,"original setting row not resolved");
  }
  ah_obj *array=ah_array_new(vm);for(int i=0;i<n;i++)ah_array_push(vm,array,ah_int(values[i]));*ret=ah_objv(array);return 0;
 }
 int set=name[0]=='s',index=argc?(int)ah_num(argv[0]):-1;
 if(argc!=1+set)return ah_error(vm,"invalid setting arguments");
 if(set){
  if(gt4_setting_set(body,group,index,low32(argv[argc-1]))){
   if(!gt4_career_blob_set(vm,self,"full_car_ps2",body,sizeof body))return ah_error(vm,"setting write failed");changed(vm,self);
  }*ret=ah_nil();
 }else{int32_t value=-1;gt4_setting_get(body,group,index,&value);*ret=ah_int(value);}return 0;
}
/* 0013FED8 -> 00147D08/00147D50; original part cache is resolved by
   the menu's disc bridge, then each equipped row is loaded. */
static int car_begin_setting(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
 ah_val fn=member(vm,self,"_cacheSettingParts"),r;
 if(fn.t==AV_NIL || ah_call(vm,fn,0,NULL,&r)<0)return ah_error(vm,"setting part list unavailable");
 return car_setup_parts_buffer(vm,self,data,argc,argv,ret);
}
static int car_gear_auto(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
 (void)data;(void)argv;uint8_t body[0x4b8];gt4_setting_details d={0};
 if(argc || !gt4_game_car_body(vm,self,body) || !setting_reader || !setting_reader(setting_reader_ctx,body,1,-1,&d))return ah_error(vm,"automatic gear calculation unavailable");
 if(!gt4_career_blob_set(vm,self,"full_car_ps2",body,sizeof body))return ah_error(vm,"automatic gear write failed");changed(vm,self);*ret=ah_nil();return 0;
}
/* 00148B60 +0x18..0x530. PC representation of BinaryBuffer is an array
   of float pairs; normalization and endpoints retain original arithmetic. */
static int car_engine_curve(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
 (void)data;uint8_t body[0x4b8];gt4_setting_details d={0};
 if(!gt4_game_car_body(vm,self,body) || !setting_reader || !setting_reader(setting_reader_ctx,body,2,-1,&d))return ah_error(vm,"engine curve unavailable");
 int normalize=argc?ah_num(argv[0])!=0:1;float min=d.count?d.rpm[0]:0;
 float p=normalize?d.peak_power:(argc>1?(float)ah_num(argv[1]):1000.f);
 float t=normalize?d.peak_torque:(argc>2?(float)ah_num(argv[2]):200.f);
 if(!normalize){if(p<d.peak_power)p=d.peak_power;if(t<d.peak_torque)t=d.peak_torque;}
 ah_obj *result=ah_array_new(vm);
 for(int curve=0;curve<2;curve++){ah_obj *points=ah_array_new(vm);
  for(int i=0;i<d.count;i++){ah_obj *xy=ah_array_new(vm);
   ah_array_push(vm,xy,ah_float((d.rpm[i]-min)/(d.max_rpm-min)));
   ah_array_push(vm,xy,ah_float((curve?d.torque[i]:d.power[i])/(curve?t:p)));
   ah_array_push(vm,points,ah_objv(xy));}
  ah_array_push(vm,result,ah_objv(points));
 }
 /* 00148EC0/00148F1C box the measured peaks, not the supplied graph
    scales. In the expanded graph those scales can exceed the peaks. */
 ah_array_push(vm,result,ah_float(d.peak_power));ah_array_push(vm,result,ah_float(d.peak_torque));
 ah_array_push(vm,result,ah_float(min));ah_array_push(vm,result,ah_float(d.max_rpm));*ret=ah_objv(result);return 0;
}
static int car_setting_stats(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
 (void)argv;int32_t stats[3];if(argc)return ah_error(vm,"current car statistics takes no arguments");
 if(!gt4_game_car_stats(vm,self,stats)){ah_val cached=member(vm,self,!strcmp(data,"getCurrentWeight")?"weight":"power");
  *ret=!strcmp(data,"getCurrentWeight")?cached:ah_int((int32_t)((float)ah_num(cached)/100.f+.5f));return 0;}
 *ret=ah_int(!strcmp(data,"getCurrentWeight")?stats[2]:(int32_t)((float)stats[0]/100.f+.5f));return 0;
}
static int car_gear_chart(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
 (void)data;uint8_t body[0x4b8];gt4_setting_details d={0};
 if(!gt4_game_car_body(vm,self,body) || !setting_reader || !setting_reader(setting_reader_ctx,body,3,argc?(int)ah_num(argv[0]):-1,&d))return ah_error(vm,"gear chart unavailable");
 ah_obj *points=ah_array_new(vm);for(int i=0;i<d.chart_count;i++){ah_obj *xy=ah_array_new(vm);ah_array_push(vm,xy,ah_float(d.chart[i][0]));ah_array_push(vm,xy,ah_float(d.chart[i][1]));ah_array_push(vm,points,ah_objv(xy));}*ret=ah_objv(points);return 0;
}
void gt4_game_bind_car(ah_vm *vm,ah_obj *car){
 method(vm,car,"buildGearRatioData",car_gear_chart);method(vm,car,"getCurrentPower",car_setting_stats);
 static const char *const tuning_methods[]={"getBrakeControllerSetting","setBrakeControllerSetting","getSuspensionSetting","setSuspensionSetting","getLSDSetting","setLSDSetting","getGearSetting","setGearSetting","getNOSSetting","setNOSSetting","getDriveTrainSetting","setDriveTrainSetting","getBallastSetting","setBallastSetting","getASCCSetting","setASCCSetting","getTCSCSetting","setTCSCSetting","getRacingModifySetting","setRacingModifySetting","getBrakeControllerRange","getSuspensionRange","getLSDRange","getGearRange","getNOSRange","getDriveTrainRange","getBallastRange","getASCCRange","getTCSCRange","getRacingModifyRange"};
 for(unsigned i=0;i<sizeof tuning_methods/sizeof *tuning_methods;i++)method(vm,car,tuning_methods[i],car_tuning);
 method(vm,car,"beginSetting",car_begin_setting);method(vm,car,"setGearAuto",car_gear_auto);method(vm,car,"buildEngineCurveData",car_engine_curve);method(vm,car,"getCurrentWeight",car_setting_stats);ah_module_attach_host(car,&car_setting_host);method(vm,car,"isDefaultNoParts",car_default_no_parts);method(vm,car,"changeParts",car_change_parts);/* 0014452C aliases setParts to 001403A0. */method(vm,car,"setParts",car_change_parts);method(vm,car,"getParts",car_preview);method(vm,car,"getEquipedParts",car_part_query);method(vm,car,"isRemovableType",car_part_query);method(vm,car,"isReversibilityType",car_reversibility);method(vm,car,"isRemovedNormalType",car_part_query);method(vm,car,"setupPartsBuffer",car_setup_parts_buffer);method(vm,car,"endSetting",car_end_setting);/* 0014408C registers syncWheel with the same 0013FF38 callback. */method(vm,car,"syncWheel",car_end_setting);method(vm,car,"replaceOil",car_oil);method(vm,car,"getOilLifeRate",car_oil_life);method(vm,car,"getColorIndex",car_color);method(vm,car,"getUsedPrice",car_price);method(vm,car,"getUsedSellPrice",car_price);const char *ops[]={"getCarLabel","makeUsed","copyFrom","updateCurrentStatus","getOdometer","isHavingParts"};for(unsigned i=0;i<6;i++)method(vm,car,ops[i],car_preview);method(vm,car,"getGarageIndex",car_index);}
static int money(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){(void)data;
    if(argc!=1 || (argv[0].t!=AV_INT && argv[0].t!=AV_LONG))return ah_error(vm,"incMoney requires integer credits");
    ah_val current=member(vm,self,"money");
    if(current.t!=AV_INT && current.t!=AV_LONG)return ah_error(vm,"invalid credit balance");
    /* func_001638D8 -> func_0043AFB0. Preserve all 64 bits of daddu;
       affordability belongs to the authored purchase flow, not this method. */
    uint64_t bits=gt4_career_credit_add((uint64_t)current.u.i,(uint64_t)argv[0].u.i);
    int64_t result;memcpy(&result,&bits,sizeof result);
    ah_set_member(vm,self,"money",ah_int(result));ah_set_member(vm,self,"need_save",ah_int(1));changed(vm,self);*ret=ah_int(result);return 0;
}
static int enough_money(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
    (void)data;ah_val current=member(vm,self,"money");
    if(argc<1 || (argv[0].t!=AV_INT && argv[0].t!=AV_LONG) || (current.t!=AV_INT && current.t!=AV_LONG))
        return ah_error(vm,"isEnoughMoney requires integer credits");
    *ret=ah_int(gt4_career_money_enough(current.u.i,argv[0].u.i));return 0;
}
/* Adapt the PC's sparse car array to the original 1000 occupied flags.
   This validates slot selection/count only, not full car initialization. */
static void garage_flags(ah_obj *cars,uint8_t flags[0x7d10]){
    memset(flags,0,0x7d10);
    for(unsigned i=0;i<GT4_CAREER_GARAGE_CAPACITY;i++)
        if(ah_array_get(cars,(int)i).t!=AV_NIL)flags[0x10+i*0x20]=1;
}
static int count_cars(ah_obj *cars){uint8_t flags[0x7d10];garage_flags(cars,flags);return (int)gt4_career_garage_count(flags);}
static int car_index(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){(void)data;(void)argc;(void)argv;*ret=member(vm,self,"garage_index");return 0;}
static uint64_t car_bits(ah_vm *vm,ah_obj *car,const char *name,uint64_t fallback){
    ah_val value=member(vm,car,name);
    return value.t==AV_INT || value.t==AV_LONG?(uint64_t)value.u.i:fallback;
}
static void car_bits_set(ah_vm *vm,ah_obj *car,const char *name,uint64_t bits){
    int64_t value;memcpy(&value,&bits,8);ah_set_member(vm,car,name,ah_int(value));
}
/* Adapter for 00435388 / 00435418 / 00435690. Keep the history of
   unoccupied slots too: the original promotion loop does not skip them. */
static void garage_select_metadata(ah_vm *vm,ah_obj *garage,ah_obj *cars,int index,int change){
    uint8_t raw[0x81f8]={0};
    ah_obj *history=ah_as_obj(member(vm,garage,"original_unit_history"),AO_ARRAY);
    ah_obj *vacant=ah_as_obj(member(vm,garage,"vacant_slots"),AO_MODULE);
    for(unsigned i=0;i<1000;i++){
        ah_obj *entry=ah_as_obj(ah_array_get(cars,(int)i),AO_MODULE);char key[16];
        if(!entry){snprintf(key,sizeof key,"%u",i);entry=ah_as_obj(member(vm,vacant,key),AO_MODULE);}
        ah_val saved=ah_array_get(history,(int)i);
        uint64_t h=car_bits(vm,entry,"history",saved.t==AV_INT || saved.t==AV_LONG?(uint64_t)saved.u.i:1023u);
        uint64_t flags=car_bits(vm,entry,"flags",ah_array_get(cars,(int)i).t!=AV_NIL?1u:0u);
        for(unsigned j=0;j<8;j++){raw[0x20+i*32+j]=(uint8_t)(h>>(8*j));raw[0x10+i*32+j]=(uint8_t)(flags>>(8*j));}
    }
    int previous=(int)ah_num(member(vm,garage,"riding_index"));
    if(change && ah_truthy(member(vm,garage,"original_pending_change")) && previous>=0 && previous<1000)
        raw[0x10+previous*32]&=253;
    raw[0x10+index*32]|=2;gt4_career_garage_promote(raw,(uint32_t)index);
    ah_obj *updated=ah_array_new(vm);
    for(unsigned i=0;i<1000;i++){
        uint64_t h=0,flags=0;for(unsigned j=0;j<8;j++){h|=(uint64_t)raw[0x20+i*32+j]<<(8*j);flags|=(uint64_t)raw[0x10+i*32+j]<<(8*j);}
        int64_t signed_h;memcpy(&signed_h,&h,8);ah_array_push(vm,updated,ah_int(signed_h));
        ah_obj *entry=ah_as_obj(ah_array_get(cars,(int)i),AO_MODULE);char key[16];
        if(!entry){snprintf(key,sizeof key,"%u",i);entry=ah_as_obj(member(vm,vacant,key),AO_MODULE);}
        if(entry){car_bits_set(vm,entry,"history",h);car_bits_set(vm,entry,"flags",flags);}
    }
    ah_set_member(vm,garage,"original_unit_history",ah_objv(updated));
    if(change)ah_set_member(vm,garage,"original_pending_change",ah_int(0));
}
int gt4_game_car_unit(ah_vm *vm,ah_obj *car,uint8_t out[32]){
    if(!gt4_career_blob_get(vm,car,"compact_unit_ps2",out,32))memset(out,0,32);
    const char *fields[]={"code","table","acquisition_word","odometer"};const unsigned offsets[]={0,4,16,20};
    for(unsigned k=0;k<4;k++){uint32_t v=(uint32_t)car_bits(vm,car,fields[k],0);for(unsigned i=0;i<4;i++)out[offsets[k]+i]=(uint8_t)(v>>(8*i));}
    uint64_t flags=car_bits(vm,car,"flags",1),history=car_bits(vm,car,"history",1023);
    for(unsigned i=0;i<8;i++){out[8+i]=(uint8_t)(flags>>(8*i));out[24+i]=(uint8_t)(history>>(8*i));}return 1;
}
int gt4_game_car_body(ah_vm *vm,ah_obj *car,uint8_t out[0x4b8]){
    if(!gt4_career_blob_get(vm,car,"full_car_ps2",out,0x4b8))return 0;
    ah_obj *wear=ah_as_obj(member(vm,car,"oil_state"),AO_ARRAY);
    if(ah_array_size(wear)==32)for(unsigned i=0;i<32;i++)out[0x498+i]=(uint8_t)ah_array_get(wear,(int)i).u.i;
    return 1;
}
static uint64_t bytes_u64(const uint8_t *p){uint64_t value=0;for(unsigned i=0;i<8;i++)value|=(uint64_t)p[i]<<(8*i);return value;}
static void install_constructed_car(ah_vm *vm,ah_obj *created,const uint8_t unit[32],const uint8_t body[0x4b8]){
    gt4_career_blob_set(vm,created,"compact_unit_ps2",unit,32);gt4_career_blob_set(vm,created,"full_car_ps2",body,0x4b8);
    ah_set_member(vm,created,"code",ah_int((int32_t)(uint32_t)bytes_u64(unit)));ah_set_member(vm,created,"table",ah_int((int32_t)(uint32_t)(bytes_u64(unit)>>32)));
    uint64_t flags=bytes_u64(unit+8);car_bits_set(vm,created,"flags",flags);car_bits_set(vm,created,"history",bytes_u64(unit+24));
    ah_set_member(vm,created,"acquisition_word",ah_int((uint32_t)bytes_u64(unit+16)));ah_set_member(vm,created,"odometer",ah_int((uint32_t)(bytes_u64(unit+16)>>32)));
    ah_set_member(vm,created,"variation",ah_int((flags>>2)&127));ah_set_member(vm,created,"power",ah_int((flags>>9)&0x3ffff));ah_set_member(vm,created,"weight",ah_int((flags>>32)&8191));
    unsigned color=body[0x18];ah_set_member(vm,created,"color_index",ah_int(color<128?(int)color:(int)color-256));
    ah_obj *wear=ah_array_new(vm);for(unsigned i=0;i<32;i++)ah_array_push(vm,wear,ah_int(body[0x498+i]));ah_set_member(vm,created,"oil_state",ah_objv(wear));
}
int gt4_game_construct_car(ah_vm *vm,ah_obj *car,const char *label,int32_t color,uint64_t mileage){
    uint8_t unit[32]={0},body[0x4b8];
    if(!car_factory || !car || !label || !car_factory(car_factory_ctx,label,color,mileage,0,unit,body))return 0;
    install_constructed_car(vm,car,unit,body);ah_set_member(vm,car,"car_code",ah_str(vm,label));gt4_game_bind_car(vm,car);return 1;
}
static int car_preview(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
    const char *op=data;*ret=ah_nil();ret->t=AV_VOID;
    if(!strcmp(op,"getCarLabel")){*ret=member(vm,self,"car_code");return 0;}
    if(!strcmp(op,"makeUsed")){
        /* func_0013E8E8 -> 00440FB8: negative color/meter use the original
           seeded choices; explicit nonnegative values leave the seed unused. */
        if(argc<1 || argc>3)return ah_error(vm,"makeUsed requires seed and optional color/mileage");
        /* 0013E920..0013E984 invokes each argument's +0x58 integer
           conversion; weekly stock stores paint indices as strings. */
        ah_val converted[3];
        for(int i=0;i<argc;i++){
            converted[i]=argv[i];
            if(ah_cstr(argv[i])){ah_val fn;if(!ah_get_member(vm,argv[i].u.o,"toInt",&fn) || ah_call(vm,fn,0,NULL,&converted[i])<0)return ah_error(vm,"makeUsed conversion failed");}
            if(converted[i].t!=AV_INT && converted[i].t!=AV_LONG)return ah_error(vm,"makeUsed requires convertible integer arguments");
        }
        argv=converted;
        const char *label=ah_cstr(member(vm,self,"car_code"));
        uint8_t unit[32]={0},body[0x4b8];int32_t color=argc>1?low32(argv[1]):-1;uint64_t mileage=argc>2?(uint64_t)argv[2].u.i:UINT64_MAX;
        if(!car_factory || !car_factory(car_factory_ctx,label,color,mileage,low32(argv[0]),unit,body))return ah_error(vm,"used preview construction failed");
        install_constructed_car(vm,self,unit,body);
    }else if(!strcmp(op,"copyFrom")){
        /* func_0013C148 -> 00145970 -> 0043DE40: copy live car state,
           preserving the destination host object's identity/callbacks. */
        ah_obj *source=argc==1?ah_as_obj(argv[0],AO_MODULE):NULL;
        if(!source){if(argc==1 && argv[0].t==AV_NIL)return 0;return ah_error(vm,"copyFrom requires a car");}
        uint8_t body[0x4b8];if(gt4_game_car_body(vm,source,body))gt4_career_blob_set(vm,self,"full_car_ps2",body,sizeof body);
        const char *fields[]={"car_code","color_index","variation","odometer","oil_state","power","weight","original_base_price","name","short_name","garage_index"};
        for(unsigned i=0;i<sizeof fields/sizeof *fields;i++){ah_val v=member(vm,source,fields[i]);if(v.t!=AV_NIL)ah_set_member(vm,self,fields[i],v);}
    }else if(!strcmp(op,"isHavingParts") || !strcmp(op,"getParts")){
        /* func_0043F3A8 / 0043F4A8: owned bit at body + 0x48c. */
        uint8_t body[0x4b8];if(argc!=2 || !original_parts_ready || !gt4_game_car_body(vm,self,body))return ah_error(vm,"original owned-part data unavailable");
        for(int i=0;i<2;i++)if(argv[i].t!=AV_INT && argv[i].t!=AV_LONG)return ah_error(vm,"isHavingParts requires integer arguments");
        uint32_t type=(uint32_t)argv[0].u.i;int32_t base=type>=1 && type<=31?original_parts.owned_bases[type-1]:-1;
        uint32_t bit=(uint32_t)base+(uint32_t)argv[1].u.i;
        /* Original uses signed sra, including a negative base for unknown types. */
        int64_t signed_bit=bit<=INT32_MAX?(int64_t)bit:(int64_t)bit-INT64_C(0x100000000);
        int64_t offset=0x48c+(signed_bit>=0?signed_bit/8:-((-signed_bit+7)/8));
        if(offset<0 || offset>=(int64_t)sizeof body)return ah_error(vm,"owned-part index outside native car body");
        if(!strcmp(op,"getParts")){
            /* 00140608 -> 0043F368 -> 0043F470: signed bit <96;
               acquisition is separate from installation and price. */
            if(signed_bit<96){body[offset]|=(uint8_t)(1u<<(bit&7));
                if(!gt4_career_blob_set(vm,self,"full_car_ps2",body,sizeof body))return ah_error(vm,"owned parts write failed");changed(vm,self);}
            *ret=ah_nil();
        }else *ret=ah_int((body[offset]>>(bit&7))&1);
    }else if(!strcmp(op,"getOdometer")){
        /* Formatting is PC presentation; the saved original meter stays u32. */
        uint32_t meter=(uint32_t)car_bits(vm,self,"odometer",0);
        /* 00146528 +0x1c..0x24 -> 003434C8 reads live wear +4a0;
           native serialized full bodies omit the eight-byte class header. */
        uint8_t body[0x4b8];if(gt4_game_car_body(vm,self,body))memcpy(&meter,body+0x498,4);
        if(argc){char text[64];snprintf(text,sizeof text,"%.1f km",(double)meter/1000.0);*ret=ah_str(vm,text);}else *ret=ah_int(meter);
    }else if(!strcmp(op,"updateCurrentStatus")){
        /* 00146648 +78/+CC/+EC caches PS hundredths, torque and kg;
           Serialized compact flags (+8) use the existing compact_ratings layout;
           retain unrelated paint/history bits. */
        int32_t stats[3];if(gt4_game_car_stats(vm,self,stats)){
            uint64_t flags=car_bits(vm,self,"flags",1);
            flags=(flags & ~(UINT64_C(0x3ffff)<<9))|((uint64_t)((uint32_t)stats[0]&0x3ffff)<<9);
            flags=(flags & ~(UINT64_C(8191)<<32))|((uint64_t)((uint32_t)stats[2]&8191)<<32);
            car_bits_set(vm,self,"flags",flags);ah_set_member(vm,self,"power",ah_int(stats[0]));
            ah_set_member(vm,self,"weight",ah_int(stats[2]));
        }
    }else return ah_error(vm,"unsupported car preview operation");
    return 0;
}
static int purchase_day(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
    (void)self;(void)argc;(void)argv;ah_obj *date=ah_as_obj(member(vm,ah_as_obj(member(vm,data,"calendar"),AO_MODULE),"today"),AO_ARRAY);
    if(!original_calendar_ready || ah_array_size(date)!=3)return ah_error(vm,"original purchase date unavailable");
    *ret=ah_int(gt4_career_date_to_day(&original_calendar,low32(ah_array_get(date,0)),low32(ah_array_get(date,1)),low32(ah_array_get(date,2))));return 0;
}
static int garage(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){const char *op=data;ah_obj *cars=array(vm,self,"cars");int n=count_cars(cars);
    int riding=(int)ah_num(member(vm,self,"riding_index"));ah_val car=ah_array_get(cars,riding);
    /* 0016EAA0: flush the riding full body before swapping cars.
       Native PC profiles retain this body directly in the owned slot. */
    if(!strcmp(op,"pushRidingCar")){
        if(car.t!=AV_NIL){uint8_t body[0x4b8];if(!gt4_game_car_body(vm,car.u.o,body))return ah_error(vm,"riding body unavailable");
            gt4_career_blob_set(vm,self,"current_car_ps2",body,sizeof body);changed(vm,self);}
        *ret=ah_str(vm,"OK");return 0;
    }
    /* 0016E7B0 +0xa8..0x10c: selected car copies the live full body;
       +0x110..0x124 loads the other owned body before returning OK.
       PC storage already retains each complete body in the owned slot. */
    if(!strcmp(op,"popGarageCar")){
        if(argc<2 || !ah_as_obj(argv[1],AO_MODULE))return ah_error(vm,"garage load requires index and target");
        int index=(int)ah_num(argv[0]);ah_val source=ah_array_get(cars,index);uint8_t body[0x4b8];
        if(source.t==AV_NIL || !gt4_game_car_body(vm,source.u.o,body))return ah_error(vm,"owned garage body unavailable");
        gt4_career_blob_set(vm,argv[1].u.o,"full_car_ps2",body,sizeof body);
        *ret=ah_str(vm,"OK");return 0;
    }
    if(!strcmp(op,"addNewCar") || !strcmp(op,"addUsedCar")){
        ah_val purchase_args[2];uint64_t mileage=0;int used=!strcmp(op,"addUsedCar");
        if(used){
            ah_obj *source=argc==1?ah_as_obj(argv[0],AO_MODULE):NULL;if(!source)return ah_error(vm,"addUsedCar requires an original car object");
            purchase_args[0]=member(vm,source,"car_code");if(!ah_cstr(purchase_args[0]))purchase_args[0]=member(vm,source,"code");purchase_args[1]=member(vm,source,"color_index");
            if(purchase_args[1].t==AV_NIL)purchase_args[1]=member(vm,source,"variation");
            ah_val miles=member(vm,source,"odometer");if(miles.t!=AV_INT && miles.t!=AV_LONG)return ah_error(vm,"used-car mileage unavailable");
            mileage=(uint32_t)miles.u.i;argv=purchase_args;argc=2;
        }
        const char *code=argc?ah_cstr(argv[0]):NULL;
        if(argc!=2 || !code || !*code || strlen(code)>64 || (argv[1].t!=AV_INT && argv[1].t!=AV_LONG) || argv[1].u.i<0 || argv[1].u.i>INT_MAX || n>=1000)return ah_error(vm,"invalid garage insertion");
        uint8_t flags[0x7d10];garage_flags(cars,flags);
        int index=gt4_career_garage_first_free(flags);
        if(index<0)return ah_error(vm,"garage is full");
        ah_obj *created=ah_module_new(vm,"car",NULL,NULL,NULL);ah_set_member(vm,created,"car_code",argv[0]);ah_set_member(vm,created,"color_index",argv[1]);ah_set_member(vm,created,"garage_index",ah_int(index));
        /* 00434D80 starts a reused slot at history sentinel 0x3ff,
           preserving the high word while clearing low-word non-position bits. */
        char previous_key[16];snprintf(previous_key,sizeof previous_key,"%d",index);
        ah_obj *previous_slot=ah_as_obj(member(vm,ah_as_obj(member(vm,self,"vacant_slots"),AO_MODULE),previous_key),AO_MODULE);
        ah_obj *histories=ah_as_obj(member(vm,self,"original_unit_history"),AO_ARRAY);
        ah_val old_history=ah_array_get(histories,index);
        uint64_t h=car_bits(vm,previous_slot,"history",old_history.t==AV_INT || old_history.t==AV_LONG?(uint64_t)old_history.u.i:1023u);
        car_bits_set(vm,created,"history",(h&UINT64_C(0xffffffff000003ff))|1023u);
        ah_val name=argv[0],resolver=member(vm,self,"_getCarName");if(resolver.t!=AV_NIL)ah_call(vm,resolver,1,argv,&name);
        ah_set_member(vm,created,"name",name);ah_set_member(vm,created,"short_name",name);method(vm,created,"getGarageIndex",car_index);
        gt4_game_bind_car(vm,created);ah_val binder=member(vm,self,"_bindCar"),bound;
        if(binder.t!=AV_NIL){ah_val arg=ah_objv(created);if(ah_call(vm,binder,1,&arg,&bound)<0)return ah_error(vm,"car metadata binding failed");}
        if(car_factory){
            uint8_t unit[32]={0},body[0x4b8],units[32000];
            if(gt4_career_blob_get(vm,self,"original_units_ps2",units,sizeof units))memcpy(unit,units+index*32,32);
            if(previous_slot)gt4_game_car_unit(vm,previous_slot,unit);
            ah_val get_day=member(vm,self,"_purchaseDay"),day;
            if(!car_factory(car_factory_ctx,code,low32(argv[1]),mileage,0,unit,body) || get_day.t==AV_NIL || ah_call(vm,get_day,0,NULL,&day)<0)
                return ah_error(vm,"original car construction failed");
            gt4_career_car_acquisition_day(unit,&original_calendar,low32(day));install_constructed_car(vm,created,unit,body);
        }else if(used)return ah_error(vm,"native car factory unavailable");
        ah_obj *updated=ah_array_new(vm);int size=ah_array_size(cars);if(size<=index)size=index+1;
        for(int i=0;i<size;i++)ah_array_push(vm,updated,i==index?ah_objv(created):ah_array_get(cars,i));
        ah_set_member(vm,self,"cars",ah_objv(updated));ah_set_member(vm,self,"count",ah_int(n+1));*ret=ah_int(index);
        char slot_key[16];snprintf(slot_key,sizeof slot_key,"%d",index);ah_remove_member(vm,module(vm,self,"vacant_slots"),slot_key);
    }
    else if(!strcmp(op,"hasRidingCar"))*ret=ah_bool(car.t!=AV_NIL);
    else if(!strcmp(op,"isFull"))*ret=ah_bool(n>=1000);
    else if(!strcmp(op,"getCount"))*ret=ah_int(n);
    else if(!strcmp(op,"delCar")){
        if(argc!=1 || (argv[0].t!=AV_INT && argv[0].t!=AV_LONG) || argv[0].u.i<0 || argv[0].u.i>=GT4_CAREER_GARAGE_CAPACITY)return ah_error(vm,"invalid garage deletion");
        int index=(int)argv[0].u.i;ah_val removed=ah_array_get(cars,index);
        uint8_t original[0x81cc]={0};garage_flags(cars,original);uint32_t selected=(uint32_t)riding;
        ah_val saved_flags=removed.t!=AV_NIL?member(vm,removed.u.o,"flags"):ah_nil();
        if(saved_flags.t==AV_INT || saved_flags.t==AV_LONG)for(unsigned i=0;i<8;i++)original[0x10+index*32+i]=(uint8_t)((uint64_t)saved_flags.u.i>>(8*i));
        for(unsigned i=0;i<4;i++)original[0x81c8+i]=(uint8_t)(selected>>(8*i));
        int was_selected=gt4_career_garage_delete(original,(uint32_t)index);
        /* The original clears occupied, retaining slot metadata. Keep that
           data outside the PC's sparse occupied array until the slot is reused. */
        if(removed.t!=AV_NIL){
            ah_obj *car=ah_as_obj(removed,AO_MODULE);uint64_t bits=0;
            for(unsigned i=0;i<8;i++)bits|=(uint64_t)original[0x10+index*32+i]<<(8*i);
            int64_t value;memcpy(&value,&bits,8);ah_set_member(vm,car,"flags",ah_int(value));
            char slot_key[16];snprintf(slot_key,sizeof slot_key,"%d",index);ah_set_member(vm,module(vm,self,"vacant_slots"),slot_key,removed);
        }
        ah_obj *updated=ah_array_new(vm);for(int i=0;i<ah_array_size(cars);i++)ah_array_push(vm,updated,i==index?ah_nil():ah_array_get(cars,i));
        ah_set_member(vm,self,"cars",ah_objv(updated));ah_set_member(vm,self,"count",ah_int(n-(removed.t!=AV_NIL)));
        if(was_selected){ah_set_member(vm,self,"riding_index",ah_int(-1));ah_set_member(vm,self,"riding_car",ah_nil());}
        *ret=ah_bool(was_selected);
    }
    else if(!strcmp(op,"delAllCarExceptRidingCar")){
        ah_obj *updated=ah_array_new(vm);for(int i=0;i<ah_array_size(cars);i++)ah_array_push(vm,updated,i==riding?car:ah_nil());
        ah_set_member(vm,self,"cars",ah_objv(updated));ah_set_member(vm,self,"count",ah_int(car.t!=AV_NIL));*ret=ah_bool(1);
    }
    else if(!strcmp(op,"cancelRidingCar")){ah_set_member(vm,self,"riding_index",ah_int(-1));ah_set_member(vm,self,"riding_car",ah_nil());*ret=ah_bool(1);}
    else if(!strcmp(op,"setRidingCar") || !strcmp(op,"changeRidingCar")){
        if(argc<1 || (argv[0].t!=AV_INT && argv[0].t!=AV_LONG) || argv[0].u.i<0 || argv[0].u.i>=ah_array_size(cars))return ah_error(vm,"garage selection requires a valid index");int index=(int)argv[0].u.i;ah_val selected=ah_array_get(cars,index);
        if(index<0 || index>=ah_array_size(cars) || selected.t==AV_NIL)return ah_error(vm,"garage index is empty");
        if(argc>2 || (argc==2 && !ah_as_obj(argv[1],AO_MODULE)))return ah_error(vm,"garage selection requires a car object");
        garage_select_metadata(vm,self,cars,index,!strcmp(op,"changeRidingCar"));
        ah_obj *source=argc==2?argv[1].u.o:selected.u.o;
        uint8_t body[0x4b8];
        if(gt4_game_car_body(vm,source,body)){
            gt4_career_blob_set(vm,self,"current_car_ps2",body,sizeof body);
            gt4_career_blob_set(vm,selected.u.o,"full_car_ps2",body,sizeof body);
            ah_val wear=member(vm,source,"oil_state");if(wear.t!=AV_NIL)ah_set_member(vm,selected.u.o,"oil_state",wear);
        }else if(gt4_game_car_body(vm,selected.u.o,body))gt4_career_blob_set(vm,self,"current_car_ps2",body,sizeof body);
        else ah_remove_member(vm,self,"current_car_ps2");
        ah_set_member(vm,self,"riding_index",ah_int(index));ah_set_member(vm,self,"riding_car",selected);*ret=!strcmp(op,"changeRidingCar")?car:selected;
        if(argc==2){*ret=ah_nil();ret->t=AV_VOID;}
    }else return ah_error(vm,"unsupported garage operation");
    if(strcmp(op,"hasRidingCar") && strcmp(op,"isFull") && strcmp(op,"getCount"))changed(vm,self);
    return 0;
}
/* MRaceRecordUnit: hooks read/write the authoritative saved 32-bit word.
   Enum strings are loaded from CORE.GT4; derived attributes are not saved. */
static uint32_t unit_word(ah_vm *vm,ah_obj *unit){return (uint32_t)member(vm,unit,"original_record").u.i;}
static int unit_get(ah_vm *vm,ah_obj *unit,const char *sym,ah_val *out){
    if(strcmp(sym,"type") && strcmp(sym,"result") && strcmp(sym,"present_result") && strcmp(sym,"spot_result") && strcmp(sym,"best_score"))return 0;
    uint32_t word=unit_word(vm,unit);const gt4_career_race_lookup_data *rules=gt4_game_race_lookup_data();
    if(!strcmp(sym,"best_score")){*out=ah_int((word>>16)&255u);return 1;} /* 001A5518 */
    if(!rules)return 0;
    if(!strcmp(sym,"type")){unsigned kind=word&15u;*out=kind<4?ah_str(vm,rules->type_names[kind]):ah_nil();return 1;} /* 001A4B08 */
    unsigned shift=!strcmp(sym,"result")?4:!strcmp(sym,"present_result")?8:!strcmp(sym,"spot_result")?12:32;
    if(shift==32)return 0;
    const char *name=gt4_career_result_name(rules,(word>>shift)&15u);*out=name?ah_str(vm,name):ah_nil();return 1;
}
static int unit_set(ah_vm *vm,ah_obj *unit,const char *sym,ah_val value){
    /* 001A5518 is a getter-only attribute: updateBestScore is the writer. */
    if(!strcmp(sym,"best_score"))return 1;
    unsigned field=!strcmp(sym,"type")?0:!strcmp(sym,"result")?1:!strcmp(sym,"present_result")?2:!strcmp(sym,"spot_result")?3:4;
    if(field==4)return 0;
    const gt4_career_race_lookup_data *rules=gt4_game_race_lookup_data();if(!rules)return 1;
    char text[64];const char *name=ah_to_text(value,text,sizeof text);
    uint32_t v=field?(uint32_t)gt4_career_result_grade(rules,name):(uint32_t)gt4_career_record_type(rules,name);
    uint32_t old=unit_word(vm,unit),next=gt4_career_record_field_set(old,field,v);
    ah_set_member(vm,unit,"original_record",ah_int(next));if(next!=old)changed(vm,unit);return 1;
}
static const ah_host record_unit_host={"MRaceRecordUnit",unit_get,unit_set,NULL,NULL,1};
static int unit_method(ah_vm *vm,ah_obj *unit,void *user,int argc,const ah_val *argv,ah_val *ret){
    const char *op=user;const gt4_career_race_lookup_data *rules=gt4_game_race_lookup_data();
    uint32_t word=unit_word(vm,unit),old=word;*ret=ah_nil();ret->t=AV_VOID;
    if(!strcmp(op,"refreshResult"))word=gt4_career_record_present(word); /* 001A5350 */
    else if(!strcmp(op,"updateBestScore")){ /* 001A55B0 -> 00430A20 */
        if(argc<1 || (argv[0].t!=AV_INT && argv[0].t!=AV_LONG))return ah_error(vm,"updateBestScore requires an integer");
        gt4_career_record_score(&word,argv[0].u.i);
    }else { /* 001A5388 -> 00430990 / 00431C60 / 00430A58 */
        if(argc<2 || !rules)return ah_error(vm,"record update requires original enums and type/result");
        char type[64],grade[64];int kind=gt4_career_record_type(rules,ah_to_text(argv[0],type,sizeof type));
        unsigned result=gt4_career_result_grade(rules,ah_to_text(argv[1],grade,sizeof grade));
        gt4_career_record_update(&word,kind,result);
    }
    ah_set_member(vm,unit,"original_record",ah_int(word));if(word!=old)changed(vm,unit);return 0;
}
void gt4_game_bind_record_unit(ah_vm *vm,ah_obj *unit){
    const gt4_career_race_lookup_data *rules=gt4_game_race_lookup_data();if(!unit || !rules)return;
    if(ah_host_of(unit)==&record_unit_host)return;
    ah_val raw=member(vm,unit,"original_record");
    if(raw.t!=AV_INT && raw.t!=AV_LONG){
        uint32_t word=gt4_career_record_init(0);
        /* Compatibility with the prior PC schema: its result-only units
           represented license records. Imported words bypass this migration. */
        if(member(vm,unit,"type").t==AV_NIL && ah_cstr(member(vm,unit,"result")))word=gt4_career_record_field_set(word,0,1);
        const char *fields[]={"type","result","present_result","spot_result"};
        for(unsigned i=0;i<4;i++){const char *name=ah_cstr(member(vm,unit,fields[i]));if(name)word=gt4_career_record_field_set(word,i,i?gt4_career_result_grade(rules,name):(uint32_t)gt4_career_record_type(rules,name));}
        ah_val score=member(vm,unit,"best_score");if(score.t==AV_INT || score.t==AV_LONG)word=(word&0xff00ffffu)|(((uint32_t)score.u.i&255u)<<16);
        ah_set_member(vm,unit,"original_record",ah_int(word));
    }
    if(!ah_module_attach_host(unit,&record_unit_host))return;
    const char *fields[]={"type","result","present_result","spot_result","best_score"};
    for(unsigned i=0;i<5;i++)ah_remove_member(vm,unit,fields[i]);
    method(vm,unit,"refreshResult",unit_method);method(vm,unit,"update",unit_method);method(vm,unit,"updateBestScore",unit_method);
}
static ah_obj *record_unit(ah_vm *vm,ah_obj *self,const char *code){ah_obj *units=module(vm,self,"units");ah_obj *unit=ah_as_obj(member(vm,units,code),AO_MODULE);
    /* Resolve against the current indexed records on every query. Native save
       serialization can restore cached units as separate objects; those must
       not shadow the imported authoritative record after a load. */
    {
        ah_val resolver=member(vm,self,"_getRecordIndex"),index=ah_nil();
        if(resolver.t!=AV_NIL){ah_val name=ah_str(vm,code);
            if(ah_call(vm,resolver,1,&name,&index)>=0 && (index.t==AV_INT || index.t==AV_LONG) && index.u.i>=0 && index.u.i<768){
                ah_obj *records=ah_as_obj(member(vm,self,"records"),AO_ARRAY);
                ah_obj *imported=ah_as_obj(ah_array_get(records,(int)index.u.i),AO_MODULE);
                if(imported)unit=imported;
            }else if(gt4_game_race_lookup_data())return NULL;
        }
    }

    if(!unit){
        unit=module(vm,units,code);
        if(gt4_game_race_lookup_data())ah_set_member(vm,unit,"original_record",ah_int(gt4_career_record_init(0)));
        else {ah_set_member(vm,unit,"result",ah_str(vm,"NONE"));ah_set_member(vm,unit,"spot_result",ah_str(vm,"NONE"));ah_set_member(vm,unit,"best_score",ah_int(0));ah_set_member(vm,unit,"present_result",ah_str(vm,"NONE"));}
    }
    ah_val raw=member(vm,unit,"original_record");
    if((raw.t==AV_INT || raw.t==AV_LONG) && ((uint32_t)raw.u.i&15u)==1u && member(vm,unit,"result").t==AV_NIL){
        unsigned grade=((uint32_t)raw.u.i>>4)&15u;
        const char *medal=grade==9?"gold":grade==10?"silver":grade==11?"bronze":grade==12?"red":"NONE";
        ah_set_member(vm,unit,"result",ah_str(vm,medal));
    }
    gt4_game_bind_record_unit(vm,unit);
    ah_val dirty=member(vm,self,"_markDirty");if(dirty.t!=AV_NIL)ah_set_member(vm,unit,"_markDirty",dirty);
    return unit;
}
/* Preserve an imported original record word when supplied. Legacy PC medal
   strings are adapted only at the boundary, never used as comparison ranks. */
static uint32_t license_record_bits(ah_vm *vm,ah_obj *unit){
    ah_val raw=member(vm,unit,"original_record");
    if(raw.t==AV_INT || raw.t==AV_LONG)return (uint32_t)raw.u.i;
    const char *result=ah_cstr(member(vm,unit,"result"));unsigned grade=15;
    if(result && gt4_game_race_lookup_data())return 1u|(gt4_career_result_grade(gt4_game_race_lookup_data(),result)<<4);
    if(result){if(!strcmp(result,"gold"))grade=9;else if(!strcmp(result,"silver"))grade=10;else if(!strcmp(result,"bronze"))grade=11;else if(!strcmp(result,"red"))grade=12;}
    return 1u|(grade<<4);
}
static void license_rank_set(ah_vm *vm,ah_obj *ranks,int index,ah_val value){
    ah_val reference=ah_int(index);reference.t=AV_REF_ELEM;reference.kt=AV_INT;reference.c=ranks;
    ah_reference_set(vm,reference,value);
}
static ah_obj *license_ranks(ah_vm *vm,ah_obj *record,const char *code){
    int index=gt4_career_license_index(code);if(index<0)return NULL;
    ah_obj *all=array(vm,record,"records");
    while(ah_array_size(all)<=index)ah_array_push(vm,all,ah_objv(ah_array_new(vm)));
    return ah_as_obj(ah_array_get(all,index),AO_ARRAY);
}
/* 004321B8/00432268/0042E478/0042E660: authored RankingRoot reads
   the ten attempt records, independently of MRaceRecord's medal word. */
static int license_rank_get(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
    const char *op=data;ah_obj *entries=ah_as_obj(member(vm,self,"entries"),AO_ARRAY);
    int count=0;for(int i=0;i<10 && i<ah_array_size(entries);i++)if((uint32_t)ah_num(member(vm,ah_array_get(entries,i).u.o,"time"))!=0x157529ffu)count=i+1;
    if(!strcmp(op,"getRankNum")){*ret=ah_int(count);return 0;}
    int i=argc?(int)ah_num(argv[0]):0;ah_obj *entry=i>=0 && i<count?ah_as_obj(ah_array_get(entries,i),AO_MODULE):NULL;
    uint32_t time=entry?(uint32_t)ah_num(member(vm,entry,"time")):0x157529ffu;char text[96];
    if(!strcmp(op,"getResult")){
        const char *name=entry?gt4_career_result_name(gt4_game_race_lookup_data(),(unsigned)ah_num(member(vm,entry,"result"))):"NONE";
        *ret=ah_str(vm,name?name:"NONE");return 0;
    }
    if(!strcmp(op,"getDate")){
        if(!entry)snprintf(text,sizeof text,"\x7f/\x7f/\x7f"); /* 0042E660, 006A4FC0 */
        else {uint32_t date=(uint32_t)ah_num(member(vm,entry,"date"));snprintf(text,sizeof text,"%d/%d/%d",(int16_t)date,(date>>16)&255,date>>24);}
    }else if(time==0x157529ffu)snprintf(text,sizeof text,"\x7f\x7f'\x7f\x7f.\x7f\x7f\x7f");
    else if(time/3600000)snprintf(text,sizeof text,"%u:%02u'%02u.%03u",time/3600000,(time/60000)%60,(time/1000)%60,time%1000);
    else snprintf(text,sizeof text,"%u'%02u.%03u",time/60000,(time/1000)%60,time%1000); /* 0042E478 */
    *ret=ah_str(vm,text);return 0;
}
static int license_get(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
    (void)data;const char *code=argc?ah_cstr(argv[0]):NULL;
    ah_obj *entries=license_ranks(vm,self,code);if(!entries){*ret=ah_nil();return 0;}
    ah_obj *unit=ah_module_new(vm,"MLicenseRecordUnit",NULL,NULL,NULL);ah_set_member(vm,unit,"entries",ah_objv(entries));
    const char *methods[]={"getRankNum","getBestTime","getDate","getResult"};
    for(unsigned i=0;i<4;i++)method(vm,unit,methods[i],license_rank_get);
    *ret=ah_objv(unit);return 0;
}
uint32_t gt4_game_license_best(ah_vm *vm,ah_obj *game,const char *code){
    /* 00432268: first ranking slot contains the persistent best time. */
    ah_obj *ranks=license_ranks(vm,module(vm,game,"license_record"),code);
    ah_obj *first=ranks && ah_array_size(ranks)?ah_as_obj(ah_array_get(ranks,0),AO_MODULE):NULL;
    return first?(uint32_t)ah_num(member(vm,first,"time")):0x157529ffu;
}
int gt4_game_license_begin(ah_vm *vm,ah_obj *game,const char *code,gt4_career_license_attempts *visit){
    if(!vm || !game || !code || !visit || gt4_career_license_index(code)<0)return 0;
    ah_obj *unit=record_unit(vm,module(vm,game,"race_record"),code);if(!unit)return 0;
    visit->record=(uint32_t)member(vm,unit,"original_record").u.i;visit->best_grade=13;visit->best_time=0x157529ffu;return 1;
}
int gt4_game_license_attempt(ah_vm *vm,ah_obj *game,const char *code,
    gt4_career_license_attempts *visit,int32_t state,uint32_t time,uint32_t date){
    if(!vm || !game || !code || !visit)return 0;
    ah_obj *unit=record_unit(vm,module(vm,game,"race_record"),code);
    ah_obj *ranks=license_ranks(vm,module(vm,game,"license_record"),code);if(!unit || !ranks)return 0;
    uint32_t grade=gt4_career_license_attempt(visit,state,time);if(grade==13)return 1;
    ah_set_member(vm,unit,"original_record",ah_int(visit->record));changed(vm,game);
    /* 00432040: ties follow existing times. Replay-description bytes and
       ghost attachment (0010F718) remain pending, not fabricated here. */
    int at=0;while(at<10 && at<ah_array_size(ranks)){
        uint32_t old=(uint32_t)ah_num(member(vm,ah_array_get(ranks,at).u.o,"time"));
        if(old==0x157529ffu || time<old)break;at++;
    }
    if(at<10){
        while(ah_array_size(ranks)<10){ah_obj *empty=ah_module_new(vm,"license",NULL,NULL,NULL);ah_set_member(vm,empty,"time",ah_int(0x157529ffu));ah_array_push(vm,ranks,ah_objv(empty));}
        for(int i=9;i>at;i--)license_rank_set(vm,ranks,i,ah_array_get(ranks,i-1));
        ah_obj *entry=ah_module_new(vm,"license",NULL,NULL,NULL);
        ah_set_member(vm,entry,"time",ah_int(time));ah_set_member(vm,entry,"date",ah_int(date));ah_set_member(vm,entry,"result",ah_int(grade));
        license_rank_set(vm,ranks,at,ah_objv(entry));
    }return 1;
}
static int record(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){const char *op=data,*category=argc?ah_cstr(argv[0]):NULL;
    if(!strcmp(op,"isMissionClear")){
        /* func_001A6620 accepts a mission range string. Without a comma,
           it checks missions 1..40, not one event code. Optional result
           name goes through the original runtime enum (default grade 0). */
        char range[64];const char *text=argc?ah_to_text(argv[0],range,sizeof range):NULL;
        if(!text || strlen(text)>=sizeof range)return ah_error(vm,"mission range exceeds original buffer");
        if(text!=range)snprintf(range,sizeof range,"%s",text);
        int32_t first=1,last=40;char *comma=strchr(range,',');
        if(comma){*comma=0;uint32_t low=(uint32_t)strtoll(range,NULL,10);memcpy(&first,&low,4);
            low=(uint32_t)strtoll(comma+1,NULL,10);memcpy(&last,&low,4);}
        const gt4_career_race_lookup_data *rules=gt4_game_race_lookup_data();
        if(argc>1 && !rules)return ah_error(vm,"original result-name enum unavailable");
        char grade_text[64];unsigned limit=argc>1?gt4_career_result_grade(rules,ah_to_text(argv[1],grade_text,sizeof grade_text)):0;
        int passed=0;
        for(int64_t i=first;i<=last;i++){
            char code[64];snprintf(code,sizeof code,"l0m%04d",(int32_t)i);
            if(!gt4_career_record_meets_grade((uint8_t)license_record_bits(vm,record_unit(vm,self,code)),limit))break;
            if(i==last)passed=1;
        }
        *ret=ah_int(passed);return 0;
    }
    if(!category || !*category || strlen(category)>64)return ah_error(vm,"record query requires a code");
    if(!strcmp(op,"getUnit")){ah_obj *unit=record_unit(vm,self,category);*ret=unit?ah_objv(unit):ah_nil();return 0;}
    /* func_00431260/004312B0/004312D8/00431300 call the original
       record predicates; "passed" also accepts type-1 grade 12. */
    unsigned limit=!strcmp(op,"isLicenseAllSilver")?10u:11u;int passed=1;
    int tests=!strcmp(op,"canTryLastLicense")?15:16;
    for(int i=1;i<=tests;i++){
        char code[64];snprintf(code,sizeof code,"%s%04d",category,i);
        uint32_t value=license_record_bits(vm,record_unit(vm,self,code));
        int ok=!strcmp(op,"isLicensePerfect")?gt4_career_record_predicate_00430b58(value):
               !strcmp(op,"isLicensePassed")?gt4_career_record_predicate_00430c50(value):
               gt4_career_record_meets_grade((uint8_t)value,limit);
        if(!ok){passed=0;break;}
    }
    *ret=ah_bool(passed);return 0;
}
/* 001B1F10/001B2010 -> 0043B638/0043B690, week reset 0043B608.
   0043B598 initializes the eight bit words and week to zero. */
static int used_stock(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
    if(argc!=2)return ah_error(vm,"used stock requires week/id");
    uint32_t id=(uint32_t)ah_num(argv[1]);if(id>=256)return ah_error(vm,"used stock id outside bitset");
    int32_t week=(int32_t)ah_num(argv[0]);uint8_t bits[32]={0};
    gt4_career_blob_get(vm,self,"bits_ps2",bits,sizeof bits);
    if(low32(member(vm,self,"week"))!=week)memset(bits,0,sizeof bits);
    ah_set_member(vm,self,"week",ah_int(week));
    if(!strcmp(data,"set"))bits[id>>3]|=(uint8_t)(1u<<(id&7));
    *ret=!strcmp(data,"get")?ah_int((bits[id>>3]>>(id&7))&1):ah_nil();
    if(!gt4_career_blob_set(vm,self,"bits_ps2",bits,sizeof bits))return ah_error(vm,"used stock write failed");
    return 0;
}
static int elapsed(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
    (void)argc;(void)argv;int days=(int)ah_num(member(vm,self,"elapsed_days"));
    if(original_calendar_ready){
        ah_obj *date=ah_as_obj(member(vm,self,"today"),AO_ARRAY);
        if(ah_array_size(date)!=3)return ah_error(vm,"calendar date requires three fields");
        int32_t today=gt4_career_date_to_day(&original_calendar,low32(ah_array_get(date,0)),low32(ah_array_get(date,1)),low32(ah_array_get(date,2)));
        days=gt4_career_elapsed_days(&original_calendar,today);
    }
    if(!strcmp(data,"getElapsedWeek")){
        /* 001331A0 +0x6c..0x7c: optional signed remainder selects the
           repeating authored /usedcar stock (caller passes 100). */
        int32_t week=days/7;
        if(argc){int32_t modulus=low32(argv[0]);if(!modulus)return ah_error(vm,"week modulus is zero");if(!(week==INT32_MIN && modulus==-1))week%=modulus;else week=0;}
        *ret=ah_int(week);
    }else {
        /* func_00133288 +0x6c..0x7c: optional signed remainder.
           getElapsedDate(period), used by WheelShopRoot.getWheelList:
           the authored used wheel catalogue repeats after its list count. */
        if(argc){int32_t modulus=low32(argv[0]);if(!modulus)return ah_error(vm,"day modulus is zero");if(!(days==INT32_MIN && modulus==-1))days%=modulus;else days=0;}
        *ret=ah_int(days);
    }return 0;
}
static int leap(int y){return y%4==0 && (y%100!=0 || y%400==0);}
static int month_days(int y,int m){static const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};return days[m-1]+(m==2 && leap(y));}
static int day_number(int y,int m,int d){int previous=y-1,total=1721425+365*previous+previous/4-previous/100+previous/400+d;for(int i=1;i<m;i++)total+=month_days(y,i);return total;}
static int calendar_conversion(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
    (void)self;const char *op=data;
    if(!strcmp(op,"dayToWeekDay")){
        /* 00136270 +0x10/+0xa0: absent argument returns nil; extras ignored. */
        if(!argc){*ret=ah_nil();return 0;}
        if(argv[0].t!=AV_INT && argv[0].t!=AV_LONG)return ah_error(vm,"calendar conversion requires an integer");
        *ret=ah_int(gt4_career_day_to_weekday(low32(argv[0])));return 0;
    }
    for(int i=0;i<argc;i++)if(argv[i].t!=AV_INT && argv[i].t!=AV_LONG)return ah_error(vm,"calendar conversion requires integers");
    if(!strcmp(op,"dateToDay")){
        if(original_calendar_ready){
            if(argc!=3)return ah_error(vm,"date conversion requires three integers");
            *ret=ah_int(gt4_career_date_to_day(&original_calendar,low32(argv[0]),low32(argv[1]),low32(argv[2])));return 0;
        }
        if(argc!=3 || argv[0].u.i<1 || argv[0].u.i>9999 || argv[1].u.i<1 || argv[1].u.i>12 || argv[2].u.i<1 || argv[2].u.i>month_days((int)argv[0].u.i,(int)argv[1].u.i))return ah_error(vm,"invalid calendar date");
        *ret=ah_int(day_number((int)argv[0].u.i,(int)argv[1].u.i,(int)argv[2].u.i));return 0;
    }
    if(argc!=1)return ah_error(vm,"calendar conversion requires a day");
    if(original_calendar_ready){
        int32_t input=low32(argv[0]);
        if(!strcmp(op,"dayToGameDay")){*ret=ah_int(gt4_career_elapsed_days(&original_calendar,input));return 0;}
        if(!strcmp(op,"gameDayToDay")){
            uint32_t value=(uint32_t)input+(uint32_t)gt4_career_date_to_day(&original_calendar,2005,4,2);
            int32_t signed_value;memcpy(&signed_value,&value,4);*ret=ah_int(signed_value);return 0;
        }
        uint32_t packed=gt4_career_day_to_date(&original_calendar,input);
        ah_obj *date=ah_array_new(vm);ah_array_push(vm,date,ah_int(packed&65535));ah_array_push(vm,date,ah_int((packed>>16)&255));ah_array_push(vm,date,ah_int(packed>>24));*ret=ah_objv(date);return 0;
    }
    int64_t value=argv[0].u.i;
    if(!strcmp(op,"gameDayToDay")){
        if(value<0 || value>5373484-2453463)return ah_error(vm,"invalid game day");
        *ret=ah_int(value+2453463);return 0;
    }
    if(value<1721426 || value>5373484)return ah_error(vm,"invalid Julian day");
    if(!strcmp(op,"dayToGameDay")){*ret=ah_int(value-2453463);return 0;}
    int low=1,high=9999;while(low<high){int mid=(low+high+1)/2;if(day_number(mid,1,1)<=value)low=mid;else high=mid-1;}
    int m=1,d=(int)value-day_number(low,1,1)+1;while(d>month_days(low,m)){d-=month_days(low,m);m++;}
    ah_obj *date=ah_array_new(vm);ah_array_push(vm,date,ah_int(low));ah_array_push(vm,date,ah_int(m));ah_array_push(vm,date,ah_int(d));*ret=ah_objv(date);return 0;
}
void gt4_calendar_bind(ah_vm *vm,ah_obj *calendar){
    const char *methods[]={"dateToDay","dayToDate","dayToGameDay","gameDayToDay","dayToWeekDay"};
    for(unsigned i=0;i<sizeof methods/sizeof methods[0];i++)method(vm,calendar,methods[i],calendar_conversion);
}
/* 00133370 +0x50..0xb8 -> 0042F060. Native storage is the chronological
   serialization of the original ring, 0042F578, also used by PS2 import. */
int gt4_game_calendar_event(ah_vm *vm,ah_obj *calendar,int argc,const ah_val *argv,uint8_t event[16]){
 if(!original_calendar_ready)return -1;
 ah_obj *date=ah_as_obj(member(vm,calendar,"today"),AO_ARRAY),*events=ah_as_obj(member(vm,calendar,"original_events"),AO_ARRAY);
 if(ah_array_size(date)!=3)return -1;
 int32_t today=gt4_career_date_to_day(&original_calendar,low32(ah_array_get(date,0)),low32(ah_array_get(date,1)),low32(ah_array_get(date,2))),requested=today;
 if(argc>=3){for(int i=0;i<3;i++)if(argv[i].t!=AV_INT && argv[i].t!=AV_LONG)return -1;requested=gt4_career_date_to_day(&original_calendar,low32(argv[0]),low32(argv[1]),low32(argv[2]));}
 int index=gt4_career_history_index(today,requested,(unsigned)ah_array_size(events));if(index<0)return 0;
 ah_obj *record=ah_as_obj(ah_array_get(events,index),AO_ARRAY);if(ah_array_size(record)!=16)return -1;
 for(int i=0;i<16;i++)event[i]=(uint8_t)ah_num(ah_array_get(record,i));return 1;
}
/* Original car events, persisted as 16-byte records in chronological order.
   Their reserved stack bytes have no defined value; PC-created records use 0. */
static int calendar_append_native(ah_vm *vm,ah_obj *self,const uint8_t event[16],ah_val *ret){
    ah_obj *date=ah_as_obj(member(vm,self,"today"),AO_ARRAY);
    if(ah_array_size(date)!=3)return ah_error(vm,"calendar date requires three fields");
    int32_t day=gt4_career_date_to_day(&original_calendar,low32(ah_array_get(date,0)),low32(ah_array_get(date,1)),low32(ah_array_get(date,2)));
    ah_obj *events=array(vm,self,"original_events"),*updated=ah_array_new(vm);int first=ah_array_size(events)>=2922?ah_array_size(events)-2921:0;
    for(int i=first;i<ah_array_size(events);i++)ah_array_push(vm,updated,ah_array_get(events,i));
    ah_obj *record=ah_array_new(vm);for(int i=0;i<16;i++)ah_array_push(vm,record,ah_int(event[i]));ah_array_push(vm,updated,ah_objv(record));
    ah_set_member(vm,self,"original_events",ah_objv(updated));
    uint32_t next_bits=(uint32_t)day+1;int32_t next;memcpy(&next,&next_bits,4);
    uint32_t packed=gt4_career_day_to_date(&original_calendar,next);date=ah_array_new(vm);
    ah_array_push(vm,date,ah_int(packed&65535));ah_array_push(vm,date,ah_int((packed>>16)&255));ah_array_push(vm,date,ah_int(packed>>24));
    ah_set_member(vm,self,"today",ah_objv(date));ah_set_member(vm,self,"elapsed_days",ah_int(gt4_career_elapsed_days(&original_calendar,next)));
    changed(vm,self);ret->t=AV_VOID;return 0;
}
int gt4_game_license_leave(ah_vm *vm,ah_obj *game,const char *code,const gt4_career_license_attempts *visit){
    if(!vm || !game || !code || !visit || !original_calendar_ready || !original_race_lookup_ready)return 0;
    ah_obj *history=module(vm,game,"calendar");uint32_t grade,mode,time;uint8_t event[16]={0};ah_val ret;
    /* 00110800 -> 0042F198: even an unsuccessful visit advances one day. */
    if(gt4_career_license_history(visit,&grade,&mode,&time)){
        ah_val arg=ah_str(vm,code),key,resolver=member(vm,history,"_getRaceKey");
        if(resolver.t==AV_NIL || ah_call(vm,resolver,1,&arg,&key)<0 || !ah_is_num(key))return 0;
        gt4_career_calendar_race_event(event,1,(uint64_t)key.u.i,grade,mode,time);
    }
    return calendar_append_native(vm,history,event,&ret)>=0;
}
static int calendar_custom_event(ah_vm *vm,ah_obj *self,int wing,int argc,const ah_val *argv,ah_val *ret){
    if(argc<(wing?1:3) || !ah_cstr(argv[0]))return ah_error(vm,"customization calendar event requires original arguments");
    int category=0;
    if(!wing){
        const char *name=ah_cstr(argv[1]);category=-1;
        if(original_parts_ready && name)for(int i=0;i<4;i++)if(!strcmp(name,original_parts.wheel_categories[i]))category=i;
        if(category<0 || (argv[2].t!=AV_INT && argv[2].t!=AV_LONG))return ah_error(vm,"invalid wheel calendar category/detail");
    }
    ah_val resolver=member(vm,self,wing?"_getWingKey":"_getWheelKey"),key;
    if(resolver.t==AV_NIL || ah_call(vm,resolver,1,argv,&key)<0 || (key.t!=AV_INT && key.t!=AV_LONG))return ah_error(vm,"original customization key unavailable");
    /* Reserved original stack bytes are unavailable at the Adhoc boundary (A). */
    uint8_t event[16]={0};gt4_career_calendar_custom_event(event,wing,(uint64_t)key.u.i,category,wing?0:argv[2].u.i);
    return calendar_append_native(vm,self,event,ret);
}
static int calendar_car_event(ah_vm *vm,ah_obj *self,int sell,int subtype,int argc,const ah_val *argv,ah_val *ret){
    if(argc<(sell?2:1) || !ah_cstr(argv[0]) || (argc>1 && argv[1].t!=AV_INT && argv[1].t!=AV_LONG) ||
       (argc>2 && argv[2].t!=AV_INT && argv[2].t!=AV_LONG))return ah_error(vm,"car calendar event requires label, color and optional price");
    ah_val resolver=member(vm,self,"_getCarKey"),key;
    if(resolver.t==AV_NIL || ah_call(vm,resolver,1,argv,&key)<0 || (key.t!=AV_INT && key.t!=AV_LONG))return ah_error(vm,"original car key unavailable");
    uint8_t event[16]={0};gt4_career_calendar_car_event(event,sell,(uint64_t)key.u.i,argc>1?argv[1].u.i:0,sell && argc>2?argv[2].u.i:subtype);
    return calendar_append_native(vm,self,event,ret);
}

int gt4_game_apply_event_result(ah_vm *vm,ah_obj *game,const char *race,
    int finished,int32_t position,uint32_t best_lap,int32_t driver_mode,
    int32_t aspec,uint32_t prize,uint8_t *previous_best,uint32_t *awarded_prize)
{
    if(!vm || !game || !race || !*race || !previous_best || !awarded_prize ||
       !original_calendar_ready || !original_race_lookup_ready)return 0;
    ah_obj *history=module(vm,game,"calendar");
    ah_obj *date=ah_as_obj(member(vm,history,"today"),AO_ARRAY);
    if(ah_array_size(date)!=3)return 0;
    ah_val label=ah_str(vm,race),key=ah_nil(),resolver=member(vm,history,"_getRaceKey");
    if(resolver.t==AV_NIL || ah_call(vm,resolver,1,&label,&key)<0 || (key.t!=AV_INT && key.t!=AV_LONG))return 0;
    ah_obj *unit=record_unit(vm,module(vm,game,"race_record"),race);
    if(!unit)return 0;
    ah_obj *stats=module(vm,game,"statistics");
    ah_obj *counters=ah_as_obj(member(vm,stats,"race_positions"),AO_ARRAY);
    gt4_career_event_result state={0};state.record=unit_word(vm,unit);
    state.previous_best=*previous_best;state.awarded_prize=*awarded_prize;
    /* 0043B168 +0x14..0xc4 copies live data after its eight-byte header:
       statistics+0x48 and +0x54 are serialized offsets 64 and 76. */
    ah_val stored=member(vm,stats,"prize_money");uint64_t total=stored.t==AV_NIL?0:(uint64_t)stored.u.i;
    memcpy(state.statistics+0x48,&total,8);
    for(unsigned i=0;i<7;i++){uint32_t value=(uint32_t)ah_num(ah_array_get(counters,(int)i));memcpy(state.statistics+0x54+4*i,&value,4);}
    gt4_career_process_event_result(&state,finished,position,best_lap,driver_mode,aspec,prize);
    uint8_t event[16]={0};ah_val result;
    gt4_career_calendar_race_event(event,0,(uint64_t)key.u.i,state.calendar_grade,state.calendar_method,state.calendar_time);
    if(calendar_append_native(vm,history,event,&result)<0)return 0;
    ah_set_member(vm,unit,"original_record",ah_int(state.record));
    memcpy(&total,state.statistics+0x48,8);int64_t signed_total;memcpy(&signed_total,&total,8);
    ah_set_member(vm,stats,"prize_money",ah_int(signed_total));
    ah_obj *updated=ah_array_new(vm);
    for(unsigned i=0;i<7;i++){uint32_t value;memcpy(&value,state.statistics+0x54+4*i,4);ah_array_push(vm,updated,ah_int(value));}
    ah_set_member(vm,stats,"race_positions",ah_objv(updated));
    *previous_best=state.previous_best;*awarded_prize=state.awarded_prize;changed(vm,game);
    return 1;
}
/* 0011C328 / 0011C2E0: a driving mission's result, as an event's above
   without the place counters (gt4_career_process_mission_result) */
int gt4_game_apply_mission_result(ah_vm *vm,ah_obj *game,const char *race,
    int finished,int32_t position,uint32_t race_time,int32_t aspec,uint32_t prize,uint32_t *awarded_prize)
{
    if(!vm || !game || !race || !*race || !awarded_prize ||
       !original_calendar_ready || !original_race_lookup_ready)return 0;
    ah_obj *history=module(vm,game,"calendar");
    ah_obj *date=ah_as_obj(member(vm,history,"today"),AO_ARRAY);
    if(ah_array_size(date)!=3)return 0;
    ah_val label=ah_str(vm,race),key=ah_nil(),resolver=member(vm,history,"_getRaceKey");
    if(resolver.t==AV_NIL || ah_call(vm,resolver,1,&label,&key)<0 || (key.t!=AV_INT && key.t!=AV_LONG))return 0;
    ah_obj *unit=record_unit(vm,module(vm,game,"race_record"),race);
    if(!unit)return 0;
    ah_obj *stats=module(vm,game,"statistics");
    gt4_career_event_result state={0};state.record=unit_word(vm,unit);state.awarded_prize=*awarded_prize;
    ah_val stored=member(vm,stats,"prize_money");uint64_t total=stored.t==AV_NIL?0:(uint64_t)stored.u.i;
    memcpy(state.statistics+0x48,&total,8);
    gt4_career_process_mission_result(&state,finished,position,race_time,aspec,prize);
    uint8_t event[16]={0};ah_val result;
    gt4_career_calendar_race_event(event,0,(uint64_t)key.u.i,state.calendar_grade,state.calendar_method,state.calendar_time);
    if(calendar_append_native(vm,history,event,&result)<0)return 0;
    ah_set_member(vm,unit,"original_record",ah_int(state.record));
    memcpy(&total,state.statistics+0x48,8);int64_t signed_total;memcpy(&signed_total,&total,8);
    ah_set_member(vm,stats,"prize_money",ah_int(signed_total));
    *awarded_prize=state.awarded_prize;changed(vm,game);
    return 1;
}
int gt4_game_apply_championship_result(ah_vm *vm,ah_obj *game,const char *race,const char *series,
    int finished,int final_session,int32_t position,uint32_t best_lap,int32_t driver_mode,
    int32_t aspec,uint32_t prize,const int32_t points[6],int16_t units,
    uint32_t *awarded_prize,uint32_t *grand_prize)
{
    if(!vm || !game || !race || !*race || !series || !points || !grand_prize || !awarded_prize ||
       !original_calendar_ready || !original_race_lookup_ready)return 0;
    ah_obj *history=module(vm,game,"calendar");
    ah_obj *date=ah_as_obj(member(vm,history,"today"),AO_ARRAY);
    if(ah_array_size(date)!=3)return 0;
    ah_val label=ah_str(vm,race),key=ah_nil(),resolver=member(vm,history,"_getRaceKey");
    if(resolver.t==AV_NIL || ah_call(vm,resolver,1,&label,&key)<0 || (key.t!=AV_INT && key.t!=AV_LONG))return 0;
    ah_obj *unit=record_unit(vm,module(vm,game,"race_record"),race);
    if(!unit)return 0;
    ah_obj *stats=module(vm,game,"statistics");
    ah_obj *counters=ah_as_obj(member(vm,stats,"race_positions"),AO_ARRAY);
    ah_obj *series_unit=record_unit(vm,module(vm,game,"race_record"),series);if(!series_unit)return 0;
    gt4_career_championship_result championship={0};championship.series_record=unit_word(vm,series_unit);
    gt4_career_event_result state={0};state.record=unit_word(vm,unit);state.awarded_prize=*awarded_prize;
    /* 0043B168 +0x14..0xc4 copies live data after its eight-byte header:
       statistics+0x48 and +0x54 are serialized offsets 64 and 76. */
    ah_val stored=member(vm,stats,"prize_money");uint64_t total=stored.t==AV_NIL?0:(uint64_t)stored.u.i;
    memcpy(state.statistics+0x48,&total,8);
    for(unsigned i=0;i<7;i++){uint32_t value=(uint32_t)ah_num(ah_array_get(counters,(int)i));memcpy(state.statistics+0x54+4*i,&value,4);}
    championship.result=state;
    gt4_career_process_championship_result(&championship,finished,final_session,position,best_lap,driver_mode,aspec,prize,points,units);state=championship.result;
    ah_set_member(vm,series_unit,"original_record",ah_int(championship.series_record));
    *grand_prize=championship.grand_prize;
    uint8_t event[16]={0};ah_val result;
    gt4_career_calendar_race_event(event,0,(uint64_t)key.u.i,state.calendar_grade,state.calendar_method,state.calendar_time);
    if(calendar_append_native(vm,history,event,&result)<0)return 0;
    ah_set_member(vm,unit,"original_record",ah_int(state.record));
    memcpy(&total,state.statistics+0x48,8);int64_t signed_total;memcpy(&signed_total,&total,8);
    ah_set_member(vm,stats,"prize_money",ah_int(signed_total));
    ah_obj *updated=ah_array_new(vm);
    for(unsigned i=0;i<7;i++){uint32_t value;memcpy(&value,state.statistics+0x54+4*i,4);ah_array_push(vm,updated,ah_int(value));}
    ah_set_member(vm,stats,"race_positions",ah_objv(updated));
    *awarded_prize=state.awarded_prize;changed(vm,game);
    return 1;
}
/* func_00135348 / 00135590 -> 0042F270 / 0042F2E0. */
static int calendar_race_event(ah_vm *vm,ah_obj *self,int license,int argc,const ah_val *argv,ah_val *ret){
    if(argc<4)return 0; /* Original callback performs no writes for missing args. */
    const char *label=ah_cstr(argv[0]),*grade=ah_cstr(argv[1]);
    if(!label || !grade || (argv[2].t!=AV_NIL && !ah_cstr(argv[2])) || (argv[3].t!=AV_INT && argv[3].t!=AV_LONG) || !original_race_lookup_ready)
        return ah_error(vm,"race calendar requires label, result, optional method and integer prize");
    ah_val key,resolver=member(vm,self,"_getRaceKey");
    if(resolver.t==AV_NIL || ah_call(vm,resolver,1,argv,&key)<0 || (key.t!=AV_INT && key.t!=AV_LONG))return ah_error(vm,"original race key unavailable");
    uint8_t event[16]={0};
    gt4_career_calendar_race_event(event,license,(uint64_t)key.u.i,gt4_career_result_grade(&original_race_lookup,grade),gt4_career_race_method(&original_race_lookup,ah_cstr(argv[2])),(uint64_t)argv[3].u.i);
    return calendar_append_native(vm,self,event,ret);
}
static int calendar(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){const char *op=data;
    if(original_calendar_ready && !strcmp(op,"gameDayToDay"))return calendar_conversion(vm,self,data,argc,argv,ret);
    /* 00134C60 / 00134D40 / 00134E20 / 00134F00 all delegate to
       0042F1B8 with subtype 1 / 2 / 3 / 4 and default color zero. */
    if(original_calendar_ready){
        if(!strcmp(op,"putBuyWheelEvent") || !strcmp(op,"putBuyWingEvent"))return calendar_custom_event(vm,self,!strcmp(op,"putBuyWingEvent"),argc,argv,ret);
        if(!strcmp(op,"putRunRaceEvent") || !strcmp(op,"putRunLicenseEvent"))return calendar_race_event(vm,self,!strcmp(op,"putRunLicenseEvent"),argc,argv,ret);
        int kind=!strcmp(op,"putBuyNewCarEvent")?1:!strcmp(op,"putBuyUsedCarEvent")?2:!strcmp(op,"putBuyTradeCarEvent")?3:!strcmp(op,"putPresentCarEvent")?4:0;
        if(kind || !strcmp(op,"putSellCarEvent"))return calendar_car_event(vm,self,!strcmp(op,"putSellCarEvent"),kind,argc,argv,ret);
    }
    ah_obj *date=ah_as_obj(member(vm,self,"today"),AO_ARRAY);int y=(int)ah_num(ah_array_get(date,0)),m=(int)ah_num(ah_array_get(date,1)),d=(int)ah_num(ah_array_get(date,2));
    if(y<1 || y>9999 || m<1 || m>12 || d<1 || d>month_days(y,m))return ah_error(vm,"invalid game calendar");
    int elapsed_days=(int)ah_num(member(vm,self,"elapsed_days"));
    if(!strcmp(op,"gameDayToDay")){if(argc!=1 || !ah_is_num(argv[0]) || ah_num(argv[0])<0 || ah_num(argv[0])>INT_MAX/2)return ah_error(vm,"invalid game day");*ret=ah_int(day_number(y,m,d)-elapsed_days+(int)ah_num(argv[0]));return 0;}
    if(argc<1 || argc>8 || !ah_cstr(argv[0]))return ah_error(vm,"invalid calendar event");
    const char *type=!strcmp(op,"putRunRaceEvent")?"RUN_RACE":!strcmp(op,"putRunLicenseEvent")?"RUN_LICENSE":!strcmp(op,"putSellCarEvent")?"SELL_CAR":strstr(op,"Wheel")?"BUY_WHEEL":strstr(op,"Wing")?"BUY_WING":"GET_CAR";
    if(y==9999 && m==12 && d==31)return ah_error(vm,"calendar range exceeded");
    ah_obj *entry=ah_array_new(vm);ah_array_push(vm,entry,ah_str(vm,type));for(int i=0;i<argc;i++)ah_array_push(vm,entry,argv[i]);
    ah_obj *events=array(vm,self,"events"),*updated=ah_array_new(vm);int first=ah_array_size(events)>=2922?ah_array_size(events)-2921:0;for(int i=first;i<ah_array_size(events);i++)ah_array_push(vm,updated,ah_array_get(events,i));ah_array_push(vm,updated,ah_objv(entry));ah_set_member(vm,self,"events",ah_objv(updated));
    if(++d>month_days(y,m)){d=1;if(++m>12){m=1;y++;}}
    date=ah_array_new(vm);ah_array_push(vm,date,ah_int(y));ah_array_push(vm,date,ah_int(m));ah_array_push(vm,date,ah_int(d));ah_set_member(vm,self,"today",ah_objv(date));ah_set_member(vm,self,"elapsed_days",ah_int(elapsed_days+1));changed(vm,self);*ret=ah_int(elapsed_days+1);return 0;
}
/* func_0019FB90 / 0019FC58 / 0019FCD0 / 0019FD98.
   Original indices address a shared bitset, not independent label maps. */
static int present(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
    const char *op=data;if(argc<1)return 0;
    if(argv[0].t!=AV_INT && argv[0].t!=AV_LONG)return ah_error(vm,"present flag requires an integer index");
    uint32_t bit=(uint32_t)argv[0].u.i;if(strstr(op,"Time"))bit+=0x364u;
    if(bit>=928)return ah_error(vm,"present flag outside serialized original bitset");
    uint8_t flags[116]={0};ah_val stored=member(vm,self,"bits_ps2");
    if(stored.t!=AV_NIL && !gt4_career_blob_get(vm,self,"bits_ps2",flags,sizeof flags))return ah_error(vm,"invalid original present flags");
    /* Preserve flags from earlier native PC saves when migrating their maps. */
    if(stored.t==AV_NIL)for(unsigned time=0;time<2;time++){
        ah_obj *legacy=ah_as_obj(member(vm,self,time?"by_time":"by_race"),AO_MODULE);
        for(int i=0;i<ah_object_count(legacy);i++){
            const char *name=ah_object_member_name(legacy,i);if(!name || !*name || !ah_truthy(ah_object_value(legacy,i)))continue;
            char *end;long index=strtol(name,&end,10);if(*end || index<0 || index>=(time?60:868))continue;
            gt4_career_present_set(flags,(uint32_t)index+(time?868u:0u));
        }
    }
    if(!strncmp(op,"set",3)){
        gt4_career_present_set(flags,bit);
        if(!gt4_career_blob_set(vm,self,"bits_ps2",flags,sizeof flags))return ah_error(vm,"could not store present flags");
        changed(vm,self);ret->t=AV_VOID;
    }else *ret=ah_bool(gt4_career_present_get(flags,bit));
    return 0;
}
/* HomeRoot L3996 / StatusRoot L14863..14905, registered at
   00169D9C..0016A338. All counters belong to the persisted profile. */
static int home_statistics(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
 const char *op=data;ah_obj *stats=module(vm,self,"statistics");
 if(strstr(op,"Experience")){
  if(!original_home_ready)return ah_error(vm,"original B-Spec weights unavailable");
  uint8_t experience[0x180]={0};int32_t values[4];
  if(member(vm,stats,"b_spec_experience_ps2").t!=AV_NIL && !gt4_career_blob_get(vm,stats,"b_spec_experience_ps2",experience,sizeof experience))return ah_error(vm,"invalid B-Spec experience");
  gt4_career_bspec_status(&original_home,experience,values);
  *ret=ah_int(values[strstr(op,"Car")?1:strstr(op,"Course")?2:strstr(op,"Battle")?3:0]);return 0;
 }
 if(strstr(op,"Distance")){
  ah_val stored=member(vm,stats,!strcmp(op,"getAspecDistance")?"a_spec_distance":"b_spec_distance");
  if(stored.t!=AV_NIL && stored.t!=AV_INT && stored.t!=AV_LONG)return ah_error(vm,"invalid profile distance");
  ah_val unit=member(vm,module(vm,self,"option"),"unit_velocity");uint64_t metres=stored.t==AV_NIL?0:(uint64_t)stored.u.i;
  int64_t distance=gt4_career_home_distance(metres,ah_cstr(unit) && !strcmp(ah_cstr(unit),"MPH"));
  char number[48],text[64];snprintf(number,sizeof number,"%lld",(long long)distance);size_t n=strlen(number),at=0;
  for(size_t i=0;i<n;i++){text[at++]=number[i];if(number[i]!='-' && i+1<n && (n-i-1)%3==0)text[at++]=',';}text[at]=0;
  /* 00472898: distance unit suffix, 005A5DC8 appends it. */
  strcat(text,ah_cstr(unit) && !strcmp(ah_cstr(unit),"MPH")?" miles":" km");*ret=ah_str(vm,text);return 0;
 }
 ah_obj *positions=ah_as_obj(member(vm,stats,"race_positions"),AO_ARRAY);uint32_t counters[7]={0};
 for(unsigned i=0;i<7;i++)counters[i]=(uint32_t)ah_num(ah_array_get(positions,(int)i));
 if(!strcmp(op,"getTotalRaceCount")){*ret=ah_int(gt4_career_home_race_count(counters));return 0;}
 if(!argc){*ret=ah_nil();return 0;}
 /* 00163EF0 +0x50..0x60 indexes +0x39c: rank 1 is positions[0]. */
 int32_t rank=low32(argv[0]);if(rank<1 || rank>7)return ah_error(vm,"rank outside original counters");
 int32_t value;memcpy(&value,&counters[rank-1],4);*ret=ah_int(value);return 0;
}
/* Original statistics are independent of spendable money. */
static int prize_statistics(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
    const char *op=data;int cars=strstr(op,"Cars")!=NULL,increment=!strncmp(op,"inc",3);
    ah_obj *stats=module(vm,self,"statistics");const char *field=cars?"prize_cars":"prize_money";
    ah_val stored=member(vm,stats,field);uint64_t bits=stored.t==AV_NIL?0:(uint64_t)stored.u.i;
    if(increment){
        if(argc<1)return 0;
        if(argv[0].t!=AV_INT && argv[0].t!=AV_LONG)return ah_error(vm,"prize increment requires an integer");
        /* func_00163BB8: daddu/sd; func_00163E80: addu/sw. */
        bits=cars?gt4_career_prize_cars_add((uint32_t)bits,(uint64_t)argv[0].u.i):gt4_career_credit_add(bits,(uint64_t)argv[0].u.i);
        int64_t value;memcpy(&value,&bits,8);ah_set_member(vm,stats,field,ah_int(value));changed(vm,self);ret->t=AV_VOID;
    }else if(cars){ /* func_00163DE8: lw, sign-extended. */
        uint32_t word=(uint32_t)bits;int32_t value;memcpy(&value,&word,4);*ret=ah_int(value);
    }else{ /* func_00163A68: decimal presentation of the original u64. */
        char text[32];snprintf(text,sizeof text,"%llu",(unsigned long long)bits);*ret=ah_str(vm,text);
    }
    return 0;
}
static int availability(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
    const char *code=argc?ah_cstr(argv[0]):NULL;
    if(argc!=1 || !code || !*code || strlen(code)>64)return ah_error(vm,"car availability requires a code");
    ah_obj *available=module(vm,self,"available");
    if(!strcmp(data,"setCarAvailable")){ah_set_member(vm,available,code,ah_bool(1));changed(vm,self);*ret=ah_bool(1);}
    else *ret=ah_bool(ah_truthy(member(vm,available,code)));
    return 0;
}
int gt4_game_championship_context_get(ah_vm *vm,ah_obj *game,uint8_t live[0x90]){
    if(!vm || !game || !live)return 0;
    memset(live,0,0x90);ah_obj *champ=module(vm,game,"championship");
    ah_val saved=member(vm,champ,"context_ps2");
    return saved.t==AV_NIL || gt4_career_blob_get(vm,champ,"context_ps2",live+8,0x88);
}
int gt4_game_championship_context_set(ah_vm *vm,ah_obj *game,const uint8_t live[0x90]){
    if(!vm || !game || !live || !gt4_career_blob_set(vm,module(vm,game,"championship"),"context_ps2",live+8,0x88))return 0;
    /* 00165390 getter: label at live +8; session getter uses +0x29. */
    char series[33];memcpy(series,live+8,32);series[32]=0;
    ah_set_member(vm,game,"suspended_racecode",ah_str(vm,series));
    ah_set_member(vm,game,"suspended_session",ah_int(live[0x29]));changed(vm,game);return 1;
}
static int championship_status(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
    (void)argc;(void)argv;uint8_t live[0x90];
    if(!gt4_game_championship_context_get(vm,self,live))return ah_error(vm,"invalid championship context");
    if(!strcmp(data,"abandonRace")){
        /* 00119664 -> 00119BD8(a1=0): only active flag is cleared. */
        live[0x28]=0;if(!gt4_game_championship_context_set(vm,self,live))return ah_error(vm,"cannot clear championship context");*ret=ah_nil();
    }else *ret=ah_bool(live[0x28]!=0); /* 001652F0 +0x28 */
    return 0;
}
static int menu_stack_read(ah_vm *vm,ah_obj *game,gt4_career_menu_stack *s){
    memset(s,0,sizeof *s);gt4_career_menu_clear(s);
    if(member(vm,game,"_menu_stack_ps2").t!=AV_NIL && !gt4_career_blob_get(vm,game,"_menu_stack_ps2",(uint8_t *)s,sizeof *s))return 0;
    ah_obj *champ=module(vm,game,"championship");
    if(member(vm,champ,"menu_stack_ps2").t!=AV_NIL && !gt4_career_blob_get(vm,champ,"menu_stack_ps2",(uint8_t *)&s->saved_length,260))return 0;
    return s->length>=0 && s->length<=255;
}
int gt4_game_menu_stack_action(ah_vm *vm,ah_obj *game,const char *operation,int argc,const ah_val *argv,ah_val *ret){
    gt4_career_menu_stack s;if(!menu_stack_read(vm,game,&s))return ah_error(vm,"invalid original menu stack");
    *ret=ah_nil();const char *text=NULL;int save=0;
    if(!strcmp(operation,"menuPush")){
        if(argc<1)return 0;char buffer[256];text=ah_to_text(argv[0],buffer,sizeof buffer);
        if(ah_cstr(argv[0]))text=ah_cstr(argv[0]);gt4_career_menu_push(&s,text);
    }else if(!strcmp(operation,"menuRef")){
        text=gt4_career_menu_ref(&s,argc?(int32_t)ah_num(argv[0]):0);
    }else if(!strcmp(operation,"menuPop"))text=gt4_career_menu_pop(&s);
    else if(!strcmp(operation,"menuClearStack")){gt4_career_menu_clear(&s);save=1;}
    else if(!strcmp(operation,"saveMenuStack")){gt4_career_menu_save(&s);save=1;}
    else if(!strcmp(operation,"loadMenuStack"))*ret=ah_bool(gt4_career_menu_load(&s));
    else return ah_error(vm,"unknown menu stack operation");
    if(text)*ret=ah_str(vm,text);
    if(!gt4_career_blob_set(vm,game,"_menu_stack_ps2",(uint8_t *)&s,sizeof s))return ah_error(vm,"cannot store original menu stack");
    if(save){if(!gt4_career_blob_set(vm,module(vm,game,"championship"),"menu_stack_ps2",(uint8_t *)&s.saved_length,260))return ah_error(vm,"cannot save original menu stack");changed(vm,game);}
    return 0;
}
static int championship_menu_stack(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
    /* Native registration 001694B4/0016955C -> 001662B8/001662F8,
       operating on game+0x3A368 via 0042FE88/0042FF48. */
    return gt4_game_menu_stack_action(vm,self,data,argc,argv,ret);
}
int gt4_game_money_text(double amount,char *out,size_t size){
    if(!isfinite(amount) || amount>=9223372036854775808.0)return 0;
    if(amount<=0)return snprintf(out,size,"--")<(int)size;
    char digits[32],text[48];snprintf(digits,sizeof digits,"%lld",(long long)amount);
    size_t n=strlen(digits),at=0;
    for(size_t i=0;i<n;i++){text[at++]=digits[i];if(i+1<n && (n-i-1)%3==0)text[at++]=',';}
    text[at]=0;return snprintf(out,size,"%s",text)<(int)size;
}
/* main::game.money (00169700 registers 00163708): the balance read through
   0043AF68 and formatted by 0042E6B8, a string ("%s".format at gtmode.ad
   L302, "Cr. " + money at L6450/L7787). The engine keeps the number. */
static int game_read(ah_vm *vm,ah_obj *self,const char *sym,ah_val *out){
    if(strcmp(sym,"money"))return 0;
    ah_val current=member(vm,self,"money");char text[48];
    if(!ah_is_num(current) || !gt4_game_money_text(ah_num(current),text,sizeof text))return 0;
    *out=ah_str(vm,text);return 1;
}
static const ah_host game_host={"game",NULL,NULL,NULL,NULL,1,game_read};
void gt4_game_bind(ah_vm *vm,ah_obj *game){
    ah_module_attach_host(game,&game_host);
    if(member(vm,game,"withdrawn").t==AV_NIL)ah_set_member(vm,game,"withdrawn",ah_int(0));
    if(member(vm,game,"withdrawnGT4p").t==AV_NIL)ah_set_member(vm,game,"withdrawnGT4p",ah_int(0));
    method(vm,game,"saveMenuStack",championship_menu_stack);method(vm,game,"loadMenuStack",championship_menu_stack);
    method(vm,game,"isSuspended",championship_status);method(vm,game,"abandonRace",championship_status);
    uint8_t championship_live[0x90];if(gt4_game_championship_context_get(vm,game,championship_live)){
        char series[33];memcpy(series,championship_live+8,32);series[32]=0;
        ah_set_member(vm,game,"suspended_racecode",ah_str(vm,series));
        ah_set_member(vm,game,"suspended_session",ah_int(championship_live[0x29]));
    }

    method(vm,game,"incMoney",money);
    const char *stats_methods[]={"incTotalPrizeMoney","getTotalPrizeMoney","incTotalPrizeCars","getTotalPrizeCars"};
    for(unsigned i=0;i<4;i++)method(vm,game,stats_methods[i],prize_statistics);
    const char *home_methods[]={"getTotalRankCount","getTotalRaceCount","getAspecDistance","getBspecDistance","getBspecTotalExperience","getBspecCarExperience","getBspecCourseExperience","getBspecBattleExperience"};
    for(unsigned i=0;i<sizeof home_methods/sizeof *home_methods;i++)method(vm,game,home_methods[i],home_statistics);
    method(vm,game,"isEnoughMoney",enough_money);
    method(vm,game,"setCarAvailable",availability);method(vm,game,"isCarAvailable",availability);
    ah_obj *g=module(vm,game,"garage");array(vm,g,"cars");if(member(vm,g,"riding_index").t==AV_NIL)ah_set_member(vm,g,"riding_index",ah_int(-1));
    ah_set_member(vm,g,"_purchaseDay",ah_native(vm,"_purchaseDay",purchase_day,game));
    const char *gs[]={"addNewCar","addUsedCar","hasRidingCar","isFull","getCount","setRidingCar","changeRidingCar","cancelRidingCar","delCar","delAllCarExceptRidingCar","popGarageCar","pushRidingCar"};for(unsigned i=0;i<sizeof gs/sizeof *gs;i++)method(vm,g,gs[i],garage);
    ah_obj *cars=array(vm,g,"cars");
    /* Earlier PS2 imports stored occupied slots densely. Restore their actual
       garage addresses before resolving the selected car. */
    ah_val slots[1000];for(int i=0;i<1000;i++)slots[i]=ah_nil();int last=-1,remap=0;
    for(int i=0;i<ah_array_size(cars) && i<1000;i++){
        ah_val value=ah_array_get(cars,i);ah_obj *car=ah_as_obj(value,AO_MODULE);if(!car)continue;
        /* The live slot property is a tuning setup (00140C70), not the
           garage address. Prefer the owned garage index when already bound. */
        ah_val saved=member(vm,car,"garage_index");if(saved.t==AV_NIL)saved=member(vm,car,"slot");int index=i;
        if((saved.t==AV_INT || saved.t==AV_LONG) && saved.u.i>=0 && saved.u.i<1000)index=(int)saved.u.i;
        if(slots[index].t!=AV_NIL)continue;
        slots[index]=value;if(index>last)last=index;remap|=index!=i;
        ah_set_member(vm,car,"garage_index",ah_int(index));method(vm,car,"getGarageIndex",car_index);
        gt4_game_bind_car(vm,car);
    }
    if(remap){cars=ah_array_new(vm);for(int i=0;i<=last;i++)ah_array_push(vm,cars,slots[i]);ah_set_member(vm,g,"cars",ah_objv(cars));}
    ah_set_member(vm,g,"count",ah_int(count_cars(array(vm,g,"cars"))));ah_set_member(vm,g,"riding_car",ah_array_get(array(vm,g,"cars"),(int)ah_num(member(vm,g,"riding_index"))));
    ah_obj *r=module(vm,game,"race_record");const char *rs[]={"getUnit","isLicensePassed","isLicenseAllBronze","isLicenseAllSilver","isLicensePerfect","canTryLastLicense","isMissionClear"};for(unsigned i=0;i<sizeof rs/sizeof *rs;i++)method(vm,r,rs[i],record);
    ah_obj *record_array=ah_as_obj(member(vm,r,"records"),AO_ARRAY);
    if(gt4_game_race_lookup_data() && !ah_array_size(record_array) && !ah_object_count(ah_as_obj(member(vm,r,"units"),AO_MODULE))){
        record_array=ah_array_new(vm);
        for(unsigned i=0;i<768;i++){
            ah_obj *unit=ah_module_new(vm,"record",NULL,NULL,NULL);
            ah_set_member(vm,unit,"original_record",ah_int(gt4_career_record_init(0)));ah_array_push(vm,record_array,ah_objv(unit));
        }
        ah_set_member(vm,r,"records",ah_objv(record_array));
    }
    for(int i=0;i<ah_array_size(record_array);i++)gt4_game_bind_record_unit(vm,ah_as_obj(ah_array_get(record_array,i),AO_MODULE));
    ah_obj *units=ah_as_obj(member(vm,r,"units"),AO_MODULE);
    for(int i=0;i<ah_object_count(units);i++)gt4_game_bind_record_unit(vm,ah_as_obj(ah_object_value(units,i),AO_MODULE));
    method(vm,module(vm,game,"course_record"),"getUnit",record);
    method(vm,module(vm,game,"license_record"),"getUnit",license_get);
    ah_obj *used=module(vm,game,"used_car");
    if(member(vm,used,"week").t==AV_NIL)ah_set_member(vm,used,"week",ah_int(0));
    method(vm,used,"get",used_stock);method(vm,used,"set",used_stock);
    ah_obj *c=module(vm,game,"calendar");method(vm,c,"getElapsedDate",elapsed);method(vm,c,"getElapsedWeek",elapsed);
    const char *cs[]={"gameDayToDay","putBuyNewCarEvent","putBuyUsedCarEvent","putBuyTradeCarEvent","putPresentCarEvent","putSellCarEvent","putBuyWheelEvent","putBuyWingEvent","putRunRaceEvent","putRunLicenseEvent"};for(unsigned i=0;i<sizeof cs/sizeof *cs;i++)method(vm,c,cs[i],calendar);
    ah_obj *p=module(vm,game,"presents");const char *ps[]={"getByRace","getByTime","setByRace","setByTime"};for(unsigned i=0;i<4;i++)method(vm,p,ps[i],present);ah_set_member(vm,game,"present",ah_objv(p));
}
