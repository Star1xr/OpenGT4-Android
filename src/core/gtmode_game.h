#ifndef OPENGT4_GTMODE_GAME_H
#define OPENGT4_GTMODE_GAME_H
#include "career_license.h"
#include "adhoc/adhoc_vm.h"
#include "career_calendar.h"
#include "career_home.h"
#include "career_original.h"
#include "career_prices.h"
#include "career_race_lookup.h"
#include "career_parts.h"
typedef int (*gt4_game_car_factory)(void *ctx,const char *label,int32_t color,uint64_t mileage,int32_t seed,uint8_t unit[32],uint8_t body[0x4b8]);
typedef int (*gt4_game_auto_gears)(void *ctx,uint8_t body[0x4b8]);
/* Menu-only adapter; operation 0 reads gear limits, 1 recalculates auto
   ratios, 2 reads power/torque samples, 3 reads gear chart endpoints. It never changes the live runner. */
typedef struct {int32_t gear[23];int count;float rpm[24],power[24],torque[24],peak_power,peak_torque,max_rpm;int chart_count;float chart[18][2];} gt4_setting_details;
typedef int (*gt4_game_setting_reader)(void *ctx,uint8_t body[0x4b8],int operation,int auto_override,gt4_setting_details *out);
void gt4_game_set_setting_reader(gt4_game_setting_reader fn,void *ctx);
typedef int (*gt4_game_stats_reader)(void *ctx,const uint8_t body[0x4b8],int32_t out[3]);
void gt4_game_set_stats_reader(gt4_game_stats_reader fn,void *ctx);
int gt4_game_car_stats(ah_vm *vm,ah_obj *car,int32_t out[3]);
typedef int (*gt4_game_service_reader)(void *ctx,const uint8_t body[0x4b8],float out[12]);
void gt4_game_set_service_reader(gt4_game_service_reader fn,void *ctx);
int gt4_game_car_service_geometry(ah_vm *vm,ah_obj *car,float out[12]);
void gt4_game_set_auto_gears(gt4_game_auto_gears fn,void *ctx);
void gt4_game_set_car_factory(gt4_game_car_factory factory,void *ctx);
/* Construct stock preview/used-list objects with the same native factory. */
int gt4_game_construct_car(ah_vm *vm,ah_obj *car,const char *label,int32_t color,uint64_t mileage);
int gt4_game_has_car_factory(void);
int gt4_game_car_unit(ah_vm *vm,ah_obj *car,uint8_t out[32]);
int gt4_game_car_body(ah_vm *vm,ah_obj *car,uint8_t out[0x4b8]);
/* 0010FAA8..0010FB24: copy the player's complete engine-wear block back
   into the equipped garage car after each driven attempt. */
int gt4_game_apply_car_wear(ah_vm *vm,ah_obj *game,const uint8_t wear[32]);
/* 00114044..0011407c: add only the last attempt's A/B distances on exit. */
int gt4_game_apply_event_distances(ah_vm *vm,ah_obj *game,uint32_t a,uint32_t b);
/* Attach stateful script contracts without manufacturing race results. */
void gt4_game_set_part_data(const gt4_career_part_data *data);
const gt4_career_part_data *gt4_game_part_data(void);
void gt4_game_bind_record_unit(ah_vm *vm,ah_obj *unit);
void gt4_game_bind(ah_vm *vm,ah_obj *game);
/* func_0042E6B8 (US divisor 1): thousands with commas, "--" for nonpositive
   amounts. 0 when it does not fit. */
int gt4_game_money_text(double amount,char *out,size_t size);
/* Load original constants from the user's disc before constructing menus. */
int gt4_game_load_disc(const char *iso);
void gt4_game_set_race_lookup_data(const gt4_career_race_lookup_data *data);
const gt4_career_race_lookup_data *gt4_game_race_lookup_data(void);
void gt4_game_set_calendar_data(const gt4_career_calendar_data *data);
void gt4_game_set_home_data(const gt4_career_home_data *data);
const gt4_career_home_data *gt4_game_home_data(void);
int gt4_game_calendar_event(ah_vm *vm,ah_obj *calendar,int argc,const ah_val *argv,uint8_t event[16]);
/* Returns zero until the original disc-backed date constants are loaded. */
int gt4_game_decode_calendar_day(int32_t day, int date[3]);
void gt4_game_set_oil_data(const gt4_career_oil_data *data);
void gt4_game_set_price_data(const gt4_career_price_data *data);
int gt4_game_car_price(int32_t base,uint32_t odometer,int sell,int32_t *price);
void gt4_game_bind_car(ah_vm *vm,ah_obj *car);
/* 00114498: apply the result before the authored prize menu pays credits.
   previous_best belongs to the current event work, initialized by 00114250.
   The caller supplies the live result getters; replay must never call this. */
int gt4_game_apply_event_result(ah_vm *vm,ah_obj *game,const char *race,
    int finished,int32_t position,uint32_t best_lap,int32_t driver_mode,
    int32_t aspec,uint32_t prize,uint8_t *previous_best,uint32_t *awarded_prize);
int gt4_game_apply_mission_result(ah_vm *vm,ah_obj *game,const char *race,
    int finished,int32_t position,uint32_t race_time,int32_t aspec,uint32_t prize,uint32_t *awarded_prize);
int gt4_game_apply_championship_result(ah_vm *vm,ah_obj *game,const char *race,const char *series,
    int finished,int final_session,int32_t position,uint32_t best_lap,int32_t driver_mode,
    int32_t aspec,uint32_t prize,const int32_t points[6],int16_t units,
    uint32_t *awarded_prize,uint32_t *grand_prize);
/* Serialized data omits the live object's eight-byte class header. */
int gt4_game_championship_context_get(ah_vm *vm,ah_obj *game,uint8_t live[0x90]);
int gt4_game_championship_context_set(ah_vm *vm,ah_obj *game,const uint8_t live[0x90]);
int gt4_game_menu_stack_action(ah_vm *vm,ah_obj *game,const char *operation,
    int argc,const ah_val *argv,ah_val *ret);
void gt4_calendar_bind(ah_vm *vm,ah_obj *calendar);
uint32_t gt4_game_license_best(ah_vm *vm,ah_obj *game,const char *code);
int gt4_game_license_begin(ah_vm *vm,ah_obj *game,const char *code,gt4_career_license_attempts *visit);
int gt4_game_license_attempt(ah_vm *vm,ah_obj *game,const char *code,
    gt4_career_license_attempts *visit,int32_t state,uint32_t time,uint32_t date);
int gt4_game_license_leave(ah_vm *vm,ah_obj *game,const char *code,const gt4_career_license_attempts *visit);
#endif
