#include "license_concourse.h"
#include <fenv.h>
#include <string.h>
#include <math.h>

void gt4_license_concourse_begin(gt4_license_concourse *c,int placements,unsigned camera_cycle)
{
    /* 00408F90 +0x2c..0xa8. A course with two authored places initially
       uses place 1. On completion 00409258 +0xbc resets to place 0. */
    memset(c,0,sizeof *c);c->enabled=placements>0;c->placement=placements>=2;
    c->placement_ready=c->placement;
    c->camera_cycle=(int)(camera_cycle%3);c->camera=-1;
    c->changed=GT4_CONCOURSE_ACTORS|GT4_CONCOURSE_PLACE;
}
void gt4_license_concourse_finish(gt4_license_concourse *c,int passed)
{
    /* 00409138: +14 is 1 success / 2 failure, only when +18 enabled. */
    if(c->enabled)c->outcome=passed?1:2;
}
void gt4_license_concourse_step(gt4_license_concourse *c,float dt)
{
    c->changed=0;if(!c->enabled || !c->outcome)return;
    fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
    /* 00409258 +0x2c stores the previous phase before the switch.
       Each transition discards excess time; only one phase per call. */
    c->previous=c->phase;
    if(c->phase<=2){
        c->elapsed+=dt;
        float end=c->phase==2?1.f:2.f; /* 00409878 / 00409888 */
        if(c->elapsed>=end){
            c->elapsed=0;c->phase++;
            if(c->phase==2){
                c->placement=0;c->placement_ready=1;c->camera=c->camera_cycle;
                c->camera_cycle=(c->camera_cycle+1)%3;
                c->changed=GT4_CONCOURSE_ACTORS|GT4_CONCOURSE_CAMERA|GT4_CONCOURSE_PLACE;
            }
        }
    }
    fesetenv(&env);
}
const char *gt4_license_concourse_actor(const gt4_license_concourse *c,unsigned actor)
{
    /* 00409078..004090b0 / 00409338..004093c8, strings 006a4378..4438. */
    static const char *const names[3][3]={
        {"license_stand_master","license_stand_staff_1","license_stand_staff_2"},
        {"license_success_master","license_success_staff_1","license_success_staff_2"},
        {"license_failed_master","license_failed_staff_1","license_failed_staff_2"}};
    if(!c || !c->enabled || actor>=3)return NULL;
    return names[c->phase>=2?c->outcome:0][actor];
}
const char *gt4_license_concourse_camera(const gt4_license_concourse *c)
{
    /* 004093d0..00409434, strings 006a4450/4468/4480. */
    static const char *const names[]={"license_cam01_seq","license_cam02_seq","license_cam03_seq"};
    return c && c->camera>=0 && c->camera<3?names[c->camera]:NULL;
}
int gt4_license_concourse_has_camera(const gt4_license_concourse *c)
{
    /* 004096F8 +0x18..0x28 returns 1 for phase >=2 (delay slot matters). */
    return c && c->enabled && c->camera>=0 && c->phase>=2;
}
int gt4_license_concourse_cut(const gt4_license_concourse *c)
{
    /* 00409828 / alias 00409860: true only on entry into phase 2. */
    return c && c->enabled && c->phase==2 && c->previous!=2;
}
void gt4_license_concourse_people(float x,float z,float shift,const float wheels[2][2],float out[3][4])
{
    /* 00408B08 +0x70..0x250: choose side from seat vs shift, master
       half a metre outside the front wheel, staff three quarters. */
    fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);int side=x<shift?0:1;float sign=side?1.f:-1.f;
    float turn;unsigned bits=0x3fc8f5c2;memcpy(&turn,&bits,4);
    memset(out,0,3*4*sizeof(float));out[0][0]=wheels[side][0]+sign*.5f;out[0][2]=z;out[0][3]=-sign*turn;
    out[1][0]=wheels[side][0]+sign*.75f;out[1][2]=z+1;
    out[2][0]=wheels[1-side][0]-sign*.75f;out[2][2]=wheels[1-side][1];
    /* 0057D118 input boundary; no fitted orientation. */
    for(unsigned i=1;i<3;i++)out[i][3]=atan2f(-(out[i][0]-x),-(out[i][2]-z));fesetenv(&env);
}
