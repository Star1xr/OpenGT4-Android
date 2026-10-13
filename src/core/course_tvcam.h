/* The course's TV cameras (CameraShot, func_0037C138): REP4 at course
   header +0xB0, its shots by the watched car's distance along the course
   (func_003FD980 -> func_003DDFF0), each one's eye, aim, lens and roll
   from its curves, the aim on the car through the camera operator's
   springs (func_00384E58 / func_00384EE8). The borrowed course bytes must
   stay alive until close. */
#ifndef OPENGT4_COURSE_TVCAM_H
#define OPENGT4_COURSE_TVCAM_H
#include <stddef.h>
#include <stdint.h>

typedef struct gt4_tvcam gt4_tvcam;

/* the watched car: its place (the camera aims 0.4 above it) and distance
   along the course (car+0x5A8) */
typedef struct { float pos[3]; float distance; } gt4_tvcam_target;

/* the camera: eye, the point it looks at, roll and lens (degrees), the
   projection (003794A8) */
typedef struct { float eye[3], target[3], roll, fov, sx, sy; int shot; float pitch, heading; } gt4_tvcam_view;

gt4_tvcam *gt4_tvcam_open(const uint8_t *course, size_t size);
void gt4_tvcam_close(gt4_tvcam *c);
/* forget the shot: the next step picks one again */
void gt4_tvcam_reset(gt4_tvcam *c);
/* the replay's zoom (ShotManager +0x14C: Triangle sets 4), 1 by default */
void gt4_tvcam_set_zoom(gt4_tvcam *c, float zoom);
/* one 60 Hz tick of the replay's zoom (003751F0 +0x60..+0xB8): the
   spring at s[3] (0..20 position, target, speed) toward Triangle's
   `on`; returns the zoom, 1 + 3 x position / 20 between 1 and 4 */
float gt4_tvcam_zoom_step(float s[3], int on);
/* one 60 Hz tick: picks a shot when none runs or the last one ended */
int gt4_tvcam_step(gt4_tvcam *c, const gt4_tvcam_target *t, gt4_tvcam_view *out);
unsigned gt4_tvcam_count(const gt4_tvcam *c);

/* The replay's Dive mode (func_00376638's director with its point
   camera, func_0037EA78): the car watched, where it was a frame ago, its
   distance along the course and speed (m/s), the time to the music's next
   mark (s, NaN when none; race_audio_music_cue) and whether one went by,
   and the course's line at a distance in the render frame
   (func_0034C518). */
typedef struct gt4_dive gt4_dive;
typedef struct {
    float pos[3], prev[3], distance, speed, cue;
    int cue_passed;
    float place[3], yaw;   /* the car's drawn place and heading (radians, gt4_race_run_place) */
    float rot[9];          /* its whole turn, rows (gt4_race_run_pose) */
    float size[4];         /* its params: +0x0 +0x50, +0x4 -0x50, +0x8, +0xC */
    int (*ground)(void *ctx, float x, float y, float z, float *height);
    int (*course_point)(void *ctx, float d, float out[3]);
    int (*has_point)(void *ctx, int i);   /* the car model has camera point i */
    /* the course's checks: the ground triangle under p (-1 none), the
       wall check from -> to (func_003FE7B0), p's distance along it */
    int (*ground_id)(void *ctx, const float p[3]);
    int (*wall)(void *ctx, float margin, const float from[3], const float to[3], float out[3], int *id);
    float (*distance_of)(void *ctx, const float p[3]);
    void *ctx;
} gt4_dive_target;
gt4_dive *gt4_dive_open(const uint8_t *course, size_t size);
void gt4_dive_close(gt4_dive *d);
float gt4_dive_length(const gt4_dive *d);   /* REP4 +0x14 */
void gt4_dive_reset(gt4_dive *d);
/* one 60 Hz tick: 1 the point camera's view in out; 2 a camera on the car,
   its model's point out->shot with the lens out->fov (degrees, 0 the
   point's own), turned by out->roll, out->pitch and out->heading
   (degrees); 0 none */
int gt4_dive_step(gt4_dive *d, const gt4_dive_target *t, gt4_tvcam_view *out);
/* the Dive's colour filter now (func_004007D8): r g b, saturation, offset,
   amount, for r3d_view.grade */
void gt4_dive_filter(const gt4_dive *d, float out[6]);

#endif
