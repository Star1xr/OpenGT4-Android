#ifndef OPENGT4_LICENSE_IMAGE_H
#define OPENGT4_LICENSE_IMAGE_H
#include <stdint.h>
/* Scene +E308: channel exponents, black/white endpoints, saturation. */
void gt4_license_image_options(const int options[4],float parameters[6]);
float gt4_license_image_decay(float previous);
/* NULL returns the fixed step; flag=1 consumes the first result draw. */
float gt4_license_result_tick(int *first);
void gt4_license_image_lut(const float parameters[6],uint8_t rgb[256][3]);
/* Three GS RGBAQ colours, indexed by input then output channel. */
void gt4_license_image_mix(float saturation,uint8_t coefficients[3][3]);
void gt4_license_image_pixel(const uint8_t lut[256][3],const uint8_t coefficients[3][3],const uint8_t input[3],uint8_t output[3]);
/* Authored staff projection in world axes (column-major). */
void gt4_license_shadow_projection(const float way[3],const float normal[3],float matrix[16]);
/* 00397FF8; RGB only, the authored alpha is unchanged. */
void gt4_license_light_color(const float color[3],float sun,float fade,float level,float blend,float exposure,float output[3]);
#endif
