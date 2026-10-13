#include "career_license_disc.h"
#include "car.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int idle(void *ctx,uint32_t car,uint8_t *command,const uint8_t *event){
 (void)ctx;(void)car;memset(command,0,0x20);command[3]=event[3];return 1;
}
static int wheel(void *ctx,uint32_t car,int16_t state[2]){(void)ctx;(void)car;state[0]=state[1]=0;return 1;}
static int vibration(void *ctx,uint32_t car,const uint8_t *data){(void)ctx;(void)car;(void)data;return 1;}
/* Disc integration, rather than an exact grade: the rules themselves have
   license_session_run's EE differential oracle. Every authored exam must
   construct and advance without executing the EE. */
int main(int argc,char **argv){
 if(argc!=2)return 2;gt4_career_race_lookup_data lookup;
 if(!gt4_career_race_lookup_data_load(argv[1],&lookup))return 2;
 const char *groups[]={"l0b","l0a","lib","lia","l0s"};unsigned count=0;
 for(unsigned g=0;g<5;g++)for(unsigned n=1;n<=16;n++){
  char code[32],error[256];snprintf(code,sizeof code,"%s%04u",groups[g],n);
  gt4_career_license_exam exam;
  if(!gt4_career_license_prepare(&exam,argv[1],&lookup,code,error,sizeof error)){fprintf(stderr,"%s\n",error);return 1;}
  /* Zero entries contradicts 003BB478's disc participant count even
     for B-1, whose normal RACE row already contains one entry. */
  if(getenv("GT4_LICENSE_DISC_NEGATIVE") && !count)exam.desc.cars=0;
  gt4_race_run *race=gt4_race_run_open(argv[1],&exam.desc,error,sizeof error);
  if(!race){fprintf(stderr,"%s: %s\n",code,error);return 1;}
  gt4_phys_pad.read=idle;
  gt4_phys_pad.wheel_state=wheel;gt4_phys_pad.write=vibration;
  int32_t state;uint32_t time;
  if(!gt4_race_run_license_result(race,&state,&time) || state!=-2 || time!=0x157529ffu)return 1;
  for(unsigned i=0;i<4;i++)if(!gt4_race_run_frame(race)){fprintf(stderr,"%s frame %u\n",code,i);return 1;}
  printf("%s %s car %x kind %u\n",code,exam.desc.course,exam.desc.car[0],GT4_U8(gt4_race_run_conductor(race)+0xf81a));fflush(stdout);
  gt4_race_run_close(race);count++;
 }
 printf("PASS %u disc examination setups and native initial frames.\n",count);return 0;
}
