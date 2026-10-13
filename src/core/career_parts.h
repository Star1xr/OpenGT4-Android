#ifndef OPENGT4_CAREER_PARTS_H
#define OPENGT4_CAREER_PARTS_H
#include <stdint.h>
#include <stddef.h>
typedef struct {uint32_t price_branches[31];int8_t owned_bases[31];uint8_t normal_flags[32],type_max[32];char table_names[39][24];char category_names[100][24],category_fallback[24];char country_names[256][24],tuner_names[256][24],wheel_categories[4][24];} gt4_career_part_data;
int gt4_career_part_data_load(const char *iso,gt4_career_part_data *out);
uint64_t gt4_career_part_key(const uint8_t *list,size_t size,int64_t type,int64_t stage);
/* func_00448928: reachable byte remainder keys 0..99 only. */
const char *gt4_career_car_category_name(const gt4_career_part_data *data,const uint8_t *row);
/* Original branch code interprets a row; table selection remains disc data. */
int32_t gt4_career_part_row_price(uint32_t branch,const uint8_t *row,size_t size);
int gt4_career_part_removable(int32_t type);
int gt4_career_part_removed_normal(int32_t type);
/* An owned car's fitted wheel (body +0x468, normal tyres; 00440938) and
   wing (the active sheet's +0x100, sheet at +0x488) as their WHEEL/WING
   labels, empty when stock: what 00454270/00454378 load the models by. */
struct gt4_vol;
int gt4_body_part_labels(struct gt4_vol *v,const uint8_t *body,char wheel[32],char wing[32]);
#endif
