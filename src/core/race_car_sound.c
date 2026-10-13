/* One car's race sound: func_003B5E80 (the frame) and func_00392640 (the
   requests), with the helpers they call. Float arithmetic as the physics:
   toward-zero rounding set by the caller (gt4_phys_fpu_enter), each
   product and sum rounded in the original order. */
#include "race_car_sound.h"
#include "phys.h"
#include "car.h"
#include <string.h>
#include <math.h>

#define K(bits) gt4_f32(bits##u)
#define F(o) (*(float *)(f->b + (o)))
#define I(o) (*(int32_t *)(f->b + (o)))
#define C(o) GT4_F32(0x00843630u + (o))      /* D_00843630: the sound constants */

const uint8_t gt4_car_wheel_effect[11] = { 25, 24, 26, 21, 22, 55, 59, 27, 57, 58, 58 };
const uint8_t gt4_car_tone_effect[5] = { 28, 29, 19, 12, 71 };

/* func_003F3A68, as func_003F3AB8 fills D_00845A10: D_00622330's s16
   times 0x3C23D70A, five camera modes of 24 levels. */
static float level(int mode, int k)
{
    return (float)(int16_t)GT4_U16(0x00622330u + (uint32_t)(mode * 48 + k * 2)) * K(0x3c23d70a);
}

static uint32_t wheel_at(uint32_t car, int w) { return car + 0x164u + (uint32_t)w * 0xecu; }

/* func_00366678: 3 when the car sounds as driven */
static int sound_kind(uint32_t car)
{
    uint32_t a1 = car + 0x104;
    if (!GT4_U8(a1 + 0x441)) return 3;
    int8_t k = (int8_t)GT4_U8(a1 + 0x466);
    if (k == 4) return GT4_U8(a1 + 0x4b2) ? 0 : 2;
    uint32_t a0 = GT4_U32(car);
    if (!GT4_U32(a0 + 0xe0) && !GT4_U32(GT4_U32(a0 + 0x84) + 0xf050u)) return 1;
    if (k == 2) return 0;
    uint8_t m = GT4_U8(a1 + 0x467);
    if (m == 6 || m == 8 || m == 1) return 0;
    return 2;
}

/* func_0034FA90: the rolling noise and the road-noise ENGN's rpm */
static float rolling(uint32_t car, uint16_t *rpm)
{
    if (sound_kind(car) != 3) return 0;
    float v = GT4_F32(car + 0x640), f5 = v < 0 ? -v : v, r;
    float f6 = ((C(0) * GT4_F32(GT4_U32(car + 0x10) + 0x1064)) * v) * v;
    f6 = f6 * (K(0x3f800000) - GT4_F32(car + 0x6a0));
    if (f5 < C(4)) r = C(0xc);
    else if (C(8) < f5) r = C(0x10);
    else r = C(0xc) + ((f5 - C(4)) * (C(0x10) - C(0xc))) / (C(8) - C(4));
    /* cvt.w.s, with the unsigned fix-up above 2^31 */
    *rpm = (uint16_t)(K(0x4f000000) <= r ? (uint32_t)(int32_t)(r - K(0x4f000000)) | 0x80000000u : (uint32_t)(int32_t)r);
    return C(0x14) < f6 ? C(0x14) : f6;
}

/* func_0034FBC0: the wind inside the car */
static float wind(uint32_t car, float *pitch)
{
    if (sound_kind(car) != 3) return 0;
    float v = GT4_F32(car + 0x640), f5 = v < 0 ? -v : v;
    float f6 = ((C(0x18) * GT4_F32(GT4_U32(car + 0x10) + 0x1064)) * v) * v;
    f6 = f6 * (K(0x3f800000) - GT4_F32(car + 0x6a4));
    if (f5 < C(0x1c)) *pitch = C(0x24);
    else if (C(0x20) < f5) *pitch = C(0x28);
    else *pitch = C(0x24) + ((f5 - C(0x1c)) * (C(0x28) - C(0x24))) / (C(0x20) - C(0x1c));
    return C(0x2c) < f6 ? C(0x2c) : f6;
}

/* func_00350DA0: the driven shaft's rpm, by the drive (jump table 0069F650) */
static float shaft(uint32_t car)
{
    uint32_t p = GT4_U32(car + 0x10) + 0x234;
    unsigned kind = GT4_U8(p);
    if (kind >= 11) return 0;
    uint32_t branch = GT4_U32(0x0069f650u + kind * 4);
    float r = GT4_F32(car + 0x634), f = GT4_F32(car + 0x630);
    if (branch == 0x00350dd8u) return r;
    if (branch == 0x00350de0u) return f;
    if (branch == 0x00350de8u) return (f + r) * K(0x3f000000);
    if (branch == 0x00350e08u) return r + GT4_F32(p + 4) * (f - r);
    return 0;
}

/* func_0034FED0 (turbine2): electric (00356D88), hybrid (00369510) or
   the transmission whine. */
