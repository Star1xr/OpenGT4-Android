/* Career vehicle construction ports. Physics APIs are reused read-only;
   the original EE runtime is exclusively a test oracle. */
#include "career_vehicle.h"
#include "car_service.h"
#include "career_float.h"
#include "career_setting.h"
#include "engine.h"
#include "spectables.h"
#include "specdb.h"
#include "vol.h"
#include "inflate.h"
#include <stdlib.h>
#include <string.h>
#include <fenv.h>
#define RAM_SIZE (32u*1024*1024)
#define FULL 0x1e00000u
#define REC 0x1e01000u
#define BUF 0x1e04000u
#define TMP 0x1e10000u
static uint32_t r32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t r64(const uint8_t *p){return r32(p)|(uint64_t)r32(p+4)<<32;}
static void w32(uint8_t *p,uint32_t x){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(x>>(i*8));}
static void w64(uint8_t *p,uint64_t x){w32(p,(uint32_t)x);w32(p+4,(uint32_t)(x>>32));}
/* func_0043F270. Targets are read from the disc's jump table at 006A6790;
   these return constants are the original branch instructions. */
static int owned_base(unsigned type){
    if(type<1 || type>31)return -1;
    switch(GT4_U32(0x6a6790+(type-1)*4)){
    case 0x43f298:return 0;case 0x43f2a0:return 2;case 0x43f2a8:return 4;
    case 0x43f2b0:return 9;case 0x43f2b8:return 11;case 0x43f2c0:return 13;
    case 0x43f2c8:return 17;case 0x43f2d0:return 19;case 0x43f2d8:return 23;
    case 0x43f2e0:return 27;case 0x43f2e8:return 33;case 0x43f2f0:return 35;
    case 0x43f2f8:return 37;case 0x43f300:return 39;case 0x43f308:return 41;
    case 0x43f310:return 44;case 0x43f318:return 48;case 0x43f320:return 52;
    case 0x43f328:return 56;case 0x43f330:return 58;case 0x43f338:return 64;
    case 0x43f340:return 77;case 0x43f348:return 90;case 0x43f350:return 92;
    case 0x43f358:return 94;default:return -1;
    }
}
/* func_0043FA10 and 004462A0..004468E0. Key offsets, row offsets,
   signedness and type arguments below are instruction logic, not car data. */
static int owned_parts(const gt4_specdb_src *db,uint32_t setup,uint8_t owned[12]){
    static const struct {uint16_t slot;uint8_t field,type,sign;} getters[]={
        {0x20,2,1,0},{0x28,2,2,0},{0x48,0x22,11,0},{0x50,14,3,1},
        {0x60,0x3a,25,0},{0x68,0x38,26,0},{0x40,14,10,0},{0x58,2,24,0},
        {0xe0,2,4,0},{0xe8,2,5,0},{0x78,4,8,0},{0x88,6,15,0},
        {0x90,6,16,0},{0x98,6,17,0},{0xa0,6,18,0},{0xa8,8,13,0},
        {0xb0,8,14,0},{0xb8,2,22,0},{0xc0,2,21,0},{0xc8,2,23,0},
        {0xd8,6,19,0},{0xd0,6,20,0},{0xf8,4,27,0},{0x108,6,28,0}};
    memset(owned,0,12);
    for(unsigned i=0;i<sizeof getters/sizeof *getters;i++){
        uint64_t key=r64(gt4_ptr(setup+getters[i].slot));uint8_t row[256]={0};
        if(key==UINT64_MAX)continue;
        if(db->row(db->ctx,(uint32_t)(key>>32),(uint32_t)key,0,row,sizeof row)<=getters[i].field)return 0;
        int stage=getters[i].sign?(int8_t)row[getters[i].field]:row[getters[i].field];
        if(stage==-1)continue;
        int base=owned_base(getters[i].type);if(base<0)return 0;int bit=base+stage;
        if(bit<0)return 0;if(bit<96)owned[bit>>3]|=(uint8_t)(1u<<(bit&7));
    }
    unsigned stage=GT4_U8(setup+0x168);
    if(stage){int base=owned_base(31);if(base<0)return 0;unsigned bit=base+stage;if(bit<96)owned[bit>>3]|=(uint8_t)(1u<<(bit&7));}
    return 1;
}
/* func_0034F3F8, power result only. Keep the original 100-rpm sampling,
   rounding, intermediate float order, and redline cutoff. */
