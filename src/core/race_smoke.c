/*
 * The tyres' smoke, from the game's code (see race_smoke.h). Offsets in
 * the comments are the particle's 64 bytes (the pool at the object's +0)
 * and the class record's (Env0 +0x4D0 + class * 0x44).
 */
#include "race_smoke.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define COUNT 256

typedef struct {
    int next;            /* +0, 13 bits */
    int tag;             /* +0 bits 13..15: the car (a2) */
    int size_class;      /* +2 bits 0..2: which D_006A3A80 rotation share */
    int flip;            /* +2 bit 5: a right side wheel (turns the other way) */
    int frame;           /* +2 bits 6..7: the texture's quarter */
    int cls;             /* +3 */
    float rgb[3];        /* +4, the GS's 128 = 1 */
    float size;          /* +8 */
    float strength;      /* +0xC */
    float share;         /* +0x10 */
    float angle;         /* +0x14, degrees */
    float rise;          /* +0x18 */
    float fade_mul;      /* +0x1C, u16 / 256 */
    float grow_mul;      /* +0x1E, u8 / 128 */
    float pos[3];        /* +0x20 */
    float ground;        /* +0x2C: the ground's height under the point */
} particle;

struct race_smoke {
    const uint8_t *env;
    particle p[COUNT];
    int live, free_list;   /* +4, +6 */
    unsigned counter[8];   /* +8 */
    uint32_t r;            /* the VU0 R register (vrinit/vrnext) */
    float alpha_scale, color_scale;          /* +0x3C, +0x40 */
    float near_base, near_scale, near_dist;  /* +0x5C, +0x60, +0x64 */
};

/* D_006A3A00: the surface (wheel +0x89) to its smoke class */
static const uint8_t surface_class[24] = { 0, 0, 1, 2, 3, 4, 5, 0, 0, 0, 0, 0, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5 };
/* D_006A3A60: the share of the amount by the size class */
static const float amount_share[8] = { 0.9953219891f, 0.00440770993f, 0.1185689941f, 0.5101419687f,
                                       0.03525039926f, 0.7672259808f, 0.2763259709f, 0.9418839812f };
/* D_006A3A80: the turning speed's share */
static const float turn_share[8] = { 0.006611519959f, 1.f, 0.7265239954f, 0.4041169882f,
                                     0.9955199957f, 0.177034989f, 0.6997049451f, 0.05285409838f };
/* D_006A3AB0: the four quarters of the texture, corners in drawing order */
static const float frame_uv[4][4][2] = {
    { { 0.00390625f, 0.0078125f }, { 0.24609375f, 0.0078125f }, { 0.00390625f, 0.9921875f }, { 0.24609375f, 0.9921875f } },
    { { 0.25390625f, 0.0078125f }, { 0.49609375f, 0.0078125f }, { 0.25390625f, 0.9921875f }, { 0.49609375f, 0.9921875f } },
    { { 0.50390625f, 0.0078125f }, { 0.74609375f, 0.0078125f }, { 0.50390625f, 0.9921875f }, { 0.74609375f, 0.9921875f } },
    { { 0.75390625f, 0.0078125f }, { 0.99609375f, 0.0078125f }, { 0.75390625f, 0.9921875f }, { 0.99609375f, 0.9921875f } },
};

static float envf(const race_smoke *s, size_t off)
{
    float f;
    memcpy(&f, s->env + off, 4);
    return f;
}
static uint32_t envu(const race_smoke *s, size_t off)
{
    uint32_t u;
    memcpy(&u, s->env + off, 4);
    return u;
}

int race_smoke_snapshot(race_smoke *s,const uint8_t (*records)[33],int count)
{
    if(!s || count<0 || count>COUNT || (count && !records))return 0;
    for(int i=0;i<count;i++)if(records[i][1]>=6)return 0;
    race_smoke_clear(s);s->live=count?0:-1;s->free_list=count<COUNT?count:-1;
    for(int i=0;i<count;i++){
        const uint8_t *a=records[i];particle *p=&s->p[i];memset(p,0,sizeof *p);
        /* 003E96B8 +44..130: packed colors/bytes and authored scales. */
        p->next=i+1<count?i+1:-1;p->size_class=a[0]&7;p->flip=!!(a[0]&32);p->frame=a[0]>>6;p->cls=a[1];
        for(int k=0;k<3;k++)p->rgb[k]=a[2+k]/128.f;
        p->size=a[6]*10.f/255.f;p->strength=a[7]/255.f;p->share=a[8]/255.f;
        p->angle=(int8_t)a[9]*360.f/255.f;p->rise=(int8_t)a[10]*2.f/127.f;
        p->fade_mul=(a[11]|a[12]<<8)/256.f;p->grow_mul=(int8_t)a[13]/128.f;
        memcpy(p->pos,a+14,12);memcpy(&p->ground,a+26,4);
    }
    return 1;
}
static float rec(const race_smoke *s, int cls, size_t off) { return envf(s, 0x4d0u + (size_t)cls * 0x44u + off); }

