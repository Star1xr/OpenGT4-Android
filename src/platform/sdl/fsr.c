/*
 * Scene upscaling with AMD FidelityFX Super Resolution 1 (third_party/fsr1,
 * MIT): EASU then RCAS on the anti-aliased scene each frame. Source and
 * intermediate images remain on the GPU. UI is drawn afterwards at the
 * window's native drawable resolution; game textures stay unchanged.
 */
#include "fsr.h"
#include "gl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const char fsr_ffx_a[], fsr_ffx_fsr1[];
extern const size_t fsr_ffx_a_size, fsr_ffx_fsr1_size;

static GLuint easu, rcas, vao, fbo;
static int state;   /* 0 untried, 1 ready, -1 unavailable */
static GLint easu_con[4], easu_src, easu_size, rcas_con, rcas_src, rcas_unpremultiply;

static const char *vertex =
    "#version 430 core\n"
    "void main(){ vec2 p=vec2((gl_VertexID<<1)&2, gl_VertexID&2); gl_Position=vec4(p*2.0-1.0,0.0,1.0); }\n";

static const char *easu_head =
    "#version 430 core\n#define A_GPU 1\n#define A_GLSL 1\n";
static const char *easu_mid =
    "\n#define FSR_EASU_F 1\n"
    "uniform sampler2D u_src;\n"
    "AF4 FsrEasuRF(AF2 p){return textureGather(u_src,p,0);}\n"
    "AF4 FsrEasuGF(AF2 p){return textureGather(u_src,p,1);}\n"
    "AF4 FsrEasuBF(AF2 p){return textureGather(u_src,p,2);}\n";
static const char *easu_main =
    "\nuniform uvec4 u_con0,u_con1,u_con2,u_con3; uniform vec2 u_size; out vec4 o;\n"
    "void main(){ AF3 c; FsrEasuF(c,AU2(gl_FragCoord.xy),u_con0,u_con1,u_con2,u_con3);\n"
    "  o=vec4(c,texture(u_src,gl_FragCoord.xy/u_size).a); }\n";
static const char *rcas_mid =
    "\n#define FSR_RCAS_F 1\n#define FSR_RCAS_PASSTHROUGH_ALPHA 1\n"
    "uniform sampler2D u_src;\n"
    "AF4 FsrRcasLoadF(ASU2 p){return texelFetch(u_src,clamp(p,ASU2(0),textureSize(u_src,0)-1),0);}\n"
    "void FsrRcasInputF(inout AF1 r,inout AF1 g,inout AF1 b){}\n";
static const char *rcas_main =
    "\nuniform uvec4 u_con; uniform int u_unpremultiply; out vec4 o;\n"
    "void main(){ AF1 r,g,b,a; FsrRcasF(r,g,b,a,AU2(gl_FragCoord.xy),u_con);\n"
    "  vec3 c=vec3(r,g,b); if(u_unpremultiply==1 && a>0.0) c=min(c/a,vec3(1.0)); o=vec4(c,a); }\n";

static char *join(const char *a, const char *b, size_t bn, const char *c, const char *d, size_t dn, const char *e)
{
    size_t n = strlen(a) + bn + strlen(c) + dn + strlen(e) + 1;
    char *s = malloc(n);
    if (!s) return NULL;
    size_t at = 0;
    memcpy(s + at, a, strlen(a)); at += strlen(a);
    memcpy(s + at, b, bn); at += bn;
    memcpy(s + at, c, strlen(c)); at += strlen(c);
    memcpy(s + at, d, dn); at += dn;
    memcpy(s + at, e, strlen(e)); at += strlen(e);
    s[at] = 0;
    return s;
}