static float peak_stats(uint32_t engine,uint32_t wear,float *torque){
    int16_t points[24];int count=0;
    for(int i=0;i<(int16_t)GT4_U16(engine);i++){
        int rpm=(int)(GT4_F32(engine+0xc+i*4)*60.0f/gt4_f32(0x40c90fda));
        rpm=(rpm+50)/100*100;
        if(!count || points[count-1]<rpm){if(count==24)return 0;points[count++]=(int16_t)rpm;}
    }
    if(!count)return 0;
    int end=GT4_U16(engine+ENGINE_REDLINE),redline=end;if(end<points[count-1])end=points[count-1];
    float best=0,best_t=0;
    for(int rpm=points[0];rpm<=end;rpm+=100){
        float speed=(float)rpm*gt4_f32(0x3dd6774e);
        float t=gt4_engine_torque(gt4_ptr(engine),speed,gt4_ptr(wear));
        t=gt4_engine_power_cap(engine,t,speed);t=t/gt4_f32(0x411ccccc);
        float p=t*(float)rpm;p=p/gt4_f32(0x44330ccc);
        if(rpm<=redline && best<p)best=p;
        if(rpm<=redline && best_t<t)best_t=t;
    }
    if(torque)*torque=best_t;
    return best;
}
static float peak_power(uint32_t engine,uint32_t wear){return peak_stats(engine,wear,NULL);}
/* func_00364868 / 00364818 / 00434FD0. */
static void compact_ratings(const gt4_specdb_src *db,uint32_t setup,uint32_t wear,uint8_t unit[32],const uint8_t generic[24]){
    gt4_surface_bumps(0);
    uint8_t settings[0x180];memcpy(settings,gt4_ptr(0x620cd8),sizeof settings);
    /* 003F3B68 takes the career record settings explicitly at 0069F780.
       The existing C API takes them through its settings global. */
    memcpy(gt4_ptr(0x620cd8),gt4_ptr(0x69f780),sizeof settings);
    gt4_car_record(REC,setup,0,db);memcpy(gt4_ptr(0x620cd8),settings,sizeof settings);
    gt4_car_params(REC+0x600,REC,0,0,0x6203c8,wear,BUF);
    float power=peak_power(REC+0x734,wear),extra=0;
    uint32_t drive=REC+0x834;
    if(GT4_U8(drive)==7){ /* 00356E08 -> 00355978: power constants */
        float a=(float)GT4_U8(0x62045a),b=(float)GT4_U8(0x62045b);
        extra=((a+a)+b)/gt4_f32(0x3f3c49ba);
    }else if(GT4_U8(drive+0x13)==4)extra=gt4_f32(0x41598a02);
    power=power+extra;power=power*100.0f;
    uint32_t p=(uint32_t)(int32_t)power&0x3ffffu;
    uint32_t weight=GT4_U16(REC+0x1648)&0x1fffu;
    if(generic[0x12]&8){p=0;weight=0;}
    uint64_t flags=r64(unit+8);
    flags=(flags&UINT64_C(0xffffe000f80001ff))|((uint64_t)p<<9)|((uint64_t)weight<<32);
    w64(unit+8,flags);
}
int gt4_career_vehicle_build(const gt4_specdb_src *db,uint64_t key,int32_t color,uint64_t mileage,
                            gt4_career_oil_data *oil,uint8_t unit[32],uint8_t body[0x4b8]){
    if(!db || !oil || color<0 || (key>>32)!=0)return 0;
    uint8_t generic[24];if(db->row(db->ctx,0,(uint32_t)key,0,generic,sizeof generic)!=24)return 0;
    memset(gt4_ptr(FULL),0,0x4c0);memset(gt4_ptr(REC),0,0x3000);memset(gt4_ptr(BUF),0,0xc000);
    /* 0043E240 / 00440EC8 / 00441298, then explicit 00440FB8 wear. */
    for(unsigned i=0;i<3;i++){
        uint32_t setup=FULL+8+i*0x178;
        if(!gt4_garage_new(setup,key,db,TMP))return 0;
        GT4_U8(setup+0x166)=generic[0x11]>=100;
        uint64_t variation=r64(gt4_ptr(setup+0x10));
        int colors=db->row(db->ctx,(uint32_t)(variation>>32),(uint32_t)variation,0,NULL,0);
        /* Repeated CAR_VARIATION entries of this key are the color count. */
        unsigned count=0;while(db->row(db->ctx,(uint32_t)(variation>>32),(uint32_t)variation,count,NULL,0)>0)count++;
        if(colors>0 && color<(int)count)GT4_U8(setup+0x18)=(uint8_t)color;
    }
    memset(gt4_ptr(FULL+0x470),255,32);
    if(!owned_parts(db,FULL+8,gt4_ptr(FULL+0x494)))return 0;
    gt4_career_car_wear_init(gt4_ptr(FULL+0x4a0),mileage,generic[0x11]>=100,oil);
    memcpy(gt4_ptr(0x620320),oil->params,36);memcpy(gt4_ptr(0x620cd8),oil->settings,sizeof oil->settings);
    memcpy(gt4_ptr(0x620b18),oil->clutch_maximum,4);
    /* 00434D80 / 00434E38: keep reserved unit bits and the history high word. */
    uint64_t flags=r64(unit+8)&UINT64_C(0xffffe000f8000000);
    flags|=1u|(((uint64_t)(uint32_t)color&127u)<<2);w64(unit,key);w64(unit+8,flags);
    w32(unit+16,r32(unit+16)&0xff000000u);w32(unit+20,(uint32_t)mileage);
    w64(unit+24,(r64(unit+24)&UINT64_C(0xffffffff000003ff))|1023u);
    compact_ratings(db,FULL+8,FULL+0x4a0,unit,generic);
    memcpy(body,gt4_ptr(FULL+8),0x4b8);return 1;
}
/* func_00579330 / 00578598 / 005792F0. Private seeded RNG: the
   makeUsed operation never consumes the global menu RNG. */
