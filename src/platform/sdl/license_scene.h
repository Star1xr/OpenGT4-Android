#ifndef OPENGT4_LICENSE_SCENE_H
#define OPENGT4_LICENSE_SCENE_H
#include "license_motion.h"
#include "render3d.h"
#include "physics/car.h"
#include "course.h"
#include "race_pit_crew.h"
typedef struct license_scene license_scene;
license_scene *license_scene_open(gt4_license_motion *motion);
license_scene *license_scene_open_pit(gt4_license_motion *motion);
void license_scene_draw_pit(license_scene *s,const r3d_view *view,const gt4_pit_crew *crew,uint32_t course,const gt4_course *asset,int watched);
void license_scene_close(license_scene *s);
/* Called after the course draw; retains its depth and camera viewport. */
void license_scene_draw(license_scene *s,const r3d_view *view,const float placement[3][4],uint32_t course,const gt4_course *asset,const gt4_ray_hit surfaces[3]);
/* CPU light adapter also exercised against 00409B68 by the EE oracle. */
void license_scene_lights(const uint8_t *env,uint32_t mesh,const gt4_ray_hit *surface,const float pos[3],r3d_lights *out);
#endif
