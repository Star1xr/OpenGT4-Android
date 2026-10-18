#include "gl.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <SDL.h>

#ifdef OPENGT4_GLES

/* GLES 3.2 links the entry points directly. The Android port keeps the
   classic 24-bit depth path, so there is no clip control to enable. */
int gl_reversed_z;
int gl_enable_reversed_z(void) { return gl_reversed_z = 0; }

/* ES has no noperspective qualifier at all; GL_NV_shader_noperspective_
   interpolation is the only way to get it back. Asked once per process. */
int gl_noperspective_supported(void)
{
    static int supported = -1;
    if (supported < 0) supported = SDL_GL_ExtensionSupported("GL_NV_shader_noperspective_interpolation") ? 1 : 0;
    return supported;
}

#else

/* Desktop GL 3.3 has noperspective in core. */
int gl_noperspective_supported(void) { return 1; }

#define OPENGT4_GL_DEFINE(type, name) type name;
OPENGT4_GL_FUNCS(OPENGT4_GL_DEFINE)
#undef OPENGT4_GL_DEFINE

int gl_load(void)
{
    int ok = 1;
#define OPENGT4_GL_LOAD(type, name) \
    name = (type)SDL_GL_GetProcAddress(strncmp(#name, "opengt4_", 8) ? #name : #name + 8); \
    if (!name) { fprintf(stderr, "OpenGL: missing %s\n", #name); ok = 0; }
    OPENGT4_GL_FUNCS(OPENGT4_GL_LOAD)
#undef OPENGT4_GL_LOAD
    return ok;
}

int gl_reversed_z;
int gl_enable_reversed_z(void)
{
    typedef void (APIENTRY *clip_control_fn)(GLenum, GLenum);
    clip_control_fn clip_control = (clip_control_fn)SDL_GL_GetProcAddress("glClipControl");
    if (!clip_control || getenv("OPENGT4_CLASSIC_DEPTH")) return gl_reversed_z = 0;
    clip_control(0x8CA1 /* GL_LOWER_LEFT */, 0x935F /* GL_ZERO_TO_ONE */);
    if (glGetError() != GL_NO_ERROR) { clip_control(0x8CA1, 0x935E /* NEGATIVE_ONE_TO_ONE */); return gl_reversed_z = 0; }
    gl_reversed_z = 1;
    glDepthFunc(GL_GREATER);
    glClearDepth(0);
    return 1;
}

#endif /* OPENGT4_GLES */

static GLuint shader(GLenum kind, const char *source)
{
    GLuint s = glCreateShader(kind);
    glShaderSource(s, 1, &source, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof log, NULL, log);
        fprintf(stderr, "OpenGL shader: %s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint gl_program(const char *vertex, const char *fragment)
{
    GLuint v = shader(GL_VERTEX_SHADER, vertex), f = shader(GL_FRAGMENT_SHADER, fragment);
    if (!v || !f) { if (v) glDeleteShader(v); if (f) glDeleteShader(f); return 0; }
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, sizeof log, NULL, log);
        fprintf(stderr, "OpenGL program: %s\n", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}