static float used_random(uint32_t *seed){
    uint32_t x=*seed*17u+17u;*seed=x;uint32_t r=x^((x<<16)|(x>>16));if(!r)return 0;
    uint32_t sign=(r>>31)?UINT32_MAX:0,y=r^sign,lead=0;
    while(lead<32 && !(y&(0x80000000u>>lead)))lead++;
    uint32_t lz=((lead-1)&31)|sign,e=125u-lz,m=(r<<((0u-e)&31))>>9;
    return gt4_f32((e<<23)|m);
}
int gt4_career_vehicle_used_choices(const gt4_specdb_src *db,uint64_t key,int32_t seed,int32_t *color,uint64_t *mileage){
    /* func_00440FB8: color selection precedes age-based meter selection.
       Original age ranges are read from CORE.GT4 at runtime, not copied. */
    uint32_t rng=(uint32_t)seed;
    if(*color<0){
        if(!gt4_garage_new(TMP+0x1000,key,db,TMP+0x2000))return 0;
        uint64_t variation=r64(gt4_ptr(TMP+0x1010));unsigned count=0;
        while(db->row(db->ctx,(uint32_t)(variation>>32),(uint32_t)variation,count,NULL,0)>0)count++;
        float choice=(float)count*used_random(&rng);choice=choice+0.0f;*color=(int32_t)choice;
    }
    if((int64_t)*mileage<0){
        uint8_t row[24];if(db->row(db->ctx,(uint32_t)(key>>32),(uint32_t)key,0,row,sizeof row)!=24)return 0;
        int16_t year;memcpy(&year,row+12,2);int age=(int)year-1990;if(age<0)age=0;if(age>14)age=14;
        uint32_t low=r32(gt4_ptr(0x6a6910+age*8))*1000000u,high=r32(gt4_ptr(0x6a6914+age*8))*1000000u;
        int32_t lo,hi;memcpy(&lo,&low,4);memcpy(&hi,&high,4);
        float a=gt4_f32(career_f32_from_int(lo)),b=gt4_f32(career_f32_from_int(hi));
        float range=b-a;float result=range*used_random(&rng);result=result+a;
        *mileage=(uint64_t)(int64_t)(int32_t)result;
    }
    return 1;
}
struct gt4_career_vehicle {uint8_t *image,*ram,*labels;size_t labels_size;gt4_vol *vol;gt4_spec_tables *tables;gt4_specdb_src db;gt4_career_oil_data oil;};
static int load_image(uint8_t *ram,const char *iso){
    size_t n;uint8_t *raw=gt4_disc_read_root(iso,"CORE.GT4",&n),*decoded=NULL;int ok=0;
    if(!raw || n<6 || raw[0]!=1 || raw[1]!=1)goto done;
    uint32_t length=r32(raw+2);if(length<0x12c || length>RAM_SIZE)goto done;
    decoded=malloc(length);if(!decoded || core_inflate_raw(raw+6,n-6,decoded,length)!=length || r32(decoded+0x100)!=0x176265b3)goto done;
    for(size_t at=0x12c;at<length;){if(length-at<8)goto done;uint32_t va=r32(decoded+at),size=r32(decoded+at+4);at+=8;
        if(size>length-at || va>RAM_SIZE || size>RAM_SIZE-va)goto done;memcpy(ram+va,decoded+at,size);at+=size;}
    ok=1;
done:free(decoded);free(raw);return ok;
}
gt4_career_vehicle *gt4_career_vehicle_open(const char *iso){
    gt4_career_vehicle *v=calloc(1,sizeof *v);const char *err=NULL;if(!v)return NULL;
    v->image=calloc(1,RAM_SIZE);v->ram=malloc(RAM_SIZE);
    if(!v->image || !v->ram || !load_image(v->image,iso) || !gt4_career_oil_data_load(iso,&v->oil))goto fail;
    v->vol=gt4_vol_open(iso,&err);v->tables=v->vol?gt4_spec_tables_open(v->vol,NULL):NULL;
    if(!v->tables)goto fail;
    v->labels=gt4_vol_read(v->vol,"/specdb/GT4_US2560/GENERIC_CAR.idi",&v->labels_size);if(!v->labels)goto fail;
    v->db=(gt4_specdb_src){gt4_spec_tables_row,v->tables,gt4_spec_tables_str,gt4_spec_tables_car_parts};return v;
fail:gt4_career_vehicle_close(v);return NULL;
}
void gt4_career_vehicle_close(gt4_career_vehicle *v){if(v){gt4_spec_tables_close(v->tables);gt4_vol_close(v->vol);free(v->labels);free(v->image);free(v->ram);free(v);}}
int gt4_career_vehicle_create_seeded(void *ctx,const char *label,int32_t color,uint64_t mileage,int32_t seed,uint8_t unit[32],uint8_t body[0x4b8]){
    gt4_career_vehicle *v=ctx;uint32_t id;if(!v || !gt4_specdb_label(v->labels,v->labels_size,label,&id))return 0;
    uint8_t *old=gt4_phys_mem;const gt4_specdb_src *old_db=gt4_phys_specdb;uint32_t scratch=gt4_phys_scratch;
    fenv_t env;fegetenv(&env);memcpy(v->ram,v->image,RAM_SIZE);gt4_phys_mem=v->ram;gt4_phys_specdb=&v->db;gt4_phys_scratch=TMP;
    gt4_phys_fpu_enter();gt4_career_oil_data oil=v->oil;
    int ok=gt4_career_vehicle_used_choices(&v->db,id,seed,&color,&mileage) && gt4_career_vehicle_build(&v->db,id,color,mileage,&oil,unit,body);
    gt4_phys_mem=old;gt4_phys_specdb=old_db;gt4_phys_scratch=scratch;fesetenv(&env);return ok;
}

