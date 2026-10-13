/* The car's record (0x600 bytes, read by gt4_car_params) from its garage
   entry: func_003F3B68 and func_004413C8. Each 64-bit key of the entry
   (high word the SpecDB table, low word the row id) selects a row, which
   is copied field by field, adjusted by the tuning levels kept in the
   entry. The rows come from `gt4_specdb_src` (the disc, or the tables the
   game has in memory). */
#include "car.h"

#include <string.h>

#define GU8(a)  ((uint32_t)GT4_U8(a))
#define GU16(a) ((uint32_t)GT4_U16(a))
#define GS8(a)  ((int32_t)(int8_t)GT4_U8(a))
#define GS16(a) ((int32_t)(int16_t)GT4_U16(a))
#define W8(a, v)  (GT4_U8(a) = (uint8_t)(v))
#define W16(a, v) (GT4_U16(a) = (uint16_t)(v))
#define RU8(r, o)  ((uint32_t)(r)[o])
#define RU16(r, o) ((uint32_t)((r)[o] | (r)[(o) + 1] << 8))

#define K(bits) gt4_f32(bits)
#define GAME_SETTINGS 0x00620cd8u

const gt4_specdb_src *gt4_phys_specdb;
uint32_t gt4_phys_scratch;

static uint64_t key_at(uint32_t gar, uint32_t off)
{
    return (uint64_t)GT4_U32(gar + off) | (uint64_t)GT4_U32(gar + off + 4) << 32;
}
static int is_none(uint64_t key) { return key == ~(uint64_t)0; }

/* func_00443ED0: the row of a key into buf (unchanged when there is none). */
static int fetch(const gt4_specdb_src *db, uint64_t key, uint8_t *buf, size_t cap)
{
    return db->row(db->ctx, (uint32_t)(key >> 32), (uint32_t)key, 0, buf, cap);
}

/* func_004452C0: a tuning level (1..levels) between base and max. */
static int lerp(int base, int max, int levels, int level)
{
    if (levels < 2) return base;
    return base + (max - base) * (level - 1) / (levels - 1);
}

/* func_004462E0 / func_00446320: a byte of the row of a tyre key, -1 without one. */
static int tyre_byte(const gt4_specdb_src *db, uint32_t gar, uint32_t off, uint32_t at)
{
    uint8_t row[0x100] = { 0 };
    uint64_t key = key_at(gar, off);
    if (is_none(key)) return -1;
    fetch(db, key, row, sizeof row);
    return row[at];
}

int gt4_tyre_byte(const gt4_specdb_src *db, uint32_t gar, uint32_t off, uint32_t at)
{
    return tyre_byte(db, gar, off, at);
}

/* func_00441950: brakes */
static void brake(uint32_t rec, const uint8_t *row)
{
    W8(rec + 0x178, RU8(row, 0x3));
    W8(rec + 0x179, RU8(row, 0x4));
    W8(rec + 0x17a, RU8(row, 0x5));
}

/* func_00441970: brake controller, by tuning level */
static void brake_controller(uint32_t gar, uint32_t rec, const uint8_t *row)
{
    W8(rec + 0x1a8, lerp(row[4], row[5], row[3], GT4_U8(gar + 0x12f)));
    W8(rec + 0x1a9, lerp(row[8], row[9], row[7], GT4_U8(gar + 0x130)));
}

/* func_004419E0: steering */
static void steer(uint32_t rec, const uint8_t *row)
{
    W8(rec + 0x182, 6);
    W8(rec + 0x28, RU8(row, 0x25));
    W8(rec + 0x19c, RU8(row, 0xd));
    W8(rec + 0x19d, RU8(row, 0xe));
    W8(rec + 0x19e, RU8(row, 0xf));
    W8(rec + 0x19f, RU8(row, 0x10));
    W8(rec + 0x1a0, RU8(row, 0x11));
    W8(rec + 0x1a1, RU8(row, 0x12));
    W8(rec + 0x1a2, RU8(row, 0x13));
    W8(rec + 0x1a3, RU8(row, 0x14));
    W8(rec + 0x1a4, RU8(row, 0x15));
    W8(rec + 0x1a5, RU8(row, 0x16));
    W8(rec + 0x1a6, RU8(row, 0x17));
    W8(rec + 0x180, 6);
    W8(rec + 0x1a7, RU8(row, 0x18));
    W8(rec + 0x184, RU8(row, 0x1));
    W8(rec + 0x185, RU8(row, 0x2));
    W8(rec + 0x186, RU8(row, 0x3));
    W8(rec + 0x187, RU8(row, 0x4));
    W8(rec + 0x188, RU8(row, 0x5));
    W8(rec + 0x189, RU8(row, 0x6));
    W8(rec + 0x18a, RU8(row, 0x7));
    W8(rec + 0x18b, RU8(row, 0x8));
    W8(rec + 0x18c, RU8(row, 0x9));
    W8(rec + 0x18d, RU8(row, 0xa));
    W8(rec + 0x18e, RU8(row, 0xb));
    W8(rec + 0x181, 6);
    W8(rec + 0x18f, RU8(row, 0xc));
    W8(rec + 0x190, RU8(row, 0x19));
    W8(rec + 0x191, RU8(row, 0x1a));
    W8(rec + 0x192, RU8(row, 0x1b));
    W8(rec + 0x193, RU8(row, 0x1c));
    W8(rec + 0x194, RU8(row, 0x1d));
    W8(rec + 0x195, RU8(row, 0x1e));
    W8(rec + 0x196, RU8(row, 0x1f));
    W8(rec + 0x197, RU8(row, 0x20));
    W8(rec + 0x198, RU8(row, 0x21));
    W8(rec + 0x199, RU8(row, 0x22));
    W8(rec + 0x19a, RU8(row, 0x23));
    W8(rec + 0x19b, RU8(row, 0x24));
}

