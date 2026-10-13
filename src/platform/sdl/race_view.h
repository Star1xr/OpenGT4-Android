#ifndef OPENGT4_RACE_VIEW_H
#define OPENGT4_RACE_VIEW_H
/* Race view: a course and a car on it, with a chase camera, drawn by the
   GPU renderer. Separate from the menus' car showroom (showroom.c). */
#include "course.h"
#include "driver_model.h"
#include "course_gadget.h"
#include "driver_pose.h"
#include "photo_mode.h"
typedef struct race_view race_view;
race_view *race_view_open(const char *disc);
void race_view_close(race_view *v);
/* Loads the course (/crs/<name>) and the car (GENERIC_CAR label, colour
   index) on a thread with its own disc handle; the main thread polls:
   0 loading, 1 installed (or discarded with cancel), -1 failed. */
int race_view_request(race_view *v,const char *course,const char *car,int color);
/* the player's car's fitted wheel and wing labels for the next request */
void race_view_set_parts(race_view *v,const char *wheel,const char *wing);
int race_view_set_film_tires(race_view *v,unsigned entry,const float tires[4][3]);
int race_view_set_film_parts(race_view *v,unsigned entry,const char *wheel,const char *wing);
/* Static Photo Travel scene: full menu car, no race grid or solver. */
int race_view_request_photo(race_view *v,const char *course,const char *car,int color);
/* Frozen imported film, either a race course or a Photo Travel scene.
   Preserve the opponent selection installed before this request. */
int race_view_request_film(race_view *v,const char *course,const char *car,int color);
int race_view_photo_environment(race_view *,const uint8_t *,size_t);
int race_view_draw_photo(race_view *v,float x,float y,float w,float h);
/* Resident scene passes: a rear mirror or a lens sample, no simulation. */
int race_view_draw_mirror(race_view *,int entry,float x,float y,float w,float h);
/* Record once per physical tick; capture/sample/restore renderer state only. */
void race_view_photo_record(race_view *);
void race_view_photo_begin(race_view *);
void race_view_photo_sample(race_view *,float time,float eye[3],float rows[9]);
void race_view_photo_end(race_view *);
void race_view_photo_jitter(race_view *,const float jitter[2]);
int race_view_photo_pose(const race_view *,float pos[3],float rot[9]);
int race_view_draw_photo_map(race_view *v,const gt4_photo_mode *p,float x,float y,float w,float h);
int race_view_poll(race_view *v,int cancel);
/* Drops the installed course and car. */
void race_view_clear(race_view *v);
/* Draws the 640x448 view; 1 when the course and the car were drawn. */
int race_view_draw(race_view *v);
/* Arcade's SceneViewFace: 640x480, empty course, no car reflection work. */
int race_view_draw_pregrid(race_view *v);
/* License SceneViewFace retains the car in its 640x480 scene. */
int race_view_draw_license_menu(race_view *v);
void race_view_camera(race_view *v,float orbit,float zoom);
/* The player's car model's camera point i (car_model.cam_point), 0 when
   it has none */
int race_view_camera_point(const race_view *v,int i,float out[8]);
/* Whether the player's car is drawn (not from the view on it) */
void race_view_show_car(race_view *v,int on);
int race_view_car_visible(const race_view *v);
/* Resident objects selected by RaceCourse +B0 (00395B60). The borrowed
   instances may receive their original RNW4 surface once after loading. */
gt4_course_gadgets *race_view_gadgets(race_view *v,int section);
/* a colour filter on the course (r3d_view.grade), NULL for none */
void race_view_set_grade(race_view *v,const float grade[6]);
/* 003B6B78: authored exhaust outlets of the resident car model. */
unsigned race_view_exhaust_count(const race_view *v,int entry);
/* The backfire's flames of car `entry` (race_audio_afterfire), drawn at its outlets */
void race_view_set_afterfire(race_view *v,unsigned entry,const float flames[4][3],int on);
/* Borrowed resident mesh for inspection; invalidated by poll/clear. */
struct car_model_tag;
const struct car_model_tag *race_view_car_model(const race_view *v,int entry);
const gt4_driver_car *race_view_car_geometry(const race_view *v,int entry);
/* The loaded course (its file's bytes and runway), NULL before one */
const gt4_course *race_view_course(const race_view *v);
/* The game's camera: eye in the render frame, rows the world-to-view
   rotation (right, up, back), the projection's x and y scales (the view's
   half width and height at unit depth: 1.73205, 2.25378 for the chase
   camera) and its near plane; eye NULL goes back to the view's own chase
   camera */