int gt4_career_vehicle_create(void *ctx,const char *label,int32_t color,uint64_t mileage,uint8_t unit[32],uint8_t body[0x4b8]){return gt4_career_vehicle_create_seeded(ctx,label,color,mileage,0,unit,body);}

/* func_00443240: reuse the native physics record/ratio functions without
   editing the physics implementation. Scratch and FPU context are private. */
int gt4_career_vehicle_auto_gears(void *ctx,uint8_t body[0x4b8]){
    gt4_career_vehicle *v=ctx;uint32_t slot=r32(body+0x488);if(!v || slot>=3)return 0;
    uint8_t *old=gt4_phys_mem;const gt4_specdb_src *old_db=gt4_phys_specdb;uint32_t scratch=gt4_phys_scratch;
    fenv_t env;fegetenv(&env);memcpy(v->ram,v->image,RAM_SIZE);gt4_phys_mem=v->ram;gt4_phys_specdb=&v->db;gt4_phys_scratch=TMP;gt4_phys_fpu_enter();
    uint32_t setup=FULL;memcpy(gt4_ptr(setup),body+slot*0x178,0x178);
    uint64_t key=r64(gt4_ptr(setup+0x48));uint8_t row[256];int n=v->db.row(v->db.ctx,(uint32_t)(key>>32),(uint32_t)key,0,row,sizeof row),ok=n>0x25;
    if(ok && row[0x25]){
        gt4_car_record_rows(&v->db,setup,REC);
        gt4_car_auto_ratios(REC,(int32_t)GT4_U8(setup+0x12a)*10,BUF,1,REC+0x600);
        memcpy(body+slot*0x178+0x110,gt4_ptr(BUF),24);
    }
    gt4_phys_mem=old;gt4_phys_specdb=old_db;gt4_phys_scratch=scratch;fesetenv(&env);return ok;
}
/* 00146648 +0x50..0xA4 / 00364868: garage display caches PS/torque in
   hundredths and weight in kg, using the equipped setup and engine wear. */
