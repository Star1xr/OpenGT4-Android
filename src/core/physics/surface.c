/* Tyre against the surface (func_00367148): three per-wheel terms from
   tables per surface class (WHEEL_B4, WHEEL_B8, WHEEL_BC; used by the
   vibration and sound), a contact countdown, and the tyre wear they add up
   to (func_003438C8). */
#include "car.h"

#include <math.h>

#define K(bits) gt4_f32(bits)
#define ONE K(0x3f800000u)

#define SURFACE_FLAGS  0x006224b0u   /* 3 tables of 40-byte records per class: +0x00, +0xa0, +0x140 */
#define SURFACE_CURVES 0x00843a40u   /* built at run time: curve blocks of 0x84 bytes per class */
#define LOOSE_SURFACE  0x00844050u   /* gain, spin curve points */
#define CLASS_TABLE    0x0069f7b0u

static float ps2_sqrt(float x) { return x == 0 ? x : sqrtf(fabsf(x)); }

/* surface kind -> class: 0 plain, 1 and 2 two rough classes */
static int surface_class(uint8_t kind)
{
    uint32_t t;
    if ((uint8_t)(kind - 2) >= 0x15) return 0;
    t = GT4_U32(CLASS_TABLE + (kind - 2) * 4);
    return t == 0x00367240u ? 1 : t == 0x00367248u ? 2 : 0;
}

/* One table: two curves (x chosen by flag +0x00, y by flag +0x1f), their
   vector length, a slope above a threshold and an offset when nonzero. */
struct feel { float value, above; };
static struct feel feel_term(uint32_t flags, uint32_t curves, int cls, float a, float b, float c, float d, float w40)
{
    uint32_t e = curves + cls * 0x84 + 0x4c;   /* the second curve of the pair */
    float x = GT4_U8(flags) ? a : b;
    float c1 = gt4_curve_eval(gt4_ptr(curves + cls * 0x84), x), c2, r;
    struct feel f;
    float y = GT4_U8(flags + 0x1f) ? c : d;
    c2 = gt4_curve_eval(gt4_ptr(e), y);
    f.above = w40 - GT4_F32(e + 0x10);
    r = ps2_sqrt(c2 * c2 + c1 * c1);
    f.value = r;
    return f;
}

