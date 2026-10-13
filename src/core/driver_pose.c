/* Native human geometry, NTSC-U v2.00 00419F80/00419D80/0041B778.
   No original executable code or EE runtime is used by this implementation. */
#include "driver_pose.h"
#include <math.h>
#include <string.h>
#include <fenv.h>
#ifdef _MSC_VER
#pragma fenv_access(on)
#else
#pragma STDC FENV_ACCESS ON
#endif
static float constant(unsigned bits){float f;memcpy(&f,&bits,4);return f;}
static unsigned float_bits(float f){unsigned bits;memcpy(&bits,&f,4);return bits;}
/* 0059F050's SDK square root rounds with integer remainder bits, regardless
   of the FPU rounding mode. Used inside asin, unlike the hardware sqrt in IK. */
static float sdk_sqrt(float x)
{
    unsigned bits=float_bits(x),absolute=bits&0x7fffffff;
    if(absolute<=0x7fffff)return x;if(bits>>31)return NAN;
    int exponent=(int)(bits>>23)-127;
    unsigned mantissa=(bits&0x7fffff)|0x800000;
    mantissa<<=(unsigned)exponent&1;exponent=exponent>=0?exponent/2:-((-exponent+1)/2);
    mantissa<<=1;unsigned partial=0,result=0;
    for(unsigned bit=0x1000000;bit;bit>>=1) {
        unsigned candidate=partial+bit;
        if(mantissa>=candidate){mantissa-=candidate;partial=candidate+bit;result+=bit;}
        mantissa<<=1;
    }
    if(mantissa)result+=result&1;
    return constant(0x3f000000+(result>>1)+(unsigned)(exponent*0x800000));
}
static float dot3(const float a[3],const float b[3])
{return (a[0]*b[0]+a[1]*b[1])+a[2]*b[2];}
static void cross3(float out[3],const float a[3],const float b[3])
{float v[3]={a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};memcpy(out,v,sizeof v);}
static void normalize3(float v[3])
{
    float l2=dot3(v,v);
    /* EE rsqrt(0) saturates; the following zero products stay zero. */
    float inv=l2==0?0:1.f/sqrtf(l2);
    for(int k=0;k<3;k++)v[k]*=inv;
}
/* 0057D8A0. Keep the SDK's range reduction and polynomial, rather than
   substituting host sin/cos. Local to the driver; no physics dependency. */
static void pose_sincos(float x,float *sn,float *cs)
{
    float pi=constant(0x40490fdb),half=constant(0x3fc90fdb);
    int neg=(int)(float_bits(x)>>31),k=(int)(x*constant(0x3ea2f983));
    float xs=x+(neg?pi:0),n=(float)k,r=(xs+0)-n*pi,rc=(pi-xs)+n*pi;
    float sign=((neg^k)&1)?-1.f:1.f,u=(xs-half)-n*pi;
    if(rc<r)r=rc;
    float u2=u*u,a=u*-sign,r2=r*r,b=r*sign,u4=u2*u2,r4=r2*r2;
    float au2=u2*a,au4=u4*a,au6=u4*au2,au8=au4*u4;
    float br2=r2*b,br4=r4*b,br6=r4*br2,br8=br4*r4;
    float acc=a+0;acc+=constant(0xbe2aaaa4)*au2;acc+=constant(0x3c08873e)*au4;
    acc+=constant(0xb94fb21f)*au6;*cs=acc+constant(0x362e9c14)*au8;
    acc=b+0;acc+=constant(0xbe2aaaa4)*br2;acc+=constant(0x3c08873e)*br4;
    acc+=constant(0xb94fb21f)*br6;*sn=acc+constant(0x362e9c14)*br8;
}
/* 0059E890, the original single-precision arcsine. Callers clamp to [-1,1]. */
static float pose_asin(float x)
{
    unsigned bits=float_bits(x),ab=bits&0x7fffffff;
    float hi=constant(0x3fc90fda),lo=constant(0x33a22168);
    if(ab==0x3f800000)return x*hi+x*lo;
    if(ab>0x3f800000)return NAN;
    if(ab<=0x31ffffff)return x;
    float t=ab<0x3f000000?x*x:(1-fabsf(x))*.5f;
    float p=t*constant(0x3811ef08)+constant(0x3a4f7f04);
    p=t*p+constant(0xbd241146);p=t*p+constant(0x3e4e0aa8);
    p=t*p+constant(0xbea6b08f);p=t*p+constant(0x3e2aaaaa);p=t*p;
    float q=t*constant(0x3d9dc62d)+constant(0xbf303360);
    q=t*q+constant(0x4001572c);q=t*q+constant(0xc019d138);q=t*q+1;
    float ratio=p/q;
    if(ab<0x3f000000)return x+x*ratio;
    float s=sdk_sqrt(t),result;
    if(ab>0x3f799999){float v=s+s*ratio;v=v+v;result=hi-(v-lo);}
    else {
        float w=constant(float_bits(s)&0xfffff000),c=(t-w*w)/(s+w);
        float quarter=constant(0x3f490fda),v=(s+s)*ratio-(lo-(c+c));
        result=quarter-(v-(quarter-(w+w)));
    }
    return bits>>31?-result:result;
}
static float acos_clamped(float v)
{return constant(0x3fc90fda)-pose_asin(fminf(1,fmaxf(-1,v)));}
/* 0057D2B8's scalar reduction/polynomial, used by 00420190. */
static float pose_sin(float x)
{
    float half=constant(0x3fc90fdb),shifted=x+half;
    int k=(int)(shifted*constant(0x3ea2f983)),negative=(int)(float_bits(shifted)>>31);
    float r=(x+0)-(float)(k-negative)*(half+half),r2=r*r,r4=r2*r2;
    r*=((negative^(k&1))?-1.f:1.f);
    float r3=r2*r,r5=r4*r,r7=r4*r3,r9=r5*r4;
    float value=r+0;value+=constant(0xbe2aaaa4)*r3;value+=constant(0x3c08873e)*r5;
    value+=constant(0xb94fb21f)*r7;return value+constant(0x362e9c14)*r9;
}
/* 0057D1F0 uses its own scalar cosine reduction. */
static float pose_cos(float x)
{
    int k=(int)(x*constant(0x3ea2f983)),negative=(int)(float_bits(x)>>31);
    float half=constant(0x3fc90fdb),r=(x-half)-(float)(k-negative)*(half+half),r2=r*r,r4=r2*r2;
    r*=((negative^((~k)&1))?-1.f:1.f);
    float r3=r2*r,r5=r4*r,r7=r4*r3,r9=r5*r4;
    float value=r+0;value+=constant(0xbe2aaaa4)*r3;value+=constant(0x3c08873e)*r5;
    value+=constant(0xb94fb21f)*r7;return value+constant(0x362e9c14)*r9;
}
/* 0057D008 / 0057D118's rational atan and quadrant handling. */
static float pose_atan(float x)
{
    float r=fabsf(x);int mode=0;
    if(r>constant(0x401a827a)){r=1.f/r;mode=1;}
    else if(r>constant(0x3ed413cd)){r=(1-r)/(1+r);mode=-1;}
    float r2=r*r,r4=r2*r2,r3=r2*r,r5=r3*r2;
    float denominator=1+constant(0x3f8e38e4)*r2;denominator+=constant(0x3e73cf3d)*r4;
    float inverse=1.f/denominator,result=r+0;result+=constant(0x3f471c72)*r3;result+=constant(0x3d8ab356)*r5;
    result*=inverse;
    if(mode<0)result=constant(0x3f490fdb)-result;else if(mode>0)result=constant(0x3fc90fdb)-result;
    return float_bits(x)>>31?-result:result;
}
static float pose_atan2(float y,float x)
{
    if(x==0 && y==0)return 0;int bits=0;
    if(y<0){y=-y;bits|=4;}if(x<0){x=-x;bits|=2;}
    float ratio;if(y<x)ratio=y/x;else {ratio=x/y;bits|=1;}
    float angle=pose_atan(ratio);
    if(bits&1)angle=constant(0x3fc90fda)-angle;
    if(bits&2)angle=constant(0x40490fda)-angle;
    return bits&4?-angle:angle;
}
static float wrap_angle(float angle,float lower)
{
    float delta=angle-lower,period=constant(0x40c90fda),fraction=delta/period;
    int whole=(int)fraction;if(fraction<=0 && fraction!=(float)whole)whole--;
    return (delta-(float)whole*period)+lower;
}
static int chain_angles(const float length[3],float distance,float angles[4])
{
    float a=length[0],b=length[1],c=length[2],a2=a*a,b2=b*b,c2=c*c,sum=a+c;
    float factor=sum*-2,disc=(b2*factor)*factor;
    float term=((a2+b2)+c2)-distance*distance;term-=(a+a)*c;
    disc-=((a*16)*c)*term;
    if(disc<0) {
        if(b<sum){angles[0]=angles[1]=acos_clamped(b/sum);angles[2]=constant(0x4096cbe3)-angles[0];angles[3]=constant(0x3fc90fda)-angles[0];}
        else {angles[0]=angles[1]=0;angles[2]=angles[3]=constant(0x40490fda);}
        return 0;
    }
    float centre=(-b)*factor,denominator=(a*8)*c,radical=sqrtf(disc);
    float plus=(centre+radical)/denominator,minus=(centre-radical)/denominator,cosine=-1;
    int pvalid=plus>=-1 && plus<=1,mvalid=minus>=-1 && minus<=1;
    if(pvalid && !mvalid)cosine=plus;else if(mvalid && !pvalid)cosine=minus;
    else if(pvalid && mvalid)cosine=fminf(plus,minus);
    angles[0]=angles[1]=fabsf(acos_clamped(cosine));
    float sn=pose_sin(angles[0]),u[2]={b-sum*cosine,(a-c)*sn},v[2]={-cosine,sn};
    float l2=u[0]*u[0]+u[1]*u[1],inverse=l2==0?0:1.f/sqrtf(l2);u[0]*=inverse;u[1]*=inverse;
    angles[3]=fabsf(acos_clamped(u[0]*v[0]+u[1]*v[1]));
    angles[2]=((constant(0x40c90fda)-angles[0])-angles[1])-angles[3];return 1;
}
static void quaternion_axis(float q[4],const float axis[3],float angle)
{float sn,cs;pose_sincos(angle*.5f,&sn,&cs);for(int k=0;k<3;k++)q[k]=axis[k]*sn;q[3]=cs;}
/* 0048D530: VU0 cross plus (a*bw+b*aw); w preserves the scalar add order. */
static void quaternion_mul(float out[4],const float a[4],const float b[4])
{
    float q[4];cross3(q,a,b);
    for(int k=0;k<3;k++)q[k]=(a[k]*b[3]+b[k]*a[3])+q[k];
    float w=(-a[0])*b[0];w-=a[1]*b[1];w-=a[2]*b[2];w-=a[3]*b[3];
    q[3]=(a[3]*b[3]+b[3]*a[3])+w;memcpy(out,q,sizeof q);
}
static void quaternion_turn(float q[4],const float axis[3],float angle)
{float turn[4];quaternion_axis(turn,axis,angle);quaternion_mul(q,turn,q);}
/* 00488E28, shortest quaternion blend without an added normalization. */
static void quaternion_blend(float out[4],const float from[4],const float to[4],float factor)
{
    if(factor==0){memcpy(out,from,16);return;}if(factor==1){memcpy(out,to,16);return;}
    float dot=((from[0]*to[0]+from[1]*to[1])+from[2]*to[2])+from[3]*to[3],b[4];
    memcpy(b,to,sizeof b);if(dot<0){for(int k=0;k<4;k++)b[k]=-b[k];dot=-dot;}
    float angle=acos_clamped(dot),sn=pose_sin(angle);
    if(sn==0){memcpy(out,from,16);return;}
    float inverse=1.f/sn,a_weight=pose_sin(angle*(1-factor))*inverse,b_weight=pose_sin(angle*factor)*inverse;
    float result[4];for(int k=0;k<4;k++)result[k]=from[k]*a_weight+b[k]*b_weight;
    memcpy(out,result,sizeof result);
}
/* 004225E0. Preserve the norm-dependent factor in the original local turn. */
static void quaternion_clavicle_turn(float q[4],float angle)
{
    if(angle==0)return;float sn,cs;pose_sincos(angle*.5f,&sn,&cs);
    float norm=(((q[0]*q[0]+q[1]*q[1])+q[2]*q[2])+q[3]*q[3]);norm=(norm+norm)-1;
    float delta[4]={q[1]*norm*sn,(-q[0])*norm*sn,q[3]*sn,(-q[2])*sn};
    for(int k=0;k<4;k++)q[k]=q[k]*cs+delta[k];
}
/* 00486FF8's half-diagonal formulation, alias-safe as 00421E50. */
static void quaternion_rotate(float out[3],const float q[4],const float v[3])
{
    float x=q[0],y=q[1],z=q[2],w=q[3],xx=x*x,yy=y*y,zz=z*z;
    float xy=x*y,xz=x*z,yz=y*z,xw=x*w,yw=y*w,zw=z*w;
    float a=(((.5f-yy)-zz)*v[0]+(xy-zw)*v[1])+(xz+yw)*v[2];
    float b=((xy+zw)*v[0]+((.5f-zz)-xx)*v[1])+(yz-xw)*v[2];
    float c=((xz-yw)*v[0]+(yz+xw)*v[1])+((.5f-xx)-yy)*v[2];
    float r[3]={a+a,b+b,c+c};memcpy(out,r,sizeof r);
}
static void segment_end(float out[3],const gt4_driver_segment *s)
{quaternion_rotate(out,s->rotation,s->end);for(int k=0;k<3;k++)out[k]+=s->position[k];}
static void segment_direction(float out[3],const gt4_driver_segment *s)
{quaternion_rotate(out,s->rotation,s->end);for(int k=0;k<3;k++)out[k]*=s->inverse_length;}
/* 00423AA0: signed angle after projection onto the plane normal to axis. */
static float plane_angle(const float from[3],const float to[3],const float axis[3])
{
    float a[3],b[3],cross[3],da=dot3(from,axis),db=dot3(to,axis);
    for(int k=0;k<3;k++){a[k]=from[k]-axis[k]*da;b[k]=to[k]-axis[k]*db;}
    if(dot3(a,a)==0 || dot3(b,b)==0)return 0;
    normalize3(a);normalize3(b);float angle=acos_clamped(dot3(a,b));cross3(cross,axis,a);
    return dot3(b,cross)<0?-angle:angle;
}
/* 00487450 / 00489300, column-major 3x3 with the original add order. */
static void quaternion_matrix3(float m[9],const float q[4])
{
    float x=q[0],y=q[1],z=q[2],w=q[3],x2=x+x,y2=y+y,z2=z+z;
    float xx=x2*x,yy=y2*y,zz=z2*z,xy=x2*y,xz=x2*z,yz=y2*z,xw=x2*w,yw=y2*w,zw=z2*w;
    m[0]=(1-yy)-zz;m[1]=xy+zw;m[2]=xz-yw;
    m[3]=xy-zw;m[4]=(1-zz)-xx;m[5]=yz+xw;
    m[6]=xz+yw;m[7]=yz-xw;m[8]=(1-xx)-yy;
}
static void matrix3_rotate(float out[3],const float m[9],const float v[3])
{
    float r[3];for(int k=0;k<3;k++)r[k]=(m[k]*v[0]+m[k+3]*v[1])+m[k+6]*v[2];
    memcpy(out,r,sizeof r);
}
/* 004887D8, including 00488748's antiparallel fallback. */
static void quaternion_align(float q[4],const float from[3],const float to[3])
{
    float axis[3],angle;
    if(dot3(from,to)+1>0){cross3(axis,from,to);normalize3(axis);angle=acos_clamped(dot3(from,to));}
    else {
        if(to[0]!=0){axis[0]=-to[1];axis[1]=to[0];axis[2]=0;}
        else if(to[1]!=0){axis[0]=0;axis[1]=-to[2];axis[2]=to[1];}
        else if(to[2]!=0){axis[0]=to[2];axis[1]=0;axis[2]=-to[0];}
        else {axis[0]=1;axis[1]=axis[2]=0;}
        angle=constant(0x40490fda);
    }
    quaternion_axis(q,axis,angle);
}
/* 00488B98: map two reference axes to two target axes. */
static void quaternion_basis(float q[4],const float ref[3],const float ref2[3],
                             const float target[3],const float target2[3])
{
    float source_normal[3],target_normal[3],rotated[3],turn[4],cross[3];
    cross3(source_normal,ref,ref2);normalize3(source_normal);
    cross3(target_normal,target,target2);normalize3(target_normal);
    if(dot3(source_normal,target_normal)>-.5f)quaternion_align(q,source_normal,target_normal);
    else {
        quaternion_axis(q,ref,constant(0x3fc90fda));quaternion_rotate(rotated,q,source_normal);
        quaternion_align(turn,rotated,target_normal);quaternion_mul(q,turn,q);
    }
    quaternion_rotate(rotated,q,ref);float angle=acos_clamped(dot3(rotated,target));
    cross3(cross,rotated,target);
    if(dot3(target_normal,cross)<0)angle=constant(0x40c90fda)-angle;
    quaternion_turn(q,target_normal,angle);
}
/* 00423BF8. This routine uses asin(|cross|), including for negative dot;
   preserve that original rule rather than substituting conventional slerp. */