int gt4_career_vehicle_stats(void *ctx,const uint8_t body[0x4b8],int32_t out[3]){
    gt4_career_vehicle *v=ctx;uint32_t slot=r32(body+0x488);if(!v || slot>=3)return 0;
    uint8_t *old=gt4_phys_mem;const gt4_specdb_src *old_db=gt4_phys_specdb;uint32_t scratch=gt4_phys_scratch;
    fenv_t env;fegetenv(&env);memcpy(v->ram,v->image,RAM_SIZE);gt4_phys_mem=v->ram;gt4_phys_specdb=&v->db;gt4_phys_scratch=TMP;gt4_phys_fpu_enter();
    memcpy(gt4_ptr(FULL),body+slot*0x178,0x178);memcpy(gt4_ptr(FULL+0x4a0),body+0x498,32);
    /* 00146648 -> 00364868(car+0x4a0): the career settings (0069F780) go
       only to 003F3B68; 00361908's wear factor keeps the global ones. */
    uint8_t settings[0x180];memcpy(settings,gt4_ptr(0x620cd8),sizeof settings);
    gt4_surface_bumps(0);memcpy(gt4_ptr(0x620cd8),gt4_ptr(0x69f780),0x180);
    gt4_car_record(REC,FULL,0,&v->db);memcpy(gt4_ptr(0x620cd8),settings,sizeof settings);gt4_car_params(REC+0x600,REC,0,0,0x6203c8,FULL+0x4a0,BUF);
    float tq=0,p=peak_stats(REC+0x734,FULL+0x4a0,&tq);uint32_t drive=REC+0x834;
    if(GT4_U8(drive)==7){float a=(float)GT4_U8(0x62045a),b=(float)GT4_U8(0x62045b);p+=((a+a)+b)/gt4_f32(0x3f3c49ba);}
    else if(GT4_U8(drive+0x13)==4)p+=gt4_f32(0x41598a02);
    out[0]=(int32_t)(p*100.0f);out[1]=(int32_t)(tq*100.0f);out[2]=GT4_U16(REC+0x1648);
    uint8_t generic[24];if(v->db.row(v->db.ctx,0,r32(body),0,generic,sizeof generic)==24 && (generic[0x12]&8))memset(out,0,3*sizeof *out);
    gt4_phys_mem=old;gt4_phys_specdb=old_db;gt4_phys_scratch=scratch;fesetenv(&env);return 1;
}