void race_view_set_camera(race_view *v,const float eye[3],const float rows[9],float sx,float sy,float near_clip);
/* Native inspection camera: copy the active view, including projection. */
int race_view_get_camera(const race_view *v,float eye[3],float rows[9],float projection[3]);
/* The car at pos (render frame, y up) with rot (row-major, world = rot *
   model; car models face -z), the camera following it. pos NULL puts it
   back on grid position 0. */
void race_view_set_pose(race_view *v,const float pos[3],const float rot[9]);
/* Each wheel's steer (about the model's up axis), rotation (about its
   axle) and lift of its hub, in radians and units, in the order front,
   rear, the model's +x side first. NULL draws them as modelled. */
void race_view_set_wheels(race_view *v,const float wheels[4][3]);
/* Recorded current/previous wheel radians in the same slots as above;
   original interpolation and course exposure select each rim's blur bank. */
void race_view_set_wheel_motion(race_view *v,unsigned entry,const float current[4],const float previous[4],float interpolation);
/* Borrowed authored service state; meshes remain resident and shared. */
struct gt4_pit_crew_tag;
void race_view_set_pit_crew(race_view *v,unsigned entry,const struct gt4_pit_crew_tag *crew);
/* Original 64-channel human frame for car entry (0 player, then opponents).
   Updates native IK and a small GPU palette; no vertex buffers are rebuilt. */
int race_view_set_driver_frame(race_view *v,unsigned entry,const float channels[64]);
void race_view_set_wheel_snapshot(race_view *,unsigned entry,const float wheels[4][8]);
void race_view_set_driver_grasp(race_view *v,unsigned entry,const float grasp[2]);
int race_view_set_driver_snapshot(race_view *v,unsigned entry,const gt4_driver_skeleton *s);
/* Advances cached presentation state once per simulation step. IK/palette
   evaluation is deferred until drawing, even when several steps catch up. */
int race_view_step_driver(race_view *v,unsigned entry,const int transmission[4],const gt4_driver_animation_input *input);
/* The other cars (up to 7 GENERIC_CAR labels), loaded with the next
   request; 0 when too many or a load is running. */
int race_view_set_opponents(race_view *v,const char *const *cars,const int *colors,int n);
/* Other car i's pose and wheels as for the player's car; pos NULL hides
   it. */
void race_view_set_opponent(race_view *v,int i,const float pos[3],const float rot[9],const float wheels[4][3]);
/* opponent i drawn see-through at alpha (the Time Trial's ghost); 0 opaque */
void race_view_set_ghost(race_view *v,int i,float alpha);
/* Where a car's shadow lies and how the ground under it is lit: entry 0
   the player's car, 1.. the others (gt4_race_run_shadow). NULL pos: the
   shadow is laid on the course's surface under the car instead. */
void race_view_set_shadow(race_view *v,int entry,const float pos[3],const float rot[9],float sun,float level);
/* Car i's icon (0 the player's, then the others), its side view in the box
   (x, y, w, h) of the current 2D view */
void race_view_draw_icon(race_view *v,int i,float x,float y,float w,float h);
/* the icon maker's perspective picture (B-Spec's current car, 96x64) */
void race_view_draw_icon_front(race_view *v,int i,float x,float y,float w,float h);
/* the tyres' smoke drawn over the scene (borrowed; NULL none) */
struct race_smoke;
void race_view_set_smoke(race_view *v,const struct race_smoke *s);
/* the leaves thrown up by the wheels (borrowed; NULL none) */
struct race_leaves;
void race_view_set_leaves(race_view *v,const struct race_leaves *l);
/* Shared resident scene, a watched entry and a clipped race viewport. */
int race_view_draw_player(race_view *v,int entry,int show,float y,float h);
int race_view_camera_point_entry(const race_view *v,int entry,int point,float out[8]);
#endif
