#include "course_tvcam.h"
#include "course_pregrid.h"
#include <fenv.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* 00385058 / 00385098: value, target, speed, step limit, stiffness,
   damping, gain, lower and upper speed caps */
typedef struct { float v, x, speed, step, k, c, gain, low, high; } spring;
/* 00384E58 / 00384EE8: an angle (degrees) with the operator's hand: the
   last 32 aims and their changes (00384DB8), a wandering stiffness
   (00384C48) */
typedef struct {
    float pos, vel, k1, k2, gain;
    int count; float aim[32], turn[32];
    float wander[3]; int wander_ticks;
    float k;
} operator_spring;

struct gt4_tvcam {
    const uint8_t *data, *entries;
    size_t size;
    unsigned count;
    float length;          /* REP4 +0x14: the course's length */
    int shot;              /* the running shot, -1 none */
    int frame, first, from_mid;
    uint32_t rng;          /* the camera's random state (+0x1AC, 00579330) */
    float offset[3];       /* +0xDC: the aim above the car (0037BF00: y 0.4) */
    float roll_scale;      /* +0x1A4: a random 0..1 at the start, 1 with kind 0x400 */
    float zoom;            /* +0x14C: the replay's zoom (Triangle: 4), 1 otherwise */
    spring frame_spring[4];            /* +0x450, +0x474, +0x498, +0x4BC */
    operator_spring pitch, yaw;        /* +0x1B0, +0x2DC */
    spring roll, lens, shake;          /* +0x408, +0x42C, +0x4E0 */
};

static uint32_t u32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static float f32(const uint8_t *p) { uint32_t u = u32(p); float f; memcpy(&f, &u, 4); return f; }
static float tv_bits(uint32_t u) { float f; memcpy(&f,&u,4); return f; }

/* 00579330: x = x * 17 + 17, the result x ^ rotl16(x). 00578598
   +18..48 constructs IEEE bits via plzcw/variable shifts; a division by
   2^32 does NOT reproduce that mantissa. */
static float random01(uint32_t *state)
{
    uint32_t v = *state * 17u + 0x11u;
    *state = v;
    v ^= v << 16 | v >> 16;
    if(!v)return 0;
    int leading=-1;
    if(!(v&0x80000000u)){
        uint32_t bits=v;leading=0;
        while(!(bits&0x80000000u)){bits<<=1;leading++;}
        leading--;
    }
    uint32_t exponent=(uint32_t)(125-leading);
    uint32_t bits=(exponent<<23)|((v<<((0u-exponent)&31u))>>9);
    float result;memcpy(&result,&bits,4);return result;
}

static void spring_init(spring *s, float v, float k, float c, float gain, float step, float low, float high)
{
    s->v = s->x = v; s->speed = 0; s->step = step / 60.f; s->k = k; s->c = c; s->gain = gain; s->low = low; s->high = high;
}
static float spring_step(spring *s, float x)
{
    float prev = s->x;
    s->x = x;
    if (s->step > 0 && fabsf(x - prev) > s->step) s->x = x - prev > 0 ? prev + s->step : prev - s->step;
    s->speed += (s->k * (s->x - s->v) + s->c * s->speed) / 60.f * s->gain;
    if (s->high > 0 && s->speed > s->high) s->speed = s->high;
    if (s->low > 0 && s->speed < -s->low) s->speed = -s->low;
    s->v += s->speed / 60.f;
    return s->v;
}

/* 0036EB98: a - b within -180..180; 0036EAD8: an angle into 0..360 */
static float angle_diff(float a, float b)
{
    float d = a - b;
    if (d > 180) d -= 360;
    if (d < -180) d += 360;
    return d;
}
static float angle_wrap(float x)
{
    if (x >= 360) x -= (float)(int)(x / 360) * 360;
    if (x < 0) { x = -x; x -= (float)(int)(x / 360) * 360; x = 360 - x; }
    return x;
}

static void operator_init(operator_spring *s, uint32_t *rng, float value, float k)
{
    memset(s, 0, sizeof *s);
    s->pos = value; s->vel = 0; s->k1 = 600; s->k2 = -60; s->gain = 1; s->k = k;
    s->aim[0] = value;
    s->wander[0] = s->wander[1] = random01(rng) * 256;
}
static void operator_step(operator_spring *s, uint32_t *rng, float value, float k, int wandering)
{
    float predicted, error, stiff = 1, a;
    s->k = k;
    /* 00384DB8: the aims and their turns, the newest first */
    if (s->count < 32) s->count++;
    for (int i = 31; i > 0; i--) { s->aim[i] = s->aim[i - 1]; s->turn[i] = s->turn[i - 1]; }
    s->aim[0] = value;
    s->turn[0] = s->count >= 2 ? angle_diff(value, s->aim[1]) : 0;
    /* 00384C48: every third tick the wander may take a new goal */
    {
        float r = random01(rng);
        if (s->wander_ticks < 2) s->wander_ticks++;
        else {
            s->wander_ticks = 0;
            /* 00384C98..CB8: retain the authored mantissas. */
            if (tv_bits(0xbe2147b0) + tv_bits(0x3f7e4b17) < r) s->wander[1] = random01(rng) * 256;
        }
        if (k > 1) s->wander[1] = 256;
        s->wander[2] += ((s->wander[1] - s->wander[0]) * 60 + s->wander[2] * -10) / 60;
        s->wander[0] += s->wander[2] / 60;
    }
    /* 00384E38: the aim five ticks ahead at its last turn */
    predicted = s->aim[0] + 5 * s->turn[0];
    error = angle_diff(value, s->pos) + (predicted - value);
    /* 00384F8C..FA8: 0.6 is truncated in the original executable. */
    if (wandering) stiff = s->wander[0] / 256 * tv_bits(0x3f666666) + tv_bits(0x3f199999);
    if (k < 0) k = 0;
    if (k > 1) k = 1;
    a = (s->k1 * stiff * error * k + s->k2 * s->vel) / 60 * s->gain;
    s->vel += a;
    s->pos = angle_wrap(s->pos + s->vel / 60);
}

static int valid_curve(const gt4_tvcam *c, uint32_t at)
{
    if (!at) return 1;
    if (at > c->size || c->size - at < 4) return 0;
    unsigned n = u32(c->data + at);
    return n >= 2 && n <= (c->size - at - 4) / 8;
}

gt4_tvcam *gt4_tvcam_open(const uint8_t *course, size_t size)
{
    if (!course || size < 0xb4) return NULL;
    uint32_t at = u32(course + 0xb0);
    if (!at || at > size || size - at < 0x28 || memcmp(course + at, "REP4", 4)) return NULL;
    gt4_tvcam *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    c->zoom = 1;
    c->data = course + at;
    c->size = size - at;
    c->count = u32(c->data + 0x20);
    c->length = f32(c->data + 0x14);
    uint32_t list = u32(c->data + 0x24);
    if (!c->count || list > c->size || c->count > (c->size - list) / 0x7c || !(c->length > 0)) goto invalid;
    c->entries = c->data + list;
    for (unsigned i = 0; i < c->count; i++) {
        const uint8_t *e = c->entries + i * 0x7c;
        for (unsigned k = 0x34; k <= 0x58; k += 4) if (!valid_curve(c, u32(e + k))) goto invalid;
        for (unsigned k = 0x64; k <= 0x74; k += 4) if (!valid_curve(c, u32(e + k))) goto invalid;
    }
    /* 003721D8 +44..48 -> 005792D8 +00..0C: SceneCamera seed. */
    c->rng = 0x12345678u;
    gt4_tvcam_reset(c);
    return c;
invalid:
    free(c);
    return NULL;
}

void gt4_tvcam_close(gt4_tvcam *c) { free(c); }
void gt4_tvcam_reset(gt4_tvcam *c) { if (c) c->shot = -1; }
void gt4_tvcam_set_zoom(gt4_tvcam *c, float zoom) { if (c) c->zoom = zoom > 0 ? zoom : 1; }

float gt4_tvcam_zoom_step(float s[3], int on)
{
    /* 003852A0: toward on x 20 a spring (40, damping -10, /60) whose
       speed stays within +/-900; back, x0.8 a frame down to the target */
    float target = on ? tv_bits(0x41a00000) : 0;
    s[1] = target;
    if (s[0] < target) {
        float v = s[2] + ((target - s[0]) * tv_bits(0x42200000) + s[2] * tv_bits(0xc1200000)) / tv_bits(0x42700000);
        if (v <= tv_bits(0xc4610000)) v = tv_bits(0xc4610000);
        if (tv_bits(0x44610000) <= v) v = tv_bits(0x44610000);
        s[2] = v;
        s[0] = s[0] + v / tv_bits(0x42700000);
    } else {
        s[0] = s[0] * tv_bits(0x3f4ccccc);
        if (s[0] < target) s[0] = target;
    }
    /* 00385370, then 003751F0: 1 + 3x, within 1..4 */
    float z = s[0] / tv_bits(0x41a00000) * 3 + 1;
    if (z <= 1) z = 1;
    if (4 <= z) z = 4;
    return z;
}
unsigned gt4_tvcam_count(const gt4_tvcam *c) { return c ? c->count : 0; }

