#ifndef OPENGT4_RACE_LIGHTS_H
#define OPENGT4_RACE_LIGHTS_H
#include "course.h"
#include "render3d.h"
/* Immutable RNW4 / Env0 car lighting, 0038E7F8 -> 0038EFF8.
   triangle <0 represents an unavailable surface. No solver state access. */
int race_lights_sample(const gt4_course *,int triangle,const float pos[3],float sun,float level,r3d_lights *);
int race_lights_sample_environment(const gt4_course *,int triangle,const float pos[3],float sun,float level,const uint8_t *environment,size_t environment_size,r3d_lights *);
#endif
