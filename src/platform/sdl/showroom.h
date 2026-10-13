#ifndef OPENGT4_SHOWROOM_H
#define OPENGT4_SHOWROOM_H
typedef struct showroom showroom;
showroom *showroom_open(const char *disc);
void showroom_close(showroom *s);
void showroom_set_wing(showroom *s,const char *wing);
void showroom_set_wheel(showroom *s,const char *wheel);
int showroom_draw(showroom *s,const char *code,float dt);
/* Request/load only: 1 ready, 0 pending, -1 failed. Does not draw or advance pose. */
int showroom_prepare(showroom *s,const char *code);
/* A normal CarFace, or an explicit script pose: just
   the car and its contact shadow in the widget's rectangle of the 640x480
   UI space, over what the page drew, with the widget's own camera
   (camera_trans, camera_rotate in degrees, view_angle) and lights. */
#include "render3d.h"
#include "car_service.h"
typedef struct { int look_valid;float look[9]; float x,y,w,h; float eye[3],rotate[3],view_angle; r3d_lights lights; int pose_valid; float car_angle,car_position[3];
    const uint8_t *crew_data;size_t crew_size;uint64_t crew_serial;const struct gt4_mot1 *crew_motion;float crew_time;int crew_loop;
    const gt4_service_model *models;int model_count;
    const gt4_service_motion *motions;int motion_count;
    float service_geometry[12],additional_scale;int service_geometry_valid;
    int service_pass;float service_opacity,service_additional_opacity,service_shadow;
    float service_lift; /* CarFace Mot1 "lift", 001570A4..001570D8 */
} showroom_face;
/* 00156F18 +0xac..0x340: camera-driven fade, lift, light and shadowOpacity. */
void showroom_service_channels(showroom_face *face,const struct gt4_mot1 *motion,float time);
int showroom_draw_face(showroom *s,const char *code,float dt,const showroom_face *face);
/* Hides the current car until the next draw finds it loaded again, which
   then fades it in: used when the showroom page is (re)entered. */
void showroom_hide(showroom *s);
void showroom_color(showroom *s,int delta);
/* Shows paint `index` of the current car (CarRoot's colour chips). */
void showroom_set_color(showroom *s,int index);
/* Request a model and its paint together; a second CarFace owns another s. */
void showroom_set_selection(showroom *s,const char *code,int index);
const char *showroom_caption(showroom *s);
/* Colour index of the car shown (for the race view to load it the same). */
int showroom_color_index(showroom *s);
/* Rotation angle of the presented car (radians); captures reset it. */
void showroom_set_angle(showroom *s,float angle);
#endif
