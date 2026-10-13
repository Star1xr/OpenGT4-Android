#ifndef OPENGT4_LICENSE_CONCOURSE_H
#define OPENGT4_LICENSE_CONCOURSE_H

/* Native LicenseConcourse director, NTSC-U v2.00 00408F90/00409138/
   00409258. Rendering and the course's placements are separate boundaries. */
typedef struct {
    int outcome,enabled,placement,placement_ready;
    float elapsed;
    int phase,previous,camera_cycle,camera;
    unsigned changed;
} gt4_license_concourse;

enum {GT4_CONCOURSE_ACTORS=1,GT4_CONCOURSE_CAMERA=2,GT4_CONCOURSE_PLACE=4};
void gt4_license_concourse_begin(gt4_license_concourse *c,int placements,unsigned camera_cycle);
void gt4_license_concourse_finish(gt4_license_concourse *c,int passed);
void gt4_license_concourse_step(gt4_license_concourse *c,float dt);
const char *gt4_license_concourse_actor(const gt4_license_concourse *c,unsigned actor);
const char *gt4_license_concourse_camera(const gt4_license_concourse *c);
int gt4_license_concourse_has_camera(const gt4_license_concourse *c);
int gt4_license_concourse_cut(const gt4_license_concourse *c);
/* 00408B08: authored car seat/shift plus native wheel centres, local X/Z.
   out: local x,y,z,heading for master, staff1, staff2. Ground is separate. */
void gt4_license_concourse_people(float seat_x,float seat_z,float shift_x,const float wheels[2][2],float out[3][4]);
#endif
