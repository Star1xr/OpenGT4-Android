#ifndef OPENGT4_CAREER_FLOAT_H
#define OPENGT4_CAREER_FLOAT_H
#include <stdint.h>
/* PS2 rounding helpers for career ports; no EE runtime dependency. */
static inline uint32_t career_f32_from_int(int32_t value){
    uint32_t sign=value<0?0x80000000u:0u;
    uint32_t magnitude=value<0?0u-(uint32_t)value:(uint32_t)value;
    if(!magnitude)return 0;
    unsigned lead=0;for(uint32_t n=magnitude;n>>1;n>>=1)lead++;
    uint32_t mantissa=lead>23?magnitude>>(lead-23):magnitude<<(23-lead);
    return sign|((lead+127u)<<23)|(mantissa&0x7fffffu);
}
static inline uint32_t career_f32_mul(uint32_t a,uint32_t b){
    uint32_t sign=(a^b)&0x80000000u,ea=(a>>23)&255u,eb=(b>>23)&255u;
    if(!ea || !eb)return sign;
    /* PS2 exponent-255 inputs are interpreted as signed FLT_MAX. */
    if(ea==255){ea=254;a=0x7f7fffffu;}
    if(eb==255){eb=254;b=0x7f7fffffu;}
    uint64_t product=(uint64_t)((a&0x7fffffu)|0x800000u)*((b&0x7fffffu)|0x800000u);
    unsigned shift=(product&(UINT64_C(1)<<47))?24:23;
    int exponent=(int)ea+(int)eb-127+(shift==24);
    if(exponent>=255)return sign|0x7f7fffffu;
    if(exponent<=0)return sign;
    return sign|((uint32_t)exponent<<23)|((uint32_t)(product>>shift)&0x7fffffu);
}
static inline int32_t career_f32_to_int(uint32_t bits){
    uint32_t sign=bits>>31,exponent=(bits>>23)&255u;
    if(exponent>=158)return sign?INT32_MIN:INT32_MAX;
    if(exponent<127)return 0;
    uint32_t magnitude=(bits&0x7fffffu)|0x800000u;
    unsigned lead=exponent-127;
    magnitude=lead>23?magnitude<<(lead-23):magnitude>>(23-lead);
    return sign?-(int32_t)magnitude:(int32_t)magnitude;
}
static inline uint32_t career_f32_normalize(uint32_t bits){
    uint32_t exponent=bits&0x7f800000u;
    if(!exponent)return bits&0x80000000u;
    return exponent==0x7f800000u?(bits&0x80000000u)|0x7f7fffffu:bits;
}
static inline int career_f32_lt(uint32_t a,uint32_t b){
    a=career_f32_normalize(a);b=career_f32_normalize(b);
    if(!(a&0x7fffffffu) && !(b&0x7fffffffu))return 0;
    if((a^b)&0x80000000u)return (a>>31)!=0;
    return a&0x80000000u?a>b:a<b;
}
#endif
