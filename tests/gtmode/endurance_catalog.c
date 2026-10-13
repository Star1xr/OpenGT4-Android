/* Every endurance discovered from PresentData, with an original opponent
   model as the owned stock fixture. This checks startup, not whole runs. */
#define GT4_SUNDAY_EMBEDDED 1
#include "sunday_playable.c"
int main(int argc,char **argv)
{
    CHECK(argc==2);const char *error=NULL;gt4_vol *vol=gt4_vol_open(argv[1],&error);CHECK(vol && gt4_game_load_disc(argv[1]));
    char msg[256];menu *m=menu_open(vol,"gtmode",msg,sizeof msg);CHECK(m);load_race_tables(m);load_car_tables(m);
    gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[1]);CHECK(factory);gt4_game_set_stats_reader(gt4_career_vehicle_stats,factory);
    uint8_t unit[32]={0},body[0x4b8];CHECK(gt4_career_vehicle_create(factory,"minolta_toyota_88cv_89",0,0,unit,body));uint32_t rng;CHECK(gt4_career_event_rng_load(argv[1],&rng));
    ah_vm *vm=m->vm;ah_set_log(vm,log_script,NULL);ah_obj *data=get(vm,m->project,"PresentData").u.o,*details=get(vm,data,"detail_carlist").u.o;
    unsigned races=0,timed=0,lapped=0;
    for(int id=0;id<ah_array_size(details);id++){
        const char *series=ah_cstr(ah_array_get(details,id));if(!series || strncmp(series,"en_",3))continue;
        ah_val arg=ah_str(vm,series),list;CHECK(ah_call_member(vm,m->project,"extractSubRaceCode",1,&arg,&list)>=0 && ah_array_size(list.u.o)==1);
        const char *label=ah_cstr(ah_array_get(list.u.o,0));CHECK(label);char race[32];snprintf(race,sizeof race,"%s",label);
        gt4_career_event event;CHECK(gt4_career_event_prepare(&event,argv[1],gt4_game_race_lookup_data(),race,body,0,&rng,msg,sizeof msg));
        const char *car_label=gt4_specdb_label_for_id(m->car_labels,m->car_labels_size,event.desc.car[1]);CHECK(car_label);
        memset(unit,0,sizeof unit);CHECK(gt4_career_vehicle_create(factory,car_label,0,0,unit,body));
        CHECK(gt4_career_event_prepare(&event,argv[1],gt4_game_race_lookup_data(),race,body,0,&rng,msg,sizeof msg));
        ah_obj *car=ah_module_new(vm,"endurance_owned_fixture",NULL,NULL,NULL);ah_set_member(vm,car,"car_code",ah_str(vm,car_label));ah_set_member(vm,car,"color_index",ah_int(0));
        CHECK(gt4_career_blob_set(vm,car,"full_car_ps2",body,sizeof body));ah_val owned=ah_objv(car),ret;CHECK(nat_bind_game_car(vm,NULL,m,1,&owned,&ret)>=0);
        ah_val reg_args[]={owned,ah_str(vm,race)},reg;CHECK(ah_call_member(vm,m->project,"getViolatedRegulatoin",2,reg_args,&reg)>=0 && !ah_truthy(get(vm,reg.u.o,"violated")));
        gt4_race_run *run=gt4_race_run_open(argv[1],&event.desc,msg,sizeof msg);if(!run){fprintf(stderr,"%s: %s\n",race,msg);return 1;}
        uint32_t cond=gt4_race_run_conductor(run);unsigned minutes=GT4_U16(cond+0xf850u),laps=gt4_session(cond,0x128,0,0,0,0,0,NULL);
        if(getenv("GT4_ENDURANCE_CATALOG_NEGATIVE"))minutes^=1;
        CHECK(minutes==(unsigned)gt4_career_race_field(event.race_row,GT4_RACE_LIMIT_TIME));CHECK(laps==event.race_row[0x43]);CHECK(minutes || laps);
        gt4_phys_pad.read=idle_pad;gt4_phys_pad.wheel_state=idle_wheel;gt4_phys_pad.write=idle_write;
        for(unsigned frame=0;frame<600;frame++)CHECK(gt4_race_run_frame(run));
        CHECK(!gt4_race_run_finished(run) && gt4_race_run_cars(run)==event.race_row[0x3e]);
        printf("endurance %s car=%s laps=%u minutes=%u\n",race,car_label,laps,minutes);fflush(stdout);
        gt4_race_run_close(run);races++;timed+=minutes>0;lapped+=laps>0;
    }
    CHECK(races && timed && lapped && !script_errors);
    printf("PASS %u original endurance startups: %u timed / %u lap-limited, legal stock fixtures, grids, native countdown and original limits. Full duration not covered here.\n",races,timed,lapped);
    gt4_game_set_stats_reader(NULL,NULL);gt4_career_vehicle_close(factory);menu_close(m);gt4_vol_close(vol);return 0;
}
