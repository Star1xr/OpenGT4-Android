/* Pitman controller against the original 003D4C38/003D50C8.
   Trig and model VM are explicit boundaries; no game data is distributed. */
#include "ee.h"
#include "race_pit_crew.h"
#include "physics/car.h"
#include "license_motion.h"
#include "vol.h"
#include "model.h"
#include <math.h>
#include <fenv.h>
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define OBJECT 0x1800000u
#define PIT 0x1900000u
#define CAR 0x1a00000u
#define PARAM 0x1b00000u
#define DATA 0x1c00000u
#define CHECK(x) do{if(!(x)){fprintf(stderr,"walker case %u frame %u line %d: %s\n",scenario,tick,__LINE__,#x);return 1;}}while(0)
static unsigned scenario,tick;static float maximum;
static void word(uint8_t*p,unsigned v){memcpy(p,&v,4);}
static void number(uint8_t*p,float v){memcpy(p,&v,4);}
static void getcar(ee_ctx*c){c->r[2].uw[0]=CAR+c->r[5].uw[0]*CAR_SIZE;}
static void nop(ee_ctx*c){(void)c;}
static void sine(ee_ctx*c){GT4_F32(c->r[4].uw[0])=sinf(c->fpr[12].f);GT4_F32(c->r[5].uw[0])=cosf(c->fpr[12].f);}
static void angle(ee_ctx*c){c->fpr[0].f=atan2f(c->fpr[12].f,c->fpr[13].f);}
static void floor_host(ee_ctx*c){c->fpr[0].f=floorf(c->fpr[12].f);}
static ee_fn boundary(uint32_t p){switch(p){case 0x34c190:return getcar;case 0x4573b8:return nop;case 0x57d8a0:return sine;case 0x57d118:return angle;case 0x57d738:return floor_host;default:return NULL;}}
static ee_fn lookup(uint32_t p){ee_fn b=boundary(p);return b?b:recomp_lookup(p);}
static int call(ee_ctx*c,unsigned p){c->r[29].uw[0]=0x1ef0000;jmp_buf j;c->exit_jmp=&j;if(setjmp(j)){fprintf(stderr,"EE %08x %s\n",c->end_pc,c->end_detail);return 0;}recomp_lookup(p)(c);return 1;}
static int same(const gt4_pit_walker*a,unsigned address){
 gt4_pit_walker b;memcpy(&b,gt4_ptr(address),sizeof b);b.target=(b.target-address-0xc)/16;
 if(a->role!=b.role || a->state!=b.state || a->target!=b.target || a->enabled!=b.enabled || a->model!=b.model){fprintf(stderr,"discrete native %u,%u,%u,%u,%u EE %u,%u,%u,%u,%u\n",a->role,a->state,a->target,a->enabled,a->model,b.role,b.state,b.target,b.enabled,b.model);return 0;}
 for(unsigned r=0;r<2;r++)if(a->anchor[r].wheel!=b.anchor[r].wheel || a->anchor[r].side!=b.anchor[r].side){fprintf(stderr,"role %u anchor %u native wheel/side %u/%u EE %u/%u\n",a->role,r,a->anchor[r].wheel,a->anchor[r].side,b.anchor[r].wheel,b.anchor[r].side);return 0;}
 const unsigned floats[]={0x14,0x18,0x24,0x28,0x34,0x38,0x3c,0x40,0x44,0x48,0x5c,0x64,0x68,0x6c,0x70};
 for(unsigned i=0;i<sizeof floats/sizeof *floats;i++){float x,y;memcpy(&x,(const uint8_t*)a+floats[i],4);memcpy(&y,(const uint8_t*)&b+floats[i],4);float d=fabsf(x-y);if(d>maximum)maximum=d;if(!isfinite(d) || d>.00005f){fprintf(stderr,"offset %02x native %.9g EE %.9g delta %.9g\n",floats[i],x,y,d);return 0;}}
 return 1;
}
static void frames(gt4_pit_crew c[2]){
 /* 003CEB90 fixed box matrices and 003D03E0 live car matrices.
    Their graphics sin/cos boundary is shared, not a native controller. */
 for(unsigned i=0;i<2;i++){
  unsigned p=PIT+i*0xa0,car=c[i].car;float sn=sinf(c[i].box[3]),co=cosf(c[i].box[3]);
  memcpy(gt4_ptr(p+0x78a8),c[i].box,12);float forward[6]={co,sn,c[i].box[0],-sn,co,c[i].box[2]},inverse[6]={co,-sn,(-co)*c[i].box[0]+sn*c[i].box[2],sn,co,(-sn)*c[i].box[0]-co*c[i].box[2]};
  memcpy(gt4_ptr(p+0x78dc),forward,24);memcpy(gt4_ptr(p+0x78f4),inverse,24);
  float t=GT4_F32(car+0x554),x=GT4_F32(car+0x8a4)*t+GT4_F32(car+0x888)*(1-t),z=GT4_F32(car+0x8ac)*t+GT4_F32(car+0x890)*(1-t),yaw=GT4_F32(car+0x8c4)*t+GT4_F32(car+0x674)*(1-t);
  sn=sinf(yaw);co=cosf(yaw);float f[6]={co,sn,x,-sn,co,z},inv[6]={co,-sn,(-co)*x+sn*z,sn,co,(-sn)*x-co*z};memcpy(gt4_ptr(p+0x790c),f,24);memcpy(gt4_ptr(p+0x7924),inv,24);
 }
}
int main(int argc,char**argv){CHECK(argc>=2 && argc<=4);int negative=argc==3 && !strcmp(argv[2],"--negative");int step_negative=argc==3 && !strcmp(argv[2],"--negative-step");ee_ctx e;CHECK(!ee_init(&e));CHECK(ee_load_image(&e,argv[1],0x100000)>0x5a3900);e.lookup=lookup;e.hook=boundary;gt4_phys_mem=e.ram;
 fenv_t saved;fegetenv(&saved);gt4_phys_fpu_enter();gt4_pit_crew crews[2]={0};GT4_U32(PIT)=DATA;
 for(unsigned i=0;i<2;i++){
  crews[i].car=CAR+i*CAR_SIZE;crews[i].box[0]=i*12.f;crews[i].box[1]=.5f;crews[i].box[2]=i*-.7f;crews[i].box[3]=i*.3f;
  uint32_t params=PARAM+i*0x2000;GT4_U32(crews[i].car+0x10)=params;
  GT4_F32(params)=1.8f;GT4_F32(params+4)=2.1f;GT4_F32(params+8)=1.7f;GT4_F32(params+0x18)=2.6f;GT4_F32(params+0x50)=.2f;GT4_F32(params+0x2d4)=.18f;GT4_F32(params+0x2d8)=.21f;
  for(unsigned w=0;w<6;w++){GT4_F32(crews[i].car+0x900+w*4)=(w&1)?.72f:-.72f;GT4_F32(crews[i].car+0x16c+w*0xec)=(w&1)?.76f:-.76f;}
 }
 unsigned starts=0,steps=0,visited=0;
 for(unsigned layout=0;layout<13;layout++)for(unsigned variant=0;variant<2;variant++)for(unsigned stage=0;stage<6;stage++)for(unsigned spacing=0;spacing<3;spacing++,scenario++){
  memset(e.ram+OBJECT,0,0x1400);GT4_U8(PARAM+0x30)=(uint8_t)layout;GT4_F32(CAR+0x554)=(scenario%5)*.25f;uint32_t seed=0xfffffff0u;GT4_U32(PIT+0x96d0)=seed;
  memset(e.r,0,sizeof e.r);e.r[4].uw[0]=OBJECT;e.r[5].uw[0]=variant;e.r[6].uw[0]=DATA;e.r[7].uw[0]=PIT;e.r[8].uw[0]=stage;e.r[9].uw[0]=CAR;e.r[10].uw[0]=spacing;CHECK(call(&e,0x3d4c38));
  gt4_pit_walk_init(crews[0].walking,CAR,variant,stage,spacing,&seed);CHECK(seed==GT4_U32(PIT+0x96d0));
  if(negative && !scenario)crews[0].walking[0].waiting[0]+=.125f;
  for(unsigned r=0;r<4;r++)CHECK(same(crews[0].walking+r,OBJECT+r*0x74));starts++;
 }
 /* Force each of the 20 states, neighboring stops, requests, timesteps
    and obstacles, then compare a full service/departure trajectory. */
 for(unsigned variant=0;variant<2;variant++)for(unsigned entry=0;entry<2;entry++)for(unsigned state=0;state<20;state++)for(unsigned sample=0;sample<12;sample++,scenario++){
  gt4_pit_crew*c=crews+entry;c->variant=(int)variant;c->stage=sample%6;c->walk_spacing=sample%3;crews[entry^1].stage=sample%2;
  uint32_t seed=0x91827364u+scenario;gt4_pit_walk_init(c->walking,c->car,variant,0,c->walk_spacing,&seed);
  for(unsigned r=0;r<4;r++){gt4_pit_walker*a=c->walking+r;a->state=state;a->enabled=1;a->timer=sample*.8f;a->rate=.6f;a->phase=sample*.09f;a->position[0]=(sample%3)*1.3f;a->position[1]=(int)(sample%5)-2.f;a->velocity[0]=sample*.15f;a->velocity[1]=-sample*.08f;
   gt4_pit_walker b=*a;b.target=OBJECT+r*0x74+0x1c;memcpy(gt4_ptr(OBJECT+r*0x74),&b,sizeof b);}
  unsigned commands=sample%4;GT4_U32(OBJECT+0x31c)=c->walk_spacing!=0;GT4_U32(PIT+0x9684+entry*4)=c->stage;GT4_U32(PIT+0x9684+(entry^1)*4)=crews[entry^1].stage;
  for(unsigned i=0;i<2;i++){uint32_t car=crews[i].car;GT4_F32(car+0x554)=.25f;for(unsigned j=0;j<3;j++)GT4_F32(car+0x888+j*4)=GT4_F32(car+0x8a4+j*4)=crews[i].box[j]+(j==0?(sample%2?2.f:30.f):0);GT4_F32(car+0x674)=GT4_F32(car+0x8c4)=sample*.07f;}
  frames(crews);GT4_U32(PIT+0x96d0)=seed;GT4_U32(PARAM+0x1f8a0)=2;
  float dt=sample%3==0?1.f/120:sample%3==1?1.f/60:1.f/30;
  for(tick=0;tick<45;tick++){
   memset(e.r,0,sizeof e.r);e.r[4].uw[0]=OBJECT;e.r[5].uw[0]=variant;e.r[6].uw[0]=entry;e.r[7].uw[0]=PIT;e.r[8].uw[0]=PARAM+0x10000;e.r[9].uw[0]=commands;e.fpr[12].f=dt;
   /* 003D50C8's data +0C indexes are model VM boundary identities. */
   for(unsigned r=0;r<4;r++)GT4_U32(DATA+0xc+(entry*4+r)*4)=DATA+0x100;
   CHECK(call(&e,0x3d50c8));gt4_pit_walk_step(crews,2,entry,(int)commands,dt,&seed);CHECK(seed==GT4_U32(PIT+0x96d0));
   if(step_negative)c->walking[0].state^=1;
   for(unsigned r=0;r<4;r++){visited|=1u<<c->walking[r].state;CHECK(same(c->walking+r,OBJECT+r*0x74));}steps++;
  }
 }
 printf("PASS %u initializations, %u controller ticks, 20 forced states, state mask %05x; integers/RNG exact, float max %.9g tolerance 5e-5.\n",starts,steps,visited,maximum);
 /* Two successive stops, starting at the wall rather than forcing poses.
    Test every entry/package through squat, withdrawal and idle again. */
 for(unsigned variant=0;variant<2;variant++)for(unsigned entry=0;entry<2;entry++,scenario++){
  gt4_pit_crew*c=crews+entry;c->variant=(int)variant;c->walk_spacing=0;crews[entry^1].stage=0;
  uint32_t seed=0x71234567u;gt4_pit_walk_init(c->walking,c->car,variant,0,0,&seed);
  for(unsigned r=0;r<4;r++){gt4_pit_walker b=c->walking[r];b.target=OBJECT+r*0x74+0x1c;memcpy(gt4_ptr(OBJECT+r*0x74),&b,sizeof b);}
  GT4_U32(OBJECT+0x31c)=0;GT4_U32(PIT+0x96d0)=seed;GT4_U32(PIT+0x9684+(entry^1)*4)=0;
  for(tick=0;tick<6000;tick++){
   unsigned frame=tick%3000;c->stage=frame<60?0:frame<420?1:frame<540?2:frame<1200?3:frame<1320?4:frame<1500?5:0;
   GT4_U32(PIT+0x9684+entry*4)=c->stage;
   for(unsigned i=0;i<2;i++){uint32_t car=crews[i].car;float offset=i!=entry || frame>=1400?30.f:0;
    for(unsigned axis=0;axis<3;axis++)GT4_F32(car+0x888+axis*4)=GT4_F32(car+0x8a4+axis*4)=crews[i].box[axis]+(axis==0?offset:0);
    GT4_F32(car+0x674)=GT4_F32(car+0x8c4)=crews[i].box[3];}
   frames(crews);memset(e.r,0,sizeof e.r);e.r[4].uw[0]=OBJECT;e.r[5].uw[0]=variant;e.r[6].uw[0]=entry;e.r[7].uw[0]=PIT;e.r[8].uw[0]=PARAM+0x10000;e.r[9].uw[0]=c->stage==1?3:0;e.fpr[12].f=1.f/60;
   CHECK(call(&e,0x3d50c8));gt4_pit_walk_step(crews,2,entry,c->stage==1?3:0,1.f/60,&seed);CHECK(seed==GT4_U32(PIT+0x96d0));
   for(unsigned r=0;r<4;r++)CHECK(same(c->walking+r,OBJECT+r*0x74));
   if(frame==2999)for(unsigned r=0;r<4;r++)if(c->walking[r].enabled)CHECK(c->walking[r].state==0);
  }
 }puts("PASS 24000 trajectory ticks: two successive full stops per entry/package, return to idle.");
 for(unsigned n=0;n<10000;n++){
  gt4_pit_walker a={0};a.anchor[1].point[0]=2;a.anchor[1].point[1]=1;a.waiting[0]=-2;a.waiting[1]=-1;a.position[0]=(n%21)*.13f;a.position[1]=(n%29)*-.17f;a.light[0]=(n%11)*.2f;a.light[2]=(n%17)*.1f;
  memcpy(gt4_ptr(OBJECT),&a,sizeof a);memset(e.r,0,sizeof e.r);e.r[4].uw[0]=OBJECT;e.r[5].uw[0]=OBJECT+0x200;e.r[6].uw[0]=OBJECT+0x204;CHECK(call(&e,0x3d4aa8));
  float light=gt4_pit_walk_light(&a);if(argc==3 && !strcmp(argv[2],"--negative-light") && n==37)light+=.125f;CHECK(fabsf(light-GT4_F32(OBJECT+0x200))<.000005f);
 }puts("PASS 10000 original endpoint colour interpolations, tolerance 5e-6.");
 /* 0049E228 loads/stores raw material vectors. Verify these instructions
    in the local executable, then compare decoded values with source bytes.
    This proves data transport, not execution of VU lighting or GS pixels. */
 CHECK(GT4_U32(0x49e23c)==0xd8810000 && GT4_U32(0x49e240)==0xd8820010 && GT4_U32(0x49e244)==0xd8830020);
 CHECK(GT4_U32(0x49e2c4)==0xf8610a60 && GT4_U32(0x49e2c8)==0xf8620a70 && GT4_U32(0x49e2cc)==0xf8630a80);
 for(unsigned n=0;n<100;n++){
  uint8_t d[1024]={0};memcpy(d,"MDLS",4);word(d+16,sizeof d);d[22]=d[24]=d[26]=d[28]=d[30]=1;
  word(d+56,128);word(d+60,168);word(d+64,512);word(d+152,180);word(d+168,192);word(d+68,172);word(d+172,176);word(d+176,400);memcpy(d+400,"Tex1",4);
  d[180]=4;d[200]=1;d[202]=1;word(d+208,32);d[212]=10;d[215]=2;d[225]=0xc0;d[226]=3;d[227]=0x68;number(d+240,1);number(d+256,1);
  d[300]=0x40;d[301]=0xc0;d[302]=2;d[303]=0x66;d[306]=3;
  for(unsigned type=0;type<3;type++)for(unsigned ch=0;ch<4;ch++)number(d+512+type*16+ch*4,(float)((int)n+(int)ch+(int)type-20)/40);
  number(d+512+64,16);
  gt4_model*m=gt4_model_decode(d,sizeof d,0);CHECK(m && m->count==1);
  if(argc==3 && !strcmp(argv[2],"--negative-material") && n==75)m->triangles[0].v[0].ambient[0]=1;
  for(unsigned v=0;v<3;v++){CHECK(!memcmp(m->triangles[0].v[v].ambient,d+512,12));CHECK(!memcmp(m->triangles[0].v[v].rgb,d+528,12));CHECK(!memcmp(m->triangles[0].v[v].specular,d+544,12));}gt4_model_free(m);
 }puts("PASS 100 material records retained bit exact, including over-unit colours; original load/store contract verified.");
 for(unsigned stage=0;stage<6;stage++)for(unsigned transition=0;transition<3;transition++){
  GT4_U32(OBJECT+0x40)=stage;GT4_U32(OBJECT+0x48)=transition;memset(e.r,0,sizeof e.r);e.r[4].uw[0]=OBJECT;CHECK(call(&e,0x3d8c48));
  int allowed=gt4_pit_monitor_input(stage,transition);if(argc==3 && !strcmp(argv[2],"--negative-monitor") && stage==3)allowed^=1;CHECK(allowed==(int)e.r[2].uw[0]);
 }puts("PASS 18 monitor ownership gates, exact against original 003D8C48.");
 if(argc>=3 && argv[2][0]!='-'){
  const char*error;gt4_vol*v=gt4_vol_open(argv[2],&error);CHECK(v);unsigned samples=0;
  for(unsigned package=1;package<=2;package++){
   size_t bytes;uint8_t*d=gt4_vol_read(v,package==1?"/pit/pit_001":"/pit/pit_002",&bytes);CHECK(d);gt4_license_motion*m=gt4_license_motion_open_pit(d,bytes);CHECK(m);free(d);const uint8_t*mdl=gt4_license_motion_model(m,0,&bytes);CHECK(mdl);
   unsigned table,count,programs;memcpy(&table,mdl+56,4);memcpy(&programs,mdl+92,4);uint16_t model_count;memcpy(&model_count,mdl+22,2);count=model_count;
   for(unsigned model=0;model<count;model++){
    unsigned program;memcpy(&program,mdl+table+model*40+36,4);if(program==~0u)continue;CHECK(programs+program+35<=bytes);
    float scale;memcpy(&scale,mdl+programs+program+6,4);unsigned int_slot=mdl[programs+program+19],fraction_slot=mdl[programs+program+31];
    for(unsigned branch=0;branch<(scale>1?4u:1u);branch++){gt4_model_stats stats;gt4_model*p=gt4_model_decode_pit_walk_part(mdl,bytes,(int)model,(int)branch,&stats);if(!p)fprintf(stderr,"decode package %u model %u branch %u line %d op %d offset %zu\n",package,model,branch,stats.fail_line,stats.fail_op,stats.fail_offset);CHECK(p && p->morph && p->count>0 && p->phase_scale==scale && p->grasp_scale==1.f);gt4_model_free(p);}
    memcpy(gt4_ptr(DATA+0x1000),mdl+programs+program,35);GT4_U32(DATA)=DATA+0x3000;GT4_U32(DATA+8)=DATA+0x3100;
    for(unsigned n=0;n<=10000;n++){
     float phase=(float)n/10000;GT4_F32(DATA+0x3100)=phase;memset(e.r,0,sizeof e.r);e.r[4].uw[0]=DATA;e.r[5].uw[0]=DATA+0x1000;e.r[6].uw[0]=0;e.r[7].uw[0]=0;CHECK(call(&e,0x4569f0));
     unsigned branch;float tween;CHECK(gt4_pit_walk_sample(phase,scale,&branch,&tween));if(argc==4 && !strcmp(argv[3],"--negative-phase") && n==37)tween+=.125f;
     CHECK(branch==GT4_U32(DATA+0x3000+int_slot*4));CHECK(!memcmp(&tween,gt4_ptr(DATA+0x3000+fraction_slot*4),4));samples++;
    }
   }gt4_license_motion_close(m);
  }printf("PASS %u BC_WALK/BC_SQUAT samples bit-for-bit against original model VM; both packages, all cached branches.\n",samples);gt4_vol_close(v);
 }
 fesetenv(&saved);ee_free(&e);return 0;
}
