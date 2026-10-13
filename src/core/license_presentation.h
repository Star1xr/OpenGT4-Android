#ifndef OPENGT4_LICENSE_PRESENTATION_H
#define OPENGT4_LICENSE_PRESENTATION_H
#include <stdint.h>
#include <stddef.h>
/* RaceDisplayElement's first 12 bytes: shown/alignment/position/size. */
typedef struct {
    uint8_t shown,ha,va,pad;
    int16_t x,y,w,h;
} gt4_license_element;
typedef struct {
    gt4_license_element times[3],marks[3],pylons,pylon_label;
} gt4_license_layout;
void gt4_license_layout_make(gt4_license_layout *layout);
/* ResultLicense +0x584..0x623, ten value/target/gain/drift springs. */
void gt4_license_trophy_begin(float springs[10][4],int state);
void gt4_license_trophy_step(float springs[10][4],float dt);
/* Select the authored MDLS material/CLUT variant before native decoding. */
int gt4_license_trophy_variant(uint8_t *model,size_t size,unsigned variant);
/* Scene fade +CF4C/+CF54. Rendering is a separate GPU boundary. */
typedef struct {float value,rate;} gt4_license_fade;
void gt4_license_fade_begin(gt4_license_fade *fade);
void gt4_license_fade_step(gt4_license_fade *fade,int enabled,int previous,int phase,float dt);
float gt4_license_fade_factor(float value);
/* Authored Mot1 FOV is in radians; the camera stores degrees. */
void gt4_license_camera_projection(float radians,float *sx,float *sy);
/* 0037D5B0 / D_00621290: controlled opening sequence by frames left. */
int gt4_license_opening_control(int frames);
/* 0012B928/0012BC18: menu Scene lifecycle, separate from result fades.
   Fields match MenuRaceBase +A0..B8. Reset work is an explicit boundary. */
typedef struct {
    int paused,job,ready,reserved,phase;
    float duration,alpha;
} gt4_license_menu_cycle;
/* 1 requests reconstruction; acknowledge with ready=1 after success. */
int gt4_license_menu_cycle_update(gt4_license_menu_cycle *cycle);
/* Call after the Scene step (only when paused==0 and phase==0). */
void gt4_license_menu_cycle_draw(gt4_license_menu_cycle *cycle,int ended);
#endif