static float curve(const gt4_tvcam *c, const uint8_t *e, unsigned field, float t, float missing)
{
    uint32_t at = u32(e + field);
    return at ? gt4_pregrid_curve(c->data + at, t) : missing;
}

/* 003DDFF0: the first shot whose span holds the distance (the course
   wrapping at its length) */
static int pick(const gt4_tvcam *c, float distance)
{
    float d = fmodf(distance, c->length);
    if (d < 0) d += c->length;
    for (int lap = 0; lap < 2; lap++)
        for (unsigned i = 0; i < c->count; i++) {
            const uint8_t *e = c->entries + i * 0x7c;
            float x = d - lap * c->length;
            if (f32(e + 4) <= x && x < f32(e + 8)) return (int)i;
        }
    return -1;
}

/* 0037BF00: a shot starts */
static void start(gt4_tvcam *c, int shot)
{
    const uint8_t *e = c->entries + shot * 0x7c;
    uint32_t kind = u32(e);
    c->shot = shot;
    c->frame = 0;
    c->first = 1;
    c->from_mid = 0;
    /* 003747B8 +128..15C consumes this draw even when 0037BF00
       +34..50 subsequently pins kind 400 roll scale to one. */
    float roll_random = random01(&c->rng);
    c->roll_scale = (kind & 0x400) ? 1 : roll_random;
    c->offset[0] = 0; c->offset[1] = tv_bits(0x3ecccccc); c->offset[2] = 0; /* 0037BFE8..C010 */
}

/* 003FD7E0: the distance moved by whole laps toward the shot's span */
static float into_span(float start_d, float end_d, float length, float d)
{
    for (int i = 0; i < 256 && d < start_d; i++) d += length;
    for (int i = 0; i < 256 && d > end_d && d - length >= start_d; i++) d -= length;
    return d;
}

static int step(gt4_tvcam *c, const gt4_tvcam_target *tg, gt4_tvcam_view *o)
{
    if (c->shot < 0) {
        int s = pick(c, tg->distance);
        if (s < 0) return 0;
        start(c, s);
    }
    const uint8_t *e = c->entries + c->shot * 0x7c;
    uint32_t kind = u32(e);
    float t, frame_v[3], aim_v[3], eye[3], target[3], dir[3], dist, pitch, yaw, roll, lens, zoom, aimed = 1, steer = 0;
    int done;
    /* +0x00..+0xC8: the shot's progress, by time or by the car's place */
    if ((kind & 0x30) == 0x10) t = gt4_pregrid_time(f32(e + 8), ++c->frame, &done);
    else {
        float a = f32(e + 4), b = f32(e + 8), d = into_span(a, b, c->length, tg->distance);
        t = (d - a) / (b - a);
        done = t < 0 || t > 1;
        t = fmaxf(0, fminf(1, t));
    }
    if (c->first && t > .5f) c->from_mid = 1;
    /* +0x64 and +0x50: the framing pairs (curves, 1 without) */
    for (int k = 0; k < 2; k++) { frame_v[k] = curve(c, e, 0x64 + k * 4, t, 1); aim_v[k] = curve(c, e, 0x50 + k * 4, t, 1); }
    if (c->from_mid) frame_v[0] = frame_v[1] = 1;
    if (c->first) {
        spring_init(&c->frame_spring[0], frame_v[0], 50, -20, 1, 1.5f, 100, 100);
        spring_init(&c->frame_spring[1], frame_v[1], 50, -20, 1, 1.5f, 100, 100);
        spring_init(&c->frame_spring[2], aim_v[0], 70, -20, 1, 2, 150, 150);
        spring_init(&c->frame_spring[3], aim_v[1], 70, -20, 1, 2, 150, 150);
    } else {
        spring_step(&c->frame_spring[0], frame_v[0]);
        spring_step(&c->frame_spring[1], frame_v[1]);
        spring_step(&c->frame_spring[2], aim_v[0]);
        spring_step(&c->frame_spring[3], aim_v[1]);
    }
    frame_v[0] = c->frame_spring[0].v; frame_v[1] = c->frame_spring[1].v;
    aim_v[0] = c->frame_spring[2].v; aim_v[1] = c->frame_spring[3].v;
    /* the aim: the car (kind 0x0C) 0.4 above it, else the +0x40 curves */
    if (kind & 0xc) {
        for (int k = 0; k < 3; k++) target[k] = tg->pos[k] + c->offset[k];
        steer = tv_bits(0x3e999999); /* 0037C40C..414 */
        aimed = c->zoom;   /* 0x0037C1A4: +0x14C, only on the car (else 1, 0x0037C3F8) */
    } else {
        for (int k = 0; k < 3; k++) target[k] = curve(c, e, 0x40 + k * 4, t, 1);
        aimed = 1;
    }
    for (int k = 0; k < 3; k++) { eye[k] = curve(c, e, 0x34 + k * 4, t, 1); dir[k] = target[k] - eye[k]; }
    dist = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    /* 0036EA50 / 0036EA98: pitch and heading in degrees */
    pitch = -atan2f(dir[1], sqrtf(dir[0] * dir[0] + dir[2] * dir[2])) * 57.29578f;
    yaw = (tv_bits(0x40490fda) - atan2f(dir[0], dir[2])) * 57.29578f;
    roll = u32(e + 0x6c) ? curve(c, e, 0x6c, t, 0) : 0;
    /* the lens: +0x4C, with kind 0x100 the size it keeps at the aim's
       distance (2 atan(size / 2 distance)), between +0x5C and +0x60 */
    lens = curve(c, e, 0x4c, t, 1);
    if ((kind & 0x100) && dist > 0) lens = atanf(1 / (2 * dist / lens)) * 2 / 0.017453292f;
    lens = fmaxf(f32(e + 0x5c), fminf(f32(e + 0x60), lens));
    zoom = fmaxf(1, lens / aimed);
    /* the framing moves the aim by the zoom (0036EBE8) */
    yaw -= aim_v[0] * zoom * .5f;
    /* 0036EBE8 +18..60: half zoom precedes sine multiplication. */
    pitch = pitch - (sinf((pitch * tv_bits(0x40490fda)) / 180) * (zoom * .5f)) * steer + aim_v[1] * zoom * .5f;
    if (c->first) {
        operator_init(&c->pitch, &c->rng, pitch, frame_v[0]);
        operator_init(&c->yaw, &c->rng, yaw, frame_v[1]);
        spring_init(&c->roll, roll, 300, -40, 1, 0, 150, 150);
        spring_init(&c->lens, lens, 100, -20, 1, 0, 150, 100);
        {
            float r = random01(&c->rng), shake = r < .16f ? random01(&c->rng) - .5f : 1;
            spring_init(&c->shake, shake, 50, -20, 1, 2, 25, 25);
        }
    }
    /* 0037C6D8..748: initialization is followed by an update in the SAME
       frame, including both RNG operators and the shake's target 2. */
    operator_step(&c->pitch, &c->rng, pitch, frame_v[0], frame_v[0] <= 1);
    operator_step(&c->yaw, &c->rng, yaw, frame_v[1], frame_v[1] <= 1);
    spring_step(&c->roll, roll);
    spring_step(&c->lens, lens);
    spring_step(&c->shake, 2);
    if (kind & 0xc) { pitch = c->pitch.pos; yaw = c->yaw.pos; roll = c->roll.v; }
    roll *= c->roll_scale;
    lens = c->lens.v / aimed; /* 0037C888: zoom AFTER the lens spring */
    c->first = 0;
    /* the view: Rz(roll) Rx(pitch) Ry(yaw), back to a point it looks at */
    {
        float p = pitch * 0.017453292f, h = yaw * 0.017453292f;
        float d[3] = { sinf(tv_bits(0x40490fda) - h) * cosf(p), -sinf(p), cosf(tv_bits(0x40490fda) - h) * cosf(p) };
        for (int k = 0; k < 3; k++) { o->eye[k] = eye[k]; o->target[k] = eye[k] + d[k] * (dist > 0 ? dist : 1); }
    }
    /* 0037C8B4..C0: authored angles retained alongside the look point. */
    o->pitch = pitch; o->heading = yaw;
    o->roll = roll;
    o->fov = lens;
    o->shot = c->shot;
    gt4_pregrid_projection(lens, &o->sx, &o->sy);
    if (done) c->shot = -1;   /* +0xE8 = 3: the manager picks the next */
    return 1;
}

int gt4_tvcam_step(gt4_tvcam *c, const gt4_tvcam_target *t, gt4_tvcam_view *out)
{
    if (!c || !t || !out) return 0;
    /* EE FCR31 rounds toward zero */
    int mode = fegetround();
    fesetround(FE_TOWARDZERO);
    int ok = step(c, t, out);
    fesetround(mode);
    return ok;
}

/* ------------------------------------------------------------------
   The replay's Dive mode (the selector's type 8, func_00376638): a
   director cutting between shots. Only its most likely shot is here:
   the point camera (+0x170, func_0037E4D8 / func_0037EA78) standing at
   one of the course's camera points (REP4 +0xB0: +0x2C count, +0x30
   entries of 0x30, sorted by start: +4 position, +0x10/+0x14 the span
   it sees, +0x24 30) and turning to the car. The director's other picks
   (its indices 0..7: the cameras +0x174, +0x180, +0x188, func_00376238)
   are not ported; they fall back to this one (A). */

