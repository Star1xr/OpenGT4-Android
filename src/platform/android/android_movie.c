/*
 * Menu films (the disc's MPEG-2 .pss) need the embedded FFmpeg build, which has
 * no Android port yet. Until then the game runs with the films skipped: the
 * layer draws nothing and a request reports itself unavailable, so the menu
 * advances exactly as it does when a film cannot be loaded.
 */
#include "movie.h"

sdl_movie *sdl_movie_open(const uint8_t *data, size_t size, int interlace, int repeat, int audio_track)
{
    (void)data; (void)size; (void)interlace; (void)repeat; (void)audio_track;
    return NULL;
}

void sdl_movie_audio(sdl_movie *m, int enabled) { (void)m; (void)enabled; }

int sdl_movie_draw(sdl_movie *m, float x, float y, float w, float h, float opacity)
{
    (void)m; (void)x; (void)y; (void)w; (void)h; (void)opacity;
    return 0;
}

uint64_t sdl_movie_frame_count(const sdl_movie *m) { (void)m; return 0; }

int sdl_movie_finished(sdl_movie *m) { (void)m; return 1; }

void sdl_movie_close(sdl_movie *m) { (void)m; }

void sdl_movie_release_texture(sdl_movie *m) { (void)m; }

int sdl_movie_ready(sdl_movie *m) { (void)m; return 0; }
