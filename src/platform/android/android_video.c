/*
 * Android video: SDL2 penceresi + EGL/GLES baglami.
 *
 * Faz 1: baglam kurulumu, ES 3.2 kapi kontrolu, temizle/sun.
 * Faz 3: 2B yigili cizim ve 3B sahne buraya gelecek; paylasilan GL kaynagi
 *        src/platform/gl/* altinda olacak. Bu dosyada KASITLI olarak eksik
 *        birakilan semboller (plat_draw_*, plat_texture_*, plat_set_clip...)
 *        Faz 3'te eklenecek: linker hatasi versin, sessiz siyah ekran olmasin.
 * FSR/upscale yolu yoktur (saf port, AGENTS.md karari): desc->upscale yok sayilir.
 */
#include "platform/platform.h"
#include "android_platform.h"

#include <SDL.h>
#include <GLES3/gl3.h>
#include <stdio.h>

static SDL_Window   *window;
static SDL_GLContext context;
static int es_major, es_minor, es32;
static char gl_info[320];
static float view_w = 640.0f, view_h = 448.0f;

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
    /* Android'de boyut tam ekrandan gelir; masaustu pencere boyutu sadece ipucu. */
    int w = view_w > 0 ? (int)view_w : 1280, h = view_h > 0 ? (int)view_h : 720;
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
        ALOGE("SDL_Init basarisiz: %s", SDL_GetError());
        return -1;
    }
    if (desc && desc->width > 0 && desc->height > 0) {
        view_w = (float)desc->width;
        view_h = (float)desc->height;
    }
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    if (!create_window()) {
        ALOGE("pencere acilamadi: %s", SDL_GetError());
        return -1;
    }

    /* Kapi: ES 3.2 istenir; surucu veremezse ES 3.0 + uyari (AGENTS.md). */
    context = create_context(3, 2);
    if (context) {
        es_major = 3;
        es_minor = 2;
    } else {
        ALOGW("GLES 3.2 baglami kurulamadi (%s); ES 3.0 fallback", SDL_GetError());
        context = create_context(3, 0);
        if (context) {
            es_major = 3;
            es_minor = 0;
        }
    }
    if (!context) {
        ALOGE("GLES baglami kurulamadi (ES 3.0 da yok): %s", SDL_GetError());
        return -1;
    }
    if (SDL_GL_MakeCurrent(window, context) != 0) {
        ALOGE("SDL_GL_MakeCurrent: %s", SDL_GetError());
        return -1;
    }

    /* Surucu istenenden farkli bir surum verdiyse gercegini olc. */
    GLint major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    if (major > 0) {
        es_major = (int)major;
        es_minor = (int)minor;
    }
    es32 = es_major > 3 || (es_major == 3 && es_minor >= 2);
    if (!es32)
        ALOGW("gercek baglam ES %d.%d: noperspective yok, smooth ile devam edilecek", es_major, es_minor);

    const int nv_nopersp = SDL_GL_ExtensionSupported("GL_NV_shader_noperspective_interpolation");
    snprintf(gl_info, sizeof gl_info, "ES %d.%d | %s | %s | noperspective:%s",
             es_major, es_minor, glstr(GL_VENDOR), glstr(GL_RENDERER), nv_nopersp ? "NV" : "smooth");
    ALOGI("GLES %d.%d | GLSL %s", es_major, es_minor, glstr(GL_SHADING_LANGUAGE_VERSION));
    ALOGI("vendor=%s renderer=%s", glstr(GL_VENDOR), glstr(GL_RENDERER));
    ALOGI("noperspective interpolation: %s", nv_nopersp ? "NV uzantisi" : "yok (smooth kullanilacak)");

    SDL_GL_SetSwapInterval(desc && desc->vsync ? 1 : 0);
    (void)desc->upscale; /* FSR yok: Android'de her zaman native */
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0.06f, 0.07f, 0.09f, 1.0f);
    return 0;
}

void plat_video_set_view(float width, float height)
{
    if (width > 0.0f) view_w = width;
    if (height > 0.0f) view_h = height;
}

void plat_video_begin_frame(void)
{
    int w = 0, h = 0;
    if (window) SDL_GL_GetDrawableSize(window, &w, &h);
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void plat_video_end_frame(void)
{
    if (window) SDL_GL_SwapWindow(window);
}

void plat_video_shutdown(void)
{
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
}
