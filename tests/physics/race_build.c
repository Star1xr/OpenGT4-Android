/*
 * Transitional: a race in guest memory without a PCSX2 snapshot.
 *
 * The executable's image (code and data, bss zero) is loaded, and the few
 * objects the race setup and the step read before building the rest
 * themselves are made: the conductor (the Time Trial session), the race
 * with its two entries and their driver profiles, the "world" object, the
 * course object (its runway and lines come later from the disc,
 * tests/physics/race_setup.c), the header of car 0, and two globals (the
 * display units and the random generator's state).
 *
 * Which words these are was found by building RAMs from the image plus
 * parts of a snapshot and comparing the race set up and stepped on them
 * with the same on the whole snapshot (docs/analysis/PHYSICS.md, "Out of
 * the snapshot"). The objects sit at the addresses they have in the Time
 * Trial snapshots, so both can be compared word for word.
 */
#include "race_build.h"
#include "phys.h"
#include "inflate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COND        0x01bf7860u   /* the conductor: the race session */
#define RACE        0x01be11d0u
#define ENTRIES     0x00ac4510u   /* the race's entry list object */
#define ENTRY0      0x01be1b60u   /* the garage entries (race_setup builds entry 0's car) */
#define ENTRY1      0x01be5140u
#define WORLD       0x01be8720u   /* race+0x84 */
#define PROFILE0    0x01bf6bacu   /* the entries' driver profiles (also car 0's controller) */
#define PROFILE1    0x01bf6d8cu
#define COURSE      0x00bcd180u   /* race+0x80 */
#define COURSE_DATA 0x00d34b00u   /* course+4: the runway, and the lines 2 MB on */

#define TIME_TRIAL_VT 0x00685890u /* the Time Trial session's virtual table */
#define NO_TIME       0x157529ffu /* "no time" in the race's records */

static void w32(uint8_t *ram, uint32_t a, uint32_t v) { memcpy(ram + a, &v, 4); }

