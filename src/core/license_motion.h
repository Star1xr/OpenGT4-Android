#ifndef OPENGT4_LICENSE_MOTION_H
#define OPENGT4_LICENSE_MOTION_H
#include <stddef.h>
#include <stdint.h>
typedef struct gt4_license_motion gt4_license_motion;
/* /pit/license_001, relocated on PS2 by 003E7360. Copies the package;
   immutable models/motions live until close. Slots 0..2 actors, 3 camera. */
gt4_license_motion *gt4_license_motion_open(const uint8_t *data,size_t size);
/* Same container, /pit/pit_001: slots 0..5 crew, 6 camera. */
gt4_license_motion *gt4_license_motion_open_pit(const uint8_t *data,size_t size);
/* Independent clocks borrowing the owner's immutable package. Close clones
   before owner; no repeated package copies or mesh/texture decoding. */
gt4_license_motion *gt4_license_motion_clone(const gt4_license_motion *owner);
/* Course +D4 (0037D734). Borrows immutable data until close. */
gt4_license_motion *gt4_license_motion_open_camera(const uint8_t *data,size_t size);
void gt4_license_motion_close(gt4_license_motion *m);
const uint8_t *gt4_license_motion_model(const gt4_license_motion *m,unsigned part,size_t *size);
int gt4_license_motion_select(gt4_license_motion *m,unsigned slot,const char *name);
int gt4_license_motion_step(gt4_license_motion *m,unsigned slot,float dt);
int gt4_license_motion_step_control(gt4_license_motion *m,unsigned slot,float dt,int control);
/* 00429AD8: first authored entry at/above control, for stage jumps. */
int gt4_license_motion_seek_control(gt4_license_motion *m,unsigned slot,int control);
/* 00429D20 / 0042A428: label in an entry preceding the current entry. */
int gt4_license_motion_passed(const gt4_license_motion *m,unsigned slot,const char *label);
/* 00429E10: current authored sequence interval. */
int gt4_license_motion_interval(const gt4_license_motion *m,unsigned slot,const char **label,float *begin,float *end);
/* 00429E10: number of alternatives in the current authored interval. */
unsigned gt4_license_motion_targets(const gt4_license_motion *m,unsigned slot);
int gt4_license_motion_set_time(gt4_license_motion *m,unsigned slot,float time);
float gt4_license_motion_time(const gt4_license_motion *m,unsigned slot);
/* Atomic selection passed to 0042B440 by 0042C348. */
int gt4_license_motion_sample(const gt4_license_motion *m,unsigned slot,unsigned *group,unsigned *atomic,float *time);
/* 0042C348 selection with the authored multi-target blend coordinate. */
int gt4_license_motion_sample_blend(const gt4_license_motion *m,unsigned slot,float blend,unsigned *group,unsigned atomics[2],float *time,float *factor);
/* 00429740 / 0042C4B8: event marks over the old/new clock interval. */
float gt4_license_motion_mark(const gt4_license_motion *m,unsigned slot,const char *name,float old_time,float new_time,float dt);
/* One scalar channel of the current packed motion, for evidence probes. */
int gt4_license_motion_curve(const gt4_license_motion *m,unsigned slot,unsigned index,float *value);
/* Column-major accumulated frames at the authored drawing markers. */
int gt4_license_motion_frames(const gt4_license_motion *m,unsigned slot,const char *const *names,unsigned count,float (*frames)[16]);
int gt4_license_motion_frames_blend(const gt4_license_motion *m,unsigned slot,float blend,const char *const *names,unsigned count,float (*frames)[16]);
/* 00429360 / 0042B440 opcode 16: authored named scalar parameter. */
int gt4_license_motion_scalar(const gt4_license_motion *m,unsigned slot,float blend,const char *label,float *value);
/* 00429F98: first sequence interval carrying a label. */
int gt4_license_motion_label_interval(const gt4_license_motion *m,unsigned slot,const char *label,float *begin,float *end);
int gt4_license_motion_camera(const gt4_license_motion *m,float look[9],float *fov,float *camera_coord,float *target_coord);
int gt4_license_motion_camera_blend(const gt4_license_motion *m,float blend,float look[9],float *fov,float *camera_coord,float *target_coord);
#endif
