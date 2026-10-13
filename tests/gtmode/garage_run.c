#include "ee.h"
#include "career_original.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t addr);
static int load(const char *path,uint8_t *out,size_t size){FILE *f=fopen(path,"rb");if(!f)return 0;int ok=fread(out,1,size,f)==size && fgetc(f)==EOF;fclose(f);return ok;}
static uint32_t random32(uint32_t *s){*s^=*s<<13;*s^=*s>>17;*s^=*s<<5;return *s;}
int main(int argc,char **argv){
    if(argc!=4){fprintf(stderr,"usage: gtmode_garage_oracle RAM SPR garage-address\n");return 2;}
    uint32_t garage=(uint32_t)strtoul(argv[3],NULL,0);if(garage>EE_RAM_SIZE-0x81cc)return 2;
    ee_ctx c;if(ee_init(&c))return 2;
    uint8_t *ram=malloc(EE_RAM_SIZE),*spr=malloc(EE_SPR_SIZE),*expected=malloc(EE_RAM_SIZE);
    if(!ram || !spr || !expected || !load(argv[1],ram,EE_RAM_SIZE) || !load(argv[2],spr,EE_SPR_SIZE))return 2;
    const uint32_t ranges[][2]={{0x435348,0x435388},{0x435920,0x435954},{0x435860,0x4358c4},{0x434e08,0x434e1c}};
    FILE *f=fopen("extracted/core.bin","rb");if(!f)return 2;
    for(unsigned i=0;i<4;i++){
        uint8_t code[128];size_t size=ranges[i][1]-ranges[i][0];
        if(fseek(f,ranges[i][0]-0x100000,SEEK_SET) || fread(code,1,size,f)!=size || memcmp(code,ram+ranges[i][0],size))return 2;
    }
    fclose(f);c.lookup=recomp_lookup;jmp_buf jump;c.exit_jmp=&jump;uint32_t seed=0x30a92026;
    for(unsigned op=0;op<2;op++){
        ee_fn fn=recomp_lookup(ranges[op][0]);if(!fn)return 2;
        for(unsigned test=0;test<1130;test++){
            memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);memset(c.r,0,sizeof c.r);c.end_reason=0;
            if(test)for(unsigned i=0;i<1000;i++){
                uint32_t flags=random32(&seed);
                if(test<=1001)flags=(flags|1u)&(i+2==test?~1u:UINT32_MAX);
                memcpy(c.ram+garage+0x10+i*32,&flags,4);
            }
            memcpy(expected,c.ram,EE_RAM_SIZE);
            int result=op?(int)gt4_career_garage_count(expected+garage):gt4_career_garage_first_free(expected+garage);
            c.r[4].uw[0]=garage;c.r[29].uw[0]=0x01ef0000;
            if(setjmp(jump)){fprintf(stderr,"original stopped: %s\n",c.end_detail);return 1;}
            fn(&c);
            if(c.r[2].sd[0]!=result || memcmp(c.ram,expected,EE_RAM_SIZE) || memcmp(c.spr,spr,EE_SPR_SIZE)){
                fprintf(stderr,"func=%08x case=%u return=%lld/%d\n",ranges[op][0],test,(long long)c.r[2].sd[0],result);return 1;
            }
        }
        printf("func_%08X: 1130/1130 identical; full RAM and SPR per case\n",ranges[op][0]);fflush(stdout);
    }
    for(unsigned trial=0;trial<2000;trial++){
        memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);memset(c.r,0,sizeof c.r);
        uint32_t index=trial%1000,selected=trial<1000?index:UINT32_MAX;
        for(unsigned i=0;i<32;i++)c.ram[garage+8+index*32+i]=(uint8_t)random32(&seed);
        memcpy(c.ram+garage+0x81c8,&selected,4);memcpy(expected,c.ram,EE_RAM_SIZE);
        int result=gt4_career_garage_delete(expected+garage,index);uint64_t zero=0;const uint32_t stack=0x1ef0000;
        memcpy(expected+stack-32,&zero,8);memcpy(expected+stack-24,&zero,8);memcpy(expected+stack-16,&zero,8);
        c.r[4].uw[0]=garage;c.r[5].uw[0]=index;c.r[29].uw[0]=stack;c.end_reason=0;
        if(setjmp(jump))return 1;recomp_lookup(0x435860)(&c);
        if(c.r[2].sd[0]!=result || memcmp(c.ram,expected,EE_RAM_SIZE) || memcmp(c.spr,spr,EE_SPR_SIZE)){
            fprintf(stderr,"delete mismatch index=%u selected=%u\n",index,selected);return 1;
        }
    }
    puts("func_00435860: 2000/2000 identical delete cases; full RAM and SPR per case");
    free(expected);free(spr);free(ram);ee_free(&c);return 0;
}
