/*
 * gt4spec: a car's rows in the user's SpecDB, for finding fields.
 *
 *   gt4spec <disc.iso> <car label>                  the GENERIC_CAR and DEFAULT_PARTS rows
 *   gt4spec <disc.iso> <car label> <TABLE> <at>     that table's row whose id is the u32 at
 *                                                   DEFAULT_PARTS+<at> (e.g. CHASSIS 0x28)
 *   ... [--find N]                                  offsets where the row holds N (u8/u16/u32)
 */
#include "specdb.h"
#include "vol.h"
#include "spectables.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t u32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static uint8_t *table(gt4_vol *v, const char *name, size_t *n)
{
    char path[128];
    snprintf(path, sizeof path, "/specdb/GT4_US2560/%s", name);
    return gt4_vol_read(v, path, n);
}

static void dump(const char *what, const uint8_t *r, int len, long find)
{
    printf("%s (%d bytes):\n", what, len);
    for (int i = 0; i < len; i += 16) {
        printf("  %03x:", i);
        for (int k = i; k < i + 16 && k < len; k++) printf(" %02x", r[k]);
        printf("\n");
    }
    if (find >= 0)
        for (int i = 0; i < len; i++) {
            if (r[i] == (find & 0xff) && find < 256) printf("  u8 %ld at 0x%x\n", find, i);
            if (i + 1 < len && (r[i] | r[i + 1] << 8) == find) printf("  u16 %ld at 0x%x\n", find, i);
            if (i + 3 < len && (long)u32(r + i) == find) printf("  u32 %ld at 0x%x\n", find, i);
        }
}

int main(int argc, char **argv)
{
    const char *err = NULL;
    gt4_vol *v;
    uint8_t *labels, *gc, *dp, row[256], parts[256];
    size_t nl = 0, ng = 0, nd = 0;
    uint32_t id;
    long find = -1;
    int n;
    if (argc < 3) {
        fprintf(stderr, "usage: %s <disc.iso> <car label> [TABLE at] [--find N]\n", argv[0]);
        return 2;
    }
    for (int i = 3; i + 1 < argc; i++) if (!strcmp(argv[i], "--find")) find = strtol(argv[i + 1], NULL, 0);
    v = gt4_vol_open(argv[1], &err);
    if (!v) { fprintf(stderr, "%s\n", err ? err : "cannot open"); return 1; }
    if (!strcmp(argv[2], "--parts") && argc >= 4) {
        /* gt4spec <disc> --parts <car>: the car's parts list (CARS/<car>):
           per part its key (table << 32 | id) and level */
        gt4_spec_tables *st = gt4_spec_tables_open(v, NULL);
        size_t nl2 = 0, size = 0;
        uint8_t *lab = table(v, "GENERIC_CAR.idi", &nl2), *parts;
        uint32_t cid;
        if (!gt4_specdb_label(lab, nl2, argv[3], &cid)) { fprintf(stderr, "unknown car %s\n", argv[3]); return 1; }
        parts = gt4_spec_tables_car_parts(st, cid, &size);
        if (!parts) { fprintf(stderr, "no parts list\n"); return 1; }
        for (uint32_t i = 0; 8 + (size_t)i * 16 + 16 <= size && i < u32(parts); i++) {
            const uint8_t *e = parts + 8 + (size_t)i * 16;
            printf("table 0x%02x id 0x%05x level %u\n", u32(e + 4), u32(e), e[8]);
        }
        return 0;
    }
    if (!strcmp(argv[2], "--grep") && argc >= 5) {
        /* gt4spec <disc> --grep TABLE.dbt <hex bytes>: rows holding them */
        size_t nt = 0, len = strlen(argv[4]) / 2;
        uint8_t *t = table(v, argv[3], &nt), pat[64], r[256];
        for (size_t i = 0; i < len && i < sizeof pat; i++) { unsigned x; sscanf(argv[4] + i * 2, "%2x", &x); pat[i] = (uint8_t)x; }
        for (uint32_t i = 0;; i++) {
            int w = gt4_specdb_row_at(t, nt, i, r, sizeof r);
            if (w <= 0) break;
            for (int k = 0; k + (int)len <= w; k++)
                if (!memcmp(r + k, pat, len)) { printf("row %u at 0x%x (id 0x%x)\n", i, k, u32(t + 16 + (size_t)i * 8)); dump("row", r, w, -1); break; }
        }
        return 0;
    }
    labels = table(v, "GENERIC_CAR.idi", &nl);
    gc = table(v, "GENERIC_CAR.dbt", &ng);
    dp = table(v, "DEFAULT_PARTS.dbt", &nd);
    if (!gt4_specdb_label(labels, nl, argv[2], &id)) { fprintf(stderr, "unknown car %s\n", argv[2]); return 1; }
    n = gt4_specdb_row(gc, ng, id, row, sizeof row);
    printf("%s: id 0x%x\n", argv[2], (unsigned)id);
    if (argc < 5 || !strcmp(argv[3], "--find")) {
        dump("GENERIC_CAR", row, n, find);
        n = gt4_specdb_row(dp, nd, u32(row), parts, sizeof parts);
        dump("DEFAULT_PARTS", parts, n, -1);
        return 0;
    }
    n = gt4_specdb_row(dp, nd, u32(row), parts, sizeof parts);
    {
        char name[64];
        size_t nt = 0;
        uint8_t *t, r[256];
        uint32_t key = u32(parts + strtoul(argv[4], NULL, 0));
        snprintf(name, sizeof name, "%s.dbt", argv[3]);
        t = table(v, name, &nt);
        if (!t) { fprintf(stderr, "no table %s\n", name); return 1; }
        n = gt4_specdb_row(t, nt, key, r, sizeof r);
        printf("%s row 0x%x\n", argv[3], (unsigned)key);
        dump(argv[3], r, n, find);
    }
    return 0;
}