static void vector_blend(float out[3],const float from[3],const float to[3],float factor,const float fallback[3])
{
    float axis[3],q[4],angle;cross3(axis,from,to);float length=sqrtf(dot3(axis,axis));
    if(length<=constant(0x3a83126e)) {
        if(dot3(from,to)>0){memcpy(out,from,3*sizeof(float));return;}
        if(fallback)memcpy(axis,fallback,sizeof axis);else {axis[0]=-from[1];axis[1]=from[0];axis[2]=0;}
        angle=constant(0x40490fda);
    }else {float inverse=1.f/length;for(int k=0;k<3;k++)axis[k]*=inverse;angle=pose_asin(fminf(1,fmaxf(-1,length)));}
    quaternion_axis(q,axis,angle*factor);quaternion_rotate(out,q,from);
}
static void offset(float out[3],const float p[3],float x,float y,float z)
{out[0]=p[0]+x;out[1]=p[1]+y;out[2]=p[2]+z;}
static void segment(gt4_driver_segment *s,const float a[3],const float b[3])
{
    memcpy(s->position,a,3*sizeof(float));s->rotation[3]=1;
    for(int k=0;k<3;k++)s->end[k]=b[k]-a[k];
    float length2=s->end[0]*s->end[0]+s->end[1]*s->end[1];
    length2+=s->end[2]*s->end[2];s->length=sqrtf(length2);
    s->inverse_length=s->length==0?0:1.f/s->length;
}
int gt4_driver_skeleton_init(const float d[16],gt4_driver_skeleton *out)
{
    if(!d || !out)return 0;
    for(int k=0;k<16;k++)if(!isfinite(d[k]))return 0;
    float p[26][3]={{0}};
    /* Hips, thighs, shanks and feet. Keep the original add order. */
    offset(p[1],p[0],d[0]*.5f,d[15],0);
    offset(p[5],p[0],(-d[0])*.5f,d[15],0);
    offset(p[2],p[1],0,-d[1],0);offset(p[6],p[5],0,-d[1],0);
    offset(p[3],p[2],0,-d[2],0);offset(p[7],p[6],0,-d[2],0);
    offset(p[4],p[3],0,0,d[3]);offset(p[8],p[7],0,0,d[3]);
    /* The spine and neck are a vertical chain in the authored neutral pose. */
    offset(p[9],p[0],0,d[4],0);offset(p[10],p[9],0,d[5],0);
    offset(p[11],p[10],0,d[6],0);offset(p[12],p[11],0,d[7],0);
    float clavicle=d[11]*.02f,clavicle_r=(-d[11])*.02f;
    offset(p[13],p[12],clavicle,0,0);offset(p[14],p[12],clavicle_r,0,0);
    offset(p[15],p[12],0,d[8],0);offset(p[16],p[15],0,d[9],0);
    offset(p[17],p[16],0,0,d[10]);
    /* 2% scapula branch + 48% clavicle, rather than folding to 50%. */
    offset(p[18],p[13],d[11]*.48f,0,0);
    offset(p[22],p[14],(-d[11])*.48f,0,0);
    offset(p[19],p[18],d[12],0,0);offset(p[23],p[22],-d[12],0,0);
    offset(p[20],p[19],d[13],0,0);offset(p[24],p[23],-d[13],0,0);
    offset(p[21],p[20],d[14],0,0);offset(p[25],p[24],-d[14],0,0);
    static const unsigned char ends[GT4_DRIVER_BONES][2]={
        {0,9},{1,2},{2,3},{3,4},{5,6},{6,7},{7,8},{9,10},{10,11},{11,15},
        {18,19},{19,20},{22,23},{23,24},{16,17},{13,18},{14,22},{20,21},{24,25},{15,16}};
    gt4_driver_skeleton s={0};
    for(int b=0;b<GT4_DRIVER_BONES;b++) {
        segment(&s.bone[b],p[ends[b][0]],p[ends[b][1]]);
        if(!isfinite(s.bone[b].length) || !isfinite(s.bone[b].inverse_length))return 0;
        for(int k=0;k<3;k++)if(!isfinite(s.bone[b].position[k]) || !isfinite(s.bone[b].end[k]))return 0;
    }
    for(int k=0;k<3;k++) {
        s.bone[GT4_PELVIS].branch[0][k]=p[1][k]-p[0][k];
        s.bone[GT4_PELVIS].branch[1][k]=p[5][k]-p[0][k];
        s.bone[GT4_SCAPULA].branch[0][k]=p[13][k]-p[11][k];
        s.bone[GT4_SCAPULA].branch[1][k]=p[14][k]-p[11][k];
    }
    *out=s;return 1;
}
/* 0041B048 -> 0041BB18 / 0041BE58: positions/end/branch origins
   use weighted sums; rotations use 00488E28's shortest quaternion blend. */
int gt4_driver_skeleton_blend(gt4_driver_skeleton *out,const gt4_driver_skeleton *a,const gt4_driver_skeleton *b,float t)
{
    if(!out || !a || !b || !isfinite(t))return 0;
    fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);gt4_driver_skeleton s=*a;float u=1-t;
    for(int i=0;i<GT4_DRIVER_BONES;i++){
        gt4_driver_segment *d=&s.bone[i];const gt4_driver_segment *f=&a->bone[i],*g=&b->bone[i];
        for(int j=0;j<3;j++){d->position[j]=f->position[j]*u+g->position[j]*t;d->end[j]=f->end[j]*u+g->end[j]*t;}
        quaternion_blend(d->rotation,f->rotation,g->rotation,t);
        float length2=d->end[0]*d->end[0]+d->end[1]*d->end[1];length2+=d->end[2]*d->end[2];d->length=sqrtf(length2);d->inverse_length=d->length==0?0:1/d->length;
        if(i==GT4_PELVIS || i==GT4_SCAPULA)for(int j=0;j<2;j++)for(int k=0;k<3;k++)d->branch[j][k]=f->branch[j][k]*u+g->branch[j][k]*t;
    }
    fesetenv(&env);*out=s;return 1;
}
int gt4_driver_skeleton_matrices(const gt4_driver_skeleton *s,float out[GT4_DRIVER_BONES][16])
{
    if(!s || !out)return 0;float poses[GT4_DRIVER_BONES][16];
    for(int b=0;b<GT4_DRIVER_BONES;b++) {
        const gt4_driver_segment *v=&s->bone[b];
        for(int k=0;k<3;k++)if(!isfinite(v->position[k]))return 0;
        for(int k=0;k<4;k++)if(!isfinite(v->rotation[k]))return 0;
        float x=v->rotation[0],y=v->rotation[1],z=v->rotation[2],w=v->rotation[3];
        float x2=x+x,y2=y+y,z2=z+z;
        float xx=x2*x,yy=y2*y,zz=z2*z;
        float xw=x2*w,yw=y2*w,zw=z2*w,xy=x2*y,yz=y2*z,xz=x2*z;
        float *m=poses[b];
        m[0]=(1-yy)-zz;m[1]=xy+zw;m[2]=xz-yw;m[3]=0;
        m[4]=xy-zw;m[5]=(1-zz)-xx;m[6]=yz+xw;m[7]=0;
        m[8]=xz+yw;m[9]=yz-xw;m[10]=(1-xx)-yy;m[11]=0;
        m[12]=v->position[0];m[13]=v->position[1];m[14]=v->position[2];m[15]=1;
        for(int k=0;k<16;k++)if(!isfinite(m[k]))return 0;
    }
    memcpy(out,poses,sizeof poses);return 1;
}

int gt4_driver_controls_update(unsigned type,const gt4_driver_control_input *in,gt4_driver_controls *state)
{
    if(!in || !state)return 0;
    const float values[]={in->steering,in->hand_grasp,in->pedal[0],in->pedal[1],in->grasp_left,in->grasp_right};
    for(int k=0;k<6;k++)if(!isfinite(values[k]))return 0;
    gt4_driver_controls s=*state;s.steering_enabled=1;s.steering=in->steering;
    if(type==3){s.pedal[0]=0;s.pedal[1]=in->pedal[1];}
    else if(type==4 || type==5){s.pedal[0]=in->pedal[1];s.pedal[1]=0;}
    else {s.pedal[0]=(in->pedal[1]*3.1399998664855957f)*.0625f;
          s.pedal[1]=(in->pedal[0]*3.1399998664855957f)*.0625f;}
    if(type==1) {
        s.grasp[0]=s.grasp[1]=in->hand_grasp;
        float turn=(in->steering<0?-in->steering:in->steering)/1.570796251296997f;
        turn=fminf(1,fmaxf(0,turn));float factor=1-turn*.5f;
        if(in->steering<0)s.grasp[0]*=factor;else s.grasp[1]*=factor;
        s.grasp_enabled=1;
    } else if(type==3){s.grasp[0]=in->grasp_left;s.grasp[1]=in->grasp_right;s.grasp_enabled=1;}
    else s.grasp_enabled=0;
    if(!isfinite(s.pedal[0]) || !isfinite(s.pedal[1]) ||
       (s.grasp_enabled && (!isfinite(s.grasp[0]) || !isfinite(s.grasp[1]))))return 0;
    *state=s;return 1;
}

int gt4_driver_solve_limb(const gt4_driver_limb_target *t,gt4_driver_segment *first,
                          gt4_driver_segment *second,int *reachable)
{
    if(!t || !first || !second || first==second || !reachable)return 0;
    const float *vectors[]={t->root,t->target,t->guide,t->axis[0],t->axis[1],first->end,second->end};
    for(int v=0;v<7;v++)for(int k=0;k<3;k++)if(!isfinite(vectors[v][k]))return 0;
    if(!isfinite(t->length[0]) || !isfinite(t->length[1]) || !isfinite(t->reach_margin) ||
       !isfinite(t->min_bend) || t->length[0]<=0 || t->length[1]<=0 || t->reach_margin<0 ||
       t->reach_margin>=t->length[0]+t->length[1] || t->min_bend<0 || t->min_bend>constant(0x40490fda))return 0;
    float delta[3];for(int k=0;k<3;k++)delta[k]=t->target[k]-t->root[k];
    float distance=sqrtf(dot3(delta,delta));if(!isfinite(distance))return 0;
    if(distance<constant(0x3727c5ac))distance=constant(0x3727c5ac);
    float l1=t->length[0],l2=t->length[1],sum=l1+l2,min=fabsf(l1-l2),margin=t->reach_margin;
    int reach=1;
    if(distance>=sum+margin)reach=0;
    else if(distance<min) {
        reach=0;float inverse=1.f/distance;distance=min;
        for(int k=0;k<3;k++){delta[k]*=min;delta[k]*=inverse;}
    } else if(distance>sum-margin) {
        float inverse=1.f/distance,effective=(sum-margin)+((distance-sum)+margin)*.5f;
        for(int k=0;k<3;k++){delta[k]*=effective;delta[k]*=inverse;}distance=effective;
    }
    float d2=distance*distance,l12=l1*l1,l22=l2*l2;
    float root_angle=acos_clamped(((d2+l12)-l22)/((distance+distance)*l1));
    float joint_angle=acos_clamped(((l12+l22)-d2)/((l1+l1)*l2));
    if(joint_angle<t->min_bend)joint_angle=t->min_bend;
    float unit[3],axis[3],q[4],perpendicular[3],first_end[3],second_axis[3],a[3];
    float inverse=1.f/distance;for(int k=0;k<3;k++)unit[k]=delta[k]*inverse;
    cross3(axis,unit,t->guide);normalize3(axis);
    quaternion_axis(q,axis,constant(0x3fc90fda));quaternion_rotate(perpendicular,q,unit);
    quaternion_basis(q,t->axis[0],t->axis[1],unit,perpendicular);quaternion_turn(q,axis,root_angle);
    gt4_driver_segment s1=*first,s2=*second;
    memcpy(s1.position,t->root,sizeof s1.position);memcpy(s1.rotation,q,sizeof q);
    segment_end(first_end,&s1);for(int k=0;k<3;k++)a[k]=first_end[k]-t->root[k];
    cross3(second_axis,a,unit);normalize3(second_axis);
    quaternion_turn(q,second_axis,constant(0x40490fda)-joint_angle);
    memcpy(s2.position,first_end,sizeof first_end);memcpy(s2.rotation,q,sizeof q);
    for(int k=0;k<3;k++)if(!isfinite(s1.position[k]) || !isfinite(s2.position[k]))return 0;
    for(int k=0;k<4;k++)if(!isfinite(s1.rotation[k]) || !isfinite(s2.rotation[k]))return 0;
    *first=s1;*second=s2;*reachable=reach;return 1;
}

