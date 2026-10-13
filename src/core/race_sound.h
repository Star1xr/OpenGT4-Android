#ifndef OPENGT4_RACE_SOUND_H
#define OPENGT4_RACE_SOUND_H
#include <stddef.h>
#include <stdint.h>
#include "vol.h"
#include "spu.h"

typedef struct {
    int16_t *pcm;
    size_t frames, loop;
    int repeat;
    gt4_spu_wave wave;              /* owned copy of the ADPCM blocks */
} gt4_race_sample;
typedef struct {
    int16_t rpm, minimum, maximum, gain;
    uint32_t pitch, offset;
    gt4_race_sample sample;
} gt4_engine_layer;
typedef struct {
    gt4_engine_layer *layers;
    unsigned count;
} gt4_engine_bank;
typedef struct {
    int layer;
    uint32_t pitch;
    int16_t left, right;
} gt4_engine_voice;
typedef struct { gt4_engine_voice voice[2]; } gt4_engine_control;
typedef struct {
    char name[48];
    unsigned bank, program, note, kind;
    unsigned root, flags, volume, pan, range;
    int fine;
    uint16_t adsr1, adsr2;
    gt4_race_sample sample;
} gt4_race_effect;
typedef struct {
    gt4_race_effect *effects;
    unsigned count;
} gt4_race_effects;
typedef struct { uint64_t position; int ended; } gt4_race_cursor;

int gt4_race_sample_decode(gt4_race_sample *out,const uint8_t *data,size_t bytes);
void gt4_race_sample_clear(gt4_race_sample *sample);
gt4_engine_bank *gt4_engine_bank_decode(const uint8_t *data,size_t bytes,unsigned bank);
gt4_engine_bank *gt4_engine_bank_open(gt4_vol *vol,unsigned id,int variant);
void gt4_engine_bank_close(gt4_engine_bank *bank);
void gt4_engine_control_init(gt4_engine_control *control);
void gt4_engine_control_step(const gt4_engine_bank *bank,gt4_engine_control *control,
                             int rpm,int16_t left,int16_t right,const int16_t sine[4096]);
/* The same, told which slots still hold their hardware voice. */
void gt4_engine_control_step_ex(const gt4_engine_bank *bank,gt4_engine_control *control,
                                int rpm,int16_t left,int16_t right,const int16_t sine[4096],const int voiced[2]);
gt4_race_effects *gt4_race_effects_open(gt4_vol *vol);
/* One program's tone for `note` from an INST image (00559B10, 00559D90). */
int gt4_race_tone_open(gt4_race_effect *tone,const uint8_t *ins,size_t size,unsigned program,unsigned note);
const gt4_race_effect *gt4_race_effect_find(const gt4_race_effects *bank,const char *name);
void gt4_race_effects_close(gt4_race_effects *bank);
/* Add to a caller-owned 32-bit stereo mix. Pitch 4096 is SPU unity.
   No allocation or file access in the audio producer. */
void gt4_race_sample_mix(const gt4_race_sample *sample,gt4_race_cursor *cursor,
                         uint32_t pitch,int16_t left,int16_t right,int32_t *mix,int frames);

/* The game's sound driver (EE side): a hardware voice object (0xE0 bytes,
   D_006518C8 +0xA8) as 0055D890 reads it to make the SPU2 registers. */
typedef struct {
    int32_t group;                  /* +0x30: 4 ENGN, 5 tones */
    int32_t master, group2, level;  /* +0x4C, +0x50, +0x54 */
    int32_t v58, v5c, v60;          /* +0x58, +0x5C, +0x60 */
    int16_t left, right;            /* +0x64, +0x66 */
    int32_t pan, pan6c, pan70;      /* +0x68, +0x6C, +0x70 */
    int32_t scale;                  /* +0x74: pitch factor, 4096 = 1 */
    int32_t root, note, fine;       /* +0x78 (x16), +0x7C (x16), +0x80 */
    int32_t range, bend;            /* +0x84, +0x88 */
    uint16_t pitch;                 /* +0x90 */
    uint16_t adsr1, adsr2;          /* +0x92, +0x94 */
    uint8_t reverb;                 /* +0x25 */
} gt4_sd_voice;
/* 0055D890 +0x118..+0x2C8: VOLL/VOLR (before the 7FFFh mask) from the
   voice, its group's volume (manager +0x4C + group*12), the manager's
   stereo flag (+0) and the pan table D_006C8AC0. */
void gt4_sd_volume(const gt4_sd_voice *voice,int32_t group_volume,int stereo,
                   const uint8_t *pan_table,int16_t out[2]);
/* 0055D890 +0x420..+0x4D8: +0x90 from the note, root, fine tune and
   bend through the octave table D_00654738 (192 u32). */
uint16_t gt4_sd_pitch(const gt4_sd_voice *voice,const uint8_t *octave_table);
/* 0055DF70 +0x134..+0x144: the PITCH register, +0x90 times +0x74. */
uint16_t gt4_sd_pitch_register(const gt4_sd_voice *voice);
/* 00559EB0 +0xA4..+0x2A4 and 0055D0B0's defaults: the voice a tone gets
   when an effect handle keys it, with that handle's left/right
   (x16383), pitch factor (x4096) and pan. `once` is 00462A88's a2. */
void gt4_sd_voice_tone(gt4_sd_voice *voice,const gt4_race_effect *tone,int16_t left,int16_t right,
                       int32_t scale,int32_t pan,int once);
#endif
