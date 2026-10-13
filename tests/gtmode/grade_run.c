/* Test-only original function; generated code never links into the app. */
#include "ee.h"
#include "career_original.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t addr);
static int load(const char *path,uint8_t *out,size_t size){
    FILE *f=fopen(path,"rb");if(!f)return 0;
    int ok=fread(out,1,size,f)==size && fgetc(f)==EOF;fclose(f);return ok;
}
int main(int argc,char **argv){
    if(argc!=3){fprintf(stderr,"usage: gtmode_grade_oracle RAM SPR\n");return 2;}
    ee_ctx c;if(ee_init(&c))return 2;
    uint8_t *ram=malloc(EE_RAM_SIZE),*spr=malloc(EE_SPR_SIZE);
    if(!ram || !spr || !load(argv[1],ram,EE_RAM_SIZE) || !load(argv[2],spr,EE_SPR_SIZE))return 2;
    FILE *f=fopen("extracted/core.bin","rb");uint8_t code[40];
    if(!f)return 2;
    int valid=!fseek(f,0x330c28,SEEK_SET) && fread(code,1,sizeof code,f)==sizeof code && !memcmp(code,ram+0x430c28,sizeof code);
    fclose(f);if(!valid){fprintf(stderr,"captured grade code differs\n");return 2;}
    memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);
    c.lookup=recomp_lookup;ee_fn fn=recomp_lookup(0x430c28);if(!fn)return 2;
    jmp_buf jump;c.exit_jmp=&jump;
    const uint32_t input=0x01eff000;unsigned cases=0;
    for(unsigned record=0;record<256;record++){
        ram[input]=(uint8_t)record;c.ram[input]=(uint8_t)record;
        for(unsigned trial=0;trial<512;trial++){
            /* All byte limits, repeated with nonzero bits above the low byte. */
            uint32_t limit=(trial&255u)|(trial>=256?0xffffa500u:0u);
            memset(c.r,0,sizeof c.r);c.end_reason=0;
            c.r[4].uw[0]=input;c.r[5].uw[0]=limit;c.r[29].uw[0]=0x01ef0000;
            if(setjmp(jump)){fprintf(stderr,"original stopped: %s\n",c.end_detail);return 1;}
            fn(&c);
            int expected=gt4_career_record_meets_grade((uint8_t)record,limit);
            if(c.r[2].ud[0]!=(uint64_t)expected){fprintf(stderr,"record=%u limit=%08x mismatch\n",record,limit);return 1;}
            cases++;
        }
        /* Original is a leaf with no stores. Verify full memory after every
           512-call group as well as its return value on every invocation. */
        if(memcmp(c.ram,ram,EE_RAM_SIZE) || memcmp(c.spr,spr,EE_SPR_SIZE)){
            fprintf(stderr,"grade predicate mutated memory for record=%u\n",record);return 1;
        }
    }
    printf("func_00430C28: %u/%u identical returns; full RAM/SPR unchanged in 256 groups\n",cases,cases);
    free(spr);free(ram);ee_free(&c);return 0;
}
