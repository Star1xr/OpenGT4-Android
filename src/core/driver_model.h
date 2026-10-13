#ifndef OPENGT4_DRIVER_MODEL_H
#define OPENGT4_DRIVER_MODEL_H
#include <stddef.h>
#include <stdint.h>

enum { GT4_DRIVER_BONES=20 };
typedef struct {
    uint16_t flags;
    uint8_t type,crew;
    size_t builtin_offset,geometry_offset;
    /* CARINFO geometry: wheel centre/normal/radius, feet, seat, chair angle,
       shift centre/normal/length. Values are authored in car coordinates. */
    float wheel[3],wheel_normal[3],wheel_radius;
    float feet[2][3],seat[3],chair_angle;
    float shift[3],shift_normal[3],shift_length;
    int16_t shift_type;
    float handbrake[3],handbrake_length;
    /* 003CE288/003CE4A0: HumanGeom+AC fuel position, +B8 normal. */
    float fuel_port[3],fuel_normal[3];int fuel_geometry;
} gt4_driver_car;
enum gt4_driver_source { GT4_DRIVER_NONE,GT4_DRIVER_SHARED,GT4_DRIVER_BUILTIN };
typedef struct {
    enum gt4_driver_source source;
    const char *path;             /* shared file on the user's disc */
    int crew_enabled;
} gt4_driver_selection;
/* Published HumanModel bone order (003B0D18), distinct from MDLS slots. */
typedef struct {
    uint8_t slot,present;
    float inverse_bind[16];
    int model_first,model_count;  /* optional rigid body part */
} gt4_driver_bone;
typedef struct {
    gt4_driver_bone bone[GT4_DRIVER_BONES];
    float dimensions[16],lod[2];
    int skin_first,skin_count,head2_first,head2_count;
    unsigned slots;
} gt4_driver_rig;

/* 00390898: CARINFO +38 exhaust count, before relocation. */
unsigned gt4_car_exhaust_count(const uint8_t *data,size_t size);
/* The outlets themselves: position x, y, z and direction x, y, z each */
unsigned gt4_car_exhaust_points(const uint8_t *data,size_t size,float out[][6],unsigned max);
int gt4_driver_car_read(const uint8_t *data,size_t size,gt4_driver_car *out);
/* 003B5438: builtin first; otherwise CARINFO type and entry/variant select
   the shared pilot. variant < 0 uses entry % 3; callers own the global cycle. */
gt4_driver_selection gt4_driver_select(const gt4_driver_car *car,unsigned entry,int variant);
/* 00459178: 0 absent, 1/2 reduced builtin, 3 full animated human. */
int gt4_driver_detail(unsigned type);
/* 003B0C80/003B0D18: bounded MDLS bind map, symbols and body dimensions. */
int gt4_driver_rig_read(const uint8_t *data,size_t size,gt4_driver_rig *out);
/* 003B1248: published bone poses become dense MDLS palette slots, each
   pose * inverse-bind. Column-major matrices; no pose generation here. */
int gt4_driver_palette(const gt4_driver_rig *rig,const float pose[GT4_DRIVER_BONES][16],
                       float out[GT4_DRIVER_BONES][16]);
#endif
