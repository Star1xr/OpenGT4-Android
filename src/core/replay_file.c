#include "replay_file.h"
#include "save.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

/* The record: "GT4R", its version, then the fields in order, little-endian;
   strings with their length; the commands against the step before (from
   zeros): how many steps repeat it, then which of the next one's 32 bytes
   changed and those bytes, counts and masks as 7-bit groups. A newer
   version is refused, not guessed. */
#define VERSION 1
#define LIMIT_STEPS (60u * 60u * 60u * 25u)   /* 25 hours of 60 Hz steps (the 24 Hours) */

typedef struct { uint8_t *p; size_t n, cap; int bad; } out_buf;
static void put(out_buf *o, const void *v, size_t n)
{
    if (o->bad) return;
    if (o->n + n > o->cap) {
        size_t cap = o->cap ? o->cap * 2 : 4096;
        while (cap < o->n + n) cap *= 2;
        uint8_t *q = realloc(o->p, cap);
        if (!q) { o->bad = 1; return; }
        o->p = q;
        o->cap = cap;
    }
    memcpy(o->p + o->n, v, n);
    o->n += n;
}
static void put32(out_buf *o, uint32_t v) { uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) }; put(o, b, 4); }
static void put64(out_buf *o, uint64_t v) { put32(o, (uint32_t)v); put32(o, (uint32_t)(v >> 32)); }
static void putv(out_buf *o, uint32_t v)
{
    do {
        uint8_t b = (uint8_t)(v & 0x7f);
        v >>= 7;
        if (v) b |= 0x80;
        put(o, &b, 1);
    } while (v);
}
static void puts_(out_buf *o, const char *s) { size_t n = strlen(s); put32(o, (uint32_t)n); put(o, s, n); }

typedef struct { const uint8_t *p; size_t n, at; int bad; } in_buf;
static void get(in_buf *i, void *v, size_t n)
{
    if (i->bad || n > i->n - i->at) { i->bad = 1; memset(v, 0, n); return; }
    memcpy(v, i->p + i->at, n);
    i->at += n;
}
static uint32_t get32(in_buf *i) { uint8_t b[4]; get(i, b, 4); return b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }
static uint64_t get64(in_buf *i) { uint64_t lo = get32(i); return lo | (uint64_t)get32(i) << 32; }
static int32_t geti(in_buf *i) { return (int32_t)get32(i); }
static uint32_t getv(in_buf *i)
{
    uint32_t v = 0;
    for (int shift = 0; shift < 35; shift += 7) {
        uint8_t b;
        get(i, &b, 1);
        if (i->bad) return 0;
        v |= (uint32_t)(b & 0x7f) << shift;
        if (!(b & 0x80)) return v;
    }
    i->bad = 1;
    return 0;
}
static void gets_(in_buf *i, char *s, size_t size)
{
    uint32_t n = get32(i);
    if (n >= size) { i->bad = 1; s[0] = 0; return; }
    get(i, s, n);
    s[n] = 0;
}

