#ifndef OPENGT4_PCM_MUSIC_H
#define OPENGT4_PCM_MUSIC_H
#include "vol.h"
typedef struct gt4_pcm_music gt4_pcm_music;
/* Original 004379F0 jukebox/save object (0x810 bytes). */
typedef struct gt4_music_playlist { uint8_t bytes[0x810]; } gt4_music_playlist;
int gt4_music_playlist_uses_preferences(unsigned playback_mode,int override);
void gt4_music_playlist_order(gt4_music_playlist *p,int shuffle);
int gt4_music_playlist_next(gt4_music_playlist *p);
int gt4_music_playlist_init(gt4_music_playlist *p,gt4_vol *vol,uint32_t seed);
int gt4_music_playlist_default(gt4_music_playlist *p,gt4_vol *vol,uint32_t seed);
int gt4_music_playlist_slide(gt4_music_playlist *p,gt4_vol *vol,uint32_t seed);
/* config-main init_baseplaylist: the Jukebox library's racing indices */
unsigned gt4_music_library_default(gt4_vol *vol,unsigned *out,unsigned capacity);
int gt4_pcm_music_info(gt4_vol *vol,unsigned index,char out[3][128]);
/* Session +D4/+D8: 003C0F40, 003C04E8, 003C0FB0. */
void gt4_music_fade_start(float *remaining,float *duration,float seconds,float scale);
int gt4_music_fade_step(float *remaining,float dt);
float gt4_music_fade_gain(float remaining,float duration);
int gt4_music_gain_q15(float gain);
float gt4_music_stream_gain(float base,float external,int enabled);
void gt4_music_replay_fade(float *remaining,float *duration,int32_t frames_left,unsigned mode,int timed_exit,int looping);
int gt4_music_playlist_sequence(gt4_vol *vol,const gt4_music_playlist *p,unsigned entry);
unsigned gt4_pcm_music_count(gt4_vol *vol,const char *group);
/* Default race playlist membership (00437B10), as ADS group indices. */
unsigned gt4_pcm_music_race_sequences(gt4_vol *vol,unsigned *sequences,unsigned capacity);
gt4_pcm_music *gt4_pcm_music_open(gt4_vol *vol,const char *group,unsigned sequence);
gt4_pcm_music *gt4_pcm_music_decode(const uint8_t *data,size_t bytes);
uint32_t gt4_pcm_music_random(uint32_t *state,uint32_t clock);
float gt4_pcm_music_random_unit(uint32_t *state);
void gt4_pcm_music_render(int16_t *out,int frames,gt4_pcm_music *music);
void gt4_pcm_music_rewind(gt4_pcm_music *music);
/* Explicit non-looping playback for jingles. The caller supplies the
   original stream policy; this does not infer loop metadata from .adm. */
void gt4_pcm_music_once(gt4_pcm_music *music,int once);
int gt4_pcm_music_ended(const gt4_pcm_music *music);
void gt4_pcm_music_close(gt4_pcm_music *music);
unsigned gt4_pcm_music_rate(const gt4_pcm_music *music);
size_t gt4_pcm_music_frames(const gt4_pcm_music *music);
const int16_t *gt4_pcm_music_samples(const gt4_pcm_music *music);
/* its title and artist from ads.inf ("" when none) */
const char *gt4_pcm_music_title(const gt4_pcm_music *music,const char **artist);
/* where the stream is, in seconds */
double gt4_pcm_music_seconds(const gt4_pcm_music *music);
/* its .adm's first list of marks (sample positions at *rate): the replay's
   Dive cuts on them */
unsigned gt4_pcm_music_cues(const gt4_pcm_music *music,const uint32_t **samples,unsigned *rate);
/* following the marks (the object at race +0x84 +0x74: +0x3C its place in
   the list, +0x38 the stream) */
typedef struct { int cursor, track; } gt4_music_cue;
float gt4_music_cue_step(gt4_music_cue *c,const uint32_t *cues,unsigned n,unsigned rate,float now,int track,int *passed);
#endif