/* 0036F710 / 0036F778: a cubic from 0 to 1 with the slopes b/a at 0 and
   d/c at 1, t kept in 0..1 */
typedef struct { float c10, c14, c18; } dive_cubic;
static dive_cubic cubic(float a, float b, float c, float d)
{
    dive_cubic k;
    float s0 = b / a, s1 = d / c;
    k.c18 = s0;
    k.c14 = s0 * -2 - s1 + 3;
    k.c10 = s0 + s1 - 2;
    return k;
}
static float cubic_at(const dive_cubic *k, float t)
{
    t = fmaxf(0, fminf(1, t));
    return ((k->c10 * t + k->c14) * t + k->c18) * t;
}

/* 00385190 / 003851D0: a spring whose value may change by at most its
   ratios a frame (the lens: 0.91..1.03) */
typedef struct { float v, x, vel, k, c, gain, low, high; } ratio_spring;
static void ratio_init(ratio_spring *s, float v, float k, float c, float gain, float low, float high)
{
    if (v < .001f) v = .001f;
    s->v = s->x = v; s->vel = 0; s->k = k; s->c = c; s->gain = gain; s->low = low; s->high = high;
}
static float ratio_step(ratio_spring *s, float x)
{
    float was = s->v;
    s->x = x;
    s->vel += (s->k * (x - was) + s->c * s->vel) / 60 * s->gain;
    s->v = was + s->vel / 60;
    if (s->v < .001f) s->v = .001f;
    if (s->v / was < s->low) s->v = was * s->low;
    if (s->high < s->v / was) s->v = was * s->high;
    return s->v;
}

typedef struct {
    const uint8_t *point;
    float pos[3], start, end, at;      /* +0x1EC: the point's distance */
    int first, frames;                 /* +0x180, +0x140 */
    float duration;                    /* +0x198: the time to the next cut when it started */
    int zoom_out;                      /* +0x1C0: starts wide */
    int mode;                          /* +0x1C4: 0 the span's lens, 1 to the point then held */
    int lens_n; float lens_x[3], lens_y[3];   /* +0x1C8 */
    float point_lens;                  /* +0x1F4 */
    float roll_to, roll_from, roll_span; dive_cubic roll_curve;   /* +0x1FC */
    dive_cubic lift_curve;             /* +0x228 */
    float roll_kick;                   /* +0x224 */
    float lift0, lift1;                /* +0x244, +0x248 */
    float fade0, fade1;                /* +0x24C, +0x250 */
    spring lead, frame_a, frame_b, roll;   /* +0x53C, +0x4F4, +0x518, +0x584 */
    ratio_spring lens;                 /* +0x4B0 */
    operator_spring pitch, heading;    /* +0x258, +0x384 */
} dive_point_cam;

/* 00384908 / 00384928 / 00384A20: a hand's shake, a target redrawn now
   and then (-128..128), followed by a spring; its value -1..1 */
typedef struct { float pos, target, vel, k; } dive_shake;
static void shake_init(dive_shake *h, float k) { h->pos = h->target = h->vel = 0; h->k = k * 115; }
static void shake_step(dive_shake *h, uint32_t *rng, float scale, float chance)
{
    if (chance <= 0) chance = .3f;
    if (random01(rng) < chance) h->target = (random01(rng) * 2 - 1) * 128;
    h->vel += (scale * h->k * (h->target - h->pos) + h->vel * -10) / 60;
    h->pos += h->vel / 60;
}
static float shake_value(const dive_shake *h) { return fmaxf(-1, fminf(1, h->pos * (1.f / 128))); }

/* 0036F7C0 / 0036F7D8: two segments from (0,0) through (x1,y1), (x2,y2) to (1,1) */
typedef struct { float x1, x2, y1, y2; } dive_bend;
static float bend_at(const dive_bend *b, float t)
{
    float x0 = 0, y0 = 0, x1 = b->x1, y1 = b->y1;
    t = fmaxf(0, fminf(1, t));
    if (x1 < t) { x0 = x1; y0 = y1; x1 = b->x2; y1 = b->y2; }
    if (x1 < t) { x0 = b->x2; y0 = b->y2; x1 = 1; y1 = 1; }
    if (x1 <= x0) return 0;
    return y0 + (y1 - y0) * (t - x0) / (x1 - x0);
}


typedef struct {
    int frames, variant, first;
    float total;                       /* +0x188: the shot's frames */
    float fixed, ease, bent;           /* +0x19C, +0x1D4, +0x1E8 */
    dive_bend bend;                    /* +0x1D8 */
    float lens, roll0, roll1;          /* +0x200, +0x204, +0x208 */
    float start, sweep, dir, offset;   /* +0x1EC, +0x1F0, +0x1F4, +0x1FC */
    float aim[2][3], eye[2][3];        /* +0x1BC/+0x1C8 the aims, +0x1A0/+0x1AC the eyes, about the car */
    float last_height;                 /* +0x1B8 */
    dive_shake shake[2];               /* +0x20C, +0x21C */
} dive_orbit;

/* the still cameras (+0x174, kinds 4..6: func_00381D88, 003821F0,
   00382708; their frame does nothing, func_00382C28) */
typedef struct { float eye[3], pitch, heading, roll, lens, focus; int frames; } dive_still;

/* the helicopter (+0x188, kind 7: func_003834A0 / func_003837C0) */
typedef struct {
    float eye[2][3], aim[2][3];        /* +0x2D8/+0x2E4, +0x2F0/+0x2FC: about the car */
    float roll, lens, duration;        /* +0x2D4, +0xC, +0x308 */
    int frames, first;
    operator_spring heading;           /* +0x1A8 */
    dive_shake shake[2];               /* +0x188, +0x198 */
} dive_heli;

/* the colour filter (display +0xBC0, func_00400758): the colour shown
   and the one it goes to, as rgb, saturation, offset, amount (+0x4..0x18
   and +0x1C..0x30 in the order r g b offset amount saturation); its
   state (+0x38) 0 none, 1 a tone, 2 a strong tint; fading (+0x3C bit 0) */
typedef struct { float now[6], to[6]; int state, fading; } dive_filter;

struct gt4_dive {
    const uint8_t *data, *points;
    size_t size;
    unsigned count;
    float length;
    uint32_t rng;
    float timer;                       /* +0xB80 +0x18 (func_00400A18) */
    int cut, running;      /* running: 1 the point camera, 2 a camera on the car */
    int car_point, car_frames, car_last;   /* the car's camera (kind 0): its point, frames run, the last one */
    float car_fov, car_want;
    /* its roll (func_00379FA8, +0x238; +0x264 on): from roll[1] to roll[0]
       along the curve within the first `span` of the shot */
    int car_roll_on;
    float car_roll[2], car_roll_span;
    dive_cubic car_roll_curve;
    /* its hand (+0x265 on, points 0 and 5): two drifts (+0x1B8, +0x1C4)
       with their own draws (A: the camera's random state +0x1B4 taken
       apart from the director's) */
    int car_shake_on;
    float car_drift[2][2];
    uint32_t car_rng;
    dive_point_cam cam;
    dive_orbit orbit;
    dive_still still;
    dive_heli heli;
    dive_filter filter;
};

static void wall_check(const gt4_dive_target *tg, float margin, const float from[3], int id, float eye[3]);
static int ground_id(const gt4_dive_target *tg, const float p[3]);

gt4_dive *gt4_dive_open(const uint8_t *course, size_t size)
{
    if (!course || size < 0xb4) return NULL;
    uint32_t at = u32(course + 0xb0);
    if (!at || at > size || size - at < 0x34 || memcmp(course + at, "REP4", 4)) return NULL;
    gt4_dive *d = calloc(1, sizeof *d);
    if (!d) return NULL;
    d->data = course + at;
    d->size = size - at;
    d->count = u32(d->data + 0x2c);
    d->length = f32(d->data + 0x14);
    uint32_t list = u32(d->data + 0x30);
    if (!d->count || list > d->size || d->count > (d->size - list) / 0x30 || !(d->length > 0)) { free(d); return NULL; }
    d->points = d->data + list;
    d->rng = 0x7654321u;   /* A: the PS2's random state is not known */
    d->timer = 8;          /* func_004009F0 */
    d->car_last = -1;
    d->car_rng = 0x1234567u;
    for (int k = 0; k < 6; k++) d->filter.now[k] = d->filter.to[k] = k == 4 ? 0 : 1;   /* func_00400758 */
    return d;
}
void gt4_dive_close(gt4_dive *d) { free(d); }
float gt4_dive_length(const gt4_dive *d) { return d ? d->length : 0; }
void gt4_dive_reset(gt4_dive *d)
{
    if (!d) return;
    d->running = 0; d->timer = 8; d->cut = 0;
    memset(&d->filter, 0, sizeof d->filter);   /* func_00400758 */
    for (int k = 0; k < 6; k++) d->filter.now[k] = d->filter.to[k] = k == 4 ? 0 : 1;
}

