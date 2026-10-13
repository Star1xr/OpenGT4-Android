/* CameraShot 0037BF00/0037C138 against native REP4. The world position,
   course length, libm and matrix submission are explicit boundaries.
   No solver state is patched or simulated by this test. */
#include "ee.h"
#include "../../src/core/course_tvcam.c"
ee_fn recomp_lookup(uint32_t);
#define OBJECT 0x1800000u
#define TARGET 0x1801000u
#define WORLD 0x1802000u
#define RNG 0x1803000u
#define REP 0x1810000u
#define CHECK(x) do{if(!(x)){fprintf(stderr,"TV fixture %u tick %u line %d: %s\n",fixture,tick,__LINE__,#x);exit(1);}}while(0)
static unsigned fixture,tick;
static int exact=1;static float maximum,maximum_speed;
static void nop(ee_ctx*c){(void)c;}
static void length(ee_ctx*c){c->fpr[0].f=1000;}
static void copy3(ee_ctx*c){memcpy(c->ram+c->r[5].uw[0],c->ram+c->r[4].uw[0],12);}
static void add3(ee_ctx*c){float *a=(float*)(c->ram+c->r[4].uw[0]),*b=(float*)(c->ram+c->r[5].uw[0]);for(unsigned i=0;i<3;i++)a[i]+=b[i];}
static void sub3(ee_ctx*c){float *a=(float*)(c->ram+c->r[4].uw[0]),*b=(float*)(c->ram+c->r[5].uw[0]);for(unsigned i=0;i<3;i++)a[i]-=b[i];}
static void norm(ee_ctx*c){float *a=(float*)(c->ram+c->r[4].uw[0]);c->fpr[0].f=(a[0]*a[0]+a[1]*a[1])+a[2]*a[2];}
static void sine(ee_ctx*c){c->fpr[0].f=sinf(c->fpr[12].f);}
static void arctan(ee_ctx*c){c->fpr[0].f=atanf(c->fpr[12].f);}
static void atan2host(ee_ctx*c){c->fpr[0].f=atan2f(c->fpr[12].f,c->fpr[13].f);}
static ee_fn boundary(uint32_t pc){switch(pc){
 case 0x395400:return length;case 0x5f5040:return copy3;case 0x5f5230:return add3;case 0x5f5268:return sub3;case 0x5f5010:return norm;
 case 0x57d1f0:return sine;case 0x57d008:return arctan;case 0x57d240:case 0x4ac118:return atan2host;
 case 0x378cb8:case 0x4a0690:case 0x4a53f8:case 0x4a7454:case 0x4a79d8:case 0x4a7988:
 case 0x4a79b0:case 0x4a7844:case 0x4a7550:case 0x4a5400:case 0x4a06f0:return nop;
 default:return NULL;}}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
