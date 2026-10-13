/* Diagnostic only: executing an original function is not proof of a port.
   No capture is written back. @text arguments live in private guest scratch. */
#include "ee.h"
#include "fpu.h"
#include "thread.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t addr);
static int load(const char *path,uint8_t *out,size_t size){
    FILE *f=fopen(path,"rb");if(!f)return 0;
    int ok=fread(out,1,size,f)==size && fgetc(f)==EOF;fclose(f);return ok;
}
static ee_fn scheduled_function;
static void scheduled_entry(ee_ctx *c){ee_fpu_enter();scheduled_function(c);ee_fpu_leave();}
int main(int argc,char **argv){
    if(argc<4 || argc>12){fprintf(stderr,"usage: gtmode_spec_probe RAM SPR function [a0..a7; @string allowed]\n");return 2;}
    ee_ctx c;if(ee_init(&c))return 2;
    if(!load(argv[1],c.ram,EE_RAM_SIZE) || !load(argv[2],c.spr,EE_SPR_SIZE))return 2;
    uint32_t address=(uint32_t)strtoul(argv[3],NULL,0);
    FILE *f=fopen("extracted/core.bin","rb");uint8_t code[32];if(!f)return 2;
    int valid=address>=0x100000 && address<EE_RAM_SIZE-32 && !fseek(f,address-0x100000,SEEK_SET) && fread(code,1,32,f)==32 && !memcmp(code,c.ram+address,32);
    fclose(f);if(!valid){fprintf(stderr,"entry instructions differ from snapshot\n");return 2;}
    c.lookup=recomp_lookup;ee_fn fn=recomp_lookup(address);if(!fn){fprintf(stderr,"function not generated\n");return 2;}
    if(argc==7 && !strcmp(argv[4],"--car")){
        const uint32_t car=0x01edf000;uint64_t key=strtoull(argv[5],NULL,0);
        uint32_t mileage=(uint32_t)strtoul(argv[6],NULL,0);
        /* func_00441280/00441260: setup 0 at car+8, key at setup+0.
           func_00441128 reads the unsigned odometer at car+0x4a0. */
        memset(c.ram+car,0,0x4b8);memcpy(c.ram+car+8,&key,8);memcpy(c.ram+car+0x4a0,&mileage,4);
        c.r[4].uw[0]=car;
    }else for(int i=4;i<argc;i++){
        if(argv[i][0]=='@'){
            size_t size=strlen(argv[i]+1)+1;if(size>256)return 2;
            uint32_t slot=0x01ee0000+(uint32_t)(i-4)*256;
            memcpy(c.ram+slot,argv[i]+1,size);c.r[i].uw[0]=slot;
        }else c.r[i].ud[0]=strtoull(argv[i],NULL,0);
    }
    c.r[29].uw[0]=0x01ef0000;jmp_buf jump;c.exit_jmp=&jump;
    ee_fpu_enter();
    if(setjmp(jump)){ee_fpu_leave();fprintf(stderr,"original stopped at %08x: %s\n",c.end_pc,c.end_detail);ee_free(&c);return 1;}
    if(getenv("GT4_PROBE_SCHEDULER")) {
        scheduled_function=fn;
        if(ee_sched_run(&c,scheduled_entry)!=0)return 2;
        if(c.end_reason!=EE_END_RETURNED){
            ee_fpu_leave();fprintf(stderr,"original stopped at %08x: %s\n",c.end_pc,c.end_detail);ee_free(&c);return 1;
        }
    }else fn(&c);
    ee_fpu_leave();
    printf("diagnostic func_%08X: v0=%016llx v1=%016llx trace_seq=%llu (no equivalence claim)\n",address,(unsigned long long)c.r[2].ud[0],(unsigned long long)c.r[3].ud[0],(unsigned long long)c.trace_seq);
    ee_free(&c);return 0;
}