/* 00156BF0..56CCC reads the stock display record, not the live race world.
   Contact x (00364C40), tire width (00364D78), signed axle z (00364C60),
   overhangs (00364F70/80). Private scratch; no physics implementation edits. */
int gt4_career_vehicle_service_geometry(void *ctx,const uint8_t body[0x4b8],float out[12]){
 gt4_career_vehicle *v=ctx;uint32_t slot=r32(body+0x488);if(!v || slot>=3)return 0;
 uint8_t *old=gt4_phys_mem;const gt4_specdb_src *old_db=gt4_phys_specdb;uint32_t scratch=gt4_phys_scratch;
 fenv_t env;fegetenv(&env);memcpy(v->ram,v->image,RAM_SIZE);gt4_phys_mem=v->ram;gt4_phys_specdb=&v->db;gt4_phys_scratch=TMP;gt4_phys_fpu_enter();
 memcpy(gt4_ptr(FULL),body+slot*0x178,0x178);memcpy(gt4_ptr(FULL+0x4a0),body+0x498,32);
 gt4_surface_bumps(0);gt4_car_record(REC,FULL,0,&v->db);gt4_car_params(REC+0x600,REC,0,0,0x6203c8,FULL+0x4a0,BUF);
 uint32_t params=REC+0x600,layout=GT4_LAYOUTS+GT4_U8(params)*84u;
 for(int j=0;j<2;j++){int i=j*2,valid=i<GT4_U8(layout+1);unsigned axle=valid?GT4_U8(layout+0x14+i):0;
  out[j*3]=gt4_wheel_contact_y(params,i,0,0);out[j*3+1]=GT4_F32(REC+0x8d4+axle*4);
  int sign=valid?(int8_t)GT4_U8(layout+0x3c+i):0;float z=GT4_F32(params+0x18)*.5f;
  out[j*3+2]=!valid || !sign?0:sign>0?-z:z;
 }
 out[6]=(float)(int16_t)GT4_U16(REC+0x12)*gt4_f32(0x3a83126e);
 out[7]=(float)(int16_t)GT4_U16(REC+0x14)*gt4_f32(0x3a83126e);
 /* 001569C0 -> 00364B30: use suspension droop, not TIRESIZE radius.
    These are display queries of the existing immutable physics output. */
 for(int i=0;i<4;i++){
  unsigned axle=i<GT4_U8(layout+1)?GT4_U8(layout+0x14+i):0;float values[5];
  memcpy(values,gt4_ptr(REC+0x908+axle*0x58),sizeof values);out[8+i]=gt4_service_drop_limit(values);
 }
 gt4_phys_mem=old;gt4_phys_specdb=old_db;gt4_phys_scratch=scratch;fesetenv(&env);return 1;
}
int gt4_career_vehicle_setting(void *ctx,uint8_t body[0x4b8],int operation,int auto_override,gt4_setting_details *out){
 gt4_career_vehicle *v=ctx;uint32_t slot=r32(body+0x488);if(!v || slot>=3 || operation<0 || operation>3)return 0;
 uint8_t *old=gt4_phys_mem;const gt4_specdb_src *old_db=gt4_phys_specdb;uint32_t scratch=gt4_phys_scratch;
 fenv_t env;fegetenv(&env);memcpy(v->ram,v->image,RAM_SIZE);gt4_phys_mem=v->ram;gt4_phys_specdb=&v->db;gt4_phys_scratch=TMP;gt4_phys_fpu_enter();
 int ok=1;uint32_t setup=FULL;memcpy(gt4_ptr(setup),body+slot*0x178,0x178);
 gt4_car_record_rows(&v->db,setup,REC);
 if(operation<=1){
  uint64_t key=r64(gt4_ptr(setup+0x48));uint8_t row[256];
  int width=v->db.row(v->db.ctx,(uint32_t)(key>>32),(uint32_t)key,0,row,sizeof row);
  /* 00445780 returns the row's count without a seven-gear UI clamp.
     NIKE ONE has eight; the authored panel still exposes seven sliders. */
  int gears=width>0x24?row[0x24]:0;ok=width>0x27 && gears<=9;
  if(ok && operation==1){
   /* 0014A328..3A8: preserve reverse, save current final ratio, enable
      record auto mode and write forward gears only. */
   int32_t final,top;gt4_setting_get(body,GT4_SETTING_GEAR,8,&final);gt4_setting_set(body,GT4_SETTING_GEAR,10,final);
   gt4_setting_get(body,GT4_SETTING_GEAR,9,&top);GT4_U8(REC+0x156)=1;
   gt4_car_auto_ratios(REC,top*10,BUF,0,REC+0x600);
   /* 0043E2F0 indexes ratios directly, including index 8. Index 8 in
      the menu accessor enum instead means Final, so do not use that enum. */
   for(int i=1;i<=gears;i++){uint16_t ratio=GT4_U16(BUF+i*2);memcpy(body+slot*0x178+0x110+i*2,&ratio,2);}
  }else if(ok){
   /* 00149538 uses two 0x20-byte signed arrays before constructing its
      23 elements (only seven editable forward gears, plus outer limits). */
   int16_t lo[16]={0},hi[16]={0},ratios[16]={0};uint16_t a,b;
   memcpy(&a,row+0x18,2);memcpy(&b,row+0x1a,2);out->gear[0]=a;out->gear[1]=b;
   out->gear[2]=row[0x26];out->gear[3]=row[0x27];out->gear[4]=gears;
   if(row[0x26]<row[0x27]){
    int32_t final,top;gt4_setting_get(body,GT4_SETTING_GEAR,10,&final);gt4_setting_get(body,GT4_SETTING_GEAR,9,&top);
    GT4_U16(REC+0x150)=(uint16_t)final;if(auto_override>=0)GT4_U8(REC+0x156)=(uint8_t)auto_override;
    gt4_car_auto_ratios(REC,top*10,BUF,0,REC+0x600);memcpy(ratios,gt4_ptr(BUF),sizeof ratios);gt4_setting_ratio_bounds(gears,ratios,lo,hi);
   }
   out->gear[5]=lo[gears];out->gear[6]=hi[1];for(int i=0;i<8;i++){out->gear[7+i*2]=lo[i];out->gear[8+i*2]=hi[i];}
  }
 }else{
  /* 0034B960 / 0034F3F8: original engine point RPM rounding, 100-RPM
     peak scan, torque/power conversion. Reuse existing engine APIs.
     setting_curve_run compares the independent original engine/drive
     builders against this display payload in all stock catalogue cars. */
  memcpy(gt4_ptr(FULL+0x4a0),body+0x498,32);gt4_surface_bumps(0);memcpy(gt4_ptr(0x620cd8),gt4_ptr(0x69f780),0x180);
  gt4_car_record(REC,FULL,0,&v->db);if(operation==3 && auto_override>=0)GT4_U8(REC+0x156)=(uint8_t)auto_override;
  gt4_car_params(REC+0x600,REC,0,0,0x6203c8,FULL+0x4a0,BUF);
  uint32_t engine=REC+0x734;int count=0;out->peak_power=peak_stats(engine,FULL+0x4a0,&out->peak_torque);out->max_rpm=GT4_U16(engine+ENGINE_REDLINE);
  /* 0034B9EC..BA18 adds 00356EA8 after sampling the engine curve.
     Electric/hybrid contribution changes the peak label/scale only. */
  uint32_t display_drive=REC+0x834;
  if(GT4_U8(display_drive)==7){float a=(float)GT4_U8(0x62045a),b=(float)GT4_U8(0x62045b);out->peak_power+=((a+a)+b)/gt4_f32(0x3f3c49ba);}
  else if(GT4_U8(display_drive+0x13)==4)out->peak_power+=gt4_f32(0x41598a02);
  if(operation==3){
   /* 0034B808 +34..D0, 00148940 +B8..1A0: forward gear endpoints,
      up to nine forward gears (0034B088..B0A0; ten includes reverse). */
   uint32_t drive=REC+0x834;unsigned n=GT4_U8(drive+2);float maximum=1;
   if(n>9)ok=0;else{
    /* 0034B6A0..750: the Setting builder uses drive_params directly;
       full race params later replaces +0x30 with converter stall RPM
       (00360780 -> 003F1F78). Rebuild display shift points from the record's raw
       ratios, before converter normalization. Physics RAM stays private. */
    float raw[10]={0};int final=(int16_t)GT4_U16(REC+0x150);
    float f=(float)(final<500?4000:final)*gt4_f32(0x3a83126e);
    for(unsigned i=1;i<=n;i++){
     int ratio=(int16_t)GT4_U16(REC+0x138+i*2);if(ratio<100)ratio=4000;
     raw[i]=(float)ratio*gt4_f32(0x3a83126e)*f;
     unsigned kind=GT4_U8(REC+0x154);int v=(int16_t)GT4_U16(REC+0x152);
     if(kind && kind!=4 && v>=500){float m=(float)v*gt4_f32(0x3a83126e);
      if(kind==1)raw[i]*=m;
      else if(kind==2 && i>=5)raw[i]*=m/f;
      else if(kind==3 && i>=3 && i<5)raw[i]*=m/f;
     }
    }
    for(unsigned i=1;i<=n;i++){unsigned r=1000;
     if(i>=2){unsigned g=i-1;
      int up=(int)((float)GT4_U16(drive+0x2e)*gt4_f32(0x3dd6774e)*raw[g+1]/raw[g]*60.f/gt4_f32(0x40c90fda));
      int lo=GT4_U16(drive+0x2c);
      if(GT4_U8(0x620dea) && g==1)up=(lo+up)/2;
      int drop=(int)(GT4_F32(drive+0x68+i*4)*gt4_f32(0x4031c71b)*60.f/gt4_f32(0x40c90fda));
      up-=drop;if(up<lo+500)up=lo+500;r=(uint16_t)up;
     }
     unsigned end=i==n?(unsigned)out->max_rpm:GT4_U16(drive+0x2e);
     float factor=GT4_F32(drive+0x68+i*4);
     out->chart[(i-1)*2][0]=((float)r*gt4_f32(0x3dd6774e)/factor)/100.f;
     out->chart[(i-1)*2+1][0]=((float)end*gt4_f32(0x3dd6774e)/factor)/100.f;
     out->chart[(i-1)*2][1]=(float)r;out->chart[(i-1)*2+1][1]=(float)end;if(maximum<end)maximum=(float)end;
    }
    for(unsigned i=0;i<n*2;i++)out->chart[i][1]/=maximum;out->chart_count=(int)n*2;
   }
  }
  for(int i=0;i<(int16_t)GT4_U16(engine);i++){
   int rpm=(int)(GT4_F32(engine+0xc+i*4)*60.f/gt4_f32(0x40c90fda));rpm=(rpm+50)/100*100;
   if(!count || out->rpm[count-1]<rpm){if(count==24){ok=0;break;}out->rpm[count++]=(float)rpm;}
  }
  out->count=count;
  for(int i=0;i<count;i++){float speed=out->rpm[i]*gt4_f32(0x3dd6774e);
   float t=gt4_engine_torque(gt4_ptr(engine),speed,gt4_ptr(FULL+0x4a0));t=gt4_engine_power_cap(engine,t,speed);t=t/gt4_f32(0x411ccccc);
   out->torque[i]=t;out->power[i]=(t*out->rpm[i])/gt4_f32(0x44330ccc);
  }
 }
 gt4_phys_mem=old;gt4_phys_specdb=old_db;gt4_phys_scratch=scratch;fesetenv(&env);return ok;
}