int gt4_driver_solve_legs(gt4_driver_skeleton *s,const float feet[2][3],
                          const gt4_driver_leg_controls *controls,int reachable[2])
{
    if(!s || !feet || !controls || !reachable)return 0;
    for(int side=0;side<2;side++) {
        const float values[]={controls->spread[side],controls->shank_twist[side],controls->foot_twist[side],controls->foot_roll[side]};
        for(int k=0;k<4;k++)if(!isfinite(values[k]) || fabsf(values[k])>1000)return 0;
        for(int k=0;k<3;k++)if(!isfinite(feet[side][k]))return 0;
    }
    gt4_driver_skeleton pose=*s;gt4_driver_segment *pelvis=&pose.bone[GT4_PELVIS];
    for(int k=0;k<3;k++)if(!isfinite(pelvis->position[k]))return 0;
    for(int k=0;k<4;k++)if(!isfinite(pelvis->rotation[k]))return 0;
    float basis[9],inverse[9],hips[2][3],delta[2][3],guide[2][3],bend[2][3];
    quaternion_matrix3(basis,pelvis->rotation);
    for(int k=0;k<9;k++)inverse[k]=basis[(k%3)*3+k/3];
    for(int side=0;side<2;side++) {
        matrix3_rotate(hips[side],basis,pelvis->branch[side]);
        for(int k=0;k<3;k++){hips[side][k]+=pelvis->position[k];delta[side][k]=feet[side][k]-hips[side][k];}
    }
    for(int side=0;side<2;side++){matrix3_rotate(guide[side],inverse,delta[side]);normalize3(guide[side]);}
    const float down[3]={0,-1,0},forward[3]={0,0,1},up[3]={0,1,0},right[3]={1,0,0};
    for(int side=0;side<2;side++) {
        float q[4],align[4],v[3],angle=controls->spread[side]*(side?-.5f:.5f);
        angle*=constant(0x40490fda);quaternion_axis(q,up,angle);
        quaternion_align(align,down,guide[side]);quaternion_mul(q,align,q);
        quaternion_rotate(v,q,forward);matrix3_rotate(bend[side],basis,v);
    }
    int reach[2];const int thigh[2]={GT4_LTHIGH,GT4_RTHIGH},shank[2]={GT4_LSHANK,GT4_RSHANK},foot[2]={GT4_LFOOT,GT4_RFOOT};
    for(int side=0;side<2;side++) {
        gt4_driver_segment *a=&pose.bone[thigh[side]],*b=&pose.bone[shank[side]];
        gt4_driver_limb_target target={0};
        memcpy(target.root,hips[side],sizeof target.root);memcpy(target.target,feet[side],sizeof target.target);
        memcpy(target.guide,bend[side],sizeof target.guide);memcpy(target.axis[0],down,sizeof down);memcpy(target.axis[1],forward,sizeof forward);
        target.length[0]=a->length;target.length[1]=b->length;target.min_bend=5*constant(0x3c8efa35);
        if(!gt4_driver_solve_limb(&target,a,b,&reach[side]))return 0;
    }
    for(int side=0;side<2;side++) {
        gt4_driver_segment *sh=&pose.bone[shank[side]],*ft=&pose.bone[foot[side]];
        float direction[3],axis[3],turn[4],foot_q[4],x[3];quaternion_rotate(direction,sh->rotation,forward);
        for(int k=0;k<3;k++)axis[k]=basis[k]*(side?-1.f:1.f);
        float angle=constant(0x3fc90fda)-acos_clamped(dot3(direction,axis));
        angle=fminf(constant(0x3fa0d97a),fmaxf(0,angle));angle*=controls->shank_twist[side]*.5f;
        quaternion_rotate(axis,sh->rotation,sh->end);for(int k=0;k<3;k++)axis[k]*=sh->inverse_length;
        quaternion_axis(turn,axis,side?-angle:angle);quaternion_mul(sh->rotation,turn,sh->rotation);
        segment_end(ft->position,sh);
        /* Original applies the same twist again for the foot's orientation. */
        quaternion_mul(foot_q,turn,sh->rotation);
        quaternion_rotate(axis,foot_q,ft->end);for(int k=0;k<3;k++)axis[k]*=ft->inverse_length;
        angle=(controls->foot_twist[side]-.5f)*constant(side?0xbfa0d97a:0x3fa0d97a);
        quaternion_turn(foot_q,axis,angle);quaternion_rotate(x,foot_q,right);
        if(side)for(int k=0;k<3;k++)x[k]=-x[k];
        quaternion_turn(foot_q,x,side?-controls->foot_roll[side]:controls->foot_roll[side]);
        memcpy(ft->rotation,foot_q,sizeof foot_q);
    }
    float matrices[GT4_DRIVER_BONES][16];if(!gt4_driver_skeleton_matrices(&pose,matrices))return 0;
    *s=pose;memcpy(reachable,reach,sizeof reach);return 1;
}

int gt4_driver_solve_chain(const gt4_driver_chain_target *t,gt4_driver_segment chain[3],int *reachable)
{
    if(!t || !chain || !reachable)return 0;
    const float *vectors[]={t->root,t->target,t->guide,t->axis[0],t->axis[1],chain[0].end,chain[1].end,chain[2].end};
    for(int v=0;v<8;v++)for(int k=0;k<3;k++)if(!isfinite(vectors[v][k]))return 0;
    for(int k=0;k<3;k++)if(!isfinite(t->length[k]) || t->length[k]<=0)return 0;
    float delta[3];for(int k=0;k<3;k++)delta[k]=t->target[k]-t->root[k];
    float distance=sqrtf(dot3(delta,delta));if(!isfinite(distance) || distance==0)return 0;
    float angle[4];int reach=chain_angles(t->length,distance,angle);
    float unit[3],axis[3],q[4],perpendicular[3];float inverse=1.f/distance;
    for(int k=0;k<3;k++)unit[k]=delta[k]*inverse;
    cross3(axis,unit,t->guide);normalize3(axis);
    quaternion_axis(q,axis,constant(0x3fc90fda));quaternion_rotate(perpendicular,q,unit);
    quaternion_basis(q,t->axis[0],t->axis[1],unit,perpendicular);quaternion_turn(q,axis,angle[3]);
    gt4_driver_segment pose[3];memcpy(pose,chain,sizeof pose);
    memcpy(pose[0].position,t->root,sizeof pose[0].position);memcpy(pose[0].rotation,q,sizeof q);
    segment_end(pose[1].position,&pose[0]);
    float a[3];for(int k=0;k<3;k++)a[k]=pose[1].position[k]-t->root[k];normalize3(a);
    cross3(axis,a,unit);normalize3(axis);
    quaternion_turn(q,axis,constant(0x40490fda)-angle[0]);memcpy(pose[1].rotation,q,sizeof q);
    segment_end(pose[2].position,&pose[1]);quaternion_turn(q,axis,constant(0x40490fda)-angle[1]);memcpy(pose[2].rotation,q,sizeof q);
    for(int b=0;b<3;b++) {
        for(int k=0;k<3;k++)if(!isfinite(pose[b].position[k]))return 0;
        for(int k=0;k<4;k++)if(!isfinite(pose[b].rotation[k]))return 0;
    }
    memcpy(chain,pose,sizeof pose);*reachable=reach;return 1;
}

int gt4_driver_solve_torso(gt4_driver_skeleton *s,const gt4_driver_torso_target *t,
                           const gt4_driver_torso_controls *controls,const float look[3])
{
    if(!s || !t || !controls)return 0;
    const float *vectors[]={t->root,t->end,t->guide};
    for(int v=0;v<3;v++)for(int k=0;k<3;k++)if(!isfinite(vectors[v][k]))return 0;
    for(int k=0;k<4;k++)if(!isfinite(t->rotation[k]))return 0;
    const float values[]={controls->twist,controls->roll,controls->pelvis_twist,controls->look_gain};
    for(int k=0;k<4;k++)if(!isfinite(values[k]) || fabsf(values[k])>1000)return 0;
    if(look)for(int k=0;k<3;k++)if(!isfinite(look[k]))return 0;
    gt4_driver_skeleton pose=*s;gt4_driver_segment *pelvis=&pose.bone[GT4_PELVIS];
    gt4_driver_segment *lower=&pose.bone[GT4_LOWER_SPINE],*middle=&pose.bone[GT4_MIDDLE_SPINE],*scapula=&pose.bone[GT4_SCAPULA];
    const float forward[3]={0,0,1};float axis[3];quaternion_rotate(axis,t->rotation,forward);
    memcpy(pelvis->position,t->root,sizeof pelvis->position);memcpy(pelvis->rotation,t->rotation,sizeof pelvis->rotation);
    quaternion_turn(pelvis->rotation,axis,-controls->roll);
    gt4_driver_chain_target target={0};segment_end(target.root,pelvis);
    memcpy(target.target,t->end,sizeof target.target);memcpy(target.guide,t->guide,sizeof target.guide);
    target.axis[0][1]=1;target.axis[1][2]=-1;
    target.length[0]=lower->length;target.length[1]=middle->length;target.length[2]=scapula->length;
    gt4_driver_segment chain[3]={*lower,*middle,*scapula};int reach;
    if(!gt4_driver_solve_chain(&target,chain,&reach))return 0;
    *lower=chain[0];*middle=chain[1];*scapula=chain[2];
    if(!controls->twist_locked) {
        float ratio=(controls->twist-controls->pelvis_twist)/(pelvis->length+lower->length);
        const float angles[4]={controls->pelvis_twist,controls->pelvis_twist+ratio*pelvis->length,
                              controls->twist-ratio*lower->length,controls->twist};
        gt4_driver_segment *bones[4]={pelvis,lower,middle,scapula};
        for(int b=0;b<4;b++){segment_direction(axis,bones[b]);quaternion_turn(bones[b]->rotation,axis,angles[b]);}
        if(look) {
            float horizontal[3]={look[0]-scapula->position[0],0,look[2]-scapula->position[2]};
            float l2=dot3(horizontal,horizontal);
            if(l2>constant(0x38d1b717)) {
                float facing[3],vertical[3];quaternion_rotate(facing,scapula->rotation,forward);
                normalize3(horizontal);segment_direction(vertical,scapula);
                float angle=plane_angle(facing,horizontal,vertical)*.25f;
                if(vertical[1]<constant(0x3e4ccccc))angle*=fminf(1,fmaxf(0,5*vertical[1]));
                angle=fminf(constant(0x3eb2b8c1),fmaxf(constant(0xbeb2b8c1),angle))*controls->look_gain;
                quaternion_turn(scapula->rotation,vertical,angle);
                segment_direction(axis,middle);quaternion_turn(middle->rotation,axis,angle*.5f);
            }
        }
    }
    float l2=0;for(int k=0;k<4;k++)l2+=scapula->rotation[k]*scapula->rotation[k];
    if(!isfinite(l2) || l2==0)return 0;
    float inverse=1.f/sqrtf(l2);for(int k=0;k<4;k++)scapula->rotation[k]*=inverse;
    float matrices[GT4_DRIVER_BONES][16];if(!gt4_driver_skeleton_matrices(&pose,matrices))return 0;
    *s=pose;return 1;
}

