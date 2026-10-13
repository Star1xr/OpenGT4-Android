/* RaceLicense rules: func_003F8818..003FA30C. No original-code bridge. */
#include "license_session.h"
#include "car.h"
#include "career_original.h"
#include <math.h>
#define K(x) gt4_f32(x)
#define NONE 0x157529ffu
static uint32_t car_at(uint32_t c,uint32_t i){return c+0x6698u+i*CAR_SIZE;}

/* func_003F8818: explicit finish policy +0xf859, otherwise test kind
   +0xf81a, contact/time limit +0xf85e and all-car finish +0xf858. */
int gt4_license_finish_mode(uint32_t c,int failure)
{
    int policy=GT4_U8(c+0xf859),kind=GT4_U8(c+0xf81a),s=3;
    if(policy==1)return 3;
    if(policy==2)return 1;
    if(policy==3)return 2;
    if(kind<3)s=kind==1?(failure?1:2):1;
    else s=(kind==3 || kind==4 || kind==6)?3:1;
    if(GT4_U8(c+0xf85e))s=2;
    if(GT4_U8(c+0xf858))s=3;
    return s;
}
/* func_003F88E8: release cars held in mode 2 when this test ends. */
static void release(uint32_t c)
{
    if(!GT4_U8(c+0xf858))return;
    for(uint32_t i=0;(int32_t)i<(int32_t)GT4_U32(c+0xf8a0);i++)
        if((int8_t)GT4_U8(car_at(c,i)+0x56a)==2)gt4_car_take_over(car_at(c,i),5);
}
/* func_003F8980: contact/road/position failure; replay/demo must not
   change the user's work result (+0x88/+0x8c). */
void gt4_license_fail(uint32_t c,uint32_t car,int reason)
{
    uint32_t race=GT4_U32(c),b=car+0x104;
    if((int8_t)GT4_U8(b+0x466)==2)return;
    int how=2;
    if(reason==3 || reason==4 || reason==6){
        how=gt4_license_finish_mode(c,1);
        if(!GT4_U8(c+0xf858) && how==3)how=1;
    }
    release(c);gt4_car_finish_drive(car,how);
    if(GT4_U32(race+0xdc) || GT4_U8(b+0x441) || GT4_U32(race+0xf0))return;
    gt4_race_event(race,10,GT4_U8(b+0x440),0);
    if(!GT4_U8(c+0xcbdc)){GT4_U32(race+0x88)=UINT32_MAX;GT4_U32(race+0x8c)=NONE;}
}
/* func_00354E10 -> 00472AE0: the optional +0xf85d target is fuel
   economy, not velocity. Same constants and operation order as clutch.c. */
static int32_t economy(uint32_t car)
{
    float d=GT4_F32(car+0x5cc),fuel=GT4_F32(car+0x5c8),v;
    uint32_t unit=GT4_U32(0x6244e8);
    if(unit==1)fuel=fuel*K(0x3e874251u);
    else if(unit==2)fuel=fuel*K(0x3e61413bu);
    if(unit==1 || unit==2)d=d*K(0x3f1f136au);
    else if(unit==3){
        if(d<K(0x3a83126eu))return 999;
        v=(fuel*K(0x49742400u))/d;
        return K(0x4479c000u)<=v?999:(int32_t)v;
    }
    if(d<K(0x3c23d70au))return 0;
    if(fuel<K(0x358637bdu))return 999;
    v=(d*K(0x3c23d70au))/fuel;
    return K(0x4479c000u)<v?999:(int32_t)v;
}
/* func_00358000: flag a new best (+0x98), but never a timed-contact
   test, replay, external replay recording, or non-player entry. */
static void best(uint32_t c,uint32_t i,uint32_t elapsed)
{
    uint32_t r=GT4_U32(c);
    if(i || GT4_U32(r+0xdc) || GT4_U8(c+0xcbdc) || GT4_U32(r+0xe0)
       || elapsed==NONE || !(elapsed<GT4_U32(r+0x968)))return;
    if(!GT4_U8(c+0xf85e))GT4_U32(r+0x98)=1;
    if(GT4_U32(r+0x88))gt4_race_event(r,0x16,i,0);
}
void gt4_session_best(uint32_t c,uint32_t i,uint32_t elapsed){best(c,i,elapsed);}
/* func_0035E740: gap against the best recorded attempt. */
static void gap(uint32_t c,uint32_t car,uint32_t t)
{
    uint32_t r=GT4_U32(c);
    if(!GT4_U8(c+0xcbdc) && !GT4_U8(car+0x5b7) && GT4_U32(r+0x968)!=NONE)
        gt4_race_gap(r,GT4_U8(car+0x544),t-GT4_U32(r+0x968));
}
/* func_003F8AB0: elapsed time thresholds include zero at indices 0/4;
   1/2/3 are gold/silver/bronze (003C0B88 -> 00447670/B0/F0). */
