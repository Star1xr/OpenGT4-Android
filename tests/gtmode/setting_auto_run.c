/* Original 00149538/0014A2D0 versus the native menu adapter. SpecDB row
   lookup, initial record construction and VM boxing are explicit shared
   boundaries; automatic ratios, limits and body writes execute original
   instructions. No generated original code is linked into the app. */
#include "../../src/core/career_vehicle.c"
#include "career_original.h"
#include "career_parts.h"
#include "ee.h"
#include <stdio.h>
ee_fn recomp_lookup(uint32_t);
#define CAR 0x1800000u
#define WRAP 0x1801000u
#define POOL 0x1803000u
#define ARG 0x1806000u
static gt4_career_vehicle *factory;
static unsigned pool_at;
static int override;
static const char *label;
static unsigned slot,stage;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"auto line %d %s sheet=%u stage=%u: %s\n",__LINE__,label,slot,stage,#x);return 1;}}while(0)
static void noop(ee_ctx *c){(void)c;}
static void reference(ee_ctx *c){w32(c->ram+c->r[4].uw[0],WRAP);}
static void full_car(ee_ctx *c){c->r[2].uw[0]=CAR;}
static void record(ee_ctx *c){memcpy(c->ram+c->r[5].uw[0],factory->ram+REC,0x600);}
static void row(ee_ctx *c){uint64_t key=c->r[5].ud[0];uint8_t data[256];
 int n=factory->db.row(factory->db.ctx,(uint32_t)(key>>32),(uint32_t)key,0,data,sizeof data);
 if(n<=0){ee_end(c,EE_END_BAD_ADDRESS,0,"missing original row %llx",(unsigned long long)key);return;}
 memcpy(c->ram+c->r[6].uw[0],data,(size_t)n);c->r[2].sw[0]=n;
}
static void array(ee_ctx *c){memset(c->ram+POOL,0,0x600);w32(c->ram+c->r[4].uw[0],POOL);pool_at=POOL+0x300;}
static void element(ee_ctx *c){c->r[2].uw[0]=POOL+c->r[5].uw[0]*4;}
static void integer(ee_ctx *c){w32(c->ram+pool_at,c->r[5].uw[0]);w32(c->ram+c->r[4].uw[0],pool_at);pool_at+=4;}
static void argument(ee_ctx *c){c->r[2].sw[0]=override;}
static ee_fn boundary(uint32_t pc){switch(pc){
 case 0x13bdc0:return reference;case 0x147d80:return full_car;case 0x4413c8:return record;case 0x443ed0:return row;
 case 0x2efc48:return array;case 0x2efce0:return element;case 0x2fe278:return integer;case 0x70000:return argument;
 case 0x13bd68:case 0x3285a8:case 0x3285f8:case 0x2fc870:case 0x2ed5c0:return noop;default:return NULL;}}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