int gt4_replay_pack(const gt4_replay_file *r, uint8_t **out, size_t *size)
{
    out_buf o = { 0 };
    *out = NULL;
    *size = 0;
    if (!r || r->disc_info || !r->tape || !r->steps || r->steps > LIMIT_STEPS) return 0;
    put(&o, "GT4R", 4);
    put32(&o, VERSION);
    put32(&o, (uint32_t)r->kind);
    puts_(&o, r->course);
    puts_(&o, r->car);
    put32(&o, (uint32_t)r->color);
    put32(&o, (uint32_t)r->nothers);
    put32(&o, (uint32_t)r->has_colors);
    for (int k = 0; k < 5; k++) { puts_(&o, r->others[k]); put32(&o, (uint32_t)r->colors[k]); }
    put32(&o, (uint32_t)r->laps);
    put32(&o, (uint32_t)r->tire);
    put32(&o, (uint32_t)r->manual);
    put32(&o, (uint32_t)r->assist_asm);
    put32(&o, (uint32_t)r->assist_tcs);
    put32(&o, (uint32_t)r->time_trial);
    put32(&o, (uint32_t)r->prepared);
    if (r->prepared) {
        put32(&o, (uint32_t)r->championship);
        put32(&o, (uint32_t)r->cars);
        put32(&o, r->rng);
        for (int k = 0; k < 6; k++) { put32(&o, r->car_id[k]); put64(&o, r->tuned_car[k]); put64(&o, r->event_paint[k]); }
        put(&o, r->event_row, sizeof r->event_row);
        for (int k = 0; k < 6; k++) {
            put32(&o, (uint32_t)r->owned_on[k]);
            if (r->owned_on[k]) put(&o, r->owned[k], sizeof r->owned[k]);
        }
    }
    puts_(&o, r->player);
    puts_(&o, r->course_name);
    puts_(&o, r->car_name);
    put32(&o, r->race_time);
    put32(&o, r->best_lap);
    put32(&o, (uint32_t)r->steps);
    {
        static const uint8_t zero[0x20];
        const uint8_t *prev = zero;
        for (size_t at = 0; at < r->steps;) {
            uint32_t run = 0, mask = 0;
            const uint8_t *s;
            while (at < r->steps && !memcmp(r->tape + at * 0x20, prev, 0x20)) { run++; at++; }
            putv(&o, run);
            if (at == r->steps) break;
            s = r->tape + at * 0x20;
            for (int k = 0; k < 0x20; k++) if (s[k] != prev[k]) mask |= 1u << k;
            putv(&o, mask);
            for (int k = 0; k < 0x20; k++) if (mask >> k & 1) put(&o, s + k, 1);
            prev = s;
            at++;
        }
    }
    if (r->thumb) {
        put(&o, "THMB", 4);
        put(&o, r->thumb, GT4_REPLAY_THUMB_W * GT4_REPLAY_THUMB_H * 3);
    }
    if (r->ghost_car) {   /* a best lap: where the ghost starts from */
        put(&o, "GHST", 4);
        put32(&o, r->ghost_rest);
        {
            uint32_t f;
            memcpy(&f, &r->ghost_frac, 4);
            put32(&o, f);
        }
        put(&o, r->ghost_car, GT4_REPLAY_GHOST_CAR);
    }
    if (r->title[0]) {   /* the player's name for it */
        size_t n = strnlen(r->title, sizeof r->title - 1);
        put(&o, "TITL", 4);
        putv(&o, (uint32_t)n);
        put(&o, r->title, n);
    }
    if(r->split_battle){
        if(r->kind!=GT4_REPLAY_FULL || r->nothers!=1 || !r->tape_2p || !r->steps_2p || r->steps_2p>LIMIT_STEPS){free(o.p);return 0;}
        /* P: optional PC extension; old single-player encodings stay valid. */
        put(&o,"SP2R",4);put32(&o,(uint32_t)r->steps_2p);
        put32(&o,r->tire_2p);put32(&o,r->manual_2p);put32(&o,r->assist_asm_2p);put32(&o,r->assist_tcs_2p);
        put32(&o,r->split_boost);put32(&o,r->split_handicap);put32(&o,r->split_tire_damage);
        for(int k=0;k<2;k++){put32(&o,r->owned_on[k]);if(r->owned_on[k])put(&o,r->owned[k],GT4_REPLAY_OWNED);}
        put(&o,r->tape_2p,r->steps_2p*32);
    }
    if(r->world_mode){put(&o,"WMOD",4);put32(&o,(uint32_t)r->world_mode);}   /* P: optional, older files read as before */
    if (o.bad || o.n > GT4_SAVE_LIMIT) { free(o.p); return 0; }
    *out = o.p;
    *size = o.n;
    return 1;
}

