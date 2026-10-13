#ifndef GT4_PSS_AUDIO_H
#define GT4_PSS_AUDIO_H
#include <stddef.h>
#include <stdint.h>

/* Extract one PS2 private audio track (1-based) and decode its stereo 48 kHz
   SShd ADPCM stream. Caller owns PCM. No looping, device or disc access. */
int gt4_pss_audio(const uint8_t *data,size_t size,unsigned track,
                  int16_t **pcm,size_t *frames);
#endif
