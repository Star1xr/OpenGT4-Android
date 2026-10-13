/* Setting builders 0034B960/0034B8E8 execute from the user's CORE.
   Record construction/SpecDB are explicit, shared input boundaries. The
   original builds its own engine/drivetrain; no native physics hook runs
   inside the oracle. Including the adapter exposes its private test RAM. */
#include "../../src/core/career_vehicle.c"
#include "ee.h"
#include "career_blob.h"
#include "career_original.h"
#include "career_parts.h"
#include <stdio.h>
#include <math.h>
ee_fn recomp_lookup(uint32_t);
#define OUT 0x1800000u
#define SUMMARY (OUT+0x1000)
#define WRAPPER (OUT+0x3000)
#define RETURN_REF (OUT+0x2000)
#define POOL (OUT+0x4000)
#define ARENA (OUT+0x8000)
#define ARGS (OUT+0x2800)
static uint32_t allocated;
static int normalized;
static void noop(ee_ctx *c){(void)c;}
static void reference(ee_ctx *c){w32(c->ram+c->r[4].uw[0],WRAPPER);}
static void full_car(ee_ctx *c){c->r[2].uw[0]=FULL;}
static void record(ee_ctx *c){memcpy(c->ram+c->r[5].uw[0],c->ram+REC,0x600);}
static void array(ee_ctx *c){w32(c->ram+c->r[4].uw[0],POOL);memset(c->ram+POOL,0,24);}
static void element(ee_ctx *c){c->r[2].uw[0]=POOL+c->r[5].uw[0]*4;}
static void allocate(ee_ctx *c){c->r[2].uw[0]=allocated;allocated+=0x900;}
static void binary_buffer(ee_ctx *c){w32(c->ram+c->r[4].uw[0],c->r[5].uw[0]);}
static void box_float(ee_ctx *c){w32(c->ram+allocated,c->fpr[12].u);w32(c->ram+c->r[4].uw[0],allocated);allocated+=4;}
static void boolean(ee_ctx *c){c->r[2].ud[0]=(uint64_t)normalized;}
static void scale(ee_ctx *c){c->fpr[0].u=r32(c->ram+c->r[4].uw[0]);}
static ee_fn boundary(uint32_t pc){switch(pc){
 case 0x13bdc0:return reference;case 0x147d80:case 0x441248:return full_car;case 0x4413c8:return record;
 case 0x2efc48:return array;case 0x2efce0:return element;case 0x575dc8:return allocate;
 case 0x200140:return binary_buffer;case 0x2f9360:return box_float;case 0x70000:return boolean;case 0x70010:return scale;
 case 0x13bd68:case 0x3285a8:case 0x3285f8:case 0x1fff30:case 0x2f7b68:case 0x2ed5c0:return noop;default:return NULL;}}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
static int fixture_reader(void *ctx,uint8_t body[0x4b8],int op,int override,gt4_setting_details *out){(void)body;(void)override;if(op!=2)return 0;*out=*(gt4_setting_details *)ctx;return 1;}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"curve line %d car=%s slot=%u: %s\n",__LINE__,label,slot,#x);return 1;}}while(0)
static float f32(const uint8_t *p){float x;memcpy(&x,p,4);return x;}
static unsigned u16(const uint8_t *p){uint16_t x;memcpy(&x,p,2);return x;}
static int call(ee_ctx *c,uint32_t pc,uint32_t b,uint32_t d,uint32_t e){
 memset(c->r,0,sizeof c->r);c->r[4].uw[0]=REC;c->r[5].uw[0]=b;c->r[6].uw[0]=d;c->r[7].uw[0]=e;c->r[29].uw[0]=0x1df0000;
 jmp_buf j;c->exit_jmp=&j;if(setjmp(j))return 0;ee_fn fn=recomp_lookup(pc);if(!fn)return 0;fn(c);return 1;
}
/* C tolerance: 2e-5 absolute + 2e-6 relative. Report worst error; deliberate
   corruption is much larger. Counts/RPM are exact integer comparisons. */
