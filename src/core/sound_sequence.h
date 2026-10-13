#ifndef OPENGT4_SOUND_SEQUENCE_H
#define OPENGT4_SOUND_SEQUENCE_H
#include <stdint.h>
#include <stddef.h>
#include "vol.h"
typedef struct {uint8_t pan[256],octave[768];} gt4_se_tables;
int gt4_se_tables_load(gt4_vol *vol,gt4_se_tables *out);
int gt4_sound_render_service(const uint8_t *bank,size_t bytes,unsigned sequence,const gt4_se_tables *tables,
                            const uint8_t *libsd,size_t libsd_size,int16_t **pcm,size_t *frames);
typedef struct {float target,step,fraction;int32_t done;} gt4_se_ramp;
/* 0055A858 +0x24..0x50: resolution is ticks/quarter note, not ticks/s. */
float gt4_se_rate(float tempo,float resolution,float frequency);
/* 0055AB98 +0x60..0xe0: accumulate before dispatch, including frame 0. */
float gt4_se_clock(float elapsed,float rate);
int gt4_se_due(float elapsed,float due);
/* Default 120 BPM, 240 ppqn, 60 Hz; first frame dispatches ticks 0..8. */
size_t gt4_se_sample_time(unsigned ticks);
/* 0055D420 / 0055D470: integer automation published once per driver tick. */
void gt4_se_ramp_start(gt4_se_ramp *r,int milliseconds,int target,int current,float frequency);
int gt4_se_ramp_step(gt4_se_ramp *r,int32_t *value);
#endif