static float whine(uint32_t car, float *pitch)
{
    if (sound_kind(car) != 3) return 0;
    uint32_t params = GT4_U32(car + 0x10);
    if (GT4_U8(params + 0x234) == 7) {
        float s = 0;
        for (int w = 0; w < 2; w++) {
            float v = GT4_F32(wheel_at(car, w) + 0x20);
            s = s + (v < 0 ? -v : v);
        }
        s = s * K(0x3f000000);
        *pitch = s * K(0x3c449ba6) + K(0x3ecccccc);
        return GT4_F32(car + 0x620) * K(0x3e800000);
    }
    if (GT4_U8(params + 0x246) == 3) {
        if (GT4_F32(car + 0x590) == 0 || GT4_F32(car + 0x618) == 0) {
            *pitch = K(0x3f800000);
            return 0;
        }
        uint32_t layout = gt4_car_layout(car);
        unsigned group = GT4_U8(layout + 0xc + GT4_U8(params + 0x237));
        unsigned other = GT4_U8(layout + 0x24 + group);
        uint32_t w0 = wheel_at(car, (int)group), w1 = wheel_at(car, (int)other);
        float s = (GT4_F32(w0 + 0x20) + GT4_F32(w1 + 0x20)) * K(0x3f000000);
        if (s < 0) s = -s;
        *pitch = s * K(0x3c449ba6) + K(0x3e999999);
        return (GT4_F32(w0 + 0x64) + GT4_F32(w1 + 0x64)) * K(0x3e4ccccc);
    }
    if (!GT4_U8(params + 0x10d1) || !GT4_U8(car + 0x626)) {
        *pitch = K(0x3f800000);
        return 0;
    }
    float t = GT4_F32(car + 0x620);
    if (t < 0) t = -t;
    if (C(0x58) < t) t = C(0x58);
    float d = C(0x58) * GT4_F32(params + 0x278);
    t = (t * GT4_F32(params + 0x274 + GT4_U8(car + 0x624) * 4u)) / d;
    if (t < 0) t = -t;
    t = C(0x50) + (C(0x54) - C(0x50)) * t;
    float r = shaft(car);
    if (r < 0) r = -r;
    *pitch = C(0x60) + r * C(0x5c);
    return t;
}

/* func_0034FCC0: the tyre on tarmac */
static float on_road(uint32_t car, int w, float *pitch)
{
    if (sound_kind(car) != 3) return 0;
    uint32_t params = GT4_U32(car + 0x10), layout = gt4_car_layout(car), wheel = wheel_at(car, w);
    unsigned s = GT4_U8(wheel + 0x89);
    int on = GT4_U8(wheel + 0xb3) && (s < 2 || (s < 13 && s >= 7));
    if (!on) {
        *pitch = C(0x44);
        return 0;
    }
    unsigned axle = w < GT4_U8(layout + 1) ? GT4_U8(layout + 0x14 + (uint32_t)w) : 0;
    float width = GT4_F32(params + 0x2d4 + axle * 4), scale;
    if (width < K(0x3e0a3d70)) scale = C(0x30);
    else if (K(0x3eb5c28e) < width) scale = C(0x34);
    else scale = C(0x30) + ((width - K(0x3e0a3d70)) * (C(0x34) - C(0x30))) / K(0x3e6147ad);
    float v = GT4_F32(wheel + 0x20);
    if (v < 0) v = -v;
    float q = v / K(0x42c80000);
    float f6 = ((q * q) * C(0x38)) * scale;
    if (v < C(0x3c)) *pitch = C(0x44);
    else if (C(0x40) < v) *pitch = C(0x48);
    else *pitch = C(0x44) + ((v - C(0x3c)) * (C(0x48) - C(0x44))) / (C(0x40) - C(0x3c));
    return C(0x4c) < f6 ? C(0x4c) : f6;
}

/* func_003FA7B8: a wheel landing (on_bound), off the loose surfaces */
static float bound(uint32_t car, int w)
{
    uint32_t wheel = wheel_at(car, w);
    unsigned s = GT4_U8(wheel + 0x89);
    if (s == 1 || s == 10 || s == 11 || s == 2 || s == 3 || s == 4 || s == 6 || s == 13 || s == 14) return 0;
    if (GT4_U8(wheel + 0xdc) != 2) return 0;
    float f = -GT4_F32(wheel + 0xd8);
    if (f < K(0x3f000000)) return 0;
    return K(0x3f800000) < f ? K(0x3f800000) : f;
}

/* func_00344798 family: the car's place at its interpolation plus t */
static float place(uint32_t car, int axis, float t)
{
    t = t + GT4_F32(car + 0x554);
    return GT4_F32(car + 0x8a4 + axis * 4) * t + GT4_F32(car + 0x888 + axis * 4) * (K(0x3f800000) - t);
}

/* func_004A7AA8 with the camera's matrix in VU0 (vmulax/vmadday/vmaddaz/
   vmaddw: each product rounded, the sums in that order). */
static void transform(const float *m, float *v)
{
    float x = v[0], y = v[1], z = v[2], w = v[3];
    for (int k = 0; k < 4; k++) v[k] = ((m[k] * x + m[4 + k] * y) + m[8 + k] * z) + m[12 + k] * w;
}

/* func_00391C28: a turn about y (0057D1F0, 0057D2B8) */
static void turn(float *v, float a)
{
    float x = v[0], z = v[2], c = gt4_sinf(a), s = gt4_cosf(a);
    v[0] = x * c - z * s;
    v[2] = x * s + z * c;
}

void gt4_car_sound_state_reset(gt4_car_sound_state *s)
{
    /* 003924D0: +0xB38 set, the position objects (00391EB0) and the
       smoothed levels cleared. */
    memset(s, 0, sizeof *s);
    s->started = 1;
}

