/* Original paint arithmetic and RNG; palette access is an explicit data
   boundary. No gameplay function is replaced with the native selector. */
#include "ee.h"
#include "fpu.h"
#include "career_original.h"
#include "car.h"
#include "spectables.h"
#include "vol.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"paint line %d: %s\n",__LINE__,#x);return 1;}}while(0)
#define RACE 0x1800000u
#define ENTRIES 0x1800100u
#define SLOTS 0x1800200u
#define FIRST 0x1810000u
#define OUTPUT 0x1820000u
#define RNG 0x1830000u
static gt4_career_paint_car fixture[6];
static int palette_probe;
static uint8_t palette_rows[32][52],palette_generic[24];
static unsigned palette_count;
static int list_probe;
static uint64_t list_paint;
static uint32_t argument2;
static void list_key(ee_ctx *c){c->r[2].ud[0]=UINT64_C(0x2600000001);}
static void list_color(ee_ctx *c){c->r[2].ud[0]=list_paint;}
static void list_construct(ee_ctx *c){c->ram[c->r[4].uw[0]+0x18]=0;c->r[2].ud[0]=2;}
static gt4_specdb_src palette_db;
static void palette_total(ee_ctx *c){c->r[2].ud[0]=palette_count;}
static void palette_car(ee_ctx *c){memcpy(c->ram+c->r[5].uw[0],palette_generic,24);c->r[2].ud[0]=1;}
static void palette_first(ee_ctx *c){c->r[2].ud[0]=0;}
static void palette_row(ee_ctx *c){unsigned i=c->r[5].uw[0];if(i>=palette_count)abort();memcpy(c->ram+c->r[6].uw[0],palette_rows[i],52);c->r[2].ud[0]=1;}
static int palette_native_row(void *ctx,uint32_t table,uint32_t id,uint32_t nth,uint8_t *out,size_t cap)
{
    (void)ctx;
    if(table==0){if(out){size_t n=cap<24?cap:24;memcpy(out,palette_generic,n);}return 24;}
    return palette_db.row(palette_db.ctx,table,id,nth,out,cap);
}
static unsigned car_index(ee_ctx *c){return (c->r[4].uw[0]-FIRST-0x20u)/0x100u;}
static void color_index(ee_ctx *c){c->r[2].sd[0]=(int8_t)c->ram[c->r[4].uw[0]+0x18];}
static void color_count(ee_ctx *c){c->r[2].ud[0]=fixture[car_index(c)].count;}
static void color_rgb(ee_ctx *c){c->r[2].ud[0]=fixture[car_index(c)].rgb[c->r[5].uw[0]]&0xffffffu;}
static void color_set(ee_ctx *c){c->ram[c->r[4].uw[0]+0x18]=(uint8_t)c->r[5].uw[0];c->r[2].ud[0]=1;}
static ee_fn boundary(uint32_t pc){
    if(list_probe){switch(pc){case 0x445808:return palette_total;case 0x447400:return list_key;case 0x447458:return list_color;case 0x444690:return list_construct;default:return NULL;}}
    if(palette_probe){switch(pc){case 0x445808:return palette_total;case 0x448820:return palette_car;case 0x449210:return palette_first;case 0x4492d8:return palette_row;default:return NULL;}}
    switch(pc){case 0x4458c8:return color_index;case 0x445828:return color_count;case 0x445ab8:return color_rgb;case 0x445880:return color_set;default:return NULL;}}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
