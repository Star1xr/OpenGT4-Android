/* Full 003CFA98 pit presentation cycle. Service physics and graphics are
   boundaries; authored clocks, readiness, scalar channels and sound calls
   execute in the original. Inputs are from the user's local capture. */
#include "ee.h"
#include "race_pit_crew.h"
#include "physics/car.h"
#include "vol.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fenv.h>
ee_fn recomp_lookup(uint32_t);
#define PIT 0x1800000u
#define BASE 0x1900000u
static uint32_t car,bank,cambank;static float box[4];static unsigned cycle,frame,role;static gt4_pit_crew_sound heard;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"controller cycle %u frame %u role %u line %d: %s\n",cycle,frame,role,__LINE__,#x);return 1;}}while(0)
static unsigned word(const uint8_t*d){unsigned v;memcpy(&v,d,4);return v;}
static unsigned half(const uint8_t*d){return d[0]|(unsigned)d[1]<<8;}
static void nop(ee_ctx*c){(void)c;}
/* Coordinate math has its independent 003CB2C0 transform oracle; this
   test isolates CC010's take selection and origin snapshot lifecycle. */
static void coordinate(ee_ctx*c){c->r[2].uw[0]=1;}
static void getcar(ee_ctx*c){c->r[2].uw[0]=car;}
static void getbank(ee_ctx*c){c->r[2].uw[0]=bank;}
static void getcamera(ee_ctx*c){c->r[2].uw[0]=cambank;}
static void point(ee_ctx*c){GT4_F32(c->r[6].uw[0])=box[0];GT4_F32(c->r[6].uw[0]+4)=-box[2];GT4_F32(c->r[6].uw[0]+8)=box[1];GT4_F32(c->r[7].uw[0]+8)=box[3];}
static void compare(ee_ctx*c){c->r[2].sw[0]=strcmp((char*)c->ram+c->r[4].uw[0],(char*)c->ram+c->r[5].uw[0]);}
static void length(ee_ctx*c){c->r[2].uw[0]=(unsigned)strlen((char*)c->ram+c->r[4].uw[0]);}
static void clear(ee_ctx*c){memset(c->ram+c->r[4].uw[0],c->r[5].uw[0],c->r[6].uw[0]);c->r[2]=c->r[4];}
static void sound(ee_ctx*c){if(heard.count<18)heard.effect[heard.count++]=(uint8_t)c->r[4].uw[0];}
static void jack(ee_ctx*c){c->r[2].uw[0]=(GT4_U16(car+0x5bc)&0x200)!=0;}
static void sine(ee_ctx*c){GT4_F32(c->r[4].uw[0])=sinf(c->fpr[12].f);GT4_F32(c->r[5].uw[0])=cosf(c->fpr[12].f);}
/* Graphic/text calls capture the original refuelling meter's draw list. */
static gt4_pit_refuel_layout refuel;static unsigned rectangles;static int refuel_bad;
static void refuel_align(ee_ctx*c){c->fpr[0].f=c->fpr[12].f-c->fpr[13].f*.5f;}
static void refuel_background(ee_ctx*c){unsigned i=rectangles++;if(i>=3){refuel_bad=1;return;}refuel.rect[i][0]=c->fpr[12].f-c->fpr[14].f*.5f;refuel.rect[i][1]=c->fpr[13].f-c->fpr[15].f*.5f;refuel.rect[i][2]=c->fpr[14].f;refuel.rect[i][3]=c->fpr[15].f;}
static void refuel_rectangle(ee_ctx*c){unsigned i=rectangles++;if(i>=3){refuel_bad=1;return;}refuel.rect[i][0]=c->fpr[12].f;refuel.rect[i][1]=c->fpr[13].f;refuel.rect[i][2]=c->fpr[14].f-c->fpr[12].f;refuel.rect[i][3]=c->fpr[15].f-c->fpr[13].f;}
static void refuel_color(ee_ctx*c){refuel.color=c->r[4].uw[0];c->r[2]=c->r[4];}
static void refuel_position(ee_ctx*c){refuel.text[0]=c->fpr[12].f;refuel.text[1]=c->fpr[13].f;}
static void refuel_font(ee_ctx*c){refuel_bad|=strcmp((char*)c->ram+c->r[5].uw[0],"hn_20R")!=0;}
static void refuel_label(ee_ctx*c){refuel_bad|=strcmp((char*)c->ram+c->r[5].uw[0],"%d/%d")!=0;refuel.fuel=c->r[6].sw[0];refuel.capacity=c->r[7].sw[0];}
static ee_fn boundary(uint32_t p){if(p>=0x600000 && p<0x600500)return nop;switch(p){case 0x3afd18:return refuel_align;case 0x3af108:return refuel_background;case 0x3af4f8:return refuel_rectangle;case 0x3afec8:return refuel_color;case 0x44ddf8:return refuel_position;case 0x44d920:return refuel_font;case 0x44e088:return refuel_label;case 0x4aa1d8:case 0x4aa168:case 0x44de80:case 0x44d988:return nop;case 0x3cb2c0:return coordinate;case 0x34c190:return getcar;case 0x3cc808:return getbank;case 0x3cc8b0:return getcamera;case 0x46bec0:return point;case 0x393428:return sound;case 0x35be38:return jack;case 0x57f238:return compare;case 0x57f260:return length;case 0x57b3a0:return clear;case 0x57d8a0:return sine;case 0x40c130:case 0x3d5610:case 0x35bee0:return nop;default:return NULL;}}
static ee_fn lookup(uint32_t p){ee_fn f=boundary(p);return f?f:recomp_lookup(p);}
static int call(ee_ctx*c,unsigned p){c->r[29].uw[0]=0x1ef0000;jmp_buf j;c->exit_jmp=&j;if(setjmp(j)){fprintf(stderr,"EE %08x %s\n",c->end_pc,c->end_detail);return 0;}recomp_lookup(p)(c);return 1;}
int main(int argc,char**argv){
 CHECK(argc==4 || argc==5);ee_ctx e;CHECK(!ee_init(&e));FILE*f=fopen(argv[3],"rb");CHECK(f);CHECK(fread(e.ram,1,0x2000000,f)==0x2000000);fclose(f);e.lookup=lookup;e.hook=boundary;gt4_phys_mem=e.ram;
 uint32_t world=0;for(unsigned p=0x1000000;p<0x1ff0000;p+=16)if(GT4_U32(p+0x80)==0x67a078){world=p;break;}CHECK(world);uint32_t cond=GT4_U32(world+0x70);car=cond+0x6698;
 /* Execute the original draw routine over partial/overfull tanks,
    including rounding boundaries and a deliberately moved native bar. */
 for(unsigned q=0;q<10000;q++){
  float fuel=((int)(q%250)-20)*.49f,capacity=20.f+(q%151)*.37f;
  memset(e.ram+PIT,0,0x40);GT4_U8(PIT+1)=GT4_U8(PIT+2)=2;GT4_U16(PIT+4)=320;GT4_U16(PIT+6)=58;GT4_F32(PIT+12)=1;GT4_U32(PIT+16)=1;GT4_U32(PIT+24)=2;GT4_U32(PIT+28)=3;GT4_F32(PIT+32)=fuel;GT4_F32(PIT+36)=capacity;
  memset(&refuel,0,sizeof refuel);rectangles=0;refuel_bad=0;memset(e.r,0,sizeof e.r);e.r[4].uw[0]=PIT;CHECK(call(&e,0x3a59d8));CHECK(rectangles==3 && !refuel_bad);
  gt4_pit_refuel_layout native;gt4_pit_refuel_meter(fuel,capacity,&native);if(argc==5 && !strcmp(argv[4],"--negative-refuel") && !q)native.rect[1][0]+=1;
  for(unsigned i=0;i<3;i++)for(unsigned j=0;j<4;j++)CHECK(fabsf(native.rect[i][j]-refuel.rect[i][j])<=.00005f);
  CHECK(!memcmp(native.text,refuel.text,8));CHECK(native.color==refuel.color && native.fuel==refuel.fuel && native.capacity==refuel.capacity);
 }
 puts("PASS 10000 original refuelling meter draw lists, rectangles tolerance 5e-5, text coordinates/colour/labels exact.");
 /* Exhaust every ownership mask, each axle and compound, against the
    original getter. Zero ownership is a real filtered state. */
 for(unsigned bits=0;bits<8192;bits++){
  uint8_t body[0x4b8]={0};for(unsigned a=0;a<2;a++)for(unsigned k=0;k<13;k++)if(bits>>k&1){unsigned bit=0x40+a*13+k;body[0x48c+bit/8]|=1u<<(bit&7);}
  memcpy(e.ram+PIT+8,body,sizeof body);unsigned masks[2];gt4_pit_owned_tires(body,masks);
  if(argc==5 && !strcmp(argv[4],"--negative-owned") && !bits)masks[0]^=1;
  for(unsigned a=0;a<2;a++)for(unsigned k=0;k<13;k++){memset(e.r,0,sizeof e.r);e.r[4].uw[0]=PIT;e.r[5].uw[0]=0x19+a;e.r[6].uw[0]=k;CHECK(call(&e,0x43f3a8));CHECK(((masks[a]>>k)&1)==e.r[2].uw[0]);}
 }
 const char*error;gt4_vol*v=gt4_vol_open(argv[1],&error);CHECK(v);unsigned ticks=0;
 static const unsigned requests[]={0x1c0,0xc0,0x100,0x40,0x80,0,0x11,0x17,0x19,0x1f};
 for(unsigned package=1;package<=2;package++){
  size_t size;uint8_t*d=gt4_vol_read(v,package==1?"/pit/pit_001":"/pit/pit_002",&size);CHECK(d);memcpy(e.ram+BASE,d,size);bank=BASE+word(d+4);memset(e.r,0,sizeof e.r);e.r[4].uw[0]=bank;CHECK(call(&e,0x427698));
  const uint8_t*b=d+word(d+4);static const char*names[]={"tiremanLF_1seq","tiremanRF_1seq","tiremanLR_1seq","tiremanRR_1seq","fuelman_1seq","lollipopman_1seq"};
  unsigned indices[6];for(role=0;role<6;role++){indices[role]=~0u;for(unsigned i=0;i<half(b+22);i++)if(!strcmp(names[role],(char*)b+word(b+word(b+28)+i*16)))indices[role]=i;CHECK(indices[role]!=~0u);}
  gt4_license_motion*m=gt4_license_motion_open_pit(d,size);CHECK(m);
  cambank=BASE+word(d+8);memset(e.r,0,sizeof e.r);e.r[4].uw[0]=cambank;CHECK(call(&e,0x427698));
  const uint8_t*cb=d+word(d+8);unsigned camera_index=~0u;const char*camera_name="pitcam_fuel_RR_seq";
  for(unsigned i=0;i<half(cb+22);i++)if(!strcmp(camera_name,(char*)cb+word(cb+word(cb+28)+i*16)))camera_index=i;CHECK(camera_index!=~0u);
  for(unsigned services=0;services<4;services++)for(unsigned layout=0;layout<4;layout++)for(cycle=0;cycle<sizeof requests/sizeof *requests;cycle++){
   GT4_U8(GT4_U32(car+0x10)+0x30)=(uint8_t)layout;
   memset(e.ram+PIT,0,0x10000);gt4_pit_crew c={0};gt4_pit_crew_init(&c,m);c.fuel_height=.609311f;
   CHECK(gt4_license_motion_select(m,6,camera_name));GT4_U32(PIT+4+0xc8)=camera_index;GT4_U32(PIT+4+0xb0)=~0u;GT4_U32(cond+0xf8a0)=1;
   c.car=car;c.placed=1;for(unsigned j=0;j<3;j++)c.box[j]=box[j]=GT4_F32(car+0x888+j*4);box[3]=c.box[3]=0;
   GT4_F32(PIT+0x9640)=c.fuel_height;GT4_U32(PIT+0x9678)=0;GT4_U8(car+0x5b4)=1;
   for(role=0;role<6;role++)GT4_U32(PIT+0x38c4+role*0xaa4)=indices[role];
   memset(e.r,0,sizeof e.r);e.r[4].uw[0]=PIT;e.r[5].uw[0]=0;e.r[6].uw[0]=car;CHECK(call(&e,0x3cd730));
   gt4_pit_crew_service_enable(&c,services&1,services&2);
   const unsigned disables[3]={0x3d3208,0x3d3228,0x3d3248};for(unsigned i=0;i<3;i++)if(!(services&(i<2?1:2))){memset(e.r,0,sizeof e.r);e.r[4].uw[0]=PIT;e.r[5].uw[0]=0;CHECK(call(&e,disables[i]));}
   GT4_U32(PIT+4+0x28)=1;GT4_F32(PIT+4+0x128)=-1;GT4_U32(PIT+4+0x110)=~0u;
   for(frame=0;frame<980;frame++){
    GT4_U8(car+0x56b)=frame<120?2:frame<841?3:4;GT4_U8(car+0x5b5)|=0x10;
    GT4_U16(car+0x5bc)=frame<840?requests[cycle]:0;GT4_U16(car+0x5be)=frame<120 || frame>=841?0:(uint16_t)(840-frame);
    if(frame==841)GT4_U16(car+0x5bc)|=0x200;
    unsigned stage=gt4_pit_stage(car);CHECK(stage);GT4_U32(PIT+0x9684)=stage;
    for(role=0;role<6;role++)GT4_U32(PIT+0x3e4+role*0x178+0x170)=GT4_U32(PIT+0x3e4+role*0x178+0x16c);
    memset(&heard,0,sizeof heard);memset(e.r,0,sizeof e.r);e.r[4].uw[0]=PIT;e.r[5].uw[0]=0;e.r[6].uw[0]=cond;e.fpr[12].f=1.f/60;CHECK(call(&e,0x3cfa98));
    gt4_pit_crew_sound native;gt4_pit_crew_step(&c,car,1.f/60,&native);
    if(argc==5 && !strcmp(argv[4],"--negative") && services==3 && frame==200)gt4_license_motion_set_time(m,0,gt4_license_motion_time(m,0)+.125f);
    for(role=0;role<6;role++){
     float time=gt4_license_motion_time(m,role),original=GT4_F32(PIT+0x3e4+role*0x178);if(memcmp(&time,&original,4))fprintf(stderr,"clock %.9g vs %.9g stage %u\n",time,original,stage);CHECK(!memcmp(&time,&original,4));
     CHECK(c.active[role]==!(GT4_U32(PIT+0x3e4+role*0x178+0x170) || GT4_U32(PIT+0x3e4+role*0x178+0x174)));
     if(role<4){CHECK(c.tire_end[role]==GT4_U32(PIT+0x9200+role*4));CHECK(!memcmp(c.cable[role],gt4_ptr(PIT+0x96d8+role*8),8));CHECK(!memcmp(c.cable[role]+2,gt4_ptr(PIT+0x9798+role*8),8));}
    }
    CHECK(native.refueling==GT4_U32(PIT+0x98b8));CHECK(c.refuel_seen==GT4_U32(PIT+0x9260));
    if(native.count!=heard.count || memcmp(native.effect,heard.effect,heard.count)){fprintf(stderr,"sound native %u original %u\n",native.count,heard.count);for(unsigned k=0;k<native.count;k++)fprintf(stderr," n:%x",native.effect[k]);for(unsigned k=0;k<heard.count;k++)fprintf(stderr," e:%x",heard.effect[k]);fputc('\n',stderr);}CHECK(native.count==heard.count && !memcmp(native.effect,heard.effect,heard.count));
    memset(e.r,0,sizeof e.r);e.r[4].uw[0]=PIT+4;e.r[5].uw[0]=cambank;e.r[6].uw[0]=PIT;e.r[7].uw[0]=cond;e.fpr[12].f=1.f/60;CHECK(call(&e,0x3cbb28));
    gt4_pit_camera_step(&c,1.f/60);CHECK(c.camera_control==(int)GT4_U32(PIT+4+0xb0));float ct=gt4_license_motion_time(m,6);if(memcmp(&ct,gt4_ptr(PIT+4+0x80),4))fprintf(stderr,"camera clock %.9g vs %.9g control %d\n",ct,GT4_F32(PIT+4+0x80),c.camera_control);CHECK(!memcmp(&ct,gt4_ptr(PIT+4+0x80),4));
    if(c.camera_control>=0){int random=frame%4==0?-1:frame%4==1?0:frame%4==2?17:INT32_MAX;
     memset(e.r,0,sizeof e.r);e.r[4].uw[0]=PIT+4;e.r[5].uw[0]=cambank;e.r[6].uw[0]=PIT;e.r[7].uw[0]=cond;e.r[8].uw[0]=0;e.r[9].sd[0]=random;CHECK(call(&e,0x3cc010));gt4_pit_camera_pick(&c,random);
     if(argc==5 && !strcmp(argv[4],"--negative-camera") && frame==250)c.camera_choice^=1;
     if(c.camera_choice!=(int)GT4_U32(PIT+4+0xe0))fprintf(stderr,"camera choice native %d EE %d begin %.9g EE %.9g time %.9g control %d targets %u services %u\n",c.camera_choice,GT4_U32(PIT+4+0xe0),c.camera_begin,GT4_F32(PIT+4+0x128),ct,c.camera_control,gt4_license_motion_targets(m,6),services);
     CHECK(c.camera_choice==(int)GT4_U32(PIT+4+0xe0));CHECK(c.camera_begin==GT4_F32(PIT+4+0x128));CHECK(c.camera_atomic==(int)GT4_U32(PIT+4+0x110));
     CHECK(!memcmp(c.camera_origin,gt4_ptr(PIT+4+0x158),12));CHECK(c.camera_origin[3]==GT4_F32(PIT+4+0x140));
    }
    /* The render model comes from camera scalar channels, not a distance
       guessed by the port. Exercise both single and multi-car overrides. */
    for(unsigned cars=1;cars<=2;cars++){
     GT4_U32(cond+0xf8a0)=cars;GT4_U32(PIT+4+0x28)=c.camera_control>=0;GT4_U32(PIT+4+0x2c)=1;
     memset(e.r,0,sizeof e.r);e.r[4].uw[0]=PIT+4;e.r[5].uw[0]=cambank;e.r[6].uw[0]=cond;e.r[7].uw[0]=0;CHECK(call(&e,0x3cc498));
     for(role=0;role<6;role++){int model=gt4_pit_actor_model(&c,role,cars);if(argc==5 && !strcmp(argv[4],"--negative-model") && frame==200)model^=1;CHECK(model==(int)GT4_U32(PIT+4+0xf8+role*4));}
    }
    GT4_U32(cond+0xf8a0)=1;GT4_U32(PIT+4+0x28)=1;
    for(unsigned tire=0;tire<2;tire++){memset(e.r,0,sizeof e.r);e.r[4].uw[0]=car;e.r[5].uw[0]=tire;CHECK(call(&e,0x3cf7c8));CHECK(gt4_pit_tire_visible(stage,tire)==e.r[2].uw[0]);}
    GT4_U32(PIT+0x969c)=stage;ticks++;
   }
   /* Leave the lane, then reuse the same workspace for another stop.
      No service or camera state may leak into that next cycle. */
   GT4_U8(car+0x56b)=0;gt4_pit_crew_sound reset;gt4_pit_crew_step(&c,car,1.f/60,&reset);
   CHECK(!c.stage && !reset.count && !reset.refueling && c.camera_control==-1);
   CHECK(!memcmp(c.root,c.box,16));for(role=0;role<6;role++){CHECK(gt4_license_motion_time(m,role)==0);CHECK(!c.active[role] && !c.requested[role]);}
  }
  gt4_license_motion_close(m);free(d);
 }
 for(int model=-1;model<16;model++){
  memset(e.r,0,sizeof e.r);e.r[4].sd[0]=model;CHECK(call(&e,0x3c9fc8));const char*name=gt4_pit_actor_model_name(model);CHECK(name?e.r[2].uw[0] && !strcmp(name,(char*)e.ram+e.r[2].uw[0]):!e.r[2].uw[0]);
  memset(e.r,0,sizeof e.r);e.r[4].sd[0]=model;CHECK(call(&e,0x3ca038));float yaw=gt4_pit_actor_model_heading(model);CHECK(!memcmp(&yaw,&e.fpr[0].f,4));
  memset(e.r,0,sizeof e.r);e.r[4].sd[0]=model;e.r[5].uw[0]=PIT;e.r[6].uw[0]=PIT+4;e.r[7].uw[0]=PIT+8;CHECK(call(&e,0x3ca0c8));float point[3];gt4_pit_actor_cable_end(model,point);CHECK(!memcmp(point,e.ram+PIT,12));
 }
 printf("PASS %u full pit ticks, both packages, four wheel layouts, 10 requests, four service masks; original clocks, roles, cable state, flow, sounds, camera cuts, model channels and tire visibility; 212992 ownership queries.\n",ticks);gt4_vol_close(v);ee_free(&e);return 0;
}
