/* Local multiplayer integration; commands remain the original 32-byte pad
   stream. No dynamics state is injected. The negative swaps the two ports. */
#include "race_run.h"
#include "car.h"
#include "vol.h"
#include "specdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"split line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t cars[2];static int gas[2],negative,reads[2];
static int wheel(void *u,uint32_t car,int16_t state[2])
{(void)u;state[0]=state[1]=0;return car==cars[0] || car==cars[1];}
static int read_pad(void *u,uint32_t car,uint8_t *cmd,const uint8_t *ev)
{
    (void)u;int i=car==cars[0]?0:car==cars[1]?1:-1;if(i<0)return 0;
    memset(cmd,0,32);cmd[1]=1;cmd[3]=ev[3];cmd[8]=(uint8_t)gas[i^negative];
    memcpy(cmd+16,ev+16,ev[3]<=16?ev[3]:16);reads[i]++;return 1;
}
int main(int argc,char **argv)
{
    if(argc<2)return 2;negative=argc>2 && !strcmp(argv[2],"--negative");
    gt4_race_desc d={0};d.split_battle=1;d.cars=2;d.laps=1;d.tire=d.tire_2p=1;
    snprintf(d.course,sizeof d.course,"tsukuba");const char *error=NULL;
    gt4_vol *v=gt4_vol_open(argv[1],&error);CHECK(v);size_t n;
    uint8_t *labels=gt4_vol_read(v,"/specdb/GT4_US2560/GENERIC_CAR.idi",&n);CHECK(labels);
    CHECK(gt4_specdb_label(labels,n,"ka_01",&d.car[0]));d.car[1]=d.car[0];free(labels);gt4_vol_close(v);
    char msg[256];gt4_race_run *r=gt4_race_run_open(argv[1],&d,msg,sizeof msg);
    if(!r){fprintf(stderr,"%s\n",msg);return 1;}
    CHECK(gt4_race_run_cars(r)==2);uint32_t c=gt4_race_run_conductor(r);
    CHECK(GT4_U32(c+0x10140)==0x00679988u);
    for(int i=0;i<2;i++){cars[i]=gt4_race_run_car(r,i);CHECK(cars[i]);CHECK(GT4_U8(cars[i]+0x56a)==0);}
    gt4_phys_pad.read=read_pad;gt4_phys_pad.wheel_state=wheel;
    for(int k=0;k<660;k++)CHECK(gt4_race_run_frame(r));
    gas[0]=1;
    for(int k=0;k<120;k++)CHECK(gt4_race_run_frame(r));
    gt4_race_hud a,b;CHECK(gt4_race_run_hud(r,0,&a));CHECK(gt4_race_run_hud(r,1,&b));
    printf("P1-only: %.3f / %.3f m/s; reads %d/%d\n",a.speed,b.speed,reads[0],reads[1]);
    CHECK(a.speed>2 && fabsf(b.speed)<.5f);CHECK(reads[0]>0 && reads[1]>0);
    gas[0]=0;gas[1]=1;
    for(int k=0;k<120;k++)CHECK(gt4_race_run_frame(r));
    CHECK(gt4_race_run_hud(r,1,&b));CHECK(b.speed>2);
    /* The original Battle2P lap callback hands each finished driver to
       the existing run-out routine; exercise both entry indices. */
    for(int i=0;i<2;i++){
        gt4_session(c,0xa8,i,1,0,60000+i*1000,60000+i*1000,NULL);
        CHECK(GT4_U8(cars[i]+0x5b6) && GT4_U32(cars[i]+0x718)==60000u+(unsigned)i*1000);
    }
    CHECK(gt4_race_run_finished(r));gt4_race_run_close(r);
    r=gt4_race_run_open(argv[1],&d,msg,sizeof msg);CHECK(r && !gt4_race_run_finished(r));
    CHECK(!GT4_U8(gt4_race_run_car(r,1)+0x5b6));gt4_race_run_close(r);
    d.cars=1;r=gt4_race_run_open(argv[1],&d,msg,sizeof msg);CHECK(!r);
    puts("split battle: two human drivers, isolated input, one world; both finish, fresh restart, invalid count rejected");return 0;
}
