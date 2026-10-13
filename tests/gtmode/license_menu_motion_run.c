/* Course +D4 menu camera: controlled Mot1 sequence clocks, atomic dispatch
   and scalar channels against the original EE routines. Drawing is outside
   this comparison. Includes controls 1..5 and deliberate negative cases. */
#include "ee.h"
#include "license_motion.h"
#include "vol.h"
#include "career_license_disc.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define BASE 0x1900000u
#define TRACK 0x1800000u
#define CHECK(x) do{if(!(x)){fprintf(stderr,"motion line %d (%u/%u): %s\n",__LINE__,named,tick,#x);return 1;}}while(0)
static unsigned named,tick,sampled;
static uint32_t sample_group,sample_atomic;static float sample_time;
static void sample(ee_ctx*c){sampled++;sample_group=c->r[4].uw[0];sample_atomic=c->r[5].uw[0];sample_time=c->fpr[12].f;}
static ee_fn boundary(uint32_t p){return p==0x42b440?sample:NULL;}
static ee_fn lookup(uint32_t p){ee_fn f=boundary(p);return f?f:recomp_lookup(p);}
static unsigned word(const uint8_t*d){uint32_t v;memcpy(&v,d,4);return v;}
static unsigned half(const uint8_t*d){return d[0]|(unsigned)d[1]<<8;}
static void put(ee_ctx*c,unsigned p,unsigned v){memcpy(c->ram+p,&v,4);}
static int call(ee_ctx*c,uint32_t pc){c->r[29].uw[0]=0x1ef0000;jmp_buf jump;c->exit_jmp=&jump;
 if(setjmp(jump)){fprintf(stderr,"original %08x %s\n",c->end_pc,c->end_detail);return 0;}recomp_lookup(pc)(c);return 1;}
int main(int argc,char**argv){
 CHECK(argc==3);ee_ctx e;CHECK(!ee_init(&e));CHECK(ee_load_image(&e,argv[1],0x100000)>0x5a3900);
 const char *error;gt4_vol*v=gt4_vol_open(argv[2],&error);CHECK(v);size_t size;
 gt4_career_race_lookup_data spec;gt4_career_license_exam exam;char error_text[256],path[80];
 CHECK(gt4_career_race_lookup_data_load(argv[2],&spec));
 CHECK(gt4_career_license_prepare(&exam,argv[2],&spec,"l0b0001",error_text,sizeof error_text));
 snprintf(path,sizeof path,"/crs/%s",exam.desc.course);
 uint8_t *course=gt4_vol_read(v,path,&size);CHECK(course && size>0xd8);
 unsigned offset=word(course+0xd4);CHECK(offset && offset<size);
 uint8_t *d=course+offset;size-=offset;CHECK(size<0x600000);
 gt4_license_motion *m=gt4_license_motion_open_camera(d,size);CHECK(m);e.lookup=lookup;e.hook=boundary;
 memcpy(e.ram+BASE,d,size);unsigned cases=0;
 unsigned root=BASE;const uint8_t *b=d;unsigned slot=3;
 memset(e.r,0,sizeof e.r);e.r[4].uw[0]=root;CHECK(call(&e,0x427698));
  for(named=0;named<half(b+22);named++){
   const uint8_t*r=b+word(b+28)+named*16;
   CHECK(gt4_license_motion_select(m,slot,(char*)b+word(r)));
   memset(e.ram+TRACK,0,24);put(&e,TRACK+8,named);put(&e,TRACK+16,1);
   for(tick=0;tick<2400;tick++){
    float dt=tick%3==0?1.f/120:tick%3==1?1.f/60:1.f/30;
    memset(e.r,0,sizeof e.r);e.r[4].uw[0]=root;e.r[5].uw[0]=TRACK;memcpy(e.ram+TRACK+4,&dt,4);
    int control=1+(tick/180)%5;put(&e,TRACK+20,control);
    CHECK(call(&e,0x429bc0));CHECK(gt4_license_motion_step_control(m,slot,dt,control));
    float time=gt4_license_motion_time(m,slot);if(getenv("GT4_LICENSE_MENU_MOTION_NEGATIVE") && tick==200)time+=.125f;
    if(memcmp(&time,e.ram+TRACK,4)){float original;memcpy(&original,e.ram+TRACK,4);fprintf(stderr,"clock native %.9g original %.9g\n",time,original);}
    CHECK(!memcmp(&time,e.ram+TRACK,4));
    unsigned g,a;float local;CHECK(gt4_license_motion_sample(m,slot,&g,&a,&local));
    /* Direct packed motions select the atomic directly. Sequences run
       0042C348 with zero authored blend control, the application path. */
    if((int16_t)half(r+6)<0){
     memset(e.r,0,sizeof e.r);sampled=0;e.r[4].uw[0]=root+word(b+16)+g*16;e.r[5].uw[0]=half(r+8);
     e.fpr[12].f=time;e.fpr[13].f=0;CHECK(call(&e,0x42c348));CHECK(sampled==1);
     CHECK(sample_group==root+word(b+16)+g*16);CHECK(a==sample_atomic);CHECK(!memcmp(&local,&sample_time,4));
    }
    if(tick%31==0){
     const uint8_t*gr=b+word(b+16)+g*16,*ar=b+word(gr+4)+a*32;
     for(unsigned curve=0;curve<word(ar+20);curve++){
      float native;CHECK(gt4_license_motion_curve(m,slot,curve,&native));
      if(getenv("GT4_LICENSE_MENU_CURVE_NEGATIVE"))native+=.125f;
      unsigned header=word(b+word(ar+24)+curve*4);
      memset(e.r,0,sizeof e.r);e.r[4].uw[0]=root+header;e.fpr[12].f=local;CHECK(call(&e,0x42daa0));
      if(memcmp(&native,&e.fpr[0].f,4))fprintf(stderr,"curve %u t %.9g native %.9g original %.9g\n",curve,local,native,e.fpr[0].f);
      CHECK(!memcmp(&native,&e.fpr[0].f,4));
     }
    }cases++;
   }
  }
 printf("PASS %u original Mot1 clock/atomic dispatch samples and all scalar channels every 31 ticks; drawing is a boundary.\n",cases);
 gt4_license_motion_close(m);free(course);gt4_vol_close(v);ee_free(&e);return 0;
}
