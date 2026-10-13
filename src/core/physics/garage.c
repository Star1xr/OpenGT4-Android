/* A new car's garage entry (0x178 bytes) from SpecDB: func_00444480. The
   GENERIC_CAR row of the car points to its DEFAULT_PARTS row (one key per
   part table); the keys go to their slots, the tuning settings get the
   defaults of the parts, the colour its first variation. gt4_car_record
   then builds the record from the entry. */
#include "car.h"

#include <string.h>
#include <stdlib.h>

#define GU8(a)  ((uint32_t)GT4_U8(a))
#define GU16(a) ((uint32_t)GT4_U16(a))
#define W8(a, v)  (GT4_U8(a) = (uint8_t)(v))
#define W16(a, v) (GT4_U16(a) = (uint16_t)(v))
#define RU16(r, o) ((int32_t)((r)[o] | (r)[(o) + 1] << 8))

#define SPEC_TABLES      0x2bu
#define SPEC_CAR_VARIATION 0x29u

static uint64_t key_at(uint32_t gar, uint32_t off)
{
    return (uint64_t)GT4_U32(gar + off) | (uint64_t)GT4_U32(gar + off + 4) << 32;
}
static void set_key(uint32_t gar, uint32_t off, uint64_t key)
{
    GT4_U32(gar + off) = (uint32_t)key;
    GT4_U32(gar + off + 4) = (uint32_t)(key >> 32);
}

/* func_00443F40 */
static int exists(const gt4_specdb_src *db, uint64_t key)
{
    uint32_t table = (uint32_t)(key >> 32);
    return table < SPEC_TABLES && db->row(db->ctx, table, (uint32_t)key, 0, NULL, 0) > 0;
}
/* func_00443ED0 */
static int fetch(const gt4_specdb_src *db, uint64_t key, uint8_t *buf, size_t cap)
{
    return db->row(db->ctx, (uint32_t)(key >> 32), (uint32_t)key, 0, buf, cap);
}
/* func_004452A8 */
static int clampi(int lo, int v, int hi)
{
    if (v < lo) v = lo;
    if (hi < v) v = hi;
    return v;
}

/* func_004442B8: no parts, default settings */
static void garage_clear(uint32_t gar)
{
    for (uint32_t o = 0; o <= 0x108; o += 8) if (o != 0x18) set_key(gar, o, ~(uint64_t)0);
    W8(gar + 0x18, 0xff);
    W8(gar + 0x166, 1);
    W8(gar + 0x161, 0xff);
    W8(gar + 0x164, 100);
    W16(gar + 0x15e, 0);
    W8(gar + 0x160, 0xff);
    W8(gar + 0x162, 100);
    W8(gar + 0x163, 100);
    W8(gar + 0x165, 0);
    W8(gar + 0x167, 0);
    W8(gar + 0x168, 0);
    for (uint32_t o = 0x169; o <= 0x170; o++) W8(gar + o, 0);
}

/* The default settings of the parts (func_004449C0 and the functions it
   calls); each needs its part, and the chain stops at the first missing. */
static int part_row(const gt4_specdb_src *db, uint32_t gar, uint32_t slot, uint8_t *r)
{
    uint64_t key = key_at(gar, slot);
    if (!exists(db, key)) return 0;
    fetch(db, key, r, 0x100);
    return 1;
}

