#include "career_original.h"
#include "career_float.h"
#include <string.h>
#include <fenv.h>
static uint32_t read32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void write32(uint8_t *p,uint32_t value){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(value>>(8*i));}
/* func_00350678: initialize D_00620320 from settings at D_00620CD8.
   Each conversion and multiplication rounds individually, as in assembly. */
static void oil_params(uint8_t *params,uint8_t *settings){
    if(settings[0xcc])return;
    write32(params,career_f32_from_int(settings[0xd0]*100000)^0x80000000u);
    write32(params+4,career_f32_mul(career_f32_from_int(settings[0xd1]),0x3a83126eu));
    uint32_t start=career_f32_from_int(settings[0xd2]*100000);
    uint32_t length=career_f32_from_int(settings[0xd3]*100000);
    write32(params+8,start);
    /* Both converted operands here are nonnegative integral floats <=
       25,500,000. Their exact integer sum, rounded once, equals add.s. */
    write32(params+12,career_f32_from_int(career_f32_to_int(start)+career_f32_to_int(length)));
    write32(params+16,career_f32_mul(career_f32_from_int(settings[0xd4]),0x3a83126eu));
    write32(params+20,career_f32_mul(career_f32_from_int(settings[0xd5]*100000)^0x80000000u,0x3f000000u));
    write32(params+24,career_f32_mul(career_f32_from_int(settings[0xd6]),0x3a83126eu));
    write32(params+28,career_f32_from_int(settings[0xd7]*100000));
    write32(params+32,career_f32_from_int(settings[0xd8]*100000));
    settings[0xcc]=1;
}
/* func_00343928 -> 003507D0 -> 00343698 / 00350808.
   Wear points to the original car+0x4a0 block. No changes to engine/physics
   routines: this only ports the maintenance state update and lazy globals. */
void gt4_career_replace_oil(uint8_t *wear,uint8_t *params,uint8_t *settings){
    if(wear[0x14])return;
    oil_params(params,settings);
    uint32_t value=career_f32_mul(read32(params+20),0x40400000u);
    uint32_t maximum=read32(params+28);
    write32(wear+8,career_f32_lt(maximum,value)?maximum:value);
}
/* 0014E9C0 -> 003437F0: exemption zero, zero maximum one, otherwise
   negative mileage relative to new-oil offset, divide and upper clamp. */
float gt4_career_oil_life(const uint8_t wear[32],uint8_t params[36],uint8_t settings[0xd9]){
 if(wear[0x14])return 0;oil_params(params,settings);
 float maximum,value,offset;memcpy(&maximum,params+28,4);if(maximum==0)return 1;
 memcpy(&value,wear+8,4);uint32_t bits=career_f32_mul(read32(params+20),0x40400000);memcpy(&offset,&bits,4);
 fenv_t env;fegetenv(&env);fesetround(FE_TOWARDZERO);if(value<0)value-=offset;value/=maximum;if(value>1)value=1;fesetenv(&env);return value;
}

/* func_003433E8 / 00343420 / 00440FB8, explicit nonnegative mileage.
   003434A8 saturates the unsigned odometer; the following cvt.s.w uses
   the signed low word of the argument, independently of that saturation.
   Exempt cars keep zero wear. 003438A0 runs twice, lastly with +0. */
void gt4_career_car_wear_init(uint8_t wear[32],uint64_t mileage,uint8_t exempt,gt4_career_oil_data *data){
    for(unsigned i=0;i<32;i++)wear[i]=0;
    wear[0x14]=exempt;
    gt4_career_odometer_set(wear,mileage);
    if(exempt)return;
    oil_params(data->params,data->settings);
    uint32_t word=(uint32_t)mileage;int32_t signed_word;memcpy(&signed_word,&word,4);
    uint32_t value=career_f32_from_int(signed_word);
    uint32_t engine=read32(data->params+12),oil=read32(data->params+28),clutch=read32(data->clutch_maximum);
    write32(wear+4,career_f32_lt(engine,value)?engine:value);
    write32(wear+8,career_f32_lt(oil,value)?oil:value);
    write32(wear+0x10,career_f32_lt(clutch,value)?clutch:value);
}