int fsr_available(void)
{
    if (state) return state > 0;
    state = -1;
    GLint major = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    if (major < 4) return 0;   /* the FSR shaders need GLSL 4.x (textureGather, bit operations) */
    char *e = join(easu_head, fsr_ffx_a, fsr_ffx_a_size, easu_mid, fsr_ffx_fsr1, fsr_ffx_fsr1_size, easu_main);
    char *r = join(easu_head, fsr_ffx_a, fsr_ffx_a_size, rcas_mid, fsr_ffx_fsr1, fsr_ffx_fsr1_size, rcas_main);
    if (e && r) { easu = gl_program(vertex, e); rcas = gl_program(vertex, r); }
    free(e);
    free(r);
    if (!easu || !rcas) { fprintf(stderr, "FSR: shaders unavailable, textures are not upscaled\n"); return 0; }
    const char *names[4] = { "u_con0", "u_con1", "u_con2", "u_con3" };
    for (int i = 0; i < 4; i++) easu_con[i] = glGetUniformLocation(easu, names[i]);
    easu_src = glGetUniformLocation(easu, "u_src");
    easu_size = glGetUniformLocation(easu, "u_size");
    rcas_con = glGetUniformLocation(rcas, "u_con");
    rcas_src = glGetUniformLocation(rcas, "u_src");
    rcas_unpremultiply = glGetUniformLocation(rcas, "u_unpremultiply");
    glGenVertexArrays(1, &vao);
    glGenFramebuffers(1, &fbo);
    state = 1;
    return 1;
}

static unsigned bits(float f) { unsigned u; memcpy(&u, &f, 4); return u; }

static GLuint texture(int w, int h, const void *px, GLint filter)
{
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

GLuint fsr_resolve(GLuint input,int w,int h,int ow,int oh)
{
    if (!input || w<1 || h<1 || ow<1 || oh<1 || !fsr_available())return 0;
    GLint old_fbo = 0, old_vp[4], old_prog = 0, old_vao = 0, old_tex = 0, old_active = 0;
    GLboolean blend = glIsEnabled(GL_BLEND), scissor = glIsEnabled(GL_SCISSOR_TEST), depth = glIsEnabled(GL_DEPTH_TEST);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_fbo);
    glGetIntegerv(GL_VIEWPORT, old_vp);
    glGetIntegerv(GL_CURRENT_PROGRAM, &old_prog);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &old_vao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_tex);

    static GLuint mid,out;
    static int cached_w,cached_h;
    if(cached_w!=ow || cached_h!=oh) {
        if(mid)glDeleteTextures(1,&mid);
        if(out)glDeleteTextures(1,&out);
        mid=texture(ow,oh,NULL,GL_NEAREST);out=texture(ow,oh,NULL,GL_LINEAR);
        cached_w=ow;cached_h=oh;
    }

    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindVertexArray(vao);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, ow, oh);

    /* EASU: constants as FsrEasuCon() computes them (viewport = whole image) */
    unsigned c[16] = {
        bits((float)w / ow), bits((float)h / oh), bits(.5f * w / ow - .5f), bits(.5f * h / oh - .5f),
        bits(1.f / w), bits(1.f / h), bits(1.f / w), bits(-1.f / h),
        bits(-1.f / w), bits(2.f / h), bits(1.f / w), bits(2.f / h),
        bits(0.f / w), bits(4.f / h), 0, 0 };
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mid, 0);
    glUseProgram(easu);
    for (int i = 0; i < 4; i++) glUniform4ui(easu_con[i], c[i * 4], c[i * 4 + 1], c[i * 4 + 2], c[i * 4 + 3]);
    glUniform1i(easu_src, 0);
    glUniform2f(easu_size, (float)ow, (float)oh);
    glBindTexture(GL_TEXTURE_2D, input);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    /* RCAS: FsrRcasCon(sharpness 0.5 stops) */
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, out, 0);
    glUseProgram(rcas);
    glUniform4ui(rcas_con, bits(0.70710678f), 0, 0, 0);   /* exp2(-0.5), restrained sharpening */
    glUniform1i(rcas_src, 0);
    glUniform1i(rcas_unpremultiply, 0);
    glBindTexture(GL_TEXTURE_2D, mid);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)old_fbo);
    glViewport(old_vp[0], old_vp[1], old_vp[2], old_vp[3]);
    glUseProgram((GLuint)old_prog);
    glBindVertexArray((GLuint)old_vao);
    glBindTexture(GL_TEXTURE_2D, (GLuint)old_tex);
    glActiveTexture((GLenum)old_active);
    if (blend) glEnable(GL_BLEND);
    if (scissor) glEnable(GL_SCISSOR_TEST);
    if (depth) glEnable(GL_DEPTH_TEST);
    return out;
}