static void call(ee_ctx*c,unsigned pc){jmp_buf j;c->exit_jmp=&j;c->r[29].uw[0]=0x1ef0000;if(setjmp(j)){fprintf(stderr,"original %08x: %s\n",c->end_pc,c->end_detail);CHECK(0);}lookup(pc)(c);}
static void word(ee_ctx*c,unsigned a,unsigned v){memcpy(c->ram+a,&v,4);}
static void real(ee_ctx*c,unsigned a,float v){memcpy(c->ram+a,&v,4);}
static float get(ee_ctx*c,unsigned a){float v;memcpy(&v,c->ram+a,4);return v;}
static void put(uint8_t*p,unsigned a,unsigned v){memcpy(p+a,&v,4);}
static void putf(uint8_t*p,unsigned a,float v){memcpy(p+a,&v,4);}
static void floats_match(ee_ctx*c,const float*v,unsigned at,unsigned count){
 if(exact){if(memcmp(v,c->ram+at,count*4))for(unsigned i=0;i<count;i++)if(memcmp(v+i,c->ram+at+i*4,4))fprintf(stderr,"exact %x/%u: native %.9g original %.9g\n",at,i,v[i],get(c,at+i*4));CHECK(!memcmp(v,c->ram+at,count*4));return;}
 for(unsigned i=0;i<count;i++){float delta=fabsf(v[i]-get(c,at+i*4));if(delta>maximum)maximum=delta;
  float tolerance=.0002f;
  /* atan2 is a host boundary: finite-difference angular speed amplifies
     tiny heading errors. Cap speed error at .01 degree/s, angle at .0002. */
  if(count==5 && i==1){tolerance=.01f;if(delta>maximum_speed)maximum_speed=delta;}
  if(delta>tolerance)fprintf(stderr,"state %x/%u: native %.9g original %.9g error %.9g\n",at,i,v[i],get(c,at+i*4),delta);
  CHECK(isfinite(delta) && delta<=tolerance);}
}
static void spring_match(ee_ctx*c,const spring*s,unsigned at){floats_match(c,(const float*)s,at,9);}
static void operator_match(ee_ctx*c,const operator_spring*s,unsigned at){
 floats_match(c,(const float*)s,at,5);CHECK((unsigned)s->count==ee_r32(c,at+0x14));
 floats_match(c,s->aim,at+0x18,32);floats_match(c,s->turn,at+0x98,32);
 floats_match(c,&s->k,at+0x118,1);floats_match(c,s->wander,at+0x11c,3);
 CHECK((unsigned)s->wander_ticks==ee_r32(c,at+0x128));
}
int main(int argc,char**argv){
 CHECK(argc==2);ee_ctx e;CHECK(!ee_init(&e));CHECK(ee_load_image(&e,argv[1],0x100000)>0x5a3900);e.lookup=lookup;e.hook=boundary;
 uint8_t data[4096]={0};put(data,0xb0,0x100);memcpy(data+0x100,"REP4",4);putf(data,0x114,1000);put(data,0x120,1);put(data,0x124,0x40);
 /* Two knots per curve, including changing lens/roll and off-centre framing. */
 const unsigned fields[]={0x34,0x38,0x3c,0x40,0x44,0x48,0x4c,0x50,0x54,0x58,0x64,0x68,0x6c};
 const float from[]={-20,4,-50,0,1,0,35,.2f,.3f,1,.75f,1.25f,-4};
 const float to[]={-10,8,-25,4,3,10,65,.7f,-.3f,1,1.4f,.5f,7};
 for(unsigned i=0;i<13;i++){unsigned a=0x100+i*20;put(data,0x140+fields[i],a);put(data,0x100+a,2);putf(data,0x100+a+4,0);putf(data,0x100+a+8,from[i]);putf(data,0x100+a+12,1);putf(data,0x100+a+16,to[i]);}
 const unsigned kinds[]={0,0xc,0x10,0x10c,0x40c};unsigned samples=0;
 int saved=fegetround();CHECK(!fesetround(FE_TOWARDZERO));
 /* Identical angular inputs isolate the operator from host atan2/sine. */
 for(fixture=0;fixture<8;fixture++){
  operator_spring n;uint32_t rng=fixture*0x13579bdu+1;word(&e,RNG,rng);
  float angle=(float)fixture*40,k=(float)(fixture%3)*.75f;
  e.r[4].uw[0]=OBJECT;e.r[5].uw[0]=RNG;e.fpr[12].f=e.fpr[13].f=angle;e.fpr[14].f=k;call(&e,0x384e58);operator_init(&n,&rng,angle,k);
  operator_match(&e,&n,OBJECT);CHECK(rng==ee_r32(&e,RNG));
  for(tick=0;tick<1000;tick++){
   angle=(float)((tick*7)%360);k=(float)(tick%7)*.25f;
   e.r[4].uw[0]=OBJECT;e.r[5].uw[0]=RNG;e.r[6].uw[0]=k<=1;e.fpr[12].f=angle;e.fpr[13].f=k;call(&e,0x384ee8);
   operator_step(&n,&rng,angle,k,k<=1);
   if(getenv("GT4_TVCAM_OPERATOR_NEGATIVE") && tick==5)n.pos+=.1f;
   operator_match(&e,&n,OBJECT);CHECK(rng==ee_r32(&e,RNG));
  }
 }
 exact=0;
 for(fixture=0;fixture<20;fixture++){
  unsigned kind=kinds[fixture%5];put(data,0x140,kind);putf(data,0x144,0);putf(data,0x148,(kind&0x10)?1.5f:1000);putf(data,0x19c,5);putf(data,0x1a0,120);
  gt4_tvcam*n=gt4_tvcam_open(data,sizeof data);CHECK(n && n->rng==0x12345678u);n->rng=fixture*0x13579bdu+1;n->zoom=fixture<10?1:4;
  memset(e.ram+OBJECT,0,0x600);memcpy(e.ram+REP,data+0x100,0x500);word(&e,RNG,n->rng);
  for(unsigned i=0;i<13;i++)word(&e,REP+0x40+fields[i],REP+u32(data+0x140+fields[i]));
  word(&e,OBJECT+0x1ac,RNG);real(&e,OBJECT+0x14c,n->zoom);int frame=0;
  for(tick=0;tick<240;tick++){
   gt4_tvcam_target t={{(float)(tick%40)*.3f,1, (float)(tick%53)*.2f},(kind&0x10)?0:(float)(tick%240)*3};
   if(n->shot<0){
    /* 003748E0 draws even when the constructor pins kind 400 to one. */
    e.r[4].uw[0]=RNG;call(&e,0x578598);float scale=e.fpr[0].f;
    e.r[4].uw[0]=OBJECT;e.r[6].uw[0]=WORLD;e.r[7].uw[0]=TARGET;e.r[8].uw[0]=REP+0x40;e.r[9].uw[0]=0;e.fpr[12].f=scale;
    call(&e,0x37bf00);frame=0;
   }
   word(&e,OBJECT+0x140,++frame);memcpy(e.ram+TARGET+0x14,t.pos,12);real(&e,TARGET+0x34,t.distance);
   e.r[4].uw[0]=OBJECT;e.r[7].uw[0]=TARGET;call(&e,0x37c138);
   gt4_tvcam_view out;CHECK(gt4_tvcam_step(n,&t,&out));
   if(getenv("GT4_TVCAM_ORACLE_NEGATIVE") && tick==5)n->yaw.pos+=.1f;
   CHECK(n->rng==ee_r32(&e,RNG));operator_match(&e,&n->pitch,OBJECT+0x1b0);operator_match(&e,&n->yaw,OBJECT+0x2dc);
   for(unsigned i=0;i<4;i++)spring_match(&e,n->frame_spring+i,OBJECT+0x450+i*36);
   spring_match(&e,&n->roll,OBJECT+0x408);spring_match(&e,&n->lens,OBJECT+0x42c);spring_match(&e,&n->shake,OBJECT+0x4e0);
   CHECK(fabsf(out.fov-get(&e,OBJECT+0xc))<.0001f);CHECK(fabsf(out.roll-get(&e,OBJECT+0x3c))<.0001f);
   CHECK(fabsf(out.pitch-get(&e,OBJECT+0x34))<.0002f);CHECK(fabsf(out.heading-get(&e,OBJECT+0x38))<.0002f);
   for(unsigned i=0;i<3;i++)CHECK(fabsf(out.eye[i]+get(&e,OBJECT+0x28+i*4))<.0001f);
   CHECK((n->shot<0)==(ee_r32(&e,OBJECT+0xe8)==3));samples++;
  }gt4_tvcam_close(n);
 }
 {/* 003852A0 + 00385370: Triangle's zoom spring, on and off in turns */
  const unsigned Z=0x1804000u;float st[3]={0};unsigned steps=0;memset(e.ram+Z,0,12);
  for(unsigned k=0;k<400;k++){int on=(k/37)%2==0 || k%53==0;fixture=99;tick=k;
   e.r[4].uw[0]=Z;e.fpr[12].f=on?1.f:0.f;call(&e,0x3852a0);e.r[4].uw[0]=Z;call(&e,0x385370);float original=e.fpr[0].f;
   float z=gt4_tvcam_zoom_step(st,on),x=original*3+1;if(x<=1)x=1;if(4<=x)x=4;
   if(getenv("GT4_TVCAM_ORACLE_NEGATIVE") && k==40)st[2]+=1;
   CHECK(!memcmp(st,e.ram+Z,12));CHECK(!memcmp(&z,&x,4));steps++;}
  printf("zoom: %u exact spring steps\n",steps);}
 fesetround(saved);printf("PASS 8000 exact operator steps; %u original TV camera frames, state tolerance 0.0002, angular speed 0.01 degree/s (maximum %.9g); eye/lens/roll 0.0001; RNG/end exact. World/libm/matrix boundaries.\n",samples,maximum_speed);ee_free(&e);return 0;
}
