#ifndef OPENGT4_CAREER_ORIGINAL_H
#define OPENGT4_CAREER_ORIGINAL_H
#include <stdint.h>
#include <stddef.h>
/* 00119AB0: EE signed float-to-word conversion, truncating/saturating. */
int32_t gt4_career_course_length(float metres);
/* 00114390 +0x454: unfinished entries have the original no-time marker. */
uint32_t gt4_career_grid_time(int finished,uint32_t line_time);
#define GT4_CAREER_MONEY_OFFSET 0x13ad0u
#define GT4_CAREER_MONEY_KEY UINT64_C(0x7a726f706c6c525e)
#define GT4_CAREER_GARAGE_CAPACITY 1000u
uint64_t gt4_career_credit_add(uint64_t balance,uint64_t delta);
int gt4_career_money_enough(int64_t balance,int64_t cost);
uint64_t gt4_career_money_get(const uint8_t *profile);
void gt4_career_money_set(uint8_t *profile,uint64_t balance);
void gt4_career_money_inc(uint8_t *profile,uint64_t delta);
int gt4_career_record_meets_grade(uint8_t record,uint32_t limit);
/* Direct native attribute writes preserve every other bit (unlike update). */
uint32_t gt4_career_record_field_set(uint32_t record,unsigned field,uint32_t value);
uint32_t gt4_career_record_init(uint32_t record);
uint32_t gt4_career_record_present(uint32_t record);
int gt4_career_record_score(uint32_t *record,int64_t score);
int gt4_career_record_update(uint32_t *record,int32_t type,uint32_t grade);
int gt4_career_record_predicate_00430bc0(uint32_t record);
int gt4_career_record_predicate_00430c50(uint32_t record);
int gt4_career_record_predicate_00430b58(uint32_t record);
int gt4_career_garage_first_free(const uint8_t *garage);
unsigned gt4_career_garage_count(const uint8_t *garage);
/* Valid index 0..999; preserves car bytes and all flag bits except occupied. */
int gt4_career_garage_delete(uint8_t *garage,uint32_t index);
/* Valid index [0,999]; garage contains the original 0x81f8-byte object.
   Full live car is 0x4c0 bytes, including its eight-byte class header. */
void gt4_career_garage_promote(uint8_t *garage,uint32_t index);
void gt4_career_garage_set_riding(uint8_t *garage,uint32_t index,const uint8_t car[0x4c0]);
void gt4_career_garage_change_riding(uint8_t *garage,uint32_t index,const uint8_t car[0x4c0]);
/* Serialized car body excludes the eight-byte live class header.
   Copy the active 0x178-byte setup, not a freshly constructed stock car. */
int gt4_career_active_setup(const uint8_t body[0x4b8],uint8_t setup[0x178]);
/* 0035E108: decoded RACE row to conductor's 0x80-byte rule block. */
void gt4_career_session_rules(const uint8_t row[151],uint8_t rules[0x80]);