void gt4_car_sound_setup_race(gt4_car_sound_setup *s, uint32_t cond, unsigned mode, float master)
{
    /* 003922B0: bits of the session's +0xCBF0 -> D_0088F2C0 */
    static const uint8_t slot[11] = { 0, 1, 2, 3, 7, 4, 8, 9, 10, 5, 6 };
    uint32_t bits = GT4_U32(cond + 0xcbf0u);
    memset(s, 0, sizeof *s);
    for (int k = 0; k < 11; k++)
        if (bits & (1u << k)) s->wheel_on[slot[k]] = 1;
    /* 00391A60 -> 0039A7A0: D_006A0250 [mode][3] {falloff, near} */
    for (int k = 0; k < 3; k++) {
        s->falloff[k] = GT4_F32(0x006a0250u + mode * 24 + k * 8);
        s->near[k] = GT4_F32(0x006a0254u + mode * 24 + k * 8);
    }
    s->master = master;
}

void gt4_car_sound_frame_build(gt4_car_sound_frame *frame, gt4_car_sound_state *state, gt4_car_sound_out *out,
                               uint32_t entry, const gt4_car_listener *cam, int target, int replay, uint32_t random)
{
    gt4_car_sound_frame *f = frame;
    memset(f, 0, sizeof *f);
    memset(out, 0, sizeof *out);
    F(0x8) = K(0x3f800000);
    int replaying = GT4_U32(entry + 0x198) == 1;
    if (!cam->continuous) {
        /* +0x58..+0xBC: 00392FC8 then 003924D0, a cut restarts */
        gt4_car_sound_state_reset(state);
        out->restart = 1;
    }
    /* A: the second viewport's camera switch (+0xC4..+0x158, cam+0x9B8
       of 2 or more) is for split screen only. */
    uint32_t car = GT4_U32(entry + 0x18), s4 = car + 0x104;
    float one = K(0x3f800000), f22 = one;
    float x = place(car, 0, 0), y = place(car, 1, 0), z = place(car, 2, 0);
    float p[4] = { x, y, z, one };
    transform(cam->view, p);
    I(0) = target;
    if (cam->continuous) {
        /* +0x1FC..+0x37C: the Doppler factor, sound at 0x40B0C5F8 per frame */
        float dx = x - cam->eye_previous[0], dy = y - cam->eye_previous[1], dz = z - cam->eye_previous[2];
        float d = (dx * dx + dy * dy) + dz * dz;
        d = sqrtf(d);
        if (d == 0) d = one;
        dz = dz / d;
        dx = dx / d;
        dy = dy / d;
        float c = dx * (cam->eye_previous[0] - cam->eye[0]);
        c = c + dy * (cam->eye_previous[1] - cam->eye[1]);
        c = c + dz * (cam->eye_previous[2] - cam->eye[2]);
        float t = GT4_F32(GT4_U32(car + 4) + 0xcbf8u);   /* func_00344720 */
        float v = dx * (x - place(car, 0, t));
        v = v + dy * (y - place(car, 1, t));
        v = v + dz * (z - place(car, 2, t));
        c = c + K(0x40b0c5f8);
        v = v + K(0x40b0c5f8);
        f22 = c / v;
    }
    /* +0x380..+0x3FC: the mode by the camera's kind and whether it rides
       this car; the cockpit hears the car from its own origin (D_006A2580) */
    int mode = 4;
    if (cam->type >= 0) {
        if (cam->type < 2) {
            if (!target) mode = 1;
            else {
                mode = 0;
                p[0] = p[1] = p[2] = 0;
                p[3] = one;
            }
        } else if (cam->type == 2)
            mode = target ? 2 : 3;
    }
    uint16_t rpm = GT4_U16(s4 + 0x4fa);
    float aux = 0;
    for (int k = 0; k < 3; k++) memcpy(f->b + 0x10 + k * 16, p, 16);
    if (!mode) {
        /* +0x438..+0x4BC: engine ahead, exhaust behind, turned with the view */
        F(0x18) = F(0x18) + K(0xbfa00000);
        F(0x28) = F(0x28) + K(0xbc23d70a);
        F(0x38) = F(0x38) + K(0x3fa00000);
        float a = -cam->angle;
        for (int k = 0; k < 3; k++) turn((float *)(f->b + 0x10 + k * 16), a);
    }
    unsigned where = GT4_U8(GT4_U32(car + 0x10) + 0x10d2);   /* func_003FA910 */
    I(0x40) = (int32_t)(where != 4 ? where : 1) - 1;
    I(0x44) = 1;
    I(0x48) = 2;
    float r256 = K(0x3c800000);
    F(0x50) = f22;
    F(0x4c) = (level(mode, 0) * (float)GT4_U8(s4 + 0x604)) * r256;
    F(0x54) = (float)rpm;
    F(0x64) = (level(mode, 2) * (float)GT4_U8(s4 + 0x606)) * K(0x3c000000);
    F(0x68) = (float)GT4_U16(s4 + 0x4f8) / K(0x45c00000);
    {
        float l = level(mode, 4), v = whine(car, &aux);
        F(0x6c) = l * v;
        F(0x70) = f22 * aux;
    }
    F(0x78) = f22;
    F(0x74) = (level(mode, 1) * (float)GT4_U8(s4 + 0x605)) * r256;
    F(0x7c) = (float)rpm;
    {
        float l = level(mode, 7), v = wind(car, &aux);
        F(0x80) = l * v;
        F(0x84) = f22 * aux;
    }
    {
        float l = level(mode, 8), v = rolling(car, &rpm);
        F(0x58) = l * v;
    }
    F(0x5c) = f22;
    F(0x60) = (float)rpm;
    F(0x88) = 0;
    F(0x8c) = f22;
    F(0x90) = !mode && GT4_F32(car + 0x5e0) != 0 ? one : 0;   /* 00354EB0: nitrous */
    F(0x94) = one;
    /* +0x6B4..+0x718: backfire, one of two at random (00579358 & 1),
       when the car model posts it (003B6B78: the interface +0x64, read
       and cleared, and the model's exhaust count, vt+0x148). Native
       003D6D30 presentation clock posts it before this sound frame. */
    uint32_t fire = GT4_U32(entry + 0x2890 + 0x64);
    GT4_U32(entry + 0x2890 + 0x64) = 0;
    if (!replaying && fire) {
        int r = (int)(random & 1) + 1;
        if (r == 1) F(0x2c0) = K(0x40400000);
        if (r == 2) F(0x2c4) = K(0x40400000);
    }
    float b = 0;
    if (!mode) {
        for (int w = 0; w < 4; w++) {
            float v = bound(car, w);
            b = b < v ? v : b;   /* max.s */
        }
        b = b * (level(mode, 15) * K(0x40200000));
    }
    F(0x2bc) = b;
    F(0x2c8) = !mode && GT4_U8(GT4_U32(car + 0x10) + 0x32) == 3 ? GT4_F32(car + 0x78c) : 0;   /* 00355950 */
    F(0x2a0) = level(mode, 3) * GT4_F32(s4 + 0x504);
    if (target && !replay) {
        /* 00350500 -> 00350468: the hit's strength, its class consumed */
        int8_t kind = (int8_t)GT4_U8(s4 + 0x60c);
        float h = GT4_F32(s4 + 0x608) * C(0x74);
        if (kind == 2) GT4_U32(s4 + 0x608) = 0;
        h = level(mode, 5) * h;
        if (kind == 0) F(0x2a4) = h;
        else if (kind == 1) F(0x2a8) = h;
        else if (kind == 2) F(0x2ac) = h;
    }
    /* +0x8F8..+0xB0C: each wheel's place and its 11 surface sounds */
    float lv[11];
    lv[0] = level(mode, 0xb);
    lv[1] = level(mode, 9);
    lv[2] = level(mode, 0xa);
    lv[3] = level(mode, 0xe);
    lv[7] = level(mode, 6);
    lv[4] = level(mode, 0xd);
    lv[8] = level(mode, 6);
    lv[9] = level(mode, 6);
    lv[10] = level(mode, 6);
    lv[5] = level(mode, 0xe);
    lv[6] = level(mode, 0xe);
    for (int w = 0; w < 4; w++) {
        uint32_t o = (uint32_t)w * 0x80u, wheel = wheel_at(car, w);
        float q[4];
        /* 00344FC0: the place of the wheel the layout's +0x4C names
           (00344E98), between the previous (+0x7B8 + 52g, the wheel's
           +0x8C) and current (+0x950 + 12g) at the interpolation */
        {
            uint32_t g = (uint32_t)(int32_t)(int8_t)GT4_U8(gt4_car_layout(car) + 0x4c + (uint32_t)w);
            float t = GT4_F32(car + 0x554), u = one - t;
            q[0] = GT4_F32(car + 0x950 + g * 12) * t + GT4_F32(car + 0x7b8 + g * 52) * u;
            q[1] = GT4_F32(car + 0x954 + g * 12) * t + GT4_F32(car + 0x1f0 + g * 0xec) * u;
            q[2] = GT4_F32(car + 0x958 + g * 12) * t + GT4_F32(car + 0x7c0 + g * 52) * u;
            q[3] = one;
        }
        transform(cam->view, q);
        memcpy(f->b + 0xa0 + o, q, 16);
        for (int k = 0; k < 11; k++) {
            F(0xc4 + o + k * 8) = 0;
            F(0xc8 + o + k * 8) = f22;
        }
        unsigned s = GT4_U8(wheel + 0x89);
        float vol = GT4_F32(wheel + 0xb4), pit = GT4_F32(wheel + 0xb8);
#define SLOT(k, cond)                                    \
    if (cond) {                                          \
        F(0xc4 + o + (k) * 8) = lv[k] * vol;             \
        F(0xc8 + o + (k) * 8) = f22 * pit;               \
    }
        SLOT(0, s == 0 || s == 12 || (s >= 7 && s < 10))   /* 0035FFB0 on_skill */
        if (s == 0 || s == 12 || (s >= 7 && s < 10)) {      /* 00360028 on_road */
            float pr = 0, v = on_road(car, w, &pr);
            F(0xcc + o) = lv[1] * v;
            F(0xd0 + o) = f22 * pr;
        }
        SLOT(2, s == 20 || s == 6 || s == 22)              /* 003600E8 rain_noiz */
        SLOT(3, s == 5)                                    /* 00360158 off_roadnoiz */
        SLOT(7, s == 1 || s == 21)                         /* 003601A8 rumble */
        SLOT(4, s >= 2 && s < 5)                           /* 00360200 off_skill */
        SLOT(8, s == 10)                                   /* 003602A0 rumble2 */
        SLOT(9, s == 11)                                   /* 003602C8 rumble3 */
        /* 003602F0 (slot 10) returns at once */
        SLOT(5, s == 13)                                   /* 003602F8 pebble */
        SLOT(6, s == 14)                                   /* 00360320 sand */
#undef SLOT
    }
}