/* func_00441B20: chassis */
static void chassis(const gt4_specdb_src *db, uint32_t gar, uint32_t rec, const uint8_t *row)
{
    int f, r;
    W8(rec + 0x22, row[0x1c]);
    W8(rec + 0x2a, row[0xe] + 0x7c);
    W8(rec + 0x2b, row[0x10] + 0x7c);
    W16(rec + 0x1a, RU16(row, 4));
    W16(rec + 0x20, RU16(row, 6));
    W8(rec + 0x24, row[0x1d]);
    W8(rec + 0x27, row[0x1e]);
    W16(rec + 0x10, RU16(row, 0));
    W16(rec + 0x3a, RU16(row, 6));
    W8(rec + 2, row[0x1f]);
    W16(rec + 0x30, RU16(row, 0x12));
    W16(rec + 0x32, RU16(row, 0x14));
    W16(rec + 0x34, RU16(row, 0x16));
    W16(rec + 0x36, RU16(row, 0x18));
    W8(rec + 0x38, row[0x22] + GT4_U8(gar + 0x168));
    W8(rec + 0x39, row[0x23] + GT4_U8(gar + 0x168));
    f = tyre_byte(db, gar, 0x60, 0x3a);
    if ((uint32_t)(f - 6) < 5) {
        W8(rec + 0x1e2, row[0x20]);
        W8(rec + 0x1e4, row[0x21]);
    }
    r = tyre_byte(db, gar, 0x68, 0x38);
    if ((uint32_t)(r - 6) < 5) {
        W8(rec + 0x1e3, row[0x20]);
        W8(rec + 0x1e5, row[0x21]);
    }
}

/* func_00441C58: engine */
static void engine(uint32_t rec, const uint8_t *row)
{
    W16(rec + 0xd8, RU16(row, 0x1e));
    W16(rec + 0xda, RU16(row, 0x20));
    W16(rec + 0xdc, RU16(row, 0x22));
    W16(rec + 0xde, RU16(row, 0x24));
    W16(rec + 0xe0, RU16(row, 0x26));
    W16(rec + 0xe2, RU16(row, 0x28));
    W16(rec + 0xe4, RU16(row, 0x2a));
    W16(rec + 0xe6, RU16(row, 0x2c));
    W16(rec + 0xe8, RU16(row, 0x2e));
    W16(rec + 0xea, RU16(row, 0x30));
    W16(rec + 0xec, RU16(row, 0x32));
    W16(rec + 0xee, RU16(row, 0x34));
    W16(rec + 0xf0, RU16(row, 0x36));
    W16(rec + 0xf2, RU16(row, 0x38));
    W16(rec + 0xf4, RU16(row, 0x3a));
    W16(rec + 0xf6, RU16(row, 0x3c));
    W16(rec + 0xf8, RU16(row, 0x3e));
    W16(rec + 0xfa, RU16(row, 0x40));
    W16(rec + 0xfc, RU16(row, 0x42));
    W16(rec + 0xfe, RU16(row, 0x44));
    W16(rec + 0x100, RU16(row, 0x46));
    W16(rec + 0x102, RU16(row, 0x48));
    W16(rec + 0x104, RU16(row, 0x4a));
    W16(rec + 0x106, RU16(row, 0x4c));
    W8(rec + 0xc0, RU8(row, 0x55));
    W8(rec + 0xc1, RU8(row, 0x56));
    W8(rec + 0xc2, RU8(row, 0x57));
    W8(rec + 0xc3, RU8(row, 0x58));
    W8(rec + 0xc4, RU8(row, 0x59));
    W8(rec + 0xc5, RU8(row, 0x5a));
    W8(rec + 0xc6, RU8(row, 0x5b));
    W8(rec + 0xc7, RU8(row, 0x5c));
    W8(rec + 0xc8, RU8(row, 0x5d));
    W8(rec + 0xc9, RU8(row, 0x5e));
    W8(rec + 0xca, RU8(row, 0x5f));
    W8(rec + 0xcb, RU8(row, 0x60));
    W8(rec + 0xcc, RU8(row, 0x61));
    W8(rec + 0xcd, RU8(row, 0x62));
    W8(rec + 0xce, RU8(row, 0x63));
    W8(rec + 0xcf, RU8(row, 0x64));
    W8(rec + 0xd0, RU8(row, 0x65));
    W8(rec + 0xd1, RU8(row, 0x66));
    W8(rec + 0xd2, RU8(row, 0x67));
    W8(rec + 0xd3, RU8(row, 0x68));
    W8(rec + 0xd4, RU8(row, 0x69));
    W8(rec + 0xd5, RU8(row, 0x6a));
    W8(rec + 0xd6, RU8(row, 0x6b));
    W8(rec + 0xd7, RU8(row, 0x6c));
    W16(rec + 0x108, RU8(row, 0x6f) * 10);
    W8(rec + 0x160, RU8(row, 0x53));
    W8(rec + 0xbc, RU8(row, 0x52));
    W8(rec + 0xbd, RU8(row, 0x51));
    W8(rec + 0xbe, RU8(row, 0x50));
    W8(rec + 0xbf, RU8(row, 0x54));
    W8(rec + 0x110, RU8(row, 0x6d));
    W8(rec + 0x111, RU8(row, 0x6e));
}

