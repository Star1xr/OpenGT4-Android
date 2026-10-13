#include "glyph_sdf.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Mitchell-Netravali (B = C = 1/3) weights for a sample at fraction f
   between texels 1 and 2: smooth enough that the traced edge follows the
   letter's curves rather than its texel steps (Catmull-Rom kept an "o"
   octagonal), without thinning one-texel strokes below the 0.5 cut. */
static float mitchell(float x)
{
    const float B = 1.f / 3, C = 1.f / 3;
    x = fabsf(x);
    if (x < 1) return ((12 - 9 * B - 6 * C) * x * x * x + (-18 + 12 * B + 6 * C) * x * x + (6 - 2 * B)) / 6;
    if (x < 2) return ((-B - 6 * C) * x * x * x + (6 * B + 30 * C) * x * x + (-12 * B - 48 * C) * x + (8 * B + 24 * C)) / 6;
    return 0;
}
static void cubic_weights(float f, float w[4])
{
    for (int i = 0; i < 4; i++) w[i] = mitchell(f - (i - 1));
}

static float coverage(const uint8_t *rgba, int w, int h, int x, int y)
{
    x = x < 0 ? 0 : x >= w ? w - 1 : x;
    y = y < 0 ? 0 : y >= h ? h - 1 : y;
    return rgba[((size_t)y * w + x) * 4 + 3] / 255.f;
}

/* Squared Euclidean distance transform of one line (Felzenszwalb and
   Huttenlocher): f holds 0 on the feature, a large value elsewhere. */
static void edt_line(float *f, int n, float *d, int *v, float *z)
{
    int k = 0;
    v[0] = 0; z[0] = -INFINITY; z[1] = INFINITY;
    for (int q = 1; q < n; q++) {
        float s = ((f[q] + (float)q * q) - (f[v[k]] + (float)v[k] * v[k])) / (2.f * q - 2.f * v[k]);
        while (s <= z[k]) {
            k--;
            s = ((f[q] + (float)q * q) - (f[v[k]] + (float)v[k] * v[k])) / (2.f * q - 2.f * v[k]);
        }
        k++; v[k] = q; z[k] = s; z[k + 1] = INFINITY;
    }
    k = 0;
    for (int q = 0; q < n; q++) {
        while (z[k + 1] < q) k++;
        float dq = (float)(q - v[k]);
        d[q] = dq * dq + f[v[k]];
    }
    memcpy(f, d, (size_t)n * sizeof *f);
}

/* Squared distance, per pixel, to the nearest pixel where `feature` is set. */
static int edt(const uint8_t *feature,int w,int h,float *out,void (*progress)(void *),void *user)
{
    int n = w > h ? w : h;
    float *f = malloc((size_t)n * sizeof *f), *d = malloc((size_t)n * sizeof *d), *z = malloc((size_t)(n + 1) * sizeof *z);
    int *v = malloc((size_t)n * sizeof *v);
    if (!f || !d || !z || !v) { free(f); free(d); free(z); free(v); return 0; }
    for (size_t i = 0; i < (size_t)w * h; i++) out[i] = feature[i] ? 0 : 1e20f;
    for (int x = 0; x < w; x++) {
        if(progress && !(x%16))progress(user);
        for (int y = 0; y < h; y++) f[y] = out[(size_t)y * w + x];
        edt_line(f, h, d, v, z);
        for (int y = 0; y < h; y++) out[(size_t)y * w + x] = f[y];
    }
    for (int y = 0; y < h; y++) {if(progress && !(y%16))progress(user);edt_line(out + (size_t)y * w, w, d, v, z);}
    free(f); free(d); free(z); free(v);
    return 1;
}

uint8_t *gt4_glyph_sdf(const uint8_t *rgba,int w,int h,int scale,int *out_w,int *out_h)
{return gt4_glyph_sdf_presented(rgba,w,h,scale,out_w,out_h,NULL,NULL);}

uint8_t *gt4_glyph_sdf_presented(const uint8_t *rgba,int w,int h,int scale,int *out_w,int *out_h,void (*progress)(void *),void *user)
{
    if (!rgba || w <= 0 || h <= 0 || scale < 1) return NULL;
    int ow = w * scale, oh = h * scale;
    size_t n = (size_t)ow * oh;
    uint8_t *out = malloc(n * 4), *inside = malloc(n), *outside = malloc(n);
    float *din = malloc(n * sizeof *din), *dout = malloc(n * sizeof *dout);
    if (!out || !inside || !outside || !din || !dout) goto fail;
    for (int y = 0; y < oh; y++) {
        if(progress && !(y%16))progress(user);
        float sy = (y + .5f) / scale - .5f, fy = sy - floorf(sy), wy[4];
        int y0 = (int)floorf(sy);
        cubic_weights(fy, wy);
        for (int x = 0; x < ow; x++) {
            float sx = (x + .5f) / scale - .5f, fx = sx - floorf(sx), wx[4], cubic = 0;
            int x0 = (int)floorf(sx);
            cubic_weights(fx, wx);
            for (int j = 0; j < 4; j++)
                for (int i = 0; i < 4; i++) cubic += wx[i] * wy[j] * coverage(rgba, w, h, x0 + i - 1, y0 + j - 1);
            float linear = (coverage(rgba, w, h, x0, y0) * (1 - fx) + coverage(rgba, w, h, x0 + 1, y0) * fx) * (1 - fy) +
                           (coverage(rgba, w, h, x0, y0 + 1) * (1 - fx) + coverage(rgba, w, h, x0 + 1, y0 + 1) * fx) * fy;
            size_t at = (size_t)y * ow + x;
            inside[at] = cubic >= .5f;
            outside[at] = !inside[at];
            uint8_t c = (uint8_t)(linear * 255 + .5f);
            out[at * 4] = out[at * 4 + 1] = out[at * 4 + 2] = c;
        }
    }
    if (!edt(inside,ow,oh,din,progress,user) || !edt(outside,ow,oh,dout,progress,user)) goto fail;
    /* Signed distance in output pixels (positive inside), half a pixel off
       the pixel centres so the 0.5 level lies on the boundary; mapped to
       0..255 over +-spread. */
    float spread = 2.f * scale;
    for (size_t i = 0; i < n; i++) {
        if(progress && !(i%32768))progress(user);
        float sd = inside[i] ? sqrtf(dout[i]) - .5f : .5f - sqrtf(din[i]);
        float a = .5f + sd / (2 * spread);
        out[i * 4 + 3] = (uint8_t)(255 * (a < 0 ? 0 : a > 1 ? 1 : a) + .5f);
    }
    free(inside); free(outside); free(din); free(dout);
    *out_w = ow; *out_h = oh;
    return out;
fail:
    free(out); free(inside); free(outside); free(din); free(dout);
    return NULL;
}
