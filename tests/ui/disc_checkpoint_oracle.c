/* Replay snapshot import compared with original 0035ABA8, no solver edits. */
#include "disc_checkpoint.h"
#include "disc_replay.h"
#include "race_run.h"
#include "car.h"
#include "ee.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fenv.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"checkpoint line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
    CHECK(argc==2 || argc==3);int negative=argc==3;const char *why;char error[256];
    gt4_vol *v=gt4_vol_open(argv[1],&why);CHECK(v);gt4_disc_replay *reader=gt4_disc_replay_open(v);CHECK(reader);
    gt4_replay_file file;CHECK(gt4_disc_replay_read(reader,"/replay/Rdemo9/Rdemo9",&file,error,sizeof error));
    CHECK(file.disc_segments==1);gt4_race_desc d={0};strcpy(d.course,file.course);d.cars=1;d.car[0]=file.car_id[0];d.owned_car[0]=file.owned[0];d.time_trial=1;
    gt4_race_run *run=gt4_race_run_open(argv[1],&d,error,sizeof error);CHECK(run);
    uint32_t cond=gt4_race_run_conductor(run),car=gt4_race_run_car(run,0),snapshot=0x1e10000;
    const uint8_t *state=file.disc_info+file.disc_segment_offset[0];
    ee_ctx ee;ee_init(&ee);memcpy(ee.ram,gt4_phys_mem,32u*1024*1024);ee.lookup=recomp_lookup;
    memcpy(ee.ram+snapshot,state,0x14f4);ee.r[4].uw[0]=cond;ee.r[5].uw[0]=0;ee.r[6].uw[0]=snapshot;ee.r[29].uw[0]=0x1ff0000;
    recomp_lookup(0x35aba8)(&ee);CHECK(ee.end_reason==EE_END_NONE);
    fenv_t env;fegetenv(&env);gt4_phys_fpu_enter();disc_checkpoint_restore(cond,0,state);fesetenv(&env);
    if(negative)GT4_U8(car+0x400)^=1;
    unsigned different=0;for(unsigned i=0;i<0x10a0;i++)if(ee.ram[car+i]!=GT4_U8(car+i)){if(different<10)fprintf(stderr,"car +%x original=%02x native=%02x\n",i,ee.ram[car+i],GT4_U8(car+i));different++;}
    for(unsigned i=0;i<0x1dc8;i++)if(ee.ram[cond+0xda6c+i]!=GT4_U8(cond+0xda6c+i)){if(different<10)fprintf(stderr,"hulls +%x original=%02x native=%02x\n",i,ee.ram[cond+0xda6c+i],GT4_U8(cond+0xda6c+i));different++;}
    CHECK(!different);puts("PASS original 0035ABA8 car state, links, history and collision hulls; negative flips one car byte.");
    gt4_race_run_close(run);gt4_replay_free(&file);gt4_disc_replay_close(reader);gt4_vol_close(v);ee_free(&ee);return 0;
}
