/* Career active setup against original 00441248 -> 00441228 and
   004452F8. Recompiled instructions belong only to this test. */
#include "ee.h"
#include "career_original.h"
#include "physics/car.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do{if(!(x)){fprintf(stderr,"active setup line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int load(const char *path,uint8_t *out,size_t n){
    FILE *f=fopen(path,"rb");if(!f)return 0;
    int ok=fread(out,1,n,f)==n && fgetc(f)==EOF;fclose(f);return ok;
}
static int invoke(ee_ctx *c,uint32_t fn,uint32_t a,uint32_t b){
    memset(c->r,0,sizeof c->r);c->r[4].uw[0]=a;c->r[5].uw[0]=b;c->r[29].uw[0]=0x1ef0000;
    jmp_buf jump;c->exit_jmp=&jump;if(setjmp(jump))return 0;
    ee_fn f=c->lookup(fn);if(!f)return 0;f(c);return 1;
}
int main(int argc,char **argv){
    if(argc!=3){fprintf(stderr,"usage: gtmode_active_setup_oracle RAM SPR\n");return 2;}
    ee_ctx c;CHECK(!ee_init(&c));c.lookup=recomp_lookup;
    uint8_t *ram=malloc(EE_RAM_SIZE),*spr=malloc(EE_SPR_SIZE),*expected=malloc(EE_RAM_SIZE);
    CHECK(ram && spr && expected && load(argv[1],ram,EE_RAM_SIZE) && load(argv[2],spr,EE_SPR_SIZE));
    FILE *source=fopen("extracted/core.bin","rb");CHECK(source);
    const uint32_t spans[][2]={{0x441228,0x441260},{0x4452f8,0x44534c}};
    for(unsigned k=0;k<2;k++){
        uint8_t code[128];size_t n=spans[k][1]-spans[k][0];
        CHECK(!fseek(source,spans[k][0]-0x100000,SEEK_SET) && fread(code,1,n,source)==n && !memcmp(code,ram+spans[k][0],n));
    }fclose(source);
    uint32_t seed=0x20261003,full=0x1ed0000,out=0x1ee0000;unsigned cases=0;
    for(unsigned sheet=0;sheet<3;sheet++)for(unsigned k=0;k<256;k++){
        memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);
        for(unsigned j=0;j<0x4c0;j++){seed=seed*1664525u+1013904223u;c.ram[full+j]=(uint8_t)(seed>>24);}
        memcpy(c.ram+full+0x490,&sheet,4);memcpy(expected,c.ram,EE_RAM_SIZE);
        uint8_t setup[0x178];CHECK(gt4_career_active_setup(expected+full+8,setup));
        /* 00441248 +4 saves the cleared return register on its stack. */
        memset(expected+0x1ef0000-0x10,0,8);
        CHECK(invoke(&c,0x441248,full,0) && c.r[2].uw[0]==full+8+sheet*0x178 && !memcmp(expected,c.ram,EE_RAM_SIZE));
        uint32_t selected=c.r[2].uw[0];
        if(getenv("GT4_CAREER_TEST_NEGATIVE") && !cases)setup[0x110]^=1;
        memcpy(expected+out,setup,sizeof setup);
        CHECK(invoke(&c,0x4452f8,out,selected) && !memcmp(expected,c.ram,EE_RAM_SIZE) && !memcmp(c.spr,spr,EE_SPR_SIZE));cases++;
    }
    uint8_t body[0x4b8]={0},setup[0x178];memset(setup,0xa7,sizeof setup);
    uint8_t unchanged[0x178];memcpy(unchanged,setup,sizeof setup);
    memset(body,255,8);CHECK(!gt4_career_active_setup(body,setup) && !memcmp(unchanged,setup,sizeof setup));
    memset(body,0,sizeof body);body[0x488]=3;CHECK(!gt4_career_active_setup(body,setup) && !memcmp(unchanged,setup,sizeof setup));
    printf("PASS active garage setup: %u original pointer/copy cases, complete RAM/SPR; empty and invalid sheets rejected without writes.\n",cases);
    for(unsigned k=0;k<4096;k++){
        memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);
        uint8_t row[151],rules[0x80];
        for(unsigned j=0;j<sizeof row;j++){seed=seed*1664525u+1013904223u;row[j]=(uint8_t)(seed>>24);}
        /* Explicitly exercise the conditional default and signed clamp. */
        if(k<256){row[0x89]=(uint8_t)k;row[0x72]=0;row[0x6b]=(uint8_t)(k&1);}
        memcpy(c.ram+full,row,sizeof row);memcpy(expected,c.ram,EE_RAM_SIZE);
        gt4_career_session_rules(row,rules);
        if(getenv("GT4_CAREER_RULES_NEGATIVE") && !k)rules[0x31]^=1;
        memcpy(expected+out,rules,sizeof rules);
        CHECK(invoke(&c,0x35e108,out,full));
        /* Calling convention stack writes are outside the rule boundary. */
        memcpy(expected+0x1eef000,c.ram+0x1eef000,0x1000);
        CHECK(!memcmp(expected,c.ram,EE_RAM_SIZE) && !memcmp(c.spr,spr,EE_SPR_SIZE));
    }
    puts("PASS session rules: 4096 original 0x80-byte blocks; complete RAM/SPR outside call stack.");
    for(unsigned k=0;k<4096;k++){
        memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);
        for(unsigned j=0;j<0x80;j++){
            seed=seed*1664525u+1013904223u;c.ram[full+j]=(uint8_t)(seed>>24);
            seed=seed*1664525u+1013904223u;c.ram[out+j]=(uint8_t)(seed>>24);
        }
        for(unsigned j=0;j<0x18;j++){seed=seed*1664525u+1013904223u;c.ram[0x620b60+j]=(uint8_t)(seed>>24);}
        c.ram[0x620b60]=(uint8_t)(k&1);c.ram[out+0x69]=(uint8_t)((k>>1)&1);
        /* Force success and each validation failure independently. */
        const uint8_t valid[]={240,230,100,0,0,20,30,180,190};
        if(k<2048){memcpy(c.ram+full+0x2c,valid,sizeof valid);
            unsigned branch=k%7;
            if(branch==1)c.ram[full+0x2c]=0;
            if(branch==2)c.ram[full+0x33]=240;
            if(branch==3)c.ram[full+0x34]=230;
            if(branch==4)c.ram[full+0x31]=180;
            if(branch==5)c.ram[full+0x32]=190;
            if(branch==6)c.ram[full+0x2e]=230;
        }
        memcpy(expected,c.ram,EE_RAM_SIZE);gt4_phys_mem=expected;
        gt4_phys_fpu_enter();gt4_tyre_wear_global(out);
        if(getenv("GT4_CAREER_WEAR_NEGATIVE") && !k)expected[out+0x64]^=1;
        CHECK(invoke(&c,0x367af8,out,full) && !memcmp(expected,c.ram,EE_RAM_SIZE) && !memcmp(c.spr,spr,EE_SPR_SIZE));
        gt4_tyre_wear_rules(out,full);
        CHECK(invoke(&c,0x367918,out,full) && !memcmp(expected,c.ram,EE_RAM_SIZE) && !memcmp(c.spr,spr,EE_SPR_SIZE));
    }
    gt4_phys_mem=NULL;
    puts("PASS event wear: 4096 global/rule pairs, forced validation branches, complete RAM/SPR.");
    free(expected);free(spr);free(ram);ee_free(&c);return 0;
}
