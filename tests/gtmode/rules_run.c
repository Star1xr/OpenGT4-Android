/* Original prize and mileage routines, test-only. Captures stay read-only. */
#include "ee.h"
#include "career_original.h"
#include "specdb.h"
#include "vol.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t address);
static int load(const char *path,uint8_t *out,size_t n){FILE *f=fopen(path,"rb");if(!f)return 0;int ok=fread(out,1,n,f)==n && fgetc(f)==EOF;fclose(f);return ok;}
int main(int argc,char **argv){
    if(argc!=4){fprintf(stderr,"usage: gtmode_rules_oracle RAM SPR ISO\n");return 2;}ee_ctx c;if(ee_init(&c))return 2;
    uint8_t *ram=malloc(EE_RAM_SIZE),*spr=malloc(EE_SPR_SIZE),*expected=malloc(EE_RAM_SIZE);
    if(!ram || !spr || !expected || !load(argv[1],ram,EE_RAM_SIZE) || !load(argv[2],spr,EE_SPR_SIZE))return 2;
    const uint32_t addr[]={0x4475a8,0x3434a8},length[]={0x6c,0x1c};
    FILE *f=fopen("extracted/core.bin","rb");if(!f)return 2;
    for(unsigned i=0;i<2;i++){uint8_t code[0x6c];if(fseek(f,addr[i]-0x100000,SEEK_SET) || fread(code,1,length[i],f)!=length[i] || memcmp(code,ram+addr[i],length[i]))return 2;}
    uint8_t jumps[24];if(fseek(f,0x5a6f00,SEEK_SET) || fread(jumps,1,24,f)!=24 || memcmp(jumps,ram+0x6a6f00,24))return 2;fclose(f);
    c.lookup=recomp_lookup;jmp_buf jump;c.exit_jmp=&jump;uint32_t seed=0x9302026,object=0x1edf000;
    const uint64_t edges[]={0,1,5,6,0x5f5e0fff,0x5f5e1000,0x5f5e1001,0xffffffff,0x100000000,UINT64_MAX,0x8000000000000000};
    for(unsigned op=0;op<2;op++)for(unsigned trial=0;trial<1024;trial++){
        memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);memset(c.r,0,sizeof c.r);
        for(unsigned i=0;i<0x80;i++){seed=seed*1664525u+1013904223u;c.ram[object+i]=(uint8_t)(seed>>24);}
        uint64_t arg;if(trial<sizeof edges/sizeof *edges)arg=edges[trial];else if(!op)arg=trial%8;else {seed=seed*1664525u+1013904223u;arg=seed;if(trial&1)arg|=(uint64_t)seed<<32;}
        memcpy(expected,c.ram,EE_RAM_SIZE);uint32_t prize=0;
        if(op)gt4_career_odometer_set(expected+object,arg);else prize=gt4_career_race_prize(expected+object,arg);
        c.r[4].uw[0]=object;c.r[5].ud[0]=arg;c.r[29].uw[0]=0x1ef0000;c.end_reason=0;
        if(setjmp(jump)){fprintf(stderr,"original stopped: %s\n",c.end_detail);return 1;}recomp_lookup(addr[op])(&c);
        if((!op && c.r[2].uw[0]!=prize) || memcmp(c.ram,expected,EE_RAM_SIZE) || memcmp(c.spr,spr,EE_SPR_SIZE)){fprintf(stderr,"rule %08x case %u differs\n",addr[op],trial);return 1;}
        if(trial==1023)printf("func_%08X: 1024/1024 identical; full RAM and Scratchpad\n",addr[op]);
    }
    uint32_t wrapper,table,count;memcpy(&wrapper,ram+0x623768,4);
    if(wrapper>EE_RAM_SIZE-4)return 2;memcpy(&table,ram+wrapper,4);
    if(table>EE_RAM_SIZE-16)return 2;memcpy(&count,ram+table+8,4);
    if(count>(EE_RAM_SIZE-table-16)/8)return 2;
    uint8_t checked[0x7c];f=fopen("extracted/core.bin","rb");if(!f)return 2;
    if(fseek(f,0x349190,SEEK_SET) || fread(checked,1,sizeof checked,f)!=sizeof checked || memcmp(checked,ram+0x449190,sizeof checked))return 2;fclose(f);
    const char *error=NULL;gt4_vol *volume=gt4_vol_open(argv[3],&error);if(!volume)return 2;
    size_t disk_size=0;uint8_t *disk=gt4_vol_read(volume,"/specdb/GT4_US2560/RACE.dbt",&disk_size);gt4_vol_close(volume);
    size_t directory_size=16+(size_t)count*8;
    if(!disk || disk_size<directory_size || memcmp(disk,ram+table,directory_size)){fprintf(stderr,"disc RACE directory differs from captured original\n");return 2;}
    memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);
    for(unsigned trial=0;trial<65540;trial++){
        uint64_t key=trial<65536?trial:trial==65536?UINT64_MAX:trial==65537?0x100000000:trial==65538?0x8000000000000000:0x7fffffffffffffff;
        memset(c.r,0,sizeof c.r);c.r[4].uw[0]=wrapper;c.r[5].ud[0]=key;c.end_reason=0;
        if(setjmp(jump))return 1;recomp_lookup(0x449190)(&c);
        int index=gt4_specdb_row_index(disk,disk_size,key);
        if(c.r[2].sd[0]!=index){fprintf(stderr,"record index key=%llu differs %lld/%d\n",(unsigned long long)key,(long long)c.r[2].sd[0],index);return 1;}
        if((trial%512==511 || trial==65539) && (memcmp(c.ram,ram,EE_RAM_SIZE) || memcmp(c.spr,spr,EE_SPR_SIZE)))return 1;
    }
    puts("func_00449190: 65540/65540 identical indices against captured original RACE table; memory unchanged every 512 calls and at end");
    free(disk);free(expected);free(spr);free(ram);ee_free(&c);return 0;
}
