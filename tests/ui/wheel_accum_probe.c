/* GPU versus independent CPU affine rasterization, with synthetic pixels.
   This checks the PC transport, not equality to GS pixel rules. */
#include <SDL.h>
#include "platform/platform.h"
#include "../../src/platform/sdl/render3d.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"wheel GPU line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void sample(const unsigned char *im,int w,int h,double x,double y,double out[4])
{
    x-=.5;y-=.5;int ix=(int)floor(x),iy=(int)floor(y);double fx=x-ix,fy=y-iy;
    for(int k=0;k<4;k++)out[k]=0;
    for(int j=0;j<2;j++)for(int i=0;i<2;i++) {
        int px=ix+i,py=iy+j;px=px<0?0:px>=w?w-1:px;py=py<0?0:py>=h?h-1:py;
        double weight=(i?fx:1-fx)*(j?fy:1-fy);
        for(int k=0;k<4;k++)out[k]+=im[(py*w+px)*4+k]*weight;
    }
}
static void reference(const unsigned char *source,int width,int size,int samples,float angle,unsigned char *result)
{
    unsigned char scratch[64*192*4];double offset=(double)gt4_wheel_constant(0xbecccccc);
    /* Independent affine basis: rotating source UV around the centre,
       then stretching the scratch's vertical coordinate by two. */
    for(int count=2,stage=0;;count*=2,stage++,angle*=.5f) {
        memset(scratch,0,sizeof scratch);
        for(int draw=0;draw<2;draw++) {
            double a=draw?angle*.5:0,c=cos(a),s=sin(a),half=size*.5;
            double ox=half-half*c+half*s,oy=size+2*(half-half*c-half*s)+(draw?.5:offset);
            double ax=size*c,ay=2*size*s,bx=-size*s,by=2*size*c,det=ax*by-ay*bx;
            for(int y=0;y<3*size;y++)for(int x=0;x<size;x++) {
                if(draw && !(y&1))continue;
                double dx=x+.5-ox,dy=y+.5-oy,u=(dx*by-dy*bx)/det,v=(dy*ax-dx*ay)/det;
                if(u<0 || u>=1 || v<0 || v>=1)continue;
                double rgba[4];sample(stage?result:source,stage?size:width,stage?size:width,u*(stage?size:width),v*(stage?size:width),rgba);
                for(int k=0;k<4;k++)scratch[(y*size+x)*4+k]=(unsigned char)floor(rgba[k]+.5);
            }
        }
        for(int y=0;y<size;y++)for(int x=0;x<size;x++) {
            double rgba[4];sample(scratch,size,3*size,x+.5,size+2*y+1,rgba);
            for(int k=0;k<4;k++)result[(y*size+x)*4+k]=(unsigned char)floor(rgba[k]+.5);
        }
        if(count>=samples)break;
    }
}
int main(int argc,char **argv)
{
    plat_video_desc desc={160,120,0,0,0};CHECK(!plat_video_init(&desc));SDL_HideWindow(SDL_GetWindowFromID(1));
    CHECK(r3d_available());unsigned char source[128*128*4],cpu[64*64*4],gpu[64*64*4];
    unsigned cases=0;double max_error=0;
    for(int width=32;width<=128;width*=2) {
        for(int y=0;y<width;y++)for(int x=0;x<width;x++) {
            unsigned char *p=source+(y*width+x)*4;
            p[0]=(unsigned char)(x*255/(width-1));p[1]=(unsigned char)(y*255/(width-1));
            p[2]=(unsigned char)((x+y)*127/(width-1));p[3]=(unsigned char)((x*x+y*y)<width*width/2?128:0);
        }
        r3d_texture t={0};t.id=upload(width,width,source,NULL);t.tex_w=t.tex_h=t.width=t.height=width;
        t.wrap_u=t.wrap_v=1;
        int size=width<64?width:64;
        for(int samples=2;samples<=16;samples*=2)for(int a=0;a<4;a++) {
            float angle=a*(gt4_wheel_constant(0x40490fda)/3);
            plat_video_begin_frame();r3d_begin(0,0,640,448,NULL);
            GLint before_draw,before_read;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&before_draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&before_read);
            CHECK(wheel_accumulate(&t,samples,angle));CHECK(glGetError()==GL_NO_ERROR);
            GLint after_draw,after_read;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&after_draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&after_read);
            CHECK(before_draw==after_draw && before_read==after_read);
            unsigned passes=frame_stats.wheel_passes;CHECK(passes==3*(unsigned)gt4_wheel_accumulation(width,samples,angle).stages);
            CHECK(wheel_accumulate(&t,samples,angle));CHECK(frame_stats.wheel_passes==passes && frame_stats.wheel_cache_hits==1);
            glBindTexture(GL_TEXTURE_2D,t.wheel_tex[1]);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,gpu);
            reference(source,width,size,samples,angle,cpu);
            if(argc>1 && !strcmp(argv[1],"--negative"))cpu[(size/2*size+size/2)*4]^=128;
            for(int i=0;i<size*size*4;i++) {
                double e=fabs((double)cpu[i]-gpu[i]);if(e>max_error)max_error=e;
                if(e>4){fprintf(stderr,"width %d samples %d angle %g byte %d CPU %u GPU %u\n",width,samples,angle,i,cpu[i],gpu[i]);return 1;}
            }
            r3d_end();plat_video_end_frame();cases++;
        }
        glDeleteTextures(1,&t.id);glDeleteTextures(2,t.wheel_tex);glDeleteFramebuffers(2,t.wheel_fbo);
    }
    plat_video_shutdown();printf("PASS %u GPU accumulation images, max channel error %.0f/255 (limit 4), cache and framebuffer restoration.\n",cases,max_error);return 0;
}