/* 003FF380: the distance moved by a lap into the span, if that helps */
static float lap_into(float d, float start_d, float end_d, float length)
{
    if (start_d <= d && d <= end_d) return d;
    if (start_d <= d - length && d - length <= end_d) return d - length;
    if (start_d <= d + length && d + length <= end_d) return d + length;
    return d;
}

/* func_0037EA08: from the point to the course's line at distance d
   (func_0034C518 line 6), 30 without one */
static float course_distance(const gt4_dive_target *tg, const float pos[3], float d)
{
    float p[3];
    if (!tg->course_point || !tg->course_point(tg->ctx, d, p)) return 30;
    return sqrtf((p[0] - pos[0]) * (p[0] - pos[0]) + (p[1] - pos[1]) * (p[1] - pos[1]) + (p[2] - pos[2]) * (p[2] - pos[2]));
}

/* func_0037E9A8 / func_0037E908: a lens (degrees) keeping a size picked
   by r (0..1) at the distance */
static float framing_size(float r)
{
    return (((r * .428571f - .428571f) * r * r) + r) * 10 + 1.5f;
}
static float lens_for(float r, float distance)
{
    return atanf(framing_size(r) / (2 * distance)) * 2 / 0.017453292f;
}

/* func_0037E4D8: the point camera set on a point */
static void point_start(gt4_dive *d, const uint8_t *point, float duration)
{
    dive_point_cam *c = &d->cam;
    float r;
    memset(c, 0, sizeof *c);
    c->point = point;
    for (int k = 0; k < 3; k++) c->pos[k] = f32(point + 4 + k * 4);
    c->start = f32(point + 0x10);
    c->end = f32(point + 0x14);
    c->duration = duration;
    c->first = 1;
    c->zoom_out = !(random01(&d->rng) < .7f);
    {
        dive_cubic lift = cubic(.01f, .027f, .01f, .022f);
        c->lift_curve = cubic(.1f, random01(&d->rng) * .3f, 1, .01f);
        c->lift0 = cubic_at(&lift, random01(&d->rng)) * .8f;
        c->lift1 = cubic_at(&lift, random01(&d->rng)) * .8f;
    }
    c->roll_curve = cubic(.1f, .3f, 1, .01f);
    c->roll_to = c->roll_from = 0;
    c->roll_span = .5f;
    if (random01(&d->rng) < .6f) {
        c->roll_curve = cubic(.1f, random01(&d->rng) * .3f, 1, .01f);
        c->roll_to = random01(&d->rng) * 40 - 20;
        c->roll_from = random01(&d->rng) * 40 - 20;
        c->roll_span = random01(&d->rng) * .5f + .2f;
    }
    c->roll_kick = 0;
    if (random01(&d->rng) < .15f) c->roll_kick = random01(&d->rng) * 200 - 100;
    c->fade0 = random01(&d->rng) * .2f + .5f;
    c->fade1 = 1 - random01(&d->rng) * .2f;
    if (random01(&d->rng) < .5f) { c->fade0 = 1; c->fade1 = 1.05f; }
    r = random01(&d->rng); (void)r;           /* +0x254: the focus' (not drawn) */
    random01(&d->rng);
    random01(&d->rng);                        /* +0x150 */
}

/* 0036EA50 / 0036EA98: pitch and heading (degrees) of a direction */
static float pitch_of(const float v[3]) { return -atan2f(v[1], sqrtf(v[0] * v[0] + v[2] * v[2])) * 57.29578f; }
static float heading_of(const float v[3]) { return (3.1415927f - atan2f(v[0], v[2])) * 57.29578f; }

/* func_0037EA78: one frame of the point camera; 0 once the car left its span */
static int point_step(gt4_dive *d, const gt4_dive_target *tg, gt4_tvcam_view *o)
{
    dive_point_cam *c = &d->cam;
    float car = into_span(c->start, c->end, d->length, tg->distance);
    float span = c->end - c->start, along, progress, aim[3], v[3], pv[3], dist, pitch, heading, lens = 90, s, roll;
    int out = 0;
    along = (car - c->start) / span;
    if (along < 0) { along = 0; out = 1; }
    if (along > 1) { along = 1; out = 1; }
    progress = along;
    if (0 < c->duration) progress = fmaxf(0, fminf(1, (float)c->frames / (c->duration * 60)));
    memcpy(aim, tg->pos, sizeof aim);
    aim[1] += c->lift0 + (c->lift1 - c->lift0) * cubic_at(&c->lift_curve, along);
    for (int k = 0; k < 3; k++) v[k] = aim[k] - c->pos[k];
    dist = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    pitch = pitch_of(v);
    heading = heading_of(v);
    if (c->first) {
        /* the lens: a size picked at the point's distance, then (00FC758,
           D_006212C0 even) through the span or to the point and held */
        float mid = (c->start + c->end) * .5f;
        float at = mid, rel, sz;
        /* func_003955E8: the point's own distance (its place on the
           course); A: taken as its span's middle, where the savestates
           show it (rv_04: 425.30 of 365.3..485.3) */
        float d_start = course_distance(tg, c->pos, c->start);
        float d_mid = course_distance(tg, c->pos, at);
        float d_end = course_distance(tg, c->pos, c->end);
        c->at = at;
        rel = (at - c->start) / span;
        sz = 2 / framing_size(random01(&d->rng));
        c->point_lens = atanf(1 / (sz * d_mid)) * 2 / 0.017453292f;
        c->mode = random01(&d->rng) < .5f ? 0 : 1;
        if (c->mode == 0) {
            c->lens_n = 2;
            c->lens_x[0] = 0; c->lens_y[0] = lens_for(random01(&d->rng), d_start);
            c->lens_x[1] = 1; c->lens_y[1] = lens_for(random01(&d->rng), d_end);
        } else {
            c->lens_n = 3;
            c->lens_x[0] = 0; c->lens_y[0] = lens_for(random01(&d->rng), d_start);
            c->lens_x[1] = rel; c->lens_y[1] = c->point_lens;
            c->lens_x[2] = 1; c->lens_y[2] = c->point_lens;
        }
    }
    /* 0036F880: the lens along the span */
    if (c->mode == 0 || car < c->at) {
        int i = 0;
        while (i + 2 < c->lens_n && c->lens_x[i + 1] <= along) i++;
        float x0 = c->lens_x[i], x1 = c->lens_x[i + 1];
        lens = c->lens_y[i] + (c->lens_y[i + 1] - c->lens_y[i]) * (x1 > x0 ? (along - x0) / (x1 - x0) : 0);
    } else lens = c->point_lens;
    lens = fmaxf(2, fminf(70, lens));
    /* the lead: the turn since the last frame, held back by a spring */
    {
        float lead, now, before;
        for (int k = 0; k < 3; k++) pv[k] = tg->prev[k] - c->pos[k];
        before = heading_of(pv);
        now = heading_of(v);
        lead = fmaxf(-.4f, fminf(.4f, angle_diff(now, before)));
        if (c->first) spring_init(&c->lead, -lead, 20, -20, 1, 2, 120, 120);
        else spring_step(&c->lead, -lead);
    }
    /* the framing let go toward the shot's end */
    s = 1;
    if (c->fade0 < progress) s = fmaxf(.001f, fminf(1, 1 - (progress - c->fade0) / (c->fade1 - c->fade0)));
    if (.95f < along || along < .01f) s = 0;
    if (c->first) {
        spring_init(&c->frame_a, s, 50, -20, 1, 1.5f, 100, 100);
        spring_init(&c->frame_b, s, 50, -20, 1, 1.5f, 100, 100);
    } else { spring_step(&c->frame_a, s); spring_step(&c->frame_b, s); }
    heading -= c->lead.v * lens * .5f;
    pitch = pitch - sinf(pitch * 3.1415927f / 180) * lens * .5f * .3f;   /* 0036EBE8 */
    if (c->first) {
        operator_init(&c->pitch, &d->rng, pitch, c->frame_a.v);
        operator_init(&c->heading, &d->rng, heading, c->frame_b.v);
        spring_init(&c->roll, c->roll_kick, 30, -10, 1, 0, 0, 900);
        if (c->zoom_out) lens = fmaxf(2, fminf(90, lens * (random01(&d->rng) * 15 + 3)));
        ratio_init(&c->lens, lens, 100, -20, 1, .91f, 1.03f);
    } else {
        operator_step(&c->pitch, &d->rng, pitch, c->frame_a.v, 1);
        operator_step(&c->heading, &d->rng, heading, c->frame_b.v, 1);
        spring_step(&c->roll, 0);
        ratio_step(&c->lens, lens);
    }
    pitch = c->pitch.pos;
    heading = c->heading.pos;
    /* 0036F980: the roll's wander over its share of the shot */
    roll = c->roll_from + (c->roll_to - c->roll_from) * fmaxf(0, fminf(1, cubic_at(&c->roll_curve, progress / c->roll_span)));
    roll += c->roll.v;
    lens = c->lens.v;
    c->first = 0;
    c->frames++;
    {
        float p = pitch * 0.017453292f, h = heading * 0.017453292f;
        float dir[3] = { sinf(3.1415927f - h) * cosf(p), -sinf(p), cosf(3.1415927f - h) * cosf(p) };
        for (int k = 0; k < 3; k++) { o->eye[k] = c->pos[k]; o->target[k] = c->pos[k] + dir[k] * (dist > 0 ? dist : 1); }
    }
    o->roll = roll;
    o->fov = lens;
    o->shot = (int)((c->point - d->points) / 0x30);
    if (getenv("OPENGT4_DIVE_LOG") && c->frames % 20 == 1)
        fprintf(stderr, "dive pt %d cam %.1f %.1f %.1f car %.1f %.1f %.1f d %.1f span %.1f..%.1f along %.2f pitch %.1f head %.1f lens %.1f\n", o->shot,
                c->pos[0], c->pos[1], c->pos[2], tg->pos[0], tg->pos[1], tg->pos[2], tg->distance, c->start, c->end, along, pitch, heading, lens);
    gt4_pregrid_projection(lens, &o->sx, &o->sy);
    return !out;
}

