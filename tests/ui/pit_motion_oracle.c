/* Resident /pit/pit_001, native clock/dispatch/marks against the EE.
   Drawing and GPU transforms are boundaries; no game data embedded. */
#include "ee.h"
#include "license_motion.h"
#include "race_pit_crew.h"
#include "physics/car.h"
#include "vol.h"
#include <stdlib.h>
#include <string.h>
#include <fenv.h>
ee_fn recomp_lookup(uint32_t);
#define BASE 0x1900000u
#define TRACK 0x1800000u
#define CHECK(x) do{if(!(x)){fprintf(stderr,"pit motion named %u tick %u line %d: %s\n",named,tick,__LINE__,#x);return 1;}}while(0)
static unsigned named,tick,sampled,atoms[2];static float local,weight;
static void sample(ee_ctx*c){sampled++;atoms[0]=c->r[5].uw[0];atoms[1]=c->r[6].uw[0];local=c->fpr[12].f;weight=c->fpr[14].f;}
static ee_fn boundary(uint32_t p){return p==0x42b440?sample:NULL;}
static ee_fn lookup(uint32_t p){ee_fn f=boundary(p);return f?f:recomp_lookup(p);}
static unsigned word(const uint8_t*d){uint32_t v;memcpy(&v,d,4);return v;}
static unsigned half(const uint8_t*d){return d[0]|(unsigned)d[1]<<8;}
static void put(ee_ctx*c,unsigned p,unsigned v){memcpy(c->ram+p,&v,4);}
static int call(ee_ctx*c,uint32_t pc){c->r[29].uw[0]=0x1ef0000;jmp_buf jump;c->exit_jmp=&jump;
 if(setjmp(jump)){fprintf(stderr,"original %08x %s\n",c->end_pc,c->end_detail);return 0;}recomp_lookup(pc)(c);return 1;}
