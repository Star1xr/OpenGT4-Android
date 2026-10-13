/* Original 003BB5A8 selection with explicit table/constructor boundaries.
   Verifies indices and RNG, not country resolution or prepared setups. */
#include "ee.h"
#include "fpu.h"
#include "career_original.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"enemy indices line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int country_probe;
static void country_row(ee_ctx *c){uint64_t base=(uint32_t)c->r[5].ud[0];memcpy(c->ram+c->r[6].uw[0],&base,8);c->r[2].ud[0]=1;}
static uint8_t blocked[256];static unsigned count,chosen[5],chosen_count;
static void table_count(ee_ctx *c){c->r[2].ud[0]=count;}
static void country(ee_ctx *c){unsigned i=(unsigned)c->r[4].ud[0];c->r[2].ud[0]=i<count?blocked[i]:0;}
static void construct(ee_ctx *c){if(chosen_count<5)chosen[chosen_count++]=c->r[5].uw[0];}
static void paint(ee_ctx *c){(void)c;}
static ee_fn boundary(uint32_t pc){if(country_probe)return pc==0x443ed0?country_row:NULL;switch(pc){case 0x4473b0:return table_count;case 0x448c10:return country;case 0x4474b0:return construct;case 0x40aa48:return paint;default:return NULL;}}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
int main(int argc,char **argv)
{
    CHECK(argc==2);ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0x54f3f8);
    c.lookup=lookup;c.hook=boundary;uint32_t input_seed=0x20261003;
    const uint32_t row=0x1800000,list=0x1800100,race=0x1810000,entries=0x1810100,slots=0x1810200,order=0x1810300;
    /* Inject regions/keys, retaining original 00448CA0 and 0044A5A8.
       Prepared-row data alone is supplied at the SpecDB boundary. */
    country_probe=1;const uint32_t game=0x1800000;memcpy(c.ram+0x622f4c,&game,4);
    for(unsigned test=0;test<4096;test++){
        int32_t region=(int32_t)(test%8)-2;uint64_t key=(test%2?UINT64_C(0x2600000000):0)|(test*97u);
        memcpy(c.ram+game+0x39e7c,&region,4);memset(c.r,0,sizeof c.r);
        c.r[4].ud[0]=key;c.r[29].uw[0]=0x1ef0000;
        jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));recomp_lookup(0x448c10)(&c);
        int actual=gt4_career_enemy_blocked(key,region);
        if(getenv("GT4_CAREER_COUNTRY_NEGATIVE") && !test)actual^=1;
        CHECK(actual==c.r[2].sd[0]);
    }
    country_probe=0;
    for(unsigned test=0;test<8192;test++){
        count=1+test%32;unsigned cars=1+test%5;
        memset(c.ram+row,0,0x400);memset(c.ram+race,0,0x400);
        memcpy(c.ram+row+0xa8,&list,4);memcpy(c.ram+race+0x60,&entries,4);memcpy(c.ram+entries+8,&slots,4);
        for(unsigned i=0;i<count;i++){
            input_seed=input_seed*1664525u+1013904223u;
            blocked[i]=(test%3==0)?1:(test%3==1)?0:(uint8_t)(input_seed>>31);
            uint64_t key=i;memcpy(c.ram+list+8+16*i,&key,8);
        }
        for(unsigned i=0;i<cars;i++){
            uint32_t slot=0x1820000+i*0x4000;memcpy(c.ram+slots+i*4,&slot,4);memcpy(c.ram+order+i*4,&i,4);
        }
        input_seed=input_seed*1664525u+1013904223u;uint32_t native_seed=input_seed;
        unsigned native[5];ee_fpu_enter();CHECK(gt4_career_enemy_indices(blocked,count,cars,&native_seed,native));ee_fpu_leave();
        memcpy(c.ram+0x621ae8,&input_seed,4);chosen_count=0;memset(c.r,0,sizeof c.r);
        c.r[4].uw[0]=row;c.r[5].uw[0]=race;c.r[6].uw[0]=order;c.r[7].uw[0]=cars;c.r[29].uw[0]=0x1ef0000;
        jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));ee_fpu_enter();lookup(0x3bb5a8)(&c);ee_fpu_leave();
        if(getenv("GT4_CAREER_ENEMIES_NEGATIVE") && !test)native[0]^=1;
        uint32_t actual_seed;memcpy(&actual_seed,c.ram+0x621ae8,4);
        CHECK(chosen_count==cars && !memcmp(native,chosen,cars*sizeof *native) && actual_seed==native_seed);
    }
    puts("PASS country filter: 4096 original cases; enemy selection: 8192 original index/RNG cases, blocked/allowed/mixed lists and exhausted pools.");ee_free(&c);return 0;
}
