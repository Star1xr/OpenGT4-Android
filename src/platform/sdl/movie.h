#ifndef OPENGT4_SDL_MOVIE_H
#define OPENGT4_SDL_MOVIE_H
#include <stddef.h>
#include <stdint.h>
typedef struct sdl_movie sdl_movie;
/* Silent menu MPEG-2 video. Decoding runs off the render/audio threads. */
sdl_movie *sdl_movie_open(const uint8_t *data,size_t size,int interlace,int repeat,int audio_track);
/* Called only by the rendering thread. Disabling closes the device before
   the decoder can be handed to the disposal worker. */
void sdl_movie_audio(sdl_movie *m,int enabled);
int sdl_movie_draw(sdl_movie *m,float x,float y,float w,float h,float opacity);
uint64_t sdl_movie_frame_count(const sdl_movie *m);
int sdl_movie_finished(sdl_movie *m);
void sdl_movie_close(sdl_movie *m);

/* Release renderer-owned resources on the main thread before worker disposal. */
void sdl_movie_release_texture(sdl_movie *m);
int sdl_movie_ready(sdl_movie *m);

#endif
