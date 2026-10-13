#include "career_license_disc.h"
#include "license_session.h"
#include "car.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mission_pad_fixture.h"
static int idle(void *ctx,uint32_t car,uint8_t *command,const uint8_t *event){
 (void)ctx;(void)car;memset(command,0,0x20);command[3]=event[3];return 1;
}
static int wheel(void *ctx,uint32_t car,int16_t state[2]){(void)ctx;(void)car;state[0]=state[1]=0;return 1;}
static int vibration(void *ctx,uint32_t car,const uint8_t *data){(void)ctx;(void)car;(void)data;return 1;}
/* GT4_MISSION_RUN: the player's pad held on the throttle, as license_drive_run */
static uint32_t player;
static gt4_race_run *driven;
static FILE *pad_output;
static int throttle(void *ctx,uint32_t car,uint8_t *command,const uint8_t *event){
 (void)ctx;if(car!=player)return 0;
 memset(command,0,0x20);command[1]=1;command[3]=event[3];int16_t on=1;memcpy(command+8,&on,2);
 if(getenv("GT4_MISSION_STEER"))mission_pad_fixture(driven,car,command);
 if(pad_output)fwrite(command,1,32,pad_output);return 1;
}
/* Disc integration of the 34 driving missions (RACE l0m0001..0034):
   each is prepared from RACE/RACES (0011BEC0), built as RaceMission
   (003BF6E8: world 006815E0, mode 10, conductor 00685DA0) and advances
   without the EE. The conductor's own entries have their oracle in
   mission_session_run. GT4_MISSION_DISC_NEGATIVE builds the first one with
   the training's conductor and must fail. */
int main(int argc,char **argv){
 if(argc!=2)return 2;gt4_career_race_lookup_data lookup;
 if(!gt4_career_race_lookup_data_load(argv[1],&lookup))return 2;
 unsigned count=0;
 for(unsigned n=getenv("GT4_MISSION_ONE")?(unsigned)atoi(getenv("GT4_MISSION_ONE")):1;n<=(getenv("GT4_MISSION_ONE")?(unsigned)atoi(getenv("GT4_MISSION_ONE")):34);n++){
  char code[32],error[256];snprintf(code,sizeof code,"l0m%04u",n);
  gt4_career_license_exam mission;
  if(!gt4_career_mission_prepare(&mission,argv[1],&lookup,code,error,sizeof error)){fprintf(stderr,"%s\n",error);return 1;}
  if(mission.desc.license!=3 || mission.desc.cars<1 || mission.desc.cars>6){fprintf(stderr,"%s: %d cars\n",code,mission.desc.cars);return 1;}
  if(getenv("GT4_MISSION_DISC_NEGATIVE") && !count)mission.desc.license=2;
  gt4_race_run *race=gt4_race_run_open(argv[1],&mission.desc,error,sizeof error);
  if(!race){fprintf(stderr,"%s: %s\n",code,error);return 1;}
  uint32_t cond=gt4_race_run_conductor(race);
  if(GT4_U32(cond+0x10140)!=GT4_MISSION_SESSION_VT){fprintf(stderr,"%s: conductor %08x\n",code,GT4_U32(cond+0x10140));return 1;}
  gt4_phys_pad.read=idle;gt4_phys_pad.wheel_state=wheel;gt4_phys_pad.write=vibration;
  const char *pad_path=getenv("GT4_MISSION_PAD_OUTPUT");pad_output=pad_path?fopen(pad_path,"wb"):NULL;
  if(pad_path && !pad_output)return 1;
  for(unsigned i=0;i<4;i++){
   uint8_t zero[32]={0};zero[1]=1;if(pad_output)fwrite(zero,1,32,pad_output);
   if(!gt4_race_run_frame(race)){fprintf(stderr,"%s frame %u\n",code,i);return 1;}
  }
  printf("%s %s cars %d laps %d kind %u player %x\n",code,mission.desc.course,mission.desc.cars,mission.desc.laps,
         GT4_U8(cond+0xf81a),mission.desc.car[0]);fflush(stdout);
  if(getenv("GT4_MISSION_RUN")){   /* the player on the throttle: the session's states */
   driven=race;player=gt4_race_run_car(race,0);gt4_phys_pad.read=throttle;int32_t last=0x7fffffff;
   {float start=0,goal=0;gt4_session(cond,0x130,0,0,0,0,0,&start);gt4_session(cond,0x138,0,0,0,0,0,&goal);
    printf("  start line %.1f goal %.1f kind %u f866 %u laps %u\n",start,goal,GT4_U8(cond+0xf81a),GT4_U8(cond+0xf866),GT4_U32(GT4_U32(cond)));}
   for(unsigned f=0;f<60*240;f++){
    if(!gt4_race_run_frame(race))break;
    int32_t state;uint32_t time;gt4_race_run_license_result(race,&state,&time);
    gt4_race_hud h;gt4_race_run_hud(race,0,&h);
    if(state!=last){printf("  frame %u state %d time %u finished %d lap %d rank %d world %u\n",f,state,time,h.finished,h.lap,h.rank,
        GT4_U32(GT4_U32(GT4_U32(cond)+0x84)+0xce0));last=state;}
    if(!(f%600)){uint32_t pl=gt4_race_run_car(race,0);printf("  frame %u distance %.1f speed %.2f lap %d finished %d other %.1f\n",f,GT4_F32(pl+0x5a8),GT4_F32(pl+0x648),h.lap,h.finished,GT4_F32(gt4_race_run_car(race,1)+0x5a8));}
    if(gt4_race_run_finished(race)){printf("  race finished at frame %u state %d\n",f,state);break;}
    if(h.finished && state!=-2)break;
   }
  }
  if(pad_output){fclose(pad_output);pad_output=NULL;}gt4_race_run_close(race);count++;
 }
 printf("PASS %u disc mission setups and native initial frames.\n",count);return 0;
}
