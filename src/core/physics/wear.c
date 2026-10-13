/* Tyre temperature and wear (func_00352EA0), when the session turns them
   on (session block + 0x69 heat, + 0x68 wear). */
#include "car.h"

#include <math.h>

#define K(bits) gt4_f32(bits)

enum {
    WHEEL_HEAT = 0xa8,   /* tyre temperature */
    WHEEL_WEAR = 0xac,   /* wear accumulated */
    WHEEL_WEAR_SHOWN = 0xb0,   /* u8 0..255 */
};

static float ps2_sqrt(float x) { return x == 0 ? x : sqrtf(fabsf(x)); }

void gt4_car_tyre_wear(uint32_t car)
{
    uint32_t params = GT4_U32(car + CAR_PARAMS), layout = gt4_car_layout(car);
    uint32_t s = GT4_U32(car + CAR_SESSION) + 4;   /* heat and wear settings */
    float dt = GT4_F32(car + CAR_DT);
    int n = GT4_U8(layout + LAYOUT_WHEELS);

    if (GT4_U8(params + 0x32) == 4) return;
    if (GT4_U8(s + 0x69)) {
        for (int i = 0; i < n; i++) {
            uint32_t w = gt4_car_wheel(car, i);
            int axle = GT4_U8(layout + 0x14 + i);
            float a = GT4_F32(w + 0x70) * GT4_F32(s + 0x34) + GT4_F32(w + 0x74) * GT4_F32(s + 0x38);
            float b = GT4_F32(w + 0x68) * GT4_F32(s + 0x3c);
            float k = GT4_F32(s + 0x4c) / GT4_F32(params + PARAMS_LOAD_REF + (uint32_t)axle * 4);
            float heat = ps2_sqrt(b * b + a * a) * k, t, cool, v;
            if (!GT4_U8(w + WHEEL_SURFACE)) heat = heat + GT4_F32(w + 0xb4) * GT4_F32(s + 0x40);
            t = GT4_F32(w + WHEEL_HEAT);
            cool = ((GT4_F32(s + 0x44) - GT4_F32(s + 0x48)) * (t - GT4_F32(s + 0x50))) / (GT4_F32(s + 0x60) - GT4_F32(s + 0x50))
                   + GT4_F32(s + 0x48);
            v = t + ((heat - cool) * dt) * GT4_F32(s + 0x30);
            GT4_F32(w + WHEEL_HEAT) = v;
            if (v < GT4_F32(s + 0x50)) GT4_F32(w + WHEEL_HEAT) = GT4_F32(s + 0x50);
            else if (GT4_F32(s + 0x60) < v) GT4_F32(w + WHEEL_HEAT) = GT4_F32(s + 0x60);
        }
    }
    if (!GT4_U8(s + 0x68)) return;
    for (int i = 0; i < n; i++) GT4_U8(gt4_car_wheel(car, i) + WHEEL_WEAR_SHOWN) = 0;
    if (GT4_U8(car + 0x5b5) & 0x10) return;
    for (int i = 0; i < n; i++) {
        uint32_t w = gt4_car_wheel(car, i), tp;
        int axle = GT4_U8(layout + 0x14 + i);
        float load = GT4_F32(w + WHEEL_LOAD), r, f;
        if (GT4_U8(w + WHEEL_SURFACE) == 1 || load == 0) continue;
        load = GT4_F32(params + 0x1088 + (uint32_t)axle * 4) + load * GT4_F32(params + 0x1090 + (uint32_t)axle * 4);
        load = gt4_select(load < 0, 0.0f, load);
        tp = gt4_tyre_params(params, i, GT4_U8(w + WHEEL_SURFACE));
        r = GT4_F32(w + 0xb4);
        if (0 < GT4_F32(s + 0x28) && (GT4_U8(w + WHEEL_FLAGS) & 0xc)) {
            float slip = GT4_F32(w + WHEEL_38), lim = GT4_F32(s + 0x2c);
            if (slip <= lim) r = r * (GT4_F32(s + 0x28) + 1.0f);
            else if (slip < 0) r = r * (((GT4_F32(s + 0x28) + 1.0f) * slip) / lim);
        }
        if (GT4_F32(s + 0x24) < r) r = GT4_F32(s + 0x24);
        r = r * GT4_F32(tp + 4);
        f = r * load;
        r = r * 255.0f;
        GT4_F32(w + WHEEL_WEAR) = GT4_F32(w + WHEEL_WEAR) + f * dt;
        if (255.0f < r) r = 255.0f;
        GT4_U8(w + WHEEL_WEAR_SHOWN) = (uint8_t)(int32_t)r;
    }
}
