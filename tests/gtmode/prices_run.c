/* Full original SpecDB path, no helper stubs. Return-value equivalence only:
   transient stack and original SpecDB cache writes are not yet normalized. */
#include "ee.h"
#include "fpu.h"
#include "career_prices.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t addr);
static int load(const char *path,uint8_t *out,size_t size){FILE *f=fopen(path,"rb");if(!f)return 0;int ok=fread(out,1,size,f)==size && fgetc(f)==EOF;fclose(f);return ok;}
static uint32_t le32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int main(int argc,char **argv){
    if(argc!=4){fprintf(stderr,"usage: gtmode_price_oracle RAM SPR ISO\n");return 2;}
    ee_ctx c;if(ee_init(&c))return 2;
    uint8_t *ram=malloc(EE_RAM_SIZE),*spr=malloc(EE_SPR_SIZE);
    if(!ram || !spr || !load(argv[1],ram,EE_RAM_SIZE) || !load(argv[2],spr,EE_SPR_SIZE))return 2;
    gt4_career_price_data data;if(!gt4_career_price_data_load(argv[3],&data) || memcmp(data.mileage_factors,ram+0x6a6988,80)){fprintf(stderr,"disc/capture mileage table differs\n");return 2;}
    const char *error=NULL;gt4_vol *vol=gt4_vol_open(argv[3],&error);if(!vol){fprintf(stderr,"disc: %s\n",error);return 2;}
    gt4_spec_tables *tables=gt4_spec_tables_open(vol,NULL);if(!tables)return 2;
    FILE *f=fopen("extracted/core.bin","rb");if(!f)return 2;
    uint8_t code[256];if(fseek(f,0x341128,SEEK_SET) || fread(code,1,sizeof code,f)!=sizeof code || memcmp(code,ram+0x441128,sizeof code)){fprintf(stderr,"price instructions differ\n");return 2;}fclose(f);
    uint32_t labels=le32(ram+0x623630);if(labels>EE_RAM_SIZE-4)return 2;
    labels=le32(ram+labels);if(labels>EE_RAM_SIZE-16)return 2;
    uint32_t cars=le32(ram+labels+4);if(!cars || cars>4096 || labels>EE_RAM_SIZE-16-cars*8)return 2;
    uint32_t mileage[40]={0,1,999999,UINT32_MAX};unsigned mileages=4;
    for(unsigned i=0;i<10;i++){
        uint32_t threshold=le32(data.mileage_factors+i*8);
        if(threshold && threshold<=UINT32_MAX/1000000u){
            uint32_t boundary=threshold*1000000u;
            mileage[mileages++]=boundary-1;mileage[mileages++]=boundary;mileage[mileages++]=boundary+1;
        }
    }
    c.lookup=recomp_lookup;ee_fn used=recomp_lookup(0x441128),sale=recomp_lookup(0x4411f0);if(!used || !sale)return 2;
    jmp_buf jump;c.exit_jmp=&jump;const uint32_t car=0x01edf000;unsigned cases=0;
    for(unsigned i=0;i<cars;i++)for(unsigned m=0;m<mileages;m++)for(unsigned mode=0;mode<2;mode++){
        uint64_t key=le32(ram+labels+16+i*8+4);int32_t price;
        if(!gt4_career_used_price(tables,&data,key,mileage[m],&price)){fprintf(stderr,"native missing key=%llu\n",(unsigned long long)key);return 1;}
        if(mode)price=gt4_career_sell_price_value(price);
        memcpy(c.ram,ram,EE_RAM_SIZE);memcpy(c.spr,spr,EE_SPR_SIZE);memset(c.r,0,sizeof c.r);c.end_reason=0;
        memset(c.ram+car,0,0x4b8);memcpy(c.ram+car+8,&key,8);memcpy(c.ram+car+0x4a0,&mileage[m],4);
        uint8_t before[0x4b8];memcpy(before,c.ram+car,sizeof before);
        c.r[4].uw[0]=car;c.r[29].uw[0]=0x01ef0000;ee_fpu_enter();
        if(setjmp(jump)){ee_fpu_leave();fprintf(stderr,"key=%llu mileage=%u stopped: %s\n",(unsigned long long)key,mileage[m],c.end_detail);return 1;}
        (mode?sale:used)(&c);ee_fpu_leave();
        if(c.r[2].sw[0]!=price || memcmp(c.ram+car,before,sizeof before) || memcmp(c.spr,spr,EE_SPR_SIZE)){
            fprintf(stderr,"key=%llu mileage=%u sale=%u actual=%d native=%d\n",(unsigned long long)key,mileage[m],mode,c.r[2].sw[0],price);return 1;
        }
        cases++;
        if(i%100==0 && m==0 && mode==0){printf("car %u/%u, %u identical returns so far\n",i+1,cars,cases);fflush(stdout);}
    }
    printf("func_00441128/004411F0: %u/%u identical returns; %u cars, %u odometers, both price modes; car bytes and SPR unchanged\n",cases,cases,cars,mileages);
    gt4_spec_tables_close(tables);gt4_vol_close(vol);free(spr);free(ram);ee_free(&c);return 0;
}
