/* Course props: scalar translation of 003E2B28/003E3748/003E3840 and
   003E3C48/003E3ED0. Matrices are the native row-major adapter to the
   original VU columns. GDGT supplies every mass, inertia and support point. */
#include "course_objects.h"
#include "car.h"
#include <math.h>
#include <string.h>
#define K(b) gt4_f32(b)
static float readf(const uint8_t *p){float f;memcpy(&f,p,4);return f;}
static float dot(const float a[3],const float b[3]){return (a[0]*b[0]+a[1]*b[1])+a[2]*b[2];}
static void cross(float out[3],const float a[3],const float b[3])
{float t[3]={a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};memcpy(out,t,12);}
static void rotate(float out[3],const float r[9],const float p[3],int inverse)
{
    float t[3];for(int i=0;i<3;i++)t[i]=inverse?
        (r[i]*p[0]+r[3+i]*p[1])+r[6+i]*p[2]:
        (r[3*i]*p[0]+r[3*i+1]*p[1])+r[3*i+2]*p[2];memcpy(out,t,12);
}
static void point_local(const gt4_course_gadget *g,unsigned point,float p[3])
{for(int i=0;i<3;i++)p[i]=readf(g->descriptor+0x10+point*16+i*4);}
static void point_world(const gt4_course_gadget *g,unsigned point,float p[3])
{float local[3];point_local(g,point,local);rotate(p,g->rot,local,0);for(int i=0;i<3;i++)p[i]+=g->pos[i];}
static void limit_spin(gt4_course_gadget *g)
{ /* 003E3218: any component above 20 scales all three by 0.2. */
    if(fabsf(g->angular[0])>20 || fabsf(g->angular[1])>20 || fabsf(g->angular[2])>20)
        for(int i=0;i<3;i++)g->angular[i]*=K(0x3e4ccccc);
}
float gt4_course_object_contact(gt4_course_gadget *g,unsigned point,
                               const float planes[5][4],const float position[3],
                               const float velocity[3],float yaw_rate)
{
    /* 003E3ED0: point velocity relative to the car; nearest of the five
       authored planes (003E43B8) separates the prop, then applies impulse. */
    float local[3],p[3],relative[3],turn[3];point_local(g,point,local);point_world(g,point,p);
    cross(turn,g->angular,local);rotate(turn,g->rot,turn,0);
    float car_velocity[3]={velocity[0]+(p[2]-position[2])*yaw_rate,
                          velocity[1],velocity[2]-(p[0]-position[0])*yaw_rate};
    for(int i=0;i<3;i++)relative[i]=(turn[i]+g->velocity[i])-car_velocity[i];
    int plane=-1;float depth=K(0x47c35000);
    for(int j=0;j<5;j++){float d=-(dot(planes[j],p)+planes[j][3]);if(d<depth){depth=d;plane=j;}}
    if(plane<0)return 0;
    const float *n=planes[plane];for(int i=0;i<3;i++)g->pos[i]+=depth*n[i];
    float normal_local[3],arm[3],weighted[3],effect[3];rotate(normal_local,g->rot,n,1);
    cross(arm,local,normal_local);
    float inertia[3];for(int i=0;i<3;i++){inertia[i]=readf(g->descriptor+4+i*4)*1.5f;weighted[i]=arm[i]/inertia[i];}
    cross(effect,weighted,local);rotate(effect,g->rot,effect,0);
    float inverse_mass=1/readf(g->descriptor+0x94),denominator=inverse_mass+dot(n,effect);
    float restitution=(g->descriptor[2]&1)?K(0x3d4ccccc):K(0x3c23d70a);
    float impulse=(-(restitution+1)*dot(n,relative))/denominator;
    for(int i=0;i<3;i++){
        g->velocity[i]+=(impulse*inverse_mass)*n[i];
        g->angular[i]+=(impulse*arm[i])/inertia[i];
    }
    return impulse;
}
int gt4_course_object_ground(gt4_course_gadget *g,unsigned point,const float n[3])
{
    /* 003E3840: support impulse and Coulomb friction; the original's
       inverse-mass/inertia terms and the two different friction factors. */
    float local[3],nl[3],arm[3],weighted[3],effect[3],turn[3],v[3];
    point_local(g,point,local);rotate(nl,g->rot,n,1);cross(arm,local,nl);
    float inertia[3];for(int i=0;i<3;i++){inertia[i]=readf(g->descriptor+4+4*i);weighted[i]=arm[i]/inertia[i];}
    cross(effect,weighted,local);rotate(effect,g->rot,effect,0);
    float inverse_mass=1/readf(g->descriptor+0x94),denominator=inverse_mass+dot(n,effect);
    cross(turn,g->angular,local);rotate(turn,g->rot,turn,0);
    for(int i=0;i<3;i++)v[i]=turn[i]+g->velocity[i];
    float restitution=dot(v,v)<K(0x3f4f5c28)?0:K(0x3e800000);
    float speed=dot(n,v),impulse=(-(restitution+1)*speed)/denominator;
    if(impulse<0)return 0;
    g->impact+=impulse;
    float acceleration=impulse*inverse_mass,friction=acceleration*.5f,tangent[3];
    for(int i=0;i<3;i++)tangent[i]=v[i]-speed*n[i];
    float len2=dot(tangent,tangent);friction=len2>0?friction/sqrtf(len2):1;
    if(friction>1)friction=1;
    float clipped=fminf(1,friction);
    for(int i=0;i<3;i++){
        g->velocity[i]+=n[i]*acceleration-clipped*tangent[i];
    }
    rotate(tangent,g->rot,tangent,1);cross(weighted,local,tangent);
    float mass_friction=friction*readf(g->descriptor+0x94);
    for(int i=0;i<3;i++)g->angular[i]+=(impulse*arm[i]-mass_friction*weighted[i])/inertia[i];
    return 1;
}
static void ground(gt4_course_gadget *g,const float points[8][3],unsigned count)
{
    /* 003E3748 uses the deepest support point; support bytes accumulate
       exactly as the original, including the zero reset above the plane. */
    int deepest=-1;float height=0;
    for(unsigned i=0;i<count;i++){
        const float *p=points[i];float d=dot(g->normal,p)+g->plane;
        if(d<0){float h=-((g->normal[0]*p[0]+g->normal[2]*p[2])+g->plane)/g->normal[1]-p[1];
            if(height<h){height=h;deepest=(int)i;g->support[i]++;}}
        else g->support[i]=0;
    }
    g->pos[1]+=height;
    if(deepest>=0)gt4_course_object_ground(g,(unsigned)deepest,g->normal);
    limit_spin(g);
}
static float energy(const gt4_course_gadget *g)
{
    float e=0;for(int i=0;i<3;i++)e+=(readf(g->descriptor+4+i*4)*g->angular[i])*g->angular[i];
    return e+readf(g->descriptor+0x94)*dot(g->velocity,g->velocity);
}
int gt4_course_object_wall(gt4_course_gadget *g,uint32_t wall,
                          const float previous[3],const float points[8][3])
{
    /* 003E44E8: centre sweep and the descriptor's reach; 003E4708
       resolves the extreme support and prevents a wall adding energy. */
    if(GT4_U16(wall+14)&2)return 0;
    int16_t link=(int16_t)GT4_U16(wall+12);
    uint32_t next=wall+(link<0?(uint32_t)(link*16):16);
    float x=GT4_F32(wall),z=GT4_F32(wall+8),dx=GT4_F32(next)-x,dz=GT4_F32(next+8)-z;
    float px=g->pos[0]-x,pz=g->pos[2]-z,len2=dx*dx+dz*dz;
    float t=(dx*px+dz*pz)/len2,reach=readf(g->descriptor+0x90);
    int separated=0;
    if(t>0 && t<1){
        float ez=t*dz-pz,ex=t*dx-px;
        if(ex*ex+ez*ez<reach*reach){
            float r=reach/sqrtf(len2);
            g->pos[0]+=ex+dz*r;g->pos[2]+=ez-dx*r;separated=1;
        }
    }
    if(!separated){
        float side=dx*pz-dz*px;
        if(side<0)return 0;
        float side_old=dx*(previous[2]-z)-dz*(previous[0]-x);
        if(side_old>0)return 0;
        float sx=g->pos[0]-previous[0],sz=g->pos[2]-previous[2],sy=g->pos[1]-previous[1];
        if(sx*(z-g->pos[2])-sz*(x-g->pos[0])<0)return 0;
        if(sx*(GT4_F32(next+8)-g->pos[2])-sz*(GT4_F32(next)-g->pos[0])>0)return 0;
        float f=side/(side-side_old);
        g->pos[0]-=f*sx;g->pos[1]-=f*sy;g->pos[2]-=f*sz;
    }
    float len=sqrtf(len2),normal[3]={dz/len,0,-dx/len};
    /* D_006A396C is 0x7fffffff, read as maximum finite by EE FPU. */
    unsigned choice=0,count=g->descriptor[3];float best=K(0x7f7fffff);
    for(unsigned i=0;i<count;i++){float d=dot(normal,points[i]);if(d<best){best=d;choice=i;}}
    float before=energy(g);gt4_course_object_ground(g,choice,normal);
    if(energy(g)>before){memset(g->velocity,0,12);memset(g->angular,0,12);}
    limit_spin(g);return 1;
}
static void walls(gt4_course_gadget *g,uint32_t mesh,const float previous[3],const float points[8][3])
{
    /* 00395A38/003E6A58: the ground triangle's wall list, centre sweep
       rectangle (003E2B28 +5D8..620), signed closing-vertex links. */
    int32_t id=g->ground_id;
    float x0=fminf(previous[0],g->pos[0]),x1=fmaxf(previous[0],g->pos[0]);
    float z0=fminf(previous[2],g->pos[2]),z1=fmaxf(previous[2],g->pos[2]);
    if(id<0){gt4_ray_hit hit;float at[3]={(x0+x1)*.5f,10000,(z0+z1)*.5f};
        if(!gt4_course_ray_down(mesh,&hit,at))return;
        id=(int32_t)((uint16_t)hit.tri|(uint32_t)(uint16_t)hit.leaf<<16);}
    if((int16_t)((uint32_t)id>>16)<0)return;
    uint32_t list=GT4_U32(GT4_U32(mesh+0x78)+((uint32_t)id&0xffff)*12+8);
    uint32_t from=list&0xffffff,to=from+(list>>24);
    for(uint32_t i=from;i<to;i++){
        int16_t idx=(int16_t)GT4_U16(GT4_U32(mesh+0x8c)+i*2);
        uint32_t wall=GT4_U32(mesh+0x84)+(uint32_t)(idx*16);
        float x=GT4_F32(wall),z=GT4_F32(wall+8);
        int out=(x<x0)|((x1<x)<<1)|((z<z0)<<2)|((z1<z)<<3);
        if(out){int16_t link=(int16_t)GT4_U16(wall+12);uint32_t next=wall+(link<0?(uint32_t)(link*16):16);
            x=GT4_F32(next);z=GT4_F32(next+8);
            if(out&((x<x0)|((x1<x)<<1)|((z<z0)<<2)|((z1<z)<<3)))continue;}
        gt4_course_object_wall(g,wall,previous,points);
    }
}
void gt4_course_object_integrate(gt4_course_gadget *g,float dt)
{
    /* 003E2B28 +1CC..510: three-stage symplectic rotation, with
       D_006A3960 and D_008456B0's coefficients; Euler wrap is one turn. */
    const float weights[3]={K(0x3facf45b),K(0xbfd9e8b6),K(0x3facf45b)};
    const float moves[4]={K(0x3f2cf45b),K(0xbe33d16c),K(0xbe33d16c),K(0x3f2cf45b)};
    float inertia[3];for(int i=0;i<3;i++)inertia[i]=readf(g->descriptor+4+i*4);
    for(int stage=0;stage<4;stage++){
        float sx=sinf(g->angles[0]),cx=cosf(g->angles[0]),sz=sinf(g->angles[2]),cz=cosf(g->angles[2]);
        float x=g->angular[0],y=g->angular[1],z=g->angular[2],step=dt*moves[stage];
        float a=(x*sz-y*cz)/cx;
        g->angles[1]+=a*step;
        g->angles[0]+=-(x*cz+y*sz)*step;
        g->angles[2]+=-(a*sx+z)*step;
        if(stage<3){
            float t=dt*weights[stage];
            g->angular[1]+=(((inertia[2]-inertia[0])*x)*z)*t/inertia[1];
            g->angular[0]+=(((inertia[1]-inertia[2])*y)*z)*t/inertia[0];
            g->angular[2]+=(((inertia[0]-inertia[1])*x)*y)*t/inertia[2];
        }
    }
    for(int i=0;i<3;i++){if(g->angles[i]<0)g->angles[i]+=K(0x40c90fda);if(g->angles[i]>=K(0x40c90fda))g->angles[i]-=K(0x40c90fda);}
    for(int i=0;i<3;i++)g->pos[i]+=g->velocity[i]*dt;
    g->velocity[1]+=(-readf(g->descriptor+0x94)*K(0x411cf5c2))/readf(g->descriptor+0x94)*dt;
}
static void car_planes(uint32_t h,float out[5][4],float position[3],float velocity[3])
{
    /* 003E5108: yaw-only car transform and five planes of D_006A3978,
       through (half-length,0,-half-width), (-length,0,-width),
       (-length,0,front), (length,0,front), and (0,1.5,0). */
    float yaw=GT4_F32(h+0x474),c=cosf(yaw),s=sinf(yaw);
    if(fabsf(yaw)>K(0x42c90fda)){GT4_F32(h+0x474)=0;c=1;s=0;}
    position[0]=GT4_F32(h+0x454);position[1]=GT4_F32(h+0x45c);position[2]=-GT4_F32(h+0x458);
    velocity[0]=GT4_F32(h+0x460);velocity[1]=GT4_F32(h+0x468);velocity[2]=-GT4_F32(h+0x464);
    float w=GT4_F32(h+0x48c),f=GT4_F32(h+0x490),l=GT4_F32(h+0x494);
    float corners[5][3]={{l,0,-w},{-l,0,-w},{-l,0,f},{l,0,f},{0,1.5f,0}};
    for(int j=0;j<5;j++){
        float local[3];for(int i=0;i<3;i++)local[i]=GT4_F32(0x006a3978+j*16+i*4);
        out[j][0]=c*local[0]+s*local[2];out[j][1]=local[1];out[j][2]=-s*local[0]+c*local[2];
        float p[3]={position[0]+(c*corners[j][0]+s*corners[j][2]),position[1]+corners[j][1],position[2]+(-s*corners[j][0]+c*corners[j][2])};
        out[j][3]=-dot(out[j],p);
    }
}
void gt4_course_objects_init(gt4_course_gadgets *g,uint32_t course)
{
    if(!g)return;uint32_t mesh=GT4_U32(GT4_U32(course+4)+0xdc);
    for(unsigned j=0;j<g->count;j++){
        gt4_course_gadget *p=g->instance+j;gt4_ray_hit hit;float pos[3]={p->pos[0],p->pos[1]+1,p->pos[2]};
        if(gt4_course_ray_down(mesh,&hit,pos)){
            gt4_course_gadget_surface(p,hit.normal,hit.plane,hit.u,hit.v);
            p->ground_id=(int32_t)((uint16_t)hit.tri|(uint32_t)(uint16_t)hit.leaf<<16);
        }
    }
}
void gt4_course_objects_step(gt4_course_gadgets *g,uint32_t cond,float dt)
{
    if(!g)return;
    uint32_t course=GT4_U32(cond+0xcbd8),mesh=GT4_U32(GT4_U32(course+4)+0xdc);
    int section=(int32_t)GT4_U32(course+0xb0),cars=(int)GT4_U32(cond+0xf8a0);
    float planes[6][5][4],position[6][3],velocity[6][3];uint32_t hull[6];
    if(cars<0 || cars>6)return;
    for(int i=0;i<cars;i++){hull[i]=cond+0xda78+(unsigned)i*0x4f0;car_planes(hull[i],planes[i],position[i],velocity[i]);}
    for(unsigned pass=0;pass<2;pass++){
        if(pass && (section<0 || (unsigned)section>=g->sections))continue;
        unsigned slot=pass?(unsigned)section+1:0;
        for(unsigned j=0;j<g->length[slot];j++){
            gt4_course_gadget *p=g->instance+g->first[slot]+j;const uint8_t *d=p->descriptor;
            if(!p->active || p->fixed)continue;p->impact=0;
            if(p->age){p->age++;if((float)p->age*K(0x3c888888)>K(0x3fd99999)){p->active=0;continue;}}
            unsigned count=d[3];if(count>8)continue;
            unsigned supports=0;for(unsigned k=0;k<count;k++)supports+=p->support[k]==2;
            int asleep=supports>=3;
            float previous[3];memcpy(previous,p->pos,12);
            if(!asleep)gt4_course_object_integrate(p,dt);
            gt4_course_gadget_rotation(p->angles,p->rot);
            float points[8][3];for(unsigned k=0;k<count;k++)point_world(p,k,points[k]);
            if(!asleep){gt4_ray_hit hit;float at[3]={p->pos[0],p->pos[1],p->pos[2]};
                walls(p,mesh,previous,points);memcpy(at,p->pos,12);
                /* 003E29F8 queries the vertical ray through the current prop. */
                if(gt4_course_ray_down(mesh,&hit,at)){
                    p->ground_id=(int32_t)((uint16_t)hit.tri|(uint32_t)(uint16_t)hit.leaf<<16);
                    memcpy(p->normal,hit.normal,12);p->plane=hit.plane;
                    p->sun=(float)(int)fminf(255,fmaxf(0,hit.u*255))/255.f;
                    p->shade=(float)(int)fminf(255,fmaxf(0,hit.v*255))/255.f;
                    ground(p,points,count);
                }else continue;}
            if(p->age)continue;
            /* 003E3C48: expanded centre rejection, then all five planes
               against each authored support vertex; type 2/detail 9. */
            for(int i=0;i<cars;i++){
                uint32_t car=GT4_U32(hull[i]+0x450);
                if(!car || GT4_U8(car+0x545))continue;
                int inside=1;float reach=readf(d+0x90);
                for(int k=0;k<5;k++)if(dot(planes[i][k],p->pos)+planes[i][k][3]>reach){inside=0;break;}
                if(!inside)continue;
                for(unsigned k=0;k<count;k++){
                    inside=1;for(int n=0;n<5;n++)if(dot(planes[i][n],points[k])+planes[i][n][3]>0){inside=0;break;}
                    if(!inside)continue;
                    float impulse=gt4_course_object_contact(p,k,planes[i],position[i],velocity[i],GT4_F32(hull[i]+0x480));
                    p->impact+=impulse;memset(p->support,0,8);
                    gt4_car_impact(car,2,9,impulse);
                    if(d[2]&1){p->age=1;break;}
                }
                if(p->age)break;
            }
        }
    }
}
