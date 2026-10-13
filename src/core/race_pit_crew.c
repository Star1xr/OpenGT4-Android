/* PitWorld 003D03E0 / 003CFA98. Native authored clocks and sound marks;
   car state is read-only, including the jack-down notification. */
#include "race_pit_crew.h"
#include "physics/car.h"
#include <string.h>
#include <math.h>
#include <fenv.h>
/* 00621C30..44, used by PitWorld: LF, RF, LR, RR, fuel, lollipop. */
static const char *const motions[6]={"tiremanLF_1seq","tiremanRF_1seq","tiremanLR_1seq","tiremanRR_1seq","fuelman_1seq","lollipopman_1seq"};
int gt4_pit_stage_fields(unsigned lane,unsigned flags,unsigned orders,unsigned timer)
{
    /* 0035BBD8 +04..90: stage, independent of service calculations. */
    if(!lane || !(flags&0x10))return 0;
    if(lane==2)return 1;if(lane==4)return 5;if(lane!=3)return 0;
    if(!timer)return orders?1:5;if(timer==0x2d1)return 1;
    if(timer>=0x285)return 2;if(timer>=0x3c)return 3;return 4;
}
int gt4_pit_monitor_input(int pit_sequence,int transition){return !pit_sequence && !transition;}
int gt4_pit_stage(uint32_t car)
{
    /* 003C9F38 -> 00343C20 / 0035AE58: display only in the pit interval. */
    if(!car || gt4_car_race_over(car)==1)return 0;
    unsigned lane=GT4_U8(car+0x56b);if(lane<2 || lane>4)return 0;
    uint32_t cond=GT4_U32(car+4),mesh=GT4_U32(GT4_U32(GT4_U32(cond+0xcbd8)+4)+0xdc);
    float length=GT4_F32(mesh+0x14),distance=GT4_F32(car+0x5a8);
    if(distance<length*.5f)distance+=length;
    if(distance<gt4_line_point_distance(cond+0xda50,1) || gt4_line_point_distance(cond+0xda50,-1)<distance)return 0;
    return gt4_pit_stage_fields(lane,GT4_U8(car+0x5b5),GT4_U16(car+0x5bc),GT4_U16(car+0x5be));
}
int gt4_pit_refueling(unsigned orders,unsigned timer)
{
    /* 0035BEB0 +14..20: subtraction wraps at 32 bits, not 16 bits. */
    return (orders&0x100) && (uint32_t)(timer-0x5a)<0x22c;
}
void gt4_pit_refuel_meter(float fuel,float capacity,gt4_pit_refuel_layout *o)
{
    /* RaceDisplay+2D78 in the user's slot 5: (320,58), align (2,2).
       003A59D8 +70..178: 256x26 background, 155-unit bar, 157x12 scale;
       +1D0..228: hn_20R, 805EA8F2, integer fuel/capacity label. */
    const float x=320.f-256.f*.5f,y=58.f-26.f*.5f;
    float fraction=fminf(fmaxf(fuel/capacity,0.f),1.f);
    o->rect[0][0]=x;o->rect[0][1]=y;o->rect[0][2]=256;o->rect[0][3]=26;
    o->rect[1][0]=x+15;o->rect[1][1]=y;o->rect[1][2]=fraction*155;o->rect[1][3]=26;
    o->rect[2][0]=x+14;o->rect[2][1]=y+7;o->rect[2][2]=157;o->rect[2][3]=12;
    o->text[0]=x+14+227;o->text[1]=y+7+12-1;
    o->color=0x805ea8f2;o->fuel=(int)(fuel+.5f);o->capacity=(int)(capacity+.5f);
}
int gt4_pit_role_control(unsigned role,int stage,int refueling,int ready)
{
    /* 003CF6F0, 003CF730, 003CF780. */
    if(!stage)return -1;
    if(role<4){if(stage<2)return 0;if(stage<3)return 1;return stage<6?2:-1;}
    if(role==4){if(stage<3)return 0;if(stage==3)return refueling==1?1:ready?2:0;return stage<6?2:-1;}
    if(role==5){if(stage<2)return 0;if(ready && stage<5)return 2;if(stage<4)return 1;return stage<6?3:-1;}
    return -1;
}
void gt4_pit_crew_init(gt4_pit_crew *c,gt4_license_motion *motion)
{
    /* Preserve the immutable car-specific placement across service resets. */
    c->motion=motion;c->stage=0;memset(c->active,0,sizeof c->active);memset(c->requested,0,sizeof c->requested);
    memset(c->tire_end,0,sizeof c->tire_end);c->refuel_seen=c->refuel_flag=c->jack_seen=c->nozzle_flag=0;
    c->fixed=c->frozen=0;memset(c->cable,0,sizeof c->cable);
    /* 003D0DF0 +64..170 returns the service root to the box line on
       reset; the next stop must not reuse the preceding car translation. */
    if(c->placed)memcpy(c->root,c->box,16);
    c->camera_control=-1;c->camera_begin=-1;c->camera_choice=0;c->camera_atomic=-1;
    for(unsigned role=0;role<6;role++)gt4_license_motion_select(motion,role,motions[role]);
}
void gt4_pit_crew_service_enable(gt4_pit_crew*c,int tires,int fuel){
 /* 003D3208/3228/3248 set +174, preserving each layout's static mask. */
 if(!c)return;if(!tires)for(unsigned i=0;i<4;i++)c->disabled[i]=1;if(!fuel)c->disabled[4]=1;
}
void gt4_pit_owned_tires(const uint8_t body[0x4b8],unsigned masks[2]){
 /* 0043F270: category 19h
    selects 0043F338 (base 40h), 1Ah selects 0043F340 (base 4Dh).
    0043F4A8 reads object+494; serialized body omits its 8-byte header. */
 for(unsigned a=0;a<2;a++){masks[a]=0;if(body)for(unsigned c=0;c<13;c++){unsigned bit=0x40+a*13+c;masks[a]|=((body[0x48c+bit/8]>>(bit&7))&1u)<<c;}}
}
static unsigned wheel_index(uint32_t car,unsigned role){
 unsigned kind=GT4_U8(gt4_car_layout(car));
 /* 003CD730 maps four visible service roles to actual wheels. */
 static const unsigned map[4][4]={{0,1,2,3},{0,0,1,1},{0,0,1,2},{0,1,2,2}};
 return kind<4?map[kind][role]:role;
}
void gt4_pit_wheel_place(uint32_t car,unsigned wheel,int right,float p[4]){
 uint32_t layout=gt4_car_layout(car),params=GT4_U32(car+0x10);
 float x=0,width=0,z=0,t=GT4_F32(car+0x554);
 /* 00344AA8 / 00344CE8 / 00344C08, f12=0, exact product order. */
 if(wheel<GT4_U8(layout+1)){
  x=GT4_F32(car+0x900+wheel*4)*t+GT4_F32(car+0x16c+wheel*0xec)*(1.f-t);
  width=GT4_F32(params+0x2d4+GT4_U8(layout+0x14+wheel)*4);
  int side=(int8_t)GT4_U8(layout+0x3c+wheel);z=GT4_F32(params+0x18)*.5f;
  if(side>0)z=-z;else if(!side)z=0;
 }
 p[0]=right?x+width*.5f:x-width*.5f;p[1]=0;p[2]=z;p[3]=gt4_f32(right?0x4096cac0:0x3fc90e56);
}
static float pit_random(uint32_t *seed,float lo,float hi){
 /* 003CCB60 -> 00578598 / 005792F0. Separate presentation RNG. */
 *seed=*seed*17u+17u;uint32_t value=*seed^((*seed<<16)|(*seed>>16));float random=0;
 if(value){unsigned shift=0,scan=value;while(!(scan&0x80000000u)){shift++;scan<<=1;}
  int count=(value&0x80000000u)?-1:(int)shift-1;unsigned exponent=(unsigned)(125-count);
  random=gt4_f32((exponent<<23)|((value<<((-exponent)&31u))>>9));}
 return (hi-lo)*random+lo;
}
void gt4_pit_crew_place(gt4_pit_crew*c,uint32_t car,unsigned entry,const gt4_driver_car*g,int variant,uint32_t *rng){
 if(!c || !car || !g || !rng)return;fenv_t env;fegetenv(&env);gt4_phys_fpu_enter();
 uint32_t params=GT4_U32(car+0x10);
 c->car=car;
 c->fuel_height=g->fuel_geometry?g->fuel_port[1]:0;
 if(!c->placed){
  /* 0046BEC0: authored box line point 8-entry, converted x,z,-y.
     Read the line directly; do not allocate scratch space in physics RAM. */
  uint32_t lines=GT4_U32(car+4)+0xda50,table=GT4_U32(lines+8);
  if(!GT4_U16(lines+0xc) || !table || entry>8){fesetenv(&env);return;}
  uint32_t line=GT4_U32(table+16);int wide=GT4_U32(GT4_U32(GT4_U32(lines)+0x9c))==0x4e4c4441;
  uint32_t point=line+(wide?0x40:4)+(8-entry)*(wide?0x40:0x28);
  c->root[0]=GT4_F32(point+4);c->root[1]=GT4_F32(point+12);c->root[2]=-GT4_F32(point+8);c->root[3]=GT4_F32(point+0x24);
  for(unsigned i=0;i<4;i++)c->approach[i]=pit_random(rng,gt4_f32(i<2?0x3dcccccc:0x3f199999),gt4_f32(i<2?0x3e800000:0x3f333333));
  c->approach[4]=pit_random(rng,gt4_f32(0x3f8ccccc),gt4_f32(0x3f999999));c->variant=variant;c->placed=1;
  memcpy(c->box,c->root,16);memcpy(c->seat,g->seat,12);
  /* 003CD188 +1EC..26C captures wheel anchors once, independently of
     camera cuts. CC010 only refreshes origin/heading for a new take. */
  float t=GT4_F32(car+0x554);uint32_t layout=gt4_car_layout(car);
  for(unsigned w=0;w<GT4_U8(layout+1) && w<4;w++){float edge[4];gt4_pit_wheel_place(car,w,0,edge);unsigned axle=GT4_U8(layout+0x14+w);
   c->camera_hub[w][0]=GT4_F32(car+0x900+w*4)*t+GT4_F32(car+0x16c+w*0xec)*(1-t);
   c->camera_hub[w][1]=GT4_F32(params+0x314+axle*0x58)-(GT4_F32(car+0x8d0+w*4)*t+GT4_F32(car+0x1f8+w*0xec)*(1-t));c->camera_hub[w][2]=edge[2];}
 }
 /* 003D0DF0 phase 3 -> 4 captures the service roots once. Suspension
    continues to move during jacking; the mechanics must not follow it. */
 if(!c->fixed){for(unsigned i=0;i<4;i++)gt4_pit_wheel_place(car,wheel_index(car,i),i&1,c->place[i]);}
 if(c->stage<2){
  /* 003CDB20 aligns each side, then authored approach clearance. */
  float left=fminf(c->place[0][0],c->place[2][0]),right=fmaxf(c->place[1][0],c->place[3][0]);
  for(unsigned i=0;i<4;i++){float gap=c->approach[i];if(g->flags&0x10){if(variant==1 && !(i&1))gap=1;else if(variant!=1 && (i&1))gap=1;}
   c->place[i][0]=(i&1)?right+gap:left-gap;}
 }
 /* 003CE288 fuel-side classification, first equal maximum wins. */
 int fuelmode=0;float *fuel=c->place[4],width=GT4_F32(params+8),port=g->fuel_port[2];
 if(g->fuel_geometry){float x=g->fuel_normal[0],z=g->fuel_normal[2],big=fmaxf(fmaxf(-x,-z),fmaxf(x,z));fuelmode=big==-x?0:big==-z?1:big==x?2:3;}
 if(!c->fixed && !c->stage){
  /* 003CD188: authored camera sequence depends on nozzle side and end. */
  /* 003CE288 +9C..E0 / 003CD188 +80..8C: bit 1 overrides bit 0. */
  const char*name=(!g->fuel_geometry || (g->flags&2))?"pitcam_fuel_unknown_seq":(g->flags&1)?"pitcam_tire_unknown_seq":fuelmode==0?(port<0?"pitcam_fuel_FR_seq":"pitcam_fuel_RR_seq"):fuelmode==1?"pitcam_fuel_R_seq":fuelmode==2?(port<0?"pitcam_fuel_FL_seq":"pitcam_fuel_RL_seq"):"pitcam_fuel_F_seq";
  if(!gt4_license_motion_select(c->motion,6,name))gt4_license_motion_select(c->motion,6,"pitcam_tire_unknown_seq");
 }
 if(!c->fixed){
 if(fuelmode==0 || fuelmode==2){
  /* 003CE4A0 +570..720 / +760..90c, clearance from both tire roles. */
  unsigned front=fuelmode?0:1,rear=fuelmode?2:3;float a=c->place[front][2],b=c->place[rear][2],z=port;
  float margin=gt4_f32(0x3ee66666),offset=gt4_f32(0x3effffff);
  if(fabsf(z-a)<margin)z=a<z?a+offset:a-offset;
  if(fabsf(z-b)<margin)z=b<z?b+offset:b-offset;
  if(fabsf(z-a)<margin)z=(a+b)*.5f;
  float angle=atan2f(z-port,gt4_f32(0x3dcccccc));angle=fmaxf(gt4_f32(0xbf06098e),fminf(gt4_f32(0x3f06098e),angle));
  fuel[0]=fuelmode?(-width)*.5f-gt4_f32(0x3dcccccc):width*.5f+gt4_f32(0x3dcccccc);fuel[1]=0;fuel[2]=z;
  fuel[3]=fuelmode?angle+gt4_f32(0x3fc90e56):gt4_f32(0x4096cac0)-angle;
  if(c->stage<2)fuel[0]+=fuelmode?-(gt4_f32(0x3dcccccc)+c->approach[4]):gt4_f32(0x3dcccccc)+c->approach[4];
 }else if(fuelmode==1){fuel[0]=g->fuel_port[0];fuel[1]=0;fuel[2]=(GT4_F32(params+4)-GT4_F32(params+0x50))+gt4_f32(0x3dcccccc);fuel[3]=gt4_f32(0x40490e56);
  if(c->stage<2){unsigned wheel=wheel_index(car,3);float p[4];gt4_pit_wheel_place(car,wheel,1,p);fuel[0]=p[0]-GT4_F32(params+0x2d4+GT4_U8(gt4_car_layout(car)+0x14+wheel)*4)*.5f+gt4_f32(0x3dcccccc)+c->approach[4];fuel[2]=p[2]+.75f;fuel[3]=gt4_f32(0x4096cac0);}
 }else{fuel[0]=g->fuel_port[0];fuel[1]=0;fuel[2]=gt4_f32(0xbdcccccc)-(GT4_F32(params)+GT4_F32(params+0x50))-c->approach[4];fuel[3]=0;}
 /* 003CEB90 +3CEDB4..3CEE20: lollipop outside half width, ahead of nose. */
 c->place[5][0]=fmaxf(width*.5f,gt4_f32(0x3fda9fbe))*(variant?1.f:-1.f);
 c->place[5][1]=0;c->place[5][2]=-1.f-(GT4_F32(params)+GT4_F32(params+0x50));c->place[5][3]=0;
 }
 if(c->stage>=2 && c->stage<4){float t=GT4_F32(car+0x554);c->root[0]=GT4_F32(car+0x8a4)*t+GT4_F32(car+0x888)*(1.f-t);c->root[2]=GT4_F32(car+0x8ac)*t+GT4_F32(car+0x890)*(1.f-t);}
 if(!c->fixed){
  c->fixed=c->stage>=2;
  for(unsigned role=0;role<4;role++){
   unsigned wheel=wheel_index(car,role),layout=gt4_car_layout(car),axle=GT4_U8(layout+0x14+wheel);
   float radius=wheel<GT4_U8(layout+1)?GT4_F32(params+0x2c4+axle*4):0;
   /* 003CE178 -> 003CC980: radius determines the authored small/large blend. */
   c->blend[role]=fminf(1,fmaxf(0,1-((radius+radius)-gt4_f32(0x3ee978d4))/gt4_f32(0x3ebdf3b6)));
   const char *label="tire1";float matrix[1][16],old=gt4_license_motion_time(c->motion,role);gt4_license_motion_set_time(c->motion,role,0);
   float z=gt4_license_motion_frames_blend(c->motion,role,c->blend[role],&label,1,matrix)==1?matrix[0][14]:0;
   float width=wheel<GT4_U8(layout+1)?GT4_F32(params+0x2d4+axle*4):0;
   /* 003CD920: adapt the authored replacement tire reach to its width. */
   c->tire_scale[role]=z?(gt4_f32(0x3e849ba5)-width+z)/z:1;
   gt4_license_motion_set_time(c->motion,role,old);
   static const uint32_t offsets[2][4]={{0x3f377777,0x3fa22222,0x3ecccccc,0x3ef77777},{0x3ff33333,0x401bbbbb,0x400aaaaa,0x40166666}};
   for(unsigned k=0;k<2;k++){float begin,end,*fade=c->fade[role]+k*3;
    if(gt4_license_motion_label_interval(c->motion,role,k?"fix":"release",&begin,&end)){fade[0]=begin+gt4_f32(offsets[k][role]);fade[1]=fade[0]+gt4_f32(0x3e2aaaaa);fade[2]=1/(fade[1]-fade[0]);}
    else{fade[0]=fade[1]=-1;fade[2]=1;}}
   float t=GT4_F32(car+0x554);c->camber[role]=GT4_F32(car+0x910+wheel*4)*t+GT4_F32(car+0x188+wheel*0xec)*(1-t);
  }
 }
 if(c->stage<4){
  float t=GT4_F32(car+0x554);for(unsigned j=0;j<3;j++){
   c->old_root[j]=GT4_F32(car+0x8a4+j*4)*t+GT4_F32(car+0x888+j*4)*(1-t);
   c->old_euler[j]=GT4_F32(car+0x8bc+j*4)*t+GT4_F32(car+0x66c+j*4)*(1-t);
  }
  for(unsigned r=0;r<4;r++){float p[4];gt4_pit_wheel_place(car,wheel_index(car,r),r&1,p);unsigned w=wheel_index(car,r),axle=GT4_U8(gt4_car_layout(car)+0x14+w);
   /* 003D0DF0 calls the bounded 00344AA8/00344B40/00344C08
      getters, which return zero for absent wheels (layout 8). */
   if(w>=GT4_U8(gt4_car_layout(car)+1)){memset(c->old_hub[r],0,12);continue;}
   c->old_hub[r][0]=GT4_F32(car+0x900+w*4)*t+GT4_F32(car+0x16c+w*0xec)*(1-t);
   c->old_hub[r][1]=GT4_F32(params+0x314+axle*0x58)-(GT4_F32(car+0x8d0+w*4)*t+GT4_F32(car+0x1f8+w*0xec)*(1-t));
   c->old_hub[r][2]=p[2];
  }
 }
 if(c->stage>=4 && !c->frozen){c->frozen=1;float t=GT4_F32(car+0x554);for(unsigned j=0;j<3;j++)c->old_euler[j]=GT4_F32(car+0x8bc+j*4)*t+GT4_F32(car+0x66c+j*4)*(1-t);}
 fesetenv(&env);
}
static void shot(gt4_pit_crew_sound *out,unsigned id){if(out->count<sizeof out->effect)out->effect[out->count++]=(uint8_t)id;}
void gt4_pit_crew_step(gt4_pit_crew *c,uint32_t car,float dt,gt4_pit_crew_sound *out)
{
    memset(out,0,sizeof *out);if(!c || !c->motion || !car)return;
    fenv_t saved;fegetenv(&saved);gt4_phys_fpu_enter();
    int stage=gt4_pit_stage(car);unsigned orders=GT4_U16(car+0x5bc),timer=GT4_U16(car+0x5be);
    if(!stage){if(c->stage)gt4_pit_crew_init(c,c->motion);fesetenv(&saved);return;}
    if(!c->stage){gt4_pit_crew_init(c,c->motion);for(unsigned i=0;i<6;i++)c->requested[i]=1;}
    /* 003D3040 copies pending +16C to visible/clock +170 before dispatch. */
    memcpy(c->active,c->requested,sizeof c->active);
    /* 003CFA98 +44..118 / 0035BD10: requested services at approach.
       Disabled roles keep no event/pose work; wheel-count mask 003CD730. */
    if(stage==1 && c->stage){
        int front=0,rear=0,fuel=0;
        if(orders&0x11){front=rear=(orders&6)!=0;fuel=(orders&10)!=0;}
        else {front=(orders&0x40)!=0;rear=(orders&0x80)!=0;fuel=(orders&0x100)!=0;}
        if(GT4_U8(car+0x56b)==4){front=rear=fuel=0;c->requested[5]=0;}
        if(GT4_U8(car+0x56b)==2){c->requested[0]=c->requested[1]=front;c->requested[2]=c->requested[3]=rear;c->requested[4]=fuel;}
    }
    /* 003CD730 stores persistent +174, independent of service phase. */
    unsigned layout=GT4_U8(gt4_car_layout(car));
    /* Layout 1 writes RR +168, not +174 (003CD730 +B0..B8);
       preserve that original distinction instead of assuming symmetry. */
    if(layout==1)c->active[1]=0;
    else if(layout==2)c->active[1]=0;else if(layout==3)c->active[3]=0;
    for(unsigned i=0;i<6;i++)if(c->disabled[i])c->active[i]=0;
    int refuel=gt4_pit_refueling(orders,timer),ready=1;
    /* 003CF868 precedes 003D29B0 within the role loop. */
    for(unsigned i=0;i<4;i++){if(c->active[i] && gt4_license_motion_passed(c->motion,i,"tire_end"))c->tire_end[i]=1;if(c->active[i] && !c->tire_end[i])ready=0;}
    for(unsigned role=0;role<6;role++){
        int control=gt4_pit_role_control(role,stage,refuel,role==4?c->refuel_seen:ready);
        /* 003CFDB4..3CFDE4 records refuel history before visibility
           gating; a disabled nozzle still observes the service state. */
        if(role==4 && refuel)c->refuel_seen=1;
        if(!c->active[role])continue;
        /* 003CF868 checks sequence history before advancing. */
        if(role<4 && gt4_license_motion_passed(c->motion,role,"tire_end"))c->tire_end[role]=1;
        if(stage!=c->stage && stage!=c->stage+1)gt4_license_motion_seek_control(c->motion,role,control);
        float old=gt4_license_motion_time(c->motion,role);
        /* 003CFF88..003D0074: suppress live steering/spin while a tire
           is carried, fade it in/out only at authored release/fix windows. */
        if(role<4){float *f=c->fade[role];c->rotation[role][0]=old<f[0]?1:old<f[1]?(f[1]-old)*f[2]:0;
            c->rotation[role][1]=old<f[3]?0:old<f[4]?1-(f[4]-old)*f[5]:1;}
        int advance=1;
        if(role==4){
            /* 003CFE80..003CFF84 holds the nozzle at the authored refuel
               interval; withdrawal advances by twice the remaining time.
               The car-height getter 003447F8 is observed, never written. */
            const char*label=NULL;float begin,end;
            out->refueling=0;
            if(gt4_license_motion_interval(c->motion,role,&label,&begin,&end) && label && !strcmp(label,"refuel")){
                if(control<2){float t=GT4_F32(car+0x554);
                    float height=GT4_F32(car+0x8a8)*t+GT4_F32(car+0x88c)*(1.f-t);
                    float ratio=((height+c->fuel_height)-gt4_f32(0x3f1cac08))/gt4_f32(0x3e947ae0);
                    /* 003CFF30 uses f28 = 3C23D70A, not zero. */
                    ratio=fminf(gt4_f32(0x3f7d70a3),fmaxf(ratio,gt4_f32(0x3c23d70a)));
                    if(begin+(end-begin)*ratio<old){advance=0;out->refueling=1;}
                }else gt4_license_motion_set_time(c->motion,role,(old+2*(end-old))-dt);
            }
            int nozzle=control==1;if(nozzle!=c->nozzle_flag)shot(out,0x45);c->nozzle_flag=nozzle;c->refuel_flag=out->refueling;
        }
        if(advance)gt4_license_motion_step_control(c->motion,role,dt,control);
        float now=gt4_license_motion_time(c->motion,role);
        if(role<4){
            float ax=0,az=0;gt4_license_motion_scalar(c->motion,role,c->blend[role],"cable_accel_x",&ax);gt4_license_motion_scalar(c->motion,role,c->blend[role],"cable_accel_z",&az);
            if(fabsf(ax)<=gt4_f32(0x3c23d70a))ax=0;if(fabsf(az)<=gt4_f32(0x3c23d70a))az=0;
            gt4_pit_cable_step(c->cable[role],dt,ax,az);
            static const char *const labels[4]={"wheel_gun","wheel_nut","tire_sound2","jack_up"};
            static const uint8_t ids[4]={0x43,0x3f,0x40,0x42};
            for(unsigned k=0;k<4;k++)if(gt4_license_motion_mark(c->motion,role,labels[k],old,now,dt)>0)shot(out,ids[k]);
        }
    }
    /* 0035BE38 consumes 0x200. Observe its edge locally instead of
       modifying the physics object's pending notification. */
    int jack=(orders&0x200)!=0;if(jack && !c->jack_seen)shot(out,0x41);c->jack_seen=jack;
    c->stage=stage;fesetenv(&saved);
}
void gt4_pit_cable_step(float s[4],float dt,float ax,float az){
 /* 003CF938 +00..158: semi-implicit spring, saturation clears velocity. */
 float damping=gt4_f32(0x4025c28f);
 s[1]+=((ax+(-s[0])*10)-s[1]*damping)*dt;
 s[3]+=((az+(-s[2])*10)-s[3]*damping)*dt;
 s[0]+=s[1]*dt;s[2]+=s[3]*dt;
 for(unsigned i=0;i<4;i+=2){if(s[i]>=1){s[i]=1;s[i+1]=0;}else if(s[i]<=-1){s[i]=-1;s[i+1]=0;}}
}
static void pit_identity(float m[16]){memset(m,0,64);m[0]=m[5]=m[10]=m[15]=1;}
int gt4_pit_tire_visible(int stage,int replacement){
 /* 003CF7C8 +18..44. The car callback draws its mounted tire separately. */
 return stage && (replacement==0?stage>=3:replacement==1?stage<4:0);
}
int gt4_pit_actor_model(const gt4_pit_crew*c,unsigned role,unsigned cars){
 static const char*const labels[]={"LodTireManFL","LodTireManFR","LodTireManRL","LodTireManRR","LodFuelMan","LodLollipopMan"};
 /* 003CC498 +98..12c, D_00621C48: the camera stream supplies these
    discrete model channels. A single-car session always uses the skin,
    except state 2, which delegates to the distant walking crew. */
 float v;if(!c || role>=6 || c->camera_control<0 || !gt4_license_motion_scalar(c->motion,6,(float)c->camera_choice,labels[role],&v))return -1;
 int model=(int)(v+.5f);return model!=2 && cars==1?-1:model;
}
const char*gt4_pit_actor_model_name(int model){
 /* 003C9FC8 / D_006A2EC0. */
 switch(model){case 3:return "BAKE_FUEL";case 4:return "BAKE_LOLI";case 5:case 10:case 11:return "BAKE_GUN";case 6:case 12:case 13:return "BAKE_GUN2";default:return NULL;}
}
float gt4_pit_actor_model_heading(int model){
 /* 003CA038 +48..80: authored +/-60 degrees in radians. */
 return model==10 || model==12?gt4_f32(0x3f86098e):model==11 || model==13?gt4_f32(0xbf86098e):0;
}
void gt4_pit_actor_cable_end(int model,float p[3]){
 /* 003CA0C8 / D_006A2EF0: rigid gun's cable endpoint. */
 static const uint32_t points[2][3]={{0xbecccccc,0x3f418937,0xbf9851eb},{0xbe71a9fb,0x3f570a3d,0xbf9dd2f1}};
 int at=model==5 || model==10 || model==11?0:model==6 || model==12 || model==13?1:-1;
 for(unsigned j=0;j<3;j++)p[j]=at<0?0:gt4_f32(points[at][j]);
}
static void pit_product(float m[16],const float n[16]){
 float out[16];for(unsigned c=0;c<4;c++)for(unsigned r=0;r<4;r++)out[c*4+r]=((m[r]*n[c*4]+m[4+r]*n[c*4+1])+m[8+r]*n[c*4+2])+m[12+r]*n[c*4+3];memcpy(m,out,64);
}
static void pit_translate(float m[16],float x,float y,float z){float n[16];pit_identity(n);n[12]=x;n[13]=y;n[14]=z;pit_product(m,n);}
static void pit_rotate(float m[16],unsigned axis,float degrees){
 /* 004A79B0/7988/79D8 degree API. Trig is the PC graphics boundary. */
 float rad=(degrees*gt4_f32(0x40490fda))/180,s=sinf(rad),c=cosf(rad),n[16];pit_identity(n);
 unsigned a=(axis+1)%3,b=(axis+2)%3;n[a*4+a]=n[b*4+b]=c;n[a*4+b]=s;n[b*4+a]=-s;pit_product(m,n);
}
int gt4_pit_tire_matrix(const gt4_pit_crew*c,unsigned role,int replacement,const float marker[16],float m[16]){
 if(!c || role>=4 || !marker || !m || !c->car)return 0;
 uint32_t car=c->car,params=GT4_U32(car+0x10),layout=gt4_car_layout(car);unsigned w=wheel_index(car,role);
 float t=GT4_F32(car+0x554),p[3],e[3],hub[3],local[16];
 for(unsigned j=0;j<3;j++){p[j]=GT4_F32(car+0x8a4+j*4)*t+GT4_F32(car+0x888+j*4)*(1-t);e[j]=GT4_F32(car+0x8bc+j*4)*t+GT4_F32(car+0x66c+j*4)*(1-t);}
 float edge[4];gt4_pit_wheel_place(car,w,role&1,edge);unsigned axle=GT4_U8(layout+0x14+w);
 int valid=w<GT4_U8(layout+1);
 float width=valid?GT4_F32(params+0x2d4+axle*4):0;
 hub[0]=valid?GT4_F32(car+0x900+w*4)*t+GT4_F32(car+0x16c+w*0xec)*(1-t):0;
 hub[1]=valid?GT4_F32(params+0x314+axle*0x58)-(GT4_F32(car+0x8d0+w*4)*t+GT4_F32(car+0x1f8+w*0xec)*(1-t)):0;hub[2]=edge[2];
 pit_identity(m);
 float pi=gt4_f32(0x4048f5c2);
 if(!replacement || c->stage>=3){
  if(!replacement && c->stage>=4){memcpy(p,c->old_root,12);memcpy(e,c->old_euler,12);memcpy(hub,c->old_hub[role],12);}
  pit_translate(m,p[0],p[1],p[2]);pit_rotate(m,1,((replacement?c->root[3]:e[2])*180)/pi);pit_rotate(m,2,(e[0]*180)/pi);pit_rotate(m,0,((-e[1])*180)/pi);
  pit_translate(m,hub[0],hub[1]-gt4_f32(0x3eeb851d),hub[2]);
 }else{
  if(c->stage==2){p[1]=c->root[1];}else memcpy(p,c->root,12);
  pit_translate(m,p[0],p[1],p[2]);pit_rotate(m,1,(c->root[3]*180)/pi);pit_translate(m,hub[0],0,hub[2]);
 }
 pit_rotate(m,1,role&1?-90:90);pit_translate(m,0,0,width*.5f);
 memcpy(local,marker,64);if(replacement)local[14]*=c->tire_scale[role];pit_product(m,local);pit_translate(m,0,0,(-width)*.5f);pit_rotate(m,1,-90);
 return 1;
}
int gt4_pit_car_matrix(const gt4_pit_crew*c,float m[16]){
 if(!c || !c->car || !m)return 0;
 uint32_t car=c->car;float t=GT4_F32(car+0x554),p[3],e[3];
 for(unsigned j=0;j<3;j++){p[j]=GT4_F32(car+0x8a4+j*4)*t+GT4_F32(car+0x888+j*4)*(1-t);e[j]=GT4_F32(car+0x8bc+j*4)*t+GT4_F32(car+0x66c+j*4)*(1-t);}
 /* 003D6F40 +2C..34 uses literal 42652EE0 for radians to degrees. */
 float degrees=gt4_f32(0x42652ee0);pit_identity(m);pit_translate(m,p[0],p[1],p[2]);
 pit_rotate(m,1,e[2]*degrees);pit_rotate(m,2,e[0]*degrees);pit_rotate(m,0,-e[1]*degrees);return 1;
}
int gt4_pit_mounted_matrix(const gt4_pit_crew*c,unsigned role,float m[16]){
 if(!c || role>=4 || !gt4_pit_car_matrix(c,m))return 0;
 /* 003D72A0 -> 00344AA8 / 00344B40 / 00344C08: installed centre.
    CarModel's parent uses 42652EE0, while the carried-wheel path
    003D1488 converts with 180/3.14; preserve those separate originals. */
 uint32_t car=c->car,params=GT4_U32(car+0x10),layout=gt4_car_layout(car);unsigned w=wheel_index(car,role);
 if(w>=GT4_U8(layout+1))return 1;
 float t=GT4_F32(car+0x554),edge[4];gt4_pit_wheel_place(car,w,role&1,edge);unsigned axle=GT4_U8(layout+0x14+w);
 float x=GT4_F32(car+0x900+w*4)*t+GT4_F32(car+0x16c+w*0xec)*(1-t);
 float y=GT4_F32(params+0x314+axle*0x58)-(GT4_F32(car+0x8d0+w*4)*t+GT4_F32(car+0x1f8+w*0xec)*(1-t));
 pit_translate(m,x,y,edge[2]);return 1;
}
static void tire_turns(uint32_t car,unsigned w,float weight,float camber,float m[16]){
 float t=GT4_F32(car+0x554);
 float pi=gt4_f32(0x4048f5c2);
 /* 003D2C48, 003D2F20..003D2FC8: live rotation fade, frozen camber. */
 if(weight>0){float steer=GT4_F32(car+0x8e0+w*4)*t+GT4_F32(car+0x164+w*0xec)*(1-t),spin=GT4_F32(car+0x8f0+w*4)*t+GT4_F32(car+0x168+w*0xec)*(1-t);
  pit_rotate(m,1,((weight*steer)*180)/pi);pit_rotate(m,0,((weight*spin)*180)/pi);}
 if(GT4_U8(gt4_car_layout(car)+0x1c+w)!=1)camber=-camber;pit_rotate(m,2,(camber*180)/pi);
}
void gt4_pit_tire_rotation(const gt4_pit_crew*c,unsigned role,int replacement,float m[16]){
 if(c && role<4 && c->car)tire_turns(c->car,wheel_index(c->car,role),c->rotation[role][replacement!=0],c->camber[role],m);
}
int gt4_pit_tire_draw_matrix(const gt4_pit_crew*c,unsigned role,int replacement,int model,const float marker[16],float m[16]){
 if(!c || role>=4 || !marker || !m || !c->car)return 0;
 /* 003D2C48 +10C..178 replaces the marker by (0,.46,0) for
    unavailable actors or distant camera models. In this branch the
    rotations remain live, instead of the release/fix fade and camber. */
 int fallback=!c->active[role] || model>=2;
 float fixed[16];if(fallback){pit_identity(fixed);fixed[13]=gt4_f32(0x3eeb851d);marker=fixed;}
 if(!gt4_pit_tire_matrix(c,role,replacement,marker,m))return 0;
 if(fallback){
  unsigned wheel=wheel_index(c->car,role);float t=GT4_F32(c->car+0x554);
  float camber=GT4_F32(c->car+0x910+wheel*4)*t+GT4_F32(c->car+0x188+wheel*0xec)*(1-t);
  tire_turns(c->car,wheel,1,camber,m);
 }else gt4_pit_tire_rotation(c,role,replacement,m);
 return 1;
}
static void pit_car_position(uint32_t car,float p[3]){float t=GT4_F32(car+0x554);for(unsigned i=0;i<3;i++)p[i]=GT4_F32(car+0x8a4+i*4)*t+GT4_F32(car+0x888+i*4)*(1-t);}
void gt4_pit_camera_step(gt4_pit_crew*c,float dt){
 if(!c || !c->motion || !c->car || !c->placed)return;float pos[3];pit_car_position(c->car,pos);int control=-1,all=1,ready=1;
 for(unsigned i=0;i<4;i++){if(c->active[i])all=0;if(c->active[i] && !c->tire_end[i])ready=0;}
 float dx=pos[0]-c->box[0],dy=pos[1]-c->box[1],dz=pos[2]-c->box[2],d2=(dx*dx+dy*dy)+dz*dz;
 /* 003CBD00 exact phase gates (5 m approach, 30 m exit). */
 if(c->stage==1)control=c->camera_control==8 || (c->camera_control==4 && d2<=25)?8:4;
 else if(c->stage>=2 && c->stage<5)control=ready || all?16:c->stage<3?12:14;
 else if(c->stage==5 && c->camera_control!=-1 && d2<=900)control=20;
 if(control<0){gt4_license_motion_set_time(c->motion,6,0);c->camera_begin=-1;}
 else if(c->stage>=2 && c->stage<5 && all && control!=c->camera_control)gt4_license_motion_seek_control(c->motion,6,control);
 else gt4_license_motion_step_control(c->motion,6,dt,control);
 c->camera_control=control;
}
void gt4_pit_camera_pick(gt4_pit_crew*c,int random){
 if(!c || !c->car || !c->motion || c->camera_control<0)return;const char*label;float begin,end;
 if(gt4_license_motion_interval(c->motion,6,&label,&begin,&end) && begin!=c->camera_begin){
  c->camera_begin=begin;unsigned count=gt4_license_motion_targets(c->motion,6);c->camera_choice=random>0 && count>=2?random%(count-1)+1:0;
  unsigned group,atomic[2];float time,factor;if(!gt4_license_motion_sample_blend(c->motion,6,(float)c->camera_choice,&group,atomic,&time,&factor) || c->camera_atomic==(int)atomic[0])return;
  c->camera_atomic=atomic[0];float pos[3];pit_car_position(c->car,pos);memcpy(c->camera_origin,pos,12);float t=GT4_F32(c->car+0x554);c->camera_origin[3]=GT4_F32(c->car+0x8c4)*t+GT4_F32(c->car+0x674)*(1-t);
 }
}
int gt4_pit_camera_point(const gt4_pit_crew*c,const float source[3],int coord,int up,float out[3]){
 if(coord<0 || coord>7)return 0;float local[3];memcpy(local,source,12);float root[3],yaw=0,m[16];pit_identity(m);int wheel=-1;
 unsigned kind=GT4_U8(gt4_car_layout(c->car));unsigned roles[4]={0,1,2,3};if(kind==1){roles[1]=0;roles[2]=roles[3]=1;}else if(kind==2){roles[1]=0;roles[2]=1;roles[3]=2;}else if(kind==3)roles[3]=2;
 if(coord>=2 && coord<=5)wheel=(int)roles[coord-2];
 if(!up){if(wheel>=0)for(unsigned i=0;i<3;i++)local[i]+=c->camera_hub[wheel][i];else if(coord==6){for(unsigned i=0;i<3;i++)local[i]+=c->seat[i];if(local[1]>=gt4_f32(0x3fd00000))local[1]=gt4_f32(0x3fd00000);}}
 if(coord==0 || coord==6){pit_car_position(c->car,root);float t=GT4_F32(c->car+0x554),e[3];for(unsigned i=0;i<3;i++)e[i]=GT4_F32(c->car+0x8bc+i*4)*t+GT4_F32(c->car+0x66c+i*4)*(1-t);
  /* 0048BE88: original quaternion product order (roll, yaw, -pitch). */
  float sa=sinf(e[0]*.5f),ca=cosf(e[0]*.5f),sb=sinf(e[2]*.5f),cb=cosf(e[2]*.5f),sc=sinf(-e[1]*.5f),cc=cosf(-e[1]*.5f);
  float x=(sa*cb)*cc+(ca*sb)*sc,y=(ca*sb)*cc-(sa*cb)*sc,z=(ca*cb)*sc-(sa*sb)*cc,w=(ca*cb)*cc+(sa*sb)*sc;
  float xx=2*x*x,yy=2*y*y,zz=2*z*z,xy=2*x*y,xz=2*x*z,yz=2*y*z,xw=2*x*w,yw=2*y*w,zw=2*z*w;
  m[0]=(1-yy)-zz;m[1]=xy+zw;m[2]=xz-yw;m[4]=xy-zw;m[5]=(1-zz)-xx;m[6]=yz+xw;m[8]=xz+yw;m[9]=yz-xw;m[10]=(1-xx)-yy;
 }else{yaw=coord==1?c->box[3]:c->camera_origin[3];memcpy(root,coord==1?c->box:c->camera_origin,12);if(wheel>=0){root[1]=c->box[1];unsigned axle=GT4_U8(gt4_car_layout(c->car)+0x14+wheel);root[1]+=GT4_F32(GT4_U32(c->car+0x10)+0x2c4+axle*4);}
  float sn=sinf(yaw),cs=cosf(yaw);m[0]=m[10]=cs;m[2]=-sn;m[8]=sn;}
 for(unsigned r=0;r<3;r++){out[r]=(m[r]*local[0]+m[4+r]*local[1])+m[8+r]*local[2];if(!up)out[r]+=root[r];}
 if(up){float d=(out[0]*out[0]+out[1]*out[1])+out[2]*out[2];if(d<gt4_f32(0x38d1b716))return 0;float inv=1/sqrtf(d);for(unsigned r=0;r<3;r++)out[r]*=inv;}return 1;
}
int gt4_pit_camera(const gt4_pit_crew*c,float look[9],float*fov){
 if(!c || c->camera_control<0 || !c->motion)return 0;float local[9],cc,tc;
 if(!gt4_license_motion_camera_blend(c->motion,(float)c->camera_choice,local,fov,&cc,&tc))return 0;
 /* 003CC010 adds .5 before the original integer coordinate dispatch. */
 return gt4_pit_camera_point(c,local,(int)(cc+.5f),0,look) && gt4_pit_camera_point(c,local+3,(int)(tc+.5f),0,look+3) && gt4_pit_camera_point(c,local+6,(int)(cc+.5f),1,look+6);
}
void gt4_pit_cable_vertices(const gt4_pit_crew*c,unsigned role,const float end[3],float vertices[276][3]){
 /* 003CA698 anchors/spring offsets -> 003CAAC8 fourteen-key curves ->
    003CA168 +0x74..0x4D0: 24 samples and two crossed strips, width .01. */
 static const float anchor[4]={-1.5f,1.5f,-1.375f,1.375f};float controls[4][3],sn=sinf(c->box[3]),cs=cosf(c->box[3]);
 controls[0][0]=c->box[0]+cs*anchor[role];controls[0][1]=c->box[1]+2.125f;controls[0][2]=c->box[2]-sn*anchor[role];memcpy(controls[3],end,12);
 for(unsigned j=0;j<3;j++){controls[1][j]=controls[0][j]*gt4_f32(0x3f19999a)+end[j]*gt4_f32(0x3ecccccc);controls[2][j]=controls[0][j]*gt4_f32(0x3eccccce)+end[j]*gt4_f32(0x3f199999);}
 controls[1][1]=controls[2][1]=c->box[1]+gt4_f32(0x3ca3d70a);
 float dx=end[0]-controls[0][0],dz=end[2]-controls[0][2],d2=dx*dx+dz*dz;
 if(d2>=gt4_f32(0x38d1b716)){
  float inv=1/sqrtf(d2),across=c->cable[role][0]*gt4_f32(0x40c7ae14),along=c->cable[role][2]*gt4_f32(0x40c7ae14);
  if(d2<gt4_f32(0x3b23d708)){float scale=(d2-gt4_f32(0x38d1b716))/gt4_f32(0x3b1d494f);across*=scale;along*=scale;}
  float x=dz*inv*across+dx*inv*along,z=-dx*inv*across+dz*inv*along;controls[1][0]+=x;controls[1][2]+=z;controls[2][0]+=x*.5f;controls[2][2]+=z*.5f;
 }
 float points[24][3],strips[2][24][2][3],normal[3]={1,0,0},cross[3]={0,0,-1};float time=0;
 for(unsigned i=0;i<24;i++){
  if(i==23)time=1;
  for(unsigned j=0;j<3;j++){
   float p0=controls[0][j],p1=controls[1][j],p2=controls[2][j],p3=controls[3][j];
   float keys[14]={0,p0,j==1?p1:p0,j==1?p0*gt4_f32(0x3dccccd0)+p1*gt4_f32(0x3f666666):p0*.5f+p1*.5f,gt4_f32(0x3ecccccc),p1,p1*gt4_f32(0x3f2ac083)+p2*gt4_f32(0x3eaa7ef9),p1*gt4_f32(0x3eab020e)+p2*gt4_f32(0x3f2a7ef9),gt4_f32(0x3f199999),p2,j==1?p2*gt4_f32(0x3f666666)+p3*gt4_f32(0x3dcccccc):p2*.5f+p3*.5f,j==1?p2:p3,1,p3};
   unsigned at=time>=keys[8]?8:time>=keys[4]?4:0;float v=keys[at+1],a=(keys[at+2]-keys[at+3])*3,b=(v-keys[at+2])*3,t=(time-keys[at])/(keys[at+4]-keys[at]);points[i][j]=((((keys[at+5]-v+a)*t+(b-a))*t-b)*t)+v;
  }
  time+=gt4_f32(0x3d321642);
 }
 for(unsigned i=0;i<24;i++){
  if(i<23){float dir[3],len=0;for(unsigned j=0;j<3;j++){dir[j]=points[i+1][j]-points[i][j];len+=dir[j]*dir[j];}
   if(len){float inv=1/sqrtf(len);for(unsigned j=0;j<3;j++)dir[j]*=inv;
    if(dir[0]!=0){normal[0]=-dir[1];normal[1]=dir[0];normal[2]=0;}else if(dir[1]!=0){normal[0]=0;normal[1]=-dir[2];normal[2]=dir[1];}else{normal[0]=dir[2];normal[1]=0;normal[2]=-dir[0];}
    inv=1/sqrtf((normal[0]*normal[0]+normal[1]*normal[1])+normal[2]*normal[2]);for(unsigned j=0;j<3;j++)normal[j]*=inv;
    cross[0]=dir[1]*normal[2]-dir[2]*normal[1];cross[1]=dir[2]*normal[0]-dir[0]*normal[2];cross[2]=dir[0]*normal[1]-dir[1]*normal[0];
   }
  }
  for(unsigned j=0;j<3;j++)for(unsigned side=0;side<2;side++){float sign=side?-1.f:1.f;strips[0][i][side][j]=points[i][j]+sign*normal[j]*gt4_f32(0x3c23d70a);strips[1][i][side][j]=points[i][j]+sign*cross[j]*gt4_f32(0x3c23d70a);}
 }
 unsigned out=0;for(unsigned s=0;s<2;s++)for(unsigned i=0;i<23;i++){static const unsigned corner[6][2]={{0,0},{0,1},{1,0},{1,0},{0,1},{1,1}};for(unsigned v=0;v<6;v++)memcpy(vertices[out++],strips[s][i+corner[v][0]][corner[v][1]],12);}
}
