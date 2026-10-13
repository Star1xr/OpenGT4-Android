#include "sound_sequence.h"
#include <fenv.h>

float gt4_se_rate(float tempo,float resolution,float frequency){
    int saved=fegetround();fesetround(FE_TOWARDZERO);
    float period=60000000.f/tempo;
    float rate=1000000.f/period;rate=rate*resolution;rate=rate/frequency;
    fesetround(saved);return rate;
}
float gt4_se_clock(float elapsed,float rate){
    int saved=fegetround();fesetround(FE_TOWARDZERO);
    float next=elapsed+rate;fesetround(saved);return next;
}
int gt4_se_due(float elapsed,float due){return elapsed>=due;}
size_t gt4_se_sample_time(unsigned ticks){return ticks>8?(size_t)((ticks-1)/8)*800:0;}
void gt4_se_ramp_start(gt4_se_ramp *r,int milliseconds,int target,int current,float frequency){
    int saved=fegetround();fesetround(FE_TOWARDZERO);
    float delta=(float)(target-current),duration=frequency*(float)milliseconds;
    r->done=0;r->target=r->target+delta;
    float rate=1000.f/duration;r->step=delta*rate;
    fesetround(saved);
}
int gt4_se_ramp_step(gt4_se_ramp *r,int32_t *value){
    if(r->done)return 0;
    int saved=fegetround();fesetround(FE_TOWARDZERO);
    int before=(int)r->fraction;r->fraction=r->fraction+r->step;
    int after=(int)r->fraction,changed=after!=before;
    if(changed){
        *value+=after;r->fraction=r->fraction-(float)after;
        int target=(int)r->target;
        if((r->step>0 && *value>=target) || (r->step<0 && *value<=target)){
            r->step=0;*value=target;r->done=1;
        }
    }
    fesetround(saved);return changed;
}
