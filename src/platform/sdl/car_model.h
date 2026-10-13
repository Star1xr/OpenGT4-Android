#ifndef OPENGT4_CAR_MODEL_H
#define OPENGT4_CAR_MODEL_H
/* A car's menu model (/car/menu/<model>) with its wheels and contact
   shadow, for the showroom and the race view. */
#include "model.h"
#include "vol.h"
#include "driver_model.h"
typedef struct {unsigned order;char name[128];} car_color;
typedef struct car_model_tag {
    /* hull: reserved auxiliary model-1 volume; not loaded as visible
       bodywork, since it can close an authored open cabin. */
    gt4_model *car,*shadow,*hull;
    /* cast: model 1 for the race's shadow alone, which lays it flat on the
       ground along the sun (func_004524B8). */
    gt4_model *cast;char code[96],model[32];car_color colors[128];int count,choice;
    /* The wheels again as their own models, about their hubs (front then
       rear, the model's +x side first), for the race view to turn them;
       car's first body_count triangles are the body without them. */
    gt4_model *wheel[4];float hub[4][3],wheel_limit[2],tire_size[4][3];int body_count;
    /* Brake vertices about the hub, texture indices into car's texture set.
       Steer/travel with the wheel; the calipers do not spin with the tire. */
    gt4_model *brake[4];
    gt4_model *steering[GT4_MODEL_STEERING_MAX]; /* local meshes; parent textures */
    /* The race cameras' points on the car (CAR4 header +0x80 count, +0x84
       offset; 32 bytes each: x, y, z, then values the camera reads): where
       the car's origin sits in the camera's view (y up, looking down -z);
       entry 1 the chase camera's, (0, -1.7, -6) */
    float cam_point[32][8];int cam_points;
    gt4_driver_car driver;
    uint8_t *driver_data;size_t driver_size; /* builtin MDLS only */
    const char *driver_path; /* shared selection; owned static string */
    float body_sine,body_cosine,body_lift;
    int race_open;unsigned exhaust_count;
    float exhaust[8][6];unsigned exhaust_points;   /* the outlets (gt4_car_exhaust_points) */
} car_model;
/* The SpecDB tables the load reads, kept open by a loader thread. */
typedef struct car_model_tables car_model_tables;
car_model_tables *car_model_tables_open(gt4_vol *v);
void car_model_tables_close(car_model_tables *t);
/* CARINFO geometry only, without decoding or uploading a second car mesh. */
int car_model_race_driver(gt4_vol *v,const car_model_tables *t,const char *code,int cars,gt4_driver_car *out);
/* code: GENERIC_CAR label; choice: colour index, wrapped to the car's count. */
car_model *car_model_load(gt4_vol *v,const car_model_tables *t,const char *code,int choice);
/* 00387FB0 / 003B5358 / 004541E0: Arcade A-spec world roof selection.
   The showroom keeps its separate menu model and no race pilot. */
car_model *car_model_load_race(gt4_vol *v,const car_model_tables *t,const char *code,int choice,int cars,unsigned entry);
/* the same with an owned car's wheel and wing labels (wheel/lod, wing/lod) */
car_model *car_model_load_race_parts(gt4_vol *v,const car_model_tables *t,const char *code,int choice,int cars,unsigned entry,const char *wheel,const char *wing);
car_model *car_model_load_wheel(gt4_vol *v,const car_model_tables *t,const char *code,int choice,const char *wheel);
car_model *car_model_load_parts(gt4_vol *v,const car_model_tables *t,const char *code,int choice,const char *wheel,const char *wing);
car_model *car_model_load_film_parts(gt4_vol *v,const car_model_tables *t,const char *code,int choice,int cars,unsigned entry,const char *wheel,const char *wing,const float recorded[4][3]);
void car_model_free(car_model *a);
#endif
