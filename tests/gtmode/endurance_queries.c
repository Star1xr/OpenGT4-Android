/* Only the two original integer queries reached by endurance/pits.
   Include the adapter to exercise its exact fallback dispatch. */
#include "../../src/core/physics/race_run.c"
#include "ee.h"
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"query line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
    CHECK(argc==2);ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0x28b98c);c.lookup=recomp_lookup;
    uint8_t *native=calloc(1,RAM_SIZE);CHECK(native);gt4_phys_mem=native;
    uint32_t seed=456789;const uint32_t obj=0x1800000;
    for(unsigned i=0;i<4096;i++)for(unsigned query=0;query<2;query++){
        seed=seed*1664525u+1013904223u;memcpy(native+obj+0xe408,&seed,4);memcpy(c.ram+obj+0xe408,&seed,4);
        uint32_t fn=query?0x0038b980u:0x0035af10u;memset(c.r,0,sizeof c.r);c.r[4].uw[0]=obj;c.r[2].uw[0]=~seed;c.r[29].uw[0]=0x1ef0000;
        recomp_lookup(fn)(&c);missing=0;gt4_orig_v0=~seed;
        no_original(fn,obj,seed,gt4_f32(seed),gt4_f32(~seed));uint32_t actual=gt4_orig_v0;
        if(getenv("GT4_ENDURANCE_QUERY_NEGATIVE") && !i)actual^=1;
        CHECK(actual==c.r[2].uw[0] && !missing);CHECK(!memcmp(native+obj+0xe408,c.ram+obj+0xe408,4));
    }
    /* Unported functions must still fail, rather than being hidden by
       the two narrowly mapped virtual methods. */
    missing=0;no_original(0x0035af18u,obj,0,0,0);CHECK(missing==0x0035af18u);
    puts("PASS 8192 original endurance/pit integer queries: exact return, unchanged source word, unknown calls still rejected.");
    free(native);gt4_phys_mem=NULL;ee_free(&c);return 0;
}
