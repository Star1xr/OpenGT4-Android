/* STRB layout cross-checked against the user's LicenseRoot data and Nenkai's
   Strobe format documentation; see docs/STROBE.md. Native presentation only. */
#include "strobe.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
static unsigned u16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static uint32_t u32(const uint8_t *p){return u16(p)|(uint32_t)u16(p+2)<<16;}
static float f32(const uint8_t *p){float f;memcpy(&f,p,4);return f;}
static int span(size_t size,size_t off,size_t len){return off<=size && len<=size-off;}
static int near(float a,float b){return isfinite(a) && isfinite(b) && fabsf(a-b)<.001f;}
void gt4_strobe_free(gt4_strobe *s)
{
    tex1_free(s->bitmaps,(int)s->nbitmaps);free(s->frames);free(s->quads);memset(s,0,sizeof *s);
}
unsigned gt4_strobe_frame_at(const gt4_strobe *s,double seconds)
{
    if(!s->nframes || !isfinite(seconds) || seconds<=0)return 0;
    return (unsigned)fmod(floor(seconds*s->rate),s->loop_frames?s->loop_frames:s->nframes);
}

static int shape_quad(const uint8_t *b,size_t n,const gt4_strobe *s,unsigned id,gt4_strobe_quad *q)
{
    if(id>=u32(b+0x3c))return 0;
    size_t shape=u32(b+0x38)+(size_t)id*24;
    if(!span(n,shape,24) || u32(b+shape+20)!=1)return 0;
    size_t group=u32(b+shape+16);if(!span(n,group,8) || u32(b+group+4)!=1)return 0;
    size_t fillshape=u32(b+group);if(!span(n,fillshape,12) || u32(b+fillshape+8)!=1)return 0;
    unsigned fill=u32(b+fillshape);if(fill>=u32(b+0x6c))return 0;
    size_t style=u32(b+0x68)+(size_t)fill*8;
    if(!span(n,style,8) || u16(b+style)!=4)return 0;
    q->bitmap=u16(b+style+2);unsigned matrix=u16(b+style+4);
    if(q->bitmap>=s->nbitmaps || matrix>=u32(b+0x9c))return 0;
    size_t mat=u32(b+0x98)+(size_t)matrix*64;
    if(!span(n,mat,64) || !near(f32(b+mat+4),0) || !near(f32(b+mat+16),0))return 0;
    float l=f32(b+shape),t=f32(b+shape+4),r=f32(b+shape+8),d=f32(b+shape+12);
    if(!isfinite(l+t+r+d) || !(r>l && d>t) || r-l>8192 || d-t>8192)return 0;
    size_t stroke=u32(b+fillshape+4);
    if(!span(n,stroke,20) || u32(b+stroke+16)!=4)return 0;
    size_t path=u32(b+stroke+12);if(!span(n,path,80))return 0;
    /* Four straight quadratic edges, each control point at its midpoint.
       Validate the contour before using its rectangle as a textured quad. */
    unsigned corners=0;
    for(int i=0;i<4;i++) {
        const uint8_t *edge=b+path+i*20,*prev=b+path+((i+3)%4)*20;
        float x=f32(edge+12),y=f32(edge+16),px=f32(prev+12),py=f32(prev+16);
        if(u32(edge)!=0 || !near(f32(edge+4),(x+px)*.5f) || !near(f32(edge+8),(y+py)*.5f))return 0;
        if(!(near(x,l)||near(x,r)) || !(near(y,t)||near(y,d)) || !(near(x,px)||near(y,py)))return 0;
        corners|=1u<<((near(x,r)?1:0)+(near(y,d)?2:0));
    }
    if(corners!=15)return 0;
    const tex1_image *im=&s->bitmaps[q->bitmap];
    float sx=(float)im->tex_w/im->width,sy=(float)im->tex_h/im->height;
    q->x=l;q->y=t;q->w=r-l;q->h=d-t;
    q->u0=(l*f32(b+mat)+f32(b+mat+48))*sx;q->u1=(r*f32(b+mat)+f32(b+mat+48))*sx;
    q->v0=(t*f32(b+mat+20)+f32(b+mat+52))*sy;q->v1=(d*f32(b+mat+20)+f32(b+mat+52))*sy;
    return isfinite(q->u0+q->v0+q->u1+q->v1);
}

