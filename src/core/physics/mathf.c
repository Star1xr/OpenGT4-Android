/* The game's float math library (from the SDK's libm), kept exact: the
   dynamics depend on its rounding. */
#include <math.h>

#include "phys.h"

#define K(bits) gt4_f32(bits)

/* func_0057D008: atan by range reduction to |x| <= tan(pi/8) and a rational
   approximation. */
float gt4_atanf(float x)
{
    const float one = K(0x3f800000u);
    float t = fabsf(x), r;                 /* abs.s */
    int range = 0;   /* 1: reduced with 1/t, -1: with (1-t)/(1+t) */
    float inv = one / t, mid = (one - t) / (one + t);

    if (K(0x401a827au) < t) {           /* tan(3*pi/8) */
        t = inv;
        range = 1;
    } else if (K(0x3ed413cdu) < t) {    /* tan(pi/8) */
        t = mid;
        range = -1;
    }
    float z = t * t, z2 = z * z, tz = z * t;
    float den = (one + K(0x3f8e38e4u) * z) + K(0x3e73cf3du) * z2;
    float num = (t + K(0x3f471c72u) * tz) + K(0x3d8ab356u) * (tz * z);
    r = num * (one / den);
    if (range < 0) r = K(0x3f490fdbu) - r;        /* pi/4 */
    else if (range > 0) r = K(0x3fc90fdbu) - r;   /* pi/2 */
    uint32_t bits;
    memcpy(&bits, &x, 4);
    return (bits >> 31) ? -r : r;   /* sign bit, as the original tests it */
}

/* func_0057D118 */
float gt4_atan2f(float y, float x)
{
    int flags = 0;
    float r;

    if (x == 0 && y == 0) return 0;
    if (y < 0) { y = -y; flags |= 4; }
    if (x < 0) { x = -x; flags |= 2; }
    if (y < x) r = gt4_atanf(y / x);
    else { r = gt4_atanf(x / y); flags |= 1; }
    if (flags & 1) r = K(0x3fc90fdau) - r;   /* pi/2 (one ulp below the rounded value) */
    if (flags & 2) r = K(0x40490fdau) - r;   /* pi */
    if (flags & 4) r = -r;
    return r;
}

/* Odd polynomial shared by sin and cos, on r reduced to [-pi/2, pi/2]; the
   sign is applied to r before the powers are formed. */
static float sin_poly(float r, float sign)
{
    float z = r * r, rs = r * sign, z2 = z * z;
    float r3 = z * rs, r5 = z2 * rs, r7 = z2 * r3, r9 = r5 * z2;
    float acc = rs + 0.0f;
    acc = acc + K(0xbe2aaaa4u) * r3;
    acc = acc + K(0x3c08873eu) * r5;
    acc = acc + K(0xb94fb21fu) * r7;
    return acc + K(0x362e9c14u) * r9;
}

static int sign_bit(float x)
{
    uint32_t b;
    memcpy(&b, &x, 4);
    return (int)(b >> 31);
}

/* func_0057D1F0 */
float gt4_sinf(float x)
{
    const float half_pi = K(0x3fc90fdbu), pi = half_pi + half_pi;
    int k = (int)(x * K(0x3ea2f983u));   /* 1/pi, truncated */
    int neg = sign_bit(x);
    float n = (float)(k - neg);
    float r = (x - half_pi) - n * pi;
    return sin_poly(r, (neg ^ (~k & 1)) ? -1.0f : 1.0f);
}

/* func_0057D2B8 */
float gt4_cosf(float x)
{
    const float half_pi = K(0x3fc90fdbu), pi = half_pi + half_pi;
    float s = x + half_pi;
    int k = (int)(s * K(0x3ea2f983u));
    int neg = sign_bit(s);
    float n = (float)(k - neg);
    float r = (x + 0.0f) - n * pi;
    return sin_poly(r, (neg ^ (k & 1)) ? -1.0f : 1.0f);
}

/* func_0057D380: tangent, reduced by pi/2 when above pi/4 (an odd count
   inverts), a rational approximation built with the accumulator (each
   product rounded, then added). */
