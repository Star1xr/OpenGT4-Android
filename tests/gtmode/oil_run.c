#include "ee.h"
#include "fpu.h"
#include "career_original.h"
#include "career_float.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t addr);
static int load(const char *path,uint8_t *out,size_t size){FILE *f=fopen(path,"rb");if(!f)return 0;int ok=fread(out,1,size,f)==size && fgetc(f)==EOF;fclose(f);return ok;}
static uint32_t random32(uint32_t *s){*s^=*s<<13;*s^=*s>>17;*s^=*s<<5;return *s;}
static uint32_t read32(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
static void store64(uint8_t *p,uint64_t v){memcpy(p,&v,8);}
int main(int argc,char **argv){
    if(argc!=4){fprintf(stderr,"usage: gtmode_oil_oracle RAM SPR wear-address\n");return 2;}
    uint32_t wear=(uint32_t)strtoul(argv[3],NULL,0);if(wear>EE_RAM_SIZE-32)return 2;
    ee_ctx c;if(ee_init(&c))return 2;
    uint8_t *ram=malloc(EE_RAM_SIZE),*spr=malloc(EE_SPR_SIZE),*expected=malloc(EE_RAM_SIZE);
    if(!ram || !spr || !expected || !load(argv[1],ram,EE_RAM_SIZE) || !load(argv[2],spr,EE_SPR_SIZE))return 2;
    const uint32_t ranges[][2]={{0x343928,0x343974},{0x343698,0x343704},{0x350678,0x350780},{0x3507d0,0x350804},{0x350808,0x350830}};
    FILE *f=fopen("extracted/core.bin","rb");if(!f)return 2;
    for(unsigned i=0;i<5;i++){
        uint8_t code[512];size_t size=ranges[i][1]-ranges[i][0];
        if(fseek(f,ranges[i][0]-0x100000,SEEK_SET) || fread(code,1,size,f)!=size || memcmp(code,ram+ranges[i][0],size))return 2;
    }
    fclose(f);c.lookup=recomp_lookup;ee_fn fn=recomp_lookup(0x343928);if(!fn)return 2;
    jmp_buf jump;c.exit_jmp=&jump;uint32_t seed=0x30c62026;
    const uint32_t params=0x620320,settings=0x620cd8,stack=0x01ef0000;
    unsigned cases=1281;
    for(unsigned test=0;test<cases;test++){
        memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);memset(c.r,0,sizeof c.r);c.end_reason=0;
        if(test){
            for(unsigned i=0;i<32;i++)c.ram[wear+i]=(uint8_t)random32(&seed);
            c.ram[wear+0x14]=test<=256?(uint8_t)(test-1):test%3==0?(uint8_t)random32(&seed):0;
            c.ram[settings+0xcc]=(uint8_t)(test&1);
            for(unsigned i=0xd0;i<=0xd8;i++)c.ram[settings+i]=(uint8_t)random32(&seed);
            if(test>256){uint32_t value=random32(&seed),limit=random32(&seed);memcpy(c.ram+params+20,&value,4);memcpy(c.ram+params+28,&limit,4);}
        }
        memcpy(expected,c.ram,EE_RAM_SIZE);
        c.r[4].uw[0]=wear;c.r[29].uw[0]=stack;c.r[16].ud[0]=0x1234;c.r[17].ud[0]=0x5678;c.fpr[20].u=0x3f123456;
        /* Exact dead stack stores made by the original ABI. Game state
           comes solely from the native port; no original output is copied. */
        store64(expected+stack-16,0x1234);store64(expected+stack-8,0);
        if(!expected[wear+0x14]){
            gt4_career_replace_oil(expected+wear,expected+params,expected+settings);
            uint32_t value=career_f32_mul(read32(expected+params+20),0x40400000u);
            int clamped=career_f32_lt(read32(expected+params+28),value);
            store64(expected+stack-32,0x1234);store64(expected+stack-24,0x5678);store64(expected+stack-16,0);
            uint32_t saved_f20=0x3f123456;memcpy(expected+stack-8,&saved_f20,4);
            store64(expected+stack-48,params);store64(expected+stack-40,clamped?0x3436e0:0x3436cc);
        }else gt4_career_replace_oil(expected+wear,expected+params,expected+settings);
        ee_fpu_enter();
        if(setjmp(jump)){ee_fpu_leave();fprintf(stderr,"original stopped: %s\n",c.end_detail);return 1;}
        fn(&c);ee_fpu_leave();
        if(memcmp(c.ram,expected,EE_RAM_SIZE) || memcmp(c.spr,spr,EE_SPR_SIZE)){
            fprintf(stderr,"oil case=%u differs\n",test);
            for(uint32_t i=0;i<EE_RAM_SIZE;i++)if(c.ram[i]!=expected[i]){fprintf(stderr,"RAM mismatch %08x: %02x/%02x\n",i,c.ram[i],expected[i]);break;}
            return 1;
        }
    }
    printf("func_00343928: %u/%u identical; full RAM and SPR per case, including lazy oil globals\n",cases,cases);
    free(expected);free(spr);free(ram);ee_free(&c);return 0;
}
