#ifndef OPENGT4_SPECTABLES_H
#define OPENGT4_SPECTABLES_H
#include "vol.h"

#include <stddef.h>
#include <stdint.h>

#define GT4_SPEC_TABLES 0x2b

typedef struct gt4_spec_tables gt4_spec_tables;

/* dir: NULL for the US tables ("/specdb/GT4_US2560"). Tables load on first use. */
gt4_spec_tables *gt4_spec_tables_open(gt4_vol *vol, const char *dir);
void             gt4_spec_tables_close(gt4_spec_tables *t);
const uint8_t   *gt4_spec_tables_data(gt4_spec_tables *t, uint32_t table, size_t *size);
const char      *gt4_spec_tables_name(uint32_t table);
/* A row source for gt4_car_record (ctx: the tables). */
int gt4_spec_tables_row(void *ctx, uint32_t table, uint32_t id, uint32_t nth, uint8_t *out, size_t cap);
/* String i of UnistrDB.sdb (the string database the game keeps at
   D_00623788: course codes and the like). */
const char *gt4_spec_tables_str(void *ctx, uint32_t i);
/* CARS/<car key, 16 hex digits>: the parts a car takes (malloc'd, NULL if none) */
uint8_t *gt4_spec_tables_car_parts(void *ctx, uint64_t car, size_t *size);

#endif
