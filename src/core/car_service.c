#include "car_service.h"
#include <fenv.h>
#include <string.h>
static float f32(unsigned bits){float x;memcpy(&x,&bits,4);return x;}
void gt4_service_jitter(int samples,int index,float offset[2]){
    int saved=fegetround();fesetround(FE_TOWARDZERO);
    float x=0,y=0;
    if(samples==2){x=index?.75f:.25f;y=index?.25f:.75f;}
    offset[0]=(int)(x*f32(0x3f333333)*16)/16.f;
    offset[1]=(int)(y*f32(0x3f333333)*16)/16.f;
    fesetround(saved);
}
void gt4_service_projection_scale(int video_mode,int widescreen,float scale[2]){
    int saved=fegetround();fesetround(FE_TOWARDZERO);
    float ratio=1;
    if(video_mode==2)ratio=f32(0x3f8c8b43);
    else if(video_mode==3)ratio=f32(0x3f69fbe7);
    scale[0]=widescreen?.75f:1;scale[1]=1/ratio;
    fesetround(saved);
}
float gt4_service_drop_limit(const float axle[5]){
    int saved=fegetround();fesetround(FE_TOWARDZERO);float result=axle[2]-axle[4];fesetround(saved);return result;
}
unsigned gt4_service_blend_fix(float opacity){
    int saved=fegetround();fesetround(FE_TOWARDZERO);float value=opacity*128;
    if(value<0)value=0;if(value>255)value=255;unsigned fix=(unsigned)value;
    fesetround(saved);return fix;
}
void gt4_service_advance(float *time,int playing,int has_motion,float dt){
    int saved=fegetround();fesetround(FE_TOWARDZERO);
    if(has_motion && playing)*time=*time+dt;
    fesetround(saved);
}
float gt4_service_sample(float time,int has_motion,float end,float loop_start,int loop){
    int playing=0;gt4_service_clock(&time,&playing,has_motion,0,end,loop_start,loop);return time;
}
void gt4_service_pose(float position[3],float angles[3],const float target_position[3],const float target_angles[3]){
    int saved=fegetround();fesetround(FE_TOWARDZERO);
    for(int i=0;i<3;i++){
        position[i]=position[i]+(target_position[i]-position[i])*f32(0x3dcccccc);
        angles[i]=angles[i]+(target_angles[i]-angles[i])*f32(i==1?0x3e4ccccc:0x3dcccccc);
    }
    fesetround(saved);
}
float gt4_service_fade(float opacity){
    int saved=fegetround();fesetround(FE_TOWARDZERO);
    opacity=opacity+f32(0x3c088888);if(opacity>1)opacity=1;
    fesetround(saved);return opacity;
}
gt4_service_placement gt4_service_place(const float g[8],int alignment,
                                      float yaw,float scale,int hidden,int reflection){
    int saved=fegetround();fesetround(FE_TOWARDZERO);
    gt4_service_placement p={{0,0,0},yaw,{scale,hidden?0:scale,scale},!(hidden && reflection)};
    float front=g[0]-g[1]*.5f,rear=g[3]-g[4]*.5f,mid=(g[2]+g[5])*.5f;
    if(alignment>=1 && alignment<=8){
        p.offset[0]=(alignment==3 || alignment==4)?rear:front;
        p.offset[2]=alignment<=2?g[2]:alignment<=4?g[5]:alignment<=6?mid:g[2]+(g[5]-g[2])*.75f;
        if(!(alignment&1)){p.offset[0]=-p.offset[0];p.yaw+=180;}
    }else if(alignment==9){p.offset[2]=mid-g[6];p.yaw-=90;}
    else if(alignment==10){p.offset[2]=mid+g[7];p.yaw+=90;}
    fesetround(saved);return p;
}
void gt4_service_lift(float lift,const float radius[4],float *body,float wheel[4]){
    int saved=fegetround();fesetround(FE_TOWARDZERO);
    *body=lift>0?lift:0;
    for(int i=0;i<4;i++)wheel[i]=lift>0?-(lift<radius[i]?lift:radius[i]):0;
    fesetround(saved);
}
void gt4_service_clock(float *time,int *playing,int has_motion,float dt,float end,float loop_start,int loop){
    if(!has_motion)return;
    int saved=fegetround();fesetround(FE_TOWARDZERO);
    if(*playing)*time=*time+dt;
    /* End equality is still playing. The original subtracts only one
       period here; the Mot1 sampler handles its own wrapping separately. */
    if(*time>end){
        if(loop){float period=end-loop_start;*time=*time-period;}
        else{*time=end;*playing=0;}
    }
    fesetround(saved);
}

void gt4_service_position(float position[3],const float target[3],float ratio){
    int saved=fegetround();fesetround(FE_TOWARDZERO);
    for(int i=0;i<3;i++)position[i]=position[i]+(target[i]-position[i])*ratio;
    fesetround(saved);
}
