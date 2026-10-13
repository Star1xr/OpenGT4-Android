/* Original 00156AA0 records matrix primitive arguments. VU/GS execution,
   assets and wheel geometry queries are boundaries, not renderer parity. */
#include "ee.h"
#include "fpu.h"
#include "car_service.h"
#include <stdlib.h>
#include <string.h>
#define OBJECT 0x1800000u
#define MODELS 0x1801000u
#define TRACKS 0x1802000u
#define VTABLE 0x1803000u
#define DRAW 0x100080u
#define CHECK(x) do{if(!(x)){fprintf(stderr,"service placement case %u line %d: %s\n",test,__LINE__,#x);return 1;}}while(0)
ee_fn recomp_lookup(uint32_t);
static float geometry[8];static int endpoints[2];static gt4_service_placement recorded;static unsigned draws;
static float radii[4],original_lift,original_wheels[4];
static int query_limit;
static void put(ee_ctx *c,uint32_t p,uint32_t v){memcpy(c->ram+p,&v,4);}
static uint32_t bits(float x){uint32_t b;memcpy(&b,&x,4);return b;}
static float f32(uint32_t b){float x;memcpy(&x,&b,4);return x;}
static void nothing(ee_ctx *c){(void)c;}
static void us_frame(ee_ctx *c){c->r[2].uw[0]=0;}
static void wheel_layout(ee_ctx *c){c->r[2].uw[0]=OBJECT+0x500;}
static void contact(ee_ctx *c){c->fpr[0].f=geometry[c->r[5].uw[0]?3:0];}
static void width(ee_ctx *c){c->fpr[0].f=geometry[c->r[5].uw[0]?4:1];}
static void axle(ee_ctx *c){c->fpr[0].f=geometry[c->r[5].uw[0]?5:2];}
static void front(ee_ctx *c){c->r[2].sw[0]=endpoints[0];}
static void rear(ee_ctx *c){c->r[2].sw[0]=endpoints[1];}
static void translate(ee_ctx *c){for(int i=0;i<3;i++)recorded.offset[i]=c->fpr[12+i].f;}
static void rotate(ee_ctx *c){recorded.yaw=c->fpr[12].f;}
static void scale(ee_ctx *c){for(int i=0;i<3;i++)recorded.scale[i]=c->fpr[12+i].f;}
static void draw(ee_ctx *c){(void)c;recorded.draw=1;draws++;}
static void params(ee_ctx *c){memset(c->ram+c->r[4].uw[0],0,0x58);}
static void radius(ee_ctx *c){c->fpr[0].f=radii[c->r[5].uw[0]];}
static void lifted(ee_ctx *c){uint32_t p=c->r[5].uw[0];memcpy(&original_lift,c->ram+p+0x50,4);
 for(int i=0;i<4;i++)memcpy(&original_wheels[i],c->ram+p+0x18+i*16,4);}
static ee_fn boundary(uint32_t pc){switch(pc){
 case 0x364c40:return contact;case 0x364d78:return width;case 0x364c60:return axle;
 case 0x364f70:return front;case 0x364f80:return rear;
 case 0x4a7844:return translate;case 0x4a79b0:return rotate;case 0x4a7890:return scale;
 case 0x459780:return params;case 0x364b30:return query_limit?NULL:radius;case 0x451a00:return lifted;
 case DRAW:return draw;case 0x4a53f8:case 0x4a5400:case 0x156820:case 0x156920:return nothing;
 case 0x48edd0:return us_frame;
 case 0x359510:return wheel_layout;
 case 0x4a22c0:case 0x4a0808:case 0x4a0910:return nothing;
 default:return NULL;}}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
