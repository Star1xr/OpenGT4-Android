/* GT Mode's Practice (00118670) through the race: RaceFreePractice
   (003B92D0, table 0067FC70, mode 12) with DynamicsConductorFreePractice
   (003F4EB8, table 006856E0). Its start (003F4F10): the player's car
   onto the box line, the others 300 m back and 200 m apart at speed;
   no laps counted (003F5008), so the race never finishes; the order by
   best lap (003F5350). The player's throttle takes it out of the box;
   on the course the model drives it, as it drives the others.
   --negative builds the Arcade's session instead (mode 0): its grid
   start and its laps (the race ends), so the run's checks fail. */
#include "physics/race_run.h"
#include "physics/phys.h"
#include "physics/car.h"
#include "specdb.h"
#include "vol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"practice line %d: %s\n",__LINE__,#x);return 1;}}while(0)

static uint32_t player;
/* the player's throttle, from the box out to the course, as a player would */
static int pad_read(void *ctx,uint32_t car,uint8_t *command,const uint8_t *event){
    (void)ctx;memset(command,0,0x20);command[3]=event[3];
    if(car==player){int16_t throttle=1000;command[1]=1;memcpy(command+8,&throttle,2);}
    return 1;
}
static int pad_wheel(void *ctx,uint32_t car,int16_t out[2]){(void)ctx;(void)car;out[0]=out[1]=1;return 1;}
static int pad_write(void *ctx,uint32_t car,const uint8_t *data){(void)ctx;(void)car;(void)data;return 1;}
int main(int argc,char **argv){
    if(argc<2)return 2;
    int negative=argc>2 && !strcmp(argv[2],"--negative");
    static const char *labels[6]={"ka_01","beat_91","s800_66","cappuccino_95","copen_activetop_02","prius_g_02"};
    const char *err=NULL;gt4_vol *v=gt4_vol_open(argv[1],&err);CHECK(v);
    size_t n=0;uint8_t *idi=gt4_vol_read(v,"/specdb/GT4_US2560/GENERIC_CAR.idi",&n);CHECK(idi);
    gt4_race_desc d;memset(&d,0,sizeof d);
    snprintf(d.course,sizeof d.course,"tsukuba");d.cars=6;
    for(int i=0;i<6;i++)CHECK(gt4_specdb_label(idi,n,labels[i],&d.car[i]));
    free(idi);gt4_vol_close(v);
    d.world_mode=negative?0:12;
    char msg[256];gt4_race_run *r=gt4_race_run_open(argv[1],&d,msg,sizeof msg);
    if(!r){fprintf(stderr,"open: %s\n",msg);return 1;}
    uint32_t cond=gt4_race_run_conductor(r),race=GT4_U32(cond),world=GT4_U32(race+0x84);
    printf("session %08x world %08x mode %u\n",GT4_U32(cond+0x10140),GT4_U32(world+0x64),GT4_U32(world+0xd68));
    if(!negative)CHECK(GT4_U32(cond+0x10140)==0x006856e0u && GT4_U32(world+0x64)==0x0067fc70u && GT4_U32(world+0xd68)==12);
    if(!negative)CHECK(gt4_session(cond,0x128,0,0,0,0,0,NULL)==0);
    player=gt4_race_run_car(r,0);
    if(!negative)CHECK(GT4_U8(player+0x104+0x467)==3);   /* 00345400: on the box line */
    gt4_phys_pad.read=pad_read;gt4_phys_pad.wheel_state=pad_wheel;gt4_phys_pad.write=pad_write;
    for(int i=1;i<6;i++)gt4_car_take_over(gt4_race_run_car(r,i),5);
    int16_t lap=0;unsigned frame,boxed=0,out=0;
    for(frame=0;frame<60*60*8 && lap<4;frame++){
        CHECK(gt4_race_run_frame(r));
        CHECK(!gt4_race_run_finished(r));
        uint8_t state=GT4_U8(player+0x104+0x467);
        if(state==3)boxed++;
        if(!out && !state){out=frame;gt4_car_take_over(player,5);}   /* out on the course: the model drives on */
        lap=(int16_t)GT4_U16(player+0x104+0x4ac);
        if(getenv("PRACTICE_TRACE") && frame%1800==0){printf("f%u",frame);for(int i=0;i<6;i++){uint32_t c=gt4_race_run_car(r,i);printf(" [%d lap%d v%.1f d%.0f st%u/%u]",i,(int16_t)GT4_U16(c+0x104+0x4ac),GT4_F32(c+0x104+0x53c),GT4_F32(c+0x104+0x4a4),GT4_U8(c+0x104+0x466),GT4_U8(c+0x104+0x467));}printf("\n");}
    }
    printf("%u frames, player on lap %d; %u frames at the box, out on the course at %u\n",frame,lap,boxed,out);
    CHECK(boxed>=0x78 && out>boxed);   /* 003F4F10 kind 6: the box, then the lane */
    CHECK(lap>=4);
    /* the order by best lap, the faster first */
    uint32_t best[6];int place[6];
    for(int i=0;i<6;i++){uint32_t c=gt4_race_run_car(r,i);best[i]=GT4_U32(c+0x71c);place[i]=GT4_U8(c+0x5b3);
        printf("car %d (%s): place %d best %u\n",i,labels[i],place[i],best[i]);}
    for(int i=0;i<6;i++)for(int k=0;k<6;k++)if(best[i]<best[k])CHECK(place[i]<place[k]);
    CHECK(best[0]!=0x157529ffu);
    gt4_race_run_close(r);
    puts("practice session: passed");return 0;
}
