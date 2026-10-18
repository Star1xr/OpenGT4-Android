/*
 * Android video: the SDL2 window, the EGL/GLES context and the present.
 *
 * The drawing is the shared renderer in render_gl.c under src/platform/sdl/,
 * the same code the desktop build runs, so the 2D path, the scene target and
 * the shaders have one source. Only the context (GLES 3.2 instead of desktop
 * OpenGL) and the present differ here.
 *
 * There is no FSR/upscale path (pure port): desc->upscale is ignored and no
 * scene resolver is registered, so video_end_scene copies the scene as is.
 * desc->width/height (the game's Resolution setting) is the size 3D scenes are
 * drawn at, and the scene is blitted up to the panel from there.
 *
 * The on-screen PS2 pad (android_touch.c) is the last layer of every frame and
 * the last thing shutdown takes down.
 */
#include "platform/platform.h"
#include "android_platform.h"
#include "video_gl.h"
#include "render3d.h"

#include <SDL.h>
#include <stdarg.h>
#include <stdio.h>

/* logcat needs adb and the game's own log is stderr, so every line from the
   Android files goes to both: the launcher can share the file, nobody can share
   logcat. */
void plat_android_log(int priority, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    __android_log_vprint(priority, OPENGT4_ANDROID_LOG_TAG, format, ap);
    va_end(ap);
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
}

#undef ALOGI
#undef ALOGW
#undef ALOGE
#define ALOGI(...) plat_android_log(ANDROID_LOG_INFO, __VA_ARGS__)
#define ALOGW(...) plat_android_log(ANDROID_LOG_WARN, __VA_ARGS__)
#define ALOGE(...) plat_android_log(ANDROID_LOG_ERROR, __VA_ARGS__)

static SDL_Window   *window;
static SDL_GLContext context;
static int es_major, es_minor, es32;
static char gl_info[320];
static int display_w = 1280, display_h = 720;

int plat_android_es_major(void) { return es_major; }
int plat_android_es_minor(void) { return es_minor; }
int plat_android_es32(void) { return es32; }
const char *plat_android_gl_info(void) { return gl_info; }

static const char *glstr(GLenum name)
{
    const GLubyte *s = glGetString(name);
    return s ? (const char *)s : "?";
}

static int create_window(void)
{
    /* Android takes the size from the display; the window size is only a hint. */
    int w = display_w, h = display_h;
    SDL_DisplayMode mode;
    if (SDL_GetCurrentDisplayMode(0, &mode) == 0 && mode.w > 0 && mode.h > 0) {
        w = mode.w;
        h = mode.h;
    }
    window = SDL_CreateWindow("OpenGT4", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_ALLOW_HIGHDPI);
    return window != NULL;
}

static SDL_GLContext create_context(int major, int minor)
{
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
    return SDL_GL_CreateContext(window);
}

