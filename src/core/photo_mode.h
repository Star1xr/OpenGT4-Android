#ifndef OPENGT4_PHOTO_MODE_H
#define OPENGT4_PHOTO_MODE_H
#include "course.h"
/* PhotoPause 003C4D98/9C -> PhotoDevelop+118. */
#define GT4_PHOTO_SAMPLES 32

/* Borrows the installed course. CRPM offsets remain relative to its block;
   native pointers are never written into the disc buffer. */
typedef struct {
    const uint8_t *data; size_t size;
    uint32_t regions, region_table, cameras, camera_table;
    float bounds[4], camera[3], car[3], car_angle;
    float map_width, map_height, map_length;
} gt4_photo_location;
typedef struct {
    gt4_photo_location location;
    float eye[3], car[3], yaw, pitch, roll, car_angle;
    float view_angle, aperture, wheel_angle, lightness, brightness, contrast;
    int white_balance, stand, ready, drive, focus_requested;
    /* PhotoCamera 00374128/00374120: +C10 index / +C08 count.
       Native bank keeps independent edited cameras; renderer restrictions are A. */
    int drive_camera, drive_cameras, manual_focus,drive_ev,drive_white;
    float drive_kelvin;
    float focus_distance;
    int shutter; /* 0036F628: index 0..18 in CORE 00620EF0 */
    int turn_frames, image_width, image_height, save_requested;
    float image_scale, image_x, image_y;
    char operate_mode[24], color_filter[24];
} gt4_photo_mode;
int gt4_photo_location_read(const uint8_t *course,size_t size,gt4_photo_location *out);
int gt4_photo_mode_init(gt4_photo_mode *p,const gt4_course *course);
/* 0036F5C0 / 00370DB0, read directly from CORE's aperture table. */
float gt4_photo_aperture(const uint8_t *core,size_t size,int index);
int gt4_photo_apertures_load(const char *disc,float out[17]);
float gt4_photo_fov(float millimetres);
int gt4_photo_tables_load(const char *disc,float apertures[17],int32_t shutters[19]);
int gt4_photo_drive_tables_load(const char *disc,float ev[21],float kelvin[11]);
float gt4_photo_shutter_seconds(int32_t encoded);
float gt4_photo_exposure(float seconds);
float gt4_photo_time(float exposure,int sample,int count);
float gt4_photo_wheel_height(float height,float limit,float radius);
float gt4_photo_angle(float current,float previous,float time);
void gt4_photo_lerp(float *out,const float *current,const float *previous,int count,float time);
void gt4_photo_step(gt4_photo_mode *p,const gt4_course *course,float x,float z,float turn);
/* Photo Develop 003EFC10 / 003C7DA8: RGB gains, offset, contrast, saturation. */
void gt4_photo_color(const gt4_photo_mode *,float out[6]);
/* 003323B0: deterministic lens-disc samples, seed 12345678. */
void gt4_photo_samples(float (*out)[4],int count);
/* 00332550: lens displacement and projection compensation, focus in metres. */
void gt4_photo_lens(float focal,float aperture,float focus,float aspect,float sy,
                    const float sample[4],float shift[2],float jitter[2]);
#endif