int gt4_replay_unpack(const uint8_t *data, size_t size, gt4_replay_file *r)
{
    in_buf i = { data, size, 0, 0 };
    char magic[4];
    uint32_t steps;
    memset(r, 0, sizeof *r);
    get(&i, magic, 4);
    if (i.bad || memcmp(magic, "GT4R", 4) || get32(&i) != VERSION) return 0;
    r->kind = geti(&i);
    gets_(&i, r->course, sizeof r->course);
    gets_(&i, r->car, sizeof r->car);
    r->color = geti(&i);
    r->nothers = geti(&i);
    r->has_colors = geti(&i);
    for (int k = 0; k < 5; k++) { gets_(&i, r->others[k], sizeof r->others[k]); r->colors[k] = geti(&i); }
    r->laps = geti(&i);
    r->tire = geti(&i);
    r->manual = geti(&i);
    r->assist_asm = geti(&i);
    r->assist_tcs = geti(&i);
    r->time_trial = geti(&i);
    r->prepared = geti(&i);
    if (r->prepared) {
        r->championship = geti(&i);
        r->cars = geti(&i);
        r->rng = get32(&i);
        for (int k = 0; k < 6; k++) { r->car_id[k] = get32(&i); r->tuned_car[k] = get64(&i); r->event_paint[k] = get64(&i); }
        get(&i, r->event_row, sizeof r->event_row);
        for (int k = 0; k < 6; k++) {
            r->owned_on[k] = geti(&i);
            if (r->owned_on[k]) get(&i, r->owned[k], sizeof r->owned[k]);
        }
    }
    gets_(&i, r->player, sizeof r->player);
    gets_(&i, r->course_name, sizeof r->course_name);
    gets_(&i, r->car_name, sizeof r->car_name);
    r->race_time = get32(&i);
    r->best_lap = get32(&i);
    steps = get32(&i);
    if (i.bad || r->nothers < 0 || r->nothers > 5 || (r->prepared && (r->cars < 1 || r->cars > 6))
        || !r->course[0] || !steps || steps > LIMIT_STEPS) return 0;
    r->tape = malloc((size_t)steps * 0x20);
    if (!r->tape) return 0;
    {
        uint8_t prev[0x20] = { 0 };
        for (size_t at = 0; at < steps;) {
            uint32_t run = getv(&i), mask;
            if (i.bad || run > steps - at) { gt4_replay_free(r); return 0; }
            for (uint32_t k = 0; k < run; k++) memcpy(r->tape + (at + k) * 0x20, prev, 0x20);
            at += run;
            if (at == steps) break;
            mask = getv(&i);
            if (i.bad || !mask) { gt4_replay_free(r); return 0; }
            for (int k = 0; k < 0x20; k++) if (mask >> k & 1) get(&i, prev + k, 1);
            memcpy(r->tape + at * 0x20, prev, 0x20);
            at++;
        }
    }
    r->steps = steps;
    while (!i.bad && i.at != i.n) {   /* the picture, the ghost's start and the name, when kept */
        char tag[4];
        get(&i, tag, 4);
        if (!i.bad && !memcmp(tag, "THMB", 4) && !r->thumb && (r->thumb = malloc(GT4_REPLAY_THUMB_W * GT4_REPLAY_THUMB_H * 3)))
            get(&i, r->thumb, GT4_REPLAY_THUMB_W * GT4_REPLAY_THUMB_H * 3);
        else if (!i.bad && !memcmp(tag, "GHST", 4) && !r->ghost_car && (r->ghost_car = malloc(GT4_REPLAY_GHOST_CAR))) {
            uint32_t f;
            r->ghost_rest = get32(&i);
            f = get32(&i);
            memcpy(&r->ghost_frac, &f, 4);
            get(&i, r->ghost_car, GT4_REPLAY_GHOST_CAR);
        } else if (!i.bad && !memcmp(tag, "TITL", 4)) {
            uint32_t n = getv(&i);
            if (n >= sizeof r->title) i.bad = 1;
            else { get(&i, r->title, n); r->title[n] = 0; }
        } else if(!i.bad && !memcmp(tag,"SP2R",4) && !r->split_battle){
            r->split_battle=1;r->steps_2p=get32(&i);
            r->tire_2p=geti(&i);r->manual_2p=geti(&i);r->assist_asm_2p=geti(&i);r->assist_tcs_2p=geti(&i);
            r->split_boost=geti(&i);r->split_handicap=geti(&i);r->split_tire_damage=geti(&i);
            for(int k=0;k<2;k++){r->owned_on[k]=geti(&i);if(r->owned_on[k])get(&i,r->owned[k],GT4_REPLAY_OWNED);}
            if(i.bad || r->kind!=GT4_REPLAY_FULL || r->nothers!=1 || !r->steps_2p || r->steps_2p>LIMIT_STEPS || r->steps_2p>(i.n-i.at)/32)i.bad=1;
            else if(!(r->tape_2p=malloc(r->steps_2p*32)))i.bad=1;
            else get(&i,r->tape_2p,r->steps_2p*32);
        } else if(!i.bad && !memcmp(tag,"WMOD",4) && !r->world_mode){
            r->world_mode=geti(&i);
            if(r->world_mode!=11 && r->world_mode!=12)i.bad=1;
        } else i.bad = 1;
    }
    if (i.bad || i.at != i.n) { gt4_replay_free(r); return 0; }
    return 1;
}

