#ifndef OPENGT4_RACE_PIT_CREW_H
#define OPENGT4_RACE_PIT_CREW_H
#include "license_motion.h"
#include "driver_model.h"
#include "race_pit_walk.h"
typedef struct gt4_pit_crew_tag {
    gt4_license_motion *motion;
    int stage,active[6],requested[6],disabled[6],tire_end[4],refuel_seen,refuel_flag,jack_seen,nozzle_flag;
    int placed,variant;float root[4],place[6][4],approach[6],fuel_height;
    int fixed,frozen;uint32_t car;float old_root[3],old_euler[3],old_hub[4][3];
    float cable[4][4],tire_scale[4],blend[4],fade[4][6],rotation[4][2],camber[4];
    float box[4],camera_origin[4],camera_hub[4][3],seat[3],camera_begin;
    int camera_control,camera_choice,camera_atomic;
    gt4_pit_walker walking[4];int walk_spacing;
} gt4_pit_crew;
typedef struct {unsigned count;uint8_t effect[18];int refueling;} gt4_pit_crew_sound;
/* Read-only presentation adapters. Never apply service/tyre writes. */
int gt4_pit_stage_fields(unsigned lane,unsigned flags,unsigned orders,unsigned timer);
int gt4_pit_stage(uint32_t car);
/* 003D8C48: no monitor pad dispatch during pit/transition ownership. */
int gt4_pit_monitor_input(int pit_sequence,int transition);
int gt4_pit_role_control(unsigned role,int stage,int refueling,int ready);
int gt4_pit_refueling(unsigned orders,unsigned timer);
/* 003D3208/3228/3248: services unavailable for this session. */
void gt4_pit_crew_service_enable(gt4_pit_crew *crew,int tires,int fuel);
/* 004078D0 -> 0043F3A8: GT garage ownership; current tire is kept by UI. */
void gt4_pit_owned_tires(const uint8_t body[0x4b8],unsigned masks[2]);
void gt4_pit_crew_init(gt4_pit_crew *crew,gt4_license_motion *motion);
void gt4_pit_crew_step(gt4_pit_crew *crew,uint32_t car,float dt,gt4_pit_crew_sound *sound);
/* 003CD620: wheel-side root and heading, no physics writes. */
void gt4_pit_wheel_place(uint32_t car,unsigned wheel,int right,float place[4]);
void gt4_pit_crew_place(gt4_pit_crew *crew,uint32_t car,unsigned entry,const gt4_driver_car *geometry,int variant,uint32_t *random);
/* 003CF938: x/vx/z/vz authored cable spring, purely visual state. */
void gt4_pit_cable_step(float state[4],float dt,float ax,float az);
/* 003D1488: unscaled wheel model transform; old/new tire markers. */
int gt4_pit_tire_matrix(const gt4_pit_crew *crew,unsigned role,int replacement,const float marker[16],float matrix[16]);
/* 004595C8/00459610 -> 003D6FE0/003D6F40: live model parent frame.
   003D72A0: mounted brake/wheel centre, independent of initial travel. */
int gt4_pit_car_matrix(const gt4_pit_crew *crew,float matrix[16]);
int gt4_pit_mounted_matrix(const gt4_pit_crew *crew,unsigned role,float matrix[16]);
void gt4_pit_tire_rotation(const gt4_pit_crew *crew,unsigned role,int replacement,float matrix[16]);
/* 003D2C48: authored prop or live mounted fallback according to camera LOD. */
int gt4_pit_tire_draw_matrix(const gt4_pit_crew *crew,unsigned role,int replacement,int actor_model,const float marker[16],float matrix[16]);
/* 003CBB28 / 003CBD00 / 003CB2C0, presentation only. */
void gt4_pit_camera_step(gt4_pit_crew *crew,float dt);
/* 003CC010: choose only on interval change, freeze origin on atomic change. */
void gt4_pit_camera_pick(gt4_pit_crew *crew,int random);
int gt4_pit_camera(const gt4_pit_crew *crew,float look[9],float *fov);
int gt4_pit_camera_point(const gt4_pit_crew *crew,const float source[3],int coordinate,int direction,float out[3]);
/* 003CC498: camera-authored model selection; -1 means the posed skin. */
int gt4_pit_actor_model(const gt4_pit_crew *crew,unsigned role,unsigned cars);
const char *gt4_pit_actor_model_name(int model);
float gt4_pit_actor_model_heading(int model);
void gt4_pit_actor_cable_end(int model,float point[3]);
/* 003CF7C8: which carried wheel is drawn, separate from the mounted one. */
int gt4_pit_tire_visible(int stage,int replacement);
/* 003A59D8: RaceRefuelMeter rectangles and right-aligned label. */
typedef struct {
    float rect[3][4],text[2];
    uint32_t color;
    int fuel,capacity;
} gt4_pit_refuel_layout;
void gt4_pit_refuel_meter(float fuel,float capacity,gt4_pit_refuel_layout *layout);
/* 003CA698 / 003CAAC8 / 003CA168: two authored cable strips, 24 samples. */
void gt4_pit_cable_vertices(const gt4_pit_crew *crew,unsigned role,const float end[3],float vertices[276][3]);
#endif
