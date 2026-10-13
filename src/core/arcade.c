/* The Arcade's race setup from the SpecDB (arcade.h), read from the
   executable's menu natives: the table lookups of D_006235A8 and the
   opponents' choice of MCarData.PickUpEnemyCars. */
#include "arcade.h"
#include "specdb.h"
#include "spectables.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define T_GENERIC_CAR   0x00u
#define T_DRIVETRAIN    0x0au
#define T_GEAR          0x0bu
#define T_COURSE        0x22u
#define T_ENEMY_CARS    0x26u
#define T_CAR_VARIATION 0x29u
#define NONE            (~(uint64_t)0)

struct gt4_arcade {
    gt4_vol *vol;
    gt4_spec_tables *tables;
    uint8_t *car_idi, *enemy_idi, *course_idi, *race_idi, *info;
    size_t car_idi_size, enemy_idi_size, course_idi_size, race_idi_size, info_size;
    uint32_t random;   /* D_00874D50 */
};

static uint8_t *read_table(gt4_vol *vol, const char *name, size_t *size)
{
    char path[96];
    snprintf(path, sizeof path, "/specdb/GT4_US2560/%s", name);
    return gt4_vol_read(vol, path, size);
}

gt4_arcade *gt4_arcade_open(gt4_vol *vol)
{
    gt4_arcade *a = calloc(1, sizeof *a);
    if (!a) return NULL;
    a->vol = vol;
    a->tables = gt4_spec_tables_open(vol, NULL);
    a->car_idi = read_table(vol, "GENERIC_CAR.idi", &a->car_idi_size);
    a->enemy_idi = read_table(vol, "ENEMY_CARS.idi", &a->enemy_idi_size);
    a->course_idi = read_table(vol, "COURSE.idi", &a->course_idi_size);
    a->race_idi = read_table(vol, "RACE.idi", &a->race_idi_size);
    a->info = read_table(vol, "ARCADEINFO_NORMAL.dbt", &a->info_size);
    a->random = 0xba8350bdu;
    return a;
}

void gt4_arcade_close(gt4_arcade *a)
{
    if (!a) return;
    gt4_spec_tables_close(a->tables);
    free(a->car_idi);
    free(a->enemy_idi);
    free(a->course_idi);
    free(a->race_idi);
    free(a->info);
    free(a);
}

void gt4_arcade_seed(gt4_arcade *a, uint32_t seed) { if (a) a->random = seed; }

/* func_00579330 then func_00578598: the float made on the bits (the
   R5900's PLZCW counts the leading bits equal to the sign, less one). */
float gt4_arcade_random01(gt4_arcade *a)
{
    uint32_t x = a->random * 17u + 0x11u, r, lz, e, m, y;
    int32_t s;
    float f;
    a->random = x;
    r = x ^ ((x << 16) | (x >> 16));
    if (!r) return 0.0f;
    s = (int32_t)r >> 31;
    y = r ^ (uint32_t)s;
    lz = 0;
    while (lz < 32 && !(y & (0x80000000u >> lz))) lz++;
    lz = ((lz - 1) & 0x1f) | (uint32_t)s;
    e = 125u - lz;
    m = (r << ((0u - e) & 31)) >> 9;
    y = (e << 23) | m;
    memcpy(&f, &y, 4);
    return f;
}

static int row(gt4_arcade *a, uint64_t key, uint8_t *out, size_t cap)
{
    memset(out, 0, cap);
    return key != NONE && gt4_spec_tables_row(a->tables, (uint32_t)(key >> 32), (uint32_t)key, 0, out, cap) > 0;
}

static uint64_t le64(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v; }
static uint32_t le32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

uint64_t gt4_arcade_car_key(gt4_arcade *a, const char *label)
{
    uint32_t id;
    if (!a || !label) return NONE;
    if (gt4_specdb_label(a->car_idi, a->car_idi_size, label, &id)) return id;
    if (gt4_specdb_label(a->enemy_idi, a->enemy_idi_size, label, &id)) return (uint64_t)T_ENEMY_CARS << 32 | id;
    return NONE;
}

/* func_00447E38: the GENERIC_CAR label behind a car label (an ENEMY_CARS
   row starts with its car's key) */
static const char *base_label(gt4_arcade *a, const char *label)
{
    uint64_t key = gt4_arcade_car_key(a, label);
    uint8_t r[0x100];
    if (key == NONE) return NULL;
    if (key >> 32) {
        if (!row(a, key, r, sizeof r)) return NULL;
        key = le64(r);
        if (key >> 32) return NULL;
    }
    return gt4_specdb_label_for_id(a->car_idi, a->car_idi_size, (uint32_t)key);
}