/* func_004AC400: vrnext (the VU0's 23 bit shift register, read as 1..2), less one */
static float rnd(race_smoke *s)
{
    uint32_t x = (s->r >> 4) & 1, y = (s->r >> 22) & 1;
    float f;
    s->r = (((s->r << 1) ^ x ^ y) & 0x7fffffu) | 0x3f800000u;
    memcpy(&f, &s->r, 4);
    return f - 1;
}

/* func_003E7538: every particle free, the list in order; the generator
   seeded with 0.1 (func_004AC41C: vrinit) */
void race_smoke_clear(race_smoke *s)
{
    float seed = 0.1f;
    uint32_t bits;
    if (!s) return;
    for (int i = 0; i < COUNT; i++) s->p[i].next = i + 1 < COUNT ? i + 1 : -1;
    s->live = -1;
    s->free_list = 0;
    memset(s->counter, 0, sizeof s->counter);
    memcpy(&bits, &seed, 4);
    s->r = (bits & 0x7fffffu) | 0x3f800000u;
}

race_smoke *race_smoke_open(const uint8_t *env0, size_t size)
{
    race_smoke *s;
    if (!env0 || size < 0x920 || memcmp(env0, "Env0", 4)) return NULL;
    s = calloc(1, sizeof *s);
    if (!s) return NULL;
    s->env = env0;
    /* the object's scales as a race has them (capture ar_02, world +0x31D0) */
    s->alpha_scale = s->color_scale = 1;
    s->near_base = 153.1f;
    s->near_scale = 102.f;
    s->near_dist = 120.f;
    race_smoke_clear(s);
    return s;
}

void race_smoke_close(race_smoke *s) { free(s); }

int race_smoke_blend(const race_smoke *s) { return s ? (int)envu(s, 0x918) : 0; }

int race_smoke_live(const race_smoke *s)
{
    int n = 0;
    for (int i = s ? s->live : -1; i >= 0; i = s->p[i].next) n++;
    return n;
}

/* func_003E79F8: with none free, the live one that shows least (+0xC * +0x10) */
static int weakest(const race_smoke *s)
{
    int at = 0;
    float least = 1e7f;
    for (int i = s->live; i >= 0; i = s->p[i].next) {
        float v = s->p[i].strength * s->p[i].share;
        if (v < least) { least = v; at = i; }
    }
    return at;
}

/* func_003E75E8 */
void race_smoke_emit(race_smoke *s, const race_smoke_wheel *w)
{
    int cls, i, k;
    float amount, light, r, ny;
    particle *p;
    if (!s || !w || w->surface < 0 || w->surface >= 24) return;
    cls = surface_class[w->surface];
    if (rec(s, cls, 0xc) < 0.01f) return;              /* +0x4DC: the class does not smoke */
    if (++s->counter[cls] > envu(s, 0x4d0u + (size_t)cls * 0x44u + 0x40)) s->counter[cls] = 0;
    amount = w->amount;
    if (amount <= 0.001f) return;
    if (s->free_list < 0) i = weakest(s);
    else {
        i = s->free_list;
        s->free_list = s->p[i].next;
        s->p[i].next = s->live;
        s->live = i;
    }
    p = &s->p[i];
    k = (int)(rnd(s) * 8);
    if (k >= 8) k = 7;
    p->size_class = k;
    amount *= amount_share[k];
    p->flip = w->wheel & 1;
    p->frame = i & 3;
    p->cls = cls;
    /* the colour: the class's by the ground's light, mixed (Env0 +0x910) */
    light = (w->shade * (1 - envf(s, 0x910)) + envf(s, 0x910)) * s->color_scale;
    for (int c = 0; c < 3; c++) p->rgb[c] = rec(s, cls, 4u * (size_t)c) * light;
    p->strength = amount;
    p->size = rec(s, cls, 0x10) * amount;
    p->rise = rec(s, cls, 0x18) * ((rnd(s) - 0.2f) * 0.3f + 1);
    p->angle = rnd(s) * 360;
    p->share = amount + (1 - amount) * rec(s, cls, 0x30);
    r = rnd(s);
    {
        float f = r * ((float)envu(s, 0x4d0u + (size_t)cls * 0x44u + 0x40) + 0.2f) + 1;
        if (f >= 255) f = 255;
        p->fade_mul = (float)(uint16_t)(int)(f * 256) / 256;
    }
    (void)(int)(rnd(s) * 64);                           /* +0x1F, not read by the smoke */
    p->grow_mul = (float)(uint8_t)(int)(((rnd(s) - 0.5f) * 0.2f + 1) * 128) / 128;
    p->tag = 0;
    memcpy(p->pos, w->pos, sizeof p->pos);
    ny = w->plane[1];
    p->ground = ny != 0 ? -(w->plane[0] * w->pos[0] + w->plane[2] * w->pos[2] + w->plane[3]) / ny : w->pos[1];
}