int main(int argc,char**argv){
 CHECK(argc==3 || argc==4);ee_ctx e;CHECK(!ee_init(&e));CHECK(ee_load_image(&e,argv[1],0x100000)>0x5a3900);
 const char *error;gt4_vol*v=gt4_vol_open(argv[2],&error);CHECK(v);size_t size;
 unsigned cases=0,marks=0;for(unsigned package=1;package<=2;package++){
 uint8_t*d=gt4_vol_read(v,package==1?"/pit/pit_001":"/pit/pit_002",&size);CHECK(d && size<0x100000);
 gt4_license_motion*m=gt4_license_motion_open_pit(d,size);CHECK(m);e.lookup=lookup;e.hook=boundary;
 memcpy(e.ram+BASE,d,size);
 /* 003CD620: four service roots for every layout, both sides, fractional
    wheel interpolation and out-of-range wheel indices. No physics writes. */
 gt4_phys_mem=e.ram;put(&e,TRACK+0x10,TRACK+0x2000);
 for(unsigned layout=0;layout<13;layout++)for(unsigned wheel=0;wheel<6;wheel++)for(unsigned side=0;side<2;side++)for(unsigned fraction=0;fraction<9;fraction++){
  unsigned params=TRACK+0x2000;e.ram[params+0x30]=(uint8_t)layout;
  GT4_F32(TRACK+0x554)=(float)fraction/8;
  GT4_F32(params+0x18)=2.448f;GT4_F32(params+0x2d4)=1.556f;GT4_F32(params+0x2d8)=1.618f;
  for(unsigned i=0;i<6;i++){GT4_F32(TRACK+0x900+i*4)=(float)(i+1)*.125f;GT4_F32(TRACK+0x16c+i*0xec)=-(float)(i+1)*.25f;}
  memset(e.r,0,sizeof e.r);e.r[4].uw[0]=TRACK+0x4000;e.r[5].uw[0]=TRACK+0x4010;e.r[6].uw[0]=TRACK;e.r[7].uw[0]=wheel;e.r[8].uw[0]=side;
  fenv_t env;fegetenv(&env);gt4_phys_fpu_enter();CHECK(call(&e,0x3cd620));float actual[4];gt4_pit_wheel_place(TRACK,wheel,side,actual);fesetenv(&env);
  if(argc==4 && !strcmp(argv[3],"--negative-wheel") && layout==0 && wheel==0 && !side && !fraction)actual[0]+=.125f;
  CHECK(!memcmp(actual,e.ram+TRACK+0x4000,12));CHECK(!memcmp(actual+3,e.ram+TRACK+0x4010,4));
 }
 puts("PASS 1404 wheel-side roots, all 13 drive layouts, bit-for-bit against 003CD620.");
 /* Exhaust the stage/timer boundaries and both role readiness inputs. */
 for(unsigned lane=0;lane<6;lane++)for(unsigned flags=0;flags<=16;flags+=16)for(unsigned orders=0;orders<2;orders++)for(unsigned timer=0;timer<800;timer++){
  e.ram[TRACK+0x56b]=(uint8_t)lane;e.ram[TRACK+0x5b5]=(uint8_t)flags;
  memcpy(e.ram+TRACK+0x5bc,&orders,2);memcpy(e.ram+TRACK+0x5be,&timer,2);
  memset(e.r,0,sizeof e.r);e.r[4].uw[0]=TRACK;CHECK(call(&e,0x35bbd8));
  CHECK(gt4_pit_stage_fields(lane,flags,orders,timer)==(int)e.r[2].sw[0]);
  for(unsigned refuel=0;refuel<2;refuel++){
   unsigned bits=refuel*0x100;memcpy(e.ram+TRACK+0x5bc,&bits,2);
   memset(e.r,0,sizeof e.r);e.r[4].uw[0]=TRACK;CHECK(call(&e,0x35beb0));CHECK(gt4_pit_refueling(bits,timer)==(int)e.r[2].sw[0]);
  }
 }
 for(int stage=-2;stage<=8;stage++)for(unsigned role=0;role<6;role++)for(unsigned refuel=0;refuel<2;refuel++)for(unsigned ready=0;ready<2;ready++){
  memset(e.r,0,sizeof e.r);e.r[4].sd[0]=stage;e.r[5].uw[0]=role==4?refuel:ready;e.r[6].uw[0]=ready;
  CHECK(call(&e,role<4?0x3cf6f0:role==4?0x3cf730:0x3cf780));
  if(gt4_pit_role_control(role,stage,refuel,ready)!=e.r[2].sw[0])fprintf(stderr,"stage %d role %u refuel %u ready %u native %d EE %d\n",stage,role,refuel,ready,gt4_pit_role_control(role,stage,refuel,ready),e.r[2].sw[0]);
  CHECK(gt4_pit_role_control(role,stage,refuel,ready)==e.r[2].sw[0]);
 }
 static const char *const names[]={"jack_up","wheel_gun","wheel_nut","tire_sound2","tire_sound1","jack_down","release","tire_end","refuel"};
 static const float blends[]={-2,-.5f,0,.25f,.5f,.999f,1,2};
 for(unsigned bank=0;bank<2;bank++){
  unsigned offset=word(d+4+bank*4),root=BASE+offset;const uint8_t*b=d+offset;
  memset(e.r,0,sizeof e.r);e.r[4].uw[0]=root;CHECK(call(&e,0x427698));
  for(named=0;named<half(b+22);named++){
   const uint8_t*r=b+word(b+28)+named*16;unsigned slot=bank?6:named%6;
   CHECK(gt4_license_motion_select(m,slot,(char*)b+word(r)));
   for(int control=-1;control<5;control++){
    put(&e,TRACK+96,0);memset(e.r,0,sizeof e.r);e.r[4].uw[0]=root;e.r[5].uw[0]=named;e.r[6].sd[0]=control;e.r[7].uw[0]=TRACK+96;
    CHECK(call(&e,0x429ad8));int ok=gt4_license_motion_seek_control(m,slot,control);
    CHECK(ok==e.r[2].sw[0]);if(ok){float time=gt4_license_motion_time(m,slot);CHECK(!memcmp(&time,e.ram+TRACK+96,4));}
   }
   CHECK(gt4_license_motion_select(m,slot,(char*)b+word(r)));
   memset(e.ram+TRACK,0,24);put(&e,TRACK+8,named);put(&e,TRACK+16,1);
   for(tick=0;tick<1800;tick++){
    float old=gt4_license_motion_time(m,slot),dt=tick%3==0?1.f/120:tick%3==1?1.f/60:1.f/30;
    int control=(int)(tick/150)%5-1;put(&e,TRACK+20,(unsigned)control);
    memset(e.r,0,sizeof e.r);e.r[4].uw[0]=root;e.r[5].uw[0]=TRACK;memcpy(e.ram+TRACK+4,&dt,4);
    CHECK(call(&e,0x429bc0));CHECK(gt4_license_motion_step_control(m,slot,dt,control));
    float time=gt4_license_motion_time(m,slot);CHECK(!memcmp(&time,e.ram+TRACK,4));
    const char*label=NULL;float begin,end;memset(e.r,0,sizeof e.r);
    e.r[4].uw[0]=TRACK+128;e.r[5].uw[0]=root;e.r[6].uw[0]=named;e.fpr[12].f=time;
    CHECK(call(&e,0x429e10));int interval=gt4_license_motion_interval(m,slot,&label,&begin,&end);CHECK(interval==e.r[2].uw[0]);
    if(interval){unsigned pointer=word(e.ram+TRACK+128);CHECK((label!=NULL)==(pointer!=0));if(label)CHECK(!strcmp(label,(char*)e.ram+pointer));CHECK(!memcmp(&begin,e.ram+TRACK+132,4));CHECK(!memcmp(&end,e.ram+TRACK+136,4));CHECK(gt4_license_motion_targets(m,slot)==word(e.ram+TRACK+140));}
    if((int16_t)half(r+6)<0){
     float blend=blends[tick%8],native_time,native_weight;unsigned g,a[2];
     CHECK(gt4_license_motion_sample_blend(m,slot,blend,&g,a,&native_time,&native_weight));
     memset(e.r,0,sizeof e.r);sampled=0;e.r[4].uw[0]=root+word(b+16)+g*16;e.r[5].uw[0]=half(r+8);
     e.fpr[12].f=time;e.fpr[13].f=blend;CHECK(call(&e,0x42c348));CHECK(sampled==1);
     CHECK(a[0]==atoms[0]);if(native_weight!=0)CHECK(a[1]==atoms[1]);
     CHECK(!memcmp(&native_time,&local,4));CHECK(!memcmp(&native_weight,&weight,4));
    }
    for(unsigned k=0;k<sizeof names/sizeof *names;k++){
     float native=gt4_license_motion_mark(m,slot,names[k],old,time,dt);
     strcpy((char*)e.ram+TRACK+64,names[k]);put(&e,TRACK+32+4,root);
     memset(e.r,0,sizeof e.r);e.r[4].uw[0]=TRACK+32;e.r[5].uw[0]=named;e.r[6].uw[0]=TRACK+64;
     e.fpr[12].f=old;e.fpr[13].f=time;e.fpr[14].f=dt;CHECK(call(&e,0x429740));
     if(argc==4 && !strcmp(argv[3],"--negative") && named==0 && tick==200 && k==0)native+=1;
     if(memcmp(&native,&e.fpr[0].f,4))fprintf(stderr,"mark %s old %.9g now %.9g native %.9g EE %.9g\n",names[k],old,time,native,e.fpr[0].f);
     CHECK(!memcmp(&native,&e.fpr[0].f,4));marks++;
     memset(e.r,0,sizeof e.r);e.r[4].uw[0]=root;e.r[5].uw[0]=named;e.r[6].uw[0]=TRACK+64;e.fpr[12].f=time;
     CHECK(call(&e,0x429d20));CHECK(gt4_license_motion_passed(m,slot,names[k])==e.r[2].sw[0]);
    }cases++;
   }
  }
 }
 gt4_license_motion_close(m);free(d);}
 printf("PASS %u authored pit clocks/multi-target dispatches, %u inclusive event marks, both packages.\n",cases,marks);
 gt4_vol_close(v);ee_free(&e);return 0;
}
