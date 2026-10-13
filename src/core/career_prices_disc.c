#include "career_prices.h"
#include "inflate.h"
#include <stdlib.h>
#include <string.h>
static uint32_t le32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int gt4_career_price_data_load(const char *iso,gt4_career_price_data *out){
    if(!iso || !out)return 0;
    size_t size=0;uint8_t *raw=gt4_disc_read_root(iso,"CORE.GT4",&size),*data=NULL;int ok=0,code_matches=0;
    gt4_career_price_data candidate;
    if(!raw || size<6 || raw[0]!=1 || raw[1]!=1)goto done;
    uint32_t decoded=le32(raw+2);if(decoded<0x12c || decoded>32u*1024*1024)goto done;
    data=malloc(decoded);if(!data || core_inflate_raw(raw+6,size-6,data,decoded)!=decoded || le32(data+0x100)!=0x176265b3)goto done;
    /* tools/unpack_core.py: records {virtual address, size, bytes}. */
    for(size_t at=0x12c;at<decoded;){
        if(decoded-at<8)goto invalid;
        uint32_t address=le32(data+at),length=le32(data+at+4);at+=8;
        if(length>decoded-at)goto invalid;
        const uint32_t target=0x006a6988;
        if(address<=target && (uint64_t)target+80<=(uint64_t)address+length){
            memcpy(candidate.mileage_factors,data+at+(target-address),80);ok=1;
        }
        /* Verify this build's instruction/field/table addresses rather than
           accepting another region's data at the same numeric offset. */
        const uint32_t function=0x00441128;
        if(address<=function && (uint64_t)function+0x28<=(uint64_t)address+length){
            const uint8_t *code=data+at+(function-address);
            code_matches=le32(code)==0x27bdffd0u && le32(code+0x18)==0x3c04006au && le32(code+0x20)==0x8d0304a0u && le32(code+0x24)==0x2484698cu;
        }
        at+=length;
    }
    ok=ok && code_matches;if(ok)*out=candidate;goto done;
invalid:ok=0;
done:free(data);free(raw);return ok;
}