static int defaults(const gt4_specdb_src *db, uint32_t gar)
{
    uint8_t r[0x100];
    memset(r, 0, sizeof r);
    /* func_00444A88: gear ratios, final drive, top speed setting */
    if (!part_row(db, gar, 0x48, r)) return 0;
    W16(gar + 0x112, RU16(r, 0));
    W16(gar + 0x11a, RU16(r, 8));
    W16(gar + 0x11c, RU16(r, 0xa));
    W16(gar + 0x11e, RU16(r, 0xc));
    W16(gar + 0x120, RU16(r, 0xe));
    W16(gar + 0x122, RU16(r, 0x10));
    W16(gar + 0x124, RU16(r, 0x12));
    W16(gar + 0x126, RU16(r, 0x14));
    W16(gar + 0x110, RU16(r, 0x16));
    W16(gar + 0x114, RU16(r, 2));
    W16(gar + 0x116, RU16(r, 4));
    W16(gar + 0x118, RU16(r, 6));
    W16(gar + 0x128, clampi(RU16(r, 0x18), RU16(r, 0x1c), RU16(r, 0x1a)));
    W8(gar + 0x12a, clampi(r[0x26], r[0x28], r[0x27]));
    W16(gar + 0x12c, GU16(gar + 0x128));
    /* func_00444B88: drivetrain */
    if (!part_row(db, gar, 0x40, r)) return 0;
    W8(gar + 0x12e, clampi(r[0x12], r[0x14], r[0x13]));
    /* func_00444C08: brake controller */
    if (!part_row(db, gar, 0x28, r)) return 0;
    W8(gar + 0x12f, clampi(1, r[6], r[3]));
    W8(gar + 0x130, clampi(1, r[0xa], r[7]));
    /* func_00444CA0: racing modification (aero) */
    if (!part_row(db, gar, 0x80, r)) return 0;
    W8(gar + 0x131, clampi(r[0xd], r[0xf], r[0xe]));
    W8(gar + 0x132, clampi(r[0x10], r[0x12], r[0x11]));
    /* func_00444D38: turbo kit */
    if (!part_row(db, gar, 0xb0, r)) return 0;
    for (uint32_t k = 0; k < 6; k++) W8(gar + 0x133 + k, r[0xa + k]);
    /* func_00444DD0: suspension */
    if (!part_row(db, gar, 0x50, r)) return 0;
    W8(gar + 0x141, r[0x2c]);
    W8(gar + 0x140, r[0x29]);
    W8(gar + 0x139, clampi(r[0xf], r[0x11], r[0x10]));
    W8(gar + 0x13a, clampi(r[0x12], r[0x14], r[0x13]));
    W16(gar + 0x13c, clampi(RU16(r, 0), RU16(r, 4), RU16(r, 2)));
    W16(gar + 0x13e, clampi(RU16(r, 6), RU16(r, 0xa), RU16(r, 8)));
    W8(gar + 0x142, clampi(r[0x34], r[0x36], r[0x35]));
    W8(gar + 0x143, clampi(r[0x37], r[0x39], r[0x38]));
    W8(gar + 0x144, r[0x3a]);
    W8(gar + 0x145, r[0x3b]);
    W8(gar + 0x146, clampi(1, r[0x4b], r[0x48]));
    W8(gar + 0x147, clampi(1, r[0x4e], r[0x48]));
    W8(gar + 0x148, clampi(1, r[0x52], r[0x4f]));
    W8(gar + 0x149, clampi(1, r[0x55], r[0x4f]));
    W8(gar + 0x14a, clampi(1, r[0x59], r[0x56]));
    W8(gar + 0x14b, clampi(1, r[0x5c], r[0x56]));
    W8(gar + 0x14c, clampi(1, r[0x60], r[0x5d]));
    W8(gar + 0x14d, clampi(1, r[0x63], r[0x5d]));
    W8(gar + 0x14e, clampi(1, r[0x69], r[0x66]));
    W8(gar + 0x14f, clampi(1, r[0x6d], r[0x6a]));
    /* func_00444FE0: differential */
    if (!part_row(db, gar, 0x58, r)) return 0;
    W8(gar + 0x150, clampi(r[4], r[6], r[5]));
    W8(gar + 0x151, clampi(r[0x10], r[0x12], r[0x11]));
    W8(gar + 0x152, clampi(r[8], r[0xa], r[9]));
    W8(gar + 0x153, clampi(r[0x14], r[0x16], r[0x15]));
    W8(gar + 0x154, clampi(r[0xc], r[0xe], r[0xd]));
    W8(gar + 0x155, clampi(r[0x18], r[0x1a], r[0x19]));
    /* func_004450D8: stability aids */
    if (!part_row(db, gar, 0xe0, r)) return 0;
    W8(gar + 0x157, clampi(1, r[7], r[4]));
    W8(gar + 0x158, clampi(1, r[0xd], r[0xa]));
    W8(gar + 0x159, clampi(1, r[0x12], r[0xf]));
    W8(gar + 0x15a, clampi(1, r[0x18], r[0x15]));
    /* func_004451A0: traction control */
    if (!part_row(db, gar, 0xe8, r)) return 0;
    W8(gar + 0x156, clampi(1, r[9], r[6]));
    /* func_00445220: ballast */
    W8(gar + 0x15c, 0);
    W8(gar + 0x15b, 0);
    /* func_00445230: nitrous */
    {
        uint64_t key = key_at(gar, 0xf8);
        if (key == ~(uint64_t)0) return 1;
        if (!exists(db, key)) return 0;
        fetch(db, key, r, sizeof r);
        W8(gar + 0x167, r[5]);
    }
    return 1;
}