void gt4_career_replace_oil(uint8_t *wear,uint8_t *params,uint8_t *settings);
typedef struct {uint8_t params[36],settings[0xd9];uint8_t clutch_maximum[4],dirt_params[12];} gt4_career_oil_data;
float gt4_career_dirtiness(const uint8_t wear[32],const uint8_t params[12]);
void gt4_career_set_dirtiness(uint8_t wear[32],float value);
float gt4_career_oil_life(const uint8_t wear[32],uint8_t params[36],uint8_t settings[0xd9]);
/* Nonnegative mileage/color branch of 00440FB8 after 00343490/00343420. */
void gt4_career_car_wear_init(uint8_t wear[32],uint64_t mileage,uint8_t exempt,gt4_career_oil_data *data);
int gt4_career_oil_data_load(const char *iso,gt4_career_oil_data *out);
uint32_t gt4_career_race_prize(const uint8_t *race,uint64_t position);
void gt4_career_odometer_set(uint8_t *wear,uint64_t mileage);
/* 0010FB3C..0010FBB4: PS2 cvt.w.s with unsigned correction. */
uint32_t gt4_career_distance_integer(float distance);
/* ctor table 0061736c -> 003BB820/003BB7E8 -> 005792D8. */
void gt4_career_enemy_rng_init(uint32_t *state);
/* medal: 0 gold, 1 silver, 2 bronze; decoded original RACE structure. */
uint32_t gt4_career_license_time(const uint8_t *race,unsigned medal);
typedef enum {
 GT4_RACE_LICENSE,GT4_RACE_DRIVETRAIN,GT4_RACE_ASPIRATION,GT4_RACE_CAR_TYPE,
 GT4_RACE_TIRE,GT4_RACE_PS,GT4_RACE_PRICE,GT4_RACE_WEIGHT,GT4_RACE_LENGTH,
 GT4_RACE_COUNTRY,GT4_RACE_LAPS,GT4_RACE_LIMIT_TIME,GT4_RACE_PRIZE_GC,
 GT4_RACE_YEAR_MIN,GT4_RACE_YEAR_MAX,GT4_RACE_ENTRY_NUM
} gt4_career_race_field_id;
int32_t gt4_career_race_field(const uint8_t *race,gt4_career_race_field_id field);
/* Decoded 24-byte GENERIC_CAR row; original scalar getters. */
int32_t gt4_career_car_year(const uint8_t *row);
uint32_t gt4_career_car_open_type(const uint8_t *row);
uint32_t gt4_career_car_category(const uint8_t *row);
/* Serialized present flags exclude the four-byte live class header. */
int gt4_career_present_get(const uint8_t flags[116],uint32_t bit);
void gt4_career_present_set(uint8_t flags[116],uint32_t bit);
uint32_t gt4_career_prize_cars_add(uint32_t count,uint64_t delta);
/* NTSC-U CORE: 00448C10 -> 0044A5A8 +0x04 always returns zero,
   including region 2/3 and prepared keys. This is not an invented filter. */
int gt4_career_enemy_blocked(uint64_t key,int32_t region);
/* func_003BB5A8: select opponent-list indices. blocked marks entries
   rejected by the original country restriction; valid original bounds
   are supplied by the caller. No prepared setups are constructed here. */
int gt4_career_enemy_indices(const uint8_t *blocked, unsigned count,
                            unsigned cars, uint32_t *seed, unsigned *indices);
/* 0040A6F0/0040AA48: original variation order, RGB from VARIATION +0x24.
   color == -1 asks the original weighted selector; other indices are fixed. */
typedef struct { unsigned count; uint32_t rgb[32]; int color; } gt4_career_paint_car;
typedef struct { int32_t index; float weight, rgb[3], length; } gt4_career_paint_choice;
typedef struct { int32_t count; gt4_career_paint_choice choices[32]; } gt4_career_paint_group;
int gt4_career_paint_weights(const gt4_career_paint_car *cars,unsigned count,
                             gt4_career_paint_group groups[6]);
int gt4_career_select_paints(gt4_career_paint_car *cars,unsigned count,uint32_t *seed);
/* 004474B0 +0x44..0x78 / 00445880: one-based RACES paint to garage sb.
   Rejected high indices keep the constructor's current paint. */
int gt4_career_paint_list_index(uint64_t paint,int32_t total,int current);
/* 00114498: result processing before the quick menu pays the prize.
   statistics retains its original live layout (including class word).
   Inputs are the finished flag, sorted position, best lap, driver mode,
   A-spec value and prize getter's result, all supplied by the race caller. */
typedef struct {
    uint8_t statistics[0x70];
    uint32_t record;
    uint8_t previous_best;
    uint32_t awarded_prize,calendar_grade,calendar_method,calendar_time;
} gt4_career_event_result;
void gt4_career_process_event_result(gt4_career_event_result *state,int finished,
    int32_t position,uint32_t best_lap,int32_t driver_mode,int32_t aspec,uint32_t prize);