void gt4_replay_free(gt4_replay_file *r)
{
    if (r) { free(r->tape_2p);r->tape_2p=NULL;r->steps_2p=0;free(r->tape); r->tape = NULL; r->steps = 0; free(r->thumb); r->thumb = NULL; free(r->ghost_car); r->ghost_car = NULL; free(r->disc_info); r->disc_info = NULL; free(r->disc_icon); r->disc_icon = NULL; }
}

/* ------------------------------------------------------------ the files */

static int folder(char *p, size_t n)
{
    const char *dir = gt4_save_directory();
    if (!dir || !dir[0] || snprintf(p, n, "%s/replays", dir) >= (int)n) return 0;
    return gt4_save_mkdir(p);
}

static int valid_name(const char *name)
{
    size_t n = name ? strlen(name) : 0;
    if (n < 6 || n >= 64 || strcmp(name + n - 5, ".ogt4")) return 0;
    for (size_t k = 0; k < n; k++)
        if (!((name[k] >= 'a' && name[k] <= 'z') || (name[k] >= '0' && name[k] <= '9') || name[k] == '_' || name[k] == '.')) return 0;
    return 1;
}

static int exists(const char *path)
{
    uint8_t *data;
    size_t size;
    int status = gt4_save_read(path, &data, &size, NULL);
    if (status > 0) free(data);
    return status != 0;
}

int gt4_replay_store(const gt4_replay_file *r, char *name, size_t name_size)
{
    char dir[1100], path[1200], base[64];
    uint8_t *data;
    size_t size;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    int ok;
    if (!folder(dir, sizeof dir) || !gt4_replay_pack(r, &data, &size)) return 0;
    for (int k = 0;; k++) {
        if (k > 99) { free(data); return 0; }
        snprintf(base, sizeof base, "%s_%04d%02d%02d_%02d%02d%02d%s%.0d.ogt4", r->kind == GT4_REPLAY_BEST_LAP ? "bestlap" : "replay",
                 t ? t->tm_year + 1900 : 0, t ? t->tm_mon + 1 : 0, t ? t->tm_mday : 0, t ? t->tm_hour : 0, t ? t->tm_min : 0, t ? t->tm_sec : 0,
                 k ? "_" : "", k);
        snprintf(path, sizeof path, "%s/%s", dir, base);
        if (!exists(path)) break;
    }
    ok = gt4_save_write(path, data, size);
    free(data);
    if (ok && name) snprintf(name, name_size, "%s", base);
    return ok;
}

int gt4_replay_store_over(const gt4_replay_file *r, const char *name)
{
    char dir[1100], path[1200];
    uint8_t *data;
    size_t size;
    int ok;
    if (!name || !*name || strchr(name, '/') || strchr(name, '\\') || strstr(name, "..")) return 0;
    if (!folder(dir, sizeof dir) || !gt4_replay_pack(r, &data, &size)) return 0;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    ok = gt4_save_write(path, data, size);
    free(data);
    return ok;
}

/* the playlist: "GT4L", the count, each name with its length, in the
   save's container */
