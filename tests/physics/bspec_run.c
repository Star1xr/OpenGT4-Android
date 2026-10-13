/* B-Spec through the race (COMMAND_SPECTATOR): the world's +0xCC0..+0xCCC
   at 1 (savestates bspec 05..08) make 003F1760 give the player's car to
   its driver (car+0x56A 1), which drives it with the pad idle. The
   orders go through the race's event block (cond+0xF8F8 + car * 4) and
   the controller's read (00426D50 copies the block into the command,
   00349FD0 hands +0x14/+0x15 to 003F19E0): the pace into the driver's
   +0x6D with its ratios (slot 07: level 4, +0x70/+0x74 0x3F8CCCCD), the
   overtake into +0x6E (slot 08). --negative builds it A-Spec: the car
   stays the player's and does not move. */
#include "physics/race_run.h"
#include "car.h"
#include "specdb.h"
#include "vol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t player;
/* the pad idle; the event block as 00426D50 copies it */
static int pad(void *ctx,uint32_t car,uint8_t *cmd,const uint8_t *ev){
    (void)ctx;if(car!=player)return 0;
    memset(cmd,0,0x20);cmd[1]=1;cmd[3]=ev[3];memcpy(cmd+0x10,ev+0x10,ev[3]<=0x10?ev[3]:0x10);return 1;
}
static int wheel(void *ctx,uint32_t car,int16_t s[2]){(void)ctx;(void)car;s[0]=s[1]=0;return 1;}
static int vib(void *ctx,uint32_t car,const uint8_t *d){(void)ctx;(void)car;(void)d;return 1;}
static int run(gt4_race_run *r,int frames){for(int i=0;i<frames;i++)if(!gt4_race_run_frame(r))return 0;return 1;}
int main(int argc,char **argv){
    if(argc<2)return 2;
    int negative=argc>2 && !strcmp(argv[2],"--negative"),pit=argc>2 && !strcmp(argv[2],"--pit");
    const char *err=NULL;gt4_vol *v=gt4_vol_open(argv[1],&err);CHECK(v);
    size_t n=0;uint8_t *labels=gt4_vol_read(v,"/specdb/GT4_US2560/GENERIC_CAR.idi",&n);CHECK(labels);
    gt4_race_desc d;memset(&d,0,sizeof d);
    snprintf(d.course,sizeof d.course,"grandvalley_east");d.cars=2;   /* a standing start */
    CHECK(gt4_specdb_label(labels,n,"beat_vf_92",&d.car[0]) || gt4_specdb_label(labels,n,"crx_sir_90",&d.car[0]));
    CHECK(gt4_specdb_label(labels,n,"crx_sir_90",&d.car[1]));
    free(labels);gt4_vol_close(v);
    d.bspec=!negative;d.pit_menu=pit;
    char msg[256];gt4_race_run *r=gt4_race_run_open(argv[1],&d,msg,sizeof msg);
    if(!r){fprintf(stderr,"%s\n",msg);return 1;}
    player=gt4_race_run_car(r,0);uint32_t cond=gt4_race_run_conductor(r),world=GT4_U32(GT4_U32(cond)+0x84);
    gt4_phys_pad.read=pad;gt4_phys_pad.wheel_state=wheel;gt4_phys_pad.write=vib;
    printf("world cc0..ccc %u %u %u %u, car kind %d\n",GT4_U32(world+0xcc0),GT4_U32(world+0xcc4),GT4_U32(world+0xcc8),
           GT4_U32(world+0xccc),(int8_t)GT4_U8(player+0x56a));
    if(negative){
        CHECK(GT4_U8(player+0x56a)==0);
        float d0=GT4_F32(player+0x5a8);CHECK(run(r,60*20));
        printf("A-Spec, pad idle: %.1f m -> %.1f m, %.1f m/s\n",d0,GT4_F32(player+0x5a8),GT4_F32(player+0x648));
        CHECK(GT4_F32(player+0x5a8)==d0 && GT4_F32(player+0x648)<1 && GT4_F32(player+0x7b0)==0);
        gt4_race_run_close(r);puts("bspec negative: passed");return 0;
    }
    CHECK(GT4_U8(player+0x56a)==1);
    uint32_t obj=player+0x14,ev=cond+0xf8f8u+GT4_U8(player+0x104+0x440)*4u;
    float d0=GT4_F32(player+0x5a8);CHECK(run(r,60*20));
    float d1=GT4_F32(player+0x5a8);
    float speed=GT4_F32(player+0x648);
    printf("B-Spec, pad idle: %.1f m -> %.1f m (past the line), %.1f m/s, pace %u overtake %u\n",d0,d1,speed,GT4_U8(obj+0x6d),GT4_U8(obj+0x6e));
    CHECK(d1<d0 && d1>50 && speed>10);   /* the grid is behind the line */
    /* 0035DD00: the B-Spec mileage (+0x7B0) runs with the total (+0x6D4)
       for a car of kind 1; 0035DDB0 hands the career both */
    printf("mileage: total %.1f m, B-Spec %.1f m\n",GT4_F32(player+0x6d4),GT4_F32(player+0x7b0));
    CHECK(GT4_F32(player+0x7b0)>100 && GT4_F32(player+0x7b0)==GT4_F32(player+0x6d4));
    CHECK(GT4_U8(obj+0x6d)==2);                 /* slot 05: Steady */
    int idx=GT4_U8(player+0x104+0x440);
    CHECK(gt4_bspec_order(cond,idx,4,2)==0);      /* 003F1BD8: already so */
    CHECK(gt4_bspec_order(cond,idx,4,5)==-1);     /* out of 0..4 */
    CHECK(gt4_bspec_order(cond,idx,4,4)==1 && GT4_U8(ev)==4 && GT4_U8(ev+1)==4);
    CHECK(run(r,2));
    CHECK(gt4_bspec_order(cond,idx,7,0)==4);
    printf("pace order 4: driver +0x6D %u +0x70 %08x +0x74 %08x\n",GT4_U8(obj+0x6d),GT4_U32(obj+0x70),GT4_U32(obj+0x74));
    CHECK(GT4_U8(obj+0x6d)==4 && GT4_U32(obj+0x70)==0x3f8ccccdu && GT4_U32(obj+0x74)==0x3f8ccccdu);
    CHECK(gt4_bspec_order(cond,idx,5,1)==1);CHECK(run(r,2));
    CHECK(gt4_bspec_order(cond,idx,8,0)==1);
    printf("overtake order: driver +0x6E %u\n",GT4_U8(obj+0x6e));
    CHECK(GT4_U8(obj+0x6e)==1);
    CHECK(run(r,60*10));
    printf("10 s later: %.1f m\n",GT4_F32(player+0x5a8));
    /* the pit menu (world+0xE408 on): the pit order takes the car into the
       box, its stop asks for the menu (0035BC70: 1, tyres 2|4, a B-Spec
       car's 0x10 clear), the B-Spec countdown 300 (car+0x750); a choice
       (0035BF08: soft racing tyres front and rear, no fuel) is fitted and
       the car leaves */
    if(pit){
        int asked=0,f,i;
        CHECK(gt4_bspec_order(cond,idx,6,1)==1);
        for(i=0;i<60*400 && !asked;i++){CHECK(run(r,1));asked=gt4_pit_menu_ask(player);}
        printf("pit menu asked after %.1f s: %#x, countdown %d, compounds %u %u\n",i/60.0,asked,
               (int16_t)GT4_U16(player+0x750),GT4_U8(cond+0xfb20u+idx*2u),GT4_U8(cond+0xfb21u+idx*2u));
        CHECK((asked&7)==7 && !(asked&0x10));
        CHECK(!gt4_pit_menu_ask(player));           /* once */
        /* at the box (laps.c case 3): the countdown */
        for(i=0;i<60*30 && (int16_t)GT4_U16(player+0x750)!=300;i++)CHECK(run(r,1));
        printf("at the box after %.1f s more\n",i/60.0);
        CHECK((int16_t)GT4_U16(player+0x750)==300);
        CHECK(!gt4_pit_menu_gone(player));
        gt4_pit_menu_touch(player);CHECK(GT4_U16(player+0x750)==0);
        f=GT4_U8(cond+0xfb20u+idx*2u);
        gt4_pit_menu_choose(player,f,f,0,0xff);
        CHECK(GT4_U8(cond+0xf910u+idx*4u)==1);
        for(i=0;i<60*60 && !gt4_pit_menu_gone(player);i++)CHECK(run(r,1));
        printf("out of the box after %.1f s, flags %#x\n",i/60.0,GT4_U16(player+0x5bc));
        CHECK(i<60*60);
    }
    gt4_race_run_close(r);puts("bspec: passed");return 0;
}
