/*
 * The leaves the wheels throw up, from the game's code (see race_leaves.h).
 * Offsets in the comments are the LeafParticle's (+0x8 its angles, +0x14
 * place, +0x20 velocity, +0x2C turn, +0x38 ground plane, +0x48 light,
 * +0x4C its kind) and the kind's (0x74 bytes at the object's +0xC).
 */
#include "race_leaves.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define COUNT 128   /* the object's pool (func_003C1EC0: 128 of the largest particle) */
#define TWO_PI 6.28318530718f

typedef struct {
    int picture;                 /* +0x0: the Tex1's image, 1..8 */
    float mass, inv_mass, sink;  /* +0x4, +0x10, +0x14 */
    float inertia[3];            /* +0x18, +0x1C, +0x20 (x, y, z) */
    float drag_flat, drag_edge;  /* +0x24, +0x28 */
    float damp_x, damp_z;        /* +0x2C, +0x30 */
    float w, h;                  /* +0x8, +0xC */
} kind;

typedef struct {
    int next, kind;
    float angle[3], pos[3], vel[3], turn[3], plane[4], shade;
} leaf;

struct race_leaves {
    const uint8_t *env;
    kind kinds[8];
    leaf p[COUNT];
    int live, free_list;
    uint32_t r;
};

/* 0040B7E8 +10..120: restored films intentionally clear velocity,
   turn and plane; angles expand with PI*1/128, shade /255. */
int race_leaves_snapshot(race_leaves *l,const uint8_t records[][17],int count)
{
    if(!l || count<0 || count>COUNT || (count && !records))return 0;
    for(int i=0;i<count;i++){if(records[i][0]>=8)return 0;for(int j=0;j<3;j++){float x;memcpy(&x,records[i]+4+j*4,4);if(!isfinite(x))return 0;}}
    memset(l->p,0,sizeof l->p);l->live=count?0:-1;l->free_list=count<COUNT?count:-1;
    for(int i=0;i<COUNT;i++){leaf *p=&l->p[i];p->next=i+1<COUNT?i+1:-1;if(i<count){const uint8_t *a=records[i];p->next=i+1<count?i+1:-1;p->kind=a[0];for(int j=0;j<3;j++)p->angle[j]=a[1+j]*3.141592502593994140625f*(1.f/128);memcpy(p->pos,a+4,12);p->shade=a[16]/255.f;p->plane[1]=1;}}
    return 1;
}

static float envf(const race_leaves *l, size_t off)
{
    float f;
    memcpy(&f, l->env + off, 4);
    return f;
}

/* func_004AC400: the VU0's vrnext, less one (seeded as the smoke's) */
static float rnd(race_leaves *l)
{
    uint32_t x = (l->r >> 4) & 1, y = (l->r >> 22) & 1;
    float f;
    l->r = (((l->r << 1) ^ x ^ y) & 0x7fffffu) | 0x3f800000u;
    memcpy(&f, &l->r, 4);
    return f - 1;
}

/* func_0040AB98: a leaf of mass m, w by h: a plate's inertia, the drag
   across it and along it, the turn's damping */
static void make_kind(kind *k, int picture, float m, float w, float h)
{
    k->picture = picture;
    k->mass = m;
    k->w = w;
    k->h = h;
    k->sink = sqrtf(w * w + h * h);
    k->inertia[0] = m / 12 * h * h;
    k->inertia[1] = m / 12 * (w * w + h * h);
    k->inertia[2] = m / 12 * w * w;
    k->inv_mass = 1 / m;
    k->drag_flat = -2 * w * h;
    k->drag_edge = (w + h) * -2 * 0.005f;
    k->damp_x = -2 * w * h * h * h / 12 * 0.015f;
    k->damp_z = -2 * w * w * w * h / 12 * 0.015f;
}

race_leaves *race_leaves_open(const uint8_t *env0, size_t size)
{
    race_leaves *l;
    float seed = 0.1f;
    if (!env0 || size < 0x100 || memcmp(env0, "Env0", 4)) return NULL;
    l = calloc(1, sizeof *l);
    if (!l) return NULL;
    l->env = env0;
    /* func_003C1EC0: eight kinds of 1 g, the first three 12, 13 and 14 cm
       across, the others 10 cm */
    make_kind(&l->kinds[0], 1, 0.001f, 0.12f, 0.12f);
    make_kind(&l->kinds[1], 2, 0.001f, 0.13f, 0.13f);
    make_kind(&l->kinds[2], 3, 0.001f, 0.14f, 0.14f);
    for (int i = 3; i < 8; i++) make_kind(&l->kinds[i], i + 1, 0.001f, 0.1f, 0.1f);
    for (int i = 0; i < COUNT; i++) l->p[i].next = i + 1 < COUNT ? i + 1 : -1;
    l->live = -1;
    l->free_list = 0;
    memcpy(&l->r, &seed, 4);
    l->r = (l->r & 0x7fffffu) | 0x3f800000u;
    return l;
}

