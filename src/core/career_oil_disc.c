#include "career_original.h"
#include "career_float.h"
#include "vol.h"
#include "inflate.h"
#include <stdlib.h>
#include <string.h>
static uint32_t u32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int gt4_career_oil_data_load(const char *iso,gt4_career_oil_data *out){
    size_t size=0;uint8_t *raw=gt4_disc_read_root(iso,"CORE.GT4",&size),*decoded=NULL;
    gt4_career_oil_data data;int params=0,settings=0,clutch=0,dirt=0,code=0,ok=0;
    if(!raw || !out || size<6 || raw[0]!=1 || raw[1]!=1)goto done;
    uint32_t n=u32(raw+2);if(n<0x12c || n>32u*1024*1024)goto done;
    decoded=malloc(n);if(!decoded || core_inflate_raw(raw+6,size-6,decoded,n)!=n || u32(decoded+0x100)!=0x176265b3)goto done;
    for(size_t at=0x12c;at<n;){
        if(n-at<8)goto done;
        uint32_t base=u32(decoded+at),length=u32(decoded+at+4);at+=8;if(length>n-at)goto done;
        if(base<=0x620320 && (uint64_t)base+length>=0x620320+sizeof data.params){memcpy(data.params,decoded+at+0x620320-base,sizeof data.params);params=1;}
        if(base<=0x620cd8 && (uint64_t)base+length>=0x620cd8+sizeof data.settings){memcpy(data.settings,decoded+at+0x620cd8-base,sizeof data.settings);settings=1;}
        if(base<=0x620af0 && (uint64_t)base+length>=0x620af0+sizeof data.dirt_params){memcpy(data.dirt_params,decoded+at+0x620af0-base,sizeof data.dirt_params);dirt=1;}
        /* 003660D8 -> 00366100 constructs D_00620B18: signed lh
           at D_00620AF0+6, cvt.s.w, mul.s by 1000. The BSS is zero
           on disc; load the source parameters, not the uninitialized BSS. */
        if(base<=0x620af6 && (uint64_t)base+length>=0x620af8){
            const uint8_t *p=decoded+at+0x620af6-base;
            uint32_t bits=p[0]|(uint32_t)p[1]<<8;int32_t v=bits<32768?(int32_t)bits:(int32_t)bits-65536;
            uint32_t maximum=career_f32_mul(career_f32_from_int(v),0x447a0000u);
            for(unsigned i=0;i<4;i++)data.clutch_maximum[i]=(uint8_t)(maximum>>(8*i));clutch=1;
        }
        if(base<=0x343928 && (uint64_t)base+length>=0x343928+0x4c){const uint8_t *p=decoded+at+0x343928-base;code=u32(p)==0x27bdfff0 && u32(p+4)==0xffb00000;}
        at+=length;
    }
    ok=params && settings && clutch && dirt && code;if(ok)*out=data;
done:free(decoded);free(raw);return ok;
}