/* The ELF's loadable segments into guest memory */
static int load_elf(uint8_t *ram, const char *path, char *msg, size_t len)
{
    FILE *f = fopen(path, "rb");
    uint8_t *elf;
    long n;
    uint32_t phoff;
    uint16_t count;
    if (!f) {
        snprintf(msg, len, "cannot open %s", path);
        return 0;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    elf = malloc((size_t)n);
    if (!elf || fread(elf, 1, (size_t)n, f) != (size_t)n || n < 0x34 || memcmp(elf, "\x7f" "ELF", 4)) {
        snprintf(msg, len, "%s is not an ELF", path);
        fclose(f);
        free(elf);
        return 0;
    }
    fclose(f);
    memcpy(&phoff, elf + 0x1c, 4);
    memcpy(&count, elf + 0x2c, 2);
    for (uint32_t i = 0; i < count; i++) {
        uint32_t ph[8];
        if (phoff + i * 32 + 32 > (uint32_t)n) break;
        memcpy(ph, elf + phoff + i * 32, 32);
        /* type, offset, vaddr, paddr, file size, memory size */
        if (ph[0] != 1) continue;
        if (ph[1] + ph[4] > (uint32_t)n || ph[2] + ph[5] > EE_RAM_SIZE) {
            snprintf(msg, len, "%s: segment out of range", path);
            free(elf);
            return 0;
        }
        memcpy(ram + ph[2], elf + ph[1], ph[4]);
        memset(ram + ph[2] + ph[4], 0, ph[5] - ph[4]);
    }
    free(elf);
    return 1;
}

/* The main executable straight from the user's disc: CORE.GT4 in the ISO's
   root, raw deflate after a 6-byte header (tag 01 01, the size), then the
   load records {vaddr, size, data} from 0x12C (docs/analysis/CORE_FORMAT.md,
   tools/unpack_core.py); bss is zero up to 0x008A215C. */
#define CORE_MAGIC   0x176265b3u
#define CORE_RECORDS 0x12cu
#define CORE_END     0x008a215cu

static uint32_t le32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

static uint8_t *iso_read(FILE *f, uint32_t lba, uint32_t size)
{
    uint8_t *b = malloc(size ? size : 1);
    if (!b) return NULL;
    if (fseek(f, (long)lba * 2048L, SEEK_SET) != 0 || fread(b, 1, size, f) != size) {
        free(b);
        return NULL;
    }
    return b;
}

/* A file of the ISO9660 root directory, by name (without ";1") */
static uint8_t *iso_root_file(const char *iso, const char *name, uint32_t *size)
{
    FILE *f = fopen(iso, "rb");
    uint8_t *pvd, *dir, *out = NULL;
    uint32_t dlba, dsize;
    size_t nlen = strlen(name);
    if (!f) return NULL;
    pvd = iso_read(f, 16, 2048);
    if (!pvd || pvd[0] != 1 || memcmp(pvd + 1, "CD001", 5)) goto done;
    dlba = le32(pvd + 156 + 2);
    dsize = le32(pvd + 156 + 10);
    dir = iso_read(f, dlba, dsize);
    if (!dir) goto done;
    for (uint32_t at = 0; at < dsize;) {
        uint8_t rl = dir[at];
        if (!rl) {                                   /* records do not cross sectors */
            at = (at / 2048 + 1) * 2048;
            continue;
        }
        if (at + 33 <= dsize) {
            uint8_t il = dir[at + 32];
            const char *id = (const char *)dir + at + 33;
            if (il >= nlen && !memcmp(id, name, nlen) && (il == nlen || id[nlen] == ';')) {
                *size = le32(dir + at + 10);
                out = iso_read(f, le32(dir + at + 2), *size);
                break;
            }
        }
        at += rl;
    }
    free(dir);
done:
    free(pvd);
    fclose(f);
    return out;
}

static int load_disc(uint8_t *ram, const char *iso, char *msg, size_t len)
{
    uint32_t n = 0, size;
    uint8_t *raw = iso_root_file(iso, "CORE.GT4", &n), *img;
    if (!raw) {
        snprintf(msg, len, "%s: no CORE.GT4", iso);
        return 0;
    }
    if (n < 6 || raw[0] != 1 || raw[1] != 1) {
        snprintf(msg, len, "CORE.GT4: unknown format");
        free(raw);
        return 0;
    }
    size = le32(raw + 2);
    img = malloc(size ? size : 1);
    if (!img || core_inflate_raw(raw + 6, n - 6, img, size) != size || size < CORE_RECORDS || le32(img + 0x100) != CORE_MAGIC) {
        snprintf(msg, len, "CORE.GT4: cannot unpack (other disc?)");
        free(raw);
        free(img);
        return 0;
    }
    free(raw);
    for (uint32_t at = CORE_RECORDS; at < size;) {
        uint32_t va, sz;
        if (at + 8 > size) break;
        va = le32(img + at);
        sz = le32(img + at + 4);
        at += 8;
        if (at + sz > size || va + sz > EE_RAM_SIZE) {
            snprintf(msg, len, "CORE.GT4: record out of range");
            free(img);
            return 0;
        }
        memcpy(ram + va, img + at, sz);
        at += sz;
    }
    free(img);
    return 1;
}

/* The executable: an ELF (extracted/core.elf), or the disc itself */
static int load_image(uint8_t *ram, const char *elf, char *msg, size_t len)
{
    FILE *f = fopen(elf, "rb");
    uint8_t head[4] = { 0 };
    int is_elf = f && fread(head, 1, 4, f) == 4 && !memcmp(head, "\x7f" "ELF", 4);
    msg[0] = 0;
    memset(ram, 0, EE_RAM_SIZE);
    if (f) fclose(f);
    return is_elf ? load_elf(ram, elf, msg, len) : load_disc(ram, elf, msg, len);
}

int phys_race_build(uint8_t *ram, const char *elf, uint32_t *conductor, char *msg, size_t len)
{
    if (!load_image(ram, elf, msg, len)) return 0;

    /* globals: the display units (the options' km/h, litres...), the
       random generator's state (func_00579xxx, as the snapshots have it) */
    w32(ram, 0x006244d8u, 1);
    w32(ram, 0x006244dcu, 1);
    w32(ram, 0x006244e0u, 1);
    w32(ram, 0x006244e4u, 0);
    w32(ram, 0x006244e8u, 1);
    w32(ram, 0x00874d50u, 0xcf041fa0u);
    w32(ram, 0x00874d54u, 0xe4859d6du);
    w32(ram, 0x00874d58u, 0xed953565u);

    /* the race */
    w32(ram, RACE + 0x60, ENTRIES);
    w32(ram, RACE + 0x78, 0x0000000au);        /* the COURSE row's key (table 0x22) */
    w32(ram, RACE + 0x7c, 0x00000022u);
    w32(ram, RACE + 0x80, COURSE);
    w32(ram, RACE + 0x84, WORLD);
    w32(ram, RACE + 0x88, 0xfffffffeu);        /* the race's records: none (func_003C12B0) */
    w32(ram, RACE + 0x8c, NO_TIME);

    /* the entry list: a growing array (count, capacity, items), with the
       allocator's header before it */
    w32(ram, ENTRIES - 0x10, 0x00abbbf0u);
    w32(ram, ENTRIES - 0x0c, 0x00ac4570u);
    w32(ram, ENTRIES - 0x08, 0xffffffffu);
    w32(ram, ENTRIES - 0x04, 0xffffffffu);
    w32(ram, ENTRIES + 0x00, 2);
    w32(ram, ENTRIES + 0x04, 6);
    w32(ram, ENTRIES + 0x08, ENTRIES + 0x24);
    w32(ram, ENTRIES + 0x14, ENTRY0);
    w32(ram, ENTRIES + 0x18, ENTRY1);
    w32(ram, ENTRIES + 0x1c, 0x01d39470u);
    w32(ram, ENTRIES + 0x20, 0x006839c8u);
    w32(ram, ENTRIES + 0x24, ENTRY0);
    w32(ram, ENTRIES + 0x28, ENTRY1);

    /* the entries: driven by a player (+0x3438), their profiles */
    w32(ram, ENTRY0 + 0x3438, 1);
    w32(ram, ENTRY0 + 0x343c, PROFILE0);
    w32(ram, ENTRY0 + 0x347c, 1);
    w32(ram, ENTRY1 + 0x3438, 1);
    w32(ram, ENTRY1 + 0x343c, PROFILE1);
    w32(ram, ENTRY1 + 0x3440, 0xffffffffu);

    /* the profiles (+0x1CC: the car is not held at the start) */
    w32(ram, PROFILE0 + 0xc4, PROFILE0 + 0x18);
    w32(ram, PROFILE0 + 0xc8, PROFILE0 + 0x52);
    w32(ram, PROFILE0 + 0xcc, 0x00a46a9cu);
    w32(ram, PROFILE0 + 0xd0, 0x00686618u);
    w32(ram, PROFILE0 + 0x1cc, 1);
    w32(ram, PROFILE1 + 0xd0, 0x00686618u);
    w32(ram, PROFILE1 + 0xd4, 1);
    w32(ram, PROFILE1 + 0xd8, 2);
    w32(ram, PROFILE1 + 0xdc, 0x01bb21acu);
    w32(ram, PROFILE1 + 0xe0, 0x01bb21acu);

    /* the race's settings (race+0x70: a standing start of ten seconds) */
    w32(ram, RACE + 0x70, 0x00bcd010u);
    ram[0x00bcd010u + 0x40] = 2;

    /* the world object (+0x70: the conductor, which the race's events find
       through it) */
    w32(ram, WORLD + 0x70, COND);
    w32(ram, WORLD + 0x58, 0x01bf6b6cu);
    w32(ram, WORLD + 0x5c, 1);
    w32(ram, WORLD + 0x60, 1);
    w32(ram, WORLD + 0x64, 0x00680140u);

    /* the course: its data (runway, lines) at course+4, no variant of the
       checkpoint set (+0xB0) */
    w32(ram, COURSE + 0x04, COURSE_DATA);
    w32(ram, COURSE + 0xb0, 0xffffffffu);

    /* the conductor */
    w32(ram, COND, RACE);
    w32(ram, COND + 0x10140, TIME_TRIAL_VT);
    w32(ram, COND + 0xf818, 0x00000200u);      /* +0xF819: 2 */
    for (uint32_t k = 0xf81c; k < 0xf830; k += 4) w32(ram, COND + k, 0x64646464u);   /* per car: 100 */
    w32(ram, COND + 0xf830, 0x14141414u);                                           /* per car: 20 */
    w32(ram, COND + 0xf898, 0x0000f64au);      /* the race clocks */
    w32(ram, COND + 0xf89c, 0x0000f64au);
    w32(ram, COND + 0xf8a0, 2);                /* cars */
    w32(ram, COND + 0xf8a8, 1);                /* the race order (car 0 first) */
    for (uint32_t k = 0xf8ac; k < 0xf8bc; k += 4) w32(ram, COND + k, 0xffffffffu);

    /* car 0's header: its race, session, course, controller, parameters */
    w32(ram, COND + 0x6698, RACE);
    w32(ram, COND + 0x669c, COND);
    w32(ram, COND + 0x66a0, COURSE);
    w32(ram, COND + 0x66a4, PROFILE0);
    w32(ram, COND + 0x66a8, COND + 0x98);

    *conductor = COND;
    return 1;
}

/* A race against the AI (the Arcade's session, table 0x00679CE8), at the
   addresses of the Arcade captures (ar_*): the words found the same way,
   40 of them (the objects the setup reads before building the rest). The
   player's car is entry 0 at the back of the grid (slot n-1), the others
   in slots 0..n-2, with the AI's shared driver profile (0x00622B60, its
   virtual table set as the game's constructor would). */
#define A_COND      0x01cf8ed0u
#define A_RACE      0x01c3f620u
#define A_ENTRIES   0x00a47cb0u
#define A_WORLD     0x01ce6910u
#define A_PROFILE   0x01cf8cdcu   /* the player's */
#define A_AI        0x00622b60u   /* the AI's, in the executable's data */
#define A_COURSE    0x00bcd180u
#define A_DATA      0x00ccd600u   /* course+4: the runway, the lines after it */

#define RACE_VT        0x00679ce8u
#define ARCADE_WORLD   0x0067bad8u   /* RaceArcadeSingle's table (world+0x64) */
#define PROFILE_VT     0x00686618u

int phys_race_build_arcade(uint8_t *ram, const char *elf, int cars, uint32_t *conductor, char *msg, size_t len)
{
    static const uint32_t entry[6] = { 0x01bd7710u, 0x01bdacf0u, 0x01cd9190u, 0x01cdc770u, 0x01cdfd50u, 0x01ce3330u };
    if (cars < 1 || cars > 6) {
        snprintf(msg, len, "an Arcade race has 1 to 6 cars");
        return 0;
    }
    if (!load_image(ram, elf, msg, len)) return 0;

    w32(ram, 0x006244e8u, 1);                  /* the display units */
    w32(ram, 0x00874d50u, 0xba8350bdu);        /* the random generator's state */
    w32(ram, 0x00a46a94u, 0x00687ff0u);        /* the controller's object: its table */

    w32(ram, A_RACE, 2);                       /* the laps, as in the captures */
    w32(ram, A_RACE + 0x60, A_ENTRIES);
    w32(ram, A_RACE + 0x78, 0x0000000au);      /* the COURSE row's key */
    w32(ram, A_RACE + 0x80, A_COURSE);
    w32(ram, A_RACE + 0x84, A_WORLD);
    w32(ram, A_WORLD + 0x64, ARCADE_WORLD);
    w32(ram, A_WORLD + 0x70, A_COND);
    w32(ram, A_RACE + 0x70, 0x00bcd010u);      /* the race's settings: a standing start of ten seconds */
    ram[0x00bcd010u + 0x40] = 2;
    w32(ram, A_COURSE + 0x04, A_DATA);

    w32(ram, A_ENTRIES + 0x00, (uint32_t)cars);
    w32(ram, A_ENTRIES + 0x08, A_ENTRIES + 0x24);
    for (int i = 0; i < cars; i++) {
        w32(ram, A_ENTRIES + 0x24 + (uint32_t)i * 4, entry[i]);
        w32(ram, entry[i] + 0x343c, i ? A_AI : A_PROFILE);
        w32(ram, entry[i] + 0x3444, (uint32_t)(i ? i - 1 : cars - 1));   /* the grid slot */
        w32(ram, entry[i] + 0x347c, 1);
    }
    w32(ram, A_PROFILE + 0xd0, PROFILE_VT);
    w32(ram, A_PROFILE + 0x1cc, 1);            /* the car is not held at the start */
    w32(ram, A_AI + 0xd0, PROFILE_VT);

    w32(ram, A_COND, A_RACE);
    w32(ram, A_COND + 0x10140, RACE_VT);
    w32(ram, A_COND + 0xf864, 0x50);
    w32(ram, A_COND + 0xf8a0, (uint32_t)cars);
    *conductor = A_COND;
    return 1;
}
