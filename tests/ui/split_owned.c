/* Garage-to-Battle2P boundary, stock/owned mixtures and retained Retry.
   Uses the original native vehicle factory and unchanged race builder. */
#include <SDL.h>
#include "plat_sdl.h"
#include "platform/platform.h"
#include "career_vehicle.h"
#include "../../src/app/race_audio.c"
#include "../../src/app/race_game.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"split owned line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static int frozen_odometer;
static int odometer_pad(void *user,uint32_t car,uint8_t *cmd,const uint8_t *event)
{
 (void)user;(void)car;memset(cmd,0,32);cmd[1]=1;cmd[3]=event[3];cmd[8]=1;
 memcpy(cmd+16,event+16,event[3]<=16?event[3]:16);return 1;
}
static int odometer_wheel(void *user,uint32_t car,int16_t state[2])
{(void)user;(void)car;state[0]=state[1]=0;return 1;}
static void check_odometer(race_game *g,uint32_t start)
{
 gt4_race_hud before,after;CHECK(gt4_race_run_hud(g->run,0,&before));CHECK(before.odometer==start);
 gt4_phys_pad.read=odometer_pad;gt4_phys_pad.wheel_state=odometer_wheel;
 for(int frame=0;frame<900;frame++)CHECK(gt4_race_run_frame(g->run));
 CHECK(gt4_race_run_hud(g->run,0,&after));
 if(frozen_odometer)after.odometer=before.odometer;
 /* 0035DD00 -> 00343518 counts metres in +6dc; the HUD must retain
    the purchased mileage while the race-only +6d4 starts at zero. */
 float travelled=GT4_F32(gt4_race_run_car(g->run,0)+CAR_DISTANCE);
 printf("odometer start=%u now=%u race=%.3f m\n",start,after.odometer,travelled);
 CHECK(travelled>10 && after.odometer>start);
 CHECK(fabsf((float)(after.odometer-start)-travelled)<1.01f);
 gt4_phys_pad.read=NULL;gt4_phys_pad.wheel_state=NULL;
}
int main(int argc,char **argv)
{
 CHECK(argc>=2);int negative=argc>2 && !strcmp(argv[2],"--negative");
 frozen_odometer=argc>2 && !strcmp(argv[2],"--negative-odometer");
 plat_video_desc video={640,448,0,0,0};CHECK(!plat_video_init(&video));SDL_HideWindow(SDL_GetWindowFromID(1));
 gt4_career_vehicle *factory=gt4_career_vehicle_open(argv[1]);CHECK(factory);
 uint8_t body[2][0x4b8],unit[32]={0},expected[2][0x178];
 const char *labels[2]={"ka_01","focus_st170_jp_03"};int colors[2]={2,1};
 for(int i=0;i<2;i++){
  CHECK(gt4_career_vehicle_create(factory,labels[i],colors[i],i?0:85574576,unit,body[i]));
  uint32_t slot=i+1;memcpy(body[i]+0x488,&slot,4);
  CHECK(gt4_career_active_setup(body[i],expected[i]));
 }
 race_game *g=race_game_open(argv[1]);CHECK(g);char msg[256];
 for(int mask=1;mask<4;mask++){
  gt4_race_desc options={0};options.tire_2p=1;
  for(int i=0;i<2;i++)if(mask&(1<<i))options.owned_car[i]=body[i];
  race_game_set_split(g,&options);race_game_set_driving(g,0,0,0,0);
  if(negative)g->split_options.owned_car[0]=NULL;
  const char *others[]={labels[1]};int paints[]={colors[1]};
  CHECK(race_game_start(g,"grandvalley_east",labels[0],colors[0],others,paints,1,msg,sizeof msg));
  for(int retry=0;retry<2;retry++){
   if(retry)CHECK(race_game_restart(g,msg,sizeof msg));
   CHECK(race_game_split(g) && gt4_race_run_cars(g->run)==2);
   for(int i=0;i<2;i++)if(mask&(1<<i)){
    uint8_t setup[0x178];CHECK(gt4_race_run_setup(g->run,i,setup));
    if(memcmp(setup,expected[i],sizeof setup))for(int k=0;k<sizeof setup;k++)if(setup[k]!=expected[i][k])fprintf(stderr,"P%d +%x actual %02x expected %02x\n",i,k,setup[k],expected[i][k]);
    CHECK(!memcmp(setup,expected[i],sizeof setup));
    CHECK(g->last_desc.owned_car[i]==g->last_owned[i] && !memcmp(g->last_owned[i],body[i],sizeof body[i]));
    CHECK((i?g->last_colors[0]:g->last_color)==colors[i]);
   }
  }
 }
 /* The same garage body must survive Arcade and Time Trial restarts. */
 race_game_set_split(g,NULL);race_game_set_arcade_owned(g,body[0]);
 for(int trial=0;trial<2;trial++){
  race_game_set_time_trial(g,trial);
  const char *others[]={labels[1]};int paints[]={colors[1]};
  CHECK(race_game_start(g,"grandvalley_east",labels[0],colors[0],others,paints,trial?0:1,msg,sizeof msg));
  for(int retry=0;retry<2;retry++){
   if(retry)CHECK(race_game_restart(g,msg,sizeof msg));
   uint8_t setup[0x178];CHECK(!race_game_split(g) && gt4_race_run_setup(g->run,0,setup));
   CHECK(!memcmp(setup,expected[0],sizeof setup));
   CHECK(g->last_desc.time_trial==trial && g->last_color==colors[0]);
   CHECK(!memcmp(g->last_owned[0],body[0],sizeof body[0]));
   check_odometer(g,85574576);
  }
  race_game_set_arcade_owned(g,NULL);
  CHECK(race_game_start(g,"grandvalley_east",labels[0],colors[0],others,paints,trial?0:1,msg,sizeof msg));
  check_odometer(g,0);
  race_game_set_arcade_owned(g,body[0]);
 }
 race_game_close(g);gt4_career_vehicle_close(factory);plat_video_shutdown();
 puts("PASS garage Battle2P: owned/stock mixtures, two active sheets/paints, full bodies, Retry");return 0;
}
