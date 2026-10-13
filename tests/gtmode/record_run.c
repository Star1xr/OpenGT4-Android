#include "ee.h"
#include "career_original.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t addr);
static int load(const char *path,uint8_t *out,size_t size){
    FILE *f=fopen(path,"rb");if(!f)return 0;
    int ok=fread(out,1,size,f)==size && fgetc(f)==EOF;fclose(f);return ok;
}
static uint32_t random32(uint32_t *s){*s^=*s<<13;*s^=*s>>17;*s^=*s<<5;return *s;}
int main(int argc,char **argv){
    if(argc!=3){fprintf(stderr,"usage: gtmode_record_oracle RAM SPR\n");return 2;}
    ee_ctx c;if(ee_init(&c))return 2;
    uint8_t *ram=malloc(EE_RAM_SIZE),*spr=malloc(EE_SPR_SIZE),*native=malloc(EE_RAM_SIZE);
    if(!ram || !spr || !native || !load(argv[1],ram,EE_RAM_SIZE) || !load(argv[2],spr,EE_SPR_SIZE))return 2;
    const uint32_t ranges[][2]={{0x430958,0x430974},{0x430a10,0x430a20},{0x430a20,0x430a58},{0x430a58,0x430b54},{0x430bc0,0x430c24},{0x430c50,0x430cb8},{0x430b58,0x430bc0}};
    FILE *f=fopen("extracted/core.bin","rb");if(!f)return 2;
    for(unsigned i=0;i<7;i++){
        uint8_t code[256];size_t size=ranges[i][1]-ranges[i][0];
        if(fseek(f,ranges[i][0]-0x100000,SEEK_SET) || fread(code,1,size,f)!=size || memcmp(code,ram+ranges[i][0],size)){
            fprintf(stderr,"captured code differs at %08x\n",ranges[i][0]);fclose(f);return 2;
        }
    }
    fclose(f);c.lookup=recomp_lookup;jmp_buf jump;c.exit_jmp=&jump;
    const uint32_t input=0x01eff000,stack=0x01ef0000;
    uint32_t seed=0x30ca2026;
    const int32_t boundaries[]={INT32_MIN,-256,-255,-1,0,1,254,255,256,INT32_MAX};
    for(unsigned op=0;op<7;op++){
        ee_fn fn=recomp_lookup(ranges[op][0]);if(!fn)return 2;
        unsigned count=op==3?8192:4096;
        for(unsigned test=0;test<count;test++){
            uint32_t word=(random32(&seed)&0xffffff00u)|(test&255u),expected=word;
            int32_t argument=boundaries[test%10];uint32_t grade=random32(&seed);int result=0;
            if(op==3 && test<4096){word=(word&0xfffff000u)|(test&15u)|(((test>>4)&15u)<<4);expected=word;argument=(int32_t)(test&15u);grade=(test>>8)&15u;}
            memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);memset(c.r,0,sizeof c.r);c.end_reason=0;
            memcpy(c.ram+input,&word,4);memcpy(native,c.ram,EE_RAM_SIZE);
            c.r[4].uw[0]=input;c.r[5].sd[0]=argument;c.r[6].uw[0]=grade;c.r[29].uw[0]=stack;
            if(op==0)expected=gt4_career_record_init(word);
            if(op==1)expected=gt4_career_record_present(word);
            if(op==2)result=gt4_career_record_score(&expected,argument);
            if(op==3){
                result=gt4_career_record_update(&expected,argument,grade);
                /* ABI-only stack writes in func_00430A58. */
                uint32_t kind=word&15u;memcpy(native+stack-16,&argument,4);memcpy(native+stack-12,&kind,4);
            }
            if(op==4)result=gt4_career_record_predicate_00430bc0(word);
            if(op==5)result=gt4_career_record_predicate_00430c50(word);
            if(op==6)result=gt4_career_record_predicate_00430b58(word);
            memcpy(native+input,&expected,4);
            if(setjmp(jump)){fprintf(stderr,"original stopped: %s\n",c.end_detail);return 1;}
            fn(&c);
            if((op>=2 && c.r[2].ud[0]!=(uint64_t)result) || memcmp(c.ram,native,EE_RAM_SIZE) || memcmp(c.spr,spr,EE_SPR_SIZE)){
                fprintf(stderr,"func=%08x case=%u word=%08x argument=%d grade=%08x return=%llu/%d\n",ranges[op][0],test,word,argument,grade,(unsigned long long)c.r[2].ud[0],result);
                for(uint32_t i=0;i<EE_RAM_SIZE;i++)if(c.ram[i]!=native[i]){fprintf(stderr,"RAM mismatch at %08x: %02x/%02x\n",i,c.ram[i],native[i]);break;}
                return 1;
            }
        }
        printf("func_%08X: %u/%u identical, full RAM and SPR per case\n",ranges[op][0],count,count);fflush(stdout);
    }
    free(native);free(spr);free(ram);ee_free(&c);return 0;
}
