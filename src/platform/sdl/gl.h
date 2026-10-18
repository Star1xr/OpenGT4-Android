/*
 * OpenGL entry points shared by the desktop and the Android backends.
 *
 * Desktop: GL 3.3 core. GL 1.1 functions come from the system library
 * (opengl32 on Windows); later ones are loaded at run time through
 * SDL_GL_GetProcAddress by gl_load().
 *
 * Android: GLES 3.2. Its entry points are linked directly, so the table below
 * stays empty and gl_load() has nothing to load. The drawing and shader code
 * is the same on both platforms (render_gl.c, render3d.c); only a few calls
 * and enums differ, and they are mapped here.
 */
#ifndef OPENGT4_GL_H
#define OPENGT4_GL_H

#ifdef __ANDROID__

#include <GLES3/gl32.h>

#ifndef OPENGT4_GLES
#define OPENGT4_GLES 1
#endif
/* GLES clears depth with a float; callers only pass the value. */
#define glClearDepth glClearDepthf
/* The table is only needed where the functions are resolved at run time. */
#define OPENGT4_GL_FUNCS(X)

#else

#include <SDL.h>
#include <SDL_opengl.h>

#define OPENGT4_GL_FUNCS(X) \
    X(PFNGLACTIVETEXTUREPROC, opengt4_glActiveTexture) \
    X(PFNGLATTACHSHADERPROC, glAttachShader) \
    X(PFNGLBINDBUFFERPROC, glBindBuffer) \
    X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer) \
    X(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer) \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) \
    X(PFNGLBLENDCOLORPROC, opengt4_glBlendColor) \
    X(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer) \
    X(PFNGLBLENDEQUATIONSEPARATEPROC, glBlendEquationSeparate) \
    X(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate) \
    X(PFNGLBUFFERDATAPROC, glBufferData) \
    X(PFNGLBUFFERSUBDATAPROC, glBufferSubData) \
    X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus) \
    X(PFNGLCOMPILESHADERPROC, glCompileShader) \
    X(PFNGLCREATEPROGRAMPROC, glCreateProgram) \
    X(PFNGLCREATESHADERPROC, glCreateShader) \
    X(PFNGLDELETEBUFFERSPROC, glDeleteBuffers) \
    X(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers) \
    X(PFNGLDELETEPROGRAMPROC, glDeleteProgram) \
    X(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers) \
    X(PFNGLDELETESHADERPROC, glDeleteShader) \
    X(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays) \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
    X(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer) \
    X(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D) \
    X(PFNGLGENBUFFERSPROC, glGenBuffers) \
    X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) \
    X(PFNGLGENERATEMIPMAPPROC, glGenerateMipmap) \
    X(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers) \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) \
    X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) \
    X(PFNGLGETPROGRAMIVPROC, glGetProgramiv) \
    X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) \
    X(PFNGLGETSHADERIVPROC, glGetShaderiv) \
    X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) \
    X(PFNGLLINKPROGRAMPROC, glLinkProgram) \
    X(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage) \
    X(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC, glRenderbufferStorageMultisample) \
    X(PFNGLSHADERSOURCEPROC, glShaderSource) \
    X(PFNGLUNIFORM1FPROC, glUniform1f) \
    X(PFNGLUNIFORM1IPROC, glUniform1i) \
    X(PFNGLUNIFORM2FPROC, glUniform2f) \
    X(PFNGLUNIFORM2IPROC, glUniform2i) \
    X(PFNGLUNIFORM3FPROC, glUniform3f) \
    X(PFNGLUNIFORM2FVPROC, glUniform2fv) \
    X(PFNGLUNIFORM3FVPROC, glUniform3fv) \
    X(PFNGLUNIFORM4FPROC, glUniform4f) \
    X(PFNGLUNIFORM4IPROC, glUniform4i) \
    X(PFNGLUNIFORM4UIPROC, glUniform4ui) \
    X(PFNGLUNIFORMMATRIX3FVPROC, glUniformMatrix3fv) \
    X(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv) \
    X(PFNGLUSEPROGRAMPROC, glUseProgram) \
    X(PFNGLVERTEXATTRIBIPOINTERPROC, glVertexAttribIPointer) \
    X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer)

#define OPENGT4_GL_DECLARE(type, name) extern type name;
OPENGT4_GL_FUNCS(OPENGT4_GL_DECLARE)
#undef OPENGT4_GL_DECLARE
/* SDL_opengl.h declares these two as functions: use our loaded pointers. */
#define glActiveTexture opengt4_glActiveTexture
#define glBlendColor opengt4_glBlendColor

#endif /* __ANDROID__ */

/* Desktop: loads the entry points for the current context; 0 if any is
   missing. Android: the symbols are linked, nothing to load. */
#ifdef OPENGT4_GLES
static inline int gl_load(void) { return 1; }
#else
int gl_load(void);
#endif
/* Reversed depth (OpenGL 4.5 / ARB_clip_control): far 0, near 1 in a float
   buffer, nearly even precision with distance. 0 keeps the classic
   [-1, 1] depth in 24 bits. The Android port always uses the classic path.
   Set once by the backend after the context. */
extern int gl_reversed_z;
int gl_enable_reversed_z(void);
#define GL_DEPTH_FUNC_LESS   (gl_reversed_z ? GL_GREATER : GL_LESS)
#define GL_DEPTH_FUNC_LEQUAL (gl_reversed_z ? GL_GEQUAL : GL_LEQUAL)
#define GL_DEPTH_FAR         (gl_reversed_z ? 0.0 : 1.0)
/* Compiles and links a vertex/fragment program; 0 on error (logged). */
GLuint gl_program(const char *vertex, const char *fragment);

#endif
