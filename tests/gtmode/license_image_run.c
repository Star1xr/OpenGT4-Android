/* Compare native LUT/coefficients with original EE code. Hardware packet
   allocation is a boundary; its uploaded CLUT and colours are inspected. */
#include "ee.h"
#include "license_image.h"
#include "license_scene.h"
#include <fenv.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define OBJ 0x1800000u
#define DATA 0x1840000u
#define PACKET 0x1880000u
#define STACK 0x1ef0000u
#define MESH (DATA+0x4000)
#define ENV (DATA+0x10000)
#define COURSE (DATA+0x20000)
#define HIT (DATA+0x30000)
#define CHECK(x) do{if(!(x)){fprintf(stderr,"license image %d: %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t colours[3];static unsigned resets,ticks;static float tick,movie_clock;
static void nop(ee_ctx*c){(void)c;}
static void allocate(ee_ctx*c){c->r[2].uw[0]=PACKET;}
static void power(ee_ctx*c){c->fpr[0].f=powf(c->fpr[12].f,c->fpr[13].f);}
static void capture(ee_ctx*c){colours[0]=c->r[11].uw[0];memcpy(colours+1,c->ram+c->r[29].uw[0],4);memcpy(colours+2,c->ram+c->r[29].uw[0]+8,4);}
static void reset(ee_ctx*c){if(c->r[5].uw[0]!=0)abort();resets++;movie_clock=0;}
static void advance(ee_ctx*c){tick=c->fpr[12].f;movie_clock+=tick;ticks++;}
static void projection(ee_ctx*c){ee_end(c,EE_END_STOP_PC,0x408674,"matrix captured");}
static void scene_class(ee_ctx*c){c->r[2].uw[0]=2;}
static void ground(ee_ctx*c){float values[2]={.7f,.8f};memcpy(c->ram+c->r[5].uw[0]+0x14,values,8);c->r[2].uw[0]=1;}
static void runway(ee_ctx*c){c->r[2].uw[0]=MESH;}
static void message(ee_ctx*c){c->r[2].uw[0]=c->r[5].uw[0]==10?1:0;}
static void cosine(ee_ctx*c){uint32_t b=0x3c8efa35;float degrees;memcpy(&degrees,&b,4);c->fpr[0].f=cosf(c->fpr[12].f*degrees);}
static ee_fn boundary(uint32_t pc){switch(pc){
 case 0x4abc90:case 0x4aa558:return allocate;case 0x57d6c0:return power;case 0x4a81a8:return capture;
 case 0x3b77e0:return message;case 0x4a5ed8:return cosine;case 0x409a48:return ground;case 0x396800:return runway;
 case 0x4a5348:return projection;case 0x5ffff8:return scene_class;
 case 0x391dc8:case 0x42e478:case 0x3b76f0:case 0x3e18d0:case 0x3c0b88:case 0x3e1fb0:return nop;
 case 0x4751d0:return reset;case 0x475488:return advance;
 case 0x49fe28:case 0x4aa6b8:case 0x4a8f78:case 0x3e05a0:case 0x3e0708:case 0x3dfaE0:case 0x5f5040:return nop;
 default:return NULL;}}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
