/* Original Adhoc weekday callback. Integer boxing/reference management are
   explicit VM boundaries; arithmetic and signed conversion execute original
   instructions from the user's disc in this test only. */
#include "ee.h"
#include "career_calendar.h"
#include "gtmode_game.h"
#include <stdlib.h>
#include <string.h>

ee_fn recomp_lookup(uint32_t);
static int32_t input,boxed;
static unsigned box_count,nil_count;
static void number(ee_ctx *c){c->r[2].sd[0]=input;}
static void box(ee_ctx *c){boxed=c->r[5].sw[0];box_count++;}
static void release(ee_ctx *c){(void)c;}
static void nil(ee_ctx *c){(void)c;nil_count++;}
static ee_fn boundary(uint32_t pc){switch(pc){
 case 0x1805000:return number;case 0x2fe278:return box;
 case 0x2fc870:return release;case 0x309348:return nil;
 case 0x3285a8:case 0x3285f8:return release;default:return NULL;
}}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
static void put(ee_ctx *c,uint32_t at,uint32_t v){memcpy(c->ram+at,&v,4);}
int main(int argc,char **argv){
    if(argc!=2)return 2;ee_ctx c;if(ee_init(&c) || ee_load_image(&c,argv[1],0x100000)<=0x500000)return 2;
    c.lookup=lookup;c.hook=boundary;
    const uint32_t stack=0x1ef0000,list=0x1800000,obj=0x1800100,vt=0x1800200;
    put(&c,list,obj);put(&c,obj+4,vt);put(&c,vt+0x58,0);put(&c,vt+0x5c,0x1805000);
    ah_vm *vm=ah_vm_new();ah_obj *calendar=ah_module_new(vm,"calendar",ah_root(vm),NULL,NULL);gt4_calendar_bind(vm,calendar);
    uint32_t random=0x10082026;
    for(unsigned i=0;i<65536;i++){
        random=random*1664525u+1013904223u;
        uint32_t bits=i<32768?(uint32_t)((int)i-16384):random;memcpy(&input,&bits,4);
        if(i==32768)input=INT32_MIN;if(i==32769)input=INT32_MAX;
        memset(c.r,0,sizeof c.r);c.r[4].uw[0]=stack-0x40;c.r[5].uw[0]=1;c.r[6].uw[0]=list;c.r[29].uw[0]=stack;
        box_count=0;jmp_buf jump;c.exit_jmp=&jump;if(setjmp(jump))return 1;recomp_lookup(0x136270)(&c);
        ah_val arg=ah_int((int64_t)input+INT64_C(0x100000000)),ret;
        if(ah_call_member(vm,calendar,"dayToWeekDay",1,&arg,&ret)<1 || ret.t!=AV_INT || box_count!=1)return 1;
        int32_t actual=(int32_t)ret.u.i;if(getenv("GT4_WEEKDAY_NEGATIVE"))actual++;
        if(actual!=boxed || gt4_career_day_to_weekday(input)!=boxed){fprintf(stderr,"weekday input %d original %d native %d\n",input,boxed,actual);return 1;}
        ah_gc(vm);
    }
    ah_val ret,args[]={ah_int(2453462),ah_nil()};
    if(ah_call_member(vm,calendar,"dayToWeekDay",2,args,&ret)<1 || ret.u.i!=6)return 1;
    memset(c.r,0,sizeof c.r);c.r[4].uw[0]=stack-0x40;c.r[29].uw[0]=stack;
    nil_count=0;jmp_buf jump;c.exit_jmp=&jump;if(setjmp(jump))return 1;recomp_lookup(0x136270)(&c);
    if(nil_count!=1 || ah_call_member(vm,calendar,"dayToWeekDay",0,NULL,&ret)<1 || ret.t!=AV_NIL)return 1;
    puts("PASS 65536 original weekday returns, signed overflow/remainders, low-word conversion, extra and missing arguments.");
    ah_vm_free(vm);ee_free(&c);return 0;
}
