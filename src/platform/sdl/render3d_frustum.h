#ifndef OPENGT4_RENDER3D_FRUSTUM_H
#define OPENGT4_RENDER3D_FRUSTUM_H
#include "render3d.h"
#include <math.h>

/* A nonnegative blend is its weight sum times a point in the convex hull of
   transformed source boxes. Retain the authored sum (not always exactly 1).
   Bound each used bone's affine box, take the union and scale by the sum range.
   This retains moving hands/limbs without CPU skinning or a guessed margin.
   Bit 31 also retains any rigid vertices in a mixed batch. */
static void r3d_skin_bounds(const float lo[3],const float hi[3],unsigned mask,
                            float weight_lo,float weight_hi,
                            const float palette[20][16],float out_lo[3],float out_hi[3])
{
    float center[3],extent[3];
    for(int k=0;k<3;k++) {
        center[k]=(lo[k]+hi[k])*.5f;extent[k]=(hi[k]-lo[k])*.5f;
        out_lo[k]=1e30f;out_hi[k]=-1e30f;
    }
    for(int b=0;b<20;b++)if(mask&(1u<<b)) {
        const float *m=palette[b];
        for(int k=0;k<3;k++) {
            float c=m[12+k],e=0;
            for(int j=0;j<3;j++){c+=m[j*4+k]*center[j];e+=fabsf(m[j*4+k])*extent[j];}
            out_lo[k]=fminf(out_lo[k],c-e);out_hi[k]=fmaxf(out_hi[k],c+e);
        }
    }
    for(int k=0;k<3;k++) {
        if(mask&0xfffffu) {
            float a=out_lo[k]*weight_lo,b=out_lo[k]*weight_hi,c=out_hi[k]*weight_lo,d=out_hi[k]*weight_hi;
            out_lo[k]=fminf(fminf(a,b),fminf(c,d));out_hi[k]=fmaxf(fmaxf(a,b),fmaxf(c,d));
        }
        if(mask&0x80000000u){out_lo[k]=fminf(out_lo[k],lo[k]);out_hi[k]=fmaxf(out_hi[k],hi[k]);}
    }
}

/* Planes in model space, matching the vertex shader (including floor mirror).
   A support-point AABB test only rejects boxes wholly outside a plane. */
static void r3d_frustum(const r3d_view *v,float projection_x,float planes[6][4])
{
    float fy=v->focal_y>0?v->focal_y:v->focal;
    float fx=v->focal/320*projection_x,sy=fy/224;
    for(int i=0;i<6;i++) {
        float n[3];
        for(int k=0;k<3;k++)
            n[k]=i<2?v->f[k]+(i==0?fx:-fx)*v->r[k]:
                 i<4?v->f[k]+(i==2?sy:-sy)*v->u[k]:
                 i==4?v->f[k]:-v->f[k];
        float d=i==4?-(v->near_clip>0?v->near_clip:.2f):i==5?20000:0;
        for(int k=0;k<3;k++)d-=n[k]*v->cam[k];
        if(v->mirror)n[1]=-n[1];
        for(int k=0;k<3;k++)d+=n[k]*v->pos[k];
        for(int k=0;k<3;k++)planes[i][k]=n[0]*v->rot[k]+n[1]*v->rot[3+k]+n[2]*v->rot[6+k];
        planes[i][3]=d;
    }
}
static int r3d_bounds_visible(const float planes[6][4],const float lo[3],const float hi[3])
{
    for(int i=0;i<6;i++) {
        float d=planes[i][3];
        for(int k=0;k<3;k++)d+=planes[i][k]*(planes[i][k]>=0?hi[k]:lo[k]);
        if(d<-.001f)return 0; /* retain boundary boxes despite float rounding */
    }
    return 1;
}
#endif
