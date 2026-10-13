#include "career_prices.h"
#include "career_float.h"
#include <string.h>
static uint32_t le32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
/* The price sequence uses cvt.s.w, mul.s, cvt.w.s. Integer bit arithmetic
   implements their toward-zero rounding without changing host FPU state.
   Each operation rounds separately; there is no host double/fused shortcut. */
int32_t gt4_career_price_scale(int32_t price,uint32_t factor_bits){return career_f32_to_int(career_f32_mul(career_f32_from_int(price),factor_bits));}
/* func_00441128: unsigned odometer division, signed threshold comparison,
   ten records maximum, negative threshold as sentinel, zero on exhaustion. */
int32_t gt4_career_used_price_value(const gt4_career_price_data *data,int32_t base_price,uint32_t odometer){
    uint32_t factor=0;int32_t mileage=(int32_t)(odometer/1000000u);
    for(unsigned i=0;i<10;i++){
        int32_t threshold;uint32_t bits=le32(data->mileage_factors+i*8);memcpy(&threshold,&bits,4);
        if(mileage<threshold || threshold<0){factor=le32(data->mileage_factors+i*8+4);break;}
    }
    return gt4_career_price_scale(base_price,factor);
}
/* func_004411F0: call used price, cvt.s.w, multiply 0.25, cvt.w.s. */
int32_t gt4_career_sell_price_value(int32_t used_price){return gt4_career_price_scale(used_price,0x3e800000u);}
/* func_00445350 / 004487D8 / 00448868 for GENERIC_CAR keys. */
int gt4_career_used_price(gt4_spec_tables *tables,const gt4_career_price_data *data,uint64_t car,uint32_t odometer,int32_t *out){
    if(!tables || !data || !out || car>>32)return 0;
    uint8_t row[256];if(gt4_spec_tables_row(tables,0,(uint32_t)car,0,row,sizeof row)<12)return 0;
    uint32_t bits=le32(row+8);int32_t base;memcpy(&base,&bits,4);
    *out=gt4_career_used_price_value(data,base,odometer);return 1;
}