/* func_003E7A58 */
void race_smoke_update(race_smoke *s, float dt)
{
    int prev = -1;
    if (!s) return;
    for (int i = s->live, next; i >= 0; i = next) {
        particle *p = &s->p[i];
        float turn = rec(s, p->cls, 0x1c), dead;
        next = p->next;
        if (!p->flip) turn = -turn;
        turn *= turn_share[p->size_class] / fmaxf(p->size, 1);
        p->angle += turn * dt;
        p->size += rec(s, p->cls, 0x14) * dt * p->grow_mul;
        p->strength -= rec(s, p->cls, 0x20) * dt * p->fade_mul;
        p->rise -= dt * 9.81f * rec(s, p->cls, 0x2c);
        p->pos[1] += p->rise * dt;
        if (p->angle > 360) p->angle -= 360;
        if (p->angle < 0) p->angle += 360;
        dead = p->strength <= 0 || p->pos[1] + p->size * 1.2071068f < p->ground;
        if (!dead) { prev = i; continue; }
        if (prev >= 0) s->p[prev].next = next;
        else s->live = next;
        p->next = s->free_list;
        s->free_list = i;
    }
}

typedef struct { float z; int i; float c[3]; } seen;

static int far_first(const void *a, const void *b)
{
    float za = ((const seen *)a)->z, zb = ((const seen *)b)->z;
    return za < zb ? -1 : za > zb;
}

/* func_003E7F08: each live particle's centre into the view, kept between
   0.01 and the course's far (Env0 +0x20) in front; its colour and
   alpha (func_003E8BA8); a square of half side `size` turned by its
   angle (func_003E8610 / 003E8790) */
int race_smoke_quads(const race_smoke *s, const float eye[3], const float rows[9], race_smoke_quad *out, int max)
{
    seen list[COUNT];
    int n = 0, q = 0;
    float far = s ? envf(s, 0x20) : 0;
    if (!s || !out) return 0;
    for (int i = s->live; i >= 0 && n < COUNT; i = s->p[i].next) {
        const particle *p = &s->p[i];
        float d[3] = { p->pos[0] - eye[0], p->pos[1] - eye[1], p->pos[2] - eye[2] }, c[3];
        for (int k = 0; k < 3; k++) c[k] = rows[k * 3] * d[0] + rows[k * 3 + 1] * d[1] + rows[k * 3 + 2] * d[2];
        if (!(-c[2] > 0.01f && -c[2] < far)) continue;
        list[n].z = c[2];
        list[n].i = i;
        memcpy(list[n].c, c, sizeof c);
        n++;
    }
    if (race_smoke_blend(s) != 0x48) qsort(list, (size_t)n, sizeof *list, far_first);   /* func_003E9018 */
    for (int k = 0; k < n && q < max; k++) {
        const particle *p = &s->p[list[k].i];
        race_smoke_quad *o = &out[q];
        float a, sn, cs, e0, e1, *c = list[k].c;
        /* func_003E8BA8: strength * share * the class's +0x4DC, by 255 or,
           when Env0 +0x10 has 0x40000, by how near it is; at most 128 */
        a = p->strength * p->share * rec(s, p->cls, 0xc);
        if (envu(s, 0x10) & 0x40000u) a *= fminf(s->near_dist / -c[2], 1) * s->near_scale + s->near_base;
        else a *= 255;
        a *= s->alpha_scale;
        if ((int)a > 128) a = 128;
        if (a <= 0) continue;
        sn = sinf(p->angle * 3.14159265f / 180);
        cs = cosf(p->angle * 3.14159265f / 180);
        e0 = (cs - sn) * p->size;
        e1 = (sn + cs) * p->size;
        o->xyz[0][0] = c[0] + e0; o->xyz[0][1] = c[1] + e1;
        o->xyz[1][0] = c[0] - e1; o->xyz[1][1] = c[1] + e0;
        o->xyz[2][0] = c[0] + e1; o->xyz[2][1] = c[1] - e0;
        o->xyz[3][0] = c[0] - e0; o->xyz[3][1] = c[1] - e1;
        for (int v = 0; v < 4; v++) {
            o->xyz[v][2] = c[2];
            o->uv[v][0] = frame_uv[p->frame][v][0];
            o->uv[v][1] = frame_uv[p->frame][v][1];
        }
        for (int v = 0; v < 3; v++) o->rgba[v] = fminf(p->rgb[v], 255.f / 128);
        o->rgba[3] = (float)(int)a / 128;
        q++;
    }
    return q;
}