float gt4_tanf(float x)
{
    float z, z2, den, num;
    int odd = 0;
    if (K(0x3f490fdbu) < fabsf(x)) {   /* abs.s */
        int k = (int)(x * K(0x3f22f983u) + (sign_bit(x) ? K(0xbf000000u) : K(0x3f000000u)));
        odd = k & 1;
        x = x - (float)k * K(0x3fc90fdbu);
    }
    z = x * x;
    z2 = z * z;
    den = (K(0x3f800000u) + 0.0f) + K(0xbee38e39u) * z;
    den = den + K(0x3c820821u) * z2;
    num = (x + 0.0f) + K(0xbde38e39u) * (z * x);
    num = num + K(0x3a8ab356u) * (z2 * x);
    if (odd) {
        den = -den;
        if (num == 0) return (sign_bit(den) ^ sign_bit(num)) ? K(0xff7fffffu) : K(0x7f7fffffu);   /* the EE's x/0 */
        return den / num;
    }
    if (den == 0) return (sign_bit(num) ^ sign_bit(den)) ? K(0xff7fffffu) : K(0x7f7fffffu);
    return num / den;
}

/* func_0036CB20: wraps any angle to about [-pi, pi]. */
static float angle_wrap_far(float x)
{
    const float two_pi = K(0x40c90fdau);
    float n = (float)(int)((fabsf(x) + K(0x40490fdau)) / two_pi);   /* abs.s */
    return x < 0 ? x + n * two_pi : x + n * K(0xc0c90fdau);
}

/* func_0036CB90: to [-pi, pi), one turn at a time when close. */
float gt4_angle_wrap(float x)
{
    if (x < K(0xc0490fdau)) {
        if (x < K(0xc116cbe3u)) return angle_wrap_far(x);   /* -3 pi */
        return x + K(0x40c90fdau);
    }
    if (K(0x40490fdau) <= x) {
        if (K(0x4116cbe3u) < x) return angle_wrap_far(x);
        return x - K(0x40c90fdau);
    }
    return x;
}

/* func_0036CC40: a - b, both wrapped, wrapped again. */
float gt4_angle_diff(float a, float b)
{
    float d = gt4_angle_wrap(a) - gt4_angle_wrap(b);
    if (K(0x40490fdau) <= d) return d - K(0x40c90fdau);
    if (d < K(0xc0490fdau)) return d + K(0x40c90fdau);
    return d;
}

/* func_0057D8A0: sine and cosine together, from the same reduction. */
void gt4_sincosf(float x, float *s_out, float *c_out)
{
    const float pi = K(0x40490fdbu), half_pi = K(0x3fc90fdbu);
    int neg = sign_bit(x);
    int k = (int)(x * K(0x3ea2f983u));   /* 1/pi, truncated */
    float xs = x + (neg ? pi : 0.0f), n = (float)k;
    float r = (xs + 0.0f) - n * pi;       /* x - k pi */
    float rc = (pi - xs) + n * pi;        /* the same from the other end */
    float sg = ((neg ^ k) & 1) ? -1.0f : 1.0f;
    float u = (xs - half_pi) - n * pi;
    float u2, a, w2, b, u4, w4, acc;
    r = gt4_select(r <= rc, r, rc);   /* min.s */
    u2 = u * u; a = u * -sg;
    w2 = r * r; b = r * sg;
    u4 = u2 * u2; w4 = w2 * w2;
    {
        float au2 = u2 * a, au4 = u4 * a, au6 = u4 * au2, au8 = au4 * u4;
        float bw2 = w2 * b, bw4 = w4 * b, bw6 = w4 * bw2, bw8 = bw4 * w4;
        acc = a + 0.0f;
        acc = acc + K(0xbe2aaaa4u) * au2;
        acc = acc + K(0x3c08873eu) * au4;
        acc = acc + K(0xb94fb21fu) * au6;
        *c_out = acc + K(0x362e9c14u) * au8;
        acc = b + 0.0f;
        acc = acc + K(0xbe2aaaa4u) * bw2;
        acc = acc + K(0x3c08873eu) * bw4;
        acc = acc + K(0xb94fb21fu) * bw6;
        *s_out = acc + K(0x362e9c14u) * bw8;
    }
}