/* func_00391AF0 / 0039A7B0: distance, falloff past `near`, equal-power
   pan, and the phase flags that flip a channel behind the camera. */
static float position(const float *pos, gt4_car_sound_state *s, int which, float falloff, float near,
                      float *left, float *right)
{
    float x = pos[0], z = pos[2], x2 = x * x;
    float d = sqrtf(x2 + z * z);
    if (d < K(0x3e800000)) {
        float zz = (z - K(0x3e800000)) + d;
        d = sqrtf(x2 + zz * zz);
    }
    float m = d - near;
    if (!(0 < m)) m = 0;
    float att = K(0x3f800000) / (m * falloff + K(0x3f800000));
    float a = (x / d + K(0x3f800000)) * K(0x3f490fda);
    if (a < 0) a = 0;
    if (K(0x3fc90fda) <= a) a = K(0x3fc90fda);
    *left = gt4_sinf(a);    /* 0057D1F0 */
    *right = gt4_cosf(a);   /* 0057D2B8 */
    float nx = x / d, nz = pos[2] / d;
    s->direction[which][1] = pos[1];
    s->direction[which][0] = nx;
    s->direction[which][2] = nz;
    int a0 = s->invert[which][0] != 0, a1 = s->invert[which][1] != 0;
    if (nz < 0) {
        if (0 <= nx) a0 = a1;
        else a1 = a0;
    } else {
        if (0 <= nx) a0 = a1 ^ 1;
        else a1 = a0 ^ 1;
    }
    s->invert[which][1] = (uint8_t)a1;
    s->invert[which][0] = (uint8_t)a0;
    if (a0) *left = -*left;
    if (a1) *right = -*right;
    return K(0x3f800000) * att;
}

