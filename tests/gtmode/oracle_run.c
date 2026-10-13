/* Oracle only. Generated game code and snapshots remain in ignored paths.
   Usage: gtmode_oracle <eeMemory.bin> <Scratchpad.bin> <profile address>
   Compares full 32 MiB RAM and scratchpad after each independent call. */
#include "ee.h"
#include "career_original.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t addr);
static int load(const char *path,uint8_t *out,size_t size){FILE *f=fopen(path,"rb");if(!f)return 0;int ok=fread(out,1,size,f)==size && fgetc(f)==EOF;fclose(f);return ok;}
static int verify_code(const uint8_t *ram){
    const uint32_t ranges[][2]={{0x4359b0,0x4359e0},{0x4359e0,0x435a0c},{0x43af68,0x43af84},{0x43afb0,0x43aff0}};
    FILE *f=fopen("extracted/core.bin","rb");if(!f)return 0;
    for(unsigned i=0;i<4;i++){
        uint8_t bytes[64];size_t size=ranges[i][1]-ranges[i][0];
        if(fseek(f,ranges[i][0]-0x100000,SEEK_SET) || fread(bytes,1,size,f)!=size || memcmp(bytes,ram+ranges[i][0],size)){
            fprintf(stderr,"captured code differs from oracle source at %08x\n",ranges[i][0]);fclose(f);return 0;
        }
    }
    fclose(f);return 1;
}
static uint64_t random64(uint64_t *state){*state^=*state<<13;*state^=*state>>7;*state^=*state<<17;return *state;}
int main(int argc,char **argv){
    if(argc!=4){fprintf(stderr,"usage: gtmode_oracle RAM SPR profile-address\n");return 2;}
    uint32_t profile=(uint32_t)strtoul(argv[3],NULL,0);
    if(profile>EE_RAM_SIZE-GT4_CAREER_MONEY_OFFSET-8)return 2;
    ee_ctx original;if(ee_init(&original))return 2;
    uint8_t *ram=malloc(EE_RAM_SIZE),*spr=malloc(EE_SPR_SIZE),*native=malloc(EE_RAM_SIZE);
    if(!ram || !spr || !native || !load(argv[1],ram,EE_RAM_SIZE) || !load(argv[2],spr,EE_SPR_SIZE))return 2;
    if(!verify_code(ram))return 2;
    original.lookup=recomp_lookup;ee_fn fn=recomp_lookup(0x0043afb0);if(!fn)return 2;
    jmp_buf jump;original.exit_jmp=&jump;
    const uint64_t values[]={0,1,9999,10000,99999999,UINT64_C(0x7fffffffffffffff),UINT64_C(0x8000000000000000),UINT64_MAX};
    uint64_t seed=UINT64_C(0x63269030);int cases=0;
    for(int test=0;test<577;test++){
        memcpy(original.ram,ram,EE_RAM_SIZE);memcpy(original.spr,spr,EE_SPR_SIZE);memset(original.r,0,sizeof original.r);original.end_reason=0;
        uint64_t balance=test==0?gt4_career_money_get(ram+profile):test<=64?values[(test-1)/8]:random64(&seed);
        uint64_t delta=test==0?0:test<=64?values[(test-1)%8]:random64(&seed);
        gt4_career_money_set(original.ram+profile,balance);
        memcpy(native,original.ram,EE_RAM_SIZE);
        /* Match the original ABI's dead stack stores, not game logic. The
           rewritten function itself neither uses EE nor touches the stack. */
        const uint32_t stack=0x01ef0000;
        original.r[4].uw[0]=profile;original.r[5].ud[0]=delta;original.r[29].uw[0]=stack;
        original.r[16].ud[0]=0x1234;original.r[17].ud[0]=0x5678;original.r[31].ud[0]=0;
        memcpy(native+stack-0x20,&original.r[16].ud[0],8);memcpy(native+stack-0x18,&original.r[17].ud[0],8);memcpy(native+stack-0x10,&original.r[31].ud[0],8);
        /* Getter saves the setter-call return address on its nested stack. */
        uint64_t getter_ra=0x0043afd0;memcpy(native+stack-0x30,&getter_ra,8);
        if(setjmp(jump)){fprintf(stderr,"original stopped: %s\n",original.end_detail);return 1;}
        fn(&original);gt4_career_money_inc(native+profile,delta);
        if(memcmp(original.ram,native,EE_RAM_SIZE) || memcmp(original.spr,spr,EE_SPR_SIZE)){
            for(uint32_t i=0;i<EE_RAM_SIZE;i++)if(original.ram[i]!=native[i]){fprintf(stderr,"case %d RAM mismatch at %08x: %02x/%02x\n",test,i,original.ram[i],native[i]);break;}return 1;
        }
        cases++;
    }
    printf("func_0043AFB0: %d/%d identical; full RAM=%u bytes, SPR=%u bytes per case\n",cases,cases,EE_RAM_SIZE,EE_SPR_SIZE);
    free(native);free(spr);free(ram);ee_free(&original);return 0;
}