/* func_00375F58 with func_003FF3E0 / func_003FF1B0: the points whose
   span holds the car, scored by when the car reaches the point and
   leaves within the time wanted, the best three drawn 70/25/5 */
static int point_pick(gt4_dive *d, const gt4_dive_target *tg, float want)
{
    typedef struct { const uint8_t *p; int flags; float score; } cand;
    cand list[16];
    int n = 0, order[16], kept = 0;
    float car = fmodf(tg->distance, d->length), speed = fmaxf(.1f, tg->speed);
    float back = fminf(500, d->length * .25f);
    if (car < 0) car += d->length;
    int last = -1;
    for (unsigned i = 0; i < d->count; i++) if (f32(d->points + i * 0x30 + 0x10) <= car) last = (int)i;
    if (last < 0) return 0;
    float last_start = f32(d->points + last * 0x30 + 0x10);
    for (int i = last; i >= 0 && n < 16; i--) {
        const uint8_t *p = d->points + i * 0x30;
        float a = f32(p + 0x10), b = f32(p + 0x14);
        if (last_start - a >= back) break;
        if (!(a <= car && car < b)) continue;
        float at = (a + b) * .5f;   /* func_003955E8 falls back to the middle (A: always) */
        float cd = lap_into(car, a, b, d->length);
        float reach = fmaxf(0, (at - cd) / speed), leave = (b - cd) / speed;
        cand c = { p, 0, 0 };
        if (want / 10 < reach) c.flags |= 8;
        if (reach < want - want / 10) c.flags |= 0x10;
        /* func_003FF1B0 */
        if (want < leave) c.score -= (leave - want) * .5f;
        if (0 < reach && want < reach) c.score -= (reach - want) * 4;
        {
            float in = fmaxf(.01f, at - a), past = fmaxf(.01f, b - at);
            float to = fmaxf(0, at - cd), beyond = fmaxf(0, cd + speed * want - at);
            to /= in; beyond /= past;
            if (1 < to) c.score -= to - 1;
            if (1 < beyond) c.score -= (beyond - 1) / 3;
            c.score -= fabsf(to - beyond) * 3;
        }
        list[n++] = c;
    }
    for (int pass = 0; pass < 2 && !kept; pass++)
        for (int i = 0; i < n; i++) {
            if (!pass && !((list[i].flags & 8) && (list[i].flags & 0x10))) continue;
            int j = kept;
            while (j > 0 && list[order[j - 1]].score < list[i].score) { order[j] = order[j - 1]; j--; }
            order[j] = i;
            kept++;
        }
    if (!kept) return 0;
    {
        static const float bands[3] = { .7f, .95f, 2 };   /* D_006210A8 */
        float r = fmaxf(0, fminf(1, random01(&d->rng)));
        int k = 0;
        while (k < 2 && bands[k] < r) k++;
        if (k >= kept) k = 0;
        point_start(d, list[order[k]].p, want);
    }
    return 1;
}

/* 00381428: an angle eased within its quarter */
static float quarter_ease(float a)
{
    float t = fmaxf(0, fminf(1, a / 90));
    return ((.128571f * t - .557143f) * t + 1.428571f) * t * 90;
}

/* the car's frame turned by its heading alone (func_003FCEA8 with the
   flag 2): world = the car's place + Ry(heading) v, Ry as the GS's
   (func_004A79B0) */
static void yaw_apply(float yaw, const float v[3], float out[3])
{
    float c = cosf(yaw), sn = sinf(yaw);
    out[0] = c * v[0] + sn * v[2];
    out[1] = v[1];
    out[2] = -sn * v[0] + c * v[2];
}

/* func_00380E48: the orbiting camera (+0x180) for kinds 1..3 (variant
   0..2), its draws in the game's order */
static void orbit_start(gt4_dive *d, const gt4_dive_target *tg, int variant, float duration)
{
    dive_orbit *c = &d->orbit;
    float f21, f22, f23, f15, radius, h, w, front, rear, depth, span;
    memset(c, 0, sizeof *c);
    c->frames = 0;
    c->total = duration * 60;
    c->variant = 0;
    c->ease = .3f;
    c->sweep = 720; c->dir = 1; c->lens = 60;
    shake_init(&c->shake[0], .35f);
    shake_init(&c->shake[1], .35f);
    c->fixed = random01(&d->rng);
    c->ease = random01(&d->rng) * .3f;
    f22 = random01(&d->rng) * .3f;
    f21 = random01(&d->rng) * .3f + .7f;
    f23 = random01(&d->rng) * f22;
    f15 = random01(&d->rng) * .3f + .7f;
    if (f15 < f21) f15 = f21;
    c->bend.x1 = f22; c->bend.x2 = f21; c->bend.y1 = f23; c->bend.y2 = f15;
    c->bent = random01(&d->rng);
    c->lens = random01(&d->rng) * 35 + 25;
    c->roll1 = random01(&d->rng) * 40 - 20;
    c->roll0 = random01(&d->rng) * 40 - 20;
    span = fminf(duration, 5);
    c->start = angle_wrap(random01(&d->rng) * 360);
    if (variant == 0 || variant == 2) {
        c->sweep = random01(&d->rng) * 220 * span * .6f + 100;
        c->variant = variant;
    } else {
        c->sweep = random01(&d->rng) * 160 + 20;
        c->variant = 1;
    }
    c->dir = 1;
    if (random01(&d->rng) < .5f) c->dir = -1;
    c->offset = random01(&d->rng) * 360;
    /* the car's size (func_003F3708..3748: params +0x0 and +0x4 about
       +0x50, +0x8, +0xC) */
    front = tg->size[0]; rear = tg->size[1]; w = tg->size[2]; h = tg->size[3];
    radius = sqrtf(front * front + w * w);
    if (radius < sqrtf(rear * rear + w * w)) radius = sqrtf(rear * rear + w * w);
    if (radius < w) radius = w;
    c->aim[1][0] = (random01(&d->rng) * w - w * .5f) * .7f;
    c->aim[1][1] = random01(&d->rng) * h * .8f;
    c->aim[1][2] = random01(&d->rng) * 3 - 1.5f;
    c->aim[0][0] = (random01(&d->rng) * w - w * .5f) * .7f;
    c->aim[0][1] = random01(&d->rng) * h * .8f;
    c->aim[0][2] = random01(&d->rng) * 3 - 1.5f;
    depth = fmaxf(fabsf(c->aim[1][2]), fabsf(c->aim[0][2]));
    {
        float near = (1 - (c->lens - 25) / 35) * .8f + .2f;
        c->eye[0][1] = random01(&d->rng) * 2 - 1;
        c->eye[0][2] = near * random01(&d->rng) * 4 + depth + radius + .2f;
        c->eye[1][1] = random01(&d->rng) * 3 - 1.5f;
        c->eye[1][2] = random01(&d->rng) * 4 * near + depth + radius + .2f;
    }
    c->first = 1;
}