/* func_00441E28: drivetrain */
static void drivetrain(uint32_t rec, const uint8_t *row)
{
    W8(rec + 0x12a, RU8(row, 0x14));
    W8(rec + 0x128, RU8(row, 0x10));
    W8(rec + 0x129, RU8(row, 0x11));
    W16(rec + 0xba, RU16(row, 0x2));
    W16(rec + 0x174, RU16(row, 0x4));
    W16(rec + 0x176, RU16(row, 0x6));
    W16(rec + 0x164, RU16(row, 0x8));
    W16(rec + 0x166, RU16(row, 0xa));
    W16(rec + 0xb8, RU16(row, 0x0));
}

/* func_00441E78: gearbox, with the ratios set in the garage */
static void gear(uint32_t gar, uint32_t rec, const uint8_t *row)
{
    W16(rec + 0x152, RU16(row, 0x1e));
    W8(rec + 0x154, row[0x29]);
    for (uint32_t k = 0; k < 12; k++) W16(rec + 0x138 + k * 2, GU16(gar + 0x110 + k * 2));
    W16(rec + 0x150, GU16(gar + 0x128));
    W8(rec + 0x157, GU8(gar + 0x12a));
    W8(rec + 0x156, 0);
    W8(rec + 0x155, row[0x24]);
    W8(rec + 0x158, row[0x23]);
}

/* func_00441EE0: suspension, with the settings of the garage */
static void suspension(uint32_t gar, uint32_t rec, const uint8_t *row)
{
    W8(rec + 0x92, RU8(row, 0x2d));
    W8(rec + 0x93, RU8(row, 0x2e));
    W8(rec + 0x94, RU8(row, 0x31));
    W8(rec + 0x95, RU8(row, 0x32));
    W8(rec + 0x5a, RU8(row, 0x3c));
    W8(rec + 0x72, RU8(row, 0x3d));
    W8(rec + 0x1d0, RU8(row, 0x17));
    W8(rec + 0x1d1, RU8(row, 0x18));
    W8(rec + 0x1d2, RU8(row, 0x19));
    W8(rec + 0x1d3, RU8(row, 0x1a));
    W8(rec + 0x1d8, RU8(row, 0x1b));
    W8(rec + 0x1d9, RU8(row, 0x1c));
    W8(rec + 0x1da, RU8(row, 0x1d));
    W8(rec + 0x1db, RU8(row, 0x1e));
    W8(rec + 0x1d4, RU8(row, 0x1f));
    W8(rec + 0x1d5, RU8(row, 0x20));
    W8(rec + 0x1d6, RU8(row, 0x21));
    W8(rec + 0x1d7, RU8(row, 0x22));
    W8(rec + 0x1dc, RU8(row, 0x23));
    W8(rec + 0x1dd, RU8(row, 0x24));
    W8(rec + 0x1de, RU8(row, 0x25));
    W8(rec + 0x1df, RU8(row, 0x26));
    W8(rec + 0x90, RU8(row, 0x15));
    W8(rec + 0x91, RU8(row, 0x16));
    W8(rec + 0x5b, RU8(row, 0x64));
    W8(rec + 0x73, RU8(row, 0x65));
    W8(rec + 0x5c, RU8(row, 0x40));
    W8(rec + 0x5e, RU8(row, 0x42));
    W8(rec + 0x60, RU8(row, 0x44));
    W8(rec + 0x62, RU8(row, 0x46));
    W8(rec + 0x74, RU8(row, 0x41));
    W8(rec + 0x76, RU8(row, 0x43));
    W8(rec + 0x78, RU8(row, 0x45));
    W8(rec + 0x7a, RU8(row, 0x47));
    W16(rec + 0x66, RU8(row, 0x2f));
    W16(rec + 0x7e, RU8(row, 0x30));
    W8(rec + 0x65, RU8(row, 0x3e));
    W8(rec + 0x7d, RU8(row, 0x3f));
    W16(rec + 0x9a, GU16(gar + 0x13c));
    W16(rec + 0x9c, GU16(gar + 0x13e));
    W8(rec + 0x88, GU8(gar + 0x139));
    W8(rec + 0x89, GU8(gar + 0x13a));
    W8(rec + 0x8e, GU8(gar + 0x140));
    W8(rec + 0x8f, GU8(gar + 0x141));
    W8(rec + 0x58, GU8(gar + 0x142));
    W8(rec + 0x70, GU8(gar + 0x143));
    W8(rec + 0x8c, GU8(gar + 0x144));
    W8(rec + 0x8d, GU8(gar + 0x145));
    W8(rec + 0xa8, RU8(row, 0x6e));
    W8(rec + 0x5d, lerp(row[0x49], row[0x4a], row[0x48], GT4_U8(gar + 0x146)));
    W8(rec + 0x5f, lerp(row[0x4c], row[0x4d], row[0x48], GT4_U8(gar + 0x147)));
    W8(rec + 0x61, lerp(row[0x50], row[0x51], row[0x4f], GT4_U8(gar + 0x146)));
    W8(rec + 0x63, lerp(row[0x53], row[0x54], row[0x4f], GT4_U8(gar + 0x147)));
    W8(rec + 0x75, lerp(row[0x57], row[0x58], row[0x56], GT4_U8(gar + 0x14a)));
    W8(rec + 0x77, lerp(row[0x5a], row[0x5b], row[0x56], GT4_U8(gar + 0x14b)));
    W8(rec + 0x79, lerp(row[0x5e], row[0x5f], row[0x5d], GT4_U8(gar + 0x14c)));
    W8(rec + 0x7b, lerp(row[0x61], row[0x62], row[0x5d], GT4_U8(gar + 0x14d)));
    W8(rec + 0x59, lerp(row[0x67], row[0x68], row[0x66], GT4_U8(gar + 0x14e)));
    W8(rec + 0x71, lerp(row[0x6b], row[0x6c], row[0x6a], GT4_U8(gar + 0x14f)));
    W8(rec + 0xa8, row[0x6e]);
}