static int32_t priority(float v)
{
    if (v < 0) v = -v;
    if (K(0x3f800000) <= v) return 0x7ffffffe;
    int32_t p = (int32_t)(v * K(0x4effffff));
    return p > 0x7ffffffe ? 0x7ffffffe : p;
}

static int16_t s14(float v)
{
    int32_t x = (int32_t)(v * K(0x467ffc00));
    return (int16_t)(x < 0x3fff ? (x < -0x3fff ? -0x3fff : x) : 0x3ffe);
}

void gt4_car_sound_requests(const gt4_car_sound_frame *frame, gt4_car_sound_state *s,
                            const gt4_car_sound_setup *setup, gt4_car_sound_out *out)
{
    const gt4_car_sound_frame *f = frame;
    int32_t once = I(4);
    float f27 = setup->master, f26 = f27 * F(0x8);
    float lr[3][2], att[3];
    /* channels: 0 the engine's place (falloff set 0, object +0x14),
       1 the car's middle (set 2, +0x3F4), 2 the exhaust (set 1, +0x2B8) */
    static const int set[3] = { 0, 2, 1 }, obj[3] = { 0, 1, 2 };
    for (int k = 0; k < 3; k++) {
        const float *pos = (const float *)(f->b + 0x10 + I(0x40 + k * 4) * 16);
        float a = position(pos, s, obj[k], setup->falloff[set[k]], setup->near[set[k]], &lr[k][0], &lr[k][1]);
        a = a * f26;
        att[k] = a < K(0xbf800000) ? K(0xbf800000) : K(0x3f800000) < a ? K(0x3f800000) : a;
    }
    /* +0x1A8..+0x2AC: the engine and road-noise ENGN at the engine */
    for (int c = 0; c < 2; c++) {
        float v = att[0] * F(0x4c + c * 12);
        gt4_car_sound_engine *e = out->engine + c;
        e->priority = priority(v);
        e->left = s14(lr[0][0] * v);
        e->right = s14(lr[0][1] * v);
        int32_t sc = (int32_t)(F(0x50 + c * 12) * K(0x45800000));
        e->scale = sc < 0 ? 0 : sc < 0x4000 ? sc : 0x3fff;
        e->reverb_off = once;
        e->rpm = (int16_t)(int32_t)F(0x54 + c * 12);
    }
    /* +0x2B0..+0x354: turbine, turbine2 at the engine */
    for (int c = 0; c < 2; c++) {
        float v = att[0] * F(0x64 + c * 8);
        gt4_car_sound_tone *t = out->tone + GT4_CAR_TURBINE + c;
        t->left = lr[0][0] * v;
        t->right = lr[0][1] * v;
        t->pitch = F(0x68 + c * 8);
        t->priority = priority(v);
        out->tone_on[GT4_CAR_TURBINE + c] = 1;
    }
    /* +0x358..+0x4D0: the exhaust ENGN */
    {
        float v = att[2] * F(0x74);
        gt4_car_sound_engine *e = out->engine + 2;
        e->priority = priority(v);
        e->left = s14(lr[2][0] * v);
        e->right = s14(lr[2][1] * v);
        int32_t sc = (int32_t)(F(0x78) * K(0x45800000));
        e->scale = sc < 0 ? 0 : sc < 0x4000 ? sc : 0x3fff;
        e->reverb_off = once;
        e->rpm = (int16_t)(int32_t)F(0x7c);
    }
    /* +0x4D4..+0x5B0: in_car, body_noiz, nos at the middle, rising at
       once, falling 0x3D4CCCCC a frame */
    for (int c = 0; c < 3; c++) {
        float target = att[1] * F(0x80 + c * 8), l = s->center[c];
        l = l < target ? l + K(0x3ccccccc) : l - K(0x3d4ccccc);
        if (l < target) l = target;
        s->center[c] = l;
        gt4_car_sound_tone *t = out->tone + GT4_CAR_IN_CAR + c;
        t->left = lr[1][0] * l;
        t->right = lr[1][1] * l;
        t->pitch = F(0x84 + c * 8);
        t->priority = priority(l);
        out->tone_on[GT4_CAR_IN_CAR + c] = 1;
    }
    /* +0x5B4..+0x7A0: the wheels, each its own place */
    for (int w = 0; w < 4; w++) {
        float l, r;
        float a = position((const float *)(f->b + 0xa0 + w * 0x80), s, 3 + w, setup->falloff[2], setup->near[2], &l, &r);
        a = a * f26;
        a = a < K(0xbf800000) ? K(0xbf800000) : K(0x3f800000) < a ? K(0x3f800000) : a;
        float g = a * f27;
        for (int k = 0; k < 11; k++) {
            if (!setup->wheel_on[k]) continue;
            float v;
            if (k >= 3) {
                float target = g * F(0xc4 + w * 0x80 + k * 8), x = s->wheel[w][k];
                x = x < target ? x + K(0x3ccccccc) : x - K(0x3d4ccccc);
                if (x < target) x = target;
                s->wheel[w][k] = x;
                v = x;
            } else
                v = g * F(0xc4 + w * 0x80 + k * 8);
            gt4_car_sound_tone *t = out->tone + GT4_CAR_WHEEL0 + w * 11 + k;
            t->left = l * v;
            t->right = r * v;
            t->pitch = F(0xc8 + w * 0x80 + k * 8);
            t->priority = priority(v);
            out->tone_on[GT4_CAR_WHEEL0 + w * 11 + k] = 1;
        }
    }
    /* +0x7A4..+0x824: one-shots over D_006213E4 (0.001), D_00621418 */
    for (int i = 0; i < 11; i++) {
        float v = F(0x2a0 + i * 4);
        if (!(GT4_F32(0x006213e4u) <= v)) continue;
        uint32_t e = GT4_U32(0x00621418u + (uint32_t)i * 8), at = GT4_U32(0x0062141cu + (uint32_t)i * 8);
        float g = v * att[at];   /* 00392EB8 */
        if (g == 0) continue;
        out->shot[out->shots].effect = (uint8_t)e;
        out->shot[out->shots].left = lr[at][0] * g;
        out->shot[out->shots].right = lr[at][1] * g;
        out->shot[out->shots].priority = priority(g);
        out->shots++;
    }
}

