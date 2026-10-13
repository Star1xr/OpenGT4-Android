/* Standalone comparison against the user's original driver constructor.
   Generated original functions and EE helpers never enter the game. */
#include "ee.h"
#include "driver_pose.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fenv.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do {if(!(x)){fprintf(stderr,"driver pose oracle line %d: %s\n",__LINE__,#x);exit(1);}} while(0)
static void putf(uint8_t *p,float f){memcpy(p,&f,4);}
static int invoke(ee_ctx *c,uint32_t pc)
{jmp_buf j;c->exit_jmp=&j;if(setjmp(j)){fprintf(stderr,"%s\n",c->end_detail);return 0;}ee_fn fn=c->lookup(pc);if(!fn)return 0;fn(c);return 1;}
static void near_values(const float *a,const uint8_t *raw,int n)
{for(int k=0;k<n;k++){float b;memcpy(&b,raw+4*k,4);if(fabsf(a[k]-b)>2e-6f){fprintf(stderr,"component %d: native %.9g original %.9g\n",k,a[k],b);exit(1);}}}
static uint32_t seed=0x49211288;
static float random_unit(void){seed=seed*1664525u+1013904223u;return (seed>>8)*(1.f/16777216.f);}
/* The EE diagnostic has no VU registers. Emulate decoded cross, product,
   affine and addition instructions independently;
   all scalar IK, SDK trigonometry and quaternion-vector code runs original. */
static void vu_product(ee_ctx *c)
{
    float a[4],b[4],out[4];memcpy(a,c->ram+c->r[5].uw[0],sizeof a);memcpy(b,c->ram+c->r[6].uw[0],sizeof b);
    for(int k=0;k<3;k++){int j=(k+1)%3,l=(k+2)%3;out[k]=a[j]*b[l]-a[l]*b[j];}
    if(c->end_pc==0x48d530) {
        for(int k=0;k<3;k++){float acc=a[k]*b[3];acc+=b[k]*a[3];out[k]=acc+out[k];}
        float scalar=-a[0]*b[0];for(int k=1;k<4;k++)scalar-=a[k]*b[k];
        float acc=a[3]*b[3];acc+=b[3]*a[3];out[3]=acc+scalar;
    }else out[3]=a[3];
    memcpy(c->ram+c->r[4].uw[0],out,sizeof out);
}
static ee_fn vu_hook(uint32_t pc);
static void vu_cross(ee_ctx *c){c->end_pc=0x48d4c0;vu_product(c);}
static void vu_quaternion(ee_ctx *c){c->end_pc=0x48d530;vu_product(c);}
static void vu_affine(ee_ctx *c)
{
    float m[16],v[4],out[4];memcpy(m,c->ram+c->r[4].uw[0],sizeof m);memcpy(v,c->ram+c->r[6].uw[0],sizeof v);
    for(int k=0;k<4;k++){float acc=m[k]*v[0];acc+=m[4+k]*v[1];acc+=m[8+k]*v[2];out[k]=acc+m[12+k]*v[3];}
    memcpy(c->ram+c->r[5].uw[0],out,sizeof out);
}
static void vu_add(ee_ctx *c)
{
    float a[4],b[4],out[4];memcpy(a,c->ram+c->r[5].uw[0],sizeof a);memcpy(b,c->ram+c->r[6].uw[0],sizeof b);
    for(int k=0;k<4;k++)out[k]=a[k]+b[k];memcpy(c->ram+c->r[4].uw[0],out,sizeof out);
}
static gt4_driver_animation_input motion_input;
static void anticipation_input(ee_ctx *c)
{ee_w32(c,c->r[5].uw[0],motion_input.upshift);ee_w32(c,c->r[6].uw[0],motion_input.downshift);ee_w32(c,c->r[7].uw[0],motion_input.speed_shift);}
static void wheel_x(ee_ctx *c){c->fpr[0].f=-.3f;}
static void shift_x(ee_ctx *c){c->fpr[0].f=.1f;}
static ee_fn vu_hook(uint32_t pc){return pc==0x48d4c0?vu_cross:pc==0x48d530?vu_quaternion:pc==0x48eb10?vu_affine:pc==0x48d3e0?vu_add:
    pc==0x40ca58?anticipation_input:pc==0xf00000?wheel_x:pc==0xf00008?shift_x:NULL;}
