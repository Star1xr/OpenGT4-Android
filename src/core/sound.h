#ifndef OPENGT4_SOUND_H
#define OPENGT4_SOUND_H
#include "vol.h"
#include <stdint.h>
typedef struct gt4_sound gt4_sound;
gt4_sound *gt4_sound_open(gt4_vol *vol);
void gt4_sound_close(gt4_sound *s);
int gt4_sound_play(gt4_sound *s, const char *name);
/* 00462EC8: load gt4shutter<lowercase option>.ins in bank 4. */
int gt4_sound_shutter(gt4_sound *s,const char *kind);
void gt4_sound_gain(gt4_sound *s, float gain);
/* Bounded Sony ADPCM sample decoder, no device state. Caller frees PCM. */
int gt4_adpcm_decode(const uint8_t *data, size_t size, int16_t **pcm, size_t *frames);
/* Offline render for validation and the runtime; stereo 48000 Hz, caller owns. */
int gt4_sound_render_extended(const uint8_t *bank,size_t bytes,unsigned sequence,int16_t **pcm,size_t *frames);
int gt4_sound_render(const uint8_t *bank, size_t size, unsigned sequence,
                     int16_t **pcm, size_t *frames);
#endif