/* func_004421D0: differential, with the LSD settings of the garage */
static void lsd(uint32_t gar, uint32_t rec, const uint8_t *row)
{
    W8(rec + 0x168, RU8(row, 0x3));
    W8(rec + 0x169, RU8(row, 0xf));
    W8(rec + 0x16a, GU8(gar + 0x150));
    W8(rec + 0x16b, GU8(gar + 0x151));
    W8(rec + 0x16c, GU8(gar + 0x152));
    W8(rec + 0x16d, GU8(gar + 0x153));
    W8(rec + 0x16e, GU8(gar + 0x154));
    W8(rec + 0x16f, GU8(gar + 0x155));
}

/* func_00442490: a tyre block (t) from the compound (row) and force (row2) rows */
static void tyre_block(uint32_t t, const uint8_t *row, const uint8_t *row2)
{
    W8(t + 0x0, RU8(row, 0x1));
    W8(t + 0x2, RU8(row, 0xa));
    W8(t + 0x8, 8);
    W8(t + 0x1, RU8(row, 0x0));
    W8(t + 0x10, RU8(row, 0xb));
    W8(t + 0x11, RU8(row, 0xc));
    W8(t + 0x12, RU8(row, 0xd));
    W8(t + 0x13, RU8(row, 0xe));
    W8(t + 0x14, RU8(row, 0xf));
    W8(t + 0x15, RU8(row, 0x10));
    W8(t + 0x16, RU8(row, 0x11));
    W8(t + 0x17, RU8(row, 0x12));
    W8(t + 0x18, RU8(row, 0x13));
    W8(t + 0x19, RU8(row, 0x14));
    W8(t + 0x1a, RU8(row, 0x15));
    W8(t + 0x1b, RU8(row, 0x16));
    W8(t + 0x1c, RU8(row, 0x17));
    W8(t + 0x1d, RU8(row, 0x18));
    W8(t + 0x1e, RU8(row, 0x19));
    W8(t + 0x9, 6);
    W8(t + 0x1f, RU8(row, 0x1a));
    W8(t + 0x20, RU8(row, 0x27));
    W8(t + 0x21, RU8(row, 0x28));
    W8(t + 0x22, RU8(row, 0x29));
    W8(t + 0x23, RU8(row, 0x2a));
    W8(t + 0x24, RU8(row, 0x2b));
    W8(t + 0x25, RU8(row, 0x2c));
    W8(t + 0x30, RU8(row, 0x2d));
    W8(t + 0x31, RU8(row, 0x2e));
    W8(t + 0x32, RU8(row, 0x2f));
    W8(t + 0x33, RU8(row, 0x30));
    W8(t + 0x34, RU8(row, 0x31));
    W8(t + 0x35, RU8(row, 0x32));
    W8(t + 0x40, RU8(row, 0x39));
    W8(t + 0x41, RU8(row, 0x3a));
    W8(t + 0x42, RU8(row, 0x3b));
    W8(t + 0x43, RU8(row, 0x3c));
    W8(t + 0x44, RU8(row, 0x3d));
    W8(t + 0xa, 6);
    W8(t + 0x45, RU8(row, 0x3e));
    W8(t + 0x28, RU8(row, 0x3f));
    W8(t + 0x29, RU8(row, 0x40));
    W8(t + 0x2a, RU8(row, 0x41));
    W8(t + 0x2b, RU8(row, 0x42));
    W8(t + 0x2c, RU8(row, 0x43));
    W8(t + 0x2d, RU8(row, 0x44));
    W8(t + 0x38, RU8(row, 0x45));
    W8(t + 0x39, RU8(row, 0x46));
    W8(t + 0x3a, RU8(row, 0x47));
    W8(t + 0x3b, RU8(row, 0x48));
    W8(t + 0x3c, RU8(row, 0x49));
    W8(t + 0x3d, RU8(row, 0x4a));
    W8(t + 0x48, RU8(row, 0x51));
    W8(t + 0x49, RU8(row, 0x52));
    W8(t + 0x4a, RU8(row, 0x53));
    W8(t + 0x4b, RU8(row, 0x54));
    W8(t + 0x4c, RU8(row, 0x55));
    W8(t + 0xb, 4);
    W8(t + 0x4d, RU8(row, 0x56));
    W8(t + 0x5c, RU8(row, 0x2));
    W8(t + 0x5d, RU8(row, 0x3));
    W8(t + 0x5e, RU8(row, 0x4));
    W8(t + 0x5f, RU8(row, 0x5));
    W8(t + 0x60, RU8(row, 0x6));
    W8(t + 0x61, RU8(row, 0x7));
    W8(t + 0x62, RU8(row, 0x8));
    W8(t + 0xc, 6);
    W8(t + 0x63, RU8(row, 0x9));
    W8(t + 0x50, RU8(row, 0x1b));
    W8(t + 0x51, RU8(row, 0x1c));
    W8(t + 0x52, RU8(row, 0x1d));
    W8(t + 0x53, RU8(row, 0x1e));
    W8(t + 0x54, RU8(row, 0x1f));
    W8(t + 0x55, RU8(row, 0x20));
    W8(t + 0x56, RU8(row, 0x21));
    W8(t + 0x57, RU8(row, 0x22));
    W8(t + 0x58, RU8(row, 0x23));
    W8(t + 0x59, RU8(row, 0x24));
    W8(t + 0x5a, RU8(row, 0x25));
    W8(t + 0x5b, RU8(row, 0x26));
    W8(t + 0x3, RU8(row, 0x57));
    W8(t + 0x4, RU8(row, 0x58));
    W8(t + 0x5, RU8(row, 0x59));
    W8(t + 0x64, RU8(row2, 0x0));
    W8(t + 0x65, RU8(row2, 0x1));
    W8(t + 0x66, RU8(row2, 0x2));
    W8(t + 0x67, RU8(row2, 0x3));
    W8(t + 0x68, RU8(row2, 0x4));
    W8(t + 0x69, RU8(row2, 0x5));
    W8(t + 0x6a, RU8(row2, 0x6));
    W8(t + 0x6b, RU8(row2, 0x7));
    W8(t + 0x6c, RU8(row2, 0x8));
    W8(t + 0x6d, RU8(row2, 0x9));
    W8(t + 0x6e, RU8(row2, 0xd));
    W8(t + 0x6f, RU8(row2, 0xe));
    W8(t + 0x70, RU8(row2, 0xf));
    W8(t + 0x71, RU8(row2, 0x10));
    W8(t + 0x72, RU8(row2, 0x11));
    W8(t + 0x78, RU8(row2, 0xa));
    W8(t + 0x79, RU8(row2, 0xc));
    W8(t + 0x7a, RU8(row2, 0xb));
}

