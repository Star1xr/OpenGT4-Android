/* The SpecDB tables of the car record, read from the disc as the game
   loads them (index as in the garage keys, D_006234B0), for
   gt4_car_record's row source. */
#include "spectables.h"
#include "specdb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const names[GT4_SPEC_TABLES] = {
    "GENERIC_CAR", "BRAKE", "BRAKECONTROLLER", "SUSPENSION", "ASCC", "TCSC", "CHASSIS",
    "RACINGMODIFY", "LIGHTWEIGHT", "STEER", "DRIVETRAIN", "GEAR", "ENGINE", "NATUNE",
    "TURBINEKIT", "PORTPOLISH", "ENGINEBALANCE", "DISPLACEMENT", "COMPUTER", "INTERCOOLER",
    "MUFFLER", "CLUTCH", "FLYWHEEL", "PROPELLERSHAFT", "LSD", "FRONTTIRE", "REARTIRE", "NOS",
    "SUPERCHARGER", "WHEEL", "WING", "TIRESIZE", "TIRECOMPOUND", "TIREFORCEVOL", "COURSE",
    "RACE", "DEFAULT_PARTS", "DEFAULT_PARAM", "ENEMY_CARS", NULL, NULL, "CAR_VARIATION_american",
    "VARIATIONamerican",   /* per language; the US game reads the american one */
};

struct gt4_spec_tables {
    gt4_vol *vol;
    char     dir[64];
    int      tried[GT4_SPEC_TABLES];
    uint8_t *data[GT4_SPEC_TABLES];
    size_t   size[GT4_SPEC_TABLES];
    int      str_tried;
    uint8_t *str;
    size_t   str_size;
};

gt4_spec_tables *gt4_spec_tables_open(gt4_vol *vol, const char *dir)
{
    gt4_spec_tables *t = calloc(1, sizeof *t);
    if (!t) return NULL;
    t->vol = vol;
    snprintf(t->dir, sizeof t->dir, "%s", dir ? dir : "/specdb/GT4_US2560");
    return t;
}

void gt4_spec_tables_close(gt4_spec_tables *t)
{
    if (!t) return;
    for (int i = 0; i < GT4_SPEC_TABLES; i++) free(t->data[i]);
    free(t->str);
    free(t);
}

const uint8_t *gt4_spec_tables_data(gt4_spec_tables *t, uint32_t table, size_t *size)
{
    char path[128];
    if (!t || table >= GT4_SPEC_TABLES || !names[table]) return NULL;
    if (!t->tried[table]) {
        t->tried[table] = 1;
        snprintf(path, sizeof path, "%s/%s.dbt", t->dir, names[table]);
        t->data[table] = gt4_vol_read(t->vol, path, &t->size[table]);
    }
    if (size) *size = t->size[table];
    return t->data[table];
}

const char *gt4_spec_tables_name(uint32_t table)
{
    return table < GT4_SPEC_TABLES ? names[table] : NULL;
}

int gt4_spec_tables_row(void *ctx, uint32_t table, uint32_t id, uint32_t nth, uint8_t *out, size_t cap)
{
    size_t size;
    const uint8_t *d = gt4_spec_tables_data(ctx, table, &size);
    return d ? gt4_specdb_row_nth(d, size, id, nth, out, cap) : 0;
}

uint8_t *gt4_spec_tables_car_parts(void *ctx, uint64_t car, size_t *size)
{
    gt4_spec_tables *t = ctx;
    char path[128];
    if (!t) return NULL;
    snprintf(path, sizeof path, "%s/CARS/%08X%08X", t->dir, (unsigned)(car >> 32), (unsigned)car);
    return gt4_vol_read(t->vol, path, size);
}

const char *gt4_spec_tables_str(void *ctx, uint32_t i)
{
    gt4_spec_tables *t = ctx;
    char path[128];
    if (!t) return NULL;
    if (!t->str_tried) {
        t->str_tried = 1;
        snprintf(path, sizeof path, "%s/UnistrDB.sdb", t->dir);
        t->str = gt4_vol_read(t->vol, path, &t->str_size);
    }
    return gt4_specdb_string(t->str, t->str_size, i);
}
