/* Native integration, C: installed keys and all setting fields arrive at
   entry+20 through 00441248/004452F8. Original scalar setters have their
   separate EE oracle. Test values are fixtures, never gameplay defaults. */
#define main setting_menu_fixture_main
#include "setting_menu.c"
#undef main
#include "career_parts.h"
#include "career_setting.h"
#include "specdb.h"
#include "physics/race_run.h"
int main(int argc,char **argv){
    CHECK(argc==2 && gt4_game_load_disc(argv[1]));const char *why=NULL;
    gt4_vol *vol=gt4_vol_open(argv[1],&why);CHECK(vol);
    gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[1]);CHECK(factory);
    gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);
    gt4_game_set_auto_gears(gt4_career_vehicle_auto_gears,factory);
    gt4_game_set_stats_reader(gt4_career_vehicle_stats,factory);
    char error[256];menu *m=menu_open(vol,"gtmode",error,sizeof error);CHECK(m);
    ah_vm *vm=m->vm;
    size_t label_size;uint8_t *labels=gt4_vol_read(vol,"/specdb/GT4_US2560/GENERIC_CAR.idi",&label_size);CHECK(labels && label_size>=20);
    uint32_t car_count;memcpy(&car_count,labels+4,4);
    const char *limit=getenv("GT4_SETTING_CAR_LIMIT");if(limit && car_count>(uint32_t)atoi(limit))car_count=(uint32_t)atoi(limit);
    unsigned installs=0,fields=0,runs=0;
    gt4_game_set_setting_reader(gt4_career_vehicle_setting,factory);
    for(uint32_t vehicle=0;vehicle<car_count;vehicle++){
    uint32_t catalog_id;memcpy(&catalog_id,labels+20+vehicle*8,4);
    const char *label=gt4_specdb_label_for_id(labels,label_size,catalog_id);CHECK(label);
    fprintf(stderr,"setting car %u/%u %s\n",vehicle+1,car_count,label);fflush(stderr);
    ah_obj *car=ah_module_new(vm,"Car",NULL,NULL,NULL);
    CHECK(gt4_game_construct_car(vm,car,label,0,0));
    ah_val arg=ah_objv(car),ret;CHECK(nat_bind_game_car(vm,NULL,m,1,&arg,&ret)>=0);
    uint8_t body[0x4b8];CHECK(gt4_game_car_body(vm,car,body));
    uint32_t id;memcpy(&id,body,4);char path[80];snprintf(path,sizeof path,"/specdb/GT4_US2560/CARS/%016llX",(unsigned long long)id);
    size_t size;uint8_t *parts=gt4_vol_read(vol,path,&size);CHECK(parts && size>=8);
    const gt4_career_part_data *data=gt4_game_part_data();CHECK(data);
    const int counts[]={2,18,6,11,1,1,2,2,1,2};
    for(int slot=0;slot<3;slot++){
        ah_set_member(vm,car,"slot",ah_int(slot));
        CHECK(ah_call_member(vm,car,"beginSetting",0,NULL,&ret)>=0);
        for(unsigned type=1;type<=31;type++)for(unsigned stage=0;stage<=data->type_max[type-1];stage++){
            uint64_t key=gt4_career_part_key(parts,size,type,stage);if(key==UINT64_MAX)continue;
            ah_val args[]={ah_int(type),ah_int(stage)};
            /* Stock stage zero is already available; the shop acquires
               upgrades, not the sentinel ownership bit of a normal piece. */
            if(stage)CHECK(ah_call_member(vm,car,"getParts",2,args,&ret)>=0);
            if(ah_call_member(vm,car,"changeParts",2,args,&ret)<0){
                ah_val row,keyarg=ah_int((int64_t)key);ah_call(vm,get(vm,car,"_getSettingPartRow"),1,&keyarg,&row);
                fprintf(stderr,"install slot=%d type=%u stage=%u key=%llx row=%d\n",slot,type,stage,(unsigned long long)key,ah_array_size(ah_as_obj(row,AO_ARRAY)));return 1;
            }
            if(ah_call_member(vm,car,"setupPartsBuffer",0,NULL,&ret)<0){fprintf(stderr,"refresh slot=%d type=%u stage=%u key=%llx\n",slot,type,stage,(unsigned long long)key);return 1;}installs++;
        }
        CHECK(gt4_game_car_body(vm,car,body));
        for(int group=0;group<GT4_SETTING_GROUPS;group++)for(int index=0;index<counts[group];index++){
            int32_t value;CHECK(gt4_setting_get(body,group,index,&value));
            /* Exercise independent fields; keep automatic/final flags and
               gear count untouched so the authored auto-ratio path stays valid. */
            if(group!=GT4_SETTING_GEAR)CHECK(gt4_setting_set(body,group,index,value+slot+1));
            fields++;
        }
        CHECK(gt4_career_blob_set(vm,car,"full_car_ps2",body,sizeof body));
        CHECK(ah_call_member(vm,car,"endSetting",0,NULL,&ret)>=0);
    }
    uint8_t *packed=NULL;size_t packed_size;ah_obj *container=ah_module_new(vm,"fixture",NULL,NULL,NULL);
    ah_set_member(vm,container,"car",ah_objv(car));
    CHECK(gt4_state_pack(vm,container,&packed,&packed_size) && gt4_state_replace(vm,container,packed,packed_size));free(packed);
    car=get(vm,container,"car").u.o;CHECK(gt4_game_car_body(vm,car,body));
    uint8_t frozen[0x4b8];memcpy(frozen,body,sizeof body);
    for(uint32_t slot=0;slot<3;slot++){
        memcpy(body+0x488,&slot,4);uint8_t expected[0x178],actual[0x178];
        CHECK(gt4_career_active_setup(body,expected));
        gt4_race_desc d={0};snprintf(d.course,sizeof d.course,"tsukuba");d.cars=1;d.car[0]=id;d.owned_car[0]=body;
        gt4_race_run *run=gt4_race_run_open(argv[1],&d,error,sizeof error);CHECK(run);
        CHECK(gt4_race_run_setup(run,0,actual));
        if(getenv("GT4_SETTING_RACE_NEGATIVE"))actual[0x15b]^=1;
        CHECK(!memcmp(expected,actual,sizeof expected));
        uint32_t entry=GT4_U32(GT4_U32(GT4_U32(GT4_U32(gt4_race_run_conductor(run))+0x60)+8));
        CHECK(!memcmp(gt4_ptr(entry+0x3468),body+0x498,32));
        gt4_race_run_close(run);runs++;
        CHECK(!memcmp(frozen,body,0x488) && !memcmp(frozen+0x48c,body+0x48c,sizeof body-0x48c));
    }
    free(parts);
    }
    printf("PASS %u cars, %u disc part installations, %u setting fields in A/B/C, save roundtrip, %u complete entry setup/wear copies; caller body preserved.\n",car_count,installs,fields,runs);
    free(labels);menu_close(m);gt4_career_vehicle_close(factory);gt4_vol_close(vol);return 0;
}
