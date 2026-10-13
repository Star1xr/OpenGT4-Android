#ifndef OPENGT4_SOUND_DRIVER_H
#define OPENGT4_SOUND_DRIVER_H
/* The game's sound driver on the EE (0055xxxx), the part the race uses:
   48 effect voices (D_00650500, 0x34 bytes) that requests hold through
   handles, 46 hardware voices (D_006518C8 +0xA8, 0xE0 bytes) given out by
   priority, the ENGN controllers' voices, and the SPU2 registers each
   tick makes of them (0055DF70). The registers go to spu.c. */
#include "race_sound.h"
#include "spu.h"

enum { GT4_SD_VOICES = 46, GT4_SD_EFFECTS = 48 };

/* An effect handle (0x20 bytes in the caller: +0x18 voice, +0x1C id). */
typedef struct { int voice; uint32_t id; } gt4_sd_handle;
/* An ENGN controller (0055xxxx object): its two slots' hardware voices. */
typedef struct {
    gt4_engine_control control;
    int hw[2];
    const gt4_engine_bank *bank;
} gt4_sd_engine;

typedef struct gt4_sd gt4_sd;
/* Tables from CORE.GT4 in guest memory: the pan table D_006C8AC0 and the
   octave table D_00654738; `group_volume` per group (manager +0x4C +
   group*12), `stereo` the manager's +0. */
gt4_sd *gt4_sd_open(const uint8_t *pan_table, const uint8_t *octave_table, const int32_t group_volume[8], int stereo);
void gt4_sd_close(gt4_sd *sd);

/* 00462A88 for a tone: keep or get the handle's voice (0055A5F0 by
   priority), then its left/right (x16383), pitch (x4096), priority and
   pan. `once` frees the voice when its sound ends (004629E8 a2). */
void gt4_sd_play(gt4_sd *sd, gt4_sd_handle *handle, const gt4_race_effect *tone, int once,
                 float left, float right, float pitch, int32_t priority, int32_t pan);
/* An effect sequence's note (0055AED8 -> 00611AA0 -> 0055A8B0): group 2,
   the note itself, `velocity` through the bank's table (identity on this
   disc), the bank's volume (+0x10, 127), the handle's left/right/pitch. */
void gt4_sd_note(gt4_sd *sd, const gt4_race_effect *tone, unsigned note, unsigned velocity,
                 float left, float right, float pitch, int32_t priority, int32_t pan);
/* 00392FC8 -> 0055A258: a handle's voice keyed off and freed. */
void gt4_sd_stop(gt4_sd *sd, gt4_sd_handle *handle);
/* 00559470/00558FD8: an ENGN controller's tick with the request's
   rpm, left/right, pitch factor, priority and reverb switch. */
void gt4_sd_engine_tick(gt4_sd *sd, gt4_sd_engine *engine, int rpm, int16_t left, int16_t right,
                        int32_t scale, int32_t priority, int reverb_off, const int16_t *sine);
void gt4_sd_engine_stop(gt4_sd *sd, gt4_sd_engine *engine);
/* One driver tick: effect voices (0055A430 -> 00559EB0), voices whose
   sound ended freed (`ended`: the SPU's, per hardware voice), then the
   registers (0055D890) into `regs`. */
void gt4_sd_tick(gt4_sd *sd, const uint8_t ended[GT4_SD_VOICES], gt4_spu_regs regs[GT4_SD_VOICES]);
#endif