int gt4_driver_elbow_guide(float out[3],const gt4_driver_segment *clavicle,const float side_axis[3],
                           const float root[3],const float hand[3],float elbow_control,float elevation)
{
    if(!out || !clavicle || !side_axis || !root || !hand)return 0;
    const float *vectors[]={side_axis,root,hand};for(int v=0;v<3;v++)for(int k=0;k<3;k++)if(!isfinite(vectors[v][k]))return 0;
    for(int k=0;k<4;k++)if(!isfinite(clavicle->rotation[k]))return 0;
    if(!isfinite(elbow_control) || !isfinite(elevation) || fabsf(elbow_control)>1000 || fabsf(elevation)>1000)return 0;
    float conjugate[4]={-clavicle->rotation[0],-clavicle->rotation[1],-clavicle->rotation[2],clavicle->rotation[3]};
    float inverse[9],basis[9],delta[3],unit[3],bend[3],blend[3],result[3];
    quaternion_matrix3(inverse,conjugate);for(int k=0;k<9;k++)basis[k]=inverse[(k%3)*3+k/3];
    for(int k=0;k<3;k++)delta[k]=hand[k]-root[k];matrix3_rotate(unit,inverse,delta);normalize3(unit);
    if(unit[2]>0){bend[0]=0;bend[1]=-unit[2];bend[2]=unit[1];}
    else {bend[0]=-unit[2];bend[1]=0;bend[2]=unit[1];}
    normalize3(bend);
    float factor=elbow_control*fminf(1,fmaxf(0,unit[2]+.5f));
    if(elevation>0){float lower=(elevation*.75f)/constant(0x3fc90fda);factor=fminf(1,fmaxf(0,fmaxf(factor,lower)));}
    vector_blend(blend,bend,side_axis,factor,NULL);const float down[3]={0,-1,0};
    vector_blend(result,blend,down,dot3(unit,side_axis),side_axis);
    if(fabsf(dot3(result,unit))>constant(0x3f7d70a3))memcpy(result,unit,sizeof result);
    normalize3(result);matrix3_rotate(result,basis,result);
    for(int k=0;k<3;k++)if(!isfinite(result[k]))return 0;
    memcpy(out,result,sizeof result);return 1;
}
static void quaternion_y(float out[3],const float q[4])
{
    float x=q[0],y=q[1],z=q[2],w=q[3];
    float a=x*y-z*w,b=(.5f-z*z)-x*x,c=y*z+x*w;
    out[0]=a+a;out[1]=b+b;out[2]=c+c;
}
static void quaternion_z(float out[3],const float q[4])
{
    float x=q[0],y=q[1],z=q[2],w=q[3];
    float a=x*z+y*w,b=y*z-x*w,c=(.5f-x*x)-y*y;
    out[0]=a+a;out[1]=b+b;out[2]=c+c;
}
/* 0041C458: redirect a wrist while preserving its previous endpoint. */
static void wrist_redirect(gt4_driver_segment *hand,const gt4_driver_segment *forearm,
                           const float normal[3],const float secondary[3],float twist,float factor)
{
    float old_end[3],new_end[3],axis[3],desired[4],turn[4],up[3];
    segment_end(old_end,hand);quaternion_z(axis,forearm->rotation);for(int k=0;k<3;k++)axis[k]=-axis[k];
    if(secondary[0]==0 && secondary[1]==0 && secondary[2]==0)quaternion_align(turn,axis,normal);
    else {quaternion_y(up,forearm->rotation);quaternion_basis(turn,axis,up,normal,secondary);}
    quaternion_mul(desired,turn,forearm->rotation);quaternion_blend(hand->rotation,forearm->rotation,desired,factor);
    quaternion_z(axis,hand->rotation);for(int k=0;k<3;k++)axis[k]=-axis[k];
    quaternion_turn(hand->rotation,axis,twist);segment_end(new_end,hand);
    for(int k=0;k<3;k++)hand->position[k]+=old_end[k]-new_end[k];
}
int gt4_driver_solve_arms(gt4_driver_skeleton *s,const gt4_driver_arm_target *t,
                          gt4_driver_arm_controls *controls,int reachable[2])
{
    if(!s || !t || !controls || !reachable)return 0;
    const float *vectors[]={t->hand[0],t->hand[1],t->normal[0],t->normal[1],t->wrist_axis[0],t->wrist_axis[1]};
    for(int v=0;v<6;v++)for(int k=0;k<3;k++)if(!isfinite(vectors[v][k]))return 0;
    for(int k=0;k<4;k++)if(!isfinite(t->rotation[k]))return 0;
    const float *values[]={controls->elbow,controls->wrist_aim,controls->wrist_twist,
        controls->hand_twist,controls->clavicle_angle,controls->clavicle_roll};
    for(int v=0;v<6;v++)for(int k=0;k<2;k++)if(!isfinite(values[v][k]))return 0;
    gt4_driver_skeleton work=*s;gt4_driver_arm_controls in=*controls;
    gt4_driver_segment *scapula=&work.bone[GT4_SCAPULA];
    gt4_driver_segment *clavicle[]={&work.bone[GT4_LCLAVICLE],&work.bone[GT4_RCLAVICLE]};
    gt4_driver_segment *upper[]={&work.bone[GT4_LUPARM],&work.bone[GT4_RUPARM]};
    gt4_driver_segment *forearm[]={&work.bone[GT4_LFOREARM],&work.bone[GT4_RFOREARM]};
    gt4_driver_segment *hand[]={&work.bone[GT4_LHAND],&work.bone[GT4_RHAND]};
    for(int side=0;side<2;side++)if(clavicle[side]->length<=0 || upper[side]->length<=0 || forearm[side]->length<=0 || hand[side]->length<=0)return 0;
    float basis[9],root[2][3],guide[2][3],up[3],conjugate[4],twist[2];
    const float side_axis[2][3]={{1,0,0},{-1,0,0}},down[3]={0,-1,0},unit_up[3]={0,1,0};
    quaternion_matrix3(basis,scapula->rotation);
    for(int side=0;side<2;side++) {
        matrix3_rotate(clavicle[side]->position,basis,scapula->branch[side]);
        for(int k=0;k<3;k++)clavicle[side]->position[k]+=scapula->position[k];
        if(in.auto_clavicle) {
            float local[3],origin[3],delta[3];memcpy(local,scapula->branch[side],sizeof local);
            local[0]+=(side?-clavicle[side]->length:clavicle[side]->length);
            matrix3_rotate(origin,basis,local);
            for(int k=0;k<3;k++){origin[k]+=scapula->position[k];delta[k]=t->hand[side][k]-origin[k];}
            float length=(upper[side]->length+forearm[side]->length)+hand[side]->length;
            float fraction=fminf(1,fmaxf(0,dot3(delta,delta)/(length*length)));
            float boost=fraction<.5f?0:(fraction-.5f)+(fraction-.5f);
            float angle=((boost+(in.elbow[side]-.5f)*constant(0x3e999999))+
                         (in.wrist_twist[side]-.5f)*constant(0x3d4ccccc))*constant(0x3ea0d97a);
            in.clavicle_angle[side]=fminf(constant(0x3ea0d97a),fmaxf(0,angle));
        }
        float sn_y,cs_y,sn_z,cs_z,q[4];
        pose_sincos((side?in.clavicle_angle[side]:-in.clavicle_angle[side])*.5f,&sn_y,&cs_y);
        pose_sincos((side?-in.clavicle_roll[side]:in.clavicle_roll[side])*.5f,&sn_z,&cs_z);
        q[0]=sn_y*sn_z;q[1]=sn_y*cs_z;q[2]=cs_y*sn_z;q[3]=cs_y*cs_z;
        quaternion_mul(clavicle[side]->rotation,scapula->rotation,q);
        if(!in.normal_enabled[side]) {
            for(int k=0;k<3;k++)conjugate[k]=-scapula->rotation[k];conjugate[3]=scapula->rotation[3];
            float local_normal[3];quaternion_rotate(local_normal,conjugate,t->normal[side]);
            float angle=fminf(1,fmaxf(0,local_normal[1]))*constant(side?0xbea0d97a:0x3ea0d97a);
            quaternion_clavicle_turn(clavicle[side]->rotation,angle);
        }
        segment_end(root[side],clavicle[side]);
    }
    quaternion_rotate(up,t->rotation,unit_up);int status=0;
    gt4_driver_limb_target limb={0};memcpy(limb.axis[1],down,sizeof down);
    limb.min_bend=(5.f*constant(0x3c8efa35));
    for(int side=0;side<2;side++) {
        float elevation=constant(0x3fc90fda)-acos_clamped(dot3(up,t->normal[side]));
        if(!gt4_driver_elbow_guide(guide[side],clavicle[side],side_axis[side],root[side],t->hand[side],in.elbow[side],elevation))return 0;
        memcpy(limb.root,root[side],12);memcpy(limb.target,t->hand[side],12);memcpy(limb.guide,guide[side],12);memcpy(limb.axis[0],side_axis[side],12);
        limb.length[0]=upper[side]->length;limb.length[1]=forearm[side]->length+hand[side]->length;
        if(!gt4_driver_solve_limb(&limb,upper[side],forearm[side],&status))return 0;
        segment_end(hand[side]->position,forearm[side]);memcpy(hand[side]->rotation,forearm[side]->rotation,16);
    }
    if(in.simple_hand_twist) {
        for(int side=0;side<2;side++) {
            float axis[3],normal[3];segment_direction(axis,hand[side]);quaternion_z(normal,hand[side]->rotation);
            for(int k=0;k<3;k++)normal[k]=-normal[k];
            float angle=plane_angle(normal,t->normal[side],axis);
            quaternion_turn(hand[side]->rotation,axis,angle*constant(0x3f4ccccc));
            segment_direction(axis,forearm[side]);quaternion_turn(forearm[side]->rotation,axis,angle*constant(0x3e4ccccc));
        }
    }else {
        twist[0]=(in.wrist_twist[0]-.5f)*constant(0x3fc90fda);
        twist[1]=(.5f-in.wrist_twist[1])*constant(0x3fc90fda);
        for(int side=0;side<2;side++) {
            memcpy(limb.root,root[side],12);memcpy(limb.guide,guide[side],12);memcpy(limb.axis[0],side_axis[side],12);
            limb.length[0]=upper[side]->length;limb.length[1]=forearm[side]->length;limb.reach_margin=constant(0x3ca3d70a);
            if(in.wrist_aim[side]>0) {
                wrist_redirect(hand[side],forearm[side],t->normal[side],t->wrist_axis[side],-twist[side],in.wrist_aim[side]);
                memcpy(limb.target,hand[side]->position,12);
                if(!gt4_driver_solve_limb(&limb,upper[side],forearm[side],&status))return 0;
                segment_end(hand[side]->position,forearm[side]);
            }
        }
        if(in.wrist_mode!=1)for(int side=0;side<2;side++) {
            float from[3],to[3],axis[3],old_end[3],new_end[3],q[4];
            segment_direction(to,forearm[side]);segment_direction(from,hand[side]);segment_end(old_end,hand[side]);
            quaternion_align(q,from,to);quaternion_mul(hand[side]->rotation,q,hand[side]->rotation);
            quaternion_z(axis,hand[side]->rotation);quaternion_turn(hand[side]->rotation,axis,twist[side]);segment_end(new_end,hand[side]);
            for(int k=0;k<3;k++)limb.target[k]=hand[side]->position[k]+(old_end[k]-new_end[k]);
            memcpy(limb.root,root[side],12);memcpy(limb.guide,guide[side],12);memcpy(limb.axis[0],side_axis[side],12);
            limb.length[0]=upper[side]->length;limb.length[1]=forearm[side]->length;
            if(!gt4_driver_solve_limb(&limb,upper[side],forearm[side],&status))return 0;
            segment_end(hand[side]->position,forearm[side]);
        }
        for(int side=0;side<2;side++) {
            float axis[3],from[3],to[3];
            if(in.hand_twist[side]!=0){segment_direction(axis,hand[side]);quaternion_turn(hand[side]->rotation,axis,side?-in.hand_twist[side]:in.hand_twist[side]);}
            segment_direction(axis,forearm[side]);quaternion_y(from,forearm[side]->rotation);quaternion_y(to,hand[side]->rotation);
            float angle=plane_angle(from,to,axis);
            if(in.normal_enabled[side]) {
                if(!side && angle>constant(0x3fc90fda))angle-=constant(0x40c90fda);
                if(side && angle<constant(0xbfc90fda))angle+=constant(0x40c90fda);
            }
            quaternion_turn(forearm[side]->rotation,axis,angle*constant(side?0x3f2a7ef9:0x3f333333));
            segment_direction(axis,upper[side]);quaternion_turn(upper[side]->rotation,axis,angle*constant(side?0x3eaa7ef9:0x3e800000));
        }
    }
    float matrices[GT4_DRIVER_BONES][16];if(!gt4_driver_skeleton_matrices(&work,matrices))return 0;
    for(int k=0;k<2;k++)if(!isfinite(in.clavicle_angle[k]))return 0;
    *s=work;*controls=in;if(!in.simple_hand_twist)reachable[0]=reachable[1]=status;return 1;
}
/* 004879A0 and 00487AA8 have explicit products, not generic multiplication. */
static void quaternion_pre_x(float q[4],float angle)
{
    float sn,cs;pose_sincos(angle*.5f,&sn,&cs);
    float r[4]={sn*q[3]+cs*q[0],cs*q[1]-sn*q[2],sn*q[1]+cs*q[2],cs*q[3]-sn*q[0]};memcpy(q,r,sizeof r);
}
static void quaternion_pre_y(float q[4],float angle)
{
    float sn,cs;pose_sincos(angle*.5f,&sn,&cs);
    float r[4]={cs*q[0]+sn*q[2],sn*q[3]+cs*q[1],(-sn)*q[0]+cs*q[2],cs*q[3]-sn*q[1]};memcpy(q,r,sizeof r);
}
static void head_euler(float q[4],const float angles[3],int roll)
{
    float sn,cs;
    if(roll){pose_sincos(angles[2]*.5f,&sn,&cs);q[0]=q[1]=0;q[2]=sn;q[3]=cs;quaternion_pre_y(q,angles[1]);}
    else {pose_sincos(angles[1]*.5f,&sn,&cs);q[0]=q[2]=0;q[1]=sn;q[3]=cs;}
    quaternion_pre_x(q,-angles[0]);
}
/* 00422380, norm-aware local X turn. */
static void quaternion_local_x(float q[4],float angle)
{
    if(angle==0)return;float sn,cs;pose_sincos(angle*.5f,&sn,&cs);
    float norm=(((q[0]*q[0]+q[1]*q[1])+q[2]*q[2])+q[3]*q[3]);norm=(norm+norm)-1;
    float delta[4]={q[3]*sn,q[2]*norm*sn,(-q[1])*norm*sn,(-q[0])*sn};
    for(int k=0;k<4;k++)q[k]=q[k]*cs+delta[k];
}
/* 0041FC00: head-forward cone plus separately clamped yaw/pitch. */
static int head_look_angles(float angles[3],float out[3],const float direction[3],float yaw_range)
{
    const float forward[3]={0,0,1};float limit=constant(0x3fb4f4aa);int reachable=1;
    memcpy(out,direction,12);
    if(direction[2]<pose_cos(limit)) {
        float axis[3],q[4];cross3(axis,direction,forward);
        if(dot3(axis,axis)!=0){normalize3(axis);quaternion_axis(q,axis,acos_clamped(direction[2])-limit);quaternion_rotate(out,q,direction);}
        else {out[0]=0;out[1]=pose_sin(limit);out[2]=0;}
        reachable=0;
    }
    float yaw=out[0]*out[0]+out[2]*out[2],yaw_limit=limit*(yaw==0?1.f:yaw_range);
    yaw=yaw==0?0:pose_atan2(out[0],out[2]);yaw=wrap_angle(yaw,constant(0xc0490fda));
    angles[1]=fminf(yaw_limit,fmaxf(-yaw_limit,yaw));
    float pitch=pose_asin(fminf(1,fmaxf(-1,out[1])));pitch=wrap_angle(pitch,constant(0xc0490fda));
    angles[0]=fminf(limit,fmaxf(-limit,pitch));return reachable;
}
/* 00423988's signed unit-vector angle without an additional projection. */
static float signed_angle(const float from[3],const float to[3],const float axis[3])
{float cross[3];float angle=acos_clamped(dot3(from,to));cross3(cross,axis,from);return dot3(to,cross)<0?-angle:angle;}
/* 0041FEA0. Roll fades when the supplied up points along the head direction. */
static void head_roll(float angles[3],const float up[3],const float direction[3],float gain,float bias)
{
    float factor=dot3(up,direction)*constant(0x411ffffd)+constant(0x4117fffd);
    factor=fminf(1,fmaxf(0,factor));float q[4],reference[3],projected[3];head_euler(q,angles,0);quaternion_y(reference,q);
    float along=dot3(up,direction);for(int k=0;k<3;k++)projected[k]=up[k]-direction[k]*along;
    float angle=0;
    if(dot3(projected,projected)!=0) {
        normalize3(projected);angle=signed_angle(reference,projected,direction);
        if(angle<constant(0xc016cbe3)) {
            float f=angle*constant(0x4007cfee)+constant(0x40bfffff);f=fminf(1,fmaxf(0,f));angle=f*constant(0xbef14637);
        }else if(angle>constant(0x4016cbe3)) {
            float f=angle*constant(0xc007cfee)+constant(0x40bfffff);f=fminf(1,fmaxf(0,f));angle=f*constant(0x3ef14637);
        }else angle=fminf(constant(0x3ef14637),fmaxf(constant(0xbef14637),angle));
    }
    angles[2]=angle*(factor*gain)+bias;
}
int gt4_driver_solve_head(gt4_driver_skeleton *s,gt4_driver_head_controls *controls,
                          const float look[3],int *reachable)
{
    if(!s || !controls || !reachable)return 0;
    const float values[]={controls->blend,controls->base_pitch,controls->roll_gain,controls->roll_bias,controls->yaw_range};
    for(int k=0;k<5;k++)if(!isfinite(values[k]) || fabsf(values[k])>1000)return 0;
    for(int k=0;k<3;k++)if(!isfinite(controls->up[k]) || !isfinite(controls->angles[k]) || fabsf(controls->angles[k])>1000 || (look && !isfinite(look[k])))return 0;
    gt4_driver_skeleton work=*s;gt4_driver_head_controls in=*controls;
    gt4_driver_segment *scapula=&work.bone[GT4_SCAPULA],*neck=&work.bone[GT4_NECK],*head=&work.bone[GT4_CRANIUM];
    float base[4],direction[3],axis[3],delta[3],local[3],clamped[3],up[3],q[4],conjugate[4];int status=*reachable;
    segment_end(neck->position,scapula);memcpy(neck->rotation,scapula->rotation,16);segment_end(head->position,neck);
    if(look) {
        for(int k=0;k<3;k++)delta[k]=look[k]-head->position[k];
        if(dot3(delta,delta)!=0){normalize3(delta);segment_direction(direction,neck);in.blend=fminf(1,fmaxf(0,dot3(delta,direction)*.5f+.5f));}
    }
    memcpy(base,neck->rotation,16);const float x_axis[3]={1,0,0};quaternion_rotate(axis,scapula->rotation,x_axis);
    for(int k=0;k<3;k++)axis[k]=-axis[k];float angle=((in.blend-.5f)*.25f)*constant(0x40490fda);
    quaternion_turn(base,axis,angle);memcpy(neck->rotation,base,16);segment_end(head->position,neck);
    if(look) {
        for(int k=0;k<3;k++)delta[k]=look[k]-head->position[k];float length2=dot3(delta,delta);
        if(length2!=0) {
            for(int k=0;k<3;k++)conjugate[k]=-neck->rotation[k];conjugate[3]=neck->rotation[3];
            quaternion_rotate(local,conjugate,delta);float inverse=1.f/sqrtf(length2);for(int k=0;k<3;k++)local[k]*=inverse;
            status=head_look_angles(in.angles,clamped,local,in.yaw_range);quaternion_rotate(up,conjugate,in.up);
            head_roll(in.angles,up,clamped,in.roll_gain,in.roll_bias);
        }
    }else {
        memcpy(q,neck->rotation,16);quaternion_local_x(q,-in.base_pitch);quaternion_z(direction,q);
        for(int k=0;k<3;k++)conjugate[k]=-neck->rotation[k];conjugate[3]=neck->rotation[3];
        float basis[9];quaternion_matrix3(basis,conjugate);matrix3_rotate(up,basis,in.up);
        head_roll(in.angles,up,direction,in.roll_gain,in.roll_bias);
    }
    float pitch=wrap_angle(in.angles[0]-in.base_pitch,constant(0xc0490fda));
    in.angles[0]=fminf(constant(0x3fb4f4aa),fmaxf(constant(0xbfb4f4aa),pitch));
    head_euler(q,in.angles,1);quaternion_mul(head->rotation,neck->rotation,q);
    if(!in.neck_yaw_locked){angle=wrap_angle(in.angles[1],constant(0xbfc90fda));segment_direction(axis,neck);quaternion_turn(base,axis,angle);memcpy(neck->rotation,base,16);}
    float matrices[GT4_DRIVER_BONES][16];if(!gt4_driver_skeleton_matrices(&work,matrices))return 0;
    if(!isfinite(in.blend))return 0;for(int k=0;k<3;k++)if(!isfinite(in.angles[k]))return 0;
    *s=work;*controls=in;*reachable=status;return 1;
}
void gt4_driver_posture_init(gt4_driver_posture *posture,gt4_driver_pose_targets *targets)
{
    if(posture) {
        memset(posture,0,sizeof *posture);posture->enabled=15;posture->torso.look_gain=1;
        posture->arms.auto_clavicle=1;posture->head.blend=.5f;posture->head.roll_gain=1;
        posture->head.yaw_range=1;posture->head.up[1]=1;
        for(int side=0;side<2;side++) {
            posture->arms.elbow[side]=posture->arms.wrist_twist[side]=.5f;posture->arms.normal_enabled[side]=1;
            posture->legs.spread[side]=posture->legs.foot_twist[side]=.5f;
        }
    }
    if(targets){memset(targets,0,sizeof *targets);targets->torso.rotation[3]=targets->arms.rotation[3]=1;
        targets->torso.guide[2]=-1;targets->arms.normal[0][2]=1;targets->arms.normal[1][2]=-1;}
}
int gt4_driver_posture_solve(gt4_driver_skeleton *s,gt4_driver_posture *posture,
                             const gt4_driver_pose_targets *targets,const float look[3],int reachable[5])
{
    if(!s || !posture || !targets || !reachable || (posture->enabled&~15u))return 0;
    gt4_driver_skeleton work=*s;gt4_driver_posture state=*posture;int reach[5];memcpy(reach,reachable,sizeof reach);
    int rounding=fegetround();if(rounding<0)return 0;
    if(rounding!=FE_TOWARDZERO && fesetround(FE_TOWARDZERO))return 0;
    int ok=1;
    if(state.enabled&GT4_DRIVER_TORSO)ok=gt4_driver_solve_torso(&work,&targets->torso,&state.torso,look);
    if(ok && (state.enabled&GT4_DRIVER_ARMS))ok=gt4_driver_solve_arms(&work,&targets->arms,&state.arms,reach+3);
    if(ok && (state.enabled&GT4_DRIVER_LEGS))ok=gt4_driver_solve_legs(&work,targets->feet,&state.legs,reach+1);
    if(ok && (state.enabled&GT4_DRIVER_HEAD))ok=gt4_driver_solve_head(&work,&state.head,look,reach);
    if(rounding!=FE_TOWARDZERO && fesetround(rounding))return 0;
    if(!ok)return 0;*s=work;*posture=state;memcpy(reachable,reach,sizeof reach);return 1;
}
void gt4_driver_geometry_init(gt4_driver_geometry *g)
{
    if(!g)return;memset(g,0,sizeof *g);g->rotation[3]=1;
    for(int k=0;k<3;k++){g->normal[k][2]=1;g->wheel_basis[k*4]=1;}
    g->shift_angle=constant(0x40490fda);
}
int gt4_driver_geometry_wheel(gt4_driver_geometry *g,const float normal[3])
{
    if(!g || !normal)return 0;for(int k=0;k<3;k++)if(!isfinite(normal[k]))return 0;
    int rounding=fegetround();if(rounding<0)return 0;if(rounding!=FE_TOWARDZERO && fesetround(FE_TOWARDZERO))return 0;
    const float forward[3]={0,0,-1};float q[4],basis[9];quaternion_align(q,forward,normal);quaternion_matrix3(basis,q);
    int valid=1;for(int k=0;k<9;k++)if(!isfinite(basis[k]))valid=0;
    if(rounding!=FE_TOWARDZERO && fesetround(rounding))return 0;if(!valid)return 0;
    memcpy(g->normal[0],normal,12);memcpy(g->wheel_basis,basis,sizeof basis);return 1;
}
static void linear3(float out[3],const float a[3],const float b[3],float blend)
{float r[3],inverse=1-blend;for(int k=0;k<3;k++)r[k]=a[k]*inverse+b[k]*blend;memcpy(out,r,12);}
/* 00414E78: gear-position cycle; deliberately retains its extrapolation. */
static void gear_position(float out[3],const gt4_driver_geometry *g)
{
    float phase=g->gear_phase;int from=0,to=1;
    if(!(phase>=0 && phase<=1)){if(phase<=2){from=1;to=2;phase-=1;}else{from=2;to=0;phase-=2;}}
    linear3(out,g->gear[from],g->gear[to],phase);
}
/* 00414FE8 / 00415E90: blend the two wheel positions, not their angles. */
static void wheel_point(float out[3],const gt4_driver_geometry *g,float angle,float radius,float depth)
{
    float sn,cs;pose_sincos(angle,&sn,&cs);float local[3]={(-radius)*sn,radius*cs,-depth};
    matrix3_rotate(out,g->wheel_basis,local);for(int k=0;k<3;k++)out[k]+=g->wheel[k];
}
static void wheel_position(float out[3],const gt4_driver_grip *grip,const gt4_driver_geometry *g,const float angles[2],float blend)
{
    float a[3],b[3],radius=g->wheel_radius+grip->radius_offset;
    wheel_point(a,g,angles[0],radius,grip->depth_offset);wheel_point(b,g,angles[1],radius,grip->depth_offset);
    linear3(out,a,b,blend);
}
/* 004150A0 / 00415148: normal and secondary wrist direction. */
static void wheel_normal(float out[3],const gt4_driver_geometry *g,unsigned side,float sn,float cs)
{
    float local[3]={side?-cs:cs,side?-sn:sn,0},v[3];
    matrix3_rotate(v,g->wheel_basis,local);quaternion_rotate(out,g->rotation,v);
}
static void wheel_axis(float out[3],const gt4_driver_geometry *g,float sn,float cs,float twist)
{
    float local[3]={-sn,cs,0},v[3];
    if(twist!=0){float axis[3]={-cs,sn,0},q[4];quaternion_axis(q,axis,twist);quaternion_rotate(local,q,local);}
    matrix3_rotate(v,g->wheel_basis,local);quaternion_rotate(out,g->rotation,v);
}
static void wheel_orientation(gt4_driver_grip *grip,const gt4_driver_geometry *g,unsigned side,float angle)
{
    if(grip->normal_mode==1){wheel_axis(grip->normal,g,side?1.f:-1.f,0,0);grip->axis[0]=grip->axis[2]=0;grip->axis[1]=1;return;}
    if(grip->normal_mode==2){grip->normal[0]=grip->normal[2]=0;grip->normal[1]=-1;wheel_axis(grip->axis,g,side?1.f:-1.f,0,0);return;}
    if((side==0 && angle>constant(0xbf490fda)) || (side==1 && angle<constant(0x3f490fda))) {
        float delta=angle*constant(0x3de38e36)+(side?constant(0xbdb2b8bf):constant(0x3db2b8bf));
        delta=fminf(side?0.f:constant(0x3db2b8c1),fmaxf(side?constant(0xbdb2b8c1):0.f,delta));angle+=delta;
    }
    float sn,cs;pose_sincos(angle,&sn,&cs);wheel_normal(grip->normal,g,side,sn,cs);wheel_axis(grip->axis,g,sn,cs,0);
}
static int wheel_grip(gt4_driver_grip *grip,const gt4_driver_geometry *g,unsigned side,const float angles[2],float blend)
{
    float angle=angles[0]*(1-blend)+angles[1]*blend;
    wheel_position(grip->position,grip,g,angles,blend);wheel_orientation(grip,g,side,angle);
    grip->twist=.5f;grip->aim=1;
    return side?angle>constant(0x3fc90fda):angle<constant(0xbfc90fda);
}
/* 00415A68's sine-shaped offset, parameterized by the original phase. */
static float grip_bump(float phase,float peak,float amplitude)
{
    if(phase<=0 || phase>=1)return 0;
    float x=phase<peak?phase/peak:(1-phase)/(1-peak);
    return amplitude*pose_sin(x*constant(0x3fc90fda));
}
/* 00415828 / 00415978 / 00415AF8: clear the wheel rim on the way to
   the shifter. Includes the original angular ramp and plane projection. */