/* ---- The course's ambient sound ---- */

void gt4_ambient_state_reset(gt4_ambient_state *s)
{
    memset(s, 0, sizeof *s);
    s->depth = K(0x3e000000);
}

/* CRSS keeps offsets until 00457E78 relocates them; read either way. */
static uint32_t crss_ptr(uint32_t crss, uint32_t value) { return value + (crss - GT4_U32(crss + 4)); }

/* func_00457918: one speaker into its group's sums */
static void speaker(gt4_ambient_state *s, int index, uint32_t sp, uint32_t race, const float *eye, const float *right,
                    const float *vel, float *out)
{
    float one = K(0x3f800000), zero = 0, gate = one;
    float first = GT4_F32(sp + 0x1c), last = GT4_F32(sp + 0x2c);
    if (first != last) {
        /* +0x7C..+0x15C: the cars inside [first, last] of the lap, each
           later one in race order weighing 0.8 as much, up to 1; the
           level follows by 0x3C088888 up, 0x3BB60B60 down a frame */
        int wrap = last < first;
        float w = one;
        gate = 0;
        for (uint32_t e = GT4_U32(GT4_U32(race + 0x60) + 0x14); e; e = GT4_U32(e + 0x10)) {
            uint32_t car = GT4_U32(e + 0x18);
            float t = GT4_F32(car + 0x554);
            float p = GT4_F32(car + 0x99c) * t + GT4_F32(car + 0x5a8) * (one - t);   /* 003447C8 */
            if (!wrap) {
                if (first <= p && p <= last) gate = gate + w;
            } else if (first <= p || p <= last)
                gate = gate + w;
            w = w * K(0x3f4ccccc);
        }
        if (one <= gate) gate = one;
        float l = s->level[index & 511];
        if (gate < l) {
            float d = l - gate;
            l = l - (d < K(0x3bb60b60) ? d : K(0x3bb60b60));
        } else {
            float d = gate - l;
            l = l + (d < K(0x3c088888) ? d : K(0x3c088888));
        }
        s->level[index & 511] = gate = l;
    }
    float sum_l = 0, sum_r = 0, sum_p = 0;
    uint32_t n = GT4_U32(sp);
    for (uint32_t i = 0; i < n; i++) {
        uint32_t p = sp + 0x30 + i * 16;
        float dx = GT4_F32(p) - eye[0], dy = GT4_F32(p + 4) - eye[1], dz = GT4_F32(p + 8) - eye[2];
        float amp = GT4_F32(p + 12) * gate;
        float d = (dx * dx + dy * dy) + dz * dz;
        d = sqrtf(d);
        if (d == zero) d = one;
        dx = dx / d;
        dy = dy / d;
        dz = dz / d;
        float far = one;
        if (!(d < K(0x42000000))) far = d / K(0x42000000);
        amp = amp / far;
        float v = (dx * vel[0] + dy * vel[1]) + dz * vel[2];
        float r = (dx * right[0] + dy * right[1]) + dz * right[2];
        float dop = K(0x40b0c5f8) / (K(0x40b0c5f8) - v);
        r = (r + one) * K(0x3f000000);
        if (r < zero) r = zero;   /* max.s */
        if (one < r) r = one;     /* min.s */
        float a = (one - r) * K(0x3fc90fda);
        r = r + zero;
        sum_p = sum_p + (dop - one) * amp;
        sum_l = sum_l + gt4_cosf(a) * amp;   /* 0057D2B8 */
        sum_r = sum_r + gt4_cosf(r * K(0x3fc90fda)) * amp;
    }
    if (n) sum_p = sum_p / (float)n;
    out[0] = out[0] + sum_l;
    out[1] = out[1] + sum_r;
    out[2] = out[2] + sum_p;
}

