#ifndef OPENGT4_CAREER_RACE_LOOKUP_H
#define OPENGT4_CAREER_RACE_LOOKUP_H
#include <stdint.h>
#include <stddef.h>
typedef struct {char prefixes[2][3];uint8_t grade_values[16];char grade_names[16][16];uint8_t driver_points[6];char type_names[4][16];uint8_t method_values[15];char method_names[15][16];} gt4_career_race_lookup_data;
/* func_00447598, supported race positions 0..5. */
int gt4_career_driver_points(const gt4_career_race_lookup_data *data,unsigned position);
unsigned gt4_career_race_method(const gt4_career_race_lookup_data *data,const char *name);
int gt4_career_record_type(const gt4_career_race_lookup_data *data,const char *name);
const char *gt4_career_result_name(const gt4_career_race_lookup_data *data,unsigned grade);
unsigned gt4_career_result_grade(const gt4_career_race_lookup_data *data,const char *name);
int gt4_career_race_lookup_data_load(const char *iso,gt4_career_race_lookup_data *out);
/* Original fallback copies at most 63 source bytes after its two-byte prefix. */
int gt4_career_race_record_index(const gt4_career_race_lookup_data *data,
 const uint8_t *labels,size_t labels_size,const uint8_t *db,size_t db_size,const char *label);
int gt4_career_race_row(const gt4_career_race_lookup_data *data,
 const uint8_t *labels,size_t labels_size,const uint8_t *db,size_t db_size,const char *label,uint8_t row[151]);
#endif
