#ifndef OPENGT4_COURSE_OBJECTS_H
#define OPENGT4_COURSE_OBJECTS_H
#include "course_gadget.h"
/* 003E5040 after the world step (0033C538). Native state, no EE runtime. */
void gt4_course_objects_init(gt4_course_gadgets *objects,uint32_t course);
void gt4_course_objects_step(gt4_course_gadgets *objects,uint32_t cond,float dt);
/* Scalar/VU boundaries exposed for the original-code oracle. */
float gt4_course_object_contact(gt4_course_gadget *object,unsigned point,
                               const float planes[5][4],const float position[3],
                               const float velocity[3],float yaw_rate);
void gt4_course_object_integrate(gt4_course_gadget *object,float dt);
int gt4_course_object_ground(gt4_course_gadget *object,unsigned point,const float normal[3]);
int gt4_course_object_wall(gt4_course_gadget *object,uint32_t wall,
                          const float previous[3],const float points[8][3]);
#endif