static uint32_t next(uint32_t *s){*s=*s*1664525u+1013904223u;return *s;}
static void w32(ee_ctx *c,uint32_t at,uint32_t value){memcpy(c->ram+at,&value,4);}
static void invoke(ee_ctx *c,uint32_t fn,uint32_t a0,uint32_t a1)
{
    memset(c->r,0,sizeof c->r);c->r[4].uw[0]=a0;c->r[5].uw[0]=a1;c->r[6].uw[0]=argument2;c->r[29].uw[0]=0x1ef0000;
    ee_fpu_enter();recomp_lookup(fn)(c);ee_fpu_leave();
}
int main(int argc,char **argv)
{
    CHECK(argc==2 || argc==3);ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0x54f3f8);
    c.lookup=lookup;c.hook=boundary;uint32_t data_seed=0x20261003;
    list_probe=1;argument2=FIRST;
    for(unsigned test=0;test<4096;test++){
        static const uint64_t edges[]={0,1,2,32,33,UINT64_MAX,UINT64_C(0x100000001),UINT64_C(0x8000000000000000),UINT64_C(0xffffffff00000000)};
        list_paint=test<sizeof edges/sizeof *edges?edges[test]:((uint64_t)next(&data_seed)<<32)|next(&data_seed);
        palette_count=1+test%32;jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));invoke(&c,0x4474b0,RACE,0);
        int color=gt4_career_paint_list_index(list_paint,(int32_t)palette_count,0);
        if(getenv("GT4_CAREER_LIST_PAINT_NEGATIVE") && !test)color^=1;
        CHECK(color==(int8_t)c.ram[FIRST+0x18] && c.r[2].sd[0]==2);
    }
    list_probe=0;argument2=0;puts("PASS RACES paint conversion: 4096 original signed/wrapping/rejected-index cases.");
    gt4_career_paint_group native[6];gt4_career_paint_car selected[6];
    for(unsigned test=0;test<768;test++){
        unsigned cars=1+test%6;
        w32(&c,RACE+0x60,ENTRIES);w32(&c,ENTRIES,cars);w32(&c,ENTRIES+8,SLOTS);
        for(unsigned i=0;i<cars;i++){
            unsigned n=test>=752?32:1+next(&data_seed)%8;
            fixture[i].count=n;fixture[i].color=(test%4==0 || (test%4==2 && i==0))?(int)(next(&data_seed)%n):-1;
            for(unsigned j=0;j<n;j++){
                uint32_t rgb=next(&data_seed)&0xffffffu;
                if(test%7==0)rgb=0;else if(test%7==1)rgb=0xffffffu;else if(test%7==2)rgb=(j&1)?0x303030u:0xe0e0e0u;
                fixture[i].rgb[j]=rgb;
            }
            w32(&c,SLOTS+i*4,FIRST+i*0x100);c.ram[FIRST+i*0x100+0x38]=(uint8_t)fixture[i].color;
        }
        memset(c.ram+OUTPUT,0x5a,4+6*0x304);memset(native,0x5a,sizeof native);
        jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));invoke(&c,0x40a6f0,RACE,OUTPUT);
        ee_fpu_enter();int ok=gt4_career_paint_weights(fixture,cars,native);ee_fpu_leave();CHECK(ok);
        if(getenv("GT4_CAREER_PAINT_NEGATIVE") && !test){uint32_t bits;memcpy(&bits,&native[0].choices[0].weight,4);bits^=1;memcpy(&native[0].choices[0].weight,&bits,4);}
        if(memcmp(native,c.ram+OUTPUT+4,cars*0x304)){
            fprintf(stderr,"paint weights mismatch case=%u cars=%u\n",test,cars);
            for(unsigned j=0;j<cars*0x304;j++)if(((uint8_t *)native)[j]!=c.ram[OUTPUT+4+j]){fprintf(stderr," +%x native=%02x original=%02x\n",j,((uint8_t *)native)[j],c.ram[OUTPUT+4+j]);break;}
            return 1;
        }
        uint32_t seed=next(&data_seed);w32(&c,RNG,seed);memcpy(selected,fixture,sizeof selected);
        invoke(&c,0x40aa48,RACE,RNG);
        ee_fpu_enter();ok=gt4_career_select_paints(selected,cars,&seed);ee_fpu_leave();CHECK(ok);
        uint32_t original_seed;memcpy(&original_seed,c.ram+RNG,4);CHECK(seed==original_seed);
        for(unsigned i=0;i<cars;i++)CHECK(selected[i].color==(int8_t)c.ram[FIRST+i*0x100+0x38]);
    }
    puts("PASS paint selection: 768 original full weight blocks, chosen indices and final RNG; fixed/missing, black/white/duplicate colors, 1..6 cars, up to 32 paints.");
    if(argc==3){
        const char *error=NULL;gt4_vol *vol=gt4_vol_open(argv[2],&error);CHECK(vol);
        gt4_spec_tables *tables=gt4_spec_tables_open(vol,NULL);CHECK(tables);
        palette_db=(gt4_specdb_src){gt4_spec_tables_row,tables,gt4_spec_tables_str,gt4_spec_tables_car_parts};
        gt4_specdb_src forced=palette_db;forced.row=palette_native_row;
        size_t size=0;const uint8_t *generic=gt4_spec_tables_data(tables,0,&size);CHECK(generic && size>=16);
        uint32_t total;memcpy(&total,generic+8,4);palette_probe=1;gt4_phys_mem=c.ram;unsigned cases=0,hidden=0;
        for(unsigned i=0;i<total;i++){
            uint32_t id;memcpy(&id,generic+16+i*8,4);
            CHECK(palette_db.row(palette_db.ctx,0,id,0,palette_generic,sizeof palette_generic)==24);
            ee_fpu_enter();int ok=gt4_garage_new(FIRST+0x20,id,&palette_db,0x1818000);ee_fpu_leave();CHECK(ok);
            uint64_t key;memcpy(&key,c.ram+FIRST+0x30,8);palette_count=0;
            while(palette_count<32 && palette_db.row(palette_db.ctx,(uint32_t)(key>>32),(uint32_t)key,palette_count,palette_rows[palette_count],52)==52)palette_count++;
            CHECK(palette_count && palette_count<=32);hidden+=(palette_generic[0x12]>>1)&1u;
            uint8_t original_flags=palette_generic[0x12];
            /* Original flags plus both forced bit-1 branches for every car. */
            for(unsigned mode=0;mode<3;mode++){
                palette_generic[0x12]=mode==0?original_flags:mode==1?(original_flags&~2u):(original_flags|2u);
                uint32_t rgb[32];int n=gt4_garage_paint_palette(FIRST+0x20,&forced,1,rgb);
                if(getenv("GT4_CAREER_PALETTE_NEGATIVE") && !i && !mode)rgb[0]^=1;
                jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));invoke(&c,0x445828,FIRST+0x20,0);
                if(n!=c.r[2].sd[0]){fprintf(stderr,"palette count car=%u id=%u mode=%u count=%u key=%llx flags=%02x native=%d original=%lld\n",i,id,mode,palette_count,(unsigned long long)key,palette_generic[0x12],n,(long long)c.r[2].sd[0]);return 1;}
                for(int j=0;j<n;j++){invoke(&c,0x445ab8,FIRST+0x20,(uint32_t)j);CHECK(rgb[j]==c.r[2].ud[0]);}
                /* Fixed colors include the hidden last paint. */
                n=gt4_garage_paint_palette(FIRST+0x20,&forced,0,rgb);CHECK(n==(int)palette_count);
                for(int j=0;j<n;j++){invoke(&c,0x445ab8,FIRST+0x20,(uint32_t)j);CHECK(rgb[j]==c.r[2].ud[0]);}
                cases++;
            }
        }
        printf("PASS disc palettes: %u cars, %u original/forced flag cases, %u cars exclude the last paint for random selection.\n",total,cases,hidden);
        gt4_phys_mem=NULL;gt4_spec_tables_close(tables);gt4_vol_close(vol);
    }
    ee_free(&c);return 0;
}
