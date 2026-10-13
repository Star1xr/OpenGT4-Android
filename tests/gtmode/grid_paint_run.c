/* Event paint integration against 003BB5A8, including 004474B0 and
   0040AA48. Disc access and prepared construction are explicit boundaries;
   the latter has its separate gtmode_tuned_oracle proof. */
#include "ee.h"
#include "fpu.h"
#include "car.h"
#include "race_run.h"
#include "career_original.h"
#include "career_event.h"
#include "career_parts.h"
#include "career_race_lookup.h"
#include "spectables.h"
#include "specdb.h"
#include "vol.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"grid paint line %d: %s\n",__LINE__,#x); return 1; } } while (0)
#define RACE 0x1800000u
#define ENTRIES 0x1800100u
#define SLOTS 0x1800200u
#define ORDER 0x1800300u
#define INFO 0x1800400u
#define FIRST 0x1810000u
#define STRIDE 0x400u
static gt4_specdb_src db;
static unsigned candidates;
static uint8_t *list;
static uint32_t variation;
static uint8_t *parts;
static size_t parts_size;
static uint64_t key_at(unsigned n) { uint64_t k; memcpy(&k,list+8+16*n,8); return k; }
static void list_count(ee_ctx *c) { c->r[2].ud[0]=candidates; }
static void list_key(ee_ctx *c) { c->r[2].ud[0]=key_at(c->r[5].uw[0]); }
static void list_paint(ee_ctx *c) { memcpy(&c->r[2].ud[0],list+16+16*c->r[5].uw[0],8); }
static void country(ee_ctx *c) { c->r[2].ud[0]=0; /* 00448C10/0044A5A8, separately E */ }
static void construct(ee_ctx *c) {
    c->r[2].ud[0]=gt4_garage_tuned(c->r[4].uw[0],c->r[5].ud[0],&db,0x1e32000);
}
static void color(ee_ctx *c) { c->r[2].sd[0]=(int8_t)c->ram[c->r[4].uw[0]+0x18]; }
static void total(ee_ctx *c) {
    uint64_t key; uint8_t row[52]; unsigned n=0;
    memcpy(&key,c->ram+c->r[4].uw[0]+0x10,8);
    while(n<32 && db.row(db.ctx,(uint32_t)(key>>32),(uint32_t)key,n,row,sizeof row)==52)n++;
    c->r[2].ud[0]=n;
}
static void generic(ee_ctx *c) {
    if(db.row(db.ctx,0,c->r[4].uw[0],0,c->ram+c->r[5].uw[0],24)!=24)abort();
    c->r[2].ud[0]=1;
}
static void first(ee_ctx *c) { variation=c->r[5].uw[0]; c->r[2].ud[0]=0; }
static void row(ee_ctx *c) {
    if(db.row(db.ctx,0x2a,variation,c->r[5].uw[0],c->ram+c->r[6].uw[0],52)!=52)abort();
    c->r[2].ud[0]=1;
}
static void course_row(ee_ctx *c){
    int n=db.row(db.ctx,0x22,c->r[5].uw[0],0,c->ram+c->r[7].uw[0],c->r[6].uw[0]);
    c->r[2].ud[0]=n>0;
}
static void noop(ee_ctx *c){(void)c;}
static void read_parts(ee_ctx *c){free(parts);parts=db.car_parts(db.ctx,c->r[5].ud[0],&parts_size);c->r[2].ud[0]=parts!=NULL;}
static void release_parts(ee_ctx *c){(void)c;free(parts);parts=NULL;parts_size=0;}
static void part_key(ee_ctx *c){c->r[2].ud[0]=gt4_career_part_key(parts,parts_size,c->r[5].sd[0],c->r[6].sd[0]);}
static void front_tyre(ee_ctx *c){c->r[2].sd[0]=gt4_tyre_byte(&db,c->r[4].uw[0],0x60,0x3a);}
static void rear_tyre(ee_ctx *c){c->r[2].sd[0]=gt4_tyre_byte(&db,c->r[4].uw[0],0x68,0x38);}
static void base_car(ee_ctx *c){memcpy(&c->r[2].ud[0],c->ram+c->r[4].uw[0],8);}
static void fit_tyre(ee_ctx *c){unsigned type=c->r[5].uw[1];if(type!=0x19 && type!=0x1a)abort();memcpy(c->ram+c->r[4].uw[0]+(type==0x19?0x60:0x68),&c->r[5].ud[0],8);}
static ee_fn boundary(uint32_t pc) {
    switch(pc) {
    case 0x4473b0:return list_count; case 0x447400:return list_key;
    case 0x447458:return list_paint; case 0x448c10:return country;
    case 0x444690:return construct; case 0x4458c8:return color;
    case 0x445808:return total; case 0x448820:return generic;
    case 0x449210:return first; case 0x4492d8:return row;
    case 0x443f00:return course_row;case 0x449d58:return noop;
    case 0x449d78:return release_parts;case 0x449e10:return read_parts;
    case 0x44a048:return part_key;case 0x4462e0:return front_tyre;
    case 0x446320:return rear_tyre;case 0x4454c0:return base_car;
    case 0x4468f8:return fit_tyre;
    default:return NULL;
    }
}
static ee_fn lookup(uint32_t pc) { ee_fn f=boundary(pc); return f?f:recomp_lookup(pc); }
static void w32(ee_ctx *c,uint32_t a,uint32_t v) { memcpy(c->ram+a,&v,4); }
int main(int argc,char **argv) {
    CHECK(argc==3 || argc==4); const char *error=NULL; char msg[256];
    gt4_vol *v=gt4_vol_open(argv[2],&error); CHECK(v);
    gt4_spec_tables *tables=gt4_spec_tables_open(v,NULL); CHECK(tables);
    db=(gt4_specdb_src){gt4_spec_tables_row,tables,gt4_spec_tables_str,gt4_spec_tables_car_parts};
    gt4_career_race_lookup_data data; CHECK(gt4_career_race_lookup_data_load(argv[2],&data));
    size_t ln=0,rn=0,cn=0;
    uint8_t *labels=gt4_vol_read(v,"/specdb/GT4_US2560/RACE.idi",&ln);
    uint8_t *races=gt4_vol_read(v,"/specdb/GT4_US2560/RACE.dbt",&rn);
    uint8_t *courses=gt4_vol_read(v,"/specdb/GT4_US2560/COURSE.idi",&cn);
    CHECK(labels && races && courses);
    const char *event=argc==4?argv[3]:"am_sunday_0001"; uint32_t id; gt4_race_desc d={0}; uint8_t event_row[151];
    CHECK(gt4_career_race_row(&data,labels,ln,races,rn,event,event_row));
    CHECK(gt4_specdb_label(labels,ln,event,&id));
    char path[96];snprintf(path,sizeof path,"/specdb/GT4_US2560/RACES/%016llX",(unsigned long long)(UINT64_C(0x2300000000)|id));
    size_t size=0; list=gt4_vol_read(v,path,&size); CHECK(list && size>=8);
    memcpy(&candidates,list,4); CHECK(candidates && candidates<=256 && size>=8+candidates*16);
    const char *course=gt4_specdb_label_for_id(courses,cn,event_row[0]|(uint32_t)event_row[1]<<8); CHECK(course);
    snprintf(d.course,sizeof d.course,"%s",course); d.event_row=event_row; d.cars=event_row[0x3e];d.car[0]=596;
    CHECK(d.cars>=2 && d.cars<=6);
    ee_ctx c; CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0x54f3f8);
    c.lookup=lookup;c.hook=boundary;
    uint32_t chain_native,chain_original;CHECK(gt4_career_event_rng_load(argv[2],&chain_native));
    memset(c.r,0,sizeof c.r);c.r[29].uw[0]=0x1ef0000;
    jmp_buf startup;c.exit_jmp=&startup;CHECK(!setjmp(startup));recomp_lookup(0x3bb820)(&c);
    memcpy(&chain_original,c.ram+0x621ae8,4);CHECK(chain_native==chain_original);
    uint32_t ctor;memcpy(&ctor,c.ram+0x61736c,4);CHECK(ctor==0x3bb820);
    for(unsigned i=0;i<4096;i++){
        uint32_t initial=i*1664525u+1013904223u,expected;
        w32(&c,0x621ae8,initial);memset(c.r,0,sizeof c.r);c.r[29].uw[0]=0x1ef0000;
        CHECK(!setjmp(startup));recomp_lookup(ctor)(&c);memcpy(&expected,c.ram+0x621ae8,4);
        gt4_career_enemy_rng_init(&initial);CHECK(initial==expected);
    }
    if(getenv("GT4_CAREER_RNG_NEGATIVE")){chain_native^=1;CHECK(chain_native==chain_original);}
    static const uint32_t seeds[]={0,1,17,0x20261003,0x80000000,0xffffffff};
    for(unsigned test=0;test<sizeof seeds/sizeof *seeds;test++) {
        if(getenv("GT4_CAREER_GRID_CHAIN")){
            /* race_util.ad enumerates consecutive two-digit subrace labels.
               Preserve the RNG while changing the actual event data. */
            char subrace[96];size_t length=strlen(event);CHECK(length>=2 && length<sizeof subrace);
            snprintf(subrace,sizeof subrace,"%.*s%02u",(int)length-2,event,test+1);
            const char *selected=gt4_specdb_label(labels,ln,subrace,&id)?subrace:event;
            CHECK(gt4_specdb_label(labels,ln,selected,&id));CHECK(gt4_career_race_row(&data,labels,ln,races,rn,selected,event_row));
            snprintf(path,sizeof path,"/specdb/GT4_US2560/RACES/%016llX",(unsigned long long)(UINT64_C(0x2300000000)|id));
            free(list);list=gt4_vol_read(v,path,&size);CHECK(list && size>=8);memcpy(&candidates,list,4);CHECK(candidates && candidates<=256 && size>=8+candidates*16);
            /* The selected label remains valid until prepare has copied its row. */
            snprintf(msg,sizeof msg,"%s",selected);
        }
        uint32_t initial=getenv("GT4_CAREER_GRID_CHAIN")?chain_native:seeds[test];
        uint32_t seed=initial;uint8_t body[0x4b8]={0};
        gt4_phys_mem=c.ram;
        ee_fpu_enter();int ok=gt4_garage_new(FIRST+0x20,596,&db,0x1e32000);ee_fpu_leave();CHECK(ok);
        memcpy(body,c.ram+FIRST+0x20,0x178);gt4_phys_mem=NULL;
        gt4_career_event prepared;
        char selected_label[96];snprintf(selected_label,sizeof selected_label,"%s",getenv("GT4_CAREER_GRID_CHAIN")?msg:event);
        ee_fpu_enter();ok=gt4_career_event_prepare(&prepared,argv[2],&data,selected_label,body,0,&seed,msg,sizeof msg);ee_fpu_leave();
        if(!ok){fprintf(stderr,"prepare grid: %s\n",msg);return 1;}
        CHECK(!memcmp(prepared.player_body,body,sizeof body));
        CHECK(prepared.desc.car[0]==596 && prepared.desc.cars==event_row[0x3e]);
        CHECK(!memcmp(prepared.race_row,event_row,sizeof event_row));
        if(getenv("GT4_CAREER_EVENT_GRID_NEGATIVE") && !test)prepared.player_body[0x19]^=1;
        d=prepared.desc;
        gt4_race_run *run=gt4_race_run_open(argv[2],&d,msg,sizeof msg);
        if(!run){fprintf(stderr,"native grid: %s\n",msg);return 1;}
        uint32_t race=GT4_U32(gt4_race_run_conductor(run)),entries=GT4_U32(race+0x60);
        uint32_t native_course=GT4_U32(GT4_U32(race+0x70)+0xa0);
        uint8_t native[6][0x178];
        for(int i=0;i<d.cars;i++){uint32_t entry=GT4_U32(GT4_U32(entries+8)+i*4);memcpy(native[i],gt4_ptr(entry+0x20),0x178);}
        gt4_race_run_close(run);
        gt4_phys_mem=c.ram;memset(c.ram+RACE,0,0x2000);
        w32(&c,RACE+0x60,ENTRIES);w32(&c,ENTRIES,d.cars);w32(&c,ENTRIES+8,SLOTS);
        for(int i=0;i<d.cars;i++){w32(&c,SLOTS+i*4,FIRST+i*STRIDE);if(i)w32(&c,ORDER+(i-1)*4,i);}
        ee_fpu_enter();ok=gt4_garage_new(FIRST+0x20,d.car[0],&db,0x1e32000);ee_fpu_leave();CHECK(ok);
        w32(&c,0x621ae8,getenv("GT4_CAREER_GRID_CHAIN")?chain_original:seeds[test]);memset(c.r,0,sizeof c.r);
        c.r[4].uw[0]=INFO;c.r[5].uw[0]=RACE;c.r[6].uw[0]=ORDER;c.r[7].uw[0]=d.cars-1;c.r[29].uw[0]=0x1ef0000;
        jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));
        ee_fpu_enter();recomp_lookup(0x3bb5a8)(&c);ee_fpu_leave();
        /* GT world's 003EE3C8 follows grid construction. Exercise the
           actual COURSE lookup, including dirt and snow event variants. */
        w32(&c,INFO+0x200+0x6c,RACE);w32(&c,RACE+0x70,INFO+0x100);
        w32(&c,INFO+0x100+0xa0,event_row[0]|(uint32_t)event_row[1]<<8);
        memset(c.r,0,sizeof c.r);c.r[4].uw[0]=INFO+0x200;c.r[29].uw[0]=0x1ef0000;
        ee_fpu_enter();recomp_lookup(0x3ee3c8)(&c);ee_fpu_leave();
        if(getenv("GT4_CAREER_GRID_PAINT_NEGATIVE") && !test)native[1][0x18]^=1;
        if(getenv("GT4_CAREER_RNG_CHAIN_NEGATIVE") && test==1)seed^=1;
        uint32_t original_seed;memcpy(&original_seed,c.ram+0x621ae8,4);CHECK(seed==original_seed);
        chain_native=seed;chain_original=original_seed;
        if(getenv("GT4_CAREER_GRID_CHAIN"))printf("chain %u: %s RNG=%08x\n",test+1,selected_label,seed);
        for(int i=0;i<d.cars;i++) {
            if(memcmp(native[i],c.ram+FIRST+i*STRIDE+0x20,0x178)) {
                fprintf(stderr,"grid setup mismatch test=%u car=%d\n",test,i);
                uint8_t diagnostic[256];int row_size=db.row(db.ctx,0x22,native_course,0,diagnostic,sizeof diagnostic);
                fprintf(stderr,"native COURSE=%u event COURSE=%u row_size=%d type=%u\n",native_course,event_row[0]|(uint32_t)event_row[1]<<8,row_size,diagnostic[0x10]);
                for(unsigned j=0;j<0x178;j++)if(native[i][j]!=c.ram[FIRST+i*STRIDE+0x20+j]){fprintf(stderr,"+%x native=%02x original=%02x\n",j,native[i][j],c.ram[FIRST+i*STRIDE+0x20+j]);break;}
                return 1;
            }
        }
        gt4_phys_mem=NULL;
    }
    printf("PASS event paint/tyre integration %s: 4096 startup constructors, six original complete selection sequences, %d-car setup blocks and final RNG; prepared construction and disc access are explicit boundaries.\n",event,d.cars);
    ee_free(&c);free(list);free(labels);free(races);free(courses);gt4_spec_tables_close(tables);gt4_vol_close(v);return 0;
}