int main(int argc,char **argv){unsigned test=0;CHECK(argc==2);ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0x590000);
 c.lookup=lookup;c.hook=boundary;unsigned seed=0x20261008;
 put(&c,OBJECT+0x2a4,1);put(&c,OBJECT+0x6c,MODELS);put(&c,OBJECT+0x70,MODELS+4);
 put(&c,OBJECT+0x7c,TRACKS);put(&c,OBJECT+0x80,TRACKS+20);put(&c,MODELS,OBJECT+0x100);
 put(&c,OBJECT+0x104,VTABLE);put(&c,VTABLE+0x5c,DRAW);
 for(test=0;test<4096;test++){
  for(int i=0;i<8;i++){seed=seed*1664525u+1013904223u;geometry[i]=(int)(seed%8001)/1024.f-4;}
  /* Native endpoints convert signed shorts to metres before alignment. */
  int a=(int)(seed%6001)-3000,b=(int)((seed>>8)%6001)-3000;
  endpoints[0]=a;endpoints[1]=b;
  int alignment=test%13,hidden=(test/13)%2,reflection=(test/26)%2;
  float yaw=(float)((int)(seed%721)-360),size=(float)((seed>>10)%2049)/1024;
  put(&c,OBJECT+0x8c,bits(size));put(&c,TRACKS+4,0);put(&c,TRACKS+8,(uint32_t)alignment);
  put(&c,TRACKS+12,(uint32_t)hidden);put(&c,TRACKS+16,bits(yaw));
  memset(&recorded,0,sizeof recorded);draws=0;memset(c.r,0,sizeof c.r);
  c.r[4].uw[0]=OBJECT;c.r[5].uw[0]=reflection;c.r[29].uw[0]=0x1ef0000;
  jmp_buf jump;c.exit_jmp=&jump;ee_fpu_enter();if(setjmp(jump)){ee_fpu_leave();fprintf(stderr,"original: %s\n",c.end_detail);return 1;}
  geometry[6]=a*f32(0x3a83126e);geometry[7]=b*f32(0x3a83126e);
  recomp_lookup(0x156aa0)(&c);gt4_service_placement native=gt4_service_place(geometry,alignment,yaw,size,hidden,reflection);ee_fpu_leave();
  if(getenv("GT4_SERVICE_PLACEMENT_NEGATIVE"))native.draw^=1;
  CHECK(native.draw==(draws!=0));if(!native.draw)continue;
  CHECK(!memcmp(native.offset,recorded.offset,sizeof native.offset));CHECK(bits(native.yaw)==bits(recorded.yaw));CHECK(!memcmp(native.scale,recorded.scale,sizeof native.scale));
 }
 for(test=0;test<4096;test++){
  float lift=(float)((int)(test%1025)-1)/512;
  for(int i=0;i<4;i++)radii[i]=(float)(1+(test*(i+1))%511)/1024;
  if(test%17==0)lift=radii[test%4];
  put(&c,OBJECT+0x8a8,bits(lift));put(&c,OBJECT+0x870,0);put(&c,OBJECT+0x188,0);
  c.r[4].uw[0]=OBJECT;c.r[5].uw[0]=test&1;c.r[29].uw[0]=0x1ef0000;
  jmp_buf jump;c.exit_jmp=&jump;ee_fpu_enter();if(setjmp(jump)){ee_fpu_leave();fprintf(stderr,"original lift: %s\n",c.end_detail);return 1;}
  recomp_lookup(0x156950)(&c);ee_fpu_leave();
  float body,wheels[4];gt4_service_lift(lift,radii,&body,wheels);
  if(getenv("GT4_SERVICE_LIFT_NEGATIVE"))body+=.001f;
  CHECK(bits(body)==bits(original_lift));CHECK(!memcmp(wheels,original_wheels,sizeof wheels));
 }
 unsigned steps=0;
 for(test=0;test<1024;test++){
  int playing=test&1,loop=(test>>1)&1,has_motion=(test>>2)&1;
  float time=(float)(test%97)/8.f,end=(float)(1+test%71)/4.f,start=end*.25f,dt=f32(0x3c888888);
  if(test%17==0){time=end;dt=0;} /* Strict end boundary. */
  put(&c,OBJECT+8,has_motion?1:0);put(&c,OBJECT+0x10,playing);put(&c,OBJECT+0x14,bits(time));
  put(&c,OBJECT+0x1c,0xffffffffu);put(&c,OBJECT+0x20,loop);
  for(unsigned frame=0;frame<180;frame++){
   c.r[4].uw[0]=OBJECT;c.r[29].uw[0]=0x1ef0000;c.fpr[12].f=dt;
   jmp_buf jump;c.exit_jmp=&jump;ee_fpu_enter();if(setjmp(jump)){ee_fpu_leave();fprintf(stderr,"original clock: %s\n",c.end_detail);return 1;}
   recomp_lookup(0x21b4f8)(&c);
   if(has_motion){c.r[4].uw[0]=OBJECT+0x10;c.fpr[12].f=end;c.fpr[13].f=start;recomp_lookup(0x21b528)(&c);}
   gt4_service_clock(&time,&playing,has_motion,dt,end,start,loop);ee_fpu_leave();
   if(getenv("GT4_SERVICE_CONTROLLER_NEGATIVE") && !frame)time+=.001f;
   uint32_t original_time,original_playing;memcpy(&original_time,c.ram+OBJECT+0x14,4);memcpy(&original_playing,c.ram+OBJECT+0x10,4);
   CHECK(bits(time)==original_time && (unsigned)playing==original_playing);steps++;
  }
 }
 unsigned poses=0,draw_clocks=0,drops=0;
 query_limit=1;
 for(test=0;test<4096;test++){
  put(&c,OBJECT,OBJECT+0x1000);c.ram[OBJECT+0x501]=4;
  for(int i=0;i<4;i++)c.ram[OBJECT+0x514+i]=(uint8_t)(i/2);
  float axles[2][5];for(int a=0;a<2;a++)for(int i=0;i<5;i++){
   seed=seed*1664525u+1013904223u;axles[a][i]=(int)(seed%8001)/1024.f-4;
   put(&c,OBJECT+0x1908+a*0x58+i*4,bits(axles[a][i]));
  }
  for(int i=0;i<4;i++){
   c.r[4].uw[0]=OBJECT;c.r[5].uw[0]=i;c.r[29].uw[0]=0x1ef0000;jmp_buf jump;c.exit_jmp=&jump;
   ee_fpu_enter();if(setjmp(jump)){ee_fpu_leave();return 1;}recomp_lookup(0x364b30)(&c);
   float value=gt4_service_drop_limit(axles[i/2]);ee_fpu_leave();
   if(getenv("GT4_SERVICE_DROP_NEGATIVE"))value+=.001f;CHECK(bits(value)==c.fpr[0].u);drops++;
  }
 }
 query_limit=0;
 unsigned blends=0;
 for(test=0;test<4096;test++){
  float opacity=test/1024.f-.5f;c.r[4].uw[0]=test&1?0x68:0x64;c.fpr[12].f=opacity;c.r[29].uw[0]=0x1ef0000;
  jmp_buf jump;c.exit_jmp=&jump;ee_fpu_enter();if(setjmp(jump)){ee_fpu_leave();return 1;}
  recomp_lookup(0x4a2808)(&c);unsigned fix=gt4_service_blend_fix(opacity);ee_fpu_leave();
  if(getenv("GT4_SERVICE_BLEND_NEGATIVE"))fix^=1;CHECK(fix==c.spr[0x219c]);blends++;
 }
 for(test=0;test<512;test++){
  memset(c.ram+OBJECT,0,0x200);put(&c,OBJECT+0x10,2);
  put(&c,OBJECT+0x58,0x3dcccccc);put(&c,OBJECT+0x5c,0x3dcccccc);put(&c,OBJECT+0x64,0x3c088888);
  float p[3],a[3],tp[3],ta[3],opacity=0;
  for(int i=0;i<3;i++){
   seed=seed*1664525u+1013904223u;p[i]=(int)(seed%8001)/1024.f-4;
   tp[i]=p[i]+(float)(test%31)/16;a[i]=(float)((int)(seed%721)-360);ta[i]=-a[i];
   put(&c,OBJECT+0x14+i*4,bits(tp[i]));put(&c,OBJECT+0x2c+i*4,bits(p[i]));
   put(&c,OBJECT+0x20+i*4,bits(ta[i]));put(&c,OBJECT+(i==0?0x38:i==1?0x44:0x48),bits(a[i]));
  }
  for(unsigned frame=0;frame<180;frame++){
   c.r[4].uw[0]=OBJECT;c.r[29].uw[0]=0x1ef0000;jmp_buf jump;c.exit_jmp=&jump;
   ee_fpu_enter();if(setjmp(jump)){ee_fpu_leave();fprintf(stderr,"original pose: %s\n",c.end_detail);return 1;}
   recomp_lookup(0x153bb8)(&c);gt4_service_pose(p,a,tp,ta);opacity=gt4_service_fade(opacity);ee_fpu_leave();
   if(getenv("GT4_SERVICE_POSE_NEGATIVE") && !frame)a[1]+=.001f;
   for(int i=0;i<3;i++){
    uint32_t op,oa;memcpy(&op,c.ram+OBJECT+0x2c+i*4,4);memcpy(&oa,c.ram+OBJECT+(i==0?0x38:i==1?0x44:0x48),4);
    CHECK(bits(p[i])==op && bits(a[i])==oa);
   }
   uint32_t oo;memcpy(&oo,c.ram+OBJECT+0x4c,4);CHECK(bits(opacity)==oo);poses++;
  }
 }
 /* Same original position block, including authored prize speeds +58. */
 for(test=0;test<256;test++){
  memset(c.ram+OBJECT,0,0x200);put(&c,OBJECT+0x10,2);
  float ratio=test&1?.025f:.1f,p[3]={-3,.8f,8.5f},target[3]={-.9f,0,1.6f};
  put(&c,OBJECT+0x58,bits(ratio));put(&c,OBJECT+0x5c,0x3dcccccc);
  for(int i=0;i<3;i++){target[i]+=(test%13)*.125f;put(&c,OBJECT+0x14+i*4,bits(target[i]));put(&c,OBJECT+0x2c+i*4,bits(p[i]));}
  for(unsigned frame=0;frame<180;frame++){
   c.r[4].uw[0]=OBJECT;c.r[29].uw[0]=0x1ef0000;jmp_buf jump;c.exit_jmp=&jump;
   ee_fpu_enter();if(setjmp(jump)){ee_fpu_leave();return 1;}
   recomp_lookup(0x153bb8)(&c);gt4_service_position(p,target,ratio);ee_fpu_leave();
   if(getenv("GT4_SERVICE_POSITION_NEGATIVE") && !frame)p[2]+=.001f;
   for(int i=0;i<3;i++){uint32_t original;memcpy(&original,c.ram+OBJECT+0x2c+i*4,4);CHECK(bits(p[i])==original);}
  }
 }
 /* Additional-model draws normalize a temporary controller. Camera draws
    normalize the live one; neither operation belongs to frame update. */
 for(test=0;test<128;test++){
  float t=0,end=(test%17+1)/8.f,start=end*.25f,dt=f32(0x3c888888);int playing=1,loop=test&1;
  memset(c.ram+OBJECT,0,0x200);put(&c,OBJECT+8,1);put(&c,OBJECT+0x10,1);
  put(&c,OBJECT+0x1c,0xffffffffu);put(&c,OBJECT+0x20,loop);
  for(unsigned frame=0;frame<360;frame++){
   c.r[4].uw[0]=OBJECT;c.r[29].uw[0]=0x1ef0000;c.fpr[12].f=dt;jmp_buf jump;c.exit_jmp=&jump;
   ee_fpu_enter();if(setjmp(jump)){ee_fpu_leave();return 1;}
   recomp_lookup(0x21b4f8)(&c);gt4_service_advance(&t,playing,1,dt);
   uint32_t raw;memcpy(&raw,c.ram+OBJECT+0x14,4);CHECK(bits(t)==raw);
   memcpy(c.ram+OBJECT+0x100,c.ram+OBJECT,0x24);
   c.r[4].uw[0]=OBJECT+0x110;c.fpr[12].f=end;c.fpr[13].f=start;recomp_lookup(0x21b528)(&c);
   float sampled=gt4_service_sample(t,1,end,start,loop);ee_fpu_leave();
   if(getenv("GT4_SERVICE_DRAW_CLOCK_NEGATIVE") && frame==359)sampled+=.001f;
   uint32_t original_sample;memcpy(&original_sample,c.ram+OBJECT+0x114,4);CHECK(bits(sampled)==original_sample);draw_clocks++;
  }
 }
 unsigned jitter_cases=0;
 for(test=0;test<3;test++){
  int samples=test?2:1,index=test==2?1:0;
  memset(c.spr+0x2000,0,0x400);memcpy(c.spr+0x22b8,(float[]){1,1},8);
  c.r[4].uw[0]=samples;c.r[5].uw[0]=index;c.fpr[12].f=f32(0x3f333333);c.r[29].uw[0]=0x1ef0000;
  jmp_buf jump;c.exit_jmp=&jump;ee_fpu_enter();if(setjmp(jump)){ee_fpu_leave();fprintf(stderr,"jitter original: %s\n",c.end_detail);return 1;}
  recomp_lookup(0x4a2c58)(&c);float offset[2];gt4_service_jitter(samples,index,offset);ee_fpu_leave();
  if(getenv("GT4_SERVICE_JITTER_NEGATIVE"))offset[0]+=.0625f;
  uint16_t x,y;memcpy(&x,c.spr+0x2178,2);memcpy(&y,c.spr+0x217c,2);
  CHECK(offset[0]==x/16.f && offset[1]==y/16.f);jitter_cases++;
 }
 unsigned projections=0;const int modes[]={0,1,2,3,4,0x50,0x51,0x52};
 for(test=0;test<16;test++){
  memset(c.ram+OBJECT,0,0x200);put(&c,OBJECT,0xffffffffu);
  c.r[4].uw[0]=OBJECT;c.r[5].uw[0]=1;c.r[6].uw[0]=modes[test/2];c.r[7].uw[0]=1;c.r[29].uw[0]=0x1ef0000;
  jmp_buf jump;c.exit_jmp=&jump;ee_fpu_enter();if(setjmp(jump)){ee_fpu_leave();fprintf(stderr,"projection original: %s\n",c.end_detail);return 1;}
  recomp_lookup(0x1060f8)(&c);put(&c,OBJECT+0x20,test&1);
  c.r[4].uw[0]=OBJECT;recomp_lookup(0x107328)(&c);
  float correction[2];gt4_service_projection_scale(modes[test/2],test&1,correction);ee_fpu_leave();
  if(getenv("GT4_SERVICE_PROJECTION_NEGATIVE"))correction[1]=1;
  CHECK(bits(correction[0])==bits(recorded.scale[0]));CHECK(bits(correction[1])==bits(recorded.scale[1]));projections++;
 }
 ee_free(&c);printf("PASS 4096 exact placements, 4096 lift parameters, %u suspension limits, %u blend coefficients, %u controller states, %u motion-2/fade frames, %u draw-clock copies, %u video projections and %u GS jitter offsets\n",drops,blends,steps,poses,draw_clocks,projections,jitter_cases);return 0;
}