void race_leaves_close(race_leaves *l) { free(l); }

int race_leaves_height(const race_leaves *l, float *lo, float *hi)
{
    int n = 0;
    *lo = 1e30f;
    *hi = -1e30f;
    for (int i = l ? l->live : -1; i >= 0; i = l->p[i].next, n++) {
        const leaf *p = &l->p[i];
        float above = p->plane[0] * p->pos[0] + p->plane[1] * p->pos[1] + p->plane[2] * p->pos[2] + p->plane[3] - l->kinds[p->kind].sink;
        if (above < *lo) *lo = above;
        if (above > *hi) *hi = above;
    }
    return n;
}

int race_leaves_live(const race_leaves *l)
{
    int n = 0;
    for (int i = l ? l->live : -1; i >= 0; i = l->p[i].next) n++;
    return n;
}

/* func_003C2160 -> 0040AED0 */
void race_leaves_emit(race_leaves *l, const race_leaves_wheel *w)
{
    float slip, a, sn, cs, nx, ny, r, t, s, c, A, B;
    int i, k;
    leaf *p;
    if (!l || !w) return;
    slip = (w->ground - w->spin) * (rnd(l) * 0.45f + 0.5f);
    if (fabsf(slip) < 2) return;
    if (l->free_list < 0) return;                    /* func_003C26A8: none free */
    i = l->free_list;
    l->free_list = l->p[i].next;
    l->p[i].next = l->live;
    l->live = i;
    p = &l->p[i];
    /* thrown back across the wheel's way: within 15 degrees of its heading,
       65 to 85 degrees up the ground, to the side the wheel slips */
    a = w->heading + rnd(l) * 0.5235988f - 0.2617994f;
    sn = sinf(a);
    cs = cosf(a);
    nx = w->plane[0] * cs + w->plane[2] * sn;
    ny = w->plane[1];
    r = 1 / sqrtf(nx * nx + ny * ny);
    nx *= r;
    ny *= r;
    t = rnd(l) * 0.34906584f - 1.4835298f;
    if (slip < 0) t = -t;
    s = sinf(t);
    c = cosf(t);
    A = s * nx - c * ny;
    B = c * nx + s * ny;
    slip = fabsf(slip);
    p->vel[0] = A * cs * slip + w->vel[0] * 0.85f;
    p->vel[1] = B * slip + w->vel[1] * 0.85f;
    p->vel[2] = A * sn * slip + w->vel[2] * 0.85f;
    for (int j = 0; j < 3; j++) p->pos[j] = w->pos[j] + (rnd(l) - 0.5f) * 0.2f;
    p->turn[0] = (rnd(l) - 0.5f) * 20;
    p->turn[1] = (rnd(l) - 0.5f) * 8;
    p->turn[2] = (rnd(l) - 0.5f) * 20;
    k = (int)(rnd(l) * 8);
    if (k >= 8) k = 7;
    p->kind = k;
    memcpy(p->plane, w->plane, sizeof p->plane);
    p->plane[3] += l->kinds[k].sink;
    p->shade = w->shade;
    for (int j = 0; j < 3; j++) p->angle[j] = rnd(l) * TWO_PI;
}

/* func_0040AD10: the leaf's axes from its angles (rows: its x, y, z in
   the world) */
static void axes(const float angle[3], float m[3][3])
{
    float sx = sinf(angle[0]), cx = cosf(angle[0]), sy = sinf(angle[1]), cy = cosf(angle[1]);
    float sz = sinf(angle[2]), cz = cosf(angle[2]);
    m[0][0] = cy * cz - sx * sz * sy; m[0][1] = -cx * sz; m[0][2] = sy * cz + sx * sz * cy;
    m[1][0] = cy * sz + sx * cz * sy; m[1][1] = cx * cz;  m[1][2] = sy * sz - sx * cz * cy;
    m[2][0] = -cx * sy;               m[2][1] = sx;       m[2][2] = cx * cy;
}

static float wrap(float a)
{
    if (a < 0) a += TWO_PI;
    if (TWO_PI <= a) a -= TWO_PI;
    return a;
}

