#ifndef OPENGT4_RACE_MISSION_H
#define OPENGT4_RACE_MISSION_H
#include <stdint.h>
enum { GT4_MISSION_RUNNING, GT4_MISSION_BOARD, GT4_MISSION_RETURN, GT4_MISSION_STOPPED };
/* Race queue at race+F4, read only; retain the previous presentation state. */
int gt4_mission_presentation(const uint8_t *queue,int previous);
/* Next world tick, before advancing the race. No additional physics step. */
int gt4_mission_begin_frame(int previous);
#endif
