#ifndef OPENGT4_RACE_PIT_WALK_H
#define OPENGT4_RACE_PIT_WALK_H
#include <stdint.h>
/* 003D36D0 / 003D3E80: native presentation state, never service physics.
   The target is an anchor index instead of the original self pointer. */
typedef struct {uint32_t wheel,side;float point[2];} gt4_pit_walk_anchor;
typedef struct {
    uint32_t role,state,target;
    gt4_pit_walk_anchor anchor[2];
    uint32_t enabled,model;
    float waiting[2],position[2],velocity[2],light[4];
    float yaw,unused,target_yaw,phase,rate,timer;
} gt4_pit_walker;
struct gt4_pit_crew_tag;
void gt4_pit_walk_init(gt4_pit_walker actors[4],uint32_t car,int variant,int stage,int spacing,uint32_t *random);
void gt4_pit_walk_step(struct gt4_pit_crew_tag *crews,unsigned cars,unsigned entry,int commands,float dt,uint32_t *random);
/* 003D4AA8: colour modulation authored by the two endpoints. */
float gt4_pit_walk_light(const gt4_pit_walker *actor);
int gt4_pit_walk_sample(float phase,float authored_scale,unsigned *branch,float *tween);
#endif
