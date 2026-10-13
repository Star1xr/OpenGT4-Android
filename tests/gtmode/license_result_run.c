/* Execute original 001105A8/00110800. Completed work, record lookup,
   ranking/ghost construction and calendar insertion are explicit boundaries;
   original branches, packed record writes and history arguments execute. */
#include "ee.h"
#include "career_license.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"license result line %d: %s\n",__LINE__,#x);return 1;}}while(0)
#define GAME 0x1800000u
#define WORLD 0x1840000u
#define WORK 0x1841000u
#define RECORD 0x1851000u
#define BEST_GRADE 0x1852000u
#define BEST_TIME 0x1852010u
static unsigned ranked,history_calls,day_calls;
static uint32_t rank_time,rank_grade,history_grade,history_method,history_time;
static void put(ee_ctx *c,uint32_t a,uint32_t v){memcpy(c->ram+a,&v,4);}
static uint32_t word(ee_ctx *c,uint32_t a){uint32_t v;memcpy(&v,c->ram+a,4);return v;}
static void noop(ee_ctx *c){c->r[2].ud[0]=0;}
static void label(ee_ctx *c){c->r[2].ud[0]=0x1854000;}
static void record(ee_ctx *c){c->r[2].ud[0]=RECORD;}
static void replay(ee_ctx *c){put(c,c->r[5].uw[0],1);c->r[2].ud[0]=8;}
static void ranking(ee_ctx *c){ranked++;rank_time=c->r[5].uw[0];rank_grade=c->r[6].uw[0];}
static void history(ee_ctx *c){history_calls++;history_grade=c->r[6].uw[0];history_method=c->r[7].uw[0];history_time=c->r[8].uw[0];}
static void advance(ee_ctx *c){(void)c;day_calls++;}
static ee_fn boundary(uint32_t pc){switch(pc){
    case 0x447280:case 0x4472a0:return label;
    case 0x430d20:return record;case 0x3bcb38:return replay;
    case 0x4320b8:return ranking;case 0x42f2e0:return history;
    case 0x42f198:return advance;
    case 0x364760:case 0x364868:case 0x44ac20:case 0x4454c0:
    case 0x4482c0:case 0x448398:case 0x364e90:case 0x364fa8:
    case 0x10f718:case 0x4323d0:case 0x3647a0:return noop;
    default:return NULL;}}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
static uint32_t next(uint32_t *seed){*seed=*seed*1664525u+1013904223u;return *seed;}
static int call(ee_ctx *c,uint32_t pc){
    memset(c->r,0,sizeof c->r);c->r[4].uw[0]=WORLD;
    c->r[5].uw[0]=BEST_GRADE;c->r[6].uw[0]=BEST_TIME;c->r[29].uw[0]=0x1ef0000;
    jmp_buf jump;c->exit_jmp=&jump;
    if(setjmp(jump)){fprintf(stderr,"original stopped at %08x: %s\n",c->end_pc,c->end_detail);return 0;}
    recomp_lookup(pc)(c);return 1;
}
int main(int argc,char **argv){
    CHECK(argc==2);ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0x54f3f8);
    c.lookup=lookup;c.hook=boundary;put(&c,0x622f4c,GAME);
    put(&c,WORLD+0x6c,WORK);put(&c,WORLD+0x64,0x6807a8);
    put(&c,WORK+0x60,0x1843000);put(&c,0x1843008,0x1844000);put(&c,0x1844000,0x1845000);
    uint32_t seed=0x20261006;
    /* All state bytes, plus signed/large states; times below, equal and
       above the existing best. Non-medal states must skip rankings too. */
    const int32_t states[]={-1,INT32_MIN,INT32_MAX,256};
    for(unsigned test=0;test<260u*64u;test++){
        int32_t state=test/64<256?(int32_t)(test/64):states[test/64-256];
        gt4_career_license_attempts native={next(&seed),next(&seed),next(&seed)};
        if(test%8==0){native.best_grade=13;native.best_time=0x157529ff;}
        uint32_t time=next(&seed);
        if(test%4==0)time=native.best_time;
        if(test%4==1)time=0;
        if(test%4==2)time=UINT32_MAX;
        put(&c,RECORD,native.record);put(&c,BEST_GRADE,native.best_grade);put(&c,BEST_TIME,native.best_time);
        put(&c,WORK+0x88,(uint32_t)state);put(&c,WORK+0x8c,time);ranked=0;
        CHECK(call(&c,0x1105a8));uint32_t grade=gt4_career_license_attempt(&native,state,time);
        if(getenv("GT4_LICENSE_RESULT_NEGATIVE") && !test)native.record^=1;
        CHECK(native.record==word(&c,RECORD) && native.best_grade==word(&c,BEST_GRADE) && native.best_time==word(&c,BEST_TIME));
        CHECK(ranked==(grade!=13));if(ranked)CHECK(rank_time==time && rank_grade==grade);
        history_calls=day_calls=0;CHECK(call(&c,0x110800));
        uint32_t g=0xa5,m=0xa5,t=0xa5;int event=gt4_career_license_history(&native,&g,&m,&t);
        if(getenv("GT4_LICENSE_HISTORY_NEGATIVE") && !test)g^=1;
        CHECK(history_calls==(unsigned)event && day_calls==(unsigned)!event);
        if(event)CHECK(g==history_grade && m==history_method && t==history_time);
        else CHECK(g==0xa5 && m==0xa5 && t==0xa5);
    }
    /* One visit: bronze -> failed attempt -> tied silver -> slower gold.
       Record may improve while the best time/grade of this visit stays put. */
    gt4_career_license_attempts native={0xff0,13,0x157529ff};
    put(&c,RECORD,native.record);put(&c,BEST_GRADE,native.best_grade);put(&c,BEST_TIME,native.best_time);
    const int32_t seq[]={4,1,3,2};const uint32_t times[]={10000,9000,10000,11000};
    for(unsigned i=0;i<4;i++){
        put(&c,WORK+0x88,(uint32_t)seq[i]);put(&c,WORK+0x8c,times[i]);CHECK(call(&c,0x1105a8));
        gt4_career_license_attempt(&native,seq[i],times[i]);
        CHECK(native.record==word(&c,RECORD) && native.best_grade==word(&c,BEST_GRADE) && native.best_time==word(&c,BEST_TIME));
    }
    CHECK(native.best_grade==11 && native.best_time==10000 && ((native.record>>4)&15)==9);
    puts("PASS licence result: 16640 original state/time/record/history cases and multi-attempt visit; ranking and ghost data are boundaries.");
    ee_free(&c);return 0;
}
