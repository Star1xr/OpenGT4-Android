#ifndef OPENGT4_CAREER_CALENDAR_H
#define OPENGT4_CAREER_CALENDAR_H
#include <stdint.h>
/* Original month offsets supplied from CORE.GT4, never embedded tables. */
typedef struct {uint8_t year_offsets[16],month_offsets[52];} gt4_career_calendar_data;
int gt4_career_calendar_data_load(const char *iso,gt4_career_calendar_data *out);
int32_t gt4_career_date_to_day(const gt4_career_calendar_data *data,int32_t year,int32_t month,int32_t day);
uint32_t gt4_career_day_to_date(const gt4_career_calendar_data *data,int32_t day);
int32_t gt4_career_elapsed_days(const gt4_career_calendar_data *data,int32_t today);
int32_t gt4_career_elapsed_weeks(const gt4_career_calendar_data *data,int32_t today);
int32_t gt4_career_day_to_weekday(int32_t day);
/* Valid original calendar object, including its 2922-slot event ring. */
/* func_00600B88: return the signed dirty word and clear it. */
int32_t gt4_career_calendar_consume_dirty(uint8_t *calendar);
void gt4_career_calendar_append(uint8_t *calendar,const uint8_t event[16]);
void gt4_career_car_acquisition_day(uint8_t *slot,const gt4_career_calendar_data *data,int32_t day);
/* func_0042F1B8 (get car) / 0042F218 (sell car). Reserved bytes
   retain caller-provided contents, as the original stack record does. */
void gt4_career_calendar_car_event(uint8_t event[16],int sell,uint64_t key,int64_t color,int64_t detail);
void gt4_career_calendar_race_event(uint8_t event[16],int license,uint64_t key,uint64_t grade,uint64_t method,uint64_t prize);
void gt4_career_calendar_custom_event(uint8_t event[16],int wing,uint64_t key,int64_t category,int64_t detail);
#endif
