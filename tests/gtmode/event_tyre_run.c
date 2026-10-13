/* 003EE3C8's complete branch/axle-fitting sequence. Accessors and disc
   reads are explicit boundaries; fitted setup bytes are compared. */
#include "ee.h"
#include "fpu.h"
#include "car.h"
#include "career_parts.h"
#include "spectables.h"
#include "vol.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do {if(!(x)){fprintf(stderr,"event tyre line %d: %s\n",__LINE__,#x);return 1;}}while(0)
#define WORLD 0x1800000u
#define RACE 0x1800100u
#define ENTRIES 0x1800200u
#define SLOTS 0x1800300u
#define SETUP 0x1810020u
#define COPY 0x1820020u
static gt4_specdb_src db,forced;
static int event_type,front_type,rear_type,mode;
static uint8_t *parts;
static size_t parts_size;
static void noop(ee_ctx *c){(void)c;}
static void front(ee_ctx *c){c->r[2].sd[0]=front_type;}
static void rear(ee_ctx *c){c->r[2].sd[0]=rear_type;}
static void car(ee_ctx *c){memcpy(&c->r[2].ud[0],c->ram+c->r[4].uw[0],8);}
static void course_row(ee_ctx *c){memset(c->ram+c->r[7].uw[0],0,0x22);c->ram[c->r[7].uw[0]+0x10]=(uint8_t)event_type;c->r[2].ud[0]=1;}
static void read_parts(ee_ctx *c){c->r[2].ud[0]=parts!=NULL;}
static void find_part(ee_ctx *c){c->r[2].ud[0]=gt4_career_part_key(parts,parts_size,c->r[5].sd[0],c->r[6].sd[0]);}
static void fit(ee_ctx *c){unsigned t=c->r[5].uw[1];if(t!=0x19 && t!=0x1a)abort();memcpy(c->ram+c->r[4].uw[0]+(t==0x19?0x60:0x68),&c->r[5].ud[0],8);}
static ee_fn boundary(uint32_t pc){switch(pc){case 0x443f00:return course_row;case 0x449d58:case 0x449d78:return noop;case 0x449e10:return read_parts;case 0x44a048:return find_part;case 0x4468f8:return fit;case 0x4462e0:return front;case 0x446320:return rear;case 0x4454c0:return car;default:return NULL;}}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
static int forced_row(void *ctx,uint32_t table,uint32_t id,uint32_t nth,uint8_t *out,size_t cap){
    if(table==0x19 || table==0x1a){if(!out || cap<(table==0x19?0x3b:0x39))abort();memset(out,0,cap);out[table==0x19?0x3a:0x38]=(uint8_t)(table==0x19?front_type:rear_type);return (int)cap;}
    return db.row(ctx,table,id,nth,out,cap);
}
static uint8_t *forced_parts(void *ctx,uint64_t key,size_t *size){
    (void)ctx;(void)key;*size=parts_size;if(!parts)return NULL;uint8_t *p=malloc(parts_size);if(p)memcpy(p,parts,parts_size);return p;
}
static void w32(ee_ctx *c,uint32_t a,uint32_t v){memcpy(c->ram+a,&v,4);}
int main(int argc,char **argv){
    CHECK(argc==3);ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0x54f3f8);c.lookup=lookup;c.hook=boundary;gt4_phys_mem=c.ram;
    const char *error=NULL;gt4_vol *v=gt4_vol_open(argv[2],&error);CHECK(v);gt4_spec_tables *tables=gt4_spec_tables_open(v,NULL);CHECK(tables);
    db=(gt4_specdb_src){gt4_spec_tables_row,tables,gt4_spec_tables_str,gt4_spec_tables_car_parts};forced=db;forced.row=forced_row;forced.car_parts=forced_parts;
    size_t size=0;const uint8_t *generic=gt4_spec_tables_data(tables,0,&size);CHECK(generic && size>=16);uint32_t count;memcpy(&count,generic+8,4);unsigned cases=0;
    w32(&c,WORLD+0x6c,RACE);w32(&c,RACE+0x60,ENTRIES);w32(&c,RACE+0x70,0x1830000);w32(&c,ENTRIES,1);w32(&c,ENTRIES+8,SLOTS);w32(&c,SLOTS,SETUP-0x20);
    for(unsigned i=0;i<count;i++){
        uint32_t id;memcpy(&id,generic+16+i*8,4);ee_fpu_enter();int ok=gt4_garage_new(SETUP,id,&db,0x1e32000);ee_fpu_leave();CHECK(ok);
        uint8_t original[0x178];memcpy(original,c.ram+SETUP,sizeof original);
        size_t real_size=0;uint8_t *real=db.car_parts(db.ctx,id,&real_size);CHECK(real);
        for(event_type=0;event_type<5;event_type++)for(mode=0;mode<5;mode++){
            int expected=event_type==2?11:12;
            front_type=mode==1 || mode==2?expected:0;rear_type=mode==1 || mode==3?expected:0;
            parts=mode==4?NULL:real;parts_size=mode==4?0:real_size;
            memcpy(c.ram+SETUP,original,sizeof original);memcpy(c.ram+COPY,original,sizeof original);c.ram[0x1830000+0x4a]=(uint8_t)event_type;
            memset(c.r,0,sizeof c.r);c.r[4].uw[0]=WORLD;c.r[29].uw[0]=0x1ef0000;jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));
            ee_fpu_enter();recomp_lookup(0x3ee3c8)(&c);gt4_garage_event_tyres(COPY,event_type,&forced);ee_fpu_leave();
            if(getenv("GT4_CAREER_EVENT_TYRE_NEGATIVE") && !cases)c.ram[COPY+0x60]^=1;
            if(memcmp(c.ram+SETUP,c.ram+COPY,sizeof original)){
                fprintf(stderr,"tyre mismatch car=%u type=%d mode=%d\n",id,event_type,mode);
                for(unsigned j=0;j<sizeof original;j++)if(c.ram[SETUP+j]!=c.ram[COPY+j])fprintf(stderr,"+%x native=%02x original=%02x\n",j,c.ram[COPY+j],c.ram[SETUP+j]);
                return 1;
            }cases++;
        }
        free(real);
    }
    printf("PASS event tyres: %u cars, %u original type/axle/missing-list cases, complete setup blocks.\n",count,cases);
    gt4_phys_mem=NULL;ee_free(&c);gt4_spec_tables_close(tables);gt4_vol_close(v);return 0;
}