static int shift_clearance(float out[3],const gt4_driver_geometry *g,float angle,float phase)
{
    float point[3],direction[3];gear_position(point,g);
    for(int k=0;k<3;k++)direction[k]=point[k]-g->wheel[k];
    float l2=dot3(direction,direction);if(l2<constant(0x38d1b717))return 0;
    float length=sqrtf(l2),inverse=1.f/length;
    for(int k=0;k<3;k++)direction[k]*=inverse;
    const float up[3]={0,1,0};float projected=plane_angle(direction,up,g->normal[0]);
    float x=length*pose_sin(projected),y=length*pose_cos(projected),radius=g->wheel_radius;
    float norm2=x*x+y*y;if(norm2<=radius*radius)return 0;
    float norm=sqrtf(norm2),opening=constant(0x3fc90fda)-pose_asin(radius/norm);
    float centre=constant(0x3fc90fda)-pose_asin(y/norm);if(x<0)centre=-centre;
    float delta=fabsf(wrap_angle(angle-centre,constant(0xc0490fda)));
    float low=opening*constant(0x3f4ccccc),high=opening*constant(0x3f999999);
    inverse=1.f/(high-low);float bias=(high*0-low)*inverse;
    float factor=fminf(1,fmaxf(0,inverse*delta+bias))*grip_bump(phase,.5f,constant(0x3dcccccc));
    for(int k=0;k<3;k++)out[k]=g->normal[0][k]*factor;return 1;
}
/* 00416390: original shift travel, wrist rotation and normal-enable flag. */
static int shift_grip(gt4_driver_grip *grip,const gt4_driver_geometry *g,unsigned side,const float angles[2],float blend,float phase)
{
    float target[3],wheel[3],correction[3];gear_position(target,g);
    for(int k=0;k<3;k++)target[k]+=grip->shift_offset[k];
    if(phase>=0 && phase<=.25f) {
        float factor=(phase-0)*4;wheel_position(wheel,grip,g,angles,blend);linear3(grip->position,wheel,target,factor);
        if(shift_clearance(correction,g,angles[1],1-factor))for(int k=0;k<3;k++)grip->position[k]+=correction[k];
        grip->twist=.5f;
    }else if(phase>=.25f && phase<=.75f){memcpy(grip->position,target,12);grip->twist=.5f;}
    else if(phase>=.75f && phase<=1) {
        float factor=(phase-.75f)*4;wheel_position(wheel,grip,g,angles,blend);linear3(grip->position,target,wheel,factor);
        if(shift_clearance(correction,g,angles[1],factor))for(int k=0;k<3;k++)grip->position[k]+=correction[k];
        grip->twist=.5f;
    }
    float angle=0,p2=constant(0x3e4ccccc),p8=constant(0x3f4ccccc);
    if(phase>=0 && phase<=p2) {
        angle=(angles[0]*(1-blend)+angles[1]*blend)*(1-(phase-0)/p2);
        wheel_orientation(grip,g,side,angle);grip->aim=1;grip->extra_twist=0;
    }else if(phase>=p2 && phase<=p8) {
        float factor=(phase-p2)/constant(0x3f199999),axis[3];const float up[3]={0,1,0};
        if(factor<=p2) {
            float t=factor*5;wheel_axis(axis,g,0,1,0);vector_blend(grip->axis,axis,up,t,NULL);
            grip->aim=1-t;grip->extra_twist=t*grip->shift_twist;
        }else if(factor<=p8){memcpy(grip->axis,up,12);grip->aim=0;grip->extra_twist=grip->shift_twist;}
        else {
            float t=(factor-p8)*5;wheel_axis(axis,g,0,1,0);vector_blend(grip->axis,up,axis,t,NULL);
            grip->aim=t;grip->extra_twist=(1-t)*grip->shift_twist;
        }
        wheel_normal(grip->normal,g,side,0,1);
    }else if(phase>=p8 && phase<=1) {
        angle=(angles[0]*(1-blend)+angles[1]*blend)*((phase-p8)/constant(0x3e4cccd0));
        wheel_orientation(grip,g,side,angle);grip->aim=1;grip->extra_twist=0;
    }
    return side?angle>constant(0x3fc90fda):angle<constant(0xbfc90fda);
}
static int grip_evaluate(gt4_driver_grip *grip,const gt4_driver_geometry *g,unsigned kind,unsigned side,const float angles[2],float blend,float phase)
{
    if(kind==GT4_GRIP_WHEEL) {
        phase=fabsf(phase);float factor=phase<=.5f?phase*2:(1-phase)*2;factor=fminf(1,fmaxf(0,factor));
        float extra=grip->phase_twist*factor;
        if(phase>0 && phase<=1 && ((g->shift_side==1 && side==0) || (g->shift_side==2 && side==1))) {
            int flag=shift_grip(grip,g,side,angles,blend,phase);grip->twist=.5f-extra/constant(0x40490fda);return flag;
        }
        return wheel_grip(grip,g,side,angles,blend);
    }
    if(kind==GT4_GRIP_SIMPLE) {
        memcpy(grip->position,g->wheel,12);grip->position[0]+=side?-g->wheel_radius:g->wheel_radius;return 1;
    }
    float axis[3];
    if(kind==GT4_GRIP_REST) {
        memcpy(grip->position,g->rest[side],12);quaternion_rotate(grip->normal,g->rotation,g->normal[side+1]);
        axis[0]=side?constant(0xbf3504f3):constant(0x3f3504f3);axis[1]=0;axis[2]=constant(0x3f3504f3);
    }else {
        grip->position[0]=g->seat[0]+(side?-constant(0x3d4ccccc):constant(0x3d4ccccc));
        grip->position[1]=g->seat[1]+constant(0x3e4ccccc);grip->position[2]=g->seat[2]+constant(0x3e999999);
        const float down[3]={0,-1,0};quaternion_rotate(grip->normal,g->rotation,down);
        axis[0]=side?constant(0xbf5db3d7):constant(0x3f5db3d7);axis[1]=0;axis[2]=.5f;
    }
    quaternion_rotate(grip->axis,g->rotation,axis);grip->twist=.5f;grip->aim=1;return 0;
}
int gt4_driver_grip_solve(gt4_driver_grip *grip,const gt4_driver_geometry *g,
                         unsigned kind,unsigned side,const float angles[2],float blend,float phase,int *normal_enabled)
{
    if(!grip || !g || !angles || !normal_enabled || side>1 || kind>GT4_GRIP_FREE)return 0;
    if(!isfinite(angles[0]) || !isfinite(angles[1]) || !isfinite(blend) || !isfinite(phase))return 0;
    gt4_driver_grip work=*grip;int rounding=fegetround();if(rounding<0)return 0;
    if(rounding!=FE_TOWARDZERO && fesetround(FE_TOWARDZERO))return 0;
    int flag=grip_evaluate(&work,g,kind,side,angles,blend,phase),valid=1;
    const float *groups[]={work.position,work.normal,work.axis,&work.twist};const int counts[]={3,3,3,3};
    for(int field=0;field<4;field++)for(int k=0;k<counts[field];k++)if(!isfinite(groups[field][k]))valid=0;
    if(rounding!=FE_TOWARDZERO && fesetround(rounding))return 0;
    if(!valid)return 0;*grip=work;*normal_enabled=flag;return 1;
}
void gt4_driver_targets_init(gt4_driver_target_controls *c)
{
    if(!c)return;memset(c,0,sizeof *c);
    for(int k=0;k<2;k++){c->angles[0][k]=constant(0xbfc90fda);c->angles[1][k]=constant(0x3fc90fda);}
}
static float triangle_angle(float a,float b,float c)
{
    if(a+b<c || a*b==0)return 0;
    float cosine=((a*a+b*b)-c*c)/((a+a)*b);
    return constant(0x3fc90fda)-pose_asin(cosine);
}
static float spine_pitch(const gt4_driver_skeleton *s,float length,float pitch)
{
    float lengths[3]={s->bone[GT4_LOWER_SPINE].length,s->bone[GT4_MIDDLE_SPINE].length,s->bone[GT4_SCAPULA].length};
    /* 00417110 chooses the same quadratic's valid, smaller cosine root
       as 00420190, retaining pitch when neither root is valid. */
    float a=lengths[0],b=lengths[1],c=lengths[2],factor=(a+c)*-2;
    float disc=(b*b*factor)*factor,term=((a*a+b*b)+c*c)-length*length;term-=(a+a)*c;
    disc-=((a*16)*c)*term;if(disc<0)return pitch;
    float radical=sqrtf(disc),centre=(-b)*factor,inv=1.f/((a*8)*c);
    float x=(centre-radical)*inv,y=(centre+radical)*inv;int xv=x>=-1 && x<=1,yv=y>=-1 && y<=1;
    if(!xv && !yv)return pitch;float cosine=xv?(yv?fminf(x,y):x):y;
    float angle=fabsf(constant(0x3fc90fda)-pose_asin(cosine));
    if(angle>constant(0x3fc90fda)) {
        float extra=(b*.5f)/pose_cos(constant(0x40490fda)-angle);
        float turn=triangle_angle(c+extra,length,a+extra);if(turn>0)pitch-=turn;
    }
    return pitch;
}
static float cycle_phase(const gt4_driver_target_controls *c)
{
    float phase=c->phase;int from=0,to=1;
    if(!(phase>=0 && phase<=1)){if(phase<=2){from=1;to=2;phase-=1;}else if(phase<=3){from=2;to=0;phase-=2;}else return 0;}
    return c->phase_keys[from]*(1-phase)+c->phase_keys[to]*phase;
}
static float reach_fraction(float delta,float negative_length)
{
    float inverse=1.f/negative_length,bias=inverse*negative_length;
    return fminf(1,fmaxf(0,inverse*delta+bias));
}
static void geometry_point(float out[3],const gt4_driver_geometry *g,const float local[3])
{
    float basis[9];quaternion_matrix3(basis,g->rotation);matrix3_rotate(out,basis,local);
    for(int k=0;k<3;k++)out[k]+=g->position[k];
}
static int targets_build(const gt4_driver_skeleton *s,gt4_driver_posture *p,
                         const gt4_driver_geometry *g,const gt4_driver_target_controls *c,gt4_driver_pose_targets *out)
{
    memcpy(out->torso.rotation,g->rotation,16);memcpy(out->arms.rotation,g->rotation,16);
    unsigned kind=GT4_GRIP_WHEEL;gt4_driver_grip hand[2]={0};
    if(!(p->enabled&GT4_DRIVER_ARMS)){kind=GT4_GRIP_SIMPLE;p->arms.wrist_mode=0;}
    else if(g->hand_mode==1){kind=GT4_GRIP_REST;p->arms.wrist_mode=1;}
    else if(c->free_hands)kind=GT4_GRIP_FREE;
    else {
        float phase=c->phase,offset[3]={0},extra[2]={0},amplitude[2]={0};int from=0,to=1,valid=1;
        if(!(phase>=0 && phase<=1)){if(phase<=2){from=1;to=2;phase-=1;}else if(phase<=3){from=2;to=0;phase-=2;}else valid=0;}
        if(valid) {
            linear3(offset,c->shift_offset[from],c->shift_offset[to],phase);
            for(int side=0;side<2;side++) {
                extra[side]=c->phase_twist[from][side]*(1-phase)+c->phase_twist[to][side]*phase-c->hand_twist[side];
                amplitude[side]=c->phase_amplitude[from][side]*(1-phase)+c->phase_amplitude[to][side]*phase;
            }
        }
        for(int side=0;side<2;side++) {
            if(g->shift_side==side+1){memcpy(hand[side].shift_offset,offset,12);hand[side].radius_offset=c->radius_offset[side];
                hand[side].depth_offset=c->depth_offset[side];hand[side].phase_twist=amplitude[0];hand[side].phase_unused=amplitude[1];}
            hand[side].shift_twist=extra[side];hand[side].normal_mode=c->normal_mode;
        }
        p->arms.wrist_mode=0;
    }
    float phase=cycle_phase(c);
    for(int side=0;side<2;side++) {
        int enabled=grip_evaluate(&hand[side],g,kind,side,c->angles[side],c->blend[side],phase);
        if(c->normal_override[side]==1)enabled=1;else if(c->normal_override[side]==2)enabled=0;
        p->arms.normal_enabled[side]=enabled;
    }
    float twist=c->twist_channel;
    if(!c->direct_twist) {
        float bound=constant(0x3f490fda);twist=((twist-(-bound))/(bound-(-bound))-.5f)*constant(0x3e199999);
        twist*=constant(0x40490fda);
    }
    float height=s->bone[GT4_PELVIS].length+s->bone[GT4_LOWER_SPINE].length;
    height+=s->bone[GT4_MIDDLE_SPINE].length;height+=s->bone[GT4_SCAPULA].length;height+=c->length_offset;
    height*=1-pose_cos(c->pitch)*constant(0x3d4ccccc);
    float pitch=c->pitch_locked?c->pitch:spine_pitch(s,height-s->bone[GT4_PELVIS].length,c->pitch);
    float sn,cs;pose_sincos(pitch,&sn,&cs);float end[3]={0,height*cs,height*sn},guide[3]={0,sn,-cs};
    float axis[3]={0,-sn,cs},q[4];quaternion_axis(q,axis,twist);quaternion_rotate(end,q,end);
    for(int k=0;k<3;k++)end[k]=(end[k]+g->seat[k])+c->spine_offset[k];
    if(p->enabled&GT4_DRIVER_ARMS) {
        float left=constant(0x3ea0d97a),right=left,turn=0;
        float amount=reach_fraction(hand[0].position[0]-end[0],-s->bone[GT4_LCLAVICLE].length)*constant(0x3e714637);
        turn=amount*constant(0x3d4ccccc);left+=amount;right-=amount*.25f;
        float length=(s->bone[GT4_LUPARM].length+s->bone[GT4_LFOREARM].length)+s->bone[GT4_LHAND].length;
        amount=reach_fraction(hand[0].position[2]-end[2],-length)*constant(0xbef14637);
        left+=amount;right-=amount*.25f;
        amount=reach_fraction(end[0]-hand[1].position[0],-s->bone[GT4_RCLAVICLE].length)*constant(0x3e714637);
        turn+=amount*constant(0x3d4ccccc);right+=amount;left-=amount*.25f;
        length=(s->bone[GT4_RUPARM].length+s->bone[GT4_RFOREARM].length)+s->bone[GT4_RHAND].length;
        amount=reach_fraction(hand[1].position[2]-end[2],-length)*constant(0xbef14637);
        right+=amount;left-=amount*.25f;
        p->arms.clavicle_angle[0]=left*.75f+c->clavicle_angle[0];p->arms.clavicle_angle[1]=right*.75f+c->clavicle_angle[1];
        const float x_axis[3]={1,0,0};quaternion_axis(q,x_axis,turn);
        for(int k=0;k<3;k++)end[k]-=g->seat[k];quaternion_rotate(end,q,end);for(int k=0;k<3;k++)end[k]+=g->seat[k];
    }
    float roll=c->pitch_locked?constant(0x3ea0d97a):0;
    for(int side=0;side<2;side++)p->arms.clavicle_roll[side]=c->clavicle_roll[side]+roll;
    p->arms.auto_clavicle=0;
    if(p->enabled&GT4_DRIVER_ARMS) {
        float body[3],wheel[3],delta[3];for(int k=0;k<3;k++)delta[k]=end[k]-g->wheel[k];float span=sqrtf(dot3(delta,delta));
        for(int side=0;side<2;side++) {
            for(int k=0;k<3;k++){body[k]=hand[side].position[k]-end[k];wheel[k]=g->wheel[k]-hand[side].position[k];}
            float angle=constant(0x3fc90fda)-triangle_angle(sqrtf(dot3(body,body)),sqrtf(dot3(wheel,wheel)),span);
            angle=fminf(constant(0x3fa0d97a),fmaxf(constant(0xbfa0d97a),angle));
            quaternion_axis(q,hand[side].normal,side?angle:-angle);quaternion_rotate(out->arms.wrist_axis[side],q,hand[side].axis);
            memcpy(out->arms.normal[side],hand[side].normal,12);
            p->arms.wrist_twist[side]=fminf(1,fmaxf(0,hand[side].twist));p->arms.wrist_aim[side]=fminf(1,fmaxf(0,hand[side].aim));
            p->arms.hand_twist[side]=hand[side].extra_twist+c->hand_twist[side];
        }
    }
    float root[3];for(int k=0;k<3;k++)root[k]=g->seat[k]+c->root_offset[k];geometry_point(out->torso.root,g,root);
    for(int side=0;side<2;side++)geometry_point(out->arms.hand[side],g,hand[side].position);
    geometry_point(out->torso.end,g,end);for(int k=0;k<3;k++)out->torso.end[k]+=c->spine_world_offset[k];
    if(p->enabled&GT4_DRIVER_LEGS)for(int side=0;side<2;side++) {
        float foot[3];for(int k=0;k<3;k++)foot[k]=g->feet[side][k]+c->feet_offset[side][k];geometry_point(out->feet[side],g,foot);
    }
    quaternion_rotate(out->torso.guide,g->rotation,guide);return 1;
}
int gt4_driver_targets_build(const gt4_driver_skeleton *s,gt4_driver_posture *posture,
                            const gt4_driver_geometry *g,const gt4_driver_target_controls *c,gt4_driver_pose_targets *targets)
{
    if(!s || !posture || !g || !c || !targets)return 0;
    gt4_driver_posture p=*posture;gt4_driver_pose_targets out=*targets;
    int rounding=fegetround();if(rounding<0)return 0;if(rounding!=FE_TOWARDZERO && fesetround(FE_TOWARDZERO))return 0;
    int ok=targets_build(s,&p,g,c,&out);
    const float *groups[]={out.torso.rotation,out.torso.root,out.torso.end,out.torso.guide,out.arms.hand[0],out.arms.hand[1],out.feet[0],out.feet[1]};
    for(int f=0;f<8;f++)for(int k=0;k<(f==0?4:3);k++)if(!isfinite(groups[f][k]))ok=0;
    const float *arm_values[]={p.arms.wrist_aim,p.arms.wrist_twist,p.arms.hand_twist,p.arms.clavicle_angle,p.arms.clavicle_roll};
    for(int f=0;f<5;f++)for(int side=0;side<2;side++)if(!isfinite(arm_values[f][side]))ok=0;
    if(rounding!=FE_TOWARDZERO && fesetround(rounding))return 0;if(!ok)return 0;*posture=p;*targets=out;return 1;
}