static float worst;
static int equal(float a,float b){float e=fabsf(a-b);if(e>worst)worst=e;return isfinite(a)&&isfinite(b)&&e<=2e-5f+fabsf(b)*2e-6f;}
static int part_lookup(void *ctx,uint64_t key,uint8_t *out,size_t capacity){gt4_career_vehicle *v=ctx;return v->db.row(v->db.ctx,(uint32_t)(key>>32),(uint32_t)key,0,out,capacity);}
int main(int argc,char **argv){
 const char *label="initialization";unsigned slot=0;
 fenv_t host_env;fegetenv(&host_env);unsigned wrappers=0;
 CHECK(argc==3);gt4_career_vehicle *v=gt4_career_vehicle_open(argv[2]);CHECK(v);
 ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0);c.lookup=lookup;c.hook=boundary;c.trace=stderr;
 gt4_career_part_data parts_data;CHECK(gt4_career_part_data_load(argv[2],&parts_data));
 unsigned variants=getenv("GT4_SETTING_CURVE_UPGRADES")?3:1;
 unsigned count=r32(v->labels+4),checked=0;const char *limit=getenv("GT4_SETTING_CAR_LIMIT");if(limit && count>(unsigned)atoi(limit))count=(unsigned)atoi(limit);
 for(unsigned i=0;i<count;i++){
  uint32_t id=r32(v->labels+20+i*8);label=gt4_specdb_label_for_id(v->labels,v->labels_size,id);CHECK(label);
  uint8_t unit[32]={0},body[0x4b8];CHECK(gt4_career_vehicle_create(v,label,0,0,unit,body));
  uint8_t stock[0x4b8];memcpy(stock,body,sizeof stock);char path[96];snprintf(path,sizeof path,"/specdb/GT4_US2560/CARS/%016llX",(unsigned long long)id);
  size_t part_size;uint8_t *parts=gt4_vol_read(v->vol,path,&part_size);CHECK(parts);
  for(unsigned variant=0;variant<variants;variant++){
   memcpy(body,stock,sizeof body);
   if(variant)for(slot=0;slot<3;slot++){
    w32(body+0x488,slot);uint8_t buffer[0x142]={0};
    for(unsigned type=1;type<=31;type++){
     uint64_t key=UINT64_MAX;unsigned candidates=0;
     for(unsigned stage=1;stage<=parts_data.type_max[type-1];stage++){
      uint64_t candidate=gt4_career_part_key(parts,part_size,type,stage);if(candidate==UINT64_MAX)continue;
      candidates++;if(variant==1 || ((i+slot+type+candidates)%2))key=candidate;
     }
     if(key==UINT64_MAX)continue;
     CHECK(gt4_career_install_key_only(body,key) || gt4_career_install_defaults(body,key,buffer,part_lookup,v));
    }
   }
  for(slot=0;slot<3;slot++)for(unsigned op=2;op<=3;op++){
   w32(body+0x488,slot);gt4_setting_details d={0};CHECK(gt4_career_vehicle_setting(v,body,(int)op,-1,&d));
   /* Rebuild the unmodified record, before the native parameter builder. */
   gt4_phys_mem=v->ram;gt4_phys_specdb=&v->db;gt4_phys_scratch=TMP;gt4_phys_fpu_enter();
   gt4_car_record(REC,FULL,0,&v->db);memcpy(c.ram,v->ram,RAM_SIZE);gt4_phys_mem=NULL;
   memset(c.ram+OUT,0,0x2000);memset(c.ram+0x1de0000,0,0x10000);
   CHECK(call(&c,op==2?0x34b960:0x34b8e8,OUT,op==2?SUMMARY:0,op==2?FULL+0x4a0:0));
   if(getenv("GT4_SETTING_CURVE_NEGATIVE"))d.peak_power+=1;
   if(op==2){
    const uint8_t *s=c.ram+SUMMARY;CHECK(d.count==(int16_t)u16(s+14));CHECK(d.max_rpm==u16(s+12));
    if(!equal(d.peak_power,f32(s)) || !equal(d.peak_torque,f32(s+4))){fprintf(stderr,"peaks %g/%g torque %g/%g\n",d.peak_power,f32(s),d.peak_torque,f32(s+4));CHECK(0);}
    for(int j=0;j<d.count;j++){CHECK(d.rpm[j]==(int16_t)u16(s+16+j*2));CHECK(equal(d.power[j],f32(s+0x40+j*4)));CHECK(equal(d.torque[j],f32(s+0xa0+j*4)));}
    ah_vm *vm=ah_vm_new();CHECK(vm);ah_obj *car=ah_module_new(vm,"Car",NULL,NULL,NULL);gt4_game_bind_car(vm,car);
    CHECK(gt4_career_blob_set(vm,car,"full_car_ps2",body,sizeof body));gt4_game_set_setting_reader(fixture_reader,&d);
    for(normalized=0;normalized<=1;normalized++){
     /* The original boxing/allocation boundary records both BinaryBuffers
        and four labels. Arithmetic and normalization remain original. */
     allocated=ARENA;w32(c.ram+ARGS,OUT+0x2a00);w32(c.ram+ARGS+4,OUT+0x2a10);w32(c.ram+ARGS+8,OUT+0x2a20);
     for(unsigned j=0;j<3;j++)w32(c.ram+OUT+0x2a04+j*16,OUT+0x2900);
     w32(c.ram+OUT+0x2900+0x58,0);w32(c.ram+OUT+0x2900+0x5c,0x70000);
     w32(c.ram+OUT+0x2900+0x60,0);w32(c.ram+OUT+0x2900+0x64,0x70010);
     float ps=d.peak_power+100,torque=d.peak_torque+100;memcpy(c.ram+OUT+0x2a10,&ps,4);memcpy(c.ram+OUT+0x2a20,&torque,4);
     /* Restore input before the previous original builder's auto ratios. */
     memcpy(c.ram+REC,v->ram+REC,0x600);memset(c.ram+RETURN_REF,0,4);
     memset(c.r,0,sizeof c.r);c.r[4].uw[0]=RETURN_REF;c.r[6].uw[0]=3;c.r[7].uw[0]=ARGS;c.r[29].uw[0]=0x1df0000;
     jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));recomp_lookup(0x148b60)(&c);
     fesetenv(&host_env);
     ah_val args[]={ah_int(normalized),ah_float(ps),ah_float(torque)},result;CHECK(ah_call_member(vm,car,"buildEngineCurveData",3,args,&result)>=0);
     ah_obj *a=ah_as_obj(result,AO_ARRAY);CHECK(ah_array_size(a)==6);
     for(unsigned curve=0;curve<2;curve++){
      const uint8_t *original=c.ram+r32(c.ram+POOL+curve*4);ah_obj *points=ah_as_obj(ah_array_get(a,(int)curve),AO_ARRAY);CHECK(ah_array_size(points)==(int)r32(original));
      for(int j=0;j<d.count;j++){ah_obj *xy=ah_as_obj(ah_array_get(points,j),AO_ARRAY);CHECK(ah_array_size(xy)==2);for(int k=0;k<2;k++)CHECK(equal((float)ah_num(ah_array_get(xy,k)),f32(original+4+j*8+k*4)));}
     }
     for(unsigned j=2;j<6;j++){float actual=(float)ah_num(ah_array_get(a,(int)j));if(getenv("GT4_SETTING_CURVE_LABEL_NEGATIVE") && j==2)actual+=1;CHECK(equal(actual,f32(c.ram+r32(c.ram+POOL+j*4))));}
     wrappers++;gt4_phys_fpu_enter();
    }
    gt4_game_set_setting_reader(NULL,NULL);ah_vm_free(vm);
   }else{
    const uint8_t *s=c.ram+OUT;unsigned n=r32(s);CHECK(d.chart_count==(int)n*2);float maximum=1;
    for(unsigned j=1;j<=n;j++)if(maximum<u16(s+0x18+j*2))maximum=(float)u16(s+0x18+j*2);
    for(unsigned j=1;j<=n;j++){
     if(!equal(d.chart[(j-1)*2][0],f32(s+0x2c+j*4)/100.f)){fprintf(stderr,"gear %u x %g/%g y %g/%g\n",j,d.chart[(j-1)*2][0],f32(s+0x2c+j*4)/100.f,d.chart[(j-1)*2][1],(float)u16(s+4+j*2)/maximum);for(unsigned k=0;k<0x90;k+=2)if(u16(v->ram+REC+0x834+k)!=u16(c.ram+0x1df0000-0x220+k))fprintf(stderr,"drive+%x %x/%x\n",k,u16(v->ram+REC+0x834+k),u16(c.ram+0x1df0000-0x220+k));CHECK(0);}
     CHECK(equal(d.chart[(j-1)*2+1][0],f32(s+0x54+j*4)/100.f));
     CHECK(equal(d.chart[(j-1)*2][1],(float)u16(s+4+j*2)/maximum));
     CHECK(equal(d.chart[(j-1)*2+1][1],(float)u16(s+0x18+j*2)/maximum));
    }
   }checked++;
  }}free(parts);
  if(i%50==0){printf("curves %u/%u %s\n",i+1,count,label);fflush(stdout);}
 }
 printf("PASS %u cars, %u equipment variants, %u original engine/gear builders, %u original normalization/label callbacks, worst absolute float error %.9g.\n",count,variants,checked,wrappers,worst);
 gt4_phys_mem=NULL;gt4_career_vehicle_close(v);ee_free(&c);return 0;
}