/* func_004489B0: the car's GENERIC_CAR row (24 bytes) */
static int car_row(gt4_arcade *a, const char *label, uint8_t out[24])
{
    uint8_t r[0x100];
    uint64_t key = gt4_arcade_car_key(a, label);
    if (key != NONE && (key >> 32)) {
        if (!row(a, key, r, sizeof r)) return 0;
        key = le64(r);
    }
    if (!row(a, key, r, sizeof r)) return 0;
    memcpy(out, r, 24);
    return 1;
}

/* func_00447F48: the COURSE row */
static int course_row(gt4_arcade *a, const char *label, uint8_t out[24])
{
    uint32_t id;
    return a && label && gt4_specdb_label(a->course_idi, a->course_idi_size, label, &id) &&
           row(a, (uint64_t)T_COURSE << 32 | id, out, 24);
}

const char *gt4_arcade_course_type(gt4_arcade *a, const char *course)
{
    static const char *const types[] = { "dry", "wet", "dirt", "snow" };
    uint8_t r[24];
    if (!course_row(a, course, r) || r[0x10] > 3) return "unknown";
    return types[r[0x10]];
}

int gt4_arcade_course_pit(gt4_arcade *a,const char *course)
{
    uint8_t r[24];
    /* 00394ED8 +18C..230: COURSE+14 enables the resident package;
       +15 selects pit_002 only for type 2, otherwise pit_001. */
    return course_row(a,course,r) && r[0x14] ? (r[0x15]==2?2:1) : 0;
}

const char *gt4_arcade_course_attr(gt4_arcade *a, const char *course)
{
    uint8_t r[24];
    const char *s;
    if (!course_row(a, course, r)) return "";
    s = gt4_spec_tables_str(a->tables, le32(r + 8));
    return s ? s : "";
}

/* func_00448AD0: the car's ARCADEINFO_NORMAL row (65 bytes: six race keys,
   dry by difficulty then the others, two ENEMY_CARS keys, the tyre flag) */
static int arcade_info(gt4_arcade *a, const char *car, uint8_t out[65])
{
    uint8_t r[0x100];
    uint64_t key = gt4_arcade_car_key(a, car);
    if (key == NONE || (key >> 32) == T_ENEMY_CARS) {
        /* ARCADEINFO_TUNED has no rows on the US disc */
        return 0;
    }
    memset(r, 0, sizeof r);
    if (!a->info || gt4_specdb_row(a->info, a->info_size, (uint32_t)key, r, sizeof r) != 65) return 0;
    memcpy(out, r, 65);
    return 1;
}

const char *gt4_arcade_race_code(gt4_arcade *a, const char *car, int type, int difficulty)
{
    uint8_t r[65];
    uint64_t key;
    if (!arcade_info(a, car, r)) return NULL;
    if (difficulty >= 3) difficulty = 2;
    if (difficulty < 0) difficulty = 0;
    key = le64(r + (type ? 0x18 : 0) + difficulty * 8);
    if (key == NONE) return NULL;
    return gt4_specdb_label_for_id(a->race_idi, a->race_idi_size, (uint32_t)key);
}

const char *gt4_arcade_shift_car(gt4_arcade *a, const char *car, const char *course)
{
    uint8_t c[24], r[65];
    uint64_t key;
    if (!course_row(a, course, c) || c[0x10] < 2 || !arcade_info(a, car, r)) return NULL;
    if (c[0x10] == 2) key = le64(r + 0x30);
    else if (c[0x10] == 3) key = le64(r + 0x38);
    else return NULL;
    if ((key >> 32) != T_ENEMY_CARS) return NULL;
    return gt4_specdb_label_for_id(a->enemy_idi, a->enemy_idi_size, (uint32_t)key);
}

int gt4_arcade_tire_normal(gt4_arcade *a, const char *car)
{
    uint8_t r[65];
    return arcade_info(a, car, r) && r[0x40] != 0;
}

/* IsDirtRunnable, func_0012E498 at 0012E4F0: info+0x38 != -1.
   func_00448B48 at 00448BD0/00448BF8 copies the DB's +0x30 dirt key
   to that field (the snow key is moved from DB+0x38 to info+0x40).
   arcade.ad L4381..4382 / L6344..6345 use this for dirt AND snow. */
int gt4_arcade_dirt_runnable(gt4_arcade *a, const char *car)
{
    uint8_t r[65];
    return arcade_info(a, car, r) && le64(r + 0x30) != NONE;
}

int gt4_arcade_race_forbidden(gt4_arcade *a, const char *car)
{
    uint8_t r[24];
    return car_row(a, car, r) && (r[0x12] >> 2 & 1);
}

int gt4_arcade_test_car(gt4_arcade *a,const char *car)
{
    uint8_t r[24];
    /* 0012F258 +0xb8 calls 004489A0 with bit index 5. */
    return car_row(a,car,r) && ((r[0x12]>>5)&1);
}