void gt4_car_surface_feel(uint32_t car)
{
    uint32_t layout = gt4_car_layout(car);
    float total = 0, wear;

    for (int i = 0; i < GT4_U8(layout + LAYOUT_WHEELS); i++) {
        uint32_t w = gt4_car_wheel(car, i);
        uint8_t kind;
        int cls;
        float slip, along, across, sa, ca, t;

        if (GT4_F32(w + WHEEL_LOAD) != 0) {
            GT4_U8(w + WHEEL_CONTACT_TIMER) = 6;
        } else if (GT4_U8(w + WHEEL_CONTACT_TIMER)) {
            GT4_U8(w + WHEEL_CONTACT_TIMER) = GT4_U8(w + WHEEL_CONTACT_TIMER) - 1;
        }
        if (!GT4_U8(w + WHEEL_CONTACT_TIMER)) {
            GT4_F32(w + WHEEL_BC) = 0;
            GT4_F32(w + WHEEL_B8) = ONE;
            GT4_F32(w + WHEEL_B4) = 0;
            continue;
        }
        kind = GT4_U8(w + WHEEL_SURFACE);
        cls = surface_class(kind);

        slip = GT4_F32(w + WHEEL_SLIP);
        if (slip < 0) slip = -slip;
        sa = gt4_sinf(GT4_F32(w + WHEEL_STEER));
        ca = gt4_cosf(GT4_F32(w + WHEEL_STEER));
        along = sa * GT4_F32(w + WHEEL_VEL_Y) + ca * GT4_F32(w + WHEEL_VEL_X);
        if (along < 0) along = -along;
        {
            float s2 = gt4_sinf(GT4_F32(w + WHEEL_STEER)), c2 = gt4_cosf(GT4_F32(w + WHEEL_STEER));
            int neg;
            across = s2 * GT4_F32(w + WHEEL_VEL_X) - c2 * GT4_F32(w + WHEEL_VEL_Y);
            neg = across < 0;
            across = across - GT4_F32(w + WHEEL_SPIN);
            if (neg) across = -across;
        }
        t = GT4_F32(w + WHEEL_38);

        if (kind == 1 || kind == 10 || kind == 11 || kind == 21) {
            /* loose surfaces */
            float load = GT4_F32(w + WHEEL_LOAD) / K(0x45191fffu) - K(0x3e800000u);   /* 2450 N */
            float spin = GT4_F32(w + WHEEL_SPIN), k;
            load = gt4_select(load < 0, 0.0f, gt4_select(ONE < load, ONE, load));
            if (spin < 0) spin = -spin;
            k = (spin - K(0x3fb1c71bu)) * K(0x3e1374bdu);
            k = gt4_select(k < 0, 0.0f, gt4_select(ONE < k, ONE, k));
            GT4_F32(w + WHEEL_B4) = k * load * GT4_F32(LOOSE_SURFACE);
            if (spin <= GT4_F32(LOOSE_SURFACE + 4)) GT4_F32(w + WHEEL_B8) = GT4_F32(LOOSE_SURFACE + 8);
            else if (GT4_F32(LOOSE_SURFACE + 0xc) <= spin) GT4_F32(w + WHEEL_B8) = GT4_F32(LOOSE_SURFACE + 0x10);
            else GT4_F32(w + WHEEL_B8) = GT4_F32(LOOSE_SURFACE + 8)
                + (spin - GT4_F32(LOOSE_SURFACE + 4)) * (GT4_F32(LOOSE_SURFACE + 0x10) - GT4_F32(LOOSE_SURFACE + 8))
                  / (GT4_F32(LOOSE_SURFACE + 0xc) - GT4_F32(LOOSE_SURFACE + 4));
        } else {
            uint32_t e = SURFACE_CURVES + cls * 0x84 + 0x4c;
            struct feel f = feel_term(SURFACE_FLAGS + cls * 40, SURFACE_CURVES, cls, along, slip, across, t, GT4_F32(w + WHEEL_40));
            float v = f.value, v2;
            int settled = 0;
            if (0 < f.above) v = v + f.above * GT4_F32(e + 0x14);
            if (v != 0) v = v + GT4_F32(e + 0xc);
            if (GT4_F32(w + WHEEL_B4) == GT4_F32(e + 0xc) && v != GT4_F32(e + 0xc)) settled = 1;
            GT4_F32(w + WHEEL_B4) = v;

            e = SURFACE_CURVES + 0x210 + cls * 0x84 + 0x4c;
            f = feel_term(SURFACE_FLAGS + 0xa0 + cls * 40, SURFACE_CURVES + 0x210, cls, along, slip, across, t, GT4_F32(w + WHEEL_40));
            v2 = GT4_F32(e + 0xc) + f.value;
            if (0 < f.above) v2 = v2 + f.above * GT4_F32(e + 0x14);
            if (settled && (i & 1)) v2 = v2 * K(0x3f400000u);
            GT4_F32(w + WHEEL_B8) = v2;
        }

        if (kind == 12) cls = 1;
        {
            uint32_t e = SURFACE_CURVES + 0x420 + cls * 0x84 + 0x4c;
            struct feel f = feel_term(SURFACE_FLAGS + 0x140 + cls * 40, SURFACE_CURVES + 0x420, cls, along, slip, across, t, GT4_F32(w + WHEEL_40));
            float v = f.value;
            if (0 < f.above) v = v + f.above * GT4_F32(e + 0x14);
            if (kind == 8) v = 0;
            if (v != 0) v = v + GT4_F32(e + 0xc);
            if (ONE < v) v = ONE;
            GT4_F32(w + WHEEL_BC) = v;
            if (cls) total = total + v;
        }
    }

    /* func_0034C968: session byte (0xff = none); none or zero = four times the wear */
    {
        uint8_t s = GT4_U8(GT4_U32(car + CAR_SESSION) + 0x10000 - 0x3423);
        if (s == 0xff || s == 0) total = total * K(0x40800000u);
    }
    wear = total * K(0x3a03126eu) * GT4_F32(car + CAR_DT);   /* 0.0005 */
    /* func_003438C8 */
    if (!GT4_U8(car + CAR_ENGINE_WEAR + 0x14)) {
        float v = GT4_F32(car + CAR_ENGINE_WEAR + 0xc) + wear;
        GT4_F32(car + CAR_ENGINE_WEAR + 0xc) = v;
        if (K(0x3f4ccccc) < v) GT4_F32(car + CAR_ENGINE_WEAR + 0xc) = K(0x3f4ccccc);
    }
}
