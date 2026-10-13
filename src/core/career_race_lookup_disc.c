#include "career_race_lookup.h"
#include "vol.h"
#include "inflate.h"
#include <stdlib.h>
#include <string.h>
static uint32_t u32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static const uint8_t *mapped(const uint8_t *data,size_t n,uint32_t address,size_t size){
    for(size_t at=0x12c;at<n;){
        if(n-at<8)return NULL;uint32_t base=u32(data+at),length=u32(data+at+4);at+=8;
        if(length>n-at)return NULL;
        if(base<=address && (uint64_t)address+size<=(uint64_t)base+length)return data+at+address-base;
        at+=length;
    }return NULL;
}
int gt4_career_race_lookup_data_load(const char *iso,gt4_career_race_lookup_data *out){
    size_t size=0;uint8_t *raw=gt4_disc_read_root(iso,"CORE.GT4",&size),*decoded=NULL;
    int found=0,code=0,grades=0,points=0,ok=0;gt4_career_race_lookup_data candidate={0};
    if(!raw || size<6 || raw[0]!=1 || raw[1]!=1)goto done;
    uint32_t n=u32(raw+2);if(n<0x12c || n>32u*1024*1024)goto done;
    decoded=malloc(n);if(!decoded || core_inflate_raw(raw+6,size-6,decoded,n)!=n || u32(decoded+0x100)!=0x176265b3)goto done;
    for(size_t at=0x12c;at<n;){
        if(n-at<8)goto done;
        uint32_t base=u32(decoded+at),length=u32(decoded+at+4);at+=8;if(length>n-at)goto done;
        if(base<=0x6a6ef0 && (uint64_t)base+length>=0x6a6ef6){
            memcpy(candidate.driver_points,decoded+at+0x6a6ef0-base,6);points=1;
        }
        const uint32_t addresses[]={0x68bb20,0x692800};
        for(unsigned i=0;i<2;i++)if(base<=addresses[i] && (uint64_t)base+length>=addresses[i]+3){
            memcpy(candidate.prefixes[i],decoded+at+addresses[i]-base,3);found|=1<<i;
        }
        /* func_00431CE8 initializes the enum object from these 16 original
           {value,string pointer} pairs, consumed by func_00431C60. */
        if(base<=0x6a5548 && (uint64_t)base+length>=0x6a5648){
            for(unsigned i=0;i<16;i++){
                const uint8_t *entry=decoded+at+0x6a55c8-base+i*8;
                uint32_t value=u32(entry),address=u32(entry+4);
                if(value>255 || address<base || (uint64_t)address+16>(uint64_t)base+length)goto done;
                const char *name=(const char *)decoded+at+address-base;const char *end=memchr(name,0,16);
                if(!end)goto done;candidate.grade_values[i]=(uint8_t)value;
                memcpy(candidate.grade_names[i],name,(size_t)(end-name)+1);
            }grades=1;
        }
        if(base<=0x430e58 && (uint64_t)base+length>=0x430e58+0x40){
            const uint8_t *p=decoded+at+0x430e58-base;
            code=u32(p)==0x27bdffa0 && u32(p+0x2c)==0x3c050069 && u32(p+0x3c)==0x24a5bb20;
        }
        at+=length;
    }
    ok=found==3 && code && grades && points && candidate.prefixes[0][2]==0 && candidate.prefixes[1][2]==0;
    if(ok){
        const uint8_t *types=mapped(decoded,n,0x622d80,16);if(!types){ok=0;goto done;}
        for(unsigned i=0;i<4;i++){
            const uint8_t *name=mapped(decoded,n,u32(types+i*4),16);if(!name || !memchr(name,0,16)){ok=0;goto done;}
            memcpy(candidate.type_names[i],name,strlen((const char *)name)+1);
        }
        /* func_00431CE8 / 00431CB8: original race-driver/method enum. */
        const uint8_t *methods=mapped(decoded,n,0x6a56a8,15*8);if(!methods){ok=0;goto done;}
        for(unsigned i=0;i<15;i++){
            uint32_t value=u32(methods+i*8),address=u32(methods+i*8+4);
            const uint8_t *name=mapped(decoded,n,address,16);
            if(value>255 || !name || !memchr(name,0,16)){ok=0;goto done;}
            candidate.method_values[i]=(uint8_t)value;
            memcpy(candidate.method_names[i],name,strlen((const char *)name)+1);
        }
        *out=candidate;
    }
done:free(decoded);free(raw);return ok;
}
