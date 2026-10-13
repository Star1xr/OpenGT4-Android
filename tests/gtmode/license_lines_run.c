#include "car.h"
#include "race_run.h"
#include "vol.h"
#include "ee.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
static int original(ee_ctx *c,uint32_t pc,uint32_t lines,uint32_t course){
 memset(c->r,0,sizeof c->r);c->r[4].uw[0]=lines;c->r[5].uw[0]=course;c->r[29].uw[0]=0x1ef0000;
 jmp_buf jump;c->exit_jmp=&jump;gt4_phys_mem=c->ram;
 if(setjmp(jump)){fprintf(stderr,"lines original %08x %s\n",c->end_pc,c->end_detail);return 0;}
 recomp_lookup(pc)(c);return 1;
}
int main(int argc,char **argv){
 if(argc!=2)return 2;const char *names[]={"testcourse_100m","testcourse_200m","testcourse_500m","testcourse_1000m","fisco_slalom","tsukuba"};
 ee_ctx c;if(ee_init(&c))return 2;c.lookup=recomp_lookup;
 gt4_vol *v=gt4_vol_open(argv[1],NULL);if(!v)return 2;
 for(unsigned i=0;i<sizeof names/sizeof *names;i++){
  gt4_race_desc d={0};d.cars=1;d.car[0]=0x3b;snprintf(d.course,sizeof d.course,"%s",names[i]);char message[256];
  gt4_race_run *r=gt4_race_run_open(argv[1],&d,message,sizeof message);if(!r){fprintf(stderr,"%s\n",message);return 1;}
  uint32_t conductor=gt4_race_run_conductor(r),course=GT4_U32(conductor+0xcbd8),lines=conductor+0xda50;
  uint8_t *native=gt4_phys_mem;char path[64];snprintf(path,sizeof path,"/crs/%s.ad",names[i]);size_t n;
  uint8_t *ad=gt4_vol_read(v,path,&n);if(!ad || n<24)return 1;
  uint32_t off;memcpy(&off,ad+4,4);uint32_t address=GT4_U32(course+0x9c)-off;
  memcpy(native+address,ad,n);memcpy(c.ram,native,0x2000000);free(ad);
  gt4_phys_fpu_enter();gt4_lines_setup(lines,course);gt4_lines_clear(lines);
  if(!original(&c,0x46b0a8,lines,course) || !original(&c,0x46b9f0,lines,course))return 1;
  if(getenv("GT4_LICENSE_LINES_NEGATIVE"))native[lines+12]^=1;
  if(memcmp(native+lines,c.ram+lines,16) || memcmp(native+address,c.ram+address,n)){
   for(size_t j=0;j<n;j++)if(native[address+j]!=c.ram[address+j]){fprintf(stderr,"%s AD+%zx native %02x EE %02x\n",names[i],j,native[address+j],c.ram[address+j]);break;}
   return 1;
  }
  printf("PASS %s line preparation/clear bytes\n",names[i]);gt4_phys_mem=native;gt4_race_run_close(r);
 }
 gt4_vol_close(v);ee_free(&c);return 0;
}
