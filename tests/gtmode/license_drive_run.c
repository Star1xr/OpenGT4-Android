/* Controller integration, C: test inputs drive an exam without modifying
   its geometry, state, thresholds or physics. Rule accuracy is checked by
   license_session_run's separate original EE oracle. */
#include "career_license_disc.h"
#include "car.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t player;
static unsigned frame,brake_at,wait_until;
static int read_pad(void *ctx,uint32_t car,uint8_t *command,const uint8_t *event){
 (void)ctx;if(car!=player)return 0;
 memset(command,0,0x20);command[1]=1;command[3]=event[3];
 int16_t throttle=frame>=wait_until && frame<brake_at,brake=frame>=brake_at;
 memcpy(command+8,&throttle,2);memcpy(command+10,&brake,2);return 1;
}
static int wheel(void *ctx,uint32_t car,int16_t out[2]){(void)ctx;out[0]=out[1]=0;return car==player;}
static int write_pad(void *ctx,uint32_t car,const uint8_t *data){(void)ctx;(void)data;return car==player;}
int main(int argc,char **argv){
 if(argc<3)return 2;
 gt4_career_race_lookup_data lookup;if(!gt4_career_race_lookup_data_load(argv[1],&lookup))return 2;
 char error[256];gt4_career_license_exam exam;
 if(!gt4_career_license_prepare(&exam,argv[1],&lookup,argv[2],error,sizeof error)){fprintf(stderr,"%s\n",error);return 1;}
 brake_at=argc>3?(unsigned)atoi(argv[3]):UINT32_MAX;
 wait_until=argc>4?(unsigned)atoi(argv[4]):0;
 gt4_race_run *race=gt4_race_run_open(argv[1],&exam.desc,error,sizeof error);
 if(!race){fprintf(stderr,"%s\n",error);return 1;}
 player=gt4_race_run_car(race,0);gt4_phys_pad.read=read_pad;gt4_phys_pad.wheel_state=wheel;gt4_phys_pad.write=write_pad;
 int32_t state=-2;uint32_t elapsed=0x157529ff;
 for(frame=0;frame<1800 && !gt4_race_run_finished(race);frame++){
  if(!gt4_race_run_frame(race)){fprintf(stderr,"frame %u failed\n",frame);return 1;}
  if(!(frame%120))fprintf(stderr,"frame %u distance %.3f speed %.3f\n",frame,GT4_F32(player+0x5a8),GT4_F32(player+0x648));
 }
 if(!gt4_race_run_license_result(race,&state,&elapsed))return 1;
 printf("%s state %d time %u frame %u finished %d\n",argv[2],state,elapsed,frame,gt4_race_run_finished(race));
 if(getenv("GT4_LICENSE_DRIVE_NEGATIVE"))state=-2;
 int ok=gt4_race_run_finished(race) && (wait_until?state==0:state>=2 && state<=4);
 /* C: deterministic native input regression, not a PS2 physics oracle. */
 if(!strcmp(argv[2],"l0b0001") && brake_at==980 && !wait_until)ok=ok && state==3 && elapsed==10248;
 if(!strcmp(argv[2],"l0b0001") && brake_at==1420 && wait_until==900)ok=ok && state==0 && elapsed==17599 && frame==1535;
 gt4_race_run_close(race);return ok?0:1;
}