void gt4_ambient_groups(gt4_ambient_state *s, uint32_t crss, uint32_t race, const float eye[3],
                        const float right[3], const float velocity[3], float out[5][3])
{
    int index = 0;
    for (int g = 0; g < 5; g++) {
        out[g][0] = out[g][1] = 0;
        out[g][2] = K(0x3f800000);
    }
    for (int g = 0; g < 5; g++) {   /* 00457F60 -> 00457D90 */
        uint32_t list = crss_ptr(crss, GT4_U32(crss + 0x14 + (uint32_t)g * 4));
        int32_t n = (int32_t)GT4_U32(list);
        for (int32_t i = 0; i < n; i++)
            speaker(s, index++, crss_ptr(crss, GT4_U32(list + 4 + (uint32_t)i * 4)), race, eye, right, velocity, out[g]);
        if (n) out[g][2] = (out[g][2] - K(0x3f800000)) / (float)n + K(0x3f800000);
    }
}

static int32_t level_priority(float v)
{
    if (v < 0) v = -v;
    if (K(0x3f800000) <= v) return 0x7ffffffe;
    int32_t p = (int32_t)(v * K(0x4effffff));
    return p > 0x7ffffffe ? 0x7ffffffe : p;
}

static float clamp1(float v) { return v < K(0xbf800000) ? K(0xbf800000) : K(0x3f800000) < v ? K(0x3f800000) : v; }

void gt4_ambient_frame(gt4_ambient_state *s, uint32_t race, const gt4_car_listener *cam, const float right[3],
                       int heri, float master, gt4_ambient_out *out)
{
    memset(out, 0, sizeof *out);
    uint32_t course = GT4_U32(GT4_U32(race + 0x80) + 4), crss = GT4_U32(course + 0x8c);
    if (!crss) return;
    if (!cam->continuous) {
        /* +0x90..+0xB0: 00399300 stops everything, the depth back to
           0.125 (0FFFh); 00398B58 marks it started */
        gt4_ambient_state_reset(s);
        s->started = 1;
        out->depth = 0xfff;
        out->restart = 1;
        return;
    }
    float acc[5][3], g[5][3], vel[3], eye[3];
    for (int k = 0; k < 5; k++) {   /* 00457700 */
        acc[k][0] = acc[k][1] = 0;
        acc[k][2] = K(0x3f800000);
    }
    for (int k = 0; k < 3; k++) {
        eye[k] = cam->eye_previous[k];
        vel[k] = cam->eye_previous[k] - cam->eye[k];
    }
    gt4_ambient_groups(s, crss, race, eye, right, vel, g);
    /* +0x278..+0x31C: the triangle under a point 5 above the eye marks
       tunnels (kind & 0x18: 8 tunnel 1, 10h tunnel 2, 18h the reverb) */
    float t1 = 0, t2 = 0;
    int deep = 0;
    {
        gt4_ray_hit hit;
        float q[3] = { eye[0], eye[1] + K(0x40a00000), eye[2] };
        unsigned kind = gt4_course_ray_down(GT4_U32(course + 0xdc), &hit, q) ? hit.kind & 0x18 : 0;
        if (kind == 8) {
            t1 = K(0x3f800000);
            deep = 2;
        } else if (kind == 0x10) {
            t2 = K(0x3f800000);
            deep = 1;
        } else if (kind == 0x18)
            deep = 1;
    }
    for (int k = 0; k < 5; k++) {   /* 00457738 -> 004576C0 */
        acc[k][0] = acc[k][0] + g[k][0];
        acc[k][1] = acc[k][1] + g[k][1];
        acc[k][2] = acc[k][2] + (g[k][2] - K(0x3f800000));
    }
    /* +0x350..+0x3E4: the depth toward 0.25 per mark (0.125 outside) by
       at most 0x3CE38E3A a frame */
    float target = deep ? (float)deep * K(0x3e800000) : K(0x3e000000), d = target - s->depth;
    if (d < 0) {
        float m = -K(0x3ce38e3a);
        d = d < m ? m : d;
    } else if (K(0x3ce38e3a) < d)
        d = K(0x3ce38e3a);
    s->depth = s->depth + d;
    out->depth = (int16_t)(int32_t)(s->depth * K(0x46fffe00));
    /* +0x3E4..+0x438: air and city, when listed, at their first point's
       level from anywhere; cheer and sea at their own pitch */
    for (int k = 0; k < 2; k++) {
        uint32_t list = crss_ptr(crss, GT4_U32(crss + 0x14 + (uint32_t)k * 4));
        if (GT4_U32(list)) {
            float v = GT4_F32(crss_ptr(crss, GT4_U32(list + 4)) + 0x3c);
            acc[k][0] = acc[k][1] = v;
            acc[k][2] = K(0x3f800000);
        }
    }
    acc[3][2] = acc[4][2] = K(0x3f800000);
    for (int k = 0; k < 5; k++) {
        float l = clamp1(acc[k][0]) * master, r = clamp1(acc[k][1]) * master;
        float m = l < r ? r : l;   /* max.s */
        out->left[k] = l;
        out->right[k] = r;
        out->pitch[k] = acc[k][2];
        out->priority[k] = level_priority(m);
    }
    /* +0x570..+0x6B0: tunnel 1 left and right each on its side; tunnel 2
       crossed, so its tones' own pans silence it */
    t1 = t1 * master;
    t2 = t2 * master;
    out->tunnel[0][0] = t1;
    out->tunnel[1][1] = t1;
    out->tunnel[2][1] = t2;
    out->tunnel[3][0] = t2;
    out->tunnel_priority[0] = level_priority(t1);
    out->tunnel_priority[1] = level_priority(t2);
    out->heri = heri ? K(0x3f800000) : 0;
    out->heri_priority = level_priority(out->heri);
}

