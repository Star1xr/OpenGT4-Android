#include "course_gadget.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fenv.h>
static unsigned u16(const uint8_t *p){return p[0]|(unsigned)p[1]<<8;}
static uint32_t u32(const uint8_t *p){return u16(p)|u16(p+2)<<16;}
static float f32(const uint8_t *p){float f;memcpy(&f,p,4);return f;}
static int span(size_t n,size_t p,size_t s){return p<=n && s<=n-p;}
int gt4_course_gadget_visible(const gt4_course_gadget *g)
{
    if(!g || !g->active)return 0;if(!g->age)return 1;
    fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
    uint32_t bits=0x3c888888;float step,rate;memcpy(&step,&bits,4);
    bits=0x40d55556;memcpy(&rate,&bits,4);
    float t=(float)g->age*step;int visible=((int)((t*t)*rate)^1)&1;
    fesetenv(&env);return visible;
}
void gt4_course_gadget_rotation(const float a[3],float r[9])
{
    /* 003E24F0 +4C..108. 0057D8A0's sin/cos is a host boundary (C).
       Preserve its multiply/add order, then transpose for r3d_view. */
    fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
    float sx=sinf(a[0]),cx=cosf(a[0]),sy=sinf(a[1]),cy=cosf(a[1]),sz=sinf(a[2]),cz=cosf(a[2]);
    float xz=sx*sz,xc=sx*cz;
    r[0]=cy*cz-xz*sy;r[1]=cy*sz+xc*sy;r[2]=-cx*sy;
    r[3]=-cx*sz;r[4]=cx*cz;r[5]=sx;
    r[6]=sy*cz+xz*cy;r[7]=sy*sz-xc*cy;r[8]=cx*cy;
    fesetenv(&env);
}
void gt4_course_gadget_surface(gt4_course_gadget *g,const float normal[3],float plane,float sun,float shade)
{
    fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);
    /* 003E29F8 +78..D0: truncate the clamped light to one byte. */
    g->sun=(float)(int)fminf(255,fmaxf(0,sun*255))/255.f;
    g->shade=(float)(int)fminf(255,fmaxf(0,shade*255))/255.f;
    memcpy(g->normal,normal,12);g->plane=plane;
    if(!g->fixed){
        /* 003E2268 aligns D_006A3950 (0,1,0) to the sampled normal;
           003E2710 +128..178 concatenates it with authored rotation,
           003E2600 extracts angles, 003E4930 rebuilds the draw matrix. */
        float n[3],len=sqrtf((normal[0]*normal[0]+normal[1]*normal[1])+normal[2]*normal[2]);
        if(len>0){
            for(int i=0;i<3;i++)n[i]=normal[i]/len;
            float axis[3]={n[2],0,-n[0]},s=sqrtf(axis[0]*axis[0]+axis[2]*axis[2]);
            float b[9]={1,0,0,0,1,0,0,0,1},r[9];
            if(s>0){
                float x=axis[0]/s,z=axis[2]/s,c=n[1];
                b[0]=x*x+c*(1-x*x);b[1]=-s*z;b[2]=x*z+c*(-x*z);
                b[3]=s*z;b[4]=c;b[5]=-s*x;
                b[6]=x*z+c*(-x*z);b[7]=s*x;b[8]=z*z+c*(1-z*z);
            }
            for(int i=0;i<3;i++)for(int j=0;j<3;j++)r[i*3+j]=(b[i*3]*g->rot[j]+b[i*3+1]*g->rot[3+j])+b[i*3+2]*g->rot[6+j];
            float c=sqrtf(1-r[5]*r[5]);
            g->angles[0]=atan2f(r[5],c);g->angles[1]=atan2f(-r[2]/c,r[8]/c);g->angles[2]=atan2f(-r[3]/c,r[4]/c);
            gt4_course_gadget_rotation(g->angles,g->rot);
            float p[3];for(int i=0;i<3;i++)p[i]=((g->rot[i*3]*g->corner[0]+g->rot[i*3+1]*g->corner[1])+g->rot[i*3+2]*g->corner[2])+g->pos[i];
            if(normal[1]!=0)g->pos[1]+= -((normal[0]*p[0]+normal[2]*p[2])+plane)/normal[1]-p[1];
        }
    }
    fesetenv(&env);
}
void gt4_course_gadgets_close(gt4_course_gadgets *g)
{
    if(!g)return;
    if(g->model)for(unsigned i=0;i<g->types;i++)gt4_model_free(g->model[i]);
    free(g->descriptor);free(g->model);free(g->instance);free(g->first);free(g->length);free(g);
}
static gt4_course_gadgets *open_gadgets(const gt4_course *c,int meshes)
{
    if(!c || !c->data || c->size<0x100)return NULL;
    const uint8_t *d=c->data;size_t n=c->size,at=u32(d+0xbc),rw=c->runway;
    if(!span(n,rw,0xa0) || !at || !span(n,at,32) || memcmp(d+at,"GDGT",4))return NULL;
    unsigned types=u32(d+at+0x1c),sections=u16(d+rw+0x54);
    size_t mdls=at+u32(d+at+0x10),table=rw+u32(d+rw+0x9c);
    if(!types || types>256 || !span(n,at+32,types*160) || !span(n,mdls,128) ||
       memcmp(d+mdls,"MDLS",4) || !span(n,table,(size_t)sections*32))return NULL;
    gt4_course_gadgets *g=calloc(1,sizeof *g);if(!g)return NULL;
    g->types=types;g->sections=sections;
    g->model=calloc(types,sizeof *g->model);g->first=calloc(sections+1,sizeof *g->first);g->length=calloc(sections+1,sizeof *g->length);
    g->descriptor=malloc(types*160);
    if(!g->model || !g->first || !g->length || !g->descriptor)goto fail;
    memcpy(g->descriptor,d+at+32,types*160);
    /* 003E53B8 +28..3C: descriptor's signed model index * 2; the
       second model is its shadow. Keep the authored normal draw stream. */
    if(meshes)for(unsigned i=0;i<types;i++){
        int model=(int16_t)u16(d+at+32+i*160);
        if(model<0 || !(g->model[i]=gt4_model_decode_part(d+mdls,n-mdls,-1,model*2)))goto fail;
    }
    for(unsigned i=0;i<=sections;i++){
        g->first[i]=g->count;g->length[i]=i?u16(d+table+(i-1)*32+12):u16(d+rw+0x44);
        g->count+=g->length[i];
    }
    g->instance=calloc(g->count?g->count:1,sizeof *g->instance);if(!g->instance)goto fail;
    for(unsigned i=0;i<=sections;i++){
        size_t p=rw+(i?u32(d+table+(i-1)*32+28):u32(d+rw+0x70));
        if(!span(n,p,(size_t)g->length[i]*32))goto fail;
        for(unsigned j=0;j<g->length[i];j++,p+=32){
            gt4_course_gadget *v=g->instance+g->first[i]+j;
            /* 003E2710 +2C..64: byte type, XYZ +8, Euler +14. */
            v->type=d[p];if(v->type>=types)goto fail;
            const uint8_t *desc=g->descriptor+v->type*160;v->descriptor=desc;v->fixed=(desc[2]>>1)&1;
            if(desc[3]>8)goto fail;
            if(!v->fixed && desc[3]){
                float mass=f32(desc+0x94),reach=f32(desc+0x90);
                if(!isfinite(mass) || mass<=0 || !isfinite(reach) || reach<0)goto fail;
                for(unsigned k=0;k<3;k++)if(!isfinite(f32(desc+4+k*4)) || f32(desc+4+k*4)<=0)goto fail;
                for(unsigned k=0;k<desc[3]*4;k++)if(!isfinite(f32(desc+16+k*4)))goto fail;
            }
            v->active=1;v->ground_id=-1;v->normal[1]=1;memset(v->support,2,sizeof v->support);
            for(int k=0;k<3;k++){
                v->pos[k]=f32(d+p+8+k*4);v->angles[k]=f32(d+p+20+k*4);v->corner[k]=f32(desc+16+k*4);
                if(!isfinite(v->pos[k]) || !isfinite(v->angles[k]) || !isfinite(v->corner[k]))goto fail;
            }
            /* 003E2710 +CC..D8: fallback light bytes 0x80. */
            gt4_course_gadget_rotation(v->angles,v->rot);v->sun=v->shade=128/255.f;
        }
    }
    return g;
fail:gt4_course_gadgets_close(g);return NULL;
}

gt4_course_gadgets *gt4_course_gadgets_open(const gt4_course *c){return open_gadgets(c,1);}
gt4_course_gadgets *gt4_course_gadgets_open_state(const gt4_course *c){return open_gadgets(c,0);}
