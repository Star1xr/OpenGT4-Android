#include "career_parts.h"
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
#include "specdb.h"
#include <stdio.h>
static void part_label(gt4_vol *v,const char *idi,uint64_t key,char out[32]){
    out[0]=0;if(key==UINT64_MAX)return;size_t n=0;uint8_t *labels=gt4_vol_read(v,idi,&n);
    const char *label=labels?gt4_specdb_label_for_id(labels,n,(uint32_t)key):NULL;
    if(label && strlen(label)<32)snprintf(out,32,"%s",label);free(labels);
}
int gt4_body_part_labels(gt4_vol *v,const uint8_t *body,char wheel[32],char wing[32]){
    wheel[0]=wing[0]=0;if(!v || !body)return 0;uint64_t key;uint32_t slot;
    memcpy(&key,body+0x468,8);part_label(v,"/specdb/GT4_US2560/WHEEL.idi",key,wheel);
    memcpy(&slot,body+0x488,4);
    if(slot<3){memcpy(&key,body+slot*0x178+0x100,8);part_label(v,"/specdb/GT4_US2560/WING.idi",key,wing);}
    return 1;
}
int gt4_career_part_data_load(const char *iso,gt4_career_part_data *out){
    if(!iso || !out)return 0;size_t size=0;uint8_t *raw=gt4_disc_read_root(iso,"CORE.GT4",&size),*decoded=NULL;int ok=0;
    gt4_career_part_data candidate={0};
    if(!raw || size<6 || raw[0]!=1 || raw[1]!=1)goto done;
    uint32_t n=u32(raw+2);if(n<0x12c || n>32u*1024*1024)goto done;
    decoded=malloc(n);if(!decoded || core_inflate_raw(raw+6,size-6,decoded,n)!=n || u32(decoded+0x100)!=0x176265b3)goto done;
    const uint8_t *code=mapped(decoded,n,0x44a2e0,0x40),*branches=mapped(decoded,n,0x6a7b60,31*4),*names=mapped(decoded,n,0x6234b0,39*4);
    if(!code || !branches || !names || u32(code)!=0x27bdff50 || u32(code+0x14)!=0x0c112812 || u32(code+0x30)!=0x2c62001f)goto done;
    for(unsigned i=0;i<31;i++){
        candidate.price_branches[i]=u32(branches+i*4);
        if(candidate.price_branches[i]<0x44a338 || candidate.price_branches[i]>0x44a45c)goto done;
    }
    /* func_0043F270: decode return immediates selected by the original
       jump table, including its zero and invalid-type branches. */
    const uint8_t *owned=mapped(decoded,n,0x6a6790,31*4);if(!owned)goto done;
    for(unsigned i=0;i<31;i++){
        uint32_t target=u32(owned+i*4);
        if(target==0x43f298){candidate.owned_bases[i]=0;continue;}
        if(target==0x43f35c){candidate.owned_bases[i]=-1;continue;}
        const uint8_t *instruction=mapped(decoded,n,target==0x43f358?target:target+4,4);
        if(!instruction || (u32(instruction)&0xffff0000u)!=0x24040000u)goto done;
        candidate.owned_bases[i]=(int8_t)(int16_t)u32(instruction);
    }
    for(unsigned i=0;i<39;i++){
        const uint8_t *name=mapped(decoded,n,u32(names+i*4),24);if(!name || !memchr(name,0,24))goto done;
        memcpy(candidate.table_names[i],name,strlen((const char *)name)+1);
    }
    /* func_00448D20 -> 0057B168 / 0057B130 scans pairs until a null
       name pointer. Only keys reachable from lbu % 100 can be dereferenced
       by GetCategory. Retain first-match semantics for those keys. */
    int terminated=0;
    for(unsigned i=0;i<64;i++){
        const uint8_t *pair=mapped(decoded,n,0x6a6fe0+i*8,8);if(!pair)goto done;
        uint32_t key=u32(pair),address=u32(pair+4);
        if(!address){terminated=1;break;}
        if(key>=100 || candidate.category_names[key][0])continue;
        const uint8_t *name=mapped(decoded,n,address,24);if(!name || !memchr(name,0,24))goto done;
        memcpy(candidate.category_names[key],name,strlen((const char *)name)+1);
    }
    if(!terminated)goto done;
    const uint8_t *fallback=mapped(decoded,n,0x68e838,24);
    if(!fallback || !memchr(fallback,0,24))goto done;
    memcpy(candidate.category_fallback,fallback,strlen((const char *)fallback)+1);
    /* 0044A1B8 reads this byte table for normal-stage fallback. */
    const uint8_t *normal=mapped(decoded,n,0x6a7c08,32);if(!normal)goto done;
    memcpy(candidate.normal_flags,normal,32);
    /* 0044A1E0 +0x10..+0x18: maximum stages by category. */
    const uint8_t *maxima=mapped(decoded,n,0x6a7c28,32);if(!maxima)goto done;
    memcpy(candidate.type_max,maxima,32);
    /* 001516EC..001516FC initializes the four wheel-category names. */
    for(unsigned i=0;i<4;i++){
        const uint8_t *pair=mapped(decoded,n,0x68fb80+i*8,8);if(!pair)goto done;
        uint32_t key=u32(pair);if(key>=4)goto done;
        const uint8_t *name=mapped(decoded,n,u32(pair+4),24);if(!name || !memchr(name,0,24))goto done;
        memcpy(candidate.wheel_categories[key],name,strlen((const char *)name)+1);
    }
    /* 00448E70 +0x3c..+0x54 initializes the tuner enum from 006A7658;
       00448DF0 -> 0057B1A8 returns the first matching key. */
    terminated=0;
    for(unsigned i=0;i<256;i++){
        const uint8_t *pair=mapped(decoded,n,0x6a7658+i*8,8);if(!pair)goto done;
        uint32_t key=u32(pair),address=u32(pair+4);
        if(!address){terminated=1;break;}
        if(key>=256 || candidate.tuner_names[key][0])continue;
        const uint8_t *name=mapped(decoded,n,address,24);if(!name || !memchr(name,0,24))goto done;
        memcpy(candidate.tuner_names[key],name,strlen((const char *)name)+1);
    }
    if(!terminated)goto done;
    /* 00448E70 +0x60..0x70 initializes country enum 006A79C0;
       00448E50 looks up GENERIC_CAR byte +0x0e. */
    terminated=0;
    for(unsigned i=0;i<256;i++){
        const uint8_t *pair=mapped(decoded,n,0x6a79c0+i*8,8);if(!pair)goto done;
        uint32_t key=u32(pair),address=u32(pair+4);if(!address){terminated=1;break;}
        if(key>=256 || candidate.country_names[key][0])continue;
        const uint8_t *name=mapped(decoded,n,address,24);if(!name || !memchr(name,0,24))goto done;
        memcpy(candidate.country_names[key],name,strlen((const char *)name)+1);
    }
    if(!terminated)goto done;
    *out=candidate;ok=1;
done:free(decoded);free(raw);return ok;
}
