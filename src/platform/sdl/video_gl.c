/*
 * Desktop video for the SDL backend: the window and its OpenGL 3.3/4.3 core
 * context, the FSR 1 scene resolver and the present. The drawing itself is the
 * shared renderer in render_gl.c, the same code the Android build runs against
 * GLES 3.2.
 *
 * Every frame is drawn into an offscreen framebuffer the size of the window's
 * drawable and copied to the window at the end, so captures read the same
 * pixels with a visible or a hidden window.
 */
#include "platform/platform.h"
#include "plat_sdl.h"
#include "video_gl.h"
#include "gl.h"
#include "fsr.h"

#include <stdio.h>
#include <stdlib.h>

static SDL_Window   *window;
static SDL_GLContext context;
static int           vsync;

SDL_Window *plat_sdl_window(void) { return window; }

int plat_video_init(const plat_video_desc *d)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) return -1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    /* P: optional placement for filming on the user's secondary monitor.
       Ordinary launches retain SDL's centered window. */
    int window_x=SDL_WINDOWPOS_CENTERED,window_y=SDL_WINDOWPOS_CENTERED;
    const char *capture_position=getenv("OPENGT4_WINDOW_POSITION");
    if(capture_position && sscanf(capture_position,"%d,%d",&window_x,&window_y)==2)
        SDL_SetHint(SDL_HINT_WINDOW_NO_ACTIVATION_WHEN_SHOWN,"1");
    window = SDL_CreateWindow("OpenGT4", window_x, window_y, d->width, d->height,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI |
                              (d->fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
    if (!window) return -1;
    /* 4.3 for the FSR shaders; 3.3 is enough for everything else */
    static const int versions[2][2] = { { 4, 3 }, { 3, 3 } };
    for (int i = 0; i < 2 && !context; i++) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, versions[i][0]);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, versions[i][1]);
        context = SDL_GL_CreateContext(window);
    }
    if (!context || !gl_load()) return -1;
    if (gl_enable_reversed_z()) fprintf(stderr, "OpenGL: reversed float depth\n");
    video_gl_set_resolver(fsr_resolve, fsr_available);
    vsync = d->vsync;
    SDL_GL_SetSwapInterval(vsync ? 1 : 0);
    int w = 0, h = 0;
    SDL_GL_GetDrawableSize(window, &w, &h);
    video_gl_set_drawable(w, h);
    return video_gl_start(d->upscale) ? 0 : -1;
}

void plat_video_begin_frame(void)
{
    int w = 0, h = 0;
    if (window) SDL_GL_GetDrawableSize(window, &w, &h);
    video_gl_set_drawable(w, h);
    video_gl_frame_begin();
}

void plat_video_end_frame(void)
{
    video_gl_frame_end();
    if (window && video_gl_should_present()) SDL_GL_SwapWindow(window);
}

void plat_video_shutdown(void)
{
    plat_audio_shutdown();
    plat_input_shutdown();
    video_gl_shutdown();
    if (context) SDL_GL_DeleteContext(context);
    if (window) SDL_DestroyWindow(window);
    context = NULL;
    window = NULL;
    SDL_Quit();
}

int video_set_vsync(int enabled)
{
    if (SDL_GL_SetSwapInterval(enabled ? 1 : 0) < 0) return 0;
    vsync = enabled != 0;
    return 1;
}