unsigned gt4_driver_control_type(const gt4_driver_car *c)
{
    /* 00402270, ordinary provider 0038DB28; Nike has a separate provider. */
    if(!c)return 0;if(c->type==15)return 2;
    return c->shift_type>=3 && c->shift_type<=5?(unsigned)c->shift_type:0;
}
int gt4_driver_record_init(const gt4_driver_car *c,float ch[64])
{
    if(!c || !ch || !c->geometry_offset || c->type==10 || c->type==15)return 0;
    /* 00402320. The ordinary initial frame has no generated motion yet. */
    memset(ch,0,64*sizeof *ch);unsigned type=gt4_driver_control_type(c);
    float left=constant(0xbf8cbe4b),right=constant(0x3f8cbe4b),twist=constant(0xbf5f66f1);
    if(c->shift_type==1){left=constant(0xbfc90fda);right=-left;twist=0;}
    else if(type==3){left=constant(0xc016cbe3);right=-left;twist=constant(0xbf8cbe4b);}
    else if(type==4){left=constant(0xc0490fda);right=-left;twist=constant(0xbfc90fda);}
    else if(type==5) {
        left=constant(0xbf8cbe4b);twist=0;
        float axis[3]={0,c->wheel_normal[2],-c->wheel_normal[1]};
        if(c->wheel_normal[1]==0){axis[1]=1;axis[2]=0;}else {float n=sqrtf(dot3(axis,axis));for(int k=0;k<3;k++)axis[k]/=n;}
        right=dot3(c->shift_normal,axis);
        float span=-dot3(c->handbrake,c->shift_normal)+dot3(c->wheel,c->shift_normal)-c->handbrake_length;
        ch[0x74/4]=c->shift_length-c->handbrake_length;ch[0x7c/4]=span-constant(0x3d23d70a);
    }
    ch[0x38/4]=ch[0x3c/4]=constant(0x3e999999);
    ch[0x48/4]=ch[0x40/4]=left;ch[0x4c/4]=ch[0x44/4]=right;
    ch[0x50/4]=ch[0x54/4]=1;ch[0x64/4]=.5f;ch[0x6c/4]=type==1?constant(0x3fc90fda):0;
    /* 0040DC08 at zero yaw/pitch: seat minus 6.25 m along car z. */
    memcpy(ch+0x58/4,c->seat,12);ch[0x60/4]+=constant(0xc0c80000);
    ch[0x80/4]=ch[0x84/4]=1;ch[0xbc/4]=type==1?.5f:1;ch[0xe4/4]=ch[0xe8/4]=constant(0x3dcccccc);
    ch[0xb4/4]=twist;
    if(type==3){ch[0xcc/4]=ch[0xdc/4]=constant(0xbfc90fda);ch[0x7c/4]=0;}
    return 1;
}
static void car_vector(float out[3],const float in[3])
{out[0]=-in[0];out[1]=in[1];out[2]=-in[2];}
static float unit(float x){return fminf(1,fmaxf(0,x));}
/* 004104B8 and jtbl_006A4630; 0044FAE0 returns 0044F5A0's rate 5.
   No extra timer: +80/+84 travel at the original fixed simulation step. */