/* func_0040AFB0: 0 once it is under the ground */
static int step(const race_leaves *l, leaf *p, float dt)
{
    const kind *k = &l->kinds[p->kind];
    float m[3][3], v[3], f[3], F[3], t[3], wx = p->turn[0], wy = p->turn[1], wz = p->turn[2];
    axes(p->angle, m);
    for (int i = 0; i < 3; i++) v[i] = m[i][0] * p->vel[0] + m[i][1] * p->vel[1] + m[i][2] * p->vel[2];   /* 0040AE78 */
    f[0] = k->drag_edge * v[0];
    f[1] = k->drag_flat * v[1];
    f[2] = k->drag_edge * v[2];
    for (int i = 0; i < 3; i++) F[i] = m[0][i] * f[0] + m[1][i] * f[1] + m[2][i] * f[2];                 /* 0040AE20 */
    F[1] += -k->mass * 9.81f;
    t[0] = k->damp_x * wx + (k->inertia[1] - k->inertia[2]) * wy * wz;
    t[1] = (k->inertia[2] - k->inertia[0]) * wz * wx;
    t[2] = k->damp_z * wz + (k->inertia[0] - k->inertia[1]) * wx * wy;
    p->turn[0] += t[0] * dt / k->inertia[0];
    p->turn[2] += t[2] * dt / k->inertia[2];
    p->turn[1] += t[1] * dt / k->inertia[1];
    {
        float sx = sinf(p->angle[0]), cx = cosf(p->angle[0]), sz = sinf(p->angle[2]), cz = cosf(p->angle[2]);
        if (cx != 0) {
            float a = (wx * sz - wy * cz) / cx;
            p->angle[0] += -(wx * cz + wy * sz) * dt;
            p->angle[1] += a * dt;
            p->angle[2] += -(a * sx + wz) * dt;
        }
    }
    for (int i = 0; i < 3; i++) p->angle[i] = wrap(p->angle[i]);
    for (int i = 0; i < 3; i++) {
        p->pos[i] += p->vel[i] * dt;
        p->vel[i] += F[i] * k->inv_mass * dt;
        p->turn[i] *= 0.999f;
    }
    return p->plane[0] * p->pos[0] + p->plane[1] * p->pos[1] + p->plane[2] * p->pos[2] + p->plane[3] >= 0;
}

/* func_003C2780: each one moved, the ones under the ground freed */
void race_leaves_update(race_leaves *l, float dt)
{
    int prev = -1;
    if (!l) return;
    for (int i = l->live, next; i >= 0; i = next) {
        next = l->p[i].next;
        if (step(l, &l->p[i], dt)) { prev = i; continue; }
        if (prev >= 0) l->p[prev].next = next;
        else l->live = next;
        l->p[i].next = l->free_list;
        l->free_list = i;
    }
}

/* func_0040B4E0: the leaf's square (+-w/2, 0, +-h/2 on its axes) at its
   place; func_0040B3C8: its colour by the course's light (Env0 +0xD0 the
   way, +0xE0 the ambient, +0xF0 the light's colour, +0x38 the scale),
   the side it is seen from deciding the light's sign */
int race_leaves_quads(const race_leaves *l, const float eye[3], const float rows[9], race_leaves_quad *out, int max)
{
    static const float uv[4][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } };   /* D_006A44F0 */
    static const float corner[4][2] = { { -.5f, -.5f }, { .5f, -.5f }, { -.5f, .5f }, { .5f, .5f } };
    int q = 0;
    if (!l || !out) return 0;
    for (int i = l->live; i >= 0 && q < max; i = l->p[i].next) {
        const leaf *p = &l->p[i];
        const kind *k = &l->kinds[p->kind];
        race_leaves_quad *o = &out[q];
        float m[3][3], L[3], len, lit, d[3];
        int back;
        axes(p->angle, m);
        for (int c = 0; c < 3; c++) d[c] = p->pos[c] - eye[c];
        back = d[0] * m[1][0] + d[1] * m[1][1] + d[2] * m[1][2] < 0;
        for (int c = 0; c < 3; c++) L[c] = envf(l, 0xd0 + 4u * (unsigned)c);
        len = sqrtf(L[0] * L[0] + L[1] * L[1] + L[2] * L[2]);
        lit = (m[1][0] * L[0] + m[1][1] * L[1] + m[1][2] * L[2]) / (len > 0 ? len : 1) * (p->shade * 0.7f + 0.3f);
        if (!back) lit = -lit;
        if (lit < 0) lit = 0;
        for (int c = 0; c < 3; c++) {
            int v = (int)((envf(l, 0xf0 + 4u * (unsigned)c) * lit + envf(l, 0xe0 + 4u * (unsigned)c) * p->shade) * 128 * (1 / envf(l, 0x38)));
            if (v > 128) v = 128;
            o->rgb[c] = (float)v / 128;
        }
        for (int v = 0; v < 4; v++) {
            float lx = corner[v][0] * k->w, lz = corner[v][1] * k->h, wpt[3];
            for (int c = 0; c < 3; c++) wpt[c] = p->pos[c] + m[0][c] * lx + m[2][c] * lz - eye[c];
            for (int c = 0; c < 3; c++) o->xyz[v][c] = rows[c * 3] * wpt[0] + rows[c * 3 + 1] * wpt[1] + rows[c * 3 + 2] * wpt[2];
            o->uv[v][0] = uv[v][0];
            o->uv[v][1] = uv[v][1];
        }
        o->picture = k->picture - 1;
        q++;
    }
    return q;
}
