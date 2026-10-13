/* Decode every original RACE row from the disc and compare with original EE
   SpecDB loading. This validates row bytes, not complete race outcomes. */
#include "ee.h"
#include "career_original.h"
#include "specdb.h"
#include "vol.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t address);
static int load(const char *path,uint8_t *out,size_t n){FILE *f=fopen(path,"rb");if(!f)return 0;int ok=fread(out,1,n,f)==n && fgetc(f)==EOF;fclose(f);return ok;}
static uint32_t read32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int main(int argc,char **argv){
    if(argc!=4)return 2;ee_ctx c;if(ee_init(&c))return 2;
    uint8_t *ram=malloc(EE_RAM_SIZE),*spr=malloc(EE_SPR_SIZE);
    if(!ram || !spr || !load(argv[1],ram,EE_RAM_SIZE) || !load(argv[2],spr,EE_SPR_SIZE))return 2;
    FILE *f=fopen("extracted/core.bin","rb");uint8_t code[32];if(!f)return 2;
    if(fseek(f,0x343ed0,SEEK_SET) || fread(code,1,32,f)!=32 || memcmp(code,ram+0x443ed0,32))return 2;fclose(f);
    const char *error=NULL;gt4_vol *volume=gt4_vol_open(argv[3],&error);if(!volume)return 2;
    size_t n=0;uint8_t *db=gt4_vol_read(volume,"/specdb/GT4_US2560/RACE.dbt",&n);gt4_vol_close(volume);
    if(!db || n<16 || memcmp(db,"GTDB",4))return 2;unsigned count=read32(db+8),width=read32(db+12);
    if(count>(n-16)/8 || width>256)return 2;
    const uint32_t timers[]={0x447670,0x4476b0,0x4476f0};
    f=fopen("extracted/core.bin","rb");if(!f)return 2;
    for(unsigned i=0;i<3;i++){uint8_t checked[64];if(fseek(f,timers[i]-0x100000,SEEK_SET) || fread(checked,1,64,f)!=64 || memcmp(checked,ram+timers[i],64))return 2;}fclose(f);
    c.lookup=recomp_lookup;jmp_buf jump;c.exit_jmp=&jump;
    for(unsigned row=0;row<count;row++){
        memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);memset(c.r,0,sizeof c.r);
        uint32_t id=read32(db+16+row*8);uint8_t expected[256];memset(expected,0xa5,256);memset(c.ram+0x1edf000,0xa5,256);
        if(gt4_specdb_row_at(db,n,row,expected,256)!=(int)width)return 2;
        c.r[4].uw[0]=0x6235a8;c.r[5].ud[0]=((uint64_t)0x23<<32)|id;c.r[6].uw[0]=0x1edf000;c.r[29].uw[0]=0x1ef0000;c.end_reason=0;
        if(setjmp(jump)){fprintf(stderr,"row %u original stopped: %s\n",row,c.end_detail);return 1;}recomp_lookup(0x443ed0)(&c);
        if(!c.r[2].ud[0] || memcmp(c.ram+0x1edf000,expected,width)){
            fprintf(stderr,"row %u id=%u decoded bytes differ, original return=%llu\n",row,id,(unsigned long long)c.r[2].ud[0]);return 1;
        }
        for(unsigned medal=0;medal<3;medal++){
            memset(c.r,0,sizeof c.r);c.r[4].uw[0]=0x1edf000;c.end_reason=0;
            if(setjmp(jump))return 1;recomp_lookup(timers[medal])(&c);
            uint32_t time=gt4_career_license_time(expected,medal);
            if(c.r[2].uw[0]!=time || memcmp(c.ram+0x1edf000,expected,width)){fprintf(stderr,"row %u medal %u original timing differs\n",row,medal);return 1;}
        }
    }
    /* Zero sentinel and mixed byte extremes beyond the authored rows. */
    for(unsigned trial=0;trial<256;trial++){
        uint8_t expected[151]={0};for(unsigned i=0;i<151;i++)expected[i]=(uint8_t)trial;
        memcpy(c.ram+0x1edf000,expected,151);
        for(unsigned medal=0;medal<3;medal++){
            memset(c.r,0,sizeof c.r);c.r[4].uw[0]=0x1edf000;c.end_reason=0;
            if(setjmp(jump))return 1;recomp_lookup(timers[medal])(&c);
            if(c.r[2].uw[0]!=gt4_career_license_time(expected,medal) || memcmp(c.ram+0x1edf000,expected,151))return 1;
        }
    }
    printf("license gold/silver/bronze timers: %u/%u identical returns and input structures\n",(count+256)*3,(count+256)*3);
    printf("func_00443ED0 namespace 0x23: %u/%u identical disc-backed RACE rows, %u bytes per row; not a full-memory/native callback equivalence claim\n",count,count,width);
    free(db);free(spr);free(ram);ee_free(&c);return 0;
}
