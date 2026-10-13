#ifndef OPENGT4_CAREER_HOME_H
#define OPENGT4_CAREER_HOME_H
#include <stdint.h>
/* D_00623E40 in the user's CORE.GT4. */
typedef struct {uint8_t weights[8];char event_names[9][24],car_event_names[5][24],course_event_names[8][24];} gt4_career_home_data;
int gt4_career_home_data_load(const char *iso,gt4_career_home_data *out);
void gt4_career_bspec_status(const gt4_career_home_data *data,const uint8_t experience[0x180],int32_t out[4]);
int32_t gt4_career_home_race_count(const uint32_t positions[7]);
int32_t gt4_career_history_index(int32_t today,int32_t requested,unsigned count);
int gt4_career_course_available(const uint64_t courses[128],uint64_t key,int all);
void gt4_career_course_unlock(uint64_t courses[128],uint64_t key);
int64_t gt4_career_home_distance(uint64_t metres,int miles);
#endif
