/* PitmenData 003D4C38 / Pitman 003D3E80. Twenty original presentation
   states: approach, wheel service, withdrawal and reset; read-only cars. */
#include "race_pit_walk.h"
#include "race_pit_crew.h"
#include "physics/car.h"
#include <math.h>
#include <stddef.h>
#include <string.h>
#include <fenv.h>
_Static_assert(sizeof(gt4_pit_walker)==0x74,"Pitman record layout");
static float random_between(uint32_t *seed,float lo,float hi){
 /* 003CCB60 -> 00578598/005792F0, same presentation seed. */
 *seed=*seed*17u+17u;uint32_t v=*seed^((*seed<<16)|(*seed>>16));float r=0;
 if(v){unsigned shift=0,scan=v;while(!(scan&0x80000000u)){shift++;scan<<=1;}
  int count=(v&0x80000000u)?-1:(int)shift-1;unsigned exponent=(unsigned)(125-count);
  r=gt4_f32((exponent<<23)|((v<<((-exponent)&31u))>>9));}
 return (hi-lo)*r+lo;
}
static float facing(unsigned side){return gt4_f32(side&1?0xbfc90fd7:0x3fc90fd7);}
static void waiting(gt4_pit_walker *a,int variant,uint32_t *rng){
 /* 003D3830 / D_006A3180: four slots at the pit wall. */
 static const float z[4]={-1.5f,-.5f,1.5f,.5f};
 a->waiting[0]=variant?random_between(rng,-.25f,0)+2.75f:random_between(rng,0,.25f)-2.75f;
 a->waiting[1]=z[a->role]+random_between(rng,gt4_f32(0xbdcccccc),gt4_f32(0x3dcccccc));
}
static unsigned wheel_for(uint32_t car,unsigned role){
 static const unsigned maps[4][4]={{0,1,2,3},{0,0,1,1},{0,0,1,2},{0,1,2,2}};
 unsigned kind=GT4_U8(gt4_car_layout(car));return kind<4?maps[kind][role]:role;
}
static void anchor(uint32_t car,unsigned role,gt4_pit_walk_anchor *a){
 unsigned wheel=wheel_for(car,role);float p[4];gt4_pit_wheel_place(car,wheel,role&1,p);
 a->wheel=wheel;a->side=role&1;a->point[1]=p[2];
 /* 003D4C38 uses full hub X with width/2+1 clearance. */
 a->point[0]=p[0]+(role&1?1.f:-1.f);
}
void gt4_pit_walk_init(gt4_pit_walker a[4],uint32_t car,int variant,int stage,int spacing,uint32_t *rng){
 fenv_t saved;fegetenv(&saved);gt4_phys_fpu_enter();
 /* 003D4C38 +78..220: at most two actors, prefer the package side;
    layouts with fewer physical wheels reuse the authored role map. */
 unsigned enabled[4]={1,1,1,1},count=4;
 for(int i=3;i>=0 && count>=3;i--)if((i&1)==variant && enabled[i]){enabled[i]=0;count--;}
 for(int i=3;i>=0 && count>=3;i--)if(enabled[i]){enabled[i]=0;count--;}
 for(unsigned r=0;r<4;r++){
  memset(a+r,0,sizeof *a);gt4_pit_walker *p=a+r;p->role=r;p->target=1;p->enabled=enabled[r];
  unsigned other=(r&1)==variant?r:(r+1)%4;
  anchor(car,other,&p->anchor[0]);anchor(car,r,&p->anchor[1]);
  if(spacing>=2)p->anchor[1]=p->anchor[0];
  p->phase=random_between(rng,0,1);waiting(p,variant,rng);
  if(!stage){memcpy(p->position,p->waiting,8);p->yaw=facing((unsigned)variant)+random_between(rng,gt4_f32(0xbf20d978),gt4_f32(0x3f20d978));}
  else{p->state=stage<4?11:14;memcpy(p->position,p->anchor[1].point,8);p->yaw=facing(p->anchor[1].side);}
  p->target_yaw=p->yaw;
 }fesetenv(&saved);
}
static float drawn(uint32_t car,unsigned previous,unsigned current){float t=GT4_F32(car+0x554);return GT4_F32(car+previous)*t+GT4_F32(car+current)*(1-t);}
static void halt(gt4_pit_walker*a){a->velocity[0]=a->velocity[1]=a->timer=0;}
static int move(gt4_pit_walker*a,float dt,const float to[2],float speed,int *stuck){
 /* 003D3950: original damped walk, deadline, speed limit and phase.
    phase uses the pre-limit speed, exactly as 003D3CD8..003D3D64. */
 a->timer+=dt;*stuck=0;if(a->timer>=20)memcpy(a->position,to,8);else if(a->timer>=10)*stuck=1;
 float d[2]={to[0]-a->position[0],to[1]-a->position[1]};
 if(d[0]*d[0]+d[1]*d[1]<gt4_f32(0x3b23d708))return 1;
 for(unsigned i=0;i<2;i++){float force=((d[i]*25)*speed)*speed-(a->velocity[i]*10)*speed;a->velocity[i]+=force*dt;}
 float length2=a->velocity[0]*a->velocity[0]+a->velocity[1]*a->velocity[1],maximum=speed*1.25f;
 if(maximum*maximum<length2){float inverse=1.f/sqrtf(length2);for(unsigned i=0;i<2;i++)a->velocity[i]=(a->velocity[i]*inverse)*maximum;}
 for(unsigned i=0;i<2;i++)a->position[i]+=a->velocity[i]*dt;
 float phase=a->phase+(sqrtf(length2)/gt4_f32(0x3fafaacc))*dt;a->phase=phase-floorf(phase);return 0;
}
static void turn(gt4_pit_walker*a,const float target[2]){a->yaw=atan2f(target[0]-a->position[0],target[1]-a->position[1]);}
static float distance2(const gt4_pit_crew*c,const gt4_pit_walker*a){
 float x=drawn(c->car,0x8a4,0x888)-(a->position[0]+c->box[0]);
 float z=drawn(c->car,0x8ac,0x890)-(a->position[1]+c->box[2]);return x*x+z*z;
}
static void service_anchor(gt4_pit_walk_anchor*a,uint32_t car){
 float p[4];gt4_pit_wheel_place(car,a->wheel,a->side&1,p);a->point[0]=p[0]+(a->side&1?1.f:-1.f);a->point[1]=p[2];
}
static int push_from_car(const float bounds[4],float point[2]){
 /* 003D34D8: 0.25 clearance, nearest X edge, authored 0.01 margin. */
 float x=point[0],z=point[1],radius=.25f;
 if(x<=bounds[0]-radius || bounds[2]+radius<=x || z<=bounds[1]-radius || bounds[3]+radius<=z)return 0;
 point[0]=x-bounds[0]<bounds[2]-x?bounds[0]-radius-gt4_f32(0x3c23d70a):bounds[2]+radius+gt4_f32(0x3c23d70a);return 1;
}
static void avoid_cars(gt4_pit_walker*a,gt4_pit_crew*all,unsigned count,unsigned entry,int idle){
 /* 003D3E80 +886..AE8: transform into each live car frame, push
    out of the body rectangle, then return to this fixed box frame. */
 gt4_pit_crew*c=all+entry;float sn=sinf(c->box[3]),cs=cosf(c->box[3]);
 for(unsigned i=0;i<count;i++){
  uint32_t car=all[i].car;if(!car)continue;
  if(2.f<fabsf(drawn(car,0x8a8,0x88c)-c->box[1]))continue;
  float x=drawn(car,0x8a4,0x888)-(a->position[0]+c->box[0]),z=drawn(car,0x8ac,0x890)-(a->position[1]+c->box[2]);if(400.f<=x*x+z*z)continue;
  float yaw=drawn(car,0x8c4,0x674),si=sinf(yaw),co=cosf(yaw);
  float world[2]={(cs*a->position[0]+sn*a->position[1])+c->box[0],(-sn*a->position[0]+cs*a->position[1])+c->box[2]};
  float px=drawn(car,0x8a4,0x888),pz=drawn(car,0x8ac,0x890);
  float point[2]={(co*world[0]-si*world[1])+((-co)*px+si*pz),(si*world[0]+co*world[1])+((-si)*px-co*pz)};
  uint32_t params=GT4_U32(car+0x10);float width=GT4_F32(params+8);
  float bounds[4]={(-width)*.5f,-(GT4_F32(params)+GT4_F32(params+0x50)),width*.5f,GT4_F32(params+4)-GT4_F32(params+0x50)};
  if(!push_from_car(bounds,point))continue;
  world[0]=(co*point[0]+si*point[1])+px;world[1]=((-si)*point[0]+co*point[1])+pz;
  a->position[0]=(cs*world[0]-sn*world[1])+((-cs)*c->box[0]+sn*c->box[2]);
  a->position[1]=(sn*world[0]+cs*world[1])+((-sn)*c->box[0]-cs*c->box[2]);
  if(idle)a->state=16;
 }
}
void gt4_pit_walk_step(gt4_pit_crew*all,unsigned count,unsigned entry,int commands,float dt,uint32_t*rng){
 if(!all || !count || count>6 || entry>=count || !rng)return;fenv_t saved;fegetenv(&saved);gt4_phys_fpu_enter();
 gt4_pit_crew*c=all+entry;int stage=c->stage,outside=!stage || stage>=5,neighbor=c->walk_spacing && entry && all[entry-1].stage==1;
 for(unsigned r=0;r<4;r++){
  gt4_pit_walker*a=c->walking+r;if(!a->enabled)continue;int stuck=0,idle=0;gt4_pit_walk_anchor*p=a->anchor+a->target;
  switch(a->state){
  case 0:a->target=1;idle=1;if(stage>0){a->timer+=dt;if(commands<2)a->timer=0;else if(random_between(rng,0,.75f)<=a->timer){a->timer=0;a->state=neighbor?1:4;}}break;
  case 1:turn(a,a->anchor[0].point);a->state=2;break;
  case 2:if(!neighbor){halt(a);a->target=1;a->state=4;}else if(move(a,dt,a->anchor[0].point,1,&stuck)){halt(a);a->state=3;}else if(stuck){halt(a);a->state=15;}break;
  case 3:if(!neighbor){a->target=distance2(c,a)>gt4_f32(0x4640e6b6)?1:0;a->state=4;}break;
  case 4:turn(a,p->point);a->state=5;break;
  case 5:if(move(a,dt,p->point,1,&stuck)){halt(a);a->state=6;}else if(stuck){halt(a);a->state=15;}break;
  case 6:a->yaw=facing(p->side);a->state=7;break;
  case 7:if(outside)a->state=15;else if(stage>=2){service_anchor(p,c->car);a->state=8;}break;
  case 8:if(outside)a->state=15;else if(move(a,dt,p->point,1.5f,&stuck)){halt(a);a->state=9;}break;
  case 9:if(outside)a->state=15;else{a->phase+=dt;if(1.f<=a->phase){a->phase=1;a->state=10;}}break;
  case 10:if(outside)a->state=15;else if(stage>=3){a->timer+=dt;if(random_between(rng,0,.75f)<=a->timer){a->timer=0;a->model=1;a->phase=0;a->rate=random_between(rng,.75f,1);a->state=11;}}break;
  case 11:a->phase+=dt*a->rate;if(1.f<=a->phase){a->phase=1;if(outside){a->rate=0;a->state=12;}else if(stage>=4){a->rate=random_between(rng,0,1);a->state=12;}}break;
  case 12:a->timer+=dt;if(a->rate<=a->timer){a->phase-=dt;if(a->phase<=0){a->phase=0;a->state=13;}}break;
  case 13:if(!stage || (outside && distance2(c,a)>=400.f)){a->timer=0;a->rate=random_between(rng,2,5);a->state=14;}break;
  case 14:a->timer+=dt;if(neighbor || a->rate<=a->timer){a->timer=0;a->state=15;}break;
  case 15:a->model=0;a->phase=0;a->state=16;waiting(a,c->variant,rng);/* original falls through */
  case 16:turn(a,a->waiting);a->state=17;break;
  case 17:if(!a->target || distance2(c,a)>=400.f)a->state=18;break;
  case 18:if(move(a,dt,a->waiting,1.5f,&stuck)){halt(a);a->target_yaw=facing((unsigned)c->variant)+random_between(rng,gt4_f32(0xbf20d978),gt4_f32(0x3f20d978));a->state=19;}break;
  case 19:a->yaw=a->target_yaw;if(!stage)a->state=0;break;
  default:break;
  }
  avoid_cars(a,all,count,entry,idle);
 }fesetenv(&saved);
}
float gt4_pit_walk_light(const gt4_pit_walker*a){
 float dx=a->anchor[1].point[0]-a->position[0],dz=a->anchor[1].point[1]-a->position[1],one=dx*dx+dz*dz;
 dx=a->waiting[0]-a->position[0];dz=a->waiting[1]-a->position[1];float two=dx*dx+dz*dz;
 return (a->light[0]*one+a->light[2]*two)/(one+two);
}
int gt4_pit_walk_sample(float phase,float scale,unsigned*branch,float*tween){
 /* BC_WALK/BC_SQUAT model VM: time * authored scale, integer branch,
    remainder modulo 1. 004569F0 opcodes 82/8C/84. */
 if(!branch || !tween || !isfinite(phase) || phase<0 || phase>1 || !isfinite(scale) || scale<=0 || scale>4)return 0;
 fenv_t env;fegetenv(&env);gt4_phys_fpu_enter();float value=phase*scale;*branch=(unsigned)value;*tween=value-(float)*branch;fesetenv(&env);return 1;
}