int gt4_replay_playlist_read(char (*names)[64], int max)
{
    char dir[1100], path[1200];
    uint8_t *data;
    size_t size;
    int n = 0;
    if (!folder(dir, sizeof dir) || max <= 0) return 0;
    snprintf(path, sizeof path, "%s/playlist.ogt4l", dir);
    if (gt4_save_read(path, &data, &size, NULL) <= 0) return 0;
    {
        in_buf i = { data, size, 0, 0 };
        char magic[4];
        uint32_t count;
        get(&i, magic, 4);
        count = get32(&i);
        if (!i.bad && !memcmp(magic, "GT4L", 4))
            for (uint32_t k = 0; k < count && n < max && !i.bad; k++) {
                gets_(&i, names[n], 64);
                if (!i.bad && valid_name(names[n])) n++;
            }
    }
    free(data);
    return n;
}

int gt4_replay_playlist_write(const char (*names)[64], int count)
{
    char dir[1100], path[1200];
    out_buf o = { 0 };
    int ok;
    if (!folder(dir, sizeof dir) || count < 0) return 0;
    put(&o, "GT4L", 4);
    put32(&o, (uint32_t)count);
    for (int k = 0; k < count; k++) puts_(&o, names[k]);
    if (o.bad) { free(o.p); return 0; }
    snprintf(path, sizeof path, "%s/playlist.ogt4l", dir);
    ok = gt4_save_write(path, o.p, o.n);
    free(o.p);
    return ok;
}

static int newest_first(const void *a, const void *b) { return strcmp((const char *)b, (const char *)a); }

int gt4_replay_list(char (*names)[64], int max)
{
    char dir[1100];
    int n = 0;
    if (!folder(dir, sizeof dir) || max <= 0) return 0;
#ifdef _WIN32
    {
        char pattern[1200];
        wchar_t wide[1200];
        WIN32_FIND_DATAW f;
        HANDLE h;
        snprintf(pattern, sizeof pattern, "%s/*.ogt4", dir);
        if (!MultiByteToWideChar(CP_UTF8, 0, pattern, -1, wide, 1200)) return 0;
        h = FindFirstFileW(wide, &f);
        if (h == INVALID_HANDLE_VALUE) return 0;
        do {
            char name[64];
            if (n < max && WideCharToMultiByte(CP_UTF8, 0, f.cFileName, -1, name, sizeof name, NULL, NULL) && valid_name(name))
                memcpy(names[n++], name, sizeof name);
        } while (FindNextFileW(h, &f));
        FindClose(h);
    }
#else
    {
        DIR *d = opendir(dir);
        struct dirent *e;
        if (!d) return 0;
        while ((e = readdir(d)) && n < max)
            if (valid_name(e->d_name)) snprintf(names[n++], 64, "%s", e->d_name);
        closedir(d);
    }
#endif
    qsort(names, (size_t)n, sizeof *names, newest_first);
    return n;
}

int gt4_replay_load(const char *name, gt4_replay_file *r)
{
    char dir[1100], path[1200];
    uint8_t *data;
    size_t size;
    uint64_t stamp = 0;
    int ok;
    memset(r, 0, sizeof *r);
    if (!valid_name(name) || !folder(dir, sizeof dir)) return 0;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    if (gt4_save_read(path, &data, &size, &stamp) <= 0) return 0;
    ok = gt4_replay_unpack(data, size, r);
    free(data);
    if (ok) r->when = stamp;
    return ok;
}

int gt4_replay_remove(const char *name)
{
    char dir[1100], path[1200], gone[1250];
    if (!valid_name(name) || !folder(dir, sizeof dir)) return 0;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    snprintf(gone, sizeof gone, "%s.deleted", path);
#ifdef _WIN32
    {
        wchar_t from[1200], to[1250];
        if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, from, 1200) || !MultiByteToWideChar(CP_UTF8, 0, gone, -1, to, 1250)) return 0;
        return MoveFileExW(from, to, MOVEFILE_REPLACE_EXISTING) != 0;
    }
#else
    return rename(path, gone) == 0;
#endif
}