/* func_003814A0: a frame of the orbit */
static int orbit_step(gt4_dive *d, const gt4_dive_target *tg, gt4_tvcam_view *o)
{
    dive_orbit *c = &d->orbit;
    int bent = c->bent < .3f;
    dive_cubic ease = cubic(.1f, c->ease, 1, .01f);
    float u, w, roll, aim[3], eye[3], off[3], v[3], a, dist, pitch, heading;
    if (c->frames < 0x7ffffffe) c->frames++;
    shake_step(&c->shake[0], &d->rng, 1, .1f);
    shake_step(&c->shake[1], &d->rng, 1, .1f);
    u = 1 - fmaxf(0, fminf(1, (float)c->frames / c->total));
    if (bent) u = bend_at(&c->bend, u);
    if (c->variant == 2) u = c->fixed;
    w = 1 - cubic_at(&ease, 1 - u);
    if (bent) u = w;
    roll = c->roll1 + (c->roll0 - c->roll1) * w;
    for (int k = 0; k < 3; k++) {
        aim[k] = c->aim[1][k] + (c->aim[0][k] - c->aim[1][k]) * w;
        eye[k] = c->eye[1][k] + (c->eye[0][k] - c->eye[1][k]) * w;
    }
    a = angle_wrap(c->start + u * c->sweep * c->dir + c->offset);
    {
        int q = (int)a / 90;
        if (q == 0) a = quarter_ease(a);
        else if (q == 1) a = 180 - quarter_ease(180 - a);
        else if (q == 2) a = quarter_ease(a - 180) + 180;
        else if (q == 3) a = 360 - quarter_ease(360 - a);
        else a = quarter_ease(a - 360);
    }
    a = angle_wrap(a - c->offset);
    /* the eye: around the aim at the orbit's angle, in the car's heading */
    {
        float local[3], r[3], ea = -a * 0.017453292f, ce = cosf(ea), se = sinf(ea);
        off[0] = 0; off[1] = eye[1]; off[2] = eye[2];
        r[0] = ce * off[0] + se * off[2]; r[1] = off[1]; r[2] = -se * off[0] + ce * off[2];
        for (int k = 0; k < 3; k++) local[k] = aim[k] + r[k];
        yaw_apply(tg->yaw, local, v);
        for (int k = 0; k < 3; k++) eye[k] = tg->place[k] + v[k];
    }
    /* func_003FE7B0 from the car (0.3), then func_003FE358: kept 0.2
       above the ground */
    wall_check(tg, .3f, tg->pos, ground_id(tg, tg->pos), eye);
    {
        float ground;
        if (tg->ground && tg->ground(tg->ctx, eye[0], eye[1] + 3, eye[2], &ground)) {
            if (eye[1] < ground + .2f) eye[1] = ground + .2f;
            c->last_height = eye[1];
        } else if (!c->first) eye[1] = c->last_height;
        else c->last_height = eye[1];
    }
    yaw_apply(tg->yaw, aim, v);
    for (int k = 0; k < 3; k++) { aim[k] = tg->place[k] + v[k]; v[k] = aim[k] - eye[k]; }
    dist = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    heading = heading_of(v);
    pitch = fmaxf(-40, fminf(40, pitch_of(v)));
    if (c->variant == 1 || c->variant == 2) {
        pitch += shake_value(&c->shake[0]) * c->lens / 20;
        heading += shake_value(&c->shake[1]) * c->lens / 20;
    }
    c->first = 0;
    {
        float p = pitch * 0.017453292f, h = heading * 0.017453292f;
        float dir[3] = { sinf(3.1415927f - h) * cosf(p), -sinf(p), cosf(3.1415927f - h) * cosf(p) };
        for (int k = 0; k < 3; k++) { o->eye[k] = eye[k]; o->target[k] = eye[k] + dir[k] * (dist > 0 ? dist : 1); }
    }
    o->roll = roll;
    o->fov = c->lens;
    o->shot = -1;
    gt4_pregrid_projection(c->lens, &o->sx, &o->sy);
    if (getenv("OPENGT4_DIVE_LOG") && c->frames % 30 == 1)
        fprintf(stderr, "dive orbit v%d eye %.1f %.1f %.1f car %.1f %.1f %.1f yaw %.2f a %.0f dist %.1f lens %.0f size %.2f %.2f %.2f %.2f\n", c->variant,
                eye[0], eye[1], eye[2], tg->place[0], tg->place[1], tg->place[2], tg->yaw, a, dist, c->lens, tg->size[0], tg->size[1], tg->size[2], tg->size[3]);
    return 1;
}

/* the car's frame turned by its heading (func_003FCEA8, flag 2) */
static void car_frame(const gt4_dive_target *tg, const float local[3], float out[3])
{
    float v[3];
    yaw_apply(tg->yaw, local, v);
    for (int k = 0; k < 3; k++) out[k] = tg->place[k] + v[k];
}

/* func_003FCEA8 with flags 7 (func_003FCE70): turned by the car's
   pitch, heading and roll */
static void car_frame_full(const gt4_dive_target *tg, const float local[3], float out[3])
{
    for (int k = 0; k < 3; k++)
        out[k] = tg->place[k] + tg->rot[k * 3] * local[0] + tg->rot[k * 3 + 1] * local[1] + tg->rot[k * 3 + 2] * local[2];
}

/* func_003FE358 for the still cameras: 0.1 above the ground and up to
   0.6 more; 0 without ground */
static int still_ground(gt4_dive *d, const gt4_dive_target *tg, float eye[3])
{
    float ground;
    if (!tg->ground || !tg->ground(tg->ctx, eye[0], eye[1] + 3, eye[2], &ground)) return 0;
    eye[1] = ground + .1f + random01(&d->rng) * .6f;
    return 1;
}

static void view_from(gt4_tvcam_view *o, const float eye[3], float pitch, float heading, float roll, float lens, float dist)
{
    float p = pitch * 0.017453292f, h = heading * 0.017453292f;
    float dir[3] = { sinf(3.1415927f - h) * cosf(p), -sinf(p), cosf(3.1415927f - h) * cosf(p) };
    for (int k = 0; k < 3; k++) { o->eye[k] = eye[k]; o->target[k] = eye[k] + dir[k] * (dist > 0 ? dist : 1); }
    o->roll = roll;
    o->fov = lens;
    o->shot = -1;
    gt4_pregrid_projection(lens, &o->sx, &o->sy);
}

/* the car's place a time ahead from its move of a frame */
static void ahead_of(const gt4_dive_target *tg, float seconds, float out[3])
{
    for (int k = 0; k < 3; k++) out[k] = tg->pos[k] + (tg->pos[k] - tg->prev[k]) * seconds * 60;
}

/* func_003FE7B0 from a point toward the eye: the eye pulled back from
   a wall, margin short of it; *id the ground triangle under from */
static void wall_check(const gt4_dive_target *tg, float margin, const float from[3], int id, float eye[3])
{
    float out[3];
    if (!tg->wall) return;
    if (tg->wall(tg->ctx, margin, from, eye, out, &id) && getenv("OPENGT4_DIVE_LOG"))
        fprintf(stderr, "dive wall: %.1f %.1f %.1f -> %.1f %.1f %.1f\n", eye[0], eye[1], eye[2], out[0], out[1], out[2]);
    memcpy(eye, out, sizeof out);
}

static int ground_id(const gt4_dive_target *tg, const float p[3])
{
    return tg->ground_id ? tg->ground_id(tg->ctx, p) : 0;
}

/* func_00381D88 (kind 4): low on the car's way a little ahead (5..20 %
   of the time, half a second at most), looking on along it; held back
   from the walls seen from the course's line at its own distance (0.5).
   What fails still draws its angles. */
static int still_ahead(gt4_dive *d, const gt4_dive_target *tg, float duration)
{
    dive_still *c = &d->still;
    float ahead = fminf(.5f, duration * (random01(&d->rng) * .15f + .05f)), v[3], from[3], eye[3];
    int ok = 0, id;
    c->lens = random01(&d->rng) * 30 + 50;
    ahead_of(tg, ahead, c->eye);
    c->eye[0] += random01(&d->rng) * 2 - 1;
    c->eye[1] = tg->pos[1] + 3;
    c->eye[2] += random01(&d->rng) * 2 - 1;
    if (tg->distance_of && tg->course_point && tg->course_point(tg->ctx, tg->distance_of(tg->ctx, c->eye), from)
        && (id = ground_id(tg, from)) >= 0) {
        memcpy(eye, c->eye, sizeof eye);
        wall_check(tg, .5f, from, id, eye);
        if (still_ground(d, tg, eye)) { memcpy(c->eye, eye, sizeof eye); ok = 1; }
    }
    for (int k = 0; k < 3; k++) v[k] = c->eye[k] - tg->pos[k];
    c->pitch = random01(&d->rng) * 20 - 15;
    c->heading = heading_of(v) + random01(&d->rng) * 20 - 10;
    c->roll = random01(&d->rng) * 30 - 15;
    c->focus = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    return ok;
}

/* func_003821F0 (kind 5): beside the course (2..7 m, either side) where
   the car will be in 50..85 % of the time, looking back along it; held
   back from the walls seen from the course (1.2); only with the car
   within its lens. Its sideways step takes the heading's degrees as
   radians, as the game's does. */
static int still_side(gt4_dive *d, const gt4_dive_target *tg, float duration)
{
    dive_still *c = &d->still;
    float time = duration * (random01(&d->rng) * .35f + .5f), step = tg->speed / 60, at, a[3], b[3], v[3], h, side, eye[3];
    int ok = 0, id;
    c->lens = random01(&d->rng) * 30 + 50;
    at = tg->distance + step * time * 60;
    if (!tg->course_point || !tg->course_point(tg->ctx, at, a) || !tg->course_point(tg->ctx, at - 1, b)) return 0;
    for (int k = 0; k < 3; k++) v[k] = b[k] - a[k];
    h = heading_of(v);
    c->focus = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    side = random01(&d->rng) * 5 + 2;
    if (random01(&d->rng) < .5f) side = -side;
    c->eye[0] = a[0] - cosf(h) * side;
    c->eye[1] = a[1];
    c->eye[2] = a[2] + sinf(h) * side;
    if ((id = ground_id(tg, a)) >= 0) {
        memcpy(eye, c->eye, sizeof eye);
        wall_check(tg, 1.2f, a, id, eye);
        if (still_ground(d, tg, eye)) { memcpy(c->eye, eye, sizeof eye); ok = 1; }
    }
    c->pitch = random01(&d->rng) * 20 - 15;
    c->heading = h + random01(&d->rng) * 20 - 10;
    c->roll = random01(&d->rng) * 30 - 15;
    for (int k = 0; k < 3; k++) v[k] = tg->pos[k] - c->eye[k];
    if (fabsf(angle_diff(heading_of(v), c->heading)) > c->lens * .5f) ok = 0;
    return ok;
}