/* 00579330 -> 00578598: local exhaust random stream and PS2 float
   construction. It is separate from the race/physics random streams. */
static float backfire_random(gt4_car_backfire *s)
{
 uint32_t seed;memcpy(&seed,s->b+0x68,4);seed=seed*17u+17u;memcpy(s->b+0x68,&seed,4);
 uint32_t value=seed^((seed<<16)|(seed>>16));
 if(!value)return 0;
 unsigned shift=0;uint32_t scan=value;while(!(scan&0x80000000u)){shift++;scan<<=1;}
 /* plzcw returns leading sign bits minus one, with high bit folded
    through sra/or in 00578598 +18..24. */
 int32_t count=(value&0x80000000u)?-1:(int32_t)shift-1;
 uint32_t exponent=(uint32_t)(125-count),bits=(exponent<<23)|((value<<((-exponent)&31u))>>9);
 return gt4_f32(bits);
}
void gt4_car_backfire_init(gt4_car_backfire *s)
{
 if(!s)return;memset(s,0,sizeof *s);uint32_t seed=0x54321098u;memcpy(s->b+0x68,&seed,4);
 /* 003D6C18 +b8..e8: four inactive particles, rising-edge latch zero. */
}
void gt4_car_backfire_step(gt4_car_backfire *s,uint32_t car,float dt)
{
 if(!s || !car)return;
 /* 003F33A8 +8..1c: existing physical signal and authored engine rate. */
 float rate=(int8_t)GT4_U8(car+0x6c4)==1?GT4_F32(GT4_U32(car+0x10)+0x10dc):0;
 uint32_t prior;memcpy(&prior,s->b+0x60,4);uint32_t on=K(0x3dcccccc)<rate,fire=on && !prior;
 memcpy(s->b+0x60,&on,4);
 if(fire && rate<backfire_random(s))fire=0;
 /* 003D6D30 +f0..1ac: advance all four presentation particles. */
 for(int i=0;i<4;i++){
  uint8_t *p=s->b+0x10+i*0x14;uint32_t active;float t,speed;memcpy(&active,p+16,4);
  if(active){memcpy(&t,p,4);memcpy(&speed,p+12,4);t=t+dt*speed;memcpy(p,&t,4);if(1.f<t){memset(p,0,4);memset(p+16,0,4);}continue;}
  if(!fire)continue;
  active=1;memcpy(p+16,&active,4);
  float r=backfire_random(s);speed=1.f/(r*K(0x3dcccccc)+K(0x3dcccccc));memcpy(p+12,&speed,4);
  r=backfire_random(s);t=(-speed)*(r*K(0x3ca3d70a)+K(0x3c23d70a));memcpy(p,&t,4);
  r=backfire_random(s)*K(0x43b40000);memcpy(p+8,&r,4);
  r=backfire_random(s)*K(0x3ecccccc)+K(0x3f4ccccc);memcpy(p+4,&r,4);memcpy(s->b+0x64,&active,4);
 }
}
uint32_t gt4_car_backfire_take(gt4_car_backfire *s)
{
 uint32_t count=0;if(s){memcpy(&count,s->b+0x64,4);memset(s->b+0x64,0,4);}return count;
}