/* func_00442218 / func_00442358: the three tyre blocks of an axle (row:
   the FRONTTIRE or REARTIRE row, keys of the compound and force rows),
   and the tyre size. */
static void tyres(const gt4_specdb_src *db, uint32_t rec, const uint8_t *row, int rear)
{
    uint8_t a[0x100], b[0x100], size[0x100];
    memset(a, 0, sizeof a);
    memset(b, 0, sizeof b);
    memset(size, 0, sizeof size);
    for (uint32_t i = 0; i < 3; i++) {
        uint64_t ka, kb;
        memcpy(&ka, row + 8 + i * 8, 8);
        memcpy(&kb, row + 0x20 + i * 8, 8);
        fetch(db, ka, a, sizeof a);
        fetch(db, kb, b, sizeof b);
        tyre_block(rec + (rear ? 0x408u : 0x210u) + i * 0xa8, a, b);
    }
    {
        uint64_t ks;
        memcpy(&ks, row, 8);
        fetch(db, ks, size, sizeof size);
    }
    if (!rear) {
        W8(rec + 0x1ce, 4);
        W8(rec + 0x1ca, size[2]);
        W8(rec + 0x1cc, size[0]);
        W8(rec + 0x1c8, size[3]);
    } else {
        W8(rec + 0x1cb, size[2]);
        W8(rec + 0x1c9, size[3]);
        W8(rec + 0x1cd, size[0]);
        W8(rec + 0x1cf, 4);
    }
}

/* Tuning parts that scale values in percent. */
static void scale16(uint32_t at, uint32_t pct)
{
    W16(at, (int32_t)GU16(at) * (int32_t)pct / 100);
}
static void scale16s(uint32_t at, uint32_t pct)
{
    W16(at, GS16(at) * (int32_t)pct / 100);
}
static void scale8(uint32_t at, uint32_t pct)
{
    W8(at, (int32_t)GU8(at) * (int32_t)pct / 100);
}
/* the power (+0x10A, per 1000) and the torque (+0x10C) factors */
static void power_parts(uint32_t rec, const uint8_t *row, uint32_t a, uint32_t b)
{
    scale16(rec + 0x10a, RU16(row, a));
    scale8(rec + 0x10c, RU16(row, b));
}

/* func_004427B8: weight reduction */
static void lightweight(uint32_t rec, const uint8_t *row)
{
    W16(rec + 0x20, GS16(rec + 0x20) * (int32_t)RU16(row, 0) / 1000);
    scale8(rec + 0x24, row[5]);
}

/* func_00442800: racing modification */
static void racing_modify(uint32_t gar, uint32_t rec, const uint8_t *row)
{
    W16(rec + 0x20, GS16(rec + 0x20) * (int32_t)RU16(row, 0) / 1000);
    scale8(rec + 0x24, row[0x17]);
    W8(rec + 0x42, row[0xc]);
    W16(rec + 0x1c, RU16(row, 2));
    W16(rec + 0x1e, RU16(row, 4));
    W8(rec + 0x43, GU8(gar + 0x131));
    W8(rec + 0x44, GU8(gar + 0x132));
    W8(rec + 0x96, row[0x13]);
    W8(rec + 0x97, row[0x14]);
    W8(rec + 0x98, row[0x15]);
    W8(rec + 0x99, row[0x16]);
    W8(rec + 0x4c, row[0x18]);
    W8(rec + 0x4d, row[0x19]);
    W8(rec + 0x4e, row[0x1a]);
    W8(rec + 0x4f, row[0x1b]);
    W8(rec + 0x50, row[0x1c]);
}