/* func_00382708 (kind 6): by the car (its width out to a side, along
   its length), held back from the walls seen from the car (0.3), looking
   where it will be in 1..3 s with half the lens aside, so that it
   crosses the picture */
static int still_by(gt4_dive *d, const gt4_dive_target *tg, float duration)
{
    dive_still *c = &d->still;
    float local[3], to[3], v[3], car[3], sign = 1, length, eye[3];
    int ok = 0;
    c->lens = random01(&d->rng) * 30 + 50;
    length = tg->size[0] + tg->size[1];
    local[0] = tg->size[2] * (random01(&d->rng) * 1.5f + .7f);
    local[1] = 0;
    local[2] = length * random01(&d->rng);
    if (random01(&d->rng) < .5f) local[0] = -local[0];
    if (random01(&d->rng) < .5f) local[2] = -local[2];
    car_frame_full(tg, local, c->eye);
    memcpy(eye, c->eye, sizeof eye);
    wall_check(tg, .3f, tg->pos, ground_id(tg, tg->pos), eye);
    if (still_ground(d, tg, eye)) { memcpy(c->eye, eye, sizeof eye); ok = 1; }
    if (fabsf(tg->pos[1] - c->eye[1]) > 5) ok = 0;
    ahead_of(tg, (random01(&d->rng) * 2 + 1), to);
    for (int k = 0; k < 3; k++) { v[k] = to[k] - c->eye[k]; car[k] = tg->pos[k] - c->eye[k]; }
    c->focus = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (car[2] * v[0] - car[0] * v[2] < 0) sign = -1;
    c->pitch = random01(&d->rng) * 20 - 15;
    c->heading = heading_of(v) + c->lens * sign * .5f;
    c->roll = random01(&d->rng) * 30 - 15;
    return ok;
}

static int still_step(gt4_dive *d, gt4_tvcam_view *o)
{
    dive_still *c = &d->still;
    view_from(o, c->eye, c->pitch, c->heading, c->roll, c->lens, c->focus);
    return 1;
}

/* func_003834A0: the helicopter's draws */
static void heli_start(gt4_dive *d, float duration)
{
    dive_heli *c = &d->heli;
    float r, a;
    memset(c, 0, sizeof *c);
    shake_init(&c->shake[0], 1);
    shake_init(&c->shake[1], 1);
    for (int i = 0; i < 2; i++) {
        r = random01(&d->rng) * 6 + 1;
        a = random01(&d->rng) * 6.2831855f;
        c->eye[i][0] = r * sinf(a);
        c->eye[i][2] = r * cosf(a);
        c->eye[i][1] = random01(&d->rng) * 4 + 3;
    }
    c->duration = duration < .01f ? 1 : duration;
    for (int i = 0; i < 2; i++) {
        c->aim[i][0] = random01(&d->rng) * 1.5f - .75f;
        c->aim[i][1] = random01(&d->rng) * .6f + .4f;
        c->aim[i][2] = random01(&d->rng) * 3 - 1.5f;
    }
    c->roll = random01(&d->rng) * 40 - 20;
    c->lens = random01(&d->rng) * 30 + 40;
    c->first = 1;
}

/* func_003837C0 */
static int heli_step(gt4_dive *d, const gt4_dive_target *tg, gt4_tvcam_view *o)
{
    dive_heli *c = &d->heli;
    dive_cubic ease = cubic(.1f, .3f, 1, .01f);
    float t = fmaxf(0, fminf(1, (float)c->frames / 60 / c->duration)), w = cubic_at(&ease, t);
    float eye[3], aim[3], le[3], la[3], v[3], pitch, heading, dist;
    for (int k = 0; k < 3; k++) {
        le[k] = c->eye[0][k] + (c->eye[1][k] - c->eye[0][k]) * w;
        la[k] = c->aim[0][k] + (c->aim[1][k] - c->aim[0][k]) * w;
    }
    car_frame(tg, le, eye);
    car_frame(tg, la, aim);
    {   /* func_003FE7B0 from the car at the aim's height (0.3) */
        float from[3] = { tg->pos[0], aim[1], tg->pos[2] };
        wall_check(tg, .3f, from, ground_id(tg, tg->pos), eye);
    }
    for (int k = 0; k < 3; k++) v[k] = aim[k] - eye[k];
    dist = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    pitch = pitch_of(v);
    heading = heading_of(v);
    if (c->first) operator_init(&c->heading, &d->rng, heading, .2f);
    else operator_step(&c->heading, &d->rng, heading, .2f, 0);
    heading = c->heading.pos;
    shake_step(&c->shake[0], &d->rng, 1, -1);
    shake_step(&c->shake[1], &d->rng, 1, -1);
    heading += shake_value(&c->shake[1]) * .7f;
    pitch += shake_value(&c->shake[0]) * .7f;
    c->first = 0;
    c->frames++;
    view_from(o, eye, pitch, heading, c->roll, c->lens, dist);
    return 1;
}

/* func_00376238 (kind 0): one of the car model's camera points (the
   camera +0x158): 3 in 10 the point 0, 3 in 10 the point 5, else one of
   those the model has (func_003FF688, 26 at most), never the last one
   again; on 0 and 5 a lens of 60..80 degrees (+0x188), held for the
   time wanted (+0x150 with 3). A: its other draws (+0x170, the 0x100
   and 0x8000 flags, func_00379FA8) are not read. */
static int car_pick(gt4_dive *d, const gt4_dive_target *tg, float want)
{
    int list[26], n = 0, idx = -1;
    if (!tg->has_point) return 0;
    for (int i = 0; i < 26; i++) if (tg->has_point(tg->ctx, i)) list[n++] = i;
    if (!n) return 0;
    float r = random01(&d->rng);
    if (r < .3f) idx = 0;
    else if (r < .6f) idx = 5;
    else {
        int k = (int)(random01(&d->rng) * (float)n);
        idx = list[k < 0 ? 0 : k > n - 1 ? n - 1 : k];
    }
    if (!tg->has_point(tg->ctx, idx)) idx = list[0];
    if (idx == d->car_last) {   /* func_0037BC48: on to the next point */
        for (int k = 0; k < n; k++) if (list[k] > idx) { idx = list[k]; break; }
        if (idx == d->car_last) idx = list[0];
    }
    d->car_point = d->car_last = idx;
    d->car_fov = 0;
    d->car_frames = 0;
    d->car_want = want;
    d->car_roll_on = 0;
    if (idx == 0 || idx == 5) {
        d->car_fov = random01(&d->rng) * 20 + 60;
        random01(&d->rng);   /* < 0.5: +0x269 (A: what it does is not read) */
    }
    if (random01(&d->rng) < .4f) {   /* func_00379FA8: a roll of +-20 degrees, held on points 0, 5, 14..17 */
        int moving = !(idx == 0 || idx == 5 || (idx >= 14 && idx < 18));
        d->car_roll_curve = cubic(.1f, random01(&d->rng) * .3f, 1, .01f);
        d->car_roll[0] = random01(&d->rng) * 40 - 20;
        d->car_roll[1] = random01(&d->rng) * 40 - 20;
        if (!moving) d->car_roll[1] = d->car_roll[0];
        d->car_roll_span = random01(&d->rng) * .5f + .2f;
        d->car_roll_on = 1;
    }
    d->car_shake_on = random01(&d->rng) < .3f && (idx == 0 || idx == 5);   /* +0x265 */
    return 2;
}

/* func_00376638: a new shot. D_006210B8 (the next cut within 2 s),
   D_006210E0, D_00621108 (over 7.8 s, or no mark) weigh its nine
   kinds: 0 a camera on the car, 1..3 the orbit, 4..6 still cameras, 7
   the helicopter, 8 the point camera; a kind that cannot be placed, or
   no point holding the car, draws again from D_00621108 */