static float grasp_step(unsigned hand,unsigned gear,float phase,float other,float dt,float value)
{
    float x=gear>=14?other:phase;
    if(x>0) {
        switch(gear) {
        case 0:case 1:case 2:case 14:case 15:case 27:case 28:x=0;break;
        case 3:case 4:case 16:case 17:case 29:case 30:
            if(value>.25f)value-=5*dt;return unit(value);
        case 5:case 6:case 7:case 18:case 19:case 20:case 31:case 32:case 33:
            if(value<1)value+=5*dt;return unit(value);
        case 8:case 9:case 21:case 22:case 34:case 35:
            if(x<constant(0x3f733333)){if(value>0)value-=5*dt;return unit(value);}x=0;break;
        default:return unit(value);
        }
    }
    if(x<=0) {
        if(hand==0){if(value<1)value+=5*dt;}
        else if(hand<9){if(value>0)value-=5*dt;}
    }
    return unit(value);
}
int gt4_driver_grasp_step(unsigned hand,unsigned gear,float phase,float other,float dt,float *grasp)
{
    if(!grasp || !isfinite(*grasp) || !isfinite(phase) || !isfinite(other) || !isfinite(dt) || dt<=0 || dt>.1f)return 0;
    int mode=fegetround();if(mode<0 || (mode!=FE_TOWARDZERO && fesetround(FE_TOWARDZERO)))return 0;
    float next=grasp_step(hand,gear,phase,other,dt,*grasp);
    if(mode!=FE_TOWARDZERO && fesetround(mode))return 0;*grasp=next;return 1;
}
static int race_pose(const gt4_driver_car *c,const float ch[64],gt4_driver_skeleton *s,float matrices[20][16])
{
    if(!c->geometry_offset || c->type==10 || c->type==15)return 0;
    for(int k=0;k<64;k++)if(!isfinite(ch[k]))return 0;
    unsigned type=gt4_driver_control_type(c);
    gt4_driver_posture p;gt4_driver_pose_targets targets;gt4_driver_posture_init(&p,&targets);
    /* 003B29C8: the reduced builtin head solves torso and head, without
       arm reach pulling the shoulders toward an invisible steering wheel. */
    if(gt4_driver_detail(c->type)==1){p.enabled=GT4_DRIVER_TORSO|GT4_DRIVER_HEAD;p.torso.twist_locked=1;}
    gt4_driver_target_controls t;gt4_driver_targets_init(&t);
    gt4_driver_geometry g;gt4_driver_geometry_init(&g);
    /* 00415350 / 003B2708: mirror CARINFO x/z, then apply half-turn Y.
       The pilot stays in car coordinates; the renderer supplies world pose. */
    g.rotation[1]=1;g.rotation[3]=0;
    for(int side=0;side<2;side++)car_vector(g.feet[side],c->feet[side]);
    car_vector(g.seat,c->seat);car_vector(g.wheel,c->wheel);
    car_vector(g.gear[0],c->shift);car_vector(g.gear[1],c->handbrake);car_vector(g.gear[2],c->handbrake);
    if(type==3){g.gear[2][0]=-constant(0x3f0353f7);g.gear[2][1]=constant(0x3f82d0e5);g.gear[2][2]=-constant(0xbdd70a3d);}
    float normal[3];car_vector(normal,c->wheel_normal);if(!gt4_driver_geometry_wheel(&g,normal))return 0;
    g.wheel_radius=c->wheel_radius;g.shift_side=c->wheel[0]<c->shift[0]?2:1;
    g.shift_length=c->chair_angle;g.shift_angle=constant(0x3fc90fda);g.gear_phase=ch[0xe0/4];
    float height=ch[0x10/4],root_height=0;
    if(height>constant(0x3c23d70a)){g.seat[1]+=height-constant(0x3c23d70a);root_height=constant(0x3c23d70a);}else root_height=height;
    t.spine_world_offset[1]=root_height;
    t.phase=ch[0xe0/4];t.angles[0][0]=ch[0x48/4];t.angles[1][0]=ch[0x4c/4];
    t.angles[0][1]=ch[0x40/4];t.angles[1][1]=ch[0x44/4];
    t.blend[0]=unit(ch[0x50/4]);t.blend[1]=unit(ch[0x54/4]);
    t.radius_offset[0]=ch[0x70/4];t.radius_offset[1]=ch[0x74/4];
    t.depth_offset[0]=ch[0x78/4];t.depth_offset[1]=ch[0x7c/4];
    if(type==5)for(int k=0;k<2;k++){t.radius_offset[k]*=ch[0xd0/4];t.depth_offset[k]*=ch[0xd0/4];}
    t.hand_twist[0]=ch[0xac/4];t.hand_twist[1]=ch[0xb0/4];
    t.phase_twist[0][g.shift_side-1]=ch[0xb4/4];
    t.phase_twist[1][g.shift_side-1]=ch[0xcc/4];t.phase_twist[2][g.shift_side-1]=ch[0xdc/4];
    p.legs.spread[0]=unit(ch[0xe4/4]);p.legs.spread[1]=unit(ch[0xe8/4]);
    t.pitch=fminf(constant(0x3f490fda),fmaxf(constant(0xbf490fda),c->chair_angle+ch[0xc/4]));
    t.direct_twist=1;t.twist_channel=fminf(constant(0x3f490fda),fmaxf(constant(0xbf490fda),ch[0x8/4]));
    p.torso.twist=fminf(constant(0x3f714637),fmaxf(constant(0xbf714637),ch[0x8/4]*constant(0x3e4ccccc)+ch[0x88/4]));
    p.torso.roll=fminf(constant(0x3ea0d97a),fmaxf(constant(0xbea0d97a),ch[0xa8/4]));
    t.length_offset=ch[0xb8/4];p.torso.look_gain=ch[0xbc/4];
    t.phase_keys[0]=unit(ch[0x1c/4]);t.phase_keys[1]=unit(ch[0xc0/4]);t.phase_keys[2]=unit(ch[0xd0/4]);
    t.phase_amplitude[0][0]=ch[0x28/4];t.phase_amplitude[0][1]=ch[0x2c/4];
    /* 003B1DB0 -> 003B21D8: lever rotation from the recorded gate channels.
       Its local displacement joins the original hand travel target. */
    if(type!=5) {
        gt4_driver_control_input input={ch[0x34/4],ch[0x14/4],{ch[0x20/4],ch[0x24/4]},ch[0xd8/4],ch[0xc8/4]};
        gt4_driver_controls control={0};if(!gt4_driver_controls_update(type,&input,&control))return 0;
        float sa,ca,sb,cb;pose_sincos(control.pedal[0],&sa,&ca);pose_sincos(control.pedal[1],&sb,&cb);
        float x=-c->shift_normal[0],y=c->shift_normal[1],z=-c->shift_normal[2];
        float rx=x*cb-y*sb,ry=x*sb+y*cb;
        t.shift_offset[0][0]=(rx+c->shift_normal[0])*c->shift_length;
        t.shift_offset[0][1]=(ry*ca-z*sa-c->shift_normal[1])*c->shift_length;
        t.shift_offset[0][2]=(ry*sa+z*ca+c->shift_normal[2])*c->shift_length;
    }
    unsigned shifting=g.shift_side-1,other=shifting^1;
    p.arms.elbow[shifting]=unit(ch[(shifting?0x3c:0x38)/4]);
    p.arms.elbow[other]=unit(ch[(other?0x3c:0x38)/4]);
    /* 003B3DD0: pedal compression, separate from the wheel controller. */
    if(type!=3 && type!=4){t.feet_offset[0][1]=ch[0x30/4]*constant(0xbda3d70a);
        t.feet_offset[1][1]=fmaxf(ch[0x14/4],ch[0x18/4])*constant(0xbda3d70a);}
    t.clavicle_roll[0]=ch[0x8c/4];t.clavicle_roll[1]=ch[0x90/4];
    p.head.roll_gain=unit(ch[0x64/4]);p.head.base_pitch=(ch[0x4/4]-ch[0x6c/4])+ch[0x68/4]*constant(0x3dcccccc)+constant(0x3db2b8c1);
    p.head.roll_bias=ch[0/4]+ch[0x68/4]*.25f;
    p.head.yaw_range=1;
    float sn,cs;pose_sincos(-ch[0x68/4],&sn,&cs);p.head.up[0]=0;p.head.up[1]=cs;p.head.up[2]=sn;
    int reach[5]={0};float look[3];memcpy(look,ch+0x58/4,12);
    if(!gt4_driver_targets_build(s,&p,&g,&t,&targets) || !gt4_driver_posture_solve(s,&p,&targets,look,reach))return 0;
    return gt4_driver_skeleton_matrices(s,matrices);
}
int gt4_driver_race_pose(const gt4_driver_car *c,const float ch[64],gt4_driver_skeleton *s,float matrices[20][16])
{
    if(!c || !ch || !s || !matrices)return 0;gt4_driver_skeleton copy=*s;float out[20][16];
    int mode=fegetround();if(mode<0 || (mode!=FE_TOWARDZERO && fesetround(FE_TOWARDZERO)))return 0;
    int ok=race_pose(c,ch,&copy,out);if(mode!=FE_TOWARDZERO && fesetround(mode))return 0;
    if(!ok)return 0;*s=copy;memcpy(matrices,out,sizeof out);return 1;
}

/* 004145C8: the authored eleven-sample hand velocity envelope at 00622A20.
   This is evaluated against remaining travel, not an invented duration. */
static float hand_envelope(float x,float scale)
{
    static const float samples[11]={.261464f,.489183f,.822244f,1.20427f,1.51858f,1.64127f,
                                   1.51858f,1.20427f,.822244f,.489183f,.261464f};
    float value;
    if(x<=0)value=samples[0];else if(x>=1)value=samples[10];
    else {x*=10;int i=(int)x;float f=x-(float)i;value=samples[i]*(1-f)+samples[i+1]*f;}
    return value*scale;
}
/* 0040F7C8 / 0040F670 / 0040F718, with 0044F5A0's defaults. */
static float hand_lift(float x)
{float t=(unit(x)-.5f)*2;return (1-t*t)*constant(0x3dcccccc);}
static float hand_velocity(float phase,float wheel_rate)
{float speed=fminf(constant(0x41e0cc64),fmaxf(constant(0x4160cc64),fabsf(wheel_rate)*constant(0x3f8ccccc)));
 return hand_envelope(phase,speed);}
static float hand_rest_angle(unsigned side,float x)
{x+=side?constant(0x3ff14637):constant(0xbff14637);
 x=fminf(constant(0x4020d97a),fmaxf(constant(0xc020d97a),x));
 return side?fmaxf(x,constant(0x3f96cbe3)):fminf(x,constant(0xbf96cbe3));}
static int hand_step(gt4_driver_hand_motion *h,unsigned side,int regrip,const gt4_driver_hand_motion *other,
                     unsigned *other_state,float delta,float rate,float dt,float wheel_rate)
{
    float low=constant(side?0xc034f4aa:0xc03f0242),high=constant(side?0x403f0242:0x4034f4aa);
    if(*other_state!=2){float inset=fabsf(wheel_rate)*constant(0x3dcccccc);low+=inset;high-=inset;}
    low=fminf(low,constant(0xc016cbe3));high=fmaxf(high,constant(0x4016cbe3));
    for(int pass=0;pass<3;pass++) {
        unsigned state=h->state;int again=0;
        switch(state) {
        case 0:
            h->working+=rate;
            if(regrip && (h->working<low || h->working>high)){h->working=h->working<low?low:high;h->state=1;}
            h->first=h->second=h->working;h->blend=0;break;
        case 1:
            if(*other_state==2 || *other_state==3 || *other_state==6){h->state=0;again=1;break;}
            if(*other_state==5){h->state=0;again=1;break;}
            if(fabsf(wheel_rate)<constant(0x40490fda)) {
                h->second=fminf(constant(0x3fc90fda),fmaxf(constant(0x3ea0d97a),
                    constant(0xbde38e3f)*fabsf(wheel_rate)+constant(0x3fcd87a9)));
                if(h->first<0)h->second=-h->second;
            }else {
                h->second=fminf(constant(0x3f8cbe4b),fmaxf(constant(0x3ea0d97a),
                    constant(0x3d888887)*fabsf(wheel_rate)+constant(0xbe56774d)));
                if(h->first>=0)h->second=-h->second;
            }
            h->blend=0;h->state=((!side && other->first<h->second) || (side && h->second<other->first))?2:3;
            h->travel=fabsf(h->second-h->first);if(h->travel<constant(0x3ea0d97a))h->state=0;
            h->working=h->first;again=1;break;
        case 4: {
            if(*other_state==2 || *other_state==3 || *other_state==5 || *other_state==6){h->state=0;again=1;break;}
            float x=other->first*(1-other->blend)+other->second*other->blend;
            h->second=hand_rest_angle(side,x);h->blend=0;
            h->state=((!side && other->first<h->second) || (side && h->second<other->first))?5:6;
            h->travel=fabsf(h->second-h->first);if(h->travel<constant(0x3ea0d97a))h->state=0;
            h->working=h->first;again=1;break;
        }
        case 7:
            h->state=8;h->second=constant(side?0x3f8cbe4b:0xbf8cbe4b);
            h->travel=fabsf(h->second-h->first);if(h->travel<constant(0x3e20d97a))h->state=0;
            h->working=h->first;again=1;break;
        case 2:case 3:case 5:case 6:case 8: {
            if(state==8 && *other_state==2){h->state=0;*other_state=4;break;}
            float phase=unit(fabsf(h->second-h->working)/h->travel);
            if(state!=8)h->blend=(state==3 || state==6)?1-phase:0;
            h->depth=state==6?0:hand_lift(phase)*(state==3?constant(0x3dcccccc):1);
            float step=hand_velocity(phase,wheel_rate)*dt;
            int up=h->working<h->second;h->working+=up?step:-step;
            if(state==2 || state==5 || state==8)h->first=h->working;
            if((up && h->second<h->working) || (!up && h->working<h->second)) {
                h->first=h->working=h->second;h->blend=h->depth=0;h->state=0;
                if(state==2 || state==3)*other_state=4;
            }
            break;
        }
        default:return 0;
        }
        if(!again)return 1;
    }
    (void)delta;return 0;
}
int gt4_driver_hand_step(gt4_driver_hand_motion *hand,unsigned side,int regrip,
                         const gt4_driver_hand_motion *other,unsigned *other_state,
                         float delta,float rate,float dt,float wheel_rate)
{
    if(!hand || !other || !other_state || side>1 || hand->state>8 || *other_state>8)return 0;
    const float values[]={hand->first,hand->second,hand->blend,hand->working,hand->radius,hand->depth,hand->travel,
                          other->first,other->second,other->blend,delta,rate,dt,wheel_rate};
    for(int k=0;k<14;k++)if(!isfinite(values[k]))return 0;
    if(dt<=0 || dt>.1f || ((hand->state==2 || hand->state==3 || hand->state==5 || hand->state==6 || hand->state==8) && hand->travel<=0))return 0;
    gt4_driver_hand_motion next=*hand;unsigned os=*other_state;int mode=fegetround();
    if(mode<0 || (mode!=FE_TOWARDZERO && fesetround(FE_TOWARDZERO)))return 0;
    int ok=hand_step(&next,side,regrip,other,&os,delta,rate,dt,wheel_rate);
    if(mode!=FE_TOWARDZERO && fesetround(mode))return 0;if(!ok)return 0;*hand=next;*other_state=os;return 1;
}
/* 0040F850 / 0040F888: classic hands have their own bounds and return path. */
static int classic_hand_step(gt4_driver_hand_motion *h,unsigned side,unsigned type,unsigned *other_state,
                             float steering,float delta,float dt,float wheel_rate)
{
    if(type==4){h->working+=delta;h->first=h->second=h->working;h->state=0;h->blend=h->radius=h->depth=0;return 1;}
    float inset=side==(unsigned)(delta<0)?constant(0x3eb2b8c1):0;
    float low=constant(side?0x3f1c61a9:0xc021f76f),high=constant(side?0x4021f76f:0xbf1c61a9);
    for(int pass=0;pass<2;pass++)switch(h->state) {
    case 0:
        h->working+=delta;
        if((h->working<=low-inset && (delta<0 || !*other_state)) ||
           (h->working>=high+inset && (delta>0 || !*other_state)))h->state=1;
        h->first=h->second=h->working;h->blend=0;return 1;
    case 1:
        h->state=(*other_state==2 || *other_state==3 || *other_state==5 || *other_state==6)?0:2;
        break;
    case 2: {
        float base=steering-constant(0x4016cbe3),pi=constant(0x40490fda),two_pi=constant(0x40c90fda);
        for(int i=0;i<8;i++) {
            float x=base+(float)(i%4)*constant(0x3fc90fda);x-=(-pi);
            x-=floorf(x/two_pi)*two_pi;x+=(-pi);
            float lo=i<4?low:low-inset,hi=i<4?high:high+inset;
            if(lo<=x && x<=hi){h->second=x;break;}
        }
        float speed=fminf(constant(0x41e0cc64),fmaxf(constant(0x4160cc64),fabsf(wheel_rate)*constant(0x3f8ccccc)))*dt;
        int up=h->working<h->second;h->working+=up?speed:-speed;h->first=h->working;
        if((up && h->second<h->working) || (!up && h->working<h->second)){
            h->first=h->working=h->second;h->blend=h->depth=0;h->state=*other_state=0;
        }
        return 1;
    }
    default:return 0;
    }
    return 0;
}
int gt4_driver_classic_hand_step(gt4_driver_hand_motion *h,unsigned side,unsigned type,unsigned *other_state,
                                  float steering,float delta,float dt,float wheel_rate)
{
    if(!h || !other_state || side>1 || (type!=3 && type!=4) || h->state>2 ||
       !isfinite(steering) || !isfinite(delta) || !isfinite(dt) || dt<=0 || dt>.1f || !isfinite(wheel_rate))return 0;
    gt4_driver_hand_motion next=*h;unsigned os=*other_state;int mode=fegetround();
    const float values[]={h->first,h->second,h->blend,h->working,h->radius,h->depth,h->travel};
    for(int k=0;k<7;k++)if(!isfinite(values[k]))return 0;
    if(mode<0 || (mode!=FE_TOWARDZERO && fesetround(FE_TOWARDZERO)))return 0;
    int ok=classic_hand_step(&next,side,type,&os,steering,delta,dt,wheel_rate);
    if(!isfinite(next.first) || !isfinite(next.second) || !isfinite(next.working))ok=0;
    if(mode!=FE_TOWARDZERO && fesetround(mode))return 0;if(!ok)return 0;*h=next;*other_state=os;return 1;
}
/* 00411EB0: transmission's authored H-pattern/sequential gate position. */
static void lever_gate(const int tr[4],int gear,float out[2])
{
    out[0]=out[1]=0;
    if(tr[0]==4){out[1]=gear>0?-1:1;return;}
    if(tr[0]!=1)return;
    if(gear>=tr[3])gear=tr[3];gear=gear>0?gear+tr[1]-1:tr[2];
    out[0]=gear>0?(float)((gear-1)/2-1)*.5f:(float)(gear/2-2)*.5f;
    out[1]=(gear&1)?1:-1;
}
/* 00411628: acceleration-limited travel through neutral, with damping when
   outside the middle of a gate. Defaults: 0044F5A0 +20..34. */