static int part_lookup(void *ctx,uint64_t key,uint8_t *out,size_t capacity){gt4_career_vehicle *v=ctx;return v->db.row(v->db.ctx,(uint32_t)(key>>32),(uint32_t)key,0,out,capacity);}
static int invoke(ee_ctx *c,uint32_t pc){memset(c->r,0,sizeof c->r);c->r[4].uw[0]=ARG;c->r[5].uw[0]=ARG+4;
 c->r[6].uw[0]=override<0?0:1;c->r[7].uw[0]=ARG+8;c->r[29].uw[0]=0x1df0000;
 jmp_buf j;c->exit_jmp=&j;if(setjmp(j))return 0;recomp_lookup(pc)(c);return 1;
}
static void prepare(ee_ctx *c,const uint8_t body[0x4b8],const uint8_t gear_row[256]){
 gt4_phys_mem=factory->ram;gt4_phys_specdb=&factory->db;gt4_phys_scratch=TMP;gt4_phys_fpu_enter();
 memcpy(gt4_ptr(FULL),body+slot*0x178,0x178);gt4_car_record_rows(&factory->db,FULL,REC);
 memcpy(c->ram,factory->ram,RAM_SIZE);memcpy(c->ram+CAR+8,body,0x4b8);
 memset(c->ram+WRAP,0,0x190);w32(c->ram+WRAP+0x14,CAR);memcpy(c->ram+WRAP+0x3c,gear_row,0x2d);
 memset(c->ram+ARG,0,0x100);w32(c->ram+ARG+8,ARG+0x10);w32(c->ram+ARG+0x10+4,ARG+0x30);
 w32(c->ram+ARG+0x30+0x58,0);w32(c->ram+ARG+0x30+0x5c,0x70000);
 gt4_phys_mem=NULL;gt4_phys_specdb=NULL;
}
int main(int argc,char **argv){label="initialization";CHECK(argc==3);factory=gt4_career_vehicle_open(argv[2]);CHECK(factory);
 ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0);c.lookup=lookup;c.hook=boundary;c.trace=stderr;
 gt4_career_part_data part_data;CHECK(gt4_career_part_data_load(argv[2],&part_data));unsigned count=r32(factory->labels+4),ranges=0,autos=0;
 const char *limit=getenv("GT4_SETTING_CAR_LIMIT");if(limit && count>(unsigned)atoi(limit))count=(unsigned)atoi(limit);
 /* 0044A1E0 +0..18 indexes D_006A7C28 by type-1. An independent
    comparison protects the catalogue coverage from an index shift. */
 for(unsigned type=1;type<=32;type++){
  memset(c.r,0,sizeof c.r);c.r[4].uw[0]=type;recomp_lookup(0x44a1e0)(&c);
  unsigned maximum=part_data.type_max[type-1];if(getenv("GT4_SETTING_AUTO_STAGE_NEGATIVE") && type==1)maximum^=1;
  CHECK(maximum==c.r[2].uw[0]);
 }
 fenv_t host_env;fegetenv(&host_env);
 for(unsigned i=0;i<count;i++){
  uint32_t id=r32(factory->labels+20+i*8);label=gt4_specdb_label_for_id(factory->labels,factory->labels_size,id);CHECK(label);
  uint8_t unit[32]={0},stock[0x4b8];CHECK(gt4_career_vehicle_create(factory,label,0,0,unit,stock));
  char path[96];snprintf(path,sizeof path,"/specdb/GT4_US2560/CARS/%016llX",(unsigned long long)id);
  size_t size;uint8_t *parts=gt4_vol_read(factory->vol,path,&size);CHECK(parts);
  for(stage=0;stage<=part_data.type_max[10];stage++){
   uint64_t key=gt4_career_part_key(parts,size,11,stage);if(key==UINT64_MAX)continue;
   uint8_t gear_row[256]={0};CHECK(part_lookup(factory,key,gear_row,sizeof gear_row)>0x28);
   for(slot=0;slot<3;slot++){
    uint8_t equipped[0x4b8];memcpy(equipped,stock,sizeof equipped);w32(equipped+0x488,slot);
    uint8_t buffer[0x142]={0};CHECK(gt4_career_install_defaults(equipped,key,buffer,part_lookup,factory));
    for(unsigned test=0;test<3;test++){
     uint8_t body[0x4b8];memcpy(body,equipped,sizeof body);
     gt4_setting_set(body,GT4_SETTING_GEAR,9,test==0?gear_row[0x26]:test==1?gear_row[0x27]:(gear_row[0x26]+gear_row[0x27])/2);
     override=test==0?-1:(int)(test-1);
     gt4_setting_details d={0};int ok=gt4_career_vehicle_setting(factory,body,0,override,&d);
     if(!ok){fprintf(stderr,"unsupported gears %u\n",gear_row[0x24]);CHECK(ok);}
     prepare(&c,body,gear_row);CHECK(invoke(&c,0x149538));
     if(getenv("GT4_SETTING_AUTO_RANGE_NEGATIVE"))d.gear[6]^=1;
     for(unsigned j=0;j<23;j++){int original=(int32_t)r32(c.ram+r32(c.ram+POOL+j*4));
      if(d.gear[j]!=original)fprintf(stderr,"range[%u] native=%d original=%d override=%d\n",j,d.gear[j],original,override);CHECK(d.gear[j]==original);}
     ranges++;
     /* setting.ad GearRoot disables Auto for a fixed transmission. Its
        zero/zero limits do not define a reachable setGearAuto request. */
     if(gear_row[0x26]>=gear_row[0x27])continue;
     prepare(&c,body,gear_row);CHECK(invoke(&c,0x14a2d0));
     CHECK(gt4_career_vehicle_setting(factory,body,1,-1,&d));
     if(getenv("GT4_SETTING_AUTO_BODY_NEGATIVE"))body[slot*0x178+0x110]^=1;
     if(memcmp(body,c.ram+CAR+8,sizeof body))for(unsigned j=0;j<sizeof body;j++)if(body[j]!=c.ram[CAR+8+j])fprintf(stderr,"body+%x native=%02x original=%02x\n",j,body[j],c.ram[CAR+8+j]);
     CHECK(!memcmp(body,c.ram+CAR+8,sizeof body));autos++;
    }
   }
  }
  free(parts);if(i%50==0){printf("automatic %u/%u %s\n",i+1,count,label);fflush(stdout);}
 }
 fesetenv(&host_env);printf("PASS 32 original stage limits; %u cars: %u original 23-element gear ranges, %u complete automatic gear bodies, all available transmissions and A/B/C.\n",count,ranges,autos);
 gt4_phys_mem=NULL;gt4_phys_specdb=NULL;gt4_career_vehicle_close(factory);ee_free(&c);return 0;
}
