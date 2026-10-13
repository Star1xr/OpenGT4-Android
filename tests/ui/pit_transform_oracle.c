/* Original 003D1488 wheel matrix stack and 003CF938 cable integrator.
   Graphics stack/trig are explicit PC boundaries; original getters execute. */
#include "ee.h"
#include "race_pit_crew.h"
#include "physics/car.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fenv.h>
ee_fn recomp_lookup(uint32_t);
#define PIT 0x1800000u
#define CAR 0x1810000u
#define PARAM 0x1820000u
#define MARK 0x1830000u
#define CHECK(x) do{if(!(x)){fprintf(stderr,"pit transform line %d case %u: %s\n",__LINE__,cases,#x);return 1;}}while(0)
static unsigned cases;static float matrix[16],box[4];
static void identity(float m[16]){memset(m,0,64);m[0]=m[5]=m[10]=m[15]=1;}
static void product(const float n[16]){float o[16];for(unsigned c=0;c<4;c++)for(unsigned r=0;r<4;r++)o[c*4+r]=((matrix[r]*n[c*4]+matrix[4+r]*n[c*4+1])+matrix[8+r]*n[c*4+2])+matrix[12+r]*n[c*4+3];memcpy(matrix,o,64);}
static void translation(ee_ctx*c){float n[16];identity(n);for(unsigned i=0;i<3;i++)n[12+i]=c->fpr[12+i].f;product(n);}
static void rotation(ee_ctx*c,unsigned axis){float n[16];identity(n);float rad=(c->fpr[12].f*gt4_f32(0x40490fda))/180,s=sinf(rad),co=cosf(rad);unsigned a=(axis+1)%3,b=(axis+2)%3;n[a*4+a]=n[b*4+b]=co;n[a*4+b]=s;n[b*4+a]=-s;product(n);}
static void x(ee_ctx*c){rotation(c,0);}static void y(ee_ctx*c){rotation(c,1);}static void z(ee_ctx*c){rotation(c,2);}
static void multiply(ee_ctx*c){float n[16];memcpy(n,c->ram+c->r[4].uw[0],64);product(n);}
static void car(ee_ctx*c){c->r[2].uw[0]=CAR;}
static void zero(ee_ctx*c){c->r[2].uw[0]=0;}
static void nop(ee_ctx*c){(void)c;}
static void marker(ee_ctx*c){c->r[2].uw[0]=MARK;}
static void point(ee_ctx*c){GT4_F32(c->r[6].uw[0])=box[0];GT4_F32(c->r[6].uw[0]+4)=-box[2];GT4_F32(c->r[6].uw[0]+8)=box[1];GT4_F32(c->r[7].uw[0]+8)=box[3];}
static void sine(ee_ctx*c){GT4_F32(c->r[4].uw[0])=sinf(c->fpr[12].f);GT4_F32(c->r[5].uw[0])=cosf(c->fpr[12].f);}
static ee_fn boundary(uint32_t p){switch(p){case 0x3cdb20:case 0x3ce930:case 0x3cdfd8:case 0x3ce4a0:return nop;case 0x3d3030:return zero;case 0x3c9f00:return nop;case 0x3c9f18:return marker;case 0x34c190:return car;case 0x46bec0:return point;case 0x57d8a0:return sine;case 0x4a7844:return translation;case 0x4a79b0:return y;case 0x4a7988:return z;case 0x4a79d8:return x;case 0x4a7698:return multiply;default:return NULL;}}
static ee_fn lookup(uint32_t p){ee_fn f=boundary(p);return f?f:recomp_lookup(p);}
static int call(ee_ctx*c,unsigned pc){c->r[29].uw[0]=0x1ef0000;jmp_buf j;c->exit_jmp=&j;if(setjmp(j)){fprintf(stderr,"EE %08x %s\n",c->end_pc,c->end_detail);return 0;}recomp_lookup(pc)(c);return 1;}
int main(int argc,char**argv){unsigned cases=0;CHECK(argc==2 || argc==3);
 int negative=argc==3 && !strcmp(argv[2],"--negative");
 int parent_negative=argc==3 && !strcmp(argv[2],"--negative-parent");
 int mounted_negative=argc==3 && !strcmp(argv[2],"--negative-mounted");
 int root_negative=argc==3 && !strcmp(argv[2],"--negative-root");
 int draw_negative=argc==3 && !strcmp(argv[2],"--negative-draw");unsigned drawn_cases=0;
 ee_ctx e;CHECK(!ee_init(&e));CHECK(ee_load_image(&e,argv[1],0x100000)>0x5a3900);e.lookup=lookup;e.hook=boundary;gt4_phys_mem=e.ram;
 GT4_U32(CAR+0x10)=PARAM;GT4_F32(PARAM+0x18)=2.448f;GT4_F32(PARAM+0x2d4)=.177f;GT4_F32(PARAM+0x2d8)=.203f;GT4_F32(PARAM+0x314)=.28f;GT4_F32(PARAM+0x36c)=.32f;
 float maximum=0;fenv_t saved;fegetenv(&saved);gt4_phys_fpu_enter();
 for(unsigned layout=0;layout<13;layout++)for(unsigned stage=1;stage<6;stage++)for(unsigned role=0;role<4;role++)for(unsigned repl=0;repl<2;repl++)for(unsigned fraction=0;fraction<5;fraction++){
  gt4_pit_crew c={0};c.car=CAR;c.stage=stage;c.root[0]=-12;c.root[1]=.125f;c.root[2]=35;c.root[3]=-.8f;c.tire_scale[role]=1;
  GT4_U8(PARAM+0x30)=(uint8_t)layout;GT4_F32(CAR+0x554)=(float)fraction/4;GT4_U32(PIT+0x9684)=stage;GT4_F32(PIT+0x8f30)=c.root[3];
  for(unsigned j=0;j<3;j++){GT4_F32(CAR+0x8a4+j*4)=-(float)(j+1)*3.5f;GT4_F32(CAR+0x888+j*4)=(float)(j+1)*1.75f;GT4_F32(CAR+0x8bc+j*4)=.04f*(j+1);GT4_F32(CAR+0x66c+j*4)=-.07f*(j+1);c.old_root[j]=GT4_F32(PIT+0x8f90+j*4)=j+.75f;c.old_euler[j]=.06f*(j+1);GT4_F32(PIT+0x8f48+(j==0?0:j==1?8:4))=j==1?-c.old_euler[j]:c.old_euler[j];GT4_F32(PIT+0x90f8+j*4)=c.root[j];}
  for(unsigned w=0;w<6;w++){GT4_F32(CAR+0x900+w*4)=(w+1)*.025f;GT4_F32(CAR+0x16c+w*0xec)=-(float)(w+1)*.04f;GT4_F32(CAR+0x8d0+w*4)=(w+1)*.01f;GT4_F32(CAR+0x1f8+w*0xec)=-(float)(w+1)*.02f;GT4_F32(CAR+0x8e0+w*4)=(w+1)*.07f;GT4_F32(CAR+0x164+w*0xec)=-(float)(w+1)*.04f;GT4_F32(CAR+0x8f0+w*4)=(w+1)*.14f;GT4_F32(CAR+0x168+w*0xec)=-(float)(w+1)*.11f;GT4_F32(CAR+0x910+w*4)=(w+1)*.03f;GT4_F32(CAR+0x188+w*0xec)=-(float)(w+1)*.05f;}
  for(unsigned r=0;r<4;r++){for(unsigned j=0;j<3;j++)c.old_hub[r][j]=(r+1)*.2f*(j+1);GT4_F32(PIT+0x8fd8+r*4)=c.old_hub[r][0];GT4_F32(PIT+0x9038+r*4)=c.old_hub[r][1];GT4_F32(PIT+0x9098+r*4)=c.old_hub[r][2];
   static const unsigned maps[4][4]={{0,1,2,3},{0,0,1,1},{0,0,1,2},{0,1,2,2}};unsigned kind=GT4_U8(gt4_car_layout(CAR));GT4_U32(PIT+0x9858+r*4)=kind<4?maps[kind][r]:r;}
  float marker[16],actual[16];identity(marker);marker[12]=.32f;marker[13]=.46f;marker[14]=-.885f;memcpy(e.ram+MARK,marker,64);identity(matrix);memset(e.r,0,sizeof e.r);
  e.r[4].uw[0]=PIT;e.r[5].uw[0]=0;e.r[6].uw[0]=0;e.r[7].uw[0]=MARK;e.r[8].uw[0]=role;e.r[9].uw[0]=repl;CHECK(call(&e,0x3d1488));CHECK(gt4_pit_tire_matrix(&c,role,repl,marker,actual));if(negative)actual[13]+=.125f;
  for(unsigned j=0;j<16;j++){float d=fabsf(matrix[j]-actual[j]);if(d>maximum)maximum=d;if(d>2e-5f)fprintf(stderr,"layout %u stage %u role %u new %u element %u %.9g vs %.9g\n",layout,stage,role,repl,j,actual[j],matrix[j]);CHECK(isfinite(d) && d<=2e-5f);}cases++;
  for(unsigned enabled=0;enabled<2;enabled++)for(int model=-1;model<=3;model++){
   c.active[role]=enabled;GT4_U32(PIT+0x3e4+role*0x178+0x170)=!enabled;GT4_U32(PIT+0x3e4+role*0x178+0x174)=0;GT4_U32(PIT+0xfc+role*4)=(uint32_t)model;
   c.tire_scale[role]=GT4_F32(PIT+0x91a0+role*4)=.7f+fraction*.15f;c.rotation[role][repl]=GT4_F32(PIT+(repl?0x9580:0x9520)+role*4)=(fraction-2.f)*.5f;c.camber[role]=GT4_F32(PIT+0x95e0+role*4)=.18f;
   memcpy(e.ram+MARK,marker,64);identity(matrix);memset(e.r,0,sizeof e.r);e.r[4].uw[0]=PIT;e.r[5].uw[0]=0;e.r[6].uw[0]=0;e.r[7].uw[0]=role;e.r[8].uw[0]=repl;
   CHECK(call(&e,0x3d2c48));CHECK(gt4_pit_tire_draw_matrix(&c,role,repl,model,marker,actual));if(draw_negative)actual[13]+=.125f;
   for(unsigned j=0;j<16;j++){float d=fabsf(matrix[j]-actual[j]);if(d>2e-5f)fprintf(stderr,"draw layout %u stage %u role %u new %u enable %u model %d e%u native %.9g EE %.9g\n",layout,stage,role,repl,enabled,model,j,actual[j],matrix[j]);CHECK(isfinite(d) && d<=2e-5f);}drawn_cases++;
  }
 }
 printf("PASS %u old/new wheel transforms, 13 layouts, all stages; max error %.9g, tolerance 2e-5 (graphics matrix/trig boundary).\n",cases,maximum);
 printf("PASS %u complete 003D2C48 matrices: disabled/LOD fallback, width adaptation, live/faded rotation/camber, tolerance 2e-5.\n",drawn_cases);
 for(unsigned n=0;n<10000;n++){
  float s[4];for(unsigned j=0;j<4;j++)s[j]=((float)((n*(j+17)*73)%811)-405)/80;
  GT4_F32(PIT+0x96d8)=s[0];GT4_F32(PIT+0x96dc)=s[1];GT4_F32(PIT+0x9798)=s[2];GT4_F32(PIT+0x979c)=s[3];float dt=(n%3+1)/120.f,ax=((int)(n%41)-20)*.75f,az=((int)(n%31)-15)*.6f;
  memset(e.r,0,sizeof e.r);e.r[4].uw[0]=PIT;e.fpr[12].f=dt;e.fpr[13].f=ax;e.fpr[14].f=az;CHECK(call(&e,0x3cf938));gt4_pit_cable_step(s,dt,ax,az);CHECK(!memcmp(s,e.ram+PIT+0x96d8,8));CHECK(!memcmp(s+2,e.ram+PIT+0x9798,8));cases++;
 }puts("PASS 10000 cable spring inputs bit-for-bit against 003CF938.");
 for(unsigned layout=0;layout<13;layout++)for(unsigned coord=0;coord<8;coord++)for(unsigned direction=0;direction<2;direction++)for(unsigned fraction=0;fraction<5;fraction++){
  gt4_pit_crew c={0};c.car=CAR;GT4_U8(PARAM+0x30)=(uint8_t)layout;GT4_F32(CAR+0x554)=(float)fraction/4;
  for(unsigned j=0;j<4;j++)c.box[j]=box[j]=(float)(j+1)*.325f;for(unsigned j=0;j<3;j++){c.camera_origin[j]=(float)(j+1)*1.25f;c.seat[j]=(float)(j+1)*.7f;GT4_F32(MARK+0x100+j*4)=c.camera_origin[j];GT4_F32(MARK+0x110+j*4)=c.seat[j];}
  c.camera_origin[3]=.45f;for(unsigned r=0;r<4;r++)for(unsigned j=0;j<3;j++)GT4_F32(MARK+0x120+r*12+j*4)=c.camera_hub[r][j]=.17f*(r+1)*(j+1);
  float source[3]={.7f,.5f,-.4f};memcpy(e.ram+MARK,source,12);memset(e.r,0,sizeof e.r);e.r[4].uw[0]=MARK+64;e.r[5].uw[0]=MARK;e.r[6].uw[0]=MARK+0x110;e.r[7].uw[0]=MARK+0x120;e.r[8].uw[0]=MARK+0x100;e.r[9].uw[0]=PIT;e.r[10].uw[0]=0;e.r[11].uw[0]=coord;e.fpr[12].f=.45f;GT4_U32(0x1ef0000)=direction;
  CHECK(call(&e,0x3cb2c0));float actual[3];CHECK(gt4_pit_camera_point(&c,source,coord,direction,actual)==e.r[2].sw[0]);if(negative)actual[0]+=.125f;
  for(unsigned j=0;j<3;j++){float original=GT4_F32(MARK+64+j*4),d=fabsf(actual[j]-original);if(d>2e-5f)fprintf(stderr,"camera layout %u coord %u dir %u element %u %.9g vs %.9g\n",layout,coord,direction,j,actual[j],original);CHECK(d<=2e-5f);}cases++;
 }
 /* Original mounted centres and live parent used by CarModel callbacks.
    The old frontend used initial suspension travel, losing jack height. */
 for(unsigned n=0;n<1000;n++){
  gt4_pit_crew c={0};c.car=CAR;GT4_U8(PARAM+0x30)=(uint8_t)(n%13);GT4_U32(PIT+4)=CAR;
  GT4_F32(CAR+0x554)=(n%5)*.25f;
  for(unsigned j=0;j<3;j++){GT4_F32(CAR+0x888+j*4)=(j+1)*.37f+(n%17)*.25f;GT4_F32(CAR+0x8a4+j*4)=(j+1)*-.41f-(n%19)*.1f;GT4_F32(CAR+0x66c+j*4)=(j+1)*.07f+(n%23)*.05f;GT4_F32(CAR+0x8bc+j*4)=(j+1)*-.03f-(n%29)*.02f;}
  memset(e.r,0,sizeof e.r);e.r[4].uw[0]=MARK;e.r[5].uw[0]=PIT;e.fpr[12].f=0;CHECK(call(&e,0x3d6fe0));
  memset(e.r,0,sizeof e.r);e.r[4].uw[0]=MARK+64;e.r[5].uw[0]=PIT;e.fpr[12].f=0;CHECK(call(&e,0x3d6f40));
  identity(matrix);e.fpr[12].f=GT4_F32(MARK);e.fpr[13].f=GT4_F32(MARK+4);e.fpr[14].f=GT4_F32(MARK+8);translation(&e);
  e.fpr[12].f=GT4_F32(MARK+68);y(&e);e.fpr[12].f=GT4_F32(MARK+64);z(&e);e.fpr[12].f=GT4_F32(MARK+72);x(&e);
  float actual[16];CHECK(gt4_pit_car_matrix(&c,actual));if(parent_negative)actual[13]+=.125f;
  for(unsigned j=0;j<16;j++)CHECK(fabsf(actual[j]-matrix[j])<=2e-5f);
  for(unsigned role=0;role<4;role++){
   static const unsigned maps[4][4]={{0,1,2,3},{0,0,1,1},{0,0,1,2},{0,1,2,2}};
   unsigned kind=GT4_U8(gt4_car_layout(CAR));GT4_U8(PIT+0xc+role)=(uint8_t)(kind<4?maps[kind][role]:role);
   memset(e.r,0,sizeof e.r);e.r[4].uw[0]=MARK+128;e.r[5].uw[0]=PIT;e.r[6].uw[0]=role;e.fpr[12].f=0;CHECK(call(&e,0x3d72a0));
   float installed[16];CHECK(gt4_pit_mounted_matrix(&c,role,installed));if(mounted_negative)installed[13]-=.125f;
   for(unsigned r=0;r<3;r++){float expected=matrix[12+r];for(unsigned j=0;j<3;j++)expected+=matrix[j*4+r]*GT4_F32(MARK+128+j*4);if(fabsf(expected-installed[12+r])>2e-5f)fprintf(stderr,"mounted n %u role %u axis %u expected %.9g got %.9g\n",n,role,r,expected,installed[12+r]);CHECK(fabsf(expected-installed[12+r])<=2e-5f);}
  }
 }
 puts("PASS 1000 live CarModel parent frames and 4000 mounted centres against 003D6FE0/003D6F40/003D72A0; tolerance 2e-5.");
 /* Full 003D0DF0 root lifecycle, with authored actor-placement routines
    as explicit boundaries. Cars keep moving/jacking after roots freeze. */
 unsigned roots=0;
 for(unsigned layout=0;layout<13;layout++){
  memset(gt4_ptr(PIT),0,0xa000);gt4_pit_crew c={0};gt4_driver_car geometry={0};uint32_t seed=54321098;c.car=CAR;c.placed=1;
  GT4_U8(PARAM+0x30)=(uint8_t)layout;for(unsigned j=0;j<4;j++)c.root[j]=c.box[j]=box[j]=(j+1)*.125f;
  static const unsigned maps[4][4]={{0,1,2,3},{0,0,1,1},{0,0,1,2},{0,1,2,2}};unsigned kind=GT4_U8(gt4_car_layout(CAR));
  for(unsigned role=0;role<4;role++)GT4_U32(PIT+0x9858+role*4)=kind<4?maps[kind][role]:role;
  for(unsigned frame=0;frame<2400;frame++){
   unsigned time=frame%1200;int stage=time<100?0:time<250?1:time<310?2:time<900?3:time<960?4:time<1000?5:0;
   GT4_F32(CAR+0x554)=(frame%5)*.25f;
   for(unsigned j=0;j<3;j++){GT4_F32(CAR+0x888+j*4)=(j+1)*.07f+frame*.001f;GT4_F32(CAR+0x8a4+j*4)=(j+1)*-.03f+frame*.0011f;GT4_F32(CAR+0x66c+j*4)=(j+1)*.012f+frame*.00001f;GT4_F32(CAR+0x8bc+j*4)=(j+1)*-.011f+frame*.000011f;}
   for(unsigned w=0;w<6;w++){GT4_F32(CAR+0x900+w*4)=(w+1)*.05f;GT4_F32(CAR+0x16c+w*0xec)=-(float)(w+1)*.04f;GT4_F32(CAR+0x8d0+w*4)=frame*.0001f;GT4_F32(CAR+0x1f8+w*0xec)=frame*.00012f;}
   memset(e.r,0,sizeof e.r);e.r[4].uw[0]=PIT;e.r[5].uw[0]=0;e.r[6].uw[0]=stage;e.r[7].uw[0]=PARAM+0x10000;CHECK(call(&e,0x3d0df0));
   if(!stage && c.stage)gt4_pit_crew_init(&c,NULL);c.stage=stage;if(stage)gt4_pit_crew_place(&c,CAR,0,&geometry,1,&seed);
   if(root_negative && frame==350)c.root[0]+=.125f;
   for(unsigned j=0;j<3;j++){CHECK(fabsf(c.root[j]-GT4_F32(PIT+0x90f8+j*4))<=2e-5f);if(stage>=2 && time!=250)CHECK(fabsf(c.old_root[j]-GT4_F32(PIT+0x8f90+j*4))<=2e-5f);}
   if(stage>=2 && time!=250)for(unsigned r=0;r<4;r++)for(unsigned j=0;j<3;j++){
    float original=GT4_F32(PIT+0x8fd8+r*4+j*0x60);if(fabsf(c.old_hub[r][j]-original)>2e-5f)fprintf(stderr,"root layout %u frame %u role %u axis %u native %.9g EE %.9g\n",layout,frame,r,j,c.old_hub[r][j],original);CHECK(fabsf(c.old_hub[r][j]-original)<=2e-5f);
   }
   if(stage>=4)for(unsigned j=0;j<3;j++){float expected=GT4_F32(PIT+0x8f48+(j==0?0:j==1?8:4));if(j==1)expected=-expected;CHECK(fabsf(c.old_euler[j]-expected)<=2e-5f);}
   roots++;
  }
 }printf("PASS %u root lifecycle ticks: 13 layouts, two stops, fixation, freeze and reset against original 003D0DF0; tolerance 2e-5.\n",roots);
 puts("PASS 1040 original camera coordinate transforms, all eight frames, 13 layouts and both point/direction paths, tolerance 2e-5.");fesetenv(&saved);ee_free(&e);return 0;
}
