#ifndef OPENGT4_CAREER_PRICES_H
#define OPENGT4_CAREER_PRICES_H
#include "spectables.h"
typedef struct { uint8_t mileage_factors[80]; } gt4_career_price_data;
/* NTSC-U v2.00 CORE.GT4 data, read from the user's ISO; no extracted table
   is embedded in the application. Returns zero for unsupported/broken data. */
int gt4_career_price_data_load(const char *iso,gt4_career_price_data *out);
int32_t gt4_career_price_scale(int32_t price,uint32_t factor_bits);
int32_t gt4_career_used_price_value(const gt4_career_price_data *data,int32_t base_price,uint32_t odometer);
int32_t gt4_career_sell_price_value(int32_t used_price);
/* Currently supports GENERIC_CAR keys only. Other key namespaces must be
   resolved through the original label/fallback path before integration. */
int gt4_career_used_price(gt4_spec_tables *tables,const gt4_career_price_data *data,uint64_t car,uint32_t odometer,int32_t *out);
#endif
