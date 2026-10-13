/* GT Mode's Power & Speed (00115A90): RaceMachineTest (003BDD60, world
   00680EE0, mode 15) with DynamicsConductorMachineTest (003F6958,
   00685BF0). The race's laps word picks the test: 1 the 400 m, 2 the
   1000 m (a split at 400 m), 3 the top speed (6845 m, splits at 400 and
   1000 m). No laps are counted; past the last mark the car finishes (its
   finish event), with its time (400 / 1000 m) or its top speed kept.
   --negative builds the Arcade's session instead (mode 0): it counts laps
   and has no 400 m end, so the checks fail. */
#include "physics/race_run.h"
#include "physics/phys.h"
#include "physics/car.h"
#include "specdb.h"
#include "vol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"machine test line %d: %s\n",__LINE__,#x);return 1;}}while(0)

static uint32_t player;
static int pad_read(void *ctx,uint32_t car,uint8_t *command,const uint8_t *event){
    (void)ctx;memset(command,0,0x20);command[3]=event[3];
    if(car==player){int16_t throttle=1000;command[1]=1;memcpy(command+8,&throttle,2);}
    return 1;
}
static int pad_wheel(void *ctx,uint32_t car,int16_t out[2]){(void)ctx;(void)car;out[0]=out[1]=1;return 1;}
static int pad_write(void *ctx,uint32_t car,const uint8_t *data){(void)ctx;(void)car;(void)data;return 1;}
static int run(const char *disc,const char *course,int test,int negative,uint32_t car_id)
{
    gt4_race_desc d;memset(&d,0,sizeof d);
    snprintf(d.course,sizeof d.course,"%s",course);d.cars=1;d.car[0]=car_id;d.laps=test;
    d.world_mode=negative?0:15;
    char msg[256];gt4_race_run *r=gt4_race_run_open(disc,&d,msg,sizeof msg);
    if(!r){fprintf(stderr,"open: %s\n",msg);return 1;}
    uint32_t cond=gt4_race_run_conductor(r),race=GT4_U32(cond),world=GT4_U32(race+0x84);
    printf("test %d on %s: session %08x world %08x mode %u\n",test,course,GT4_U32(cond+0x10140),GT4_U32(world+0x64),GT4_U32(world+0xd68));
    CHECK(GT4_U32(cond+0x10140)==0x00685bf0u && GT4_U32(world+0xd68)==15);
    CHECK(gt4_session(cond,0x128,0,0,0,0,0,NULL)==0);
    float end;gt4_session(cond,0x138,0,0,0,0,0,&end);
    static const float ends[4]={0,400,1000,6845};
    CHECK(end==ends[test]);
    uint32_t car=gt4_race_run_car(r,0);player=car;
    gt4_phys_pad.read=pad_read;gt4_phys_pad.wheel_state=pad_wheel;gt4_phys_pad.write=pad_write;
    gt4_car_take_over(car,5);
    unsigned frame;float top=0,dist=0;
    for(frame=0;frame<60*60*8 && !gt4_race_run_finished(r);frame++){
        CHECK(gt4_race_run_frame(r));
        if(GT4_F32(car+0x6c8)>top)top=GT4_F32(car+0x6c8);
        dist=GT4_F32(car+0x104+0x4a4);
    }
    gt4_race_hud h;gt4_race_run_hud(r,0,&h);
    uint32_t entry=GT4_U32(GT4_U32(GT4_U32(race+0x60)+8));
    printf("  %u frames, finished %d at %.1f m, best %u ms, top %.2f m/s (kept %.2f, entry %.2f)\n",frame,gt4_race_run_finished(r),dist,
           GT4_U32(car+0x71c),top,GT4_F32(car+0x6cc),GT4_F32(entry+0x3464));
    CHECK(gt4_race_run_finished(r));
    if(test<3){CHECK(GT4_U32(car+0x71c)!=0x157529ffu);}
    else {CHECK(GT4_F32(car+0x6cc)>0 && GT4_F32(car+0x6cc)>=top-0.01f && GT4_F32(entry+0x3464)==GT4_F32(car+0x6cc));}
    gt4_race_run_close(r);
    return 0;
}
int main(int argc,char **argv){
    if(argc<2)return 2;
    int negative=argc>2 && !strcmp(argv[2],"--negative");
    const char *err=NULL;gt4_vol *v=gt4_vol_open(argv[1],&err);if(!v)return 2;
    size_t n=0;uint8_t *idi=gt4_vol_read(v,"/specdb/GT4_US2560/GENERIC_CAR.idi",&n);uint32_t id=0;
    int ok=idi && gt4_specdb_label(idi,n,"ka_01",&id);free(idi);gt4_vol_close(v);if(!ok)return 2;
    for(int test=1;test<=3;test++)if(run(argv[1],"testcourse",test,negative,id))return 1;
    puts("machine test: passed");return 0;
}
