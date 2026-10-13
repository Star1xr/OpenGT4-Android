#ifndef OPENGT4_CAREER_LICENSE_H
#define OPENGT4_CAREER_LICENSE_H
#include <stdint.h>

/* State passed between attempts of one exam by 00114E38. This is separate
   from the persistent packed record; it is not a whole-category licence. */
typedef struct {
    uint32_t record;
    uint32_t best_grade;
    uint32_t best_time;
} gt4_career_license_attempts;

/* 001105A8: completed work states 2/3/4 become gold/silver/bronze. Other
   states leave records untouched. Return grade 13 for an unsuccessful run.
   The caller must also submit each successful run to the original ranking
   adapter; ghost/presentation data is outside this record primitive. */
uint32_t gt4_career_license_attempt(gt4_career_license_attempts *attempts,
                                    int32_t work_state,uint32_t time);

/* 00110800: one calendar entry on leaving the exam, using the best run of
   this visit; a visit with no successful run advances the day instead.
   Returns 1 for a licence event, 0 for day advancement. */
int gt4_career_license_history(const gt4_career_license_attempts *attempts,
                              uint32_t *grade,uint32_t *method,uint32_t *time);
/* 004323D0: index into 125 exam ranking units, -1 for an invalid code. */
int gt4_career_license_index(const char *code);
/* 0010FCD0: next exam in B/A/IB/IA/S, 0 after exam 16. */
int gt4_career_license_next(const char *code,char next[32]);
/* ResultLicense PCM request (003E1C80/003E1EA0), standard exams. */
int gt4_career_license_music(int32_t state);
#endif
