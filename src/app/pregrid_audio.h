#ifndef OPENGT4_PREGRID_AUDIO_H
#define OPENGT4_PREGRID_AUDIO_H
#include <stdint.h>
typedef struct pregrid_audio pregrid_audio;
pregrid_audio *pregrid_audio_create(const char *disc);
/* Called by the frontend, never by the audio callback. Empty group suspends. */
void pregrid_audio_update(pregrid_audio *,const char *group,int raw_volume,uint32_t clock);
void pregrid_audio_update_ex(pregrid_audio *,const char *group,int raw_volume,uint32_t clock,int sequence,int repeat,unsigned serial);
/* Preparation continues under the loader; publish only after its fade ends. */
void pregrid_audio_update_gated(pregrid_audio *,const char *group,int raw_volume,uint32_t clock,int sequence,int repeat,unsigned serial,int publish);
int pregrid_audio_playing(pregrid_audio *);
int pregrid_audio_prepared(pregrid_audio *);
/* sound.pauseStream: silence without moving the stream */
void pregrid_audio_pause(pregrid_audio *,int paused);
void pregrid_audio_close(pregrid_audio *);
#endif