static void word(ee_ctx*c,uint32_t a,uint32_t v){memcpy(c->ram+a,&v,4);}
static void putf(ee_ctx*c,uint32_t a,float v){memcpy(c->ram+a,&v,4);}
static int call(ee_ctx*c,uint32_t pc){
 jmp_buf jump;c->exit_jmp=&jump;c->r[29].uw[0]=STACK;
 if(setjmp(jump)){if(c->end_reason==EE_END_STOP_PC && ((pc==0x3e0ee0 && c->end_pc==0x3e0ef4) || (pc==0x4085c4 && c->end_pc==0x408674)))return 1;
  fprintf(stderr,"original stopped %08x %s\n",c->end_pc,c->end_detail);return 0;}
 CHECK(recomp_lookup(pc));recomp_lookup(pc)(c);return 1;
}
int main(int argc,char**argv){
 CHECK(argc==2);ee_ctx e;CHECK(!ee_init(&e));CHECK(ee_load_image(&e,argv[1],0x100000)>0x5a3900);e.lookup=lookup;e.hook=boundary;
 fenv_t env;fegetenv(&env);int initial_round=fegetround();CHECK(!fesetround(FE_TOWARDZERO));unsigned tables=0,mixes=0,lights=0;
 for(int brightness=-10;brightness<=10;brightness++)for(int temp=-10;temp<=10;temp+=5){
  int options[4]={brightness,0,0,temp};float p[6];gt4_license_image_options(options,p);
  memcpy(e.ram+DATA,options,16);e.r[4].uw[0]=OBJ;e.r[5].uw[0]=DATA;CHECK(call(&e,0x3efdf0));CHECK(!memcmp(p,e.ram+OBJ,24));
  uint8_t lut[256][3];gt4_license_image_lut(p,lut);
  e.r[4].uw[0]=DATA;for(int c=0;c<5;c++)e.fpr[12+c].f=p[c];e.fpr[17].f=255;CHECK(call(&e,0x4a8cb8));
  if(getenv("GT4_LICENSE_LUT_NEGATIVE"))lut[100][0]^=1;
  for(unsigned i=0;i<256;i++){unsigned index=(i&0xe7)|((i&8)<<1)|((i&16)>>1);CHECK(!memcmp(lut[index],e.ram+PACKET+96+i*3,3));}tables++;
 }
 for(int n=-200;n<=200;n++){
  float p[6]={1,1,1,0,1,n*.01f};uint8_t m[3][3];gt4_license_image_mix(p[5],m);
  for(int c=0;c<6;c++)e.fpr[12+c].f=p[c];e.fpr[18].f=255;CHECK(call(&e,0x4a7d90));
  if(getenv("GT4_LICENSE_MIX_NEGATIVE"))m[0][0]^=1;
  for(int i=0;i<3;i++)for(int c=0;c<3;c++)CHECK(m[i][c]==((colours[i]>>(c*8))&255));mixes++;
 }
 float decay=1;
 for(unsigned n=0;n<600;n++){
  putf(&e,OBJ+0x568,decay);e.r[17].uw[0]=OBJ;CHECK(call(&e,0x3e0ee0));decay=gt4_license_image_decay(decay);
  float got=decay;if(getenv("GT4_LICENSE_DECAY_NEGATIVE"))got+=.001f;CHECK(!memcmp(&got,&e.fpr[0].f,4));
 }
 for(int n=0;n<512;n++){
  float color[3]={.2f,.4f,.8f},out[3],sun=(n%16)/15.f,fade=((n>>4)%4)/3.f,level=((n>>6)%4)/3.f,blend=((n>>8)%2),exposure=.75f;
  memcpy(e.ram+DATA,color,12);putf(&e,DATA+12,1);e.r[4].uw[0]=OBJ;e.r[5].uw[0]=DATA;
  e.fpr[12].f=sun;e.fpr[13].f=fade;e.fpr[14].f=level;e.fpr[15].f=blend;e.fpr[16].f=exposure;CHECK(call(&e,0x397ff8));
  gt4_license_light_color(color,sun,fade,level,blend,exposure,out);if(getenv("GT4_LICENSE_LIGHT_NEGATIVE"))out[0]+=.001f;
  CHECK(!memcmp(out,e.ram+OBJ,12));lights++;
 }
 /* The reset event does not advance the movie. Subsequent original
    Result updates send instruction constant 3C888888 to the player. */
 word(&e,OBJ+0x10,1);word(&e,OBJ+0xc,DATA);word(&e,DATA+0x58+4,0x5f5040);word(&e,OBJ+0x5c,DATA+0x100);
 e.r[4].uw[0]=OBJ;e.r[5].uw[0]=DATA+0x1000;CHECK(call(&e,0x3dfe20));CHECK(resets==1 && ticks==0);
 word(&e,OBJ,1);word(&e,OBJ+0x54,0);
 e.r[4].uw[0]=OBJ;e.r[5].uw[0]=DATA+0x1000;e.r[6].uw[0]=0;CHECK(call(&e,0x3df978));CHECK(ticks==1);
 float expected=gt4_license_result_tick(NULL);if(getenv("GT4_LICENSE_CLOCK_NEGATIVE"))expected*=2;CHECK(!memcmp(&tick,&expected,4));
 /* Execute the real outcome handler after the real movie advance. Its
    virtual reset must win on the first outcome frame, for all five medals. */
 for(unsigned state=1;state<=5;state++){
  word(&e,OBJ+0x574,0);word(&e,DATA+0x1000+0x88,state);
  word(&e,OBJ+0x564,DATA+0x2000);word(&e,DATA+0x2000+0x64,DATA+0x3000);
  word(&e,DATA+0x3000+0x300+4,0x5ffff8);word(&e,DATA+0x38+4,0x3dfe20);
  int first=getenv("GT4_LICENSE_RESET_NEGATIVE")?0:1;float native_clock=gt4_license_result_tick(&first);
  e.r[4].uw[0]=OBJ;e.r[5].uw[0]=DATA+0x1000;CHECK(call(&e,0x3e1c28));CHECK(!memcmp(&movie_clock,&native_clock,4));
  native_clock+=gt4_license_result_tick(&first);
  e.r[4].uw[0]=OBJ;e.r[5].uw[0]=DATA+0x1000;CHECK(call(&e,0x3e1c28));CHECK(!memcmp(&movie_clock,&native_clock,4));
 }
 for(int failure=0;failure<2;failure++){
  word(&e,OBJ+0x574,0);word(&e,DATA+0x1000+0x88,(uint32_t)(-failure));
  word(&e,DATA+0x1000+0x8c,0x157529ff);e.ram[DATA+0x1000+0x906]=1;
  int first=getenv("GT4_LICENSE_RESET_NEGATIVE")?0:1;float native_clock=gt4_license_result_tick(&first);
  e.r[4].uw[0]=OBJ;e.r[5].uw[0]=DATA+0x1000;CHECK(call(&e,0x3e1c28));CHECK(!memcmp(&movie_clock,&native_clock,4));
  native_clock+=gt4_license_result_tick(&first);
  e.r[4].uw[0]=OBJ;e.r[5].uw[0]=DATA+0x1000;CHECK(call(&e,0x3e1c28));CHECK(!memcmp(&movie_clock,&native_clock,4));
 }
 for(int i=0;i<512;i++){
  float way[3]={(i%16-8)*.25f,0,((i>>4)%16-8)*.25f};
  float normal[3]={.1f, i&256?0.f:1.f,-.2f},m[16];gt4_license_shadow_projection(way,normal,m);
  if(getenv("GT4_LICENSE_SHADOW_NEGATIVE"))m[4]+=.001f;
  memcpy(e.ram+DATA+0x14,normal,12);word(&e,OBJ+0xc,DATA);
  e.r[17].uw[0]=OBJ;e.r[20].uw[0]=0;e.r[16].uw[0]=STACK+0x50;e.r[18].uw[0]=STACK+0x58;e.r[30].uw[0]=STACK+0x10;
  e.fpr[26].f=1;e.fpr[21].f=0;putf(&e,STACK+0x50,way[0]);putf(&e,STACK+0x58,way[2]);
  CHECK(call(&e,0x4085c4));CHECK(!memcmp(m,e.ram+STACK+0x10,64));
 }
 /* 00409B68: nine directional/point lights, sorted before four survive.
    Ground ray/cosine are external boundaries; sqrt/pow/cos are replaced by host libm at their call boundary. */
 gt4_phys_mem=e.ram;word(&e,COURSE+4,COURSE+0x100);word(&e,COURSE+0x100+0x80,ENV);
 word(&e,MESH+0x40,1);word(&e,MESH+0x50,9);word(&e,MESH+0x68,MESH+0x100);
 word(&e,MESH+0x6c,MESH+0x300);word(&e,MESH+0x74,MESH+0x400);word(&e,MESH+0x78,MESH+0x500);
 putf(&e,ENV+0x38,2);word(&e,HIT+0x18,0);
 for(int i=0;i<9;i++){
  uint16_t index=(uint16_t)i;memcpy(e.ram+MESH+0x300+i*2,&index,2);
  uint32_t record=MESH+0x100+i*32,source=ENV+i*128;
  word(&e,record,(unsigned)i);putf(&e,record+0x10,(float)(i-4));putf(&e,record+0x14,4);putf(&e,record+0x18,3);
  putf(&e,source+0x120,180);putf(&e,source+0x124,1);putf(&e,source+0x128,.1f);putf(&e,source+0x12c,.02f);
  for(int type=0;type<3;type++)for(int c=0;c<3;c++)putf(&e,source+0xe0+type*16+c*4,(i+type+c)*.07f);
 }
 for(int n=0;n<96;n++){
  for(int i=0;i<9;i++){
   putf(&e,MESH+0x100+i*32+0x1c,n&1?1:0);
   putf(&e,MESH+0x100+i*32+8,1);putf(&e,ENV+i*128+0x120,n%3==0?180:n%3==1?30:60);
   putf(&e,ENV+i*128+0x11c,2);
  }
  float pos[3]={(n%8-4)*.3f,.2f,(n/8-6)*.3f};gt4_ray_hit hit={0};hit.u=.7f;hit.v=.8f;hit.leaf=0;
  memset(e.ram+OBJ,0,0x160);e.r[4].uw[0]=OBJ;e.r[5].uw[0]=COURSE;e.r[6].uw[0]=HIT;
  for(int c=0;c<3;c++)e.fpr[12+c].f=pos[c];CHECK(call(&e,0x409b68));
  r3d_lights light;license_scene_lights(e.ram+ENV,MESH,&hit,pos,&light);
  if(getenv("GT4_LICENSE_POINT_NEGATIVE"))light.diffuse[0][0]+=.1f;
  CHECK(light.count==4);CHECK(*(uint32_t *)(e.ram+OBJ+8)==4);
  for(int i=0;i<4;i++){
   float *actual[4]={light.direction[i],light.ambient[i],light.diffuse[i],light.specular[i]};
   for(int type=0;type<4;type++)for(int c=0;c<3;c++){
    float value;memcpy(&value,e.ram+OBJ+12+i*80+type*16+c*4,4);
    CHECK(fabsf(actual[type][c]-value)<=.000002f);
   }
  }
 }
 gt4_phys_mem=NULL;
 fesetenv(&env);CHECK(fegetround()==initial_round);
 printf("PASS %u original CLUTs, %u channel matrices, 600 decay callbacks, %u light colours, 512 shadow matrices, 96 nine-light surface contexts and original reset/tick order.\n",tables,mixes,lights);ee_free(&e);return 0;
}