/* func_00443240: gear ratios from the top speed setting when the gearbox
   has them (GEAR +0x25). scratch: guest memory as the original's frame
   (row at +0, ratios at +0x30, a record at +0x50, 0x20 more after it). */
static int auto_gear_ratios(const gt4_specdb_src *db, uint32_t gar, uint32_t scratch)
{
    uint64_t key = key_at(gar, 0x48);
    uint32_t rec = scratch + 0x50;
    if (!exists(db, key)) return 0;
    fetch(db, key, gt4_ptr(scratch), 0x100);
    if (!GT4_U8(scratch + 0x25)) return 1;
    gt4_car_record_rows(db, gar, rec);
    gt4_car_auto_ratios(rec, (int32_t)GU8(gar + 0x12a) * 10, scratch + 0x30, 1, rec + 0x600);
    for (uint32_t k = 0; k < 12; k++) W16(gar + 0x110 + k * 2, GU16(scratch + 0x30 + k * 2));
    return 1;
}

/* func_00444480: 1 when the car and its parts exist. scratch: 0x680
   bytes of guest memory. */
int gt4_garage_new(uint32_t gar, uint64_t car, const gt4_specdb_src *db, uint32_t scratch)
{
    /* the parts row in the table order: the garage slot of each table */
    static const struct { uint16_t slot, at; } parts[] = {
        { 0x78, 0x38 }, { 0x80, 0x30 }, { 0x88, 0x70 }, { 0x28, 0x08 }, { 0x30, 0x28 },
        { 0x38, 0x58 }, { 0x40, 0x48 }, { 0x48, 0x50 }, { 0x50, 0x10 }, { 0x58, 0xb8 },
        { 0x60, 0xc0 }, { 0x68, 0xc8 }, { 0x70, 0x40 }, { 0x20, 0x00 }, { 0x90, 0x78 },
        { 0x98, 0x80 }, { 0xa8, 0x60 }, { 0xb0, 0x68 }, { 0xb8, 0xa8 }, { 0xc0, 0xa0 },
        { 0xc8, 0xb0 }, { 0xd0, 0x98 }, { 0xd8, 0x90 }, { 0xe0, 0x18 }, { 0xe8, 0x20 },
        { 0xa0, 0x88 },
    };
    uint8_t row[0x100], p[0x100];
    uint64_t key, variation = ~(uint64_t)0;
    memset(row, 0, sizeof row);
    memset(p, 0, sizeof p);
    if (!exists(db, car)) return 0;
    fetch(db, car, row, sizeof row);
    memcpy(&key, row, 8);
    if (key == ~(uint64_t)0 || !exists(db, key)) return 0;
    fetch(db, key, p, sizeof p);
    garage_clear(gar);
    set_key(gar, 0, car);
    for (size_t i = 0; i < sizeof parts / sizeof parts[0]; i++) {
        uint64_t k;
        memcpy(&k, p + parts[i].at, 8);
        set_key(gar, parts[i].slot, k);
    }
    set_key(gar, 8, ~(uint64_t)0);
    set_key(gar, 0xf0, ~(uint64_t)0);
    defaults(db, gar);
    /* func_004457B0: the car's first variation (CAR_VARIATION) */
    {
        uint8_t v[0x100];
        memset(v, 0, sizeof v);
        if (db->row(db->ctx, SPEC_CAR_VARIATION, (uint32_t)car, 0, v, sizeof v) > 0) memcpy(&variation, v, 8);
    }
    set_key(gar, 0x10, variation);
    if (variation == ~(uint64_t)0) return 0;
    W8(gar + 0x18, 0);
    memset(gt4_ptr(scratch), 0, 0x680);
    return auto_gear_ratios(db, gar, scratch) != 0;
}

/* func_00444690 +0x00..0x1ac: TUNED_CAR -> DEFAULT_PARTS. These are
   the event opponent's fitted parts, not the base car's stock parts. */