int plat_video_init(const plat_video_desc *desc)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) {
        ALOGE("SDL_Init failed: %s", SDL_GetError());
        return -1;
    }
    if (desc && desc->width > 0) display_w = desc->width;
    if (desc && desc->height > 0) display_h = desc->height;
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    if (!create_window()) {
        ALOGE("window creation failed: %s", SDL_GetError());
        return -1;
    }

    /* Gate: ask for ES 3.2, fall back to ES 3.0 with a warning if the driver
       cannot provide it. */
    context = create_context(3, 2);
    if (context) {
        es_major = 3;
        es_minor = 2;
    } else {
        ALOGW("GLES 3.2 context unavailable (%s); falling back to ES 3.0", SDL_GetError());
        context = create_context(3, 0);
        if (context) {
            es_major = 3;
            es_minor = 0;
        }
    }
    if (!context) {
        ALOGE("no GLES context at all (ES 3.0 failed too): %s", SDL_GetError());
        return -1;
    }
    if (SDL_GL_MakeCurrent(window, context) != 0) {
        ALOGE("SDL_GL_MakeCurrent: %s", SDL_GetError());
        return -1;
    }

    /* A driver may hand out a different version than the one requested: measure
       what is actually current. */
    GLint major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    if (major > 0) {
        es_major = (int)major;
        es_minor = (int)minor;
    }
    es32 = es_major > 3 || (es_major == 3 && es_minor >= 2);
    if (!es32)
        ALOGW("GLES 3.2 unavailable (ES %d.%d); using the ES 3.0 feature set", es_major, es_minor);

    const int nv_nopersp = gl_noperspective_supported();
    snprintf(gl_info, sizeof gl_info, "ES %d.%d | %s | %s | noperspective:%s",
             es_major, es_minor, glstr(GL_VENDOR), glstr(GL_RENDERER), nv_nopersp ? "NV advertised" : "smooth");
    ALOGI("GLES %d.%d | GLSL %s", es_major, es_minor, glstr(GL_SHADING_LANGUAGE_VERSION));
    ALOGI("vendor=%s renderer=%s", glstr(GL_VENDOR), glstr(GL_RENDERER));
    /* The extension may still be refused at compile time; render3d.c says so. */
    ALOGI("noperspective: %s", nv_nopersp ? "NV extension advertised" : "no, smooth varyings");

    SDL_GL_SetSwapInterval(desc && desc->vsync ? 1 : 0);
    (void)(desc ? desc->upscale : 0); /* no FSR: upscaling stays unused here */

    int w = 0, h = 0;
    SDL_GL_GetDrawableSize(window, &w, &h);
    video_gl_set_drawable(w, h);
    if (video_gl_start(0) != 0) {
        ALOGE("shared renderer init failed (2D program or frame target)");
        return -1;
    }

    /* The game's Resolution setting (video.ini width/height, the same field the
       PC settings page calls Resolution) is the size the GPU draws 3D scenes
       at; the blit scales it to the panel. MSAA stays off: at panel size its
       resolves cost more than they are worth on a phone GPU. */
    video_set_render_size(desc ? desc->width : 0, desc ? desc->height : 0);
    video_set_samples(1);
    ALOGI("render: drawable %dx%d, scene %dx%d, MSAA off, vsync %d", w, h,
          desc && desc->width > 0 ? desc->width : w, desc && desc->height > 0 ? desc->height : h,
          SDL_GL_GetSwapInterval());

    /* The touch PS2 controls; they draw with the shared 2D renderer, so the
       atlas is built here, while the context is current. */
    plat_android_touch_init();
    /* Android has no adb at hand while playing, so the frame cost goes to the
       game's own log: CPU submission every frame, GPU on a sample of frames. */
    video_set_profile(1);
    return 0;
}

SDL_Window *plat_sdl_window(void) { return window; }

void plat_video_begin_frame(void)
{
    int w = 0, h = 0;
    if (window) SDL_GL_GetDrawableSize(window, &w, &h);
    video_gl_set_drawable(w, h);
    /* The frame's fingers become the pad's buttons before the game reads it. */
    plat_android_touch_update();
    video_gl_frame_begin();
}

/* The log file is the only place a player can see how a scene really runs, so
   one line every five seconds with the frame rate, the worst frame, and what
   the 3D pass of the last frame drew (r3d_begin resets those counters). */
static void frame_stats(void)
{
    static Uint64 window_start, previous;
    static unsigned frames, worst;
    const double frequency = (double)SDL_GetPerformanceFrequency();
    Uint64 now = SDL_GetPerformanceCounter();
    if (!window_start) window_start = now;
    if (previous) {
        Uint64 delta = now - previous;
        if (delta > worst) worst = (unsigned)delta;
    }
    previous = now;
    frames++;
    if (now - window_start < (Uint64)(5.0 * frequency)) return;
    double seconds = (now - window_start) / frequency;
    r3d_stats drew = r3d_frame_stats();
    fprintf(stderr, "frame: %u in %.1fs, %.1f fps, mean %.1f ms, max %.1f ms,"
                    " last frame %u draws, %u triangles, %u groups culled\n",
            frames, seconds, frames / seconds, seconds * 1000.0 / frames, worst * 1000.0 / frequency,
            drew.draws, drew.triangles, drew.culled_groups);
    fflush(stderr);
    window_start = now; frames = 0; worst = 0;
}

void plat_video_end_frame(void)
{
    /* Last layer of the frame: the on-screen pad, at the panel resolution. */
    plat_android_touch_draw();
    video_gl_frame_end();
    if (window && video_gl_should_present()) SDL_GL_SwapWindow(window);
    frame_stats();
}

int video_set_vsync(int enabled)
{
    if (SDL_GL_SetSwapInterval(enabled ? 1 : 0) < 0) return 0;
    return 1;
}

void plat_video_shutdown(void)
{
    plat_audio_shutdown();
    plat_input_shutdown();
    plat_android_touch_shutdown();
    video_gl_shutdown();
    if (context) {
        SDL_GL_DeleteContext(context);
        context = NULL;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    es_major = es_minor = es32 = 0;
    gl_info[0] = 0;
    SDL_Quit();
}
