/* Synthetic GL regression for the native destination-alpha mask transport.
   DATE/DATM semantics: 00455708 commands 29/31 (GS TEST bits 14/15).
   Single-sample and MSAA targets must observe the preceding draw's alpha.
   This is a PC renderer test, not an original-image fidelity oracle. */
#include <SDL.h>
#include "platform/platform.h"
#include "render3d.h"
#include "video_gl.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"mask line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void quad(gt4_triangle t[2],int blue,float alpha,int date,int bit)
{
    static const float pos[6][3]={{-1,-1,3},{1,-1,3},{1,1,3},{-1,-1,3},{1,1,3},{-1,1,3}};
    memset(t,0,2*sizeof *t);
    for(int i=0;i<2;i++) {
        t[i].texture=-1;t[i].kind=0;
        t[i].state.depth_write=!date;t[i].state.depth_func=1;
        t[i].state.destination_test=date;t[i].state.destination_bit=bit;
        for(int v=0;v<3;v++) {
            memcpy(t[i].v[v].p,pos[i*3+v],12);t[i].v[v].normal[2]=-1;
            t[i].v[v].rgb[blue?2:0]=1;t[i].v[v].alpha=alpha;
        }
    }
}
int main(int argc,char **argv)
{
    plat_video_desc desc={160,120,0,0,0};CHECK(!plat_video_init(&desc));
    SDL_HideWindow(SDL_GetWindowFromID(1));plat_video_set_view(640,480);
    CHECK(r3d_available());GLint limit;glGetIntegerv(GL_MAX_SAMPLES,&limit);CHECK(limit>=4);
    GLuint fbo,color,depth;glGenFramebuffers(1,&fbo);glGenRenderbuffers(1,&color);glGenRenderbuffers(1,&depth);
    r3d_view view={0};view.rot[0]=view.rot[4]=view.rot[8]=1;
    view.r[0]=view.u[1]=view.f[2]=1;view.focal=320;view.focal_y=224;view.unlit=1;
    for(int samples=1;samples<=4;samples+=3)for(int alpha_bit=0;alpha_bit<2;alpha_bit++)for(int bit=0;bit<2;bit++) {
        plat_video_begin_frame();glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glBindRenderbuffer(GL_RENDERBUFFER,color);glRenderbufferStorageMultisample(GL_RENDERBUFFER,samples,GL_RGBA8,160,120);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,color);
        glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorageMultisample(GL_RENDERBUFFER,samples,GL_DEPTH24_STENCIL8,160,120);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,depth);
        CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
        glDisable(GL_SCISSOR_TEST);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
        r3d_begin(0,0,640,448,NULL);
        gt4_triangle a[2],b[2];quad(a,0,alpha_bit?1:.25f,0,0);quad(b,1,1,1,bit);
        gt4_model base={0},overlay={0};base.triangles=a;base.count=2;overlay.triangles=b;overlay.count=2;
        r3d_mesh *ma=r3d_mesh_create(&base),*mb=r3d_mesh_create(&overlay);CHECK(ma && mb);
        CHECK(glGetError()==GL_NO_ERROR);r3d_draw(ma,&view);r3d_draw(mb,&view);CHECK(glGetError()==GL_NO_ERROR);
        GLint draw_fb,read_fb;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw_fb);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read_fb);
        CHECK(draw_fb==(GLint)fbo && read_fb==(GLint)fbo);
        unsigned char pixels[32*32*4];CHECK(r3d_begin_target(32));
        glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);
        /* Copy a uniform central patch, retaining equal extents for MSAA. */
        glBlitFramebuffer(64,40,96,72,0,0,32,32,GL_COLOR_BUFFER_BIT,GL_NEAREST);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw_fb);glBindFramebuffer(GL_READ_FRAMEBUFFER,(GLuint)draw_fb);
        r3d_end_target(pixels);CHECK(glGetError()==GL_NO_ERROR);
        int blue=alpha_bit==bit;if(argc>1 && !strcmp(argv[1],"--negative"))blue=!blue;
        const unsigned char *p=pixels+(16*32+16)*4;CHECK(p[blue?2:0]>60 && p[blue?0:2]<4);
        r3d_mesh_destroy(ma);r3d_mesh_destroy(mb);r3d_end();
    }
    glDeleteRenderbuffers(1,&color);glDeleteRenderbuffers(1,&depth);glDeleteFramebuffers(1,&fbo);
    plat_video_shutdown();puts("model masks: 8 DATE/DATM cases, 1x/4x samples, bindings and GL errors passed");return 0;
}