static ee_fn oracle_lookup(uint32_t pc){ee_fn hook=vu_hook(pc);return hook?hook:recomp_lookup(pc);}
/* Keep this fixture independent of changes to excluded-call hook generation. */
void driver_oracle_boundary(ee_ctx *c,uint32_t target,uint32_t from_pc)
{ee_fn fn=c->hook?c->hook(target):NULL;if(fn)fn(c);else ee_unrecompiled(c,target,from_pc);}
static void limb_oracle(ee_ctx *c,uint32_t object,uint32_t stack)
{
    const uint32_t s1=object,s2=object+0x100,vtable=object+0x200,data=object+0x300,result=object+0x400;
    ee_w16(c,vtable+8,0);ee_w32(c,vtable+12,0x41b898);
    unsigned count=0;float max_error=0;
    for(int i=0;i<10000;i++) {
        gt4_driver_limb_target t={0};gt4_driver_segment a={0},b={0};int reachable=-1;
        int vertical=i&1;float side=(i&2)?-1.f:1.f;
        t.axis[0][vertical?1:0]=side;t.axis[1][2]=1;
        t.length[0]=.2f+random_unit()*.3f;t.length[1]=.2f+random_unit()*.3f;
        t.reach_margin=.01f;t.min_bend=.1f;
        a.end[vertical?1:0]=t.length[0]*side;b.end[vertical?1:0]=t.length[1]*side;
        for(int k=0;k<3;k++){t.root[k]=random_unit()*2-1;t.target[k]=t.root[k]+random_unit()*2-1;t.guide[k]=random_unit()*2-1;}
        memset(c->ram+s1,0,0x100);memset(c->ram+s2,0,0x100);
        memcpy(c->ram+s1+0x30,a.end,12);memcpy(c->ram+s2+0x30,b.end,12);
        ee_w32(c,s1+0x20,vtable);ee_w32(c,s2+0x20,vtable);
        memcpy(c->ram+data,t.guide,12);memcpy(c->ram+data+0x10,t.root,12);memcpy(c->ram+data+0x20,t.target,12);
        memcpy(c->ram+data+0x30,t.axis[0],12);memcpy(c->ram+data+0x40,t.axis[1],12);putf(c->ram+data+0x50,t.min_bend);
        memset(c->r,0,sizeof c->r);memset(c->fpr,0,sizeof c->fpr);
        c->r[4].uw[0]=result;c->r[5].uw[0]=s1;c->r[6].uw[0]=s2;c->r[7].uw[0]=data;
        c->r[8].uw[0]=data+0x10;c->r[9].uw[0]=data+0x20;c->r[10].uw[0]=data+0x30;c->r[11].uw[0]=data+0x40;
        c->r[29].uw[0]=stack;ee_w32(c,stack,data+0x50);
        c->fpr[12].f=t.length[0];c->fpr[13].f=t.length[1];c->fpr[14].f=t.reach_margin;
        CHECK(invoke(c,0x420510));CHECK(gt4_driver_solve_limb(&t,&a,&b,&reachable));
        CHECK(ee_r32(c,result)==(unsigned)reachable);
        const float *native[]={a.position,a.rotation,b.position,b.rotation};
        const uint32_t addr[]={s1,s1+0x10,s2,s2+0x10};
        for(int field=0;field<4;field++)for(int k=0;k<(field&1?4:3);k++) {
            float original;memcpy(&original,c->ram+addr[field]+k*4,4);float error=fabsf(native[field][k]-original);
            if(error>max_error)max_error=error;
            if(!isfinite(original) || error>2e-5f){fprintf(stderr,"limb %d field %d component %d: native %.9g original %.9g\n",i,field,k,native[field][k],original);exit(1);}
        }
        count++;
    }
    printf("PASS %u original limb solutions and reach flags (max absolute error %.9g; VU cross/product helpers substituted).\n",count,max_error);
}
static void legs_oracle(ee_ctx *c,uint32_t object,uint32_t stack)
{
    static const unsigned offsets[20]={0,0x70,0x110,0x1b0,0xc0,0x160,0x200,0x250,0x2a0,0x2f0,0x360,0x400,0x3b0,0x450,0x4a0,0x4f0,0x540,0x590,0x5e0,0x630};
    const uint32_t input=object+0x1000,targets=object+0x1100,vtable=object+0x1200,status=object+0x1300;
    ee_w16(c,vtable+8,0);ee_w32(c,vtable+12,0x41b898);
    ee_w16(c,vtable+0x18,0);ee_w32(c,vtable+0x1c,0x41b9a8);
    memset(c->r,0,sizeof c->r);c->r[4].uw[0]=1;c->r[5].uw[0]=0xffff;c->r[29].uw[0]=stack;CHECK(invoke(c,0x4218e0));
    float max_error=0;
    for(int i=0;i<1000;i++) {
        float d[16];for(int k=0;k<16;k++)d[k]=.05f+random_unit()*.4f;
        gt4_driver_skeleton s;CHECK(gt4_driver_skeleton_init(d,&s));
        memset(c->r,0,sizeof c->r);memset(c->fpr,0,sizeof c->fpr);c->r[4].uw[0]=object;c->r[29].uw[0]=stack;
        for(int k=0;k<16;k++)if(k<8)c->fpr[12+k].f=d[k];else putf(c->ram+stack+(k-8)*8,d[k]);
        CHECK(invoke(c,0x419f80));for(int b=0;b<20;b++)ee_w32(c,object+offsets[b]+0x20,vtable);
        gt4_driver_segment *pelvis=&s.bone[GT4_PELVIS];float length=0;
        for(int k=0;k<4;k++){pelvis->rotation[k]=random_unit()*2-1;length+=pelvis->rotation[k]*pelvis->rotation[k];}
        length=sqrtf(length);for(int k=0;k<4;k++)pelvis->rotation[k]/=length;
        for(int k=0;k<3;k++)pelvis->position[k]=random_unit()*2-1;
        memcpy(c->ram+object,pelvis->position,12);memcpy(c->ram+object+0x10,pelvis->rotation,16);
        float feet[2][3];for(int side=0;side<2;side++)for(int k=0;k<3;k++)feet[side][k]=pelvis->position[k]+random_unit()*2-1;
        gt4_driver_leg_controls in;
        for(int side=0;side<2;side++){in.spread[side]=random_unit();in.shank_twist[side]=random_unit();in.foot_twist[side]=random_unit();in.foot_roll[side]=random_unit()-.5f;}
        memset(c->ram+input,0,0x100);memcpy(c->ram+targets+0x20,feet[0],12);memcpy(c->ram+targets+0x30,feet[1],12);
        for(int side=0;side<2;side++){putf(c->ram+input+8+side*4,in.spread[side]);putf(c->ram+input+0x10+side*4,in.shank_twist[side]);
            putf(c->ram+input+0x30+side*4,in.foot_twist[side]);putf(c->ram+input+0x38+side*4,in.foot_roll[side]);}
        memset(c->r,0,sizeof c->r);memset(c->fpr,0,sizeof c->fpr);c->r[4].uw[0]=input;c->r[5].uw[0]=object;c->r[6].uw[0]=targets;
        c->r[8].uw[0]=status;c->r[29].uw[0]=stack;CHECK(invoke(c,0x41e678));
        int reach[2];CHECK(gt4_driver_solve_legs(&s,feet,&in,reach));
        for(int side=0;side<2;side++)CHECK(ee_r32(c,status+4+side*4)==(unsigned)reach[side]);
        for(int b=0;b<20;b++)for(int k=0;k<7;k++) {
            float native=k<3?s.bone[b].position[k]:s.bone[b].rotation[k-3];
            float original;memcpy(&original,c->ram+object+offsets[b]+(k<3?k*4:0x10+(k-3)*4),4);
            float error=fabsf(native-original);if(error>max_error)max_error=error;
            if(!isfinite(original) || error>2e-5f){fprintf(stderr,"legs %d bone %d component %d: native %.9g original %.9g\n",i,b,k,native,original);exit(1);}
        }
    }
    printf("PASS 1000 original lower-body postures, all 20 bone transforms and reach flags (max error %.9g; VU helpers substituted).\n",max_error);
}
static void chain_oracle(ee_ctx *c,uint32_t object,uint32_t stack)
{
    const uint32_t vtable=object+0x300,data=object+0x400,status=object+0x500;
    ee_w16(c,vtable+8,0);ee_w32(c,vtable+12,0x41b898);float max_error=0;
    for(int i=0;i<10000;i++) {
        gt4_driver_chain_target t={0};gt4_driver_segment chain[3]={0};
        for(int k=0;k<3;k++){t.length[k]=.05f+random_unit()*.2f;chain[k].end[1]=t.length[k];}
        for(int k=0;k<3;k++){t.root[k]=random_unit()*2-1;t.target[k]=t.root[k]+random_unit()*1.5f-.75f;t.guide[k]=random_unit()*2-1;}
        t.axis[0][1]=1;t.axis[1][2]=-1;
        for(int b=0;b<3;b++){memset(c->ram+object+b*0x100,0,0x100);memcpy(c->ram+object+b*0x100+0x30,chain[b].end,12);ee_w32(c,object+b*0x100+0x20,vtable);}
        memcpy(c->ram+data,t.guide,12);memcpy(c->ram+data+0x10,t.root,12);memcpy(c->ram+data+0x20,t.target,12);
        memcpy(c->ram+data+0x30,t.axis[0],12);memcpy(c->ram+data+0x40,t.axis[1],12);
        memset(c->r,0,sizeof c->r);memset(c->fpr,0,sizeof c->fpr);c->r[4].uw[0]=status;c->r[5].uw[0]=object;
        c->r[6].uw[0]=object+0x100;c->r[7].uw[0]=object+0x200;c->r[8].uw[0]=data;
        c->r[9].uw[0]=data+0x10;c->r[10].uw[0]=data+0x20;c->r[11].uw[0]=data+0x30;
        c->r[29].uw[0]=stack;ee_w32(c,stack,data+0x40);c->fpr[12].f=t.length[0];c->fpr[13].f=t.length[1];c->fpr[14].f=t.length[2];
        CHECK(invoke(c,0x420aa8));int reach;CHECK(gt4_driver_solve_chain(&t,chain,&reach));CHECK(ee_r32(c,status)==(unsigned)reach);
        for(int b=0;b<3;b++)for(int k=0;k<7;k++) {
            float native=k<3?chain[b].position[k]:chain[b].rotation[k-3],original;
            memcpy(&original,c->ram+object+b*0x100+(k<3?k*4:0x10+(k-3)*4),4);float error=fabsf(native-original);
            if(error>max_error)max_error=error;
            if(!isfinite(original) || error>2e-5f){fprintf(stderr,"chain %d bone %d component %d: native %.9g original %.9g\n",i,b,k,native,original);exit(1);}
        }
    }
    printf("PASS 10000 original three-segment spine solutions and reach flags (max error %.9g; VU helpers substituted).\n",max_error);
}
static void torso_oracle(ee_ctx *c,uint32_t object,uint32_t stack)
{
    static const unsigned offsets[20]={0,0x70,0x110,0x1b0,0xc0,0x160,0x200,0x250,0x2a0,0x2f0,0x360,0x400,0x3b0,0x450,0x4a0,0x4f0,0x540,0x590,0x5e0,0x630};
    const uint32_t input=object+0x1000,targets=object+0x1100,vtable=object+0x1200,look_addr=object+0x1300;
    ee_w16(c,vtable+8,0);ee_w32(c,vtable+12,0x41b898);ee_w16(c,vtable+0x18,0);ee_w32(c,vtable+0x1c,0x41b9a8);
    float max_error=0;
    for(int i=0;i<5000;i++) {
        float d[16];for(int k=0;k<16;k++)d[k]=.05f+random_unit()*.4f;
        gt4_driver_skeleton s;CHECK(gt4_driver_skeleton_init(d,&s));
        memset(c->r,0,sizeof c->r);memset(c->fpr,0,sizeof c->fpr);c->r[4].uw[0]=object;c->r[29].uw[0]=stack;
        for(int k=0;k<16;k++)if(k<8)c->fpr[12+k].f=d[k];else putf(c->ram+stack+(k-8)*8,d[k]);CHECK(invoke(c,0x419f80));
        for(int b=0;b<20;b++)ee_w32(c,object+offsets[b]+0x20,vtable);
        gt4_driver_torso_target t;float look[3],length=0;
        for(int k=0;k<4;k++){t.rotation[k]=random_unit()*2-1;length+=t.rotation[k]*t.rotation[k];}
        length=sqrtf(length);for(int k=0;k<4;k++)t.rotation[k]/=length;
        for(int k=0;k<3;k++){t.root[k]=random_unit()*2-1;t.end[k]=t.root[k]+random_unit()*2-1;t.guide[k]=random_unit()*2-1;look[k]=random_unit()*4-2;}
        gt4_driver_torso_controls in={random_unit()-.5f,random_unit()-.5f,random_unit()-.5f,random_unit(),i&1};
        memset(c->ram+input,0,0x100);memset(c->ram+targets,0,0xc0);
        putf(c->ram+input+0x50,in.twist);putf(c->ram+input+0x54,in.roll);putf(c->ram+input+0x58,in.pelvis_twist);
        putf(c->ram+input+0x74,in.look_gain);ee_w32(c,input+0xc8,in.twist_locked);
        memcpy(c->ram+targets,t.rotation,16);memcpy(c->ram+targets+0x10,t.root,12);memcpy(c->ram+targets+0x40,t.end,12);memcpy(c->ram+targets+0xb0,t.guide,12);
        memcpy(c->ram+look_addr,look,12);const float *look_ptr=(i&2)?look:NULL;
        memset(c->r,0,sizeof c->r);memset(c->fpr,0,sizeof c->fpr);c->r[4].uw[0]=input;c->r[5].uw[0]=object;c->r[6].uw[0]=targets;
        c->r[7].uw[0]=look_ptr?look_addr:0;c->r[29].uw[0]=stack;CHECK(invoke(c,0x41cb20));
        CHECK(gt4_driver_solve_torso(&s,&t,&in,look_ptr));
        for(int b=0;b<20;b++)for(int k=0;k<7;k++) {
            float native=k<3?s.bone[b].position[k]:s.bone[b].rotation[k-3],original;
            memcpy(&original,c->ram+object+offsets[b]+(k<3?k*4:0x10+(k-3)*4),4);float error=fabsf(native-original);if(error>max_error)max_error=error;
            if(!isfinite(original) || error>2e-5f){fprintf(stderr,"torso %d bone %d component %d: native %.9g original %.9g\n",i,b,k,native,original);exit(1);}
        }
    }
    printf("PASS 5000 original torso postures, including locked twist and look targets (max error %.9g; VU helpers substituted).\n",max_error);
}
static void elbow_oracle(ee_ctx *c,uint32_t object,uint32_t stack)
{
    const uint32_t data=object+0x100,result=object+0x200;float max_error=0;
    for(int i=0;i<10000;i++) {
        gt4_driver_segment clavicle={0};float length=0,root[3],hand[3],side[3]={(i&1)?-1.f:1.f,0,0};
        for(int k=0;k<4;k++){clavicle.rotation[k]=random_unit()*2-1;length+=clavicle.rotation[k]*clavicle.rotation[k];}
        length=sqrtf(length);for(int k=0;k<4;k++)clavicle.rotation[k]/=length;
        for(int k=0;k<3;k++){root[k]=random_unit()*2-1;hand[k]=random_unit()*2-1;}
        float control=random_unit(),elevation=random_unit()-.5f,native[3];
        memcpy(c->ram+object+0x10,clavicle.rotation,16);memcpy(c->ram+data,side,12);memcpy(c->ram+data+0x10,root,12);memcpy(c->ram+data+0x20,hand,12);
        memset(c->r,0,sizeof c->r);memset(c->fpr,0,sizeof c->fpr);c->r[4].uw[0]=result;c->r[5].uw[0]=object;c->r[6].uw[0]=data;
        c->r[7].uw[0]=data+0x10;c->r[8].uw[0]=data+0x20;c->r[29].uw[0]=stack;c->fpr[12].f=control;c->fpr[13].f=elevation;
        CHECK(invoke(c,0x41c6f8));CHECK(gt4_driver_elbow_guide(native,&clavicle,side,root,hand,control,elevation));
        for(int k=0;k<3;k++){float original;memcpy(&original,c->ram+result+k*4,4);float error=fabsf(native[k]-original);if(error>max_error)max_error=error;
            /* SDK integer sqrt preserves asin near perpendicular guides. */
            if(!isfinite(original) || error>2e-6f){fprintf(stderr,"elbow %d component %d: native %.9g original %.9g\n",i,k,native[k],original);exit(1);}}
    }
    printf("PASS 10000 original elbow guides (max error %.9g; VU cross/product helpers substituted).\n",max_error);
}
static void arms_oracle(ee_ctx *c,uint32_t object,uint32_t stack)
{
    static const unsigned offsets[20]={0,0x70,0x110,0x1b0,0xc0,0x160,0x200,0x250,0x2a0,0x2f0,0x360,0x400,0x3b0,0x450,0x4a0,0x4f0,0x540,0x590,0x5e0,0x630};
    const uint32_t input=object+0x1000,targets=object+0x1100,vtable=object+0x1200,status=object+0x1300;
    ee_w16(c,vtable+8,0);ee_w32(c,vtable+12,0x41b898);ee_w16(c,vtable+0x18,0);ee_w32(c,vtable+0x1c,0x41b9a8);
    float max_error=0;
    for(int i=0;i<5000;i++) {
        float d[16];for(int k=0;k<16;k++)d[k]=.05f+random_unit()*.4f;
        gt4_driver_skeleton s;CHECK(gt4_driver_skeleton_init(d,&s));
        memset(c->r,0,sizeof c->r);memset(c->fpr,0,sizeof c->fpr);c->r[4].uw[0]=object;c->r[29].uw[0]=stack;
        for(int k=0;k<16;k++)if(k<8)c->fpr[12+k].f=d[k];else putf(c->ram+stack+(k-8)*8,d[k]);CHECK(invoke(c,0x419f80));
        for(int b=0;b<20;b++)ee_w32(c,object+offsets[b]+0x20,vtable);
        gt4_driver_segment *scapula=&s.bone[GT4_SCAPULA];float length=0;
        for(int k=0;k<4;k++){scapula->rotation[k]=random_unit()*2-1;length+=scapula->rotation[k]*scapula->rotation[k];}
        length=sqrtf(length);for(int k=0;k<4;k++)scapula->rotation[k]/=length;
        for(int k=0;k<3;k++)scapula->position[k]=random_unit()*2-1;
        memcpy(c->ram+object+0x2f0,scapula->position,12);memcpy(c->ram+object+0x300,scapula->rotation,16);
        gt4_driver_arm_target t={0};t.rotation[3]=1;
        gt4_driver_arm_controls in={0};in.auto_clavicle=i&1;in.wrist_mode=(i>>1)&1;
        in.normal_enabled[0]=(i>>2)&1;in.normal_enabled[1]=(i>>3)&1;in.simple_hand_twist=(i>>4)&1;
        for(int side=0;side<2;side++) {
            length=0;for(int k=0;k<3;k++){t.hand[side][k]=scapula->position[k]+random_unit()*2-1;t.normal[side][k]=random_unit()*2-1;length+=t.normal[side][k]*t.normal[side][k];}
            length=sqrtf(length);for(int k=0;k<3;k++)t.normal[side][k]/=length;
            if(i&32){t.wrist_axis[side][0]=-t.normal[side][1];t.wrist_axis[side][1]=t.normal[side][0];float norm=sqrtf(t.wrist_axis[side][0]*t.wrist_axis[side][0]+t.wrist_axis[side][1]*t.wrist_axis[side][1]);for(int k=0;k<2;k++)t.wrist_axis[side][k]/=norm;}
            in.elbow[side]=random_unit();in.wrist_aim[side]=(i&64)?random_unit():0;in.wrist_twist[side]=random_unit();
            in.hand_twist[side]=(i&128)?random_unit()-.5f:0;in.clavicle_angle[side]=random_unit()*.3f;in.clavicle_roll[side]=random_unit()-.5f;
        }
        memset(c->ram+input,0,0x100);memset(c->ram+targets,0,0xc0);memset(c->ram+status,0,0x20);ee_w32(c,status+0xc,77);ee_w32(c,status+0x10,88);
        const float *values[]={in.elbow,in.wrist_aim,in.wrist_twist,in.hand_twist,in.clavicle_angle,in.clavicle_roll};
        const unsigned fields[]={0,0x18,0x20,0x28,0x40,0x48};
        for(int v=0;v<6;v++)memcpy(c->ram+input+fields[v],values[v],8);
        ee_w32(c,input+0xa8,in.auto_clavicle);ee_w32(c,input+0xac,in.wrist_mode);ee_w32(c,input+0xb0,in.normal_enabled[0]);ee_w32(c,input+0xb4,in.normal_enabled[1]);ee_w32(c,input+0xcc,in.simple_hand_twist);
        memcpy(c->ram+targets,t.rotation,16);for(int side=0;side<2;side++){memcpy(c->ram+targets+0x50+side*0x10,t.hand[side],12);memcpy(c->ram+targets+0x70+side*0x10,t.normal[side],12);memcpy(c->ram+targets+0x90+side*0x10,t.wrist_axis[side],12);}
        memset(c->r,0,sizeof c->r);memset(c->fpr,0,sizeof c->fpr);c->r[4].uw[0]=input;c->r[5].uw[0]=object;c->r[6].uw[0]=targets;c->r[8].uw[0]=status;c->r[29].uw[0]=stack;
        CHECK(invoke(c,0x41d140));int reach[2]={77,88};CHECK(gt4_driver_solve_arms(&s,&t,&in,reach));
        for(int side=0;side<2;side++){CHECK(ee_r32(c,status+0xc+side*4)==(unsigned)reach[side]);near_values(&in.clavicle_angle[side],c->ram+input+0x40+side*4,1);}
        for(int b=0;b<20;b++)for(int k=0;k<7;k++) {
            float native=k<3?s.bone[b].position[k]:s.bone[b].rotation[k-3],original;
            memcpy(&original,c->ram+object+offsets[b]+(k<3?k*4:0x10+(k-3)*4),4);float error=fabsf(native-original);if(error>max_error)max_error=error;
            if(!isfinite(original) || error>2e-6f){fprintf(stderr,"arms %d bone %d component %d: native %.9g original %.9g\n",i,b,k,native,original);exit(1);}
        }
    }
    printf("PASS 5000 original arm/wrist postures, clavicle controls and reach flags (max error %.9g; VU helpers substituted).\n",max_error);
}
static void head_oracle(ee_ctx *c,uint32_t object,uint32_t stack)
{
    static const unsigned offsets[20]={0,0x70,0x110,0x1b0,0xc0,0x160,0x200,0x250,0x2a0,0x2f0,0x360,0x400,0x3b0,0x450,0x4a0,0x4f0,0x540,0x590,0x5e0,0x630};
    const uint32_t input=object+0x1000,look_addr=object+0x1100,vtable=object+0x1200,status=object+0x1300;
    ee_w16(c,vtable+8,0);ee_w32(c,vtable+12,0x41b898);ee_w16(c,vtable+0x18,0);ee_w32(c,vtable+0x1c,0x41b9a8);float max_error=0;
    for(int i=0;i<5000;i++) {
        float d[16];for(int k=0;k<16;k++)d[k]=.05f+random_unit()*.4f;
        gt4_driver_skeleton s;CHECK(gt4_driver_skeleton_init(d,&s));
        memset(c->r,0,sizeof c->r);memset(c->fpr,0,sizeof c->fpr);c->r[4].uw[0]=object;c->r[29].uw[0]=stack;
        for(int k=0;k<16;k++)if(k<8)c->fpr[12+k].f=d[k];else putf(c->ram+stack+(k-8)*8,d[k]);CHECK(invoke(c,0x419f80));
        for(int b=0;b<20;b++)ee_w32(c,object+offsets[b]+0x20,vtable);
        gt4_driver_segment *scapula=&s.bone[GT4_SCAPULA];float length=0;
        for(int k=0;k<4;k++){scapula->rotation[k]=random_unit()*2-1;length+=scapula->rotation[k]*scapula->rotation[k];}
        length=sqrtf(length);for(int k=0;k<4;k++)scapula->rotation[k]/=length;
        for(int k=0;k<3;k++)scapula->position[k]=random_unit()*2-1;
        memcpy(c->ram+object+0x2f0,scapula->position,12);memcpy(c->ram+object+0x300,scapula->rotation,16);
        gt4_driver_head_controls in={0};in.blend=random_unit();in.base_pitch=random_unit()-.5f;in.roll_gain=random_unit();in.roll_bias=random_unit()-.5f;in.yaw_range=random_unit();in.neck_yaw_locked=i&1;
        float look[3];length=0;for(int k=0;k<3;k++){in.up[k]=random_unit()*2-1;length+=in.up[k]*in.up[k];in.angles[k]=random_unit()-.5f;look[k]=random_unit()*4-2;}
        length=sqrtf(length);for(int k=0;k<3;k++)in.up[k]/=length;
        memset(c->ram+input,0,0x100);putf(c->ram+input+0x60,in.blend);putf(c->ram+input+0x64,in.base_pitch);putf(c->ram+input+0x68,in.roll_gain);putf(c->ram+input+0x6c,in.roll_bias);putf(c->ram+input+0x70,in.yaw_range);
        memcpy(c->ram+input+0x78,in.angles,12);memcpy(c->ram+input+0x90,in.up,12);ee_w32(c,input+0xd4,in.neck_yaw_locked);
        memcpy(c->ram+look_addr,look,12);ee_w32(c,status,77);const float *look_ptr=(i&2)?look:NULL;
        memset(c->r,0,sizeof c->r);memset(c->fpr,0,sizeof c->fpr);c->r[4].uw[0]=input;c->r[5].uw[0]=object;c->r[7].uw[0]=look_ptr?look_addr:0;c->r[8].uw[0]=status;c->r[29].uw[0]=stack;
        CHECK(invoke(c,0x41efe0));int reach=77;CHECK(gt4_driver_solve_head(&s,&in,look_ptr,&reach));CHECK(ee_r32(c,status)==(unsigned)reach);
        near_values(&in.blend,c->ram+input+0x60,1);near_values(in.angles,c->ram+input+0x78,3);
        for(int b=0;b<20;b++)for(int k=0;k<7;k++) {
            float native=k<3?s.bone[b].position[k]:s.bone[b].rotation[k-3],original;
            memcpy(&original,c->ram+object+offsets[b]+(k<3?k*4:0x10+(k-3)*4),4);float error=fabsf(native-original);if(error>max_error)max_error=error;
            if(!isfinite(original) || error>2e-6f){fprintf(stderr,"head %d bone %d component %d: native %.9g original %.9g\n",i,b,k,native,original);exit(1);}
        }
    }
    printf("PASS 5000 original head/neck postures, mutable angles and cone flags (max error %.9g; VU helpers substituted).\n",max_error);
}
static void posture_oracle(ee_ctx *c,uint32_t object,uint32_t stack)
{
    static const unsigned offsets[20]={0,0x70,0x110,0x1b0,0xc0,0x160,0x200,0x250,0x2a0,0x2f0,0x360,0x400,0x3b0,0x450,0x4a0,0x4f0,0x540,0x590,0x5e0,0x630};
    const uint32_t input=object+0x1000,targets=object+0x1100,vtable=object+0x1200,status=object+0x1300,look_addr=object+0x1400;
    ee_w16(c,vtable+8,0);ee_w32(c,vtable+12,0x41b898);ee_w16(c,vtable+0x18,0);ee_w32(c,vtable+0x1c,0x41b9a8);float max_error=0;
    for(int i=0;i<1000;i++) {
        float d[16];for(int k=0;k<16;k++)d[k]=.05f+random_unit()*.4f;gt4_driver_skeleton s;CHECK(gt4_driver_skeleton_init(d,&s));
        memset(c->r,0,sizeof c->r);memset(c->fpr,0,sizeof c->fpr);c->r[4].uw[0]=object;c->r[29].uw[0]=stack;
        for(int k=0;k<16;k++)if(k<8)c->fpr[12+k].f=d[k];else putf(c->ram+stack+(k-8)*8,d[k]);CHECK(invoke(c,0x419f80));for(int b=0;b<20;b++)ee_w32(c,object+offsets[b]+0x20,vtable);
        memset(c->r,0,sizeof c->r);c->r[4].uw[0]=input;c->r[29].uw[0]=stack;CHECK(invoke(c,0x41c1c8));
        memset(c->r,0,sizeof c->r);c->r[4].uw[0]=targets;c->r[29].uw[0]=stack;CHECK(invoke(c,0x418f88));
        gt4_driver_posture p;gt4_driver_pose_targets t;gt4_driver_posture_init(&p,&t);p.enabled=(unsigned)i&15;
        const float *pairs[]={p.arms.elbow,p.arms.wrist_aim,p.arms.wrist_twist,p.arms.hand_twist,p.arms.clavicle_angle,p.arms.clavicle_roll,p.legs.spread,p.legs.shank_twist,p.legs.foot_twist,p.legs.foot_roll};
        const unsigned pair_fields[]={0,0x18,0x20,0x28,0x40,0x48,8,0x10,0x30,0x38};
        for(int v=0;v<10;v++)near_values(pairs[v],c->ram+input+pair_fields[v],2);
        const float scalars[]={p.torso.twist,p.torso.roll,p.torso.pelvis_twist,p.torso.look_gain,p.head.blend,p.head.base_pitch,p.head.roll_gain,p.head.roll_bias,p.head.yaw_range};
        const unsigned scalar_fields[]={0x50,0x54,0x58,0x74,0x60,0x64,0x68,0x6c,0x70};
        for(int v=0;v<9;v++)near_values(scalars+v,c->ram+input+scalar_fields[v],1);near_values(p.head.up,c->ram+input+0x90,3);near_values(p.head.angles,c->ram+input+0x78,3);
        for(int flag=0;flag<4;flag++)ee_w32(c,input+0xb8+flag*4,(p.enabled>>flag)&1);
        float length=0,look[3];for(int k=0;k<4;k++){t.torso.rotation[k]=random_unit()*2-1;length+=t.torso.rotation[k]*t.torso.rotation[k];}
        length=sqrtf(length);for(int k=0;k<4;k++)t.torso.rotation[k]/=length;memcpy(t.arms.rotation,t.torso.rotation,16);
        for(int k=0;k<3;k++){t.torso.root[k]=random_unit()*2-1;t.torso.end[k]=t.torso.root[k]+random_unit()*2-1;t.torso.guide[k]=random_unit()*2-1;look[k]=random_unit()*4-2;}
        for(int side=0;side<2;side++)for(int k=0;k<3;k++){t.feet[side][k]=random_unit()*2-1;t.arms.hand[side][k]=random_unit()*2-1;}
        memcpy(c->ram+targets,t.torso.rotation,16);memcpy(c->ram+targets+0x10,t.torso.root,12);memcpy(c->ram+targets+0x40,t.torso.end,12);memcpy(c->ram+targets+0xb0,t.torso.guide,12);
        for(int side=0;side<2;side++){memcpy(c->ram+targets+0x20+side*0x10,t.feet[side],12);memcpy(c->ram+targets+0x50+side*0x10,t.arms.hand[side],12);}
        const float *look_ptr=(i&16)?look:NULL;memcpy(c->ram+look_addr,look,12);int reach[5]={77,88,99,111,222};for(int k=0;k<5;k++)ee_w32(c,status+k*4,(unsigned)reach[k]);
        memset(c->r,0,sizeof c->r);memset(c->fpr,0,sizeof c->fpr);c->r[4].uw[0]=input;c->r[5].uw[0]=object;c->r[6].uw[0]=targets;c->r[7].uw[0]=look_ptr?look_addr:0;c->r[8].uw[0]=status;c->r[29].uw[0]=stack;CHECK(invoke(c,0x41f560));
        int caller_mode=(i&32)?FE_UPWARD:FE_TONEAREST;CHECK(!fesetround(caller_mode));CHECK(gt4_driver_posture_solve(&s,&p,&t,look_ptr,reach));CHECK(fegetround()==caller_mode);CHECK(!fesetround(FE_TOWARDZERO));
        for(int k=0;k<5;k++)CHECK(ee_r32(c,status+k*4)==(unsigned)reach[k]);near_values(p.arms.clavicle_angle,c->ram+input+0x40,2);near_values(&p.head.blend,c->ram+input+0x60,1);near_values(p.head.angles,c->ram+input+0x78,3);
        for(int b=0;b<20;b++)for(int k=0;k<7;k++) {
            float native=k<3?s.bone[b].position[k]:s.bone[b].rotation[k-3],original;memcpy(&original,c->ram+object+offsets[b]+(k<3?k*4:0x10+(k-3)*4),4);float error=fabsf(native-original);if(error>max_error)max_error=error;
            if(!isfinite(original) || error>2e-6f){fprintf(stderr,"posture %d bone %d component %d: native %.9g original %.9g\n",i,b,k,native,original);exit(1);}
        }
    }
    printf("PASS 1000 original posture dispatches, defaults and all enable masks; scoped rounding restored (max error %.9g; VU helpers substituted).\n",max_error);
}
static void geometry_write(ee_ctx *c,uint32_t addr,const gt4_driver_geometry *g)
{
    memset(c->ram+addr,0,0x140);memcpy(c->ram+addr,g->position,12);memcpy(c->ram+addr+0x10,g->rotation,16);
    memcpy(c->ram+addr+0x20,g->feet[0],12);memcpy(c->ram+addr+0x30,g->feet[1],12);memcpy(c->ram+addr+0x40,g->seat,12);
    memcpy(c->ram+addr+0x50,g->wheel,12);
    for(int i=0;i<3;i++){memcpy(c->ram+addr+0x60+i*16,g->gear[i],12);memcpy(c->ram+addr+0xb0+i*16,g->normal[i],12);
    }
    memcpy(c->ram+addr+0xf0,g->wheel_basis,36);
    memcpy(c->ram+addr+0x90,g->rest[0],12);memcpy(c->ram+addr+0xa0,g->rest[1],12);
    putf(c->ram+addr+0xe0,g->wheel_radius);ee_w32(c,addr+0x120,g->shift_side);putf(c->ram+addr+0x124,g->shift_length);
    putf(c->ram+addr+0x128,g->shift_angle);ee_w32(c,addr+0x12c,g->hand_mode);putf(c->ram+addr+0x130,g->gear_phase);
}
static void grip_oracle(ee_ctx *c,uint32_t object,uint32_t stack)
{
    const uint32_t desc=object+0x400,angles_addr=object+0x600;
    const unsigned functions[]={0x415d38,0x415510,0x4154a0,0x415648};
    const float phases[]={0,.05f,.2f,.25f,.4f,.5f,.6f,.75f,.8f,.95f,1,-.5f,1.25f};
    float max_error=0;unsigned count=0;
    for(int i=0;i<10000;i++) {
        unsigned kind=i%4,side=(i/4)%2;gt4_driver_geometry g;gt4_driver_geometry_init(&g);
        for(int k=0;k<3;k++){g.wheel[k]=random_unit()*2-1;g.seat[k]=random_unit()*2-1;
            for(int j=0;j<3;j++){g.gear[j][k]=random_unit()*2-1;g.normal[j][k]=random_unit()*2-1;}
            for(int j=0;j<2;j++)g.rest[j][k]=random_unit()*2-1;}
        for(int j=0;j<3;j++){float inv=1.f/sqrtf((g.normal[j][0]*g.normal[j][0]+g.normal[j][1]*g.normal[j][1])+g.normal[j][2]*g.normal[j][2]);
            for(int k=0;k<3;k++)g.normal[j][k]*=inv;}
        float len=0;for(int k=0;k<4;k++){g.rotation[k]=random_unit()*2-1;len+=g.rotation[k]*g.rotation[k];}
        len=sqrtf(len);for(int k=0;k<4;k++)g.rotation[k]/=len;
        g.wheel_radius=.05f+random_unit()*.4f;g.shift_side=(i/8)%3;g.gear_phase=random_unit()*3;
        CHECK(gt4_driver_geometry_wheel(&g,g.normal[0]));geometry_write(c,desc,&g);
        memcpy(c->ram+angles_addr,g.normal[0],12);memset(c->r,0,sizeof c->r);c->r[4].uw[0]=desc;c->r[5].uw[0]=angles_addr;c->r[29].uw[0]=stack;
        CHECK(invoke(c,0x415240));for(int j=0;j<3;j++)for(int k=0;k<3;k++) {
            float original;memcpy(&original,c->ram+desc+0xf0+j*12+k*4,4);
            if(fabsf(original-g.wheel_basis[j*3+k])>2e-6f){float axis;memcpy(&axis,c->ram+0x622b58,4);fprintf(stderr,"wheel basis %d col %d k %d normal %.9g %.9g %.9g: native %.9g original %.9g axis %.9g\n",i,j,k,g.normal[0][0],g.normal[0][1],g.normal[0][2],g.wheel_basis[j*3+k],original,axis);exit(1);}
        }
        gt4_driver_grip grip={0};grip.normal_mode=(i/24)%3;
        for(int k=0;k<3;k++){grip.position[k]=random_unit();grip.normal[k]=random_unit();grip.axis[k]=random_unit();grip.shift_offset[k]=(random_unit()-.5f)*.1f;}
        grip.twist=random_unit();grip.aim=random_unit();grip.extra_twist=random_unit();grip.phase_twist=(random_unit()-.5f)*.5f;
        grip.radius_offset=(random_unit()-.5f)*.02f;grip.depth_offset=(random_unit()-.5f)*.02f;grip.shift_twist=random_unit();
        memset(c->ram+object,0,0x80);ee_w32(c,object,side);memcpy(c->ram+object+0x10,grip.position,12);memcpy(c->ram+object+0x20,grip.normal,12);memcpy(c->ram+object+0x30,grip.axis,12);
        memcpy(c->ram+object+0x40,&grip.twist,12);memcpy(c->ram+object+0x50,grip.shift_offset,12);putf(c->ram+object+0x60,grip.phase_twist);
        putf(c->ram+object+0x68,grip.radius_offset);putf(c->ram+object+0x6c,grip.depth_offset);putf(c->ram+object+0x70,grip.shift_twist);ee_w32(c,object+0x74,grip.normal_mode);
        if(kind==GT4_GRIP_FREE)ee_w32(c,object+0x50,desc+0x40);
        float angles[2]={(random_unit()-.5f)*8,(random_unit()-.5f)*8},blend=random_unit(),phase=i<104?phases[(i/8)%13]:(random_unit()-.25f)*2;
        memcpy(c->ram+angles_addr,angles,8);memset(c->r,0,sizeof c->r);memset(c->fpr,0,sizeof c->fpr);
        c->r[4].uw[0]=object;c->r[5].uw[0]=angles_addr;c->r[6].uw[0]=angles_addr+4;c->r[7].uw[0]=desc;c->r[29].uw[0]=stack;
        c->fpr[12].f=blend;c->fpr[13].f=phase;CHECK(invoke(c,functions[kind]));int flag;
        CHECK(gt4_driver_grip_solve(&grip,&g,kind,side,angles,blend,phase,&flag));CHECK(c->r[2].uw[0]==(unsigned)flag);
        const float *native[]={grip.position,grip.normal,grip.axis,&grip.twist};const unsigned offsets[]={0x10,0x20,0x30,0x40};
        for(int f=0;f<4;f++)for(int k=0;k<3;k++){float original;memcpy(&original,c->ram+object+offsets[f]+4*k,4);float error=fabsf(original-native[f][k]);if(error>max_error)max_error=error;
            if(!isfinite(original) || error>2e-5f){fprintf(stderr,"grip %d kind %u side %u phase %.9g field %d k %d: native %.9g original %.9g\n",i,kind,side,phase,f,k,native[f][k],original);exit(1);}}
        count++;
    }
    printf("PASS %u original wheel/shift/rest/free hand targets and flags (max error %.9g; VU helpers substituted).\n",count,max_error);
}
static void target_controls_write(ee_ctx *c,uint32_t addr,const gt4_driver_target_controls *in)
{
    putf(c->ram+addr+0x5c,in->twist_channel);ee_w32(c,addr+0x200,in->direct_twist);memcpy(c->ram+addr+0x210,in->phase_keys,12);
    for(int side=0;side<2;side++) {
        putf(c->ram+addr+0x220+side*4,in->angles[side][0]);putf(c->ram+addr+0x228+side*4,in->angles[side][1]);
        putf(c->ram+addr+0x230+side*4,in->blend[side]);putf(c->ram+addr+0x238+side*4,in->radius_offset[side]);
        putf(c->ram+addr+0x240+side*4,in->depth_offset[side]);putf(c->ram+addr+0x248+side*4,in->hand_twist[side]);
        memcpy(c->ram+addr+0x280+side*16,in->feet_offset[side],12);putf(c->ram+addr+0x2a0+side*4,in->clavicle_angle[side]);
        putf(c->ram+addr+0x2a8+side*4,in->clavicle_roll[side]);ee_w32(c,addr+0x2dc+side*4,in->normal_override[side]);
    }
    memcpy(c->ram+addr+0x250,in->phase_twist,24);putf(c->ram+addr+0x268,in->pitch);
    memcpy(c->ram+addr+0x2b0,in->spine_offset,12);memcpy(c->ram+addr+0x2c0,in->spine_world_offset,12);putf(c->ram+addr+0x2d0,in->length_offset);
    ee_w32(c,addr+0x2d4,in->free_hands);ee_w32(c,addr+0x2d8,in->pitch_locked);ee_w32(c,addr+0x2e4,in->normal_mode);
    for(int i=0;i<3;i++)memcpy(c->ram+addr+0x2f0+i*16,in->shift_offset[i],12);
    memcpy(c->ram+addr+0x320,in->phase_amplitude,24);memcpy(c->ram+addr+0x340,in->root_offset,12);putf(c->ram+addr+0x350,in->phase);
}
static void targets_oracle(ee_ctx *c,uint32_t object,uint32_t stack)
{
    static const unsigned bone_offsets[20]={0,0x70,0x110,0x1b0,0xc0,0x160,0x200,0x250,0x2a0,0x2f0,0x360,0x400,0x3b0,0x450,0x4a0,0x4f0,0x540,0x590,0x5e0,0x630};
    const uint32_t state=object+0x1000,desc=object+0x1500,out_addr=object+0x1700;float max_error=0;
    for(int i=0;i<2000;i++) {
        float dims[16];for(int k=0;k<16;k++)dims[k]=.05f+random_unit()*.4f;
        gt4_driver_skeleton s;CHECK(gt4_driver_skeleton_init(dims,&s));
        memset(c->r,0,sizeof c->r);memset(c->fpr,0,sizeof c->fpr);c->r[4].uw[0]=object;c->r[29].uw[0]=stack;
        for(int k=0;k<16;k++)if(k<8)c->fpr[12+k].f=dims[k];else putf(c->ram+stack+(k-8)*8,dims[k]);CHECK(invoke(c,0x419f80));
        for(int b=0;b<20;b++)ee_w32(c,object+bone_offsets[b]+0x20,0x686570);
        gt4_driver_geometry g;gt4_driver_geometry_init(&g);g.shift_side=i%3;g.hand_mode=(i/3)%2;g.gear_phase=random_unit()*3;g.wheel_radius=.15f+random_unit()*.2f;
        for(int k=0;k<3;k++){g.seat[k]=random_unit()-.5f;g.wheel[k]=random_unit()-.5f;g.position[k]=random_unit()*2-1;
            for(int j=0;j<3;j++)g.gear[j][k]=random_unit()-.5f;for(int j=0;j<2;j++){g.feet[j][k]=random_unit()-.5f;g.rest[j][k]=random_unit()-.5f;}}
        float len=0;for(int k=0;k<4;k++){g.rotation[k]=random_unit()*2-1;len+=g.rotation[k]*g.rotation[k];}len=sqrtf(len);for(int k=0;k<4;k++)g.rotation[k]/=len;
        float normal[3]={0,.5f,-sqrtf(.75f)};CHECK(gt4_driver_geometry_wheel(&g,normal));geometry_write(c,desc,&g);
        gt4_driver_posture p;gt4_driver_pose_targets out;gt4_driver_posture_init(&p,&out);p.enabled=(unsigned)i&15;
        gt4_driver_target_controls in;gt4_driver_targets_init(&in);in.phase=random_unit()*3.5f;in.pitch=(random_unit()-.5f)*1.2f;
        in.free_hands=(i/6)%2;in.pitch_locked=(i/12)%2;in.direct_twist=(i/24)%2;in.normal_mode=(i/48)%3;in.twist_channel=(random_unit()-.5f)*.4f;
        for(int j=0;j<3;j++){in.phase_keys[j]=random_unit()*2-1;for(int k=0;k<3;k++)in.shift_offset[j][k]=(random_unit()-.5f)*.1f;
            for(int side=0;side<2;side++){in.phase_twist[j][side]=(random_unit()-.5f)*.1f;in.phase_amplitude[j][side]=(random_unit()-.5f)*.1f;}}
        in.length_offset=(random_unit()-.5f)*.1f;
        for(int k=0;k<3;k++){in.spine_offset[k]=(random_unit()-.5f)*.1f;in.spine_world_offset[k]=(random_unit()-.5f)*.1f;in.root_offset[k]=(random_unit()-.5f)*.1f;}
        for(int side=0;side<2;side++) {
            for(int k=0;k<2;k++)in.angles[side][k]=(random_unit()-.5f)*4;in.blend[side]=random_unit();
            in.radius_offset[side]=(random_unit()-.5f)*.02f;in.depth_offset[side]=(random_unit()-.5f)*.02f;in.hand_twist[side]=(random_unit()-.5f)*.1f;
            in.clavicle_angle[side]=(random_unit()-.5f)*.1f;in.clavicle_roll[side]=(random_unit()-.5f)*.1f;in.normal_override[side]=(i/144+side)%3;
            for(int k=0;k<3;k++)in.feet_offset[side][k]=(random_unit()-.5f)*.1f;
        }
        memset(c->r,0,sizeof c->r);c->r[4].uw[0]=state;c->r[29].uw[0]=stack;CHECK(invoke(c,0x416b58));target_controls_write(c,state,&in);
        ee_w32(c,state+0xb8,!!(p.enabled&1));ee_w32(c,state+0xbc,!!(p.enabled&2));ee_w32(c,state+0xc0,!!(p.enabled&4));ee_w32(c,state+0xc4,!!(p.enabled&8));
        memset(c->ram+out_addr,0,0xc0);memset(c->r,0,sizeof c->r);c->r[4].uw[0]=out_addr;c->r[29].uw[0]=stack;CHECK(invoke(c,0x418f88));
        memset(c->r,0,sizeof c->r);memset(c->fpr,0,sizeof c->fpr);c->r[4].uw[0]=state;c->r[5].uw[0]=object;c->r[6].uw[0]=desc;c->r[7].uw[0]=out_addr;c->r[29].uw[0]=stack;
        CHECK(invoke(c,0x4173b0));CHECK(gt4_driver_targets_build(&s,&p,&g,&in,&out));
        const float *fields[]={out.torso.rotation,out.torso.root,out.feet[0],out.feet[1],out.torso.end,out.arms.hand[0],out.arms.hand[1],out.arms.normal[0],out.arms.normal[1],out.arms.wrist_axis[0],out.arms.wrist_axis[1],out.torso.guide};
        for(int f=0;f<12;f++)for(int k=0;k<(f==0?4:3);k++) {
            float original;memcpy(&original,c->ram+out_addr+f*16+k*4,4);float error=fabsf(original-fields[f][k]);if(error>max_error)max_error=error;
            if(!isfinite(original) || error>2e-5f){fprintf(stderr,"target %d mask %u field %d k %d native %.9g original %.9g\n",i,p.enabled,f,k,fields[f][k],original);exit(1);}
        }
        const float *control_fields[]={p.arms.wrist_aim,p.arms.wrist_twist,p.arms.hand_twist,p.arms.clavicle_angle,p.arms.clavicle_roll};
        const unsigned offsets[]={0x18,0x20,0x28,0x40,0x48};
        for(int f=0;f<5;f++)near_values(control_fields[f],c->ram+state+offsets[f],2);
        CHECK(p.arms.auto_clavicle==(int)ee_r32(c,state+0xa8));CHECK(p.arms.wrist_mode==(int)ee_r32(c,state+0xac));
        for(int side=0;side<2;side++)CHECK(p.arms.normal_enabled[side]==(int)ee_r32(c,state+0xb0+side*4));
    }
    printf("PASS 2000 original vehicle target builders and mutable controls, all enable masks (max error %.9g; VU helpers substituted).\n",max_error);
}
static void hand_motion_oracle(ee_ctx *c,uint32_t object,uint32_t stack)
{
    memset(c->r,0,sizeof c->r);c->r[4].uw[0]=0x846468;c->r[29].uw[0]=stack;CHECK(invoke(c,0x44f5a0));
    for(int i=0;i<12000;i++) {
        unsigned side=i&1,other_state=(i/9)%9;
        gt4_driver_hand_motion h={0},other={0};h.state=i%9;
        h.first=(random_unit()*2-1)*2.8f;h.second=(random_unit()*2-1)*2.8f;
        h.working=h.first;h.blend=random_unit();h.travel=.1f+fabsf(h.first-h.second);
        h.radius=random_unit()*.1f;h.depth=random_unit()*.1f;
        other.first=(random_unit()*2-1)*2.8f;other.second=(random_unit()*2-1)*2.8f;other.blend=random_unit();
        float values[]={h.first,h.second,h.blend,h.working,h.radius,h.depth,h.travel,other.first,other.second,other.blend,0,0};
        memcpy(c->ram+object,values,sizeof values);ee_w32(c,object+40,h.state);ee_w32(c,object+44,other_state);
        memset(c->r,0,sizeof c->r);memset(c->fpr,0,sizeof c->fpr);
        c->r[4].uw[0]=side;c->r[5].uw[0]=i&2;c->r[6].uw[0]=object;c->r[7].uw[0]=object+4;
        c->r[8].uw[0]=object+8;c->r[9].uw[0]=object+12;c->r[10].uw[0]=object+16;c->r[11].uw[0]=object+20;
        const unsigned args[]={24,28,32,36,0,0,0,0,40,44};
        for(int k=0;k<10;k++)ee_w32(c,stack+k*8,object+args[k]);c->r[29].uw[0]=stack;
        float delta=random_unit()-.5f,rate=random_unit()-.5f,wheel_rate=(random_unit()*2-1)*20;
        c->fpr[12].f=delta;c->fpr[13].f=rate;c->fpr[14].f=1.f/60;c->fpr[15].f=wheel_rate;
        CHECK(invoke(c,0x40fc78));CHECK(gt4_driver_hand_step(&h,side,i&2,&other,&other_state,delta,rate,1.f/60,wheel_rate));
        const float result[]={h.first,h.second,h.blend,h.working,h.radius,h.depth,h.travel};
        for(int k=0;k<7;k++){float original;memcpy(&original,c->ram+object+4*k,4);
            if(fabsf(result[k]-original)>2e-6f){fprintf(stderr,"hand motion i=%d state=%u field=%d native=%g original=%g\n",i,i%9,k,result[k],original);exit(1);}}
        CHECK(h.state==ee_r32(c,object+40));CHECK(other_state==ee_r32(c,object+44));
    }
    printf("PASS 12000 original hand regrip updates, all nine states and both sides.\n");
    for(int i=0;i<12000;i++) {
        unsigned side=i&1,type=i&2?3:4,os=(i/3)%3;
        gt4_driver_hand_motion h={0};h.state=i%3;h.first=h.working=random_unit()*5-2.5f;
        h.second=random_unit()*5-2.5f;h.blend=random_unit();h.radius=random_unit();h.depth=random_unit();
        float values[]={h.first,h.second,h.blend,h.working,h.radius,h.depth,0,0,0,0,0,0};memcpy(c->ram+object,values,sizeof values);
        ee_w32(c,object+40,h.state);ee_w32(c,object+44,os);
        memset(c->r,0,sizeof c->r);memset(c->fpr,0,sizeof c->fpr);
        c->r[4].uw[0]=side;c->r[5].uw[0]=type==3;c->r[6].uw[0]=object;c->r[7].uw[0]=object+4;
        c->r[8].uw[0]=object+8;c->r[9].uw[0]=object+12;c->r[10].uw[0]=object+16;c->r[11].uw[0]=object+20;
        for(int k=0;k<10;k++)ee_w32(c,stack+k*8,object+(k==8?40:k==9?44:24));c->r[29].uw[0]=stack;
        float steering=random_unit()*12-6,delta=random_unit()-.5f,wheel_rate=random_unit()*40-20;
        c->fpr[12].f=steering;c->fpr[13].f=delta;c->fpr[14].f=1.f/60;c->fpr[15].f=wheel_rate;
        CHECK(invoke(c,type==3?0x40f888:0x40f850));CHECK(gt4_driver_classic_hand_step(&h,side,type,&os,steering,delta,1.f/60,wheel_rate));
        const float native[]={h.first,h.second,h.blend,h.working,h.radius,h.depth};near_values(native,c->ram+object,6);
        CHECK(h.state==ee_r32(c,object+40));CHECK(os==ee_r32(c,object+44));
    }
    puts("PASS 12000 original Patent/Carriage steering hand updates.");
    for(int i=0;i<12000;i++) {
        int tr[4]={i%5,i%3,-1,6},gear=i%7;
        gt4_driver_lever_motion l={{(random_unit()*2-1)*1.5f,(random_unit()*2-1)*.9f},
                                   {(random_unit()*2-1)*10,(random_unit()*2-1)*15},random_unit()*2};
        if(i%3==0)l.gate[1]=0;
        float values[]={l.gate[0],l.gate[1],l.rate[0],l.rate[1],l.wait,0,0};memcpy(c->ram+object,values,sizeof values);
        memset(c->r,0,sizeof c->r);memset(c->fpr,0,sizeof c->fpr);
        for(int k=0;k<4;k++)c->r[4+k].sw[0]=tr[k];c->r[8].sw[0]=gear;
        c->r[9].uw[0]=object;c->r[10].uw[0]=object+4;c->r[11].uw[0]=object+8;c->r[29].uw[0]=stack;
        const unsigned args[]={12,16,20,24};for(int k=0;k<4;k++)ee_w32(c,stack+k*8,object+args[k]);c->fpr[12].f=1.f/60;
        CHECK(invoke(c,0x411628));int done=gt4_driver_lever_step(&l,tr,gear,1.f/60);
        const float result[]={l.gate[0],l.gate[1],l.rate[0],l.rate[1],l.wait,l.roll[0],l.roll[1]};
        for(int k=0;k<7;k++){float original;memcpy(&original,c->ram+object+4*k,4);
            if(fabsf(result[k]-original)>2e-6f){fprintf(stderr,"lever i=%d field=%d native=%.9g original=%.9g\n",i,k,result[k],original);exit(1);}}
        CHECK((unsigned)done==c->r[2].uw[0]);
    }
    printf("PASS 12000 original lever gate updates, all transmission kinds.\n");
}
/* Vehicle anticipation and CARINFO x getters are fixture inputs. All ten
   00412018 gear states, springs, gates and clutch execute original code. */