/* The part a car comes with (func_00444480: DEFAULT_PARTS through the
   GENERIC_CAR row's first key; at: the part's offset in that row) */
static int default_part(gt4_arcade *a, const char *car, uint32_t at, uint32_t table, uint8_t *out, size_t cap)
{
    uint8_t r[0x100];
    uint64_t key = gt4_arcade_car_key(a, car);
    if (key != NONE && (key >> 32)) {
        if (!row(a, key, r, sizeof r)) return 0;
        key = le64(r);
    }
    if (!row(a, key, r, sizeof r) || !row(a, le64(r), r, sizeof r)) return 0;
    key = le64(r + at);
    return (uint32_t)(key >> 32) == table && row(a, key, out, cap);
}

const char *gt4_arcade_drivetrain(gt4_arcade *a, const char *car)
{
    static const char *const names[] = { "FR", "FF", "4WD", "MR", "RR" };   /* D_006188B0 */
    uint8_t r[0x100];
    if (!default_part(a, car, 0x48, T_DRIVETRAIN, r, sizeof r) || r[0xf] != 1 || r[0x10] > 4) return "--";
    return names[r[0x10]];
}

/* 00445EE8..00445FB8 read the stock ENGINE; 00445E20 reads DRIVETRAIN +16.
   DEFAULT_PARTS +0x58/+0x48 are the engine/drive keys (00444480). */
int gt4_arcade_catalog_parts(gt4_arcade *a,const char *car,uint8_t engine[128],int *drive)
{
    uint8_t d[128];
    if(!default_part(a,car,0x58,0x0c,engine,128) ||
       !default_part(a,car,0x48,T_DRIVETRAIN,d,sizeof d))return 0;
    *drive=d[0x10];return 1;
}
int gt4_arcade_automatic_only(gt4_arcade *a, const char *car)
{
    uint8_t r[0x100];
    return default_part(a, car, 0x50, T_GEAR, r, sizeof r) && (r[0x23] == 1 || r[0x23] == 3);
}
/* 00446038/68/98/C8: catalogue length, height, width and mass.
   DEFAULT_PARTS +0x28 CHASSIS and +0x30 RACINGMODIFY (00444480 maps
   these keys to garage slots +0x30 and +0x80 respectively). */
int gt4_arcade_catalog_geometry(gt4_arcade *a,const char *car,uint16_t out[4])
{
    uint8_t c[256],w[256];
    if(!default_part(a,car,0x28,6,c,sizeof c) || !default_part(a,car,0x30,7,w,sizeof w))return 0;
    out[0]=c[8]|(unsigned)c[9]<<8;out[1]=w[8]|(unsigned)w[9]<<8;
    out[2]=c[10]|(unsigned)c[11]<<8;out[3]=c[12]|(unsigned)c[13]<<8;return 1;
}

int gt4_arcade_color_num(gt4_arcade *a, const char *car)
{
    uint8_t r[0x100], g[24];
    uint64_t key = gt4_arcade_car_key(a, car), var;
    int n = 0;
    if (key == NONE || (key >> 32)) return 0;
    /* func_004457B0: the first CAR_VARIATION row; its key's rows */
    if (!row(a, (uint64_t)T_CAR_VARIATION << 32 | (uint32_t)key, r, sizeof r)) return 0;
    var = le64(r);
    if (var == NONE) return 0;
    while (gt4_spec_tables_row(a->tables, (uint32_t)(var >> 32), (uint32_t)var, (uint32_t)n, NULL, 0)) n++;
    if (car_row(a, car, g) && (g[0x12] >> 1 & 1)) n--;
    return n;
}

/* ------------------------------------------------ the opponents' choice */

typedef struct { const char *car; float rating; } pick_entry;   /* the original's 0x44-byte entries */

/* func_00130618: the index where the ratings (highest first) pass key */
static int rating_search(const pick_entry *e, int lo, int hi, float key)
{
    for (;;) {
        int mid = (lo + hi) / 2;
        if (mid == lo) return mid;
        if (e[mid].rating < key) hi = mid;
        else lo = mid;
    }
}

/* func_00130690: count different indices of [lo, hi) at random, not own */
static void pick_indices(gt4_arcade *a, int lo, int hi, int *out, int count, int own)
{
    int left = hi - lo, n = left, picked = 0;
    for (int i = 0; i < count; i++) out[i] = -1;
    while (picked < count) {
        int r, dup = 0;
        if (left <= 0) return;
        if (own >= 0 && left < 2) return;
        r = (int)(gt4_arcade_random01(a) * (float)n) + lo;
        if (r == own) continue;
        for (int j = 0; j < picked; j++) if (out[j] == r) { dup = 1; break; }
        if (dup) continue;
        out[picked++] = r;
        left--;
    }
}