int gt4_garage_tuned(uint32_t gar, uint64_t key, const gt4_specdb_src *db, uint32_t scratch)
{
    uint8_t tuned[0x20]={0},parts[0x100]={0};uint64_t part_key,car,variation=UINT64_MAX;
    if(!exists(db,key) || fetch(db,key,tuned,sizeof tuned)<0x18)return 0;
    memcpy(&car,tuned,8);memcpy(&part_key,tuned+8,8);
    if(part_key==UINT64_MAX || !exists(db,part_key))return 0;
    fetch(db,part_key,parts,sizeof parts);garage_clear(gar);set_key(gar,8,key);set_key(gar,0,car);
    static const uint16_t map[][2]={
        {0x20,0},{0x28,8},{0x30,0x28},{0x38,0x58},{0x40,0x48},{0x48,0x50},
        {0x50,0x10},{0x58,0xb8},{0x60,0xc0},{0x68,0xc8},{0x70,0x40},{0x78,0x38},
        {0x80,0x30},{0x88,0x70},{0x90,0x78},{0x98,0x80},{0xa0,0x88},{0xa8,0x60},
        {0xb0,0x68},{0xb8,0xa8},{0xc0,0xa0},{0xc8,0xb0},{0xd0,0x98},{0xd8,0x90},
        {0xe0,0x18},{0xe8,0x20}};
    /* 00444724..004447FC: DEFAULT_PARTS occupies stack+0x20. */
    for(unsigned i=0;i<sizeof map/sizeof *map;i++){
        uint64_t part;memcpy(&part,parts+map[i][1],8);set_key(gar,map[i][0],part);
    }
    set_key(gar,0xf0,UINT64_MAX);defaults(db,gar);
    uint8_t v[0x100]={0};
    if(db->row(db->ctx,SPEC_CAR_VARIATION,(uint32_t)car,0,v,sizeof v)>0)memcpy(&variation,v,8);
    set_key(gar,0x10,variation);if(variation==UINT64_MAX)return 0;W8(gar+0x18,0);
    uint64_t settings;memcpy(&settings,tuned+0x10,8);
    if(settings!=UINT64_MAX){
        /* func_00444860 +0x38..0x138 applies the decoded SETTING row.
           It returns failure without writes when the settings key misses. */
        uint8_t r[0x30]={0};if(fetch(db,settings,r,sizeof r)>0){
            static const uint16_t bytes[][2]={
                {0x12a,0x14},{0x150,0xe},{0x152,0xf},{0x154,0x10},{0x151,0x11},
                {0x153,0x12},{0x155,0x13},{0x131,0xc},{0x132,0xd},
                {0x146,0x19},{0x147,0x1a},{0x148,0x1b},{0x149,0x1c},
                {0x14a,0x1d},{0x14b,0x1e},{0x14c,0x1f},{0x14d,0x20},
                {0x139,0x15},{0x13a,0x16},{0x140,0x17},{0x141,0x18},
                {0x14e,0x21},{0x14f,0x22},{0x142,8},{0x143,0xa},{0x156,0x24},{0x157,0x23}};
            W16(gar+0x128,RU16(r,2));W16(gar+0x13c,RU16(r,4));W16(gar+0x13e,RU16(r,6));
            for(unsigned i=0;i<sizeof bytes/sizeof *bytes;i++)W8(gar+bytes[i][0],r[bytes[i][1]]);
            W16(gar+0x15e,RU16(r,0));
        }
    }
    memset(gt4_ptr(scratch),0,0x680);return auto_gear_ratios(db,gar,scratch)!=0;
}

