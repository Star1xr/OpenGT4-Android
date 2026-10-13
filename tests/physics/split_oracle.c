/* Original Battle2P configuration methods execute EE, compared byte for
   byte with their native session routing. No solver substitution. */
#include "ee.h"
#include "car.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"split oracle line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
ee_fn recomp_lookup(uint32_t);
static void invoke(ee_ctx *c,uint32_t pc,uint32_t cond,uint32_t a1,uint32_t a2,uint32_t a3,uint32_t t0)
{
    memset(c->r,0,sizeof c->r);c->r[4].uw[0]=cond;c->r[5].uw[0]=a1;
    c->r[6].uw[0]=a2;c->r[7].uw[0]=a3;c->r[8].uw[0]=t0;c->r[29].uw[0]=0x1ef0000;
    jmp_buf j;c->exit_jmp=&j;CHECK(!setjmp(j));ee_fn fn=recomp_lookup(pc);CHECK(fn);fn(c);
}
int main(int argc,char **argv)
{
    CHECK(argc==2 || argc==3);int negative=argc==3;ee_ctx c;CHECK(!ee_init(&c));
    CHECK(ee_load_image(&c,argv[1],0x100000)>0x54f3f8);c.lookup=recomp_lookup;
    const uint32_t cond=0x1800000,race=0x1811000,block=0x1820000,args=0x1830000;
    gt4_phys_mem=c.ram;GT4_U32(cond)=race;GT4_U32(cond+0x10140)=0x00679988u;
    unsigned cases=0;
    for(int kind=0;kind<2;kind++)for(int k=0;k<5;k++){
        uint8_t expected[0x50];uint32_t result;
        memset(c.ram+block,0xa5,0x50);GT4_U32(race+(kind?0x1c:0x14))=k;
        invoke(&c,kind?0x00347188u:0x003471a8u,cond,block,0,0,0);
        memcpy(expected,c.ram+block,0x50);result=c.r[2].uw[0];memset(c.ram+block,0xa5,0x50);
        CHECK(gt4_session(cond,kind?0x60:0x58,block,0,0,0,0,NULL)==result);
        if(negative && !kind && !k)c.ram[block+0x2c]^=1;
        CHECK(!memcmp(expected,c.ram+block,0x50));cases++;
    }
    static const int handicap[]={-100,-20,-19,-1,0,1,19,20,100};
    for(unsigned k=0;k<sizeof handicap/sizeof *handicap;k++)for(int port=0;port<2;port++){
        uint8_t expected[12];GT4_U32(race+0x18)=(uint32_t)handicap[k];GT4_U8(cond+0xf866+port)=5;
        memset(c.ram+args,0,12);invoke(&c,0x00347108,cond,port,args,args+4,args+8);memcpy(expected,c.ram+args,12);
        memset(c.ram+args,0,12);gt4_session(cond,0xf0,port,args,args+4,args+8,0,NULL);
        CHECK(!memcmp(expected,c.ram+args,12));cases++;
    }
    printf("PASS %u Battle2P cases vs original EE: boost, wear and handicap both ports\n",cases);
    ee_free(&c);gt4_phys_mem=NULL;return 0;
}
