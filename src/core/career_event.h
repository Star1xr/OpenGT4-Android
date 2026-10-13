#ifndef OPENGT4_CAREER_EVENT_H
#define OPENGT4_CAREER_EVENT_H
#include "physics/race_run.h"
#include "career_race_lookup.h"

/* Owns the buffers referenced by desc. Do not copy/move after preparing.
   The caller supplies and retains the original RNG state; open consumes it
   further for paint selection. No clock/Arcade seed is substituted. */
typedef struct {
    gt4_race_desc desc;
    uint8_t race_row[151], player_body[0x4b8];
} gt4_career_event;

int gt4_career_event_prepare(gt4_career_event *event,const char *iso,
    const gt4_career_race_lookup_data *lookup,const char *race,
    const uint8_t player_body[0x4b8],int championship,uint32_t *rng,
    char *message,size_t capacity);
/* Fresh process state of D_00621AE8 after original startup constructor
   003BB820. Validates the user's CORE; no save state or clock seed. */
int gt4_career_event_rng_load(const char *iso,uint32_t *rng);
/* 003EF0E8/003EF388: last four digits are NN RR: NN encodes the series length;
   zero-based round rewrites the final two digits to round+1. */
int gt4_career_championship_label(const char *series,unsigned round,char label[32],unsigned *count);
/* 003EF088/003EF388: change RACE/COURSE while retaining the six entrants. */
int gt4_career_championship_round(gt4_career_event *event,const char *iso,
    const gt4_career_race_lookup_data *lookup,const char *label,
    const gt4_race_desc *grid,char *message,size_t capacity);
/* 00119AE8: equipped player plus saved opponent prepared keys/paints;
   reads the current stage without drawing new opponents. */
int gt4_career_championship_restore(gt4_career_event *event,const char *iso,
    const gt4_career_race_lookup_data *lookup,const uint8_t context[0x90],
    const uint8_t player_body[0x4b8],uint32_t *rng,char *message,size_t capacity);
#endif