void gt4_garage_event_tyres(uint32_t gar,int type,const gt4_specdb_src *db)
{
    /* 003EE3C8 +0x38..0x64 maps dirt/snow to levels 11/12. Other
       event types retain the player's and opponents' fitted tyres. */
    int level=type==2?11:type==3?12:-1;
    if(level<0)return;
    int front=gt4_tyre_byte(db,gar,0x60,0x3a),rear=gt4_tyre_byte(db,gar,0x68,0x38);
    if(front==level && rear==level)return;
    size_t size=0;
    uint8_t *parts=db->car_parts?db->car_parts(db->ctx,key_at(gar,0),&size):NULL;
    if(!parts || size<8){free(parts);return;}
    int32_t count;memcpy(&count,parts,4);
    /* 003EE3C8 +0xf0..0x128 / 0044A048: fit available axles
       independently; missing parts leave that axle unchanged. */
    for(unsigned table=0x19;table<=0x1a;table++){
        for(int32_t i=0;i<count && 8+(size_t)i*16+16<=size;i++){
            const uint8_t *row=parts+8+(size_t)i*16;uint64_t key;
            memcpy(&key,row,8);
            if((uint32_t)(key>>32)==table && row[8]==level){
                set_key(gar,table==0x19?0x60:0x68,key);break;
            }
            /* Sorted namespaces: same unsigned early-out as 0044A048. */
            if((uint64_t)table<(uint64_t)(int64_t)(int32_t)(key>>32))break;
        }
    }
    free(parts);
}

/* The Arcade world's tyres (00387348, its vtable +0x2E0, run by 00387FB0):
   a dirt or snow course fits kinds 11/12; otherwise a QuickTune (+0x165)
   asks for the kinds at +0x160/+0x161, or the fitted ones stay. A change
   or the QuickTune clears +0x165 and fits each axle's part of that kind
   from the car's parts list (0044A048 / 004468F8); a missing part leaves
   that axle. */
void gt4_garage_world_tyres(uint32_t gar,int type,const gt4_specdb_src *db)
{
    int forced=type==2?11:type==3?12:-1,quick=(int8_t)GT4_U8(gar+0x165)!=0;
    int front=quick?GT4_U8(gar+0x160):gt4_tyre_byte(db,gar,0x60,0x3a);
    int rear=quick?GT4_U8(gar+0x161):gt4_tyre_byte(db,gar,0x68,0x38);
    int want[2]={forced!=-1?forced:front,forced!=-1?forced:rear};
    if(front==want[0] && rear==want[1] && !quick)return;
    GT4_U8(gar+0x165)=0;
    size_t size=0;
    uint8_t *parts=db->car_parts?db->car_parts(db->ctx,key_at(gar,0),&size):NULL;
    if(!parts || size<8){free(parts);return;}
    int32_t count;memcpy(&count,parts,4);
    for(unsigned table=0x19;table<=0x1a;table++){
        for(int32_t i=0;i<count && 8+(size_t)i*16+16<=size;i++){
            const uint8_t *row=parts+8+(size_t)i*16;uint64_t key;
            memcpy(&key,row,8);
            if((uint32_t)(key>>32)==table && (int)row[8]==want[table-0x19]){
                set_key(gar,table==0x19?0x60:0x68,key);break;
            }
            if((uint64_t)table<(uint64_t)(int64_t)(int32_t)(key>>32))break;
        }
    }
    free(parts);
}

int gt4_garage_paint_palette(uint32_t gar,const gt4_specdb_src *db,int random,uint32_t rgb[32])
{
    uint64_t key=key_at(gar,0x10);uint8_t row[52],generic[24];unsigned count=0;
    /* 00445808 -> 00443FB8 counts matching VARIATION keys. */
    while(db->row(db->ctx,(uint32_t)(key>>32),(uint32_t)key,count,row,sizeof row)>0)count++;
    if(count>32 || !count)return 0; /* original selector's 0x304-byte group */
    unsigned available=count;
    if(random){
        if(fetch(db,key_at(gar,0),generic,sizeof generic)!=24)return 0;
        /* 00445828 +0x28..0x38 / 004489A0 +0x00: omit last special paint. */
        available-=(generic[0x12]>>1)&1u;
    }
    memset(rgb,0,32*sizeof *rgb);
    for(unsigned index=0;index<available;index++){
        for(unsigned n=0;n<count;n++){
            if(db->row(db->ctx,(uint32_t)(key>>32),(uint32_t)key,n,row,sizeof row)!=52)return 0;
            uint32_t order;memcpy(&order,row+4,4);
            /* 004458D0 +0x8c..0xa4 finds order=index+1. 00445AB8
               +0x14..0x20 reads the first chip and masks its alpha. */
            if(order==index+1)break;
        }
        /* 00445AB8 does not check 004458D0's return: a missing order
           retains the last row read at +0x24. Disc car 619 has a gap. */
        memcpy(&rgb[index],row+0x24,4);rgb[index]&=0xffffffu;
    }
    return (int)available;
}