/* func_00442900 / func_00442AE8: the rpm points move up; power and torque factors */
static void rev_parts(uint32_t rec, const uint8_t *row, uint32_t shift, uint32_t red, uint32_t a, uint32_t b)
{
    for (uint32_t k = 0; k < 16; k++) W8(rec + 0xc0 + k, GU8(rec + 0xc0 + k) + row[shift]);
    W8(rec + 0xbd, GU8(rec + 0xbd) + row[red]);
    power_parts(rec, row, a, b);
}

/* func_00442C40: turbo kit, with its settings from the garage */
static void turbo(uint32_t gar, uint32_t rec, const uint8_t *row)
{
    power_parts(rec, row, 4, 6);
    W8(rec + 0x113, row[9]);
    W8(rec + 0xbd, GU8(rec + 0xbd) + row[0x10]);
    W8(rec + 0xbe, GU8(rec + 0xbe) + row[0x11]);
    W8(rec + 0x118, GU8(gar + 0x133));
    W8(rec + 0x116, GU8(gar + 0x134));
    W8(rec + 0x11a, GU8(gar + 0x135));
    W8(rec + 0x119, GU8(gar + 0x136));
    W8(rec + 0x117, GU8(gar + 0x137));
    W8(rec + 0x11b, GU8(gar + 0x138));
}

/* func_00442CE0 / func_00442D60: clutch, flywheel */
static void clutch(uint32_t rec, const uint8_t *row)
{
    W16(rec + 0xba, (int32_t)GU16(rec + 0xba) * (int32_t)row[4] / 100);
    W16(rec + 0xb8, (int32_t)GU16(rec + 0xb8) * (int32_t)row[3] / 100);
    scale16s(rec + 0x174, row[5]);
    scale16s(rec + 0x176, row[6]);
}

/* func_00442DE8: propeller shaft */
static void shaft(uint32_t rec, const uint8_t *row)
{
    W16(rec + 0xba, (int32_t)GU16(rec + 0xba) * (int32_t)row[3] / 100);
    scale16s(rec + 0x174, row[4]);
    scale16s(rec + 0x176, row[5]);
    scale16s(rec + 0x164, row[6]);
    scale16s(rec + 0x166, row[7]);
}

/* func_00442F10: stability and traction aids (ASCC), levels from the garage */
static void ascc(uint32_t gar, uint32_t rec, const uint8_t *row)
{
    uint32_t l;
    l = GT4_U8(gar + 0x157);
    W8(rec + 0x1ba, lerp(row[5], row[6], row[4], l ? l : 1));
    W8(rec + 0x1bb, row[8]);
    W8(rec + 0x1bc, lerp(row[0xb], row[0xc], row[0xa], GT4_U8(gar + 0x158)));
    l = GT4_U8(gar + 0x159);
    W8(rec + 0x1bd, lerp(row[0x10], row[0x11], row[0xf], l ? l : 1));
    W8(rec + 0x1be, row[0x13]);
    W8(rec + 0x1bf, lerp(row[0x16], row[0x17], row[0x15], GT4_U8(gar + 0x15a)));
    W8(rec + 0x1b8, (row[3] & 3) | (row[9] & 3) << 2 | (row[0xe] & 3) << 4 | (row[0x14] & 3) << 6);
    if (!GT4_U8(gar + 0x157)) W8(rec + 0x1b8, GU8(rec + 0x1b8) & 0xf0);          /* func_003F16C0 */
    if (!GT4_U8(gar + 0x159))                                                    /* func_003F16D0 */
        W8(rec + 0x1b8, GU8(rec + 0x1b8) & (GT4_U8(0x006224a8u) ? 0xcf : 0x0f));
}

/* func_00443040: traction control (TCSC) */
static void tcsc(uint32_t gar, uint32_t rec, const uint8_t *row)
{
    uint32_t l = GT4_U8(gar + 0x156);
    W8(rec + 0x1c2, row[5]);
    W8(rec + 0x1c0, row[3]);
    W8(rec + 0x1c1, row[4]);
    W8(rec + 0x1c3, lerp(row[7], row[8], row[6], l ? l : 1));
    if (!GT4_U8(gar + 0x156)) W8(rec + 0x1c2, 0);                                 /* func_003F1748 */
}

/* func_004458D0: the VARIATION row of the car in the entry's colour
   (+0x10: its key; rows with the same id, +4 = colour + 1). */
static int variation(const gt4_specdb_src *db, uint32_t gar, int color, uint8_t *buf, size_t cap)
{
    uint64_t key = key_at(gar, 0x10);
    uint32_t id = (uint32_t)key, table = (uint32_t)(key >> 32);
    int count = 0;
    while (db->row(db->ctx, table, id, (uint32_t)count, NULL, 0) > 0) count++;
    if (!(color < count)) return 0;
    for (int k = 0; k < count; k++) {
        uint32_t c;
        db->row(db->ctx, table, id, (uint32_t)k, buf, cap);
        memcpy(&c, buf + 4, 4);
        if (c == (uint32_t)color + 1) return 1;
    }
    return 0;
}

/* float to u32 as the game converts (2^31 and up wrap into the sign bit) */
static uint32_t f2u(float f)
{
    if (!(K(0x4f000000u) <= f)) return (uint32_t)(int32_t)f;
    return (uint32_t)(int32_t)(f - K(0x4f000000u)) | 0x80000000u;
}