void gt4_license_finish(uint32_t c,uint32_t car,uint32_t elapsed)
{
    uint32_t r=GT4_U32(c),b=car+0x104;
    gt4_car_finish_drive(car,gt4_license_finish_mode(c,0));
    if((int8_t)GT4_U8(b+0x466)==2)return;
    release(c);
    if(!GT4_U8(b+0x441)){
        if(GT4_U32(r+0xdc))return;
        if(GT4_U32(r+0xf0))gt4_race_event(r,5,GT4_U8(b+0x440),2);
        else{
            uint32_t state=0;int passed=0;
            if(GT4_U8(c+0xf858)<3){
                for(unsigned i=0;i<5;i++){
                    uint32_t threshold=i>0 && i<4?gt4_career_license_time(gt4_ptr(GT4_U32(r+0x70)),i-1):0;
                    if(threshold<elapsed)continue;
                    state=i+1;passed=1;break;
                }
            }else{
                int position=(int8_t)GT4_U8(b+0x4af);
                if(position<4){state=(uint32_t)(position+1);passed=1;}
                else elapsed=NONE;
            }
            uint32_t target=GT4_U8(c+0xf85d);
            if(passed && target){
                int32_t delta=economy(car)-(int32_t)target;
                if(GT4_U32(0x6244e8)==3)delta=-delta;
                if(delta<0){passed=0;state=0;elapsed=NONE;}
            }
            if(passed){if(GT4_U8(c+0xcbdc))gt4_race_event(r,5,GT4_U8(b+0x440),2);}
            else gt4_race_event(r,10,GT4_U8(b+0x440),0);
            if(!GT4_U8(c+0xcbdc)){
                GT4_U32(r+0x88)=state;GT4_U32(r+0x8c)=elapsed;
                best(c,GT4_U8(b+0x440),elapsed);
            }
        }
    }
    if(elapsed==NONE)return;
    if(!GT4_U8(c+0xf858) || GT4_U8(c+0xf81a)==6)gap(c,car,elapsed);
    gt4_race_lap_event(c,GT4_U8(b+0x440),1,elapsed,GT4_U8(car+0x5b7));
}
/* func_003F91F0: every wheel on surface bit 4, velocity length <= 1 km/h. */
int gt4_license_stop(uint32_t car,float *fraction)
{
    uint32_t n=GT4_U8(gt4_car_layout(car)+LAYOUT_WHEELS),last=0;
    *fraction=0;
    for(uint32_t i=0;i<n;i++)if(GT4_U8(car+0x7eb+i*0x34)&4)last++;
    if(last<n)return 0;
    float x=GT4_F32(car+0x648),y=GT4_F32(car+0x64c),z=GT4_F32(car+0x650);
    float v=sqrtf(fabsf((x*x+y*y)+z*z)),limit=K(0x3e8e38e3u);
    if(limit<v)return 0;
    *fraction=v/limit;return 1;
}
/* func_003FA248/2B0: continuous lead timer, signed halfword +0x1003e.
   advance=0 is the remaining whole seconds, -1 while no timer runs. */
int gt4_license_wait(uint32_t c,int advance)
{
    int limit=GT4_U8(c+0xf85c),n=(int16_t)GT4_U16(c+0x1003e);
    if(advance){
        if(!limit)return 0;
        if(n<limit*60)GT4_U16(c+0x1003e)=(uint16_t)++n;
        return (int16_t)n;
    }
    return n && limit?limit-n/60:-1;
}
/* func_0035FE80 (via 003F6938): collision flag, or contact timer.
   Wall contacts use mass; other cars additionally use 0035FBC8's classes. */
void gt4_license_collision(uint32_t c,uint32_t car,uint32_t kind,uint32_t detail,float force)
{
    static const uint8_t cls[16]={2,1,1,2,3,4,4,3,1,1,1,3,4,4,4,3};
    uint32_t b=car+0x104,threshold=255;float f=0;int hit=0;
    if((int8_t)GT4_U8(b+0x466) || GT4_U8(b+0x467) || GT4_U8(b+0x4b2))return;
    if(kind==0){threshold=GT4_U8(c+0xf861);f=force/GT4_F32(GT4_U32(car+0x10)+0x48);}
    else if(kind==1 && (detail<16?cls[detail]:4)<3){threshold=GT4_U8(c+0xf860);f=force*K(0x42c80000u);}
    else if(kind==2){
        int limit=GT4_U8(c+0xf85e),n=(int16_t)GT4_U16(c+0x1003c);
        if(!limit)hit=1;
        else if(n<limit)GT4_U16(c+0x1003c)=(uint16_t)(n+1);
    }
    if(!threshold || (threshold!=255 && (int32_t)f>=(int32_t)threshold))hit=1;
    if(hit)GT4_U8(b+0x4b3)|=1;
}
/* func_003F8D70: per-frame stop, contact, track and leading checks. */
void gt4_license_evaluate(uint32_t c,uint32_t car)
{
    uint32_t b=car+0x104;
    if(GT4_U8(b+0x4b2))return;
    uint8_t flags=GT4_U8(b+0x4b3);int kind=GT4_U8(c+0xf81a);float fraction;
    if(flags&2){gt4_license_fail(c,car,3);return;}
    if(flags&4){gt4_license_fail(c,car,5);return;}
    if(flags&1){gt4_license_fail(c,car,4);return;}
    if(kind==1 && gt4_license_stop(car,&fraction)){
        uint32_t t=(GT4_U32(c+0xf898)+(uint32_t)(int32_t)(fraction*K(0x42480000u)))/3-GT4_U32(b+0x614);
        /* Replay timed-point buffers (00359C10) are owned by the replay
           recorder; native sessions currently have race+0xe0 = 0. */
        gt4_license_finish(c,car,t);return;
    }
    if(kind==6 && (int8_t)GT4_U8(car+0x5b3)==1)gt4_license_fail(c,car,6);
    int limit=GT4_U8(c+0xf85e);
    uint32_t t=(GT4_U32(c+0xf898)+50)/3-GT4_U32(b+0x614);
    if(limit>0 && limit==(int16_t)GT4_U16(c+0x1003c))gt4_license_finish(c,car,t);
    if(GT4_U8(c+0xf85c) && !GT4_U8(b+0x440)){
        if(GT4_U8(b+0x4af)==1)gt4_license_wait(c,1);
        else GT4_U16(c+0x1003e)=0;
        if(!gt4_license_wait(c,0))gt4_license_finish(c,car,t);
    }
}
