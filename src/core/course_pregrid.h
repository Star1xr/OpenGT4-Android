/* Arcade SceneViewFace camera: course header +0xC0, REP4, mode 5.
   The borrowed course bytes must remain alive until close. */
#ifndef OPENGT4_COURSE_PREGRID_H
#define OPENGT4_COURSE_PREGRID_H
#include <stddef.h>
#include <stdint.h>
typedef struct gt4_pregrid gt4_pregrid;
typedef struct { float eye[3], target[3], roll, fov, sx, sy; unsigned shot, frame; } gt4_pregrid_camera;
gt4_pregrid *gt4_pregrid_open(const uint8_t *course, size_t size);
void gt4_pregrid_close(gt4_pregrid *p);
void gt4_pregrid_reset(gt4_pregrid *p);
/* One original presentation tick; never steps the race physics. */
int gt4_pregrid_step(gt4_pregrid *p, gt4_pregrid_camera *out);
int gt4_pregrid_sample(const gt4_pregrid *p, unsigned shot, int frame, gt4_pregrid_camera *out, int *done);
unsigned gt4_pregrid_count(const gt4_pregrid *p);
/* Testable original scalar boundaries (003DDBC8, 003FC9A0, 003794A8). */
float gt4_pregrid_curve(const uint8_t *curve, float t);
float gt4_pregrid_time(float seconds, int frame, int *done);
void gt4_pregrid_projection(float degrees, float *sx, float *sy);
#endif