int gt4_strobe_decode(const uint8_t *b,size_t n,gt4_strobe *s)
{
    memset(s,0,sizeof *s);
    if(!b || n<0xd0 || memcmp(b,"STRB",4) || u32(b+4)!=0 || u32(b+8)>n || u32(b+0x34)!=1)return 0;
    s->width=f32(b+16);s->height=f32(b+20);s->rate=f32(b+28);
    if(!(s->width>0 && s->width<=8192 && s->height>0 && s->height<=8192 && s->rate>0 && s->rate<=240))goto fail;
    size_t spr=u32(b+0x30);if(!span(n,spr,36))goto fail;
    size_t places=u32(b+spr),frames=u32(b+spr+24);unsigned nplaces=u32(b+spr+4);
    s->nframes=u32(b+spr+28);s->nbitmaps=u32(b+0xb4);
    if(!s->nframes || s->nframes>36000 ||
       s->nframes!=u32(b+spr+32) || !s->nbitmaps || s->nbitmaps>128 ||
       nplaces>65535 || !span(n,places,(size_t)nplaces*40) || !span(n,frames,(size_t)s->nframes*8))goto fail;
    /* Manufacturer banners end with exactly GotoFrame(0), Play, End.
       Recognize that authored loop; reject arbitrary ActionScript. */
    unsigned sprite_actions=u32(b+spr+12),actions=u32(b+0xac);
    if(sprite_actions || actions){
        size_t list=u32(b+spr+8),table=u32(b+0xa8);
        static const uint8_t loop[]={0x81,2,0,0,0,6,0};
        if(sprite_actions!=1 || actions!=1 || !span(n,list,4) || !span(n,table,8) ||
           u16(b+list)!=s->nframes-1 || u16(b+list+2)!=0 || u32(b+table+4)!=sizeof loop ||
           !span(n,u32(b+table),sizeof loop) || memcmp(b+u32(b+table),loop,sizeof loop) || s->nframes<2)goto fail;
        s->loop_frames=s->nframes-1;
    }
    size_t images=u32(b+0xb0);if(!span(n,images,(size_t)s->nbitmaps*16))goto fail;
    s->bitmaps=calloc(s->nbitmaps,sizeof *s->bitmaps);if(!s->bitmaps)goto fail;
    for(unsigned i=0;i<s->nbitmaps;i++) {
        const uint8_t *entry=b+images+i*16;size_t off=u32(entry),len=u32(entry+4);
        if(!span(n,off,len))goto fail;
        tex1_image *im=NULL;int count=tex1_decode(b+off,len,&im);
        if(count!=1){tex1_free(im,count);goto fail;}
        s->bitmaps[i]=im[0];free(im);
    }
    s->frames=calloc(s->nframes,sizeof *s->frames);if(!s->frames)goto fail;
    for(unsigned i=0;i<s->nframes;i++) {
        size_t at=frames+i*8;unsigned count=u32(b+at+4);
        if(count>512 || !span(n,u32(b+at),(size_t)count*2) || s->nquads>1000000-count)goto fail;
        s->frames[i]=(gt4_strobe_frame){s->nquads,count};s->nquads+=count;
    }
    s->quads=calloc(s->nquads?s->nquads:1,sizeof *s->quads);if(!s->quads)goto fail;
    for(unsigned frame=0;frame<s->nframes;frame++) {
        const gt4_strobe_frame *f=&s->frames[frame];size_t ids=u32(b+frames+frame*8);unsigned depth=0;
        for(unsigned i=0;i<f->count;i++) {
            unsigned id=u16(b+ids+i*2);if(id>=nplaces)goto fail;
            const uint8_t *p=b+places+id*40;
            unsigned start=u16(p+2),end=u16(p+4),params=u32(p+28);
            if(u16(p)!=0 || frame<start || frame>=end || u16(p+10) || u32(p+20) || !params || u16(p+8)<depth)goto fail;
            depth=u16(p+8);size_t off=u32(p+24);
            unsigned index=frame-start;if(index>=params)index=params-1;
            if(!span(n,off,(size_t)params*36))goto fail;
            const uint8_t *param=b+off+index*36;
            /* This subset handles authored axis-aligned bitmap slides/fades. */
            if(!near(f32(param),0) || !near(f32(param+4),0))goto fail;
            gt4_strobe_quad *q=&s->quads[f->first+i];
            if(!shape_quad(b,n,s,u16(p+6),q))goto fail;
            float sx=f32(param+8),sy=f32(param+12),x=f32(param+16),y=f32(param+20);
            if(!isfinite(sx+sy+x+y) || sx<=0 || sy<=0)goto fail;
            q->x=q->x*sx+x;q->y=q->y*sy+y;q->w*=sx;q->h*=sy;
            for(int c=0;c<4;c++){unsigned v=param[24+c]*2;q->color[c]=(uint8_t)(v>255?255:v);}
        }
    }
    return 1;
fail:
    gt4_strobe_free(s);return 0;
}
