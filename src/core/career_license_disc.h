#ifndef OPENGT4_CAREER_LICENSE_DISC_H
#define OPENGT4_CAREER_LICENSE_DISC_H
#include "physics/race_run.h"
#include "career_race_lookup.h"
typedef struct {
    gt4_race_desc desc;
    uint8_t row[151];
    char code[32];
} gt4_career_license_exam;
/* 0011D0F8 -> 00447400(0) -> 00444440: exam car comes from RACES,
   without requiring or replacing the player's owned garage car. */
int gt4_career_license_prepare(gt4_career_license_exam *exam,const char *iso,
    const gt4_career_race_lookup_data *lookup,const char *code,char *error,size_t size);
/* 0011BB60 -> 0011BEC0: a driving mission (RACE "l0mNNNN"), its cars
   from RACES, the player's the first; desc.license 3 (RaceMission) */
int gt4_career_mission_prepare(gt4_career_license_exam *exam,const char *iso,
    const gt4_career_race_lookup_data *lookup,const char *code,char *error,size_t size);
#endif