static void gear_motion_oracle(ee_ctx *c,uint32_t object,uint32_t stack)
{
    uint32_t channel=object+0x1000,rates=object+0x1100,state=object+0x1200,
        tr=object+0x1300,vehicle=object+0x2000,provider=object+0x1400,vt=object+0x1500;
    gt4_driver_car car={0};car.geometry_offset=1;car.type=1;car.wheel[0]=-.3f;car.shift[0]=.1f;
    car.wheel_normal[2]=-1;car.wheel_radius=.2f;
    for(int i=0;i<10000;i++) {
        gt4_driver_animation a;int transmission[4]={i&1?1:4,1,-1,6};CHECK(gt4_driver_animation_init(&a,&car,transmission,1));
        a.shift_state=i%10;a.gear=i%6+1;a.channels[0x1c/4]=random_unit();a.phase_rate=random_unit();
        a.hold=random_unit();a.turn_hold=0;
        a.lever.gate[0]=(random_unit()*2-1);a.lever.gate[1]=(random_unit()*2-1);
        if(i%3==0)a.lever.gate[1]=0;
        a.lever.rate[0]=random_unit()*4-2;a.lever.rate[1]=random_unit()*4-2;a.lever.wait=random_unit()*2;
        a.channels[0x20/4]=a.lever.gate[0];a.channels[0x24/4]=a.lever.gate[1];
        for(int k=0;k<2;k++){a.lever_roll[k]=a.channels[(0x28+k*4)/4]=random_unit()*.04f;a.lever_roll_rate[k]=random_unit()*.1f;}
        a.channels[0x30/4]=random_unit();
        memset(c->ram+object,0,0x3000);
        memcpy(c->ram+channel,a.channels,sizeof a.channels);memcpy(c->ram+tr,transmission,sizeof transmission);
        ee_w32(c,object,object+0x400);ee_w32(c,object+8,object+0x408);ee_w32(c,object+12,object+0x40c);
        ee_w32(c,object+16,object+0x410);ee_w32(c,object+20,provider);ee_w32(c,object+24,vehicle);
        ee_w32(c,object+0x400,channel);ee_w32(c,object+0x408,rates);ee_w32(c,object+0x40c,state);ee_w32(c,object+0x410,tr);
        ee_w32(c,provider,vt);ee_w32(c,vt+0x14,0xf00000);ee_w32(c,vt+0x94,0xf00008);
        ee_w32(c,state,a.gear);ee_w32(c,state+0x18,a.shift_state);
        putf(c->ram+rates+0x1c,a.phase_rate);putf(c->ram+rates+0x20,a.lever.rate[0]);putf(c->ram+rates+0x24,a.lever.rate[1]);
        putf(c->ram+rates+0x28,a.lever_roll_rate[0]);putf(c->ram+rates+0x2c,a.lever_roll_rate[1]);
        putf(c->ram+rates+0x6c,a.hold);putf(c->ram+rates+0x70,a.lever.wait);
        motion_input=(gt4_driver_animation_input){0};motion_input.dt=1.f/60;motion_input.gear=(i/10)%7;
        motion_input.upshift=i%3==0;motion_input.downshift=i%5==0;motion_input.speed_shift=i%7==0;
        c->ram[vehicle+0x624]=(uint8_t)motion_input.gear;putf(c->ram+object+0x110,motion_input.dt);
        memset(c->r,0,sizeof c->r);c->r[4].uw[0]=object;c->r[29].uw[0]=stack;
        CHECK(invoke(c,0x412018));CHECK(gt4_driver_animation_step(&a,&car,&motion_input));
        for(int k=0;k<6;k++){float original;memcpy(&original,c->ram+channel+0x1c+k*4,4);
            if(fabsf(a.channels[0x1c/4+k]-original)>2e-6f){fprintf(stderr,"gear i=%d state=%d ch=%x native=%g original=%g\n",i,i%10,0x1c+k*4,a.channels[0x1c/4+k],original);exit(1);}}
        CHECK(a.gear==(int)ee_r32(c,state));CHECK(a.shift_state==ee_r32(c,state+0x18));
        const float native[]={a.phase_rate,a.lever.rate[0],a.lever.rate[1],a.lever_roll_rate[0],a.lever_roll_rate[1]};
        near_values(native,c->ram+rates+0x1c,5);
        near_values(&a.hold,c->ram+rates+0x6c,1);near_values(&a.lever.wait,c->ram+rates+0x70,1);
        car.shift_type=4;motion_input.throttle=i%3==0?1:0;motion_input.brake=i%3==1?1:0;
        putf(c->ram+vehicle+0x58c,motion_input.throttle);putf(c->ram+vehicle+0x590,motion_input.brake);
        /* Reset to the same input before comparing the classic selector. */
        memcpy(c->ram+channel,a.channels,sizeof a.channels);putf(c->ram+rates+0x24,a.lever.rate[1]);
        c->r[4].uw[0]=object;c->r[29].uw[0]=stack;CHECK(invoke(c,0x413fc8));
        CHECK(gt4_driver_animation_step(&a,&car,&motion_input));
        near_values(a.channels+0x1c/4,c->ram+channel+0x1c,3);near_values(a.channels+0x30/4,c->ram+channel+0x30,1);
        near_values(a.lever.rate+1,c->ram+rates+0x24,1);
        car.shift_type=5;static const unsigned pedal_states[]={0,3,6,9};a.shift_state=pedal_states[i%4];
        a.phase_rate=random_unit()*2-1;a.channels[0xd0/4]=random_unit()*2-.5f;
        motion_input.lever_pedal=random_unit();memcpy(c->ram+channel,a.channels,sizeof a.channels);
        ee_w32(c,state+0x18,a.shift_state);putf(c->ram+rates+0xec,a.phase_rate);putf(c->ram+rates+0x24,a.lever.rate[1]);
        putf(c->ram+vehicle+0x598,motion_input.lever_pedal);
        c->r[4].uw[0]=object;c->r[29].uw[0]=stack;CHECK(invoke(c,0x414130));
        CHECK(gt4_driver_animation_step(&a,&car,&motion_input));
        near_values(a.channels+0x1c/4,c->ram+channel+0x1c,3);near_values(a.channels+0x30/4,c->ram+channel+0x30,1);
        near_values(a.channels+0xb0/4,c->ram+channel+0xb0,1);near_values(a.channels+0xd0/4,c->ram+channel+0xd0,1);
        near_values(&a.phase_rate,c->ram+rates+0xec,1);near_values(a.lever.rate+1,c->ram+rates+0x24,1);
        CHECK(a.shift_state==ee_r32(c,state+0x18));car.shift_type=0;
    }
    puts("PASS 10000 original ordinary gear transitions, springs and clutch (vehicle anticipation fixture inputs).");
    puts("PASS 10000 original Patent throttle/brake lever updates (live selector mode).");
    puts("PASS 10000 original type-five pedal lever transitions and wrist controls.");
}
static int finger_negative;
static void finger_oracle(ee_ctx *c,uint32_t stack)
{
    /* Scalar original controller, including its original parameter block.
       These generated diagnostic functions never enter the native renderer. */
    memset(c->r,0,sizeof c->r);c->r[4].uw[0]=0x846468;c->r[29].uw[0]=stack;CHECK(invoke(c,0x44f5a0));
    unsigned cases=0;int mode=fegetround();CHECK(!fesetround(FE_TOWARDZERO));
    for(unsigned hand=0;hand<14;hand++)for(unsigned gear=0;gear<40;gear++)for(int sample=0;sample<32;sample++) {
        float phase=sample%4==0?0:sample%4==1?.95f:random_unit(),other=sample%3==0?0:random_unit();
        float value=sample%4==0?0:sample%4==1?1:random_unit(),dt=sample&1?1.f/60:1.f/120;
        memset(c->r,0,sizeof c->r);c->r[4].uw[0]=hand;c->r[5].uw[0]=gear;c->r[29].uw[0]=stack;
        c->fpr[12].f=phase;c->fpr[13].f=other;c->fpr[14].f=value;c->fpr[15].f=dt;
        CHECK(invoke(c,0x4104b8));CHECK(gt4_driver_grasp_step(hand,gear,phase,other,dt,&value));
        if(finger_negative && !cases)value+=.125f; /* deliberate comparator mutation */
        CHECK(!memcmp(&value,&c->fpr[0].f,4));cases++;
    }CHECK(!fesetround(mode));printf("PASS %u original finger closure updates, bit identical.\n",cases);
}
int main(int argc,char **argv)
{
    CHECK(argc==2 || (argc==3 && !strcmp(argv[2],"--finger-negative")));finger_negative=argc==3;
    ee_ctx c;CHECK(!ee_init(&c));c.lookup=oracle_lookup;c.hook=vu_hook;
    CHECK(ee_load_image(&c,argv[1],0x100000)>0x5a48f0);
    const uint32_t object=0x1800000,stack=0x1ef0000,matrix=0x1810000;
    hand_motion_oracle(&c,object,stack);
    gear_motion_oracle(&c,object,stack);
    unsigned bones=0,matrices=0;
    for(int t=0;t<1017;t++) {
        float d[16]={0};for(int k=0;k<16;k++)d[k]=t<16?(t==k?1.f:0.f):t==16?0.f:.01f+random_unit();
        gt4_driver_skeleton s;CHECK(gt4_driver_skeleton_init(d,&s));
        memset(c.r,0,sizeof c.r);memset(c.fpr,0,sizeof c.fpr);memset(c.ram+object,0xa5,0x700);
        c.r[4].uw[0]=object;c.r[29].uw[0]=stack;
        for(int k=0;k<16;k++)if(k<8)c.fpr[12+k].f=d[k];else putf(c.ram+stack+(k-8)*8,d[k]);
        CHECK(invoke(&c,0x419f80));
        for(int b=0;b<GT4_DRIVER_BONES;b++) {
            memset(c.r,0,sizeof c.r);c.r[4].uw[0]=object;c.r[5].uw[0]=(unsigned)b;c.r[29].uw[0]=stack;
            CHECK(invoke(&c,0x41a988));uint32_t p=c.r[2].uw[0];CHECK(p>=object && p<object+0x680);
            near_values(s.bone[b].position,c.ram+p,3);near_values(s.bone[b].rotation,c.ram+p+0x10,4);
            near_values(s.bone[b].end,c.ram+p+0x30,3);near_values(&s.bone[b].length,c.ram+p+0x40,1);
            near_values(&s.bone[b].inverse_length,c.ram+p+0x44,1);
            if(b==GT4_PELVIS || b==GT4_SCAPULA){near_values(s.bone[b].branch[0],c.ram+p+0x50,3);near_values(s.bone[b].branch[1],c.ram+p+0x60,3);}bones++;
            /* Exercise arbitrary quaternion and translation, not just identity. */
            for(int k=0;k<4;k++){s.bone[b].rotation[k]=random_unit()*2-1;putf(c.ram+p+0x10+k*4,s.bone[b].rotation[k]);}
            memset(c.r,0,sizeof c.r);c.r[4].uw[0]=p;c.r[5].uw[0]=matrix;c.r[29].uw[0]=stack;CHECK(invoke(&c,0x418a58));
            float poses[GT4_DRIVER_BONES][16];CHECK(gt4_driver_skeleton_matrices(&s,poses));near_values(poses[b],c.ram+matrix,16);matrices++;
        }
    }
    unsigned controls=0;
    const uint32_t posture=object,input=0x1820000,channels=0x1821000;
    for(unsigned type=0;type<256;type++)for(int turn=-100;turn<=100;turn++) {
        gt4_driver_control_input in={turn*.0625f,random_unit(),{random_unit(),random_unit()},random_unit(),random_unit()};
        gt4_driver_controls state={0};state.grasp[0]=.375f;state.grasp[1]=.75f;
        memset(c.ram+posture,0xa5,0x80);ee_w32(&c,input,channels);
        putf(c.ram+channels+0x14,in.hand_grasp);putf(c.ram+channels+0x20,in.pedal[0]);
        putf(c.ram+channels+0x24,in.pedal[1]);putf(c.ram+channels+0x34,in.steering);
        putf(c.ram+channels+0xc8,in.grasp_right);putf(c.ram+channels+0xd8,in.grasp_left);
        putf(c.ram+posture+0x58,state.grasp[0]);putf(c.ram+posture+0x5c,state.grasp[1]);
        memset(c.r,0,sizeof c.r);c.r[4].uw[0]=type;c.r[5].uw[0]=posture;c.r[6].uw[0]=input;c.r[29].uw[0]=stack;
        CHECK(invoke(&c,0x3b1db0));CHECK(gt4_driver_controls_update(type,&in,&state));
        CHECK(ee_r32(&c,posture)==(unsigned)state.steering_enabled);
        CHECK(ee_r32(&c,posture+0x54)==(unsigned)state.grasp_enabled);
        CHECK(!memcmp(c.ram+posture+4,&state.steering,4));CHECK(!memcmp(c.ram+posture+0xc,state.pedal,8));
        CHECK(!memcmp(c.ram+posture+0x58,state.grasp,8));controls++;
    }
    printf("PASS %u original steering/pedal/grasp updates, all output fields bit identical.\n",controls);
    printf("PASS %u original skeleton bones and %u quaternion matrices (absolute tolerance 2e-6).\n",bones,matrices);
    limb_oracle(&c,object,stack);legs_oracle(&c,object,stack);chain_oracle(&c,object,stack);torso_oracle(&c,object,stack);elbow_oracle(&c,object,stack);arms_oracle(&c,object,stack);head_oracle(&c,object,stack);posture_oracle(&c,object,stack);grip_oracle(&c,object,stack);targets_oracle(&c,object,stack);finger_oracle(&c,stack);ee_free(&c);return 0;
}
