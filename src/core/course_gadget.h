#ifndef OPENGT4_COURSE_GADGET_H
#define OPENGT4_COURSE_GADGET_H
#include "course.h"
#include "model.h"
typedef struct {
    unsigned type;
    float pos[3],angles[3],rot[9],corner[3],sun,shade;
    int fixed;
    const uint8_t *descriptor; /* authored 0xA0 bytes; owned by collection */
    float velocity[3],angular[3],normal[3],plane,impact;
    uint8_t support[8],age; /* 003E2710 +45/+58, 003E2B28 */
    int active;
    int32_t ground_id;
} gt4_course_gadget;
typedef struct {
    gt4_model **model;
    uint8_t *descriptor;
    unsigned types,sections,count;
    gt4_course_gadget *instance;
    unsigned *first,*length; /* slot 0 global; section i at slot i+1 */
} gt4_course_gadgets;
/* COURSE_DATA +BC GDGT and RNW4 global/section placements (00395B60).
   Decode once; caller owns the models and instances until close. */
gt4_course_gadgets *gt4_course_gadgets_open(const gt4_course *course);
/* Same placements/physics data without decoding any render mesh. */
gt4_course_gadgets *gt4_course_gadgets_open_state(const gt4_course *course);
/* 003E20A8: authored accelerating blink of a struck removable prop. */
int gt4_course_gadget_visible(const gt4_course_gadget *gadget);
void gt4_course_gadgets_close(gt4_course_gadgets *g);
/* 003E24F0, row-major adapter for the renderer. Angles in radians. */
void gt4_course_gadget_rotation(const float angles[3],float rot[9]);
/* Initial ground placement only, 003E2710 +E8..240; no car physics.
   Plane and light fractions come from the original RNW4 ray query. */
void gt4_course_gadget_surface(gt4_course_gadget *g,const float normal[3],float plane,float sun,float shade);
#endif
