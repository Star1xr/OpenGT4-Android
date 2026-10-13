/* The Arcade's QuickTune (00111DC0) through the race: the same Tsukuba
   race with and without it. The player's setup takes power/weight over
   100, tyre kinds by axle and +0x165 (cleared once the world fitted the
   tyres, 00387348), and the physics the race builds takes them: the
   ratings' peak power from that setup and the car's mass. The gearbox's
   top speed rewrites the automatic ratios (00443240). */
#include "physics/race_run.h"
#include "specdb.h"
#include "vol.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv){
    if(argc<2)return 2;
    const char *err=NULL;gt4_vol *v=gt4_vol_open(argv[1],&err);CHECK(v);
    size_t n=0;uint8_t *labels=gt4_vol_read(v,"/specdb/GT4_US2560/GENERIC_CAR.idi",&n);CHECK(labels);
    const char *label=argc>3?argv[3]:"crx_sir_90";
    uint32_t id;CHECK(gt4_specdb_label(labels,n,label,&id));free(labels);gt4_vol_close(v);
    gt4_race_desc d;memset(&d,0,sizeof d);
    snprintf(d.course,sizeof d.course,"tsukuba");d.cars=1;d.car[0]=id;
    char msg[256];
    gt4_race_run *plain=gt4_race_run_open(argv[1],&d,msg,sizeof msg);CHECK(plain);
    uint8_t stock[0x178];CHECK(gt4_race_run_setup(plain,0,stock));
    int gear,lo,hi;CHECK(gt4_race_run_gear_range(plain,0,&gear,&lo,&hi));
    float mass0=gt4_race_run_mass(plain,0);gt4_race_run_close(plain);
    printf("stock: power%% %u weight%% %u tyres %02x/%02x gear %d (%d..%d) mass %.1f\n",stock[0x163],stock[0x162],stock[0x160],stock[0x161],gear,lo,hi,mass0);
    CHECK(stock[0x163]==100 && stock[0x162]==100 && stock[0x165]==0);
    int negative=argc>2 && !strcmp(argv[2],"--negative");
    if(argc>4 && gear<=0)return 3;   /* survey: no adjustable gearbox */
    d.quick_tuned=1;d.quick_power=10;d.quick_weight=-10;d.quick_front_tire=negative?99:3;d.quick_rear_tire=3;
    d.quick_gear=hi>lo?hi:0;
    gt4_race_run *tuned=gt4_race_run_open(argv[1],&d,msg,sizeof msg);CHECK(tuned);
    uint8_t setup[0x178];CHECK(gt4_race_run_setup(tuned,0,setup));
    float mass1=gt4_race_run_mass(tuned,0);gt4_race_run_close(tuned);
    printf("tuned: power%% %u weight%% %u tyres %02x/%02x +0x165 %u gear byte %u mass %.1f\n",setup[0x163],setup[0x162],setup[0x160],setup[0x161],setup[0x165],setup[0x12a],mass1);
    CHECK(setup[0x163]==110 && setup[0x162]==90);
    CHECK(setup[0x165]==0);   /* 00387348 consumed the QuickTune's tyres */
    CHECK(memcmp(setup+0x68,stock+0x68,8) || memcmp(setup+0x60,stock+0x60,8));   /* a fitted axle changed */
    CHECK(setup[0x160]==d.quick_front_tire && setup[0x161]==3);
    gt4_car_ratings *cr=gt4_car_ratings_open(argv[1]);CHECK(cr);
    float p0,p1;int f0,r0,f1,r1;
    CHECK(gt4_car_ratings_setup(cr,stock,&p0,&f0,&r0) && gt4_car_ratings_setup(cr,setup,&p1,&f1,&r1));
    gt4_car_ratings_close(cr);
    printf("power %.2f -> %.2f PS (x%.3f), tyres %d/%d -> %d/%d, mass x%.3f\n",p0,p1,p1/p0,f0,r0,f1,r1,mass1/mass0);
    CHECK(fabsf(p1/p0-1.10f)<0.01f);
    CHECK(mass1<mass0 && fabsf(mass1/mass0-0.90f)<0.03f);
    CHECK(f1==d.quick_front_tire && r1==3);
    if(d.quick_gear)CHECK(setup[0x12a]==d.quick_gear && memcmp(setup+0x110,stock+0x110,24));
    puts("quick tune: passed");return 0;
}
