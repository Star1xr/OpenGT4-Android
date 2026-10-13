/* Replay a native checkpoint through the actual production race caller.
   No finish, rank, pose, AI coefficient or solver parameter is written. */
#define GT4_SERIES_EMBEDDED 1
#include "series_playable.c"
#include "../physics/state_digest.h"
int main(int argc,char **argv)
{
    CHECK(argc==5);const char *error=NULL;gt4_vol *vol=gt4_vol_open(argv[1],&error);CHECK(vol && gt4_game_load_disc(argv[1]));
    gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[1]);CHECK(factory);
    uint8_t unit[32]={0},body[0x4b8];CHECK(gt4_career_vehicle_create(factory,"gt40_race_69",0,0,unit,body));
    uint32_t rng;CHECK(gt4_career_event_rng_load(argv[1],&rng));gt4_career_event event;char message[256];
    CHECK(gt4_career_event_prepare(&event,argv[1],gt4_game_race_lookup_data(),"en_sarthe_old24h_0001",body,0,&rng,message,sizeof message));
    gt4_race_run *run=gt4_race_run_open(argv[1],&event.desc,message,sizeof message);CHECK(run && gt4_race_run_restore(run,argv[2]));
    CHECK(gt4_race_run_conductor(run)==0x1cf8ed0 && gt4_race_run_cars(run)==6);
    gt4_phys_pad.read=series_pad;gt4_phys_pad.wheel_state=idle_wheel;gt4_phys_pad.write=idle_write;
    FILE *output=fopen(argv[3],"wb");CHECK(output);unsigned frames=(unsigned)strtoul(argv[4],NULL,10);
    for(unsigned frame=0;frame<=frames;frame++){
        CHECK(state_write(output,gt4_ptr(gt4_race_run_car(run,0)),CAR_SIZE*6));
        if(frame<frames)CHECK(gt4_race_run_frame(run));
    }
    CHECK(!fclose(output));gt4_race_run_close(run);gt4_career_vehicle_close(factory);gt4_vol_close(vol);return 0;
}
