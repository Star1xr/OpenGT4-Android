#ifndef OPENGT4_LICENSE_SESSION_H
#define OPENGT4_LICENSE_SESSION_H
#include <stdint.h>
/* RaceLicense's conductor (vtable 00685A40). Guest addresses use car.h. */
#define GT4_LICENSE_SESSION_VT 0x00685a40u
/* 003ED298 +0x74: inherited licence conductor with training overrides. */
#define GT4_TRAINING_SESSION_VT 0x00685f50u
/* RaceMission's conductor (003BF6E8 +0x10140): the training's table with
   its own entries at 0x08, 0xa0, 0xc0, 0x110 and 0x160..0x190 */
#define GT4_MISSION_SESSION_VT 0x00685da0u
int gt4_license_finish_mode(uint32_t cond, int failure);
int gt4_license_stop(uint32_t car, float *fraction);
int gt4_license_wait(uint32_t cond, int advance);
void gt4_license_finish(uint32_t cond, uint32_t car, uint32_t elapsed);
void gt4_license_fail(uint32_t cond, uint32_t car, int reason);
void gt4_license_collision(uint32_t cond, uint32_t car, uint32_t kind, uint32_t detail, float force);
void gt4_license_evaluate(uint32_t cond, uint32_t car);
/* func_00358000: a new best flagged (race +0x98) for the player's entry */
void gt4_session_best(uint32_t c,uint32_t i,uint32_t elapsed);
#endif
