#ifndef OPENGT4_MUSIC_H
#define OPENGT4_MUSIC_H
#include "vol.h"
typedef struct gt4_music gt4_music;
gt4_music *gt4_music_decode(const uint8_t *ins,size_t ins_size,const uint8_t *sqt,size_t sqt_size);
gt4_music *gt4_music_open(gt4_vol *vol,const char *name);
unsigned gt4_music_next_sequence(unsigned current);
unsigned gt4_music_sequence_count(gt4_vol *vol,const char *name);
gt4_music *gt4_music_open_sequence(gt4_vol *vol,const char *name,unsigned sequence);
void gt4_music_render(int16_t *out,int frames,void *music);
/* Prepared menu tracks advance at end, including within one audio block. */
unsigned gt4_music_render_playlist(int16_t *out,int frames,gt4_music **tracks,
                                   unsigned count,unsigned current);
int gt4_music_prepare(gt4_music *music,double tempo);
/* Worker publishes initialized PCM prefixes; callback only copies that prefix. */
typedef void (*gt4_music_prepared)(void *user,unsigned frames);
int gt4_music_prepare_stream(gt4_music *music,double tempo,gt4_music_prepared notify,void *user);
int gt4_music_prepare_begin(gt4_music *music,double tempo);
/* Returns 1 while more PCM remains, 0 at the end. */
int gt4_music_prepare_step(gt4_music *music,gt4_music_prepared notify,void *user);
void gt4_music_render_available(int16_t *out,int frames,gt4_music *music,unsigned available);
unsigned gt4_music_render_playlist_available(int16_t *out,int frames,gt4_music **tracks,
                                   const unsigned *available,unsigned count,unsigned current);
void gt4_music_rewind(gt4_music *music);
void gt4_music_close(gt4_music *music);
#endif
