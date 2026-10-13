#include "render3d_frustum.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void)
{
    r3d_view v={{1,0,0,0,1,0,0,0,1}};
    v.r[0]=v.u[1]=v.f[2]=1;v.focal=320;v.focal_y=224;v.near_clip=1;
    float planes[6][4],lo[3]={-1,-1,4},hi[3]={1,1,6};r3d_frustum(&v,1,planes);
    CHECK(r3d_bounds_visible(planes,lo,hi));
    lo[0]=10;hi[0]=12;CHECK(!r3d_bounds_visible(planes,lo,hi));
    lo[0]=5;hi[0]=7;CHECK(r3d_bounds_visible(planes,lo,hi)); /* intersects right edge */
    lo[0]=-1;hi[0]=1;lo[2]=-3;hi[2]=.5f;CHECK(!r3d_bounds_visible(planes,lo,hi));
    hi[2]=1;CHECK(r3d_bounds_visible(planes,lo,hi)); /* near-plane boundary */
    lo[2]=4;hi[2]=6;v.pos[0]=12;r3d_frustum(&v,1,planes);CHECK(!r3d_bounds_visible(planes,lo,hi));
    v.cam[0]=12;r3d_frustum(&v,1,planes);CHECK(r3d_bounds_visible(planes,lo,hi));
    /* Differential oracle: rejection must not hide any transformed box
       corner inside the shader frustum, including rotated/mirrored objects. */
    unsigned seed=12345;
    for(int i=0;i<100000;i++) {
        float values[10];for(int k=0;k<10;k++){seed=seed*1664525u+1013904223u;values[k]=((seed>>8)&65535)/65535.f*40-20;}
        float sn=sinf(values[0]),cs=cosf(values[0]);float rot[9]={cs,0,sn,0,1,0,-sn,0,cs};
        for(int k=0;k<9;k++)v.rot[k]=rot[k];
        for(int k=0;k<3;k++){lo[k]=values[k+1];hi[k]=lo[k]+fabsf(values[k+4]);v.pos[k]=values[k+7];v.cam[k]=values[k+4];}
        float yaw=values[9],pitch=values[8]*.04f,cy=cosf(yaw),sy=sinf(yaw),cp=cosf(pitch),sp=sinf(pitch);
        v.r[0]=cy;v.r[1]=0;v.r[2]=-sy;v.u[0]=-sy*sp;v.u[1]=cp;v.u[2]=-cy*sp;
        v.f[0]=sy*cp;v.f[1]=sp;v.f[2]=cy*cp;
        float aspect=.7f+(i%10)*.1f;v.mirror=i&1;r3d_frustum(&v,aspect,planes);
        if(r3d_bounds_visible(planes,lo,hi))continue;
        for(int corner=0;corner<8;corner++) {
            float w[3];for(int k=0;k<3;k++){w[k]=v.pos[k];for(int j=0;j<3;j++)w[k]+=rot[k*3+j]*((corner&(1<<j))?hi[j]:lo[j]);}
            if(v.mirror)w[1]=-w[1];
            for(int k=0;k<3;k++)w[k]-=v.cam[k];
            float x=0,y=0,z=0;for(int k=0;k<3;k++){x+=w[k]*v.r[k];y+=w[k]*v.u[k];z+=w[k]*v.f[k];}
            CHECK(!(z>=1 && z<=20000 && fabsf(x)*aspect<=z && fabsf(y)<=z));
        }
    }
    /* Skin bounds must contain every normalized blend, including translations,
       rotations, a rigid vertex in the same batch, and mixed bone slots. */
    float palette[20][16]={0};
    for(int b=0;b<20;b++)for(int k=0;k<16;k++)palette[b][k]=k%5==0?1.f:0.f;
    lo[0]=-1;lo[1]=-2;lo[2]=-3;hi[0]=2;hi[1]=3;hi[2]=4;
    for(int i=0;i<10000;i++) {
        float total=0,weight[4];unsigned mask=0;
        for(int b=0;b<4;b++) {
            seed=seed*1664525u+1013904223u;float angle=(seed>>8)*.000001f;
            float *m=palette[b];m[0]=m[10]=cosf(angle);m[2]=-sinf(angle);m[8]=sinf(angle);
            for(int k=0;k<3;k++){seed=seed*1664525u+1013904223u;m[12+k]=((seed>>8)&65535)/65535.f*200-100;}
            seed=seed*1664525u+1013904223u;weight[b]=(float)(seed&255);total+=weight[b];mask|=1u<<b;
        }
        CHECK(total>0);float bound_lo[3],bound_hi[3];
        float sum_lo=.5f+(i%11)*.01f,sum_hi=1.5f+(i%13)*.01f;
        r3d_skin_bounds(lo,hi,mask,sum_lo,sum_hi,palette,bound_lo,bound_hi);
        for(int corner=0;corner<8;corner++)for(int k=0;k<3;k++) {
            float result=0;
            for(int b=0;b<4;b++) {
                float p=palette[b][12+k];
                for(int j=0;j<3;j++)p+=palette[b][j*4+k]*((corner&(1<<j))?hi[j]:lo[j]);
                result+=p*weight[b]/total*(i&1?sum_lo:sum_hi);
            }
            CHECK(result>=bound_lo[k]-.001f && result<=bound_hi[k]+.001f);
        }
        r3d_skin_bounds(lo,hi,mask|0x80000000u,sum_lo,sum_hi,palette,bound_lo,bound_hi);
        for(int k=0;k<3;k++)CHECK(bound_lo[k]<=lo[k] && bound_hi[k]>=hi[k]);
    }
    puts("renderer: frustum boundaries, 100000 transformed boxes and 10000 skin blends passed");return 0;
}
