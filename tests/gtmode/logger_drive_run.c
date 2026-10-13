/* C: native B-1 input/finish; E: each collected sample against 0011E450.
   This does not compare the complete session or Logger screen against PS2. */
#include "ee.h"
#include "fpu.h"
#include "career_license_disc.h"
#include "car.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define BANK 0x1800000u
#define SAMPLER 0x1850000u
#define CAR 0x1860000u
#define CHECK(x) do{if(!(x)){fprintf(stderr,"logger drive line %d frame %u: %s\n",__LINE__,frame,#x);return 1;}}while(0)
static uint32_t player;
static unsigned frame;
static int pad_read(void *ctx,uint32_t car,uint8_t *command,const uint8_t *event){
 (void)ctx;if(car!=player)return 0;memset(command,0,0x20);command[1]=1;command[3]=event[3];
 int16_t throttle=frame<980,brake=frame>=980;memcpy(command+8,&throttle,2);memcpy(command+10,&brake,2);return 1;
}
static int wheel(void *ctx,uint32_t car,int16_t out[2]){(void)ctx;out[0]=out[1]=0;return car==player;}
static int write_pad(void *ctx,uint32_t car,const uint8_t *data){(void)ctx;(void)data;return car==player;}
static int call(ee_ctx*c,uint32_t pc,float spacing,float length){
 memset(c->r,0,sizeof c->r);c->r[4].uw[0]=SAMPLER;c->r[5].uw[0]=BANK;c->r[6].uw[0]=BANK+0x40000;c->r[7].uw[0]=CAR;
 c->r[29].uw[0]=0x1ef0000;c->fpr[12].f=spacing;c->fpr[13].f=length;
 jmp_buf jump;c->exit_jmp=&jump;if(setjmp(jump)){fprintf(stderr,"original %08x %s\n",c->end_pc,c->end_detail);return 0;}
 recomp_lookup(pc)(c);return 1;
}
int main(int argc,char **argv){
 CHECK(argc==3);gt4_career_race_lookup_data lookup;CHECK(gt4_career_race_lookup_data_load(argv[1],&lookup));
 gt4_career_license_exam exam;char error[256];CHECK(gt4_career_license_prepare(&exam,argv[1],&lookup,"l0b0001",error,sizeof error));
 gt4_race_run *r=gt4_race_run_open(argv[1],&exam.desc,error,sizeof error);CHECK(r);
 player=gt4_race_run_car(r,0);gt4_phys_pad.read=pad_read;gt4_phys_pad.wheel_state=wheel;gt4_phys_pad.write=write_pad;
 ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[2],0x100000)>0x5a3900);c.lookup=recomp_lookup;
 uint32_t race=GT4_U32(gt4_race_run_conductor(r)),course=GT4_U32(race+0x80),data=GT4_U32(course+4),runway=GT4_U32(data+0xdc);
 ee_fpu_enter();float length=GT4_F32(runway+0x14),spacing=gt4_logger_spacing(length);
 gt4_logger_point *points=calloc(GT4_LOGGER_POINTS,sizeof *points);CHECK(points);gt4_logger_sampler s={0};
 gt4_logger_begin(&s,points,spacing,length);CHECK(call(&c,0x11e400,spacing,length));
 for(frame=0;frame<1800 && !gt4_race_run_finished(r);frame++){
  CHECK(gt4_race_run_frame(r));memcpy(c.ram+CAR,gt4_ptr(player),CAR_SIZE);
  gt4_logger_input input;gt4_logger_car_input(&input,gt4_ptr(player));gt4_logger_sample(&s,&input);
  CHECK(call(&c,0x11e450,0,0));uint32_t count;memcpy(&count,c.ram+BANK+0x40000,4);CHECK(s.count==count);
  CHECK(!memcmp(&s.origin,c.ram+SAMPLER,48));
  if(getenv("GT4_LOGGER_DRIVE_NEGATIVE") && s.count)points[0].brake^=1;
  CHECK(!memcmp(points,c.ram+BANK,GT4_LOGGER_POINTS*sizeof *points));
 }
 int32_t state;uint32_t elapsed;CHECK(gt4_race_run_license_result(r,&state,&elapsed));
 CHECK(state==3 && elapsed==10248 && s.count>0);
 printf("PASS native B-1 silver/%u ms: %u frames, %u Logger samples equal to original, maximum speed %.6f m/s.\n",elapsed,frame,s.count,s.maximum_speed);
 ee_fpu_leave();free(points);ee_free(&c);gt4_race_run_close(r);return 0;
}
