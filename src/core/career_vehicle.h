#ifndef OPENGT4_CAREER_VEHICLE_H
#define OPENGT4_CAREER_VEHICLE_H
#include <stdint.h>
#include "car.h"
#include "career_original.h"
#include "gtmode_game.h"
typedef struct gt4_career_vehicle gt4_career_vehicle;
gt4_career_vehicle *gt4_career_vehicle_open(const char *iso);
void gt4_career_vehicle_close(gt4_career_vehicle *v);
/* Native factory: unit is the reused slot's 32 bytes on entry; body excludes
   the live object's eight-byte class header. No original code is executed. */
int gt4_career_vehicle_used_choices(const gt4_specdb_src *db,uint64_t key,int32_t seed,int32_t *color,uint64_t *mileage);
int gt4_career_vehicle_create_seeded(void *ctx,const char *label,int32_t color,uint64_t mileage,int32_t seed,uint8_t unit[32],uint8_t body[0x4b8]);
int gt4_career_vehicle_create(void *ctx,const char *label,int32_t color,uint64_t mileage,uint8_t unit[32],uint8_t body[0x4b8]);
/* Low-level port for oracle comparison using the supplied runtime SpecDB and
   gt4_phys_mem. Scratch locations are private to the caller. */
int gt4_career_vehicle_build(const gt4_specdb_src *db,uint64_t key,int32_t color,uint64_t mileage,
                            gt4_career_oil_data *oil,uint8_t unit[32],uint8_t body[0x4b8]);
int gt4_career_vehicle_auto_gears(void *ctx,uint8_t body[0x4b8]);
int gt4_career_vehicle_stats(void *ctx,const uint8_t body[0x4b8],int32_t out[3]);
int gt4_career_vehicle_service_geometry(void *ctx,const uint8_t body[0x4b8],float out[12]);
int gt4_career_vehicle_setting(void *ctx,uint8_t body[0x4b8],int operation,int auto_override,gt4_setting_details *out);
#endif