static int lever_step(gt4_driver_lever_motion *l,const int tr[4],int gear,float dt)
{
    float goal[2];lever_gate(tr,gear,goal);
    l->roll[0]=l->roll[1]=0;
    if(l->gate[0]==goal[0])l->roll[0]=l->gate[1]<goal[1]?-constant(0x3f490fda):
        l->gate[1]>goal[1]?constant(0x3f490fda):0;
    else if(l->gate[1]==0)l->roll[1]=l->gate[0]<goal[0]?-constant(0x3d0efa35):constant(0x3d0efa35);
    else l->roll[0]=l->gate[1]>0?constant(0x3f490fda):-constant(0x3f490fda);
    if(l->gate[0]==goal[0] && l->gate[1]==goal[1]){l->wait=0;return 1;}
    if(l->wait<1){l->wait+=6*dt;return 0;}
    if(l->gate[0]!=goal[0] && l->gate[1]==0) {
        float dir=goal[0]>l->gate[0]?1:-1;
        l->rate[0]+=dir*30*dt;l->rate[0]=fminf(15,fmaxf(-15,l->rate[0]));l->gate[0]+=l->rate[0]*dt;
        if((dir>0 && goal[0]<=l->gate[0]) || (dir<0 && l->gate[0]<=goal[0])){l->gate[0]=goal[0];l->rate[0]=0;}
        else if((dir>0 && l->gate[0]<-2) || (dir<0 && l->gate[0]>2)){
            l->gate[0]=dir>0?-2:2;l->rate[0]=0;}
        return 0;
    }
    float target=l->gate[0]==goal[0]?goal[1]:0,dir=target>l->gate[1]?1:-1;
    float accel=dir*60,damping=fabsf(l->gate[1])>=.75f?3:0;
    accel-=l->rate[1]*damping;
    l->rate[1]+=accel*dt;l->rate[1]=fminf(30,fmaxf(-30,l->rate[1]));l->gate[1]+=l->rate[1]*dt;
    if((dir>0 && target<=l->gate[1]) || (dir<0 && l->gate[1]<=target)) {
        l->gate[1]=target;l->rate[1]=0;
        if(l->gate[0]==goal[0]){l->wait=0;return 1;}
    }else if((dir>0 && l->gate[1]<-1) || (dir<0 && l->gate[1]>1)){
        l->gate[1]=dir>0?-1:1;l->rate[1]=0;}
    return 0;
}
int gt4_driver_lever_step(gt4_driver_lever_motion *lever,const int tr[4],int gear,float dt)
{
    if(!lever || !tr || tr[0]<0 || tr[0]>4 || gear<0 || gear>16 || !isfinite(dt) || dt<=0 || dt>.1f)return -1;
    for(int k=0;k<2;k++)if(!isfinite(lever->gate[k]) || !isfinite(lever->rate[k]))return -1;
    if(!isfinite(lever->wait))return -1;gt4_driver_lever_motion next=*lever;int mode=fegetround();
    if(mode<0 || (mode!=FE_TOWARDZERO && fesetround(FE_TOWARDZERO)))return -1;
    int done=lever_step(&next,tr,gear,dt);
    if(mode!=FE_TOWARDZERO && fesetround(mode))return -1;*lever=next;return done;
}
int gt4_driver_animation_init(gt4_driver_animation *a,const gt4_driver_car *c,const int tr[4],int gear)
{
    if(!a || !c || !tr || gear<0 || gear>16 || tr[0]<0 || tr[0]>4)return 0;
    gt4_driver_animation next={0};if(!gt4_driver_record_init(c,next.channels))return 0;
    memcpy(next.transmission,tr,sizeof next.transmission);next.gear=gear;lever_gate(tr,gear,next.lever.gate);
    next.channels[0x20/4]=next.lever.gate[0];next.channels[0x24/4]=next.lever.gate[1];
    for(int side=0;side<2;side++) {
        gt4_driver_hand_motion *h=&next.hand[side];h->first=next.channels[(side?0x4c:0x48)/4];
        h->second=h->working=h->first;h->blend=next.channels[(side?0x54:0x50)/4];
    }
    next.classic_right=next.hand[1].first;next.initialized=1;*a=next;return 1;
}
/* 00412018's ordinary gear states. Anticipation flags are supplied by
   0040CA58's read-only vehicle adapter; no render-frame timers. */
static void animation_shift(gt4_driver_animation *a,const gt4_driver_car *c,const gt4_driver_animation_input *in)
{
    float *ch=a->channels,phase=ch[0x1c/4],want_roll[2]={0},dt=in->dt;
    int side=c->wheel[0]<c->shift[0]?1:0;
    int anticipation=(in->upshift || in->downshift || in->speed_shift) && a->turn_hold<=0 && !in->clutch_blocked && a->transmission[0]==1;
    switch(a->shift_state) {
    case 0:
        a->phase_rate=0;phase=0;
        if(in->gear!=a->gear) {
            float old[2],now[2];lever_gate(a->transmission,a->gear,old);lever_gate(a->transmission,in->gear,now);
            if(old[0]!=now[0] || old[1]!=now[1])a->shift_state=2;
        }else if(anticipation)a->shift_state=1;
        if(!a->shift_state)break;
        /* fall through: states 1/2 share the original same-step transition. */
    case 1:case 2:
        if(phase>.5f)phase=unit(1-phase);a->phase_rate=0;
        a->shift_state=a->shift_state==1?3:4;
        /* fall through */
    case 3:case 4:
        if(in->gear==a->gear && !anticipation){a->shift_state=8;break;}
        a->phase_rate=hand_envelope(phase*2,a->shift_state==3?1:2.5f);
        if(phase>=.5f){phase=.5f;a->phase_rate=0;a->shift_state=in->gear!=a->gear?6:5;}
        break;
    case 5: {
        if(in->gear==a->gear && !anticipation){a->shift_state=8;break;}
        gt4_driver_lever_motion preview={0};memcpy(preview.gate,a->lever.gate,sizeof preview.gate);
        gt4_driver_lever_step(&preview,a->transmission,in->gear,dt);
        memcpy(want_roll,preview.roll,sizeof want_roll);
        if(in->gear==a->gear)break;a->shift_state=6;
        /* fall through */
    }
    case 6:
        if(gt4_driver_lever_step(&a->lever,a->transmission,in->gear,dt)==1){a->gear=in->gear;a->shift_state=7;a->hold=0;}
        memcpy(want_roll,a->lever.roll,sizeof want_roll);
        break;
    case 8:
        if(phase<.5f){phase=unit(1-phase);a->phase_rate=0;}
        a->shift_state=9;
        /* fall through */
    case 7:case 9: {
        float scale=constant(0x3dc391ce)*fabsf(a->hand[side].first)+constant(0x3efffffe);
        a->phase_rate=hand_envelope(phase*2-1,fminf(constant(0x3f4ccccc),fmaxf(.5f,scale)));
        if(in->gear!=a->gear)a->shift_state=2;
        else if(anticipation)a->shift_state=1;
        else if(a->shift_state==7){a->hold+=dt;if(a->hold>=.5f)a->shift_state=8;}
        else if(phase>=1){phase=0;a->phase_rate=0;a->shift_state=0;}
        break;
    }
    }
    phase+=a->phase_rate*dt;ch[0x1c/4]=phase;
    ch[0x20/4]=a->lever.gate[0];ch[0x24/4]=a->lever.gate[1];
    for(int k=0;k<2;k++) {
        float accel=(want_roll[k]-a->lever_roll[k])*1520-a->lever_roll_rate[k]*80;
        a->lever_roll_rate[k]+=accel*dt;a->lever_roll[k]+=a->lever_roll_rate[k]*dt;
        float limit=constant(k?0x3d0efa35:0x3f490fda);
        if(a->lever_roll[k]>limit){a->lever_roll[k]=limit;a->lever_roll_rate[k]=0;}
        else if(a->lever_roll[k]<-limit){a->lever_roll[k]=-limit;a->lever_roll_rate[k]=0;}
    }
    ch[0x28/4]=a->lever_roll[0];ch[0x2c/4]=a->lever_roll[1];
    float clutch=0;
    if(in->clutch_blocked)clutch=1;
    else if(phase>=0 && phase<=constant(0x3e4ccccc))clutch=phase*5;
    else if(phase<=constant(0x3f4ccccc))clutch=1;
    else if(phase>=constant(0x3f4ccccc) && phase<=1)clutch=1-(phase-constant(0x3f4ccccc))*5;
    float current=ch[0x30/4];current+=current<clutch?5*dt:-5*dt;
    if((ch[0x30/4]<clutch && current>clutch) || (ch[0x30/4]>=clutch && current<clutch))current=clutch;
    ch[0x30/4]=current;
}
/* 00414130: type-five pedal lever reaches, holds, and returns the hand.
   Its input is the original car+598 provider (003446A8). */
static void classic_pedal_lever(gt4_driver_animation *a,const gt4_driver_animation_input *in)
{
    float *ch=a->channels,phase=ch[0xd0/4],dt=in->dt,goal=(in->lever_pedal-.75f)*4*constant(0xbf060a90);
    switch(a->shift_state) {
    case 0:phase=a->phase_rate=0;if(in->lever_pedal>.75f)a->shift_state=3;break;
    case 3:
        a->phase_rate+=((1.5f-phase)*100-a->phase_rate*20)*dt;phase+=a->phase_rate*dt;
        if(phase>=1){phase=1;a->phase_rate=0;a->shift_state=6;}break;
    case 6:
        a->lever.rate[1]+=((goal-ch[0x24/4])*16-a->lever.rate[1]*8)*dt;
        a->lever.rate[1]=fminf(constant(0x3fc90fda),fmaxf(constant(0xbfc90fda),a->lever.rate[1]));
        ch[0x24/4]+=a->lever.rate[1]*dt;
        if(ch[0x24/4]>0){ch[0x24/4]=0;a->lever.rate[1]=0;a->shift_state=9;}break;
    case 9:
        a->phase_rate+=((-.5f-phase)*100-a->phase_rate*20)*dt;phase+=a->phase_rate*dt;
        if(phase<0){phase=a->phase_rate=0;a->shift_state=0;}break;
    }
    ch[0xd0/4]=phase;ch[0x30/4]=ch[0x1c/4]=ch[0x20/4]=0;
    if(phase>=1){float x=ch[0x24/4]/constant(0xbf060a90);
        ch[0xb0/4]=x*constant(0xbf860a90)-(1-x)*constant(0x3fc90fda);}
    else ch[0xb0/4]=phase*constant(0xbf860a90);
}
static int animation_step(gt4_driver_animation *a,const gt4_driver_car *c,const gt4_driver_animation_input *in)
{
    float *ch=a->channels,previous=ch[0x34/4],limit=constant(0x41060a91),steering=in->steering;
    unsigned type=gt4_driver_control_type(c);
    if(c->shift_type==1)limit=fminf(limit,constant(0x3fc90fda));
    if(type==3 || type==4 || type==5) {
        float bound=constant(type==3?0x40490fda:0x4016cbe3);
        float goal=fminf(bound,fmaxf(-bound,(-in->steering_fraction)*(type==3?540:135)/30));
        float accel=(goal-previous)*400-a->steering_rate[0]*40;
        a->steering_rate[0]+=accel*in->dt;
        a->steering_rate[0]=fminf(constant(0x41fb53d0),fmaxf(-constant(0x41fb53d0),a->steering_rate[0]));
        steering=previous+a->steering_rate[0]*in->dt;
    }else steering=fminf(limit,fmaxf(-limit,steering));
    ch[0x34/4]=steering;float delta=steering-previous;
    float rate=type>=3?a->steering_rate[0]:delta/in->dt,total=rate;
    for(int k=3;k>0;k--){a->steering_rate[k]=a->steering_rate[k-1];total+=a->steering_rate[k];}
    a->steering_rate[0]=rate;float mean=total/4;
    if(a->turn_hold>0)a->turn_hold-=in->dt*.25f;
    if(fabsf(previous)>constant(0x3ea0d97a))a->turn_hold=1;
    ch[0x14/4]=unit(in->throttle);ch[0x18/4]=unit(in->brake);
    if(!type && (a->transmission[0]==1 || a->transmission[0]==4))animation_shift(a,c,in);
    /* 00413FC8 / 00413F68: Patent's single lever follows the original
       throttle/brake selector, not a modern H-pattern gear animation. */
    if(type==4) {
        float goal=in->throttle>0?constant(0x3f060a90):in->brake>0?constant(0xbdb2b8c1):0;
        float accel=(goal-ch[0x24/4])*100-a->lever.rate[1]*20;
        a->lever.rate[1]+=accel*in->dt;
        a->lever.rate[1]=fminf(constant(0x3fc90fda),fmaxf(constant(0xbfc90fda),a->lever.rate[1]));
        ch[0x24/4]+=a->lever.rate[1]*in->dt;ch[0x1c/4]=.5f;ch[0x20/4]=ch[0x30/4]=0;
    }
    if(type==5)classic_pedal_lever(a,in);
    /* 00410E60 / 00410B70 / 00410880 dispatch the original hand controllers. */
    if(!type || type==3 || type==4) {
        int regrip=c->shift_type!=1;gt4_driver_hand_motion other=a->hand[1];
        if(type){if(!gt4_driver_classic_hand_step(&a->hand[0],0,type,&a->hand[1].state,steering,delta,in->dt,mean))return 0;}
        else if(!gt4_driver_hand_step(&a->hand[0],0,regrip,&other,&a->hand[1].state,steering,delta,in->dt,mean))return 0;
        other=a->hand[0];
        if(type){if(!gt4_driver_classic_hand_step(&a->hand[1],1,type,&a->hand[0].state,steering,delta,in->dt,mean))return 0;}
        else if(!gt4_driver_hand_step(&a->hand[1],1,regrip,&other,&a->hand[0].state,steering,delta,in->dt,mean))return 0;
        for(int side=0;side<2;side++) {
            const gt4_driver_hand_motion *h=&a->hand[side];ch[(side?0x4c:0x48)/4]=h->first;
            ch[(side?0x44:0x40)/4]=h->second;ch[(side?0x54:0x50)/4]=h->blend;
            ch[(side?0x74:0x70)/4]=h->radius;ch[(side?0x7c:0x78)/4]=h->depth;
        }
    }
    if(type==5) {
        gt4_driver_hand_motion other=a->hand[1];
        if(!gt4_driver_hand_step(&a->hand[0],0,0,&other,&a->hand[1].state,steering,delta,in->dt,mean))return 0;
        gt4_driver_hand_motion *h=&a->hand[0];ch[0x48/4]=h->first;ch[0x40/4]=h->second;
        ch[0x50/4]=h->blend;ch[0x70/4]=h->radius;ch[0x78/4]=h->depth;
        ch[0x4c/4]=ch[0x44/4]=(a->classic_right-ch[0x24/4])+constant(0x3e060a90);ch[0x54/4]=0;
    }
    /* 004113B8 selects the shift-side hand; 00411570 keeps type-five's
       right hand closed on its lever while the left hand can release. */
    int shift_side=c->wheel[0]<c->shift[0]?1:0;
    for(int side=0;side<2;side++) {
        unsigned gear=(!type && side==shift_side)?a->shift_state:0;
        if(type==5 && side==1)ch[0x84/4]=1;
        else ch[(side?0x84:0x80)/4]=grasp_step(a->hand[side].state,gear,ch[0x1c/4],ch[0xc0/4],in->dt,ch[(side?0x84:0x80)/4]);
    }
    return 1;
}
int gt4_driver_animation_step(gt4_driver_animation *a,const gt4_driver_car *c,const gt4_driver_animation_input *in)
{
    if(!a || !a->initialized || !c || !in || in->gear<0 || in->gear>16)return 0;
    const float values[]={in->dt,in->steering,in->steering_fraction,in->throttle,in->brake,in->lever_pedal};
    for(int k=0;k<6;k++)if(!isfinite(values[k]))return 0;if(in->dt<=0 || in->dt>.1f)return 0;
    gt4_driver_animation next=*a;int mode=fegetround();
    if(mode<0 || (mode!=FE_TOWARDZERO && fesetround(FE_TOWARDZERO)))return 0;
    int ok=animation_step(&next,c,in);for(int k=0;k<64;k++)if(!isfinite(next.channels[k]))ok=0;
    if(mode!=FE_TOWARDZERO && fesetround(mode))return 0;if(!ok)return 0;*a=next;return 1;
}
