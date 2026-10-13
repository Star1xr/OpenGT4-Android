#ifndef OPENGT4_CAREER_SETTING_H
#define OPENGT4_CAREER_SETTING_H
#include <stdint.h>
enum gt4_setting_group {
    GT4_SETTING_BRAKE, GT4_SETTING_SUSPENSION, GT4_SETTING_LSD,
    GT4_SETTING_GEAR, GT4_SETTING_NOS, GT4_SETTING_DRIVE,
    GT4_SETTING_BALLAST, GT4_SETTING_ASCC, GT4_SETTING_TCSC,
    GT4_SETTING_DOWNFORCE, GT4_SETTING_GROUPS
};
int gt4_setting_get(const uint8_t body[0x4b8],int group,int index,int32_t *out);
int gt4_setting_set(uint8_t body[0x4b8],int group,int index,int32_t value);
/* Returns count, or -1 when an original row byte has not been resolved.
   Gear ranges are computed separately using the native record/ratio API. */
int gt4_setting_range(const uint8_t body[0x4b8],const uint8_t rows[0x142],
    const uint8_t valid[0x142],int group,int32_t out[42]);
/* Caller supplies at least n+1 signed cells; original drive supports nine
   forward ratios, while the menu displays only seven editable sliders. */
void gt4_setting_ratio_bounds(int n,const int16_t *ratios,int16_t *lo,int16_t *hi);
#endif
