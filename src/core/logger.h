#ifndef GT4_LOGGER_H
#define GT4_LOGGER_H
#include <stdint.h>

/* 0011E450 +0x28..0x358: two banks of 0x4000 eight-byte samples. */
#define GT4_LOGGER_POINTS 0x4000u
typedef struct {
    uint16_t speed_gear;
    uint8_t accelerator, brake;
    int16_t x, z;
} gt4_logger_point;
typedef struct {
    float origin, last_distance, speed_area, accelerator_area, brake_area;
    float previous_distance, previous_speed, previous_accelerator, previous_brake;
    float previous_x, previous_z, maximum_speed;
    gt4_logger_point *points;
    uint32_t count;
    float spacing, inverse_spacing, course_length;
    int first;
} gt4_logger_sampler;
typedef struct {
    float distance, speed, accelerator, brake, x, z;
    int32_t lap;
    uint8_t gear;
} gt4_logger_input;

/* Arithmetic expects the original toward-zero rounding, as the physics does.
   Caller owns a buffer of at least GT4_LOGGER_POINTS points. Inputs are the
   finite native car values; PS2 exceptional-FPU emulation is not provided. */
float gt4_logger_spacing(float course_length);
void gt4_logger_begin(gt4_logger_sampler *, gt4_logger_point *, float spacing, float course_length);
/* Raw native car layout is a read-only boundary, not an EE dependency. */
void gt4_logger_car_input(gt4_logger_input *, const uint8_t *car);
void gt4_logger_sample(gt4_logger_sampler *, const gt4_logger_input *);
float gt4_logger_length(const gt4_logger_sampler *);
float gt4_logger_speed_at(const gt4_logger_sampler *, float distance);
float gt4_logger_x_at(const gt4_logger_sampler *, float distance);
float gt4_logger_z_at(const gt4_logger_sampler *, float distance);
#endif