static int direct(gt4_dive *d, const gt4_dive_target *tg)
{
    static const float tables[3][9] = {
        { .07f, .07f, .09f, .04f, .04f, .03f, .05f, .04f, 1.57f },
        { .06f, .05f, .11f, .04f, .02f, 0, .03f, .04f, 1.65f },
        { .22f, .22f, .22f, .22f, 0, 0, 0, 1.12f, 2 },
    };
    const float *t = tables[d->timer > 7.8f ? 2 : d->timer < 2 ? 0 : 1];
    for (int tries = 0; tries < 4; tries++) {
        float r = random01(&d->rng);
        int kind = 0;
        while (kind < 8 && (r -= t[kind]) > 0) kind++;
        if (getenv("OPENGT4_DIVE_KIND")) kind = atoi(getenv("OPENGT4_DIVE_KIND"));   /* tests: one kind only */
        if (kind == 0 && car_pick(d, tg, d->timer)) return 2;
        if (kind >= 1 && kind <= 3) { orbit_start(d, tg, kind - 1, d->timer); return 3; }
        if (kind == 4 && still_ahead(d, tg, d->timer)) return 4;
        if (kind == 5 && still_side(d, tg, d->timer)) return 4;
        if (kind == 6 && still_by(d, tg, d->timer)) return 4;
        if (kind == 7) { heli_start(d, d->timer); return 5; }
        if (kind == 8 && point_pick(d, tg, d->timer)) return 1;
        t = tables[2];   /* 0x00376A94: drawn again from D_00621108 */
    }
    if (point_pick(d, tg, d->timer)) return 1;
    return car_pick(d, tg, d->timer);
}

/* func_004004A8: the filter drawn on a cut, by the state it is in: none
   (D_006226E8), a tone of D_00622700's 21 or a tint of D_006228F8's 12
   (r g b, saturation, offset, amount), or the same one kept */
static void filter_pick(gt4_dive *d)
{
    static const float none[6] = { 1, 1, 1, 1, 0, 1 };
    static const float tones[21][6] = {
        { 1.36f, .94f, .7f, 1, .03f, .9f }, { 1.2f, .9f, .9f, 1, .03f, .9f }, { 1, .9f, 1.1f, 1, .03f, .9f },
        { .8f, 1.04f, 1.16f, 1, .03f, .9f }, { .9f, .94f, 1.16f, 1, .03f, .9f },
        { 1.36f, .94f, .7f, 1, 0, .6f }, { 1.2f, .9f, .9f, 1, 0, .6f }, { 1, .9f, 1.1f, 1, 0, .6f },
        { .8f, 1.04f, 1.16f, 1, 0, .6f }, { .9f, .94f, 1.16f, 1, 0, .6f },
        { 1.22f, .85f, .63f, 1, .03f, 1 }, { 1.08f, .81f, .81f, 1, .03f, 1 }, { .9f, .81f, .99f, 1, .03f, 1 },
        { .72f, .94f, 1.04f, 1, .03f, 1 }, { .81f, .85f, 1.04f, 1, .03f, 1 },
        { .8f, .65f, .55f, .1f, -.1f, 1 }, { 1, 1, 1, .7f, 0, .7f }, { 1, 1, 1, 0, 0, .85f },
        { 1, 1, 1, 1, 0, .5f }, { 1, 1, 1, .7f, .03f, .5f }, { 1, 1, 1, .5f, .05f, .5f },
    };
    static const float tints[12][6] = {
        { 1.3f, .4f, .3f, 1, 0, 1 }, { 1, .6f, .35f, 1, 0, 1 }, { .3f, .64f, 1, 1, 0, 1 }, { .35f, .44f, 1.4f, 1, 0, 1 },
        { .9f, .4f, .3f, 1, 0, .5f }, { .7f, .45f, .45f, 1, 0, .5f }, { .45f, .45f, .7f, 1, 0, .5f }, { .35f, .4f, .9f, 1, 0, .5f },
        { 1.1f, .4f, .3f, .5f, .05f, .3f }, { .9f, .45f, .45f, .5f, .05f, .3f }, { .45f, .45f, .9f, .5f, .05f, .3f }, { .35f, .4f, 1.1f, .5f, .05f, .3f },
    };
    dive_filter *f = &d->filter;
    float none_p = .25f, tone_p = .3f, tint_p = .05f, r;
    const float *to;
    if (f->state == 0) { none_p = 0; tone_p = .35f; tint_p = .15f; }
    else if (f->state == 1) { tone_p = .25f; tint_p = .15f; }
    else if (f->state == 2) tone_p = .15f;
    else none_p = 2;
    r = random01(&d->rng);
    if ((r -= none_p) < 0) { f->state = 0; to = none; }
    else if ((r -= tone_p) < 0) { int i = (int)(random01(&d->rng) * 21); f->state = 1; to = tones[i < 0 ? 0 : i > 20 ? 20 : i]; }
    else if ((r -= tint_p) < 0) { int i = (int)(random01(&d->rng) * 12); f->state = 2; to = tints[i < 0 ? 0 : i > 11 ? 11 : i]; }
    else return;
    if (getenv("OPENGT4_DIVE_FILTER")) {   /* tests: <1 tone | 2 tint>:<index> */
        int table = atoi(getenv("OPENGT4_DIVE_FILTER")), i = strchr(getenv("OPENGT4_DIVE_FILTER"), ':') ? atoi(strchr(getenv("OPENGT4_DIVE_FILTER"), ':') + 1) : 0;
        f->state = table;
        to = table == 1 && i >= 0 && i < 21 ? tones[i] : table == 2 && i >= 0 && i < 12 ? tints[i] : none;
    }
    memcpy(f->to, to, sizeof f->to);   /* func_00400458 */
}

/* func_004007D8, each frame: on a cut the filter drawn again, a fifth of
   the time faded into (by 1.5..5.5 % a frame), else at once */
static void filter_step(gt4_dive *d, int cut)
{
    dive_filter *f = &d->filter;
    random01(&d->rng);
    if (cut) {
        memcpy(f->now, f->to, sizeof f->now);
        filter_pick(d);
        f->fading = random01(&d->rng) < .2f;
    }
    if (f->fading) {
        float t = random01(&d->rng) * .04f + .015f;
        for (int k = 0; k < 6; k++) f->now[k] += (f->to[k] - f->now[k]) * t;
    } else memcpy(f->now, f->to, sizeof f->now);
}

void gt4_dive_filter(const gt4_dive *d, float out[6])
{
    for (int k = 0; k < 6; k++) out[k] = d ? d->filter.now[k] : k == 4 ? 0 : 1;
}

int gt4_dive_step(gt4_dive *d, const gt4_dive_target *t, gt4_tvcam_view *out)
{
    if (!d || !t || !out) return 0;
    int mode = fegetround(), ok = 0;
    fesetround(FE_TOWARDZERO);
    /* func_00400A18: the time to the music's next mark (at most 100),
       copied within 8 s; beyond, or with none, a countdown from 8 s whose
       end cuts; a mark going by cuts */
    {
        float next = t->cue < 100 ? t->cue : 100;
        if (next > 8) { if ((d->timer -= 1.f / 60) < 0) { d->timer = 8; d->cut = 1; } }
        else d->timer = next;
        if (t->cue_passed) d->cut = 1;
    }
    filter_step(d, !d->running || d->cut);
    /* func_00376AF0: the director runs only on a cut (or a new camera
       type); a shot is timed to the next cut and holds its end until then */
    if (!d->running || d->cut) {
        d->running = direct(d, t);
        if (getenv("OPENGT4_DIVE_LOG")) fprintf(stderr, "dive shot: camera %d for %.2f s\n", d->running, d->timer);
    }
    d->cut = 0;
    if (d->running == 2) {   /* the caller draws the car's camera */
        memset(out, 0, sizeof *out);
        out->shot = d->car_point;
        out->fov = d->car_fov;
        {   /* func_0037A458 0x0037A6DC: the drifts follow a target redrawn 30% of the
               frames (func_00384A70), as fast as the car goes between 10 and 80 km/h */
            float v2 = t->speed * t->speed, amp = fminf(fmaxf(0, v2 - 7.71605f), 493.827f) / 493.827f;
            for (int i = 0; i < 2; i++) {
                float *dr = d->car_drift[i];
                if (random01(&d->car_rng) < .3f) dr[1] = random01(&d->car_rng) * 2 - 1;
                dr[0] += (dr[1] - dr[0]) * amp * .4f;
            }
            if (d->car_shake_on) {   /* func_0037B020 (point 0): 0.9 and 0.3 degrees; func_0037B580 (5): 0.7 (A: its flag 4 not read) */
                float a = d->car_point == 0 ? .9f : .7f, b = d->car_point == 0 ? .3f : .7f;
                out->pitch = fmaxf(-1, fminf(1, d->car_drift[0][0])) * a;
                out->heading = fmaxf(-1, fminf(1, d->car_drift[1][0])) * b;
            }
        }
        if (d->car_roll_on) {   /* func_0037A458 0x0037AB98: the frames over the time asked, then func_0036F980 */
            float t = d->car_want > 0 ? fmaxf(0, fminf(1, (float)d->car_frames / (d->car_want * 60))) : 0;
            float w = fmaxf(0, fminf(1, cubic_at(&d->car_roll_curve, fminf(1, t / d->car_roll_span))));
            out->roll = d->car_roll[1] + (d->car_roll[0] - d->car_roll[1]) * w;
        }
        d->car_frames++;
        ok = 2;
    }
    else if (d->running == 3) ok = orbit_step(d, t, out);
    else if (d->running == 4) ok = still_step(d, out);
    else if (d->running == 5) ok = heli_step(d, t, out);
    else if (d->running == 1) { point_step(d, t, out); ok = 1; }   /* past its span it goes on looking */
    fesetround(mode);
    return ok;
}
