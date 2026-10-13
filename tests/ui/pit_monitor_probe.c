/* Native monitor/pit input integration, with the original Strobe asset.
   Original ownership predicate is tested separately by pit_walk_oracle. */
#define SDL_MAIN_HANDLED
#include "race_bspec.h"
#include "platform/platform.h"
#include "plat_sdl.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"pit monitor line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static unsigned orders;
static void order(void*u,int kind,int value){(void)u;(void)kind;(void)value;orders++;}
int main(int argc,char**argv){
 CHECK(argc==2 || argc==3);plat_video_desc video={640,480,0,0,0};CHECK(!plat_video_init(&video));SDL_HideWindow(plat_sdl_window());
 const char*error;gt4_vol*v=gt4_vol_open(argv[1],&error);CHECK(v);race_bspec_host host={0};host.order=order;
 race_bspec*b=race_bspec_open(v,&host,0);CHECK(b);race_bspec_state s={0};s.cars=2;s.player=0;s.course_length=1000;
 for(unsigned i=0;i<2;i++){s.car[i].index=i;s.car[i].grid=i+1;s.car[i].last_lap=s.car[i].best_lap=0x157529ff;s.car[i].pace_up=-1;snprintf(s.car[i].name,sizeof s.car[i].name,"Car %u",i+1);}
 for(unsigned stop=0;stop<2;stop++){
  race_bspec_update(b,&s,RACE_BSPEC_R1,0,1.f/60);
  for(unsigned n=0;n<30;n++)race_bspec_update(b,&s,0,0,1.f/60);
  CHECK(race_bspec_mode(b)==1);unsigned before=orders;
  for(unsigned phase=1;phase<=5;phase++){
   s.pit_sequence=phase;
   for(unsigned n=0;n<30;n++)race_bspec_update(b,&s,RACE_BSPEC_R1|RACE_BSPEC_DOWN|RACE_BSPEC_RIGHT|RACE_BSPEC_OVERTAKE|RACE_BSPEC_L1_RIGHT,RACE_BSPEC_L1,1.f/60);
   CHECK(race_bspec_mode(b)==2 && race_bspec_selected(b)==0 && race_bspec_speed(b)==1 && orders==before);
  }
  s.pit_sequence=0;race_bspec_update(b,&s,RACE_BSPEC_DOWN,0,1.f/60);
  CHECK(race_bspec_selected(b)==1 && orders==before);
  if(argc==3 && !strcmp(argv[2],"--negative"))CHECK(race_bspec_mode(b)==1);
  race_bspec_update(b,&s,RACE_BSPEC_UP|RACE_BSPEC_RIGHT,0,1.f/60);CHECK(race_bspec_selected(b)==0 && orders>before);
 }
 race_bspec_close(b);gt4_vol_close(v);plat_video_shutdown();puts("PASS two complete stops: monitor forces live, rejects all monitor input, releases input after exit.");return 0;
}