/* func_004430F0: body dimensions from the VARIATION row */
static void dimensions(uint32_t rec, const uint8_t *row)
{
    float f;
    memcpy(&f, row + 0x14, 4); W16(rec + 0x16, f2u(f));
    memcpy(&f, row + 0x10, 4); W16(rec + 0x18, f2u(f));
    memcpy(&f, row + 0x18, 4); W16(rec + 0x12, f2u(f));
    memcpy(&f, row + 0x1c, 4); W16(rec + 0x14, f2u(f));
    memcpy(&f, row + 0x20, 4); W16(rec + 0x40, (int32_t)(f * 10000.0f));
}

/* func_004461F8: the car's class from GENERIC_CAR (+0x11) and the tyres */
static int car_class(const gt4_specdb_src *db, uint32_t gar)
{
    uint8_t row[0x100] = { 0 };
    int c;
    if (!fetch(db, key_at(gar, 0), row, sizeof row)) return 0;
    c = row[0x11];
    if (c == 1) return 3;
    if (c == 2) return 2;
    if (c == 0 || c == 3) {
        uint8_t t[0x100] = { 0 };
        uint64_t key = key_at(gar, 0xd0);
        if (is_none(key)) return 0;
        fetch(db, key, t, sizeof t);                                           /* func_00446820 */
        return t[6] > 0 ? 1 : 0;
    }
    return 0;
}

/* func_004413C8 */
void gt4_car_record_rows(const gt4_specdb_src *db, uint32_t gar, uint32_t rec)
{
    uint8_t s[0x200];                    /* the original's stack: rows stay when a key has none */
#define ROW(off, at) fetch(db, key_at(gar, off), s + (at), 0x100)
#define ROW_IF(off) (!is_none(key_at(gar, off)) && (ROW(off, 0), 1))
    memset(s, 0, sizeof s);
    W8(rec + 0x10c, 100);
    W16(rec + 0x10a, 1000);
    W8(rec + 0x113, 0);
    ROW(0x20, 0);    brake(rec, s);
    ROW(0x28, 0x10); brake_controller(gar, rec, s + 0x10);
    ROW(0x70, 0x20); steer(rec, s + 0x20);
    ROW(0x30, 0);    chassis(db, gar, rec, s);
    ROW(0x38, 0x50); engine(rec, s + 0x50);
    ROW(0x40, 0);    drivetrain(rec, s);
    ROW(0x48, 0);    gear(gar, rec, s);
    ROW(0x50, 0);    suspension(gar, rec, s);
    ROW(0x58, 0);    lsd(gar, rec, s);
    ROW(0x60, 0);    tyres(db, rec, s, 0);
    ROW(0x68, 0);    tyres(db, rec, s, 1);
    if (ROW_IF(0x78)) lightweight(rec, s);
    ROW(0x80, 0);    racing_modify(gar, rec, s);
    if (ROW_IF(0x88)) power_parts(rec, s, 0, 2);          /* func_004428B8 */
    if (ROW_IF(0x90)) rev_parts(rec, s, 7, 8, 0, 2);      /* func_00442900 */
    if (ROW_IF(0x98)) power_parts(rec, s, 0, 2);          /* func_00442A58 */
    if (ROW_IF(0xa0)) power_parts(rec, s, 0, 2);          /* func_00442AA0 */
    if (ROW_IF(0xa8)) rev_parts(rec, s, 9, 0xa, 4, 6);    /* func_00442AE8 */
    ROW(0xb0, 0);    turbo(gar, rec, s);
    if (ROW_IF(0xb8)) clutch(rec, s);
    if (ROW_IF(0xc0)) { clutch(rec, s); W8(rec + 0x161, s[7]); }   /* func_00442D60 */
    if (ROW_IF(0xc8)) shaft(rec, s);
    if (ROW_IF(0xd0)) power_parts(rec, s, 0, 2);          /* func_00442E80 */
    if (ROW_IF(0xd8)) power_parts(rec, s, 0, 2);          /* func_00442EC8 */
    if (ROW_IF(0x108)) power_parts(rec, s, 0, 2);         /* func_004431F8 */
    ROW(0xe0, 0);    ascc(gar, rec, s);
    ROW(0xe8, 0);    tcsc(gar, rec, s);
    W8(rec + 5, GU8(gar + 0x15b));                        /* func_004430B8 */
    W8(rec + 6, GU8(gar + 0x15c));
    if (ROW_IF(0x100)) {}                                 /* func_004430D0 does nothing */
    if (ROW_IF(0xf8)) {                                   /* func_004430D8 */
        W8(rec + 0x11d, GU8(gar + 0x167));
        W16(rec + 0x11e, RU16(s, 0));
    }
    if ((int8_t)GT4_U8(gar + 0x18) == -1) GT4_U8(gar + 0x18) = 0;
    variation(db, gar, (int8_t)GT4_U8(gar + 0x18), s, sizeof s);
    dimensions(rec, s);
    W8(rec + 0x25, GU8(rec + 0x24));
    W8(rec + 0x26, GU8(rec + 0x24) >> 1);
    W8(rec + 0x120, car_class(db, gar));
    ROW(0, 0);
    W8(rec + 1, s[0x13]);
    W8(rec + 4, s[0x14]);
    for (uint32_t k = GU8(rec + 0x155) + 1; k < 12; k++) W16(rec + 0x138 + k * 2, 0xffff);
    for (uint32_t k = GU8(rec + 0xbf); k < 16; k++) {
        W8(rec + 0xc0 + k, 0xff);
        W16(rec + 0xd8 + k * 2, 0);
    }
    W8(rec + 7, GU8(gar + 0x163));
    W8(rec + 8, GU8(gar + 0x162));
    if (GU16(gar + 0x15e)) W16(rec + 0x108, GU16(gar + 0x15e));
#undef ROW
#undef ROW_IF
}