/* 0011C328 / 0011C2E0: a driving mission's result (record field 3). */
void gt4_career_process_mission_result(gt4_career_event_result *state,int finished,
    int32_t position,uint32_t race_time,int32_t aspec,uint32_t prize);
/* 003EEFE0/003EF1E8: ordered driver ids; an abandoned player is skipped. */
void gt4_career_championship_points(int32_t points[6],const uint8_t order[6],
    unsigned count,int finished,const int8_t awards[6]);
/* 003EF298/003EF2D8: ties prevent victory and count ahead of the player. */
int gt4_career_championship_rank(const int32_t points[6]);
typedef struct {
    gt4_career_event_result result;
    uint32_t series_record,grand_prize,series_rank;
} gt4_career_championship_result;
void gt4_career_process_championship_result(gt4_career_championship_result *state,
    int finished,int final_session,int32_t position,uint32_t best_lap,
    int32_t driver_mode,int32_t aspec,uint32_t prize,const int32_t points[6],
    int16_t grand_prize_units);
/* 00119BD8: serialized championship context at game+0x38bf8.
   First driver uses its owned key, opponents their prepared keys.
   Clearing active changes only +0x28, retaining the rest of the block. */
void gt4_career_championship_context(uint8_t context[0x90],int active,
    uint32_t round,uint32_t last_round,const char *series,
    const uint64_t keys[6],const int32_t paints[6],const int32_t points[6]);
/* 0042FB48/0042FD00..00430010: game menu object +0xA68..0xCB0.
   Preserve unused bytes and string tails just like original memcpy. */
typedef struct {
    int32_t length;uint8_t entries[256],reserved[64];
    int32_t saved_length;uint8_t saved[256];
} gt4_career_menu_stack;
void gt4_career_menu_clear(gt4_career_menu_stack *s);
void gt4_career_menu_push(gt4_career_menu_stack *s,const char *text);
const char *gt4_career_menu_pop(gt4_career_menu_stack *s);
const char *gt4_career_menu_ref(const gt4_career_menu_stack *s,int32_t depth);
void gt4_career_menu_save(gt4_career_menu_stack *s);
int gt4_career_menu_load(gt4_career_menu_stack *s);
/* 00441350: synchronize three setup transmission keys; types are from
   004462E0, TRANSMISSION row +0x3a (or -1 for missing key). */
void gt4_career_end_setting(uint8_t body[0x4b8],const int32_t types[3]);
/* 004401C0: load 11 equipped part rows into the setting wrapper buffer.
   Lookup returns 0 for untouched/missing row, >0 on copy, <0 on error. */
typedef int (*gt4_career_setting_lookup)(void *ctx,uint64_t key,uint8_t *out,size_t capacity);
int gt4_career_prepare_setting(const uint8_t body[0x4b8],uint8_t *buffer,size_t size,
    gt4_career_setting_lookup lookup,void *ctx);
/* 00147D88 calls 00449D58(cache +0x18), then 00146628(auxiliary
   fields +0x174..0x188). Does not initialize equipped row buffer. */
void gt4_career_setting_wrapper_reset(uint8_t wrapper[0x190],uint32_t cache_type);
/* 00140530 -> 0043F4C8: per-category equipped stage, not piece key. */
int gt4_career_equipped_stage(const uint8_t body[0x4b8],int32_t type,
    const uint8_t normal_flags[32],gt4_career_setting_lookup lookup,void *ctx,int32_t *stage);
/* 00446AE0 writes only UINT64_MAX to the selected setup part key. */
int gt4_career_install_defaults(uint8_t body[0x4b8],uint64_t key,uint8_t buffer[0x142],gt4_career_setting_lookup lookup,void *ctx);
int gt4_career_install_key_only(uint8_t body[0x4b8],uint64_t key);
int gt4_career_remove_part(uint8_t body[0x4b8],uint32_t type);
/* 0013F3A8 -> 004455B0: original muffler absent in DEFAULT_PARTS. */
int gt4_career_default_no_part(const uint8_t body[0x4b8],int32_t type,
    gt4_career_setting_lookup lookup,void *ctx,int *result);
#endif