/* func_001307E8: on dirt each pick moves down the list to the first car
   with a dirt version (its arcade info's first ENEMY_CARS key), not own */
static void dirt_versions(gt4_arcade *a, int *out, int count, const pick_entry *e, int n, int own)
{
    uint8_t r[65];
    for (int i = 0; i < count;) {
        int k = out[i];
        for (;;) {
            if (k < 0 || k >= n || !arcade_info(a, e[k].car, r)) return;
            if (le64(r + 0x30) != NONE && k != own) break;
            k++;
        }
        out[i++] = k;
    }
}

int gt4_arcade_pick_enemies(gt4_arcade *a, gt4_arcade_rater rate, void *ctx, const char *player,
                            const char *const *list, int n, float offset, float range, int count, int dirt,
                            gt4_arcade_enemy out[5])
{
    pick_entry *e;
    uint64_t *keys;
    float *ratings, own_rating = 0, at;
    uint8_t r[24];
    const char *own_label;
    int kept = 0, open, lo, hi, own = -1, idx[5], color[5], got = 0;
    if (count >= 6) count = 5;
    if (!a || !rate || n < 0) return -1;
    /* the player's car: its rating, open-top type (3 counts as 0) */
    {
        uint64_t k = gt4_arcade_car_key(a, player);
        if (k != NONE && !rate(ctx, &k, 1, &own_rating)) return -1;
    }
    own_label = base_label(a, player);
    memset(r, 0, sizeof r);
    car_row(a, player, r);
    open = r[0x11] % 100;
    if (open == 3) open = 0;
    e = calloc((size_t)n + 1, sizeof *e);
    keys = calloc((size_t)n + 1, sizeof *keys);
    ratings = calloc((size_t)n + 1, sizeof *ratings);
    if (!e || !keys || !ratings) {
        free(e);
        free(keys);
        free(ratings);
        return -1;
    }
    /* the list's cars of the same type (an open car's list also takes the
       type 1 ones); a car without its row keeps the previous one's */
    for (int i = 0; i < n; i++) {
        int t;
        car_row(a, list[i], r);
        t = r[0x11] % 100;
        if (t == 3) t = 0;
        if (t == open || (open == 2 && t == 1)) e[kept++].car = list[i];
    }
    {
        int m = 0;
        for (int i = 0; i < kept; i++) {
            keys[m] = gt4_arcade_car_key(a, e[i].car);
            if (keys[m] != NONE) m++;
        }
        if (m && !rate(ctx, keys, m, ratings)) {
            free(e);
            free(keys);
            free(ratings);
            return -1;
        }
        for (int i = 0, j = 0; i < kept; i++)
            e[i].rating = gt4_arcade_car_key(a, e[i].car) != NONE ? ratings[j++] : 0;
    }
    /* highest rating first (func_00130660 with qsort) */
    for (int i = 1; i < kept; i++) {
        pick_entry t = e[i];
        int j = i - 1;
        while (j >= 0 && e[j].rating < t.rating) { e[j + 1] = e[j]; j--; }
        e[j + 1] = t;
    }
    at = own_rating + offset;
    lo = rating_search(e, 0, kept, at + range);
    hi = rating_search(e, 0, kept, at - range);
    /* a window of at least count + 1 cars: down the list, then up */
    {
        int need = count + 1, w = hi - lo;
        if (w < need) {
            if (hi < kept - 1)
                do { hi++; w++; } while (w < need && hi < kept - 1);
            while (w < need && lo > 0) { lo--; w++; }
        }
    }
    for (int i = lo; i < hi; i++) {
        const char *b = base_label(a, e[i].car);
        if (b && own_label && !strcmp(b, own_label)) { own = i; break; }
    }
    if (count > 0) {
        pick_indices(a, lo, hi, idx, count, own);
        if (dirt) dirt_versions(a, idx, count, e, kept, own);
    }
    /* a paint for each place, then the places sorted (lowest in the list,
       the weakest, first): the paints stay by place, as the original */
    for (int i = 0; i < count; i++) {
        const char *c = idx[i] >= 0 && idx[i] < kept ? e[idx[i]].car : NULL;
        color[i] = (int)(gt4_arcade_random01(a) * (float)(c ? gt4_arcade_color_num(a, c) : 0));
    }
    for (int i = 1; i < count; i++) {
        int t = idx[i], j = i - 1;
        while (j >= 0 && idx[j] < t) { idx[j + 1] = idx[j]; j--; }
        idx[j + 1] = t;
    }
    for (int i = 0; i < count; i++) {
        if (idx[i] < 0 || idx[i] >= kept) continue;
        out[got].car = e[idx[i]].car;
        out[got].color = color[i];
        got++;
    }
    free(e);
    free(keys);
    free(ratings);
    return got;
}