/* func_003F3B68: the record (rec, 0x600 bytes) of a garage entry (gar);
   mode: func_0034C968 of the race (2 and 3 add the race's settings). */
void gt4_car_record(uint32_t rec, uint32_t gar, uint32_t mode, const gt4_specdb_src *db)
{
    uint32_t cfg = GAME_SETTINGS, kind;
    memset(gt4_ptr(rec), 0, 0x600);
    gt4_car_record_rows(db, gar, rec);
    W8(rec, 2);
    if (!GU8(rec + 0x11c)) W8(rec + 0x11c, GU8(cfg + 0x14a));
    if (!GU8(rec + 0x112)) W8(rec + 0x112, GU8(cfg + 0x14b));
    kind = GU8(rec + 1);
    if (kind == 2 || kind == 3 || kind == 4) {
        W8(rec + 0x11c, 0);
        W8(rec + 0x112, 0);
    }
    if (kind == 2 && mode - 2 < 2) {
        uint32_t a0 = GT4_U8(0x006224a0u), a1 = GT4_U8(0x006224a1u), a2 = GT4_U8(0x006224a2u), a3 = GT4_U8(0x006224a3u);
        W8(rec + 0xa8, 3);
        W16(rec + 0x66, GU16(rec + 0x66) + a1);
        W16(rec + 0x7e, GU16(rec + 0x7e) + a1);
        W8(rec + 0x92, GU8(rec + 0x92) + a2);
        W8(rec + 0x93, GU8(rec + 0x93) + a2);
        W16(rec + 0x9a, GU16(rec + 0x9a) + a0);
        W16(rec + 0x9c, GU16(rec + 0x9c) + a0);
        W8(rec + 0x94, GU8(rec + 0x94) + a3);
        W8(rec + 0x95, GU8(rec + 0x95) + a3);
        W8(rec + 0x58, GT4_U8(0x006224a4u));
        W8(rec + 0x70, GT4_U8(0x006224a5u));
    }
    if (!GS16(rec + 0x12)) W16(rec + 0x12, GS16(rec + 0x10) / 2);
    if (!GS16(rec + 0x14)) W16(rec + 0x14, GS16(rec + 0x10) / 2);
    if (GU8(cfg + 0x13d)) W8(rec + 0x25, (int32_t)GU8(rec + 0x24) * (int32_t)GU8(cfg + 0x13d) / 100);
    if (GU8(cfg + 0x13e)) W8(rec + 0x26, (int32_t)GU8(rec + 0x24) * (int32_t)GU8(cfg + 0x13e) / 100);
    /* default steering curves (D_006A4028/30: six speeds, six angles) */
    if (!GU8(rec + 0x182)) {
        W8(rec + 0x182, 6);
        memcpy(gt4_ptr(rec + 0x19c), gt4_ptr(0x006a4028u), 6);
        memcpy(gt4_ptr(rec + 0x1a2), gt4_ptr(0x006a4030u), 6);
    }
    if (!GU8(rec + 0x195)) {
        W8(rec + 0x181, 6);
        memcpy(gt4_ptr(rec + 0x190), gt4_ptr(0x006a4028u), 6);
        memcpy(gt4_ptr(rec + 0x196), gt4_ptr(0x006a4030u), 6);
    }
    if (!GU8(rec + 0x1aa)) W8(rec + 0x1aa, 0x8c);
    if (!GU8(rec + 0x1ab)) W8(rec + 0x1ab, 100);
    if (GU8(rec + 0x1c3) == 100) W8(rec + 0x1c2, 0);
    /* surfaces: which tyre block each of the 24 surfaces uses */
    W8(rec + 0x1ff, 1);
    W8(rec + 0x207, 2);
    W8(rec + 0x1e0, 1);
    for (uint32_t k = 0x1f2; k <= 0x1f5; k++) W8(rec + k, 1);
    W8(rec + 0x1fd, 1);
    W8(rec + 0x1fe, 1);
    W8(rec + 0x1f6, 2);
    for (uint32_t k = 0x204; k <= 0x206; k++) W8(rec + k, 2);
    for (uint32_t t = 0; t < 6; t++) {
        uint32_t b = rec + t * 0xa8;
        for (uint32_t k = 0; k < 5; k++)
            if (!GU8(b + 0x27e + k)) W8(b + 0x27e + k, 100);
        if (!GU8(b + 0x210 + 0x73)) W8(b + 0x210 + 0x73, 0x50);
    }
    {
        uint32_t t = rec + 0x210;
        if (!GU8(t + 0x78)) W8(t + 0x78, GU8(t + 0x64));
        if (!GU8(t + 0x79)) W8(t + 0x79, GU8(t + 0x65));
        if (!GU8(t + 0x7a)) W8(t + 0x7a, GU8(t + 0x6d));
    }
    for (uint32_t i = 0; i < 6; i++) {
        uint32_t t = rec + 0x210 + i * 0xa8;
        if (!GU8(t + 2)) W8(t + 2, 0x2d);
        if (!GU8(t + 0xc)) {
            W8(t + 0xc, 6);
            for (uint32_t k = 0; k < 6; k++) {
                W8(t + 0x50 + k, GT4_U8(0x00622420u + k));
                W8(t + 0x56 + k, GT4_U8(0x00622428u + k));
            }
        }
        for (uint32_t k = 0; k < 32; k++)
            if (!GU8(t + 0x64 + k)) W8(t + 0x64 + k, GU8(rec + 0x274 + k));
    }
}
