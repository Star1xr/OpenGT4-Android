/* Walls: each car is an outline of eight points moved over the plane of the
   course (physics x, y) and kept out of the course's wall segments, with the
   impulse and the friction of a hit (func_0034DAA8 and what it calls).

   The solver works on a block of its own per car ("hull", 0x4F0 bytes at
   conductor+0xDA78, one per car): the pose is copied in from the car, moved
   over the frame's time in up to two pieces split at the first hit, pushed
   out of any wall it still crosses, and copied back.

   Course walls are chains of 16-byte vertices (x at +0, z at +8, s16 at +0xC
   linking the last vertex back to the first, u16 flags at +0xE); on the
   plane a vertex is (x, -z). The ground triangle under the car lists the
   wall segments near it. */
#include "car.h"

#include <math.h>

#define K(bits) gt4_f32(bits)
#define HF(o) GT4_F32(h + (o))
#define HU(o) GT4_U32(h + (o))

/* func_0035C848: the damage light of the cars that show one (params+0x32
   = 1): code `lit` (low nibble of car+0x789, the previous one high), how
   long (`hold`, or D_00620880 for the code with -1), and a flash when
   the light's colour (D_006208A0) changes. */
static void damage_light(uint32_t car, int lit, int hold)
{
    uint32_t s = car + 0x788;
    int cur;
    if (GT4_U8(GT4_U32(car + CAR_PARAMS) + 0x32) != 1) return;   /* func_0035C7C8 */
    if ((int8_t)GT4_U8(s + 3) > 0) {
        if (hold != 0xff) return;
    }
    if (GT4_U8(s + 2) == 0xff) return;
    if (hold == 0 && GT4_U8(s + 2)) return;
    cur = GT4_U8(car + 0x789) & 0xf;
    if (cur != lit) {
        GT4_U8(s + 6) = (uint8_t)lit;
        GT4_U8(s + 1) = (uint8_t)((cur << 4) | lit);
    }
    GT4_U8(s + 2) = hold != -1 ? (uint8_t)hold : GT4_U8(0x00620880u + (uint32_t)lit);
    if (GT4_U8(0x006208a0u + (uint32_t)cur) == GT4_U8(0x006208a0u + (uint32_t)lit)) return;
    GT4_U8(s + 3) = GT4_U8(0x006208a0u + (uint32_t)cur) ? 0x24 : 0x18;
}

/* func_0035CF88: the light for a knock from another car, by contact
   detail. */
static void knock_light(uint32_t car, int32_t detail)
{
    if (GT4_U8(GT4_U32(car + CAR_PARAMS) + 0x32) != 1) return;
    if ((uint32_t)(detail - 1) < 2) damage_light(car, 5, -1);
    else if ((uint32_t)(detail - 5) < 2) damage_light(car, 8, -1);
    else if (detail < 8) damage_light(car, 4, -1);
    else if (detail < 11) damage_light(car, 5, 0xd2);
    else if (detail == 11 || detail == 15) damage_light(car, 4, 0xd2);
    else damage_light(car, 8, 0xd2);
}

/* func_00350328: the vibration the impact asks for (car+0x70C, strongest
   wins, with its kind and detail at +0x710/+0x711): walls by force over
   50000, cars twice the force, kind 2 (when the race allows) by force over
   50 at least 0.2; up to 1, nothing under 0.2. */
static void impact_vibration(uint32_t car, int32_t kind, int32_t detail, float force)
{
    uint32_t b = car + 0x104;
    float v = 0;
    if (kind == -1) return;
    if (kind == 0) v = force / K(0x47435000u);
    else if (kind == 1) v = force + force;
    else if (kind == 2) {
        if (!GT4_U8(GT4_U32(car + 4) + 0x10000u - 0x7a2u)) return;   /* func_0035DF00 */
        v = force / K(0x42480000u);
        if (v < 0) v = -v;
        if (v < K(0x3e4ccccc)) v = K(0x3e4ccccc);
    }
    if (K(0x3f800000u) < v) v = K(0x3f800000u);
    if (v < K(0x3e4ccccc)) v = 0;
    if (GT4_F32(b + 0x608) < v) {
        GT4_U8(b + 0x60d) = (uint8_t)detail;
        GT4_F32(b + 0x608) = v;
        GT4_U8(b + 0x60c) = (uint8_t)kind;
    }
}

/* func_00350530: an impact on the car (kind 0 a wall, 1 another car, 2
   the ground): its damage light, the hit counter (car+0x733, at most
   100, once per 60-step window at +0x735), the session's collision
   penalty (method 0x190) and the vibration. */
void gt4_car_impact(uint32_t car, int32_t kind, int32_t detail, float force)
{
    uint32_t b = car + 0x104;
    if (kind == 0) damage_light(car, 9, -1);                  /* func_0035CF40 */
    else if (kind == 1) knock_light(car, detail);
    else if (kind == 2) damage_light(car, 5, -1);             /* func_0035D0C0 */
    if (!GT4_U8(b + 0x467)) {
        uint8_t f = GT4_U8(b + 0x630);
        if (!(f & 2) && GT4_U8(b + 0x62f) < 100) GT4_U8(b + 0x62f) = (uint8_t)(GT4_U8(b + 0x62f) + 1);
        GT4_U8(b + 0x631) = 60;
        GT4_U8(b + 0x630) = (uint8_t)(f | 2);
    }
    gt4_session_x(GT4_U32(car + 4), 0x190, car, (uint32_t)kind, (uint32_t)detail, 0, 0, 0, force, NULL);
    impact_vibration(car, kind, detail, force);
}

enum {
    WALLS          = 0xda6c,  /* conductor: the solver ([+0] conductor, hulls from +0xC) */
    HULL_SIZE      = 0x4f0,
    CONDUCTOR_CARS = 0xf8a0,  /* conductor: number of cars */

    H_INV_MASS     = 0x000,
    H_INV_INERTIA  = 0x004,
    H_POINTS       = 0x008,   /* [8] POINT_SIZE: the outline */
    H_EDGE_HITS    = 0x228,   /* [8] EDGE_SIZE: wall corners met by the edges */
    H_MARGIN       = 0x388,
    H_T0           = 0x38c,   /* part of the frame already moved (0..1) */
    H_POS          = 0x390,   /* x, y, z at H_T0 */
    H_YAW          = 0x39c,
    H_T1           = 0x3a0,
    H_POS_B        = 0x3a4,   /* x, y, z, yaw at H_T1 */
    H_YAW_B        = 0x3b0,
    H_STEP         = 0x3b4,   /* x, y, z, yaw from T0 to T1 */
    H_VEL          = 0x3c4,   /* x, y, z */
    H_YAW_RATE     = 0x3d0,
    H_BOX          = 0x3d4,   /* min x, min y, max x, max y of the move, plus margin */
    H_BOX_SIZE     = 0x3e4,
    H_STARTED      = 0x3ec,
    H_HIT_POINT    = 0x3f4,   /* first hit: a point met a wall */
    H_HIT_CORNER   = 0x3f8,   /* first hit: an edge met a wall's corner */
    H_HIT_T        = 0x3fc,
    H_SENSOR       = 0x400,   /* a sensor line (flag 2) was crossed ... */
    H_SENSOR_T     = 0x404,   /* ... at this fraction */
    H_HIT_EDGE     = 0x408,   /* car against car: this car's edge, */
    H_HIT_CAR      = 0x40c,   /* the other car, */
    H_HIT_OTHER    = 0x410,   /* its point, */
    H_HIT_ARM      = 0x414,   /* and where, in this car's frame */
    H_POINT        = 0x41c,
    H_WALL_DIR     = 0x420,
    H_WALL         = 0x428,
    H_CORNER       = 0x42c,
    H_CARS         = 0x430,   /* car against car (not in this pass) */
    H_HIT_FRONT    = 0x434,
    H_HIT_REAR     = 0x438,
    H_HIT_ANY      = 0x43c,
    H_SENSOR_HIT   = 0x444,
    H_IMPACT       = 0x448,
    H_INDEX        = 0x44c,   /* the car's number */
    H_CAR          = 0x450,
    H_IN_POS       = 0x454,   /* copied from the car: position, velocity, angles, rates */
    H_IN_VEL       = 0x460,
    H_IN_ANGLES    = 0x46c,
    H_IN_RATES     = 0x478,   /* pitch, roll, yaw */
    H_GROUND_ID    = 0x484,
    H_COURSE       = 0x488,
    H_FRICTION_MAX = 0x498,
    H_HULLS        = 0x4a8,   /* the first hull (car against car) */
    H_PAIR_REACH   = 0x4c4,   /* [car] square of the distance to look at the other car */
    H_DT           = 0x4dc,
    H_SHAKE_TIME   = 0x4e0,
    H_SHAKE        = 0x4e4,
    H_IMPACT_TIME  = 0x4e8,
    H_IMPACT_PEAK  = 0x4ec,

    POINT_SIZE     = 0x44,
    P_LOCAL        = 0x00,    /* x, y in the car */
    P_A            = 0x10,    /* x, y at H_T0 */
    P_B            = 0x18,    /* x, y at H_T1 */
    P_FLAGS        = 0x20,    /* bit 0 sliding on P_WALL, bit 1 against a car */
    P_HIT          = 0x24,
    P_HIT_T        = 0x28,
    P_WALL         = 0x2c,
    P_WALL_DIR     = 0x30,
    P_CAR          = 0x3c,    /* against a car: which one, and the edge of its outline */
    P_CAR_EDGE     = 0x40,

    EDGE_SIZE      = 0x2c,
    E_NORMAL       = 0x10,    /* the edge's outward normal in the car, */
    E_OFFSET       = 0x18,    /* and its distance from the centre (negative) */
    E_HIT          = 0x20,
    E_HIT_T        = 0x24,
    E_WALL         = 0x28,

    CAR_VIEW_SHAKE = 0x714,
};

#define SHAKE_ON    0x006202bau  /* byte: kick the view on hits */
#define SHAKE_PARAM 0x006202c8u  /* +0x20 threshold, +0x24 cap, +0x28 base, +0x30 gain */
#define RESTITUTION 0x00846634u
#define CAR_RESTITUTION 0x00846630u
#define FRICTION    0x0084663cu
#define SHAKE_BLOCK 0xcab8u      /* conductor: 0x30 bytes per car */

static uint32_t point(uint32_t h, int k) { return h + H_POINTS + (uint32_t)k * POINT_SIZE; }
static uint32_t edge(uint32_t h, int k)  { return h + H_EDGE_HITS + (uint32_t)k * EDGE_SIZE; }

static uint32_t wall_next(uint32_t v)
{
    int16_t o = (int16_t)GT4_U16(v + 0xc);
    return v + 0x10 + (o < 0 ? (uint32_t)(o * 16) : 0);
}
static uint32_t wall_prev(uint32_t v)
{
    int16_t o = (int16_t)GT4_U16(v + 0xc);
    return v - 0x10 + (o > 0 ? (uint32_t)(o * 16) : 0);
}

static void car_contacts(uint32_t h);

static void copy8(uint32_t dst, uint32_t src) { memcpy(gt4_ptr(dst), gt4_ptr(src), 8); }

/* func_0045C750 / func_0045C7E8: outline at a pose. */
static void place(uint32_t h, float yaw, float x, float y, uint32_t at)
{
    float s, c;
    gt4_sincosf(yaw, &s, &c);
    for (int k = 0; k < 8; k++) {
        uint32_t p = point(h, k);
        float lx = GT4_F32(p + P_LOCAL), ly = GT4_F32(p + P_LOCAL + 4);
        GT4_F32(p + at)     = (c * lx - s * ly) + x;
        GT4_F32(p + at + 4) = (s * lx + c * ly) + y;
    }
}
static void place_a(uint32_t h) { place(h, HF(H_YAW), HF(H_POS), HF(H_POS + 4), P_A); }
static void place_b(uint32_t h) { place(h, HF(H_YAW_B), HF(H_POS_B), HF(H_POS_B + 4), P_B); }

/* func_0045C898: box around the move. */
static void move_box(uint32_t h)
{
    float r = HF(H_MARGIN), lo, hi;
    if (HF(H_STEP) < 0) { lo = HF(H_POS_B); hi = HF(H_POS); }
    else                { lo = HF(H_POS);   hi = HF(H_POS_B); }
    HF(H_BOX) = lo - r;
    HF(H_BOX + 8) = hi + r;
    if (HF(H_STEP + 4) < 0) { lo = HF(H_POS_B + 4); hi = HF(H_POS + 4); }
    else                    { lo = HF(H_POS + 4);   hi = HF(H_POS_B + 4); }
    HF(H_BOX + 4) = lo - r;
    HF(H_BOX + 0xc) = hi + r;
    HF(H_BOX_SIZE) = HF(H_BOX + 8) - HF(H_BOX);
    HF(H_BOX_SIZE + 4) = HF(H_BOX + 0xc) - HF(H_BOX + 4);
}

/* func_0045C5A8: pose at T1 from the velocities. */
static void predict(uint32_t h)
{
    float dt = (HF(H_T1) - HF(H_T0)) * HF(H_DT);
    float dyaw = HF(H_YAW_RATE) * dt, dx = HF(H_VEL) * dt, dy = HF(H_VEL + 4) * dt, dz = HF(H_VEL + 8) * dt;
    HF(H_STEP + 0xc) = dyaw;
    HF(H_STEP) = dx;
    HF(H_STEP + 4) = dy;
    HF(H_STEP + 8) = dz;
    HF(H_YAW_B) = HF(H_YAW) + dyaw;
    HF(H_POS_B) = HF(H_POS) + dx;
    HF(H_POS_B + 4) = HF(H_POS + 4) + dy;
    HF(H_POS_B + 8) = HF(H_POS + 8) + dz;
    move_box(h);
    place_b(h);
}

/* func_0045C488 */
static void begin(uint32_t h, float dt)
{
    HF(H_DT) = dt;
    HF(H_T1) = 1.0f;
    HF(H_POS) = HF(H_IN_POS);
    HF(H_POS + 4) = HF(H_IN_POS + 4);
    HF(H_POS + 8) = HF(H_IN_POS + 8);
    HF(H_YAW) = HF(H_IN_ANGLES + 8);
    HF(H_VEL) = HF(H_IN_VEL);
    HF(H_VEL + 4) = HF(H_IN_VEL + 4);
    HF(H_VEL + 8) = HF(H_IN_VEL + 8);
    HF(H_YAW_RATE) = HF(H_IN_RATES + 8);
    HU(H_HIT_FRONT) = 0;
    HU(H_HIT_REAR) = 0;
    HU(0x440) = 0;
    HU(H_HIT_ANY) = 0;
    HU(H_SENSOR_HIT) = 0;
    HU(H_IMPACT) = 0;
    HU(H_T0) = 0;
    if (HU(H_STARTED)) {
        for (int k = 0; k < 8; k++) copy8(point(h, k) + P_A, point(h, k) + P_B);   /* func_0045C718 */
    } else {
        HU(H_STARTED) = 1;
        place_a(h);
    }
}

/* func_0045C530 */
static void set_cars(uint32_t h, uint32_t on)
{
    if (HU(H_CARS) && !on)
        for (int k = 0; k < 8; k++) GT4_U32(point(h, k) + P_FLAGS) &= ~2u;
    HU(H_CARS) = on;
}

/* func_0045D0E0: points sliding along a wall follow it round the corners,
   and the ones that leave it let go; the rest are held on the wall line,
   losing the velocity into it. */
static void slide(uint32_t h)
{
    for (int k = 0; k < 8; k++) {
        uint32_t p = point(h, k), w, v;
        float dx, dy, ux, uy, along;
        if (!(GT4_U32(p + P_FLAGS) & 1)) continue;
        w = GT4_U32(p + P_WALL);
        v = wall_next(w);
        dx = GT4_F32(p + P_A) - GT4_F32(w);
        dy = GT4_F32(p + P_A + 4) + GT4_F32(w + 8);
        ux = GT4_F32(p + P_WALL_DIR);
        uy = GT4_F32(p + P_WALL_DIR + 4);
        along = dx * ux + dy * uy;
        if (along < 0 || (GT4_F32(v) - GT4_F32(w)) * ux - (GT4_F32(v + 8) - GT4_F32(w + 8)) * uy < along) {
            /* past one end: on to the neighbouring segment */
            uint32_t a, b;
            float ex, ey, len;
            if (along < 0) { a = wall_prev(w); b = w; w = a; }
            else           { a = v; b = wall_next(v); w = v; }
            ex = GT4_F32(b) - GT4_F32(a);
            ey = GT4_F32(a + 8) - GT4_F32(b + 8);
            len = ex * ex + ey * ey;
            len = len == 0 ? len : sqrtf(fabsf(len));
            if (len == 0) { ex = 1.0f; ey = 0; }
            else { float inv = 1.0f / len; ex = ex * inv; ey = ey * inv; }
            GT4_F32(p + P_WALL_DIR) = ex;
            GT4_F32(p + P_WALL_DIR + 4) = ey;
            GT4_U32(p + P_WALL) = w;
            dy = GT4_F32(p + P_A + 4) + GT4_F32(w + 8);
            dx = GT4_F32(p + P_A) - GT4_F32(w);
            ux = ex; uy = ey;
            along = dx * ux + dy * uy;
        }
        if (!(0 <= HF(H_VEL) * uy - HF(H_VEL + 4) * ux) || !(0 < dx * uy - dy * ux)) {
            GT4_U32(p + P_FLAGS) &= ~1u;
            continue;
        }
        {
            float cx = (GT4_F32(w) + ux * along) - GT4_F32(p + P_A);
            float cy = ((uy * along) - GT4_F32(w + 8)) - GT4_F32(p + P_A + 4);
            float vx, vy, proj, dvx, dvy, rx, ry;
            HF(H_POS + 4) = HF(H_POS + 4) + cy;
            HF(H_POS) = HF(H_POS) + cx;
            vx = HF(H_VEL); vy = HF(H_VEL + 4);
            proj = ux * vx + uy * vy;
            HF(H_VEL) = proj * ux;
            HF(H_VEL + 4) = proj * uy;
            dvx = vx - HF(H_VEL);
            dvy = vy - HF(H_VEL + 4);
            rx = GT4_F32(p + P_A) - HF(H_POS);
            ry = GT4_F32(p + P_A + 4) - HF(H_POS + 4);
            HF(H_YAW_RATE) = HF(H_YAW_RATE) - ((rx * dvy - ry * dvx) * HF(H_FRICTION_MAX)) * HF(H_INV_INERTIA);
        }
    }
}

/* func_0045D550: one wall segment against the move A -> B. Points that cross
   it (from its front) and edges that sweep over its first vertex record the
   fraction of the move where they meet it. Flag 2 marks a sensor line. */
static void segment_check(uint32_t w, uint32_t h)
{
    uint32_t v = wall_next(w);
    float nx = GT4_F32(v), ny = -GT4_F32(v + 8), wx = GT4_F32(w), wy = -GT4_F32(w + 8);
    uint16_t flags = GT4_U16(w + 0xe);
    int sensor = flags & 2;
    float ex = nx - wx, ey = ny - wy;
    for (int k = 0; k < 8; k++) {
        uint32_t p = point(h, k);
        float bx, by, ax, ay, sb, sa, t;
        if (GT4_U32(p + P_FLAGS)) continue;
        bx = GT4_F32(p + P_B); by = GT4_F32(p + P_B + 4);
        sb = ex * (by - wy) - ey * (bx - wx);
        if (0 <= sb) continue;
        ax = GT4_F32(p + P_A); ay = GT4_F32(p + P_A + 4);
        sa = ex * (ay - wy) - ey * (ax - wx);
        if (sa <= 0) continue;
        {
            float mx = bx - ax, my = by - ay;
            if ((mx * (ay - wy) - my * (ax - wx)) < 0) continue;
            if (0 < mx * (ay - ny) - my * (ax - nx)) continue;
        }
        t = sa - sb;
        t = t == 0 ? 1.0f : sa / t;
        if (!(t < GT4_F32(p + P_HIT_T))) continue;
        if (sensor) {
            if (t < HF(H_SENSOR_T)) { HF(H_SENSOR_T) = t; HU(H_SENSOR) = 1; }
            continue;
        }
        {
            float len = ex * ex + ey * ey;
            GT4_F32(p + P_HIT_T) = t;
            GT4_U32(p + P_HIT) = 1;
            GT4_U32(p + P_WALL) = w;
            len = len == 0 ? len : sqrtf(fabsf(len));
            if (len == 0) { GT4_F32(p + P_WALL_DIR) = 1.0f; GT4_U32(p + P_WALL_DIR + 4) = 0; }
            else { GT4_F32(p + P_WALL_DIR + 4) = ey / len; GT4_F32(p + P_WALL_DIR) = ex / len; }
        }
    }
    if (!(flags & 1)) return;
    for (int k = 0; k < 8; k++) {
        uint32_t q = point(h, (k + 7) & 7), p = point(h, k);
        float qbx = GT4_F32(q + P_B), qby = GT4_F32(q + P_B + 4), pbx = GT4_F32(p + P_B), pby = GT4_F32(p + P_B + 4);
        float qax, qay, pax, pay, sb, sa, qmx, qmy, pmy, pmx, c1, c3, c2, c0, t;
        sb = (pbx - qbx) * (wy - qby) - (pby - qby) * (wx - qbx);
        if (sb <= 0) continue;
        qay = GT4_F32(q + P_A + 4); qax = GT4_F32(q + P_A);
        pax = GT4_F32(p + P_A); pay = GT4_F32(p + P_A + 4);
        sa = (pax - qax) * (wy - qay) - (pay - qay) * (wx - qax);
        if (0 <= sa) continue;
        qmx = qbx - qax; qmy = qby - qay; pmy = pby - pay; pmx = pbx - pax;
        c1 = qmx * (pay - qay) - qmy * (pax - qax);
        c3 = qmx * (wy - qay) - qmy * (wx - qax);
        c2 = pmx * (wy - pay) - pmy * (wx - pax);
        if (c1 < 0 && (0 < c3 || 0 < c2)) continue;
        c0 = pmx * (pay - qay) - pmy * (pax - qax);
        if (c0 < 0 && (c3 < 0 || c2 < 0)) continue;
        if (c3 < 0 || 0 < c2) continue;
        t = sa - sb;
        t = t == 0 ? 1.0f : sa / t;
        if (sensor) {
            if (t < HF(H_SENSOR_T)) { HF(H_SENSOR_T) = t; HU(H_SENSOR) = 1; }
        } else {
            uint32_t e = edge(h, (k + 7) & 7);
            if (t < GT4_F32(e + E_HIT_T)) {
                GT4_F32(e + E_HIT_T) = t;
                GT4_U32(e + E_WALL) = w;
                GT4_U32(e + E_HIT) = 1;
            }
        }
    }
}

/* func_0045DBB8: one wall segment against the outline at B; points behind
   it are put back on it (and start sliding), or back across it when the
   whole move went through. */
static void segment_push(uint32_t w, uint32_t h)
{
    uint32_t v = wall_next(w);
    float ny = -GT4_F32(v + 8), wy = -GT4_F32(w + 8), wx = GT4_F32(w), nx = GT4_F32(v);
    float ex = nx - wx, ey = ny - wy;
    for (int i = 0; i < 16; i++) {
        uint32_t p = point(h, i & 7), q;
        float pbx, pby, qbx, qby, rx, ry, qx, qy, mx, my, qnx, qny, sp, sq, proj, len2, ox, oy;
        if (GT4_U32(p + P_FLAGS)) continue;
        q = point(h, ((i & 8) ? i + 1 : i - 1) & 7);
        pby = GT4_F32(p + P_B + 4); pbx = GT4_F32(p + P_B);
        qbx = GT4_F32(q + P_B); qby = GT4_F32(q + P_B + 4);
        ry = pby - wy; rx = pbx - wx; qx = qbx - wx; qy = qby - wy;
        mx = pbx - qbx; my = pby - qby; qnx = qbx - nx; qny = qby - ny;
        sp = ex * ry - ey * rx;
        sq = ex * qy - ey * qx;
        if (0 <= sp || sq <= 0) continue;
        if (mx * qy - my * qx < 0 || 0 < mx * qny - my * qnx) continue;
        proj = rx * ex + ry * ey;
        len2 = ex * ex + ey * ey;
        if (proj < 0 || len2 < proj) {
            /* beyond an end: the wall's end vertex back onto the edge q-p */
            float bx = proj < 0 ? qx : qnx, by = proj < 0 ? qy : qny, m2 = mx * mx + my * my, s;
            s = m2 == 0 ? 0.0f : (-bx * mx - by * my) / m2;
            oy = s * my + by;
            ox = s * mx + bx;
            HF(H_POS_B + 4) = HF(H_POS_B + 4) - oy;
            HF(H_POS_B) = HF(H_POS_B) - ox;
            place_b(h);
            continue;
        }
        {
            float r = len2 == 0 ? 1.0f : 1.0f / sqrtf(fabsf(len2)), f;
            if (len2 != 0) proj = proj / len2;
            ox = proj * ex - rx;
            oy = proj * ey - ry;
            if (0.25f < ox * ox + oy * oy) {
                f = i < 8 ? 1.0f - proj : -proj;
                HF(H_POS_B) = HF(H_POS_B) + f * ex;
                HF(H_POS_B + 4) = HF(H_POS_B + 4) + f * ey;
                place_b(h);
                continue;
            }
            HF(H_POS_B) = HF(H_POS_B) + ox;
            HF(H_POS_B + 4) = HF(H_POS_B + 4) + oy;
            place_b(h);
            GT4_U32(p + P_WALL) = w;
            GT4_U32(p + P_FLAGS) |= 1;
            GT4_F32(p + P_WALL_DIR + 4) = ey * r;
            GT4_F32(p + P_WALL_DIR) = ex * r;
        }
    }
}

typedef void (*segment_fn)(uint32_t w, uint32_t h);

/* func_003E6A58 (with func_00395A38): the wall segments listed by the
   ground triangle under the car that come near the box (plane x, y; walls
   are in course x, z = -y). */
static void for_walls(uint32_t h, float x0, float y0, float x1, float y1, segment_fn fn)
{
    uint32_t course = HU(H_COURSE), mesh = GT4_U32(GT4_U32(course + 4) + 0xdc);
    uint32_t id = HU(H_GROUND_ID), list, from, to;
    float z0 = -y1, z1 = -y0;
    if ((int32_t)id < 0) {
        gt4_ray_hit hit;
        float p[3] = { (x0 + x1) * 0.5f, 10000.0f, (z0 + z1) * 0.5f };
        if (!gt4_course_ray_down(mesh, &hit, p)) return;
        id = (uint16_t)hit.tri | (uint32_t)(int32_t)hit.leaf << 16;
    }
    if ((int16_t)(id >> 16) < 0) return;
    list = GT4_U32(GT4_U32(mesh + 0x78) + (id & 0xffff) * 12 + 8);
    from = list & 0xffffff;
    to = from + (list >> 24);
    for (uint32_t s = from; s < to; s++) {
        int16_t idx = (int16_t)GT4_U16(GT4_U32(mesh + 0x8c) + s * 2);
        uint32_t w = GT4_U32(mesh + 0x84) + (uint32_t)(idx * 16);
        float x = GT4_F32(w), z = GT4_F32(w + 8);
        int out = (x < x0) | (x1 < x) << 1 | (z < z0) << 2 | (z1 < z) << 3;
        if (out) {
            uint32_t v = wall_next(w);
            x = GT4_F32(v); z = GT4_F32(v + 8);
            if (out & ((x < x0) | (x1 < x) << 1 | (z < z0) << 2 | (z1 < z) << 3)) continue;
        }
        fn(w, h);
    }
}

/* func_0045D8E0: the first thing the move A -> B meets. 1 when there is one. */
static int first_hit(uint32_t h)
{
    float x0, x1, y0, y1, best = 2.0f;
    int which = -1;
    {
        uint32_t p = point(h, 0);
        x0 = x1 = GT4_F32(p + P_A);
        y0 = y1 = GT4_F32(p + P_A + 4);
    }
    for (int k = 0; k < 8; k++) {
        uint32_t p = point(h, k), e = edge(h, k);
        float x = GT4_F32(p + P_A), y = GT4_F32(p + P_A + 4);
        if (x < x0) x0 = x;
        if (x1 < x) x1 = x;
        if (y < y0) y0 = y;
        if (y1 < y) y1 = y;
        x = GT4_F32(p + P_B);
        if (x < x0) x0 = x;
        if (x1 < x) x1 = x;
        y = GT4_F32(p + P_B + 4);
        if (y < y0) y0 = y;
        if (y1 < y) y1 = y;
        GT4_U32(p + P_HIT) = 0;
        GT4_F32(p + P_HIT_T) = 2.0f;
        GT4_U32(e + E_HIT) = 0;
        GT4_F32(e + E_HIT_T) = 2.0f;
    }
    for_walls(h, x0, y0, x1, y1, segment_check);
    for (int k = 0; k < 8; k++) {
        uint32_t p = point(h, k), e = edge(h, k);
        if (GT4_U32(p + P_HIT) && !GT4_U32(p + P_FLAGS) && GT4_F32(p + P_HIT_T) < best) {
            best = GT4_F32(p + P_HIT_T);
            which = k;
        }
        if (GT4_U32(e + E_HIT) && GT4_F32(e + E_HIT_T) < best) {
            best = GT4_F32(e + E_HIT_T);
            which = k + 8;
        }
    }
    if (which < 0) return 0;
    HF(H_HIT_T) = (HF(H_T1) - HF(H_T0)) * best + HF(H_T0);
    if (which < 8) {
        uint32_t p = point(h, which);
        HU(H_POINT) = which;
        HU(H_HIT_POINT) = 1;
        HU(H_WALL) = GT4_U32(p + P_WALL);
        copy8(h + H_WALL_DIR, p + P_WALL_DIR);
        HU(H_HIT_ANY) = 1;
        if (which < 4) HU(H_HIT_FRONT) = 1;
        else HU(H_HIT_REAR) = 1;
    } else {
        which -= 8;
        HU(H_HIT_CORNER) = 1;
        HU(H_CORNER) = which;
        HU(H_HIT_ANY) = 1;
        if (which < 3) HU(H_HIT_FRONT) = 1;
        if ((uint32_t)(which - 4) < 3) HU(H_HIT_REAR) = 1;
    }
    return 1;
}

/* func_0045DB78 */
static void sensor_check(uint32_t h, float t)
{
    if (HU(H_SENSOR) && (HF(H_T1) - HF(H_T0)) * HF(H_SENSOR_T) + HF(H_T0) < t)
        HU(H_SENSOR_HIT) = 1;
}

/* func_0045C668: move on to time t (absolute fraction of the frame). */
static void advance(uint32_t h, float t)
{
    float d = HF(H_T1) - HF(H_T0), f = 0, sx, sy, sz, syaw;
    if (d != 0) f = (t - HF(H_T0)) / d;
    sx = HF(H_STEP) * f; sy = HF(H_STEP + 4) * f; sz = HF(H_STEP + 8) * f; syaw = HF(H_STEP + 0xc) * f;
    HF(H_POS) = HF(H_POS) + sx;
    HF(H_POS + 4) = HF(H_POS + 4) + sy;
    HF(H_POS + 8) = HF(H_POS + 8) + sz;
    HF(H_T0) = t;
    HF(H_YAW) = HF(H_YAW) + syaw;
    HF(H_STEP + 0xc) = syaw;
    HF(H_STEP) = sx;
    HF(H_STEP + 4) = sy;
    HF(H_STEP + 8) = sz;
    place_a(h);
}

/* func_0045E898: friction impulse against the sliding velocity, capped by
   the normal impulse. */
static void friction(uint32_t h, const float imp[2], const float cv[2], float out[3])
{
    float n2 = imp[0] * imp[0] + imp[1] * imp[1], f = 0, t[3], m;
    if (n2 != 0) f = (cv[0] * imp[0] + cv[1] * imp[1]) / n2;
    t[0] = f * imp[0] - cv[0];
    t[1] = f * imp[1] - cv[1];
    t[2] = -HF(H_VEL + 8);
    m = (t[0] * t[0] + t[1] * t[1]) + t[2] * t[2];
    if (m == 0) { out[0] = out[1] = out[2] = 0; return; }
    {
        float n = n2 == 0 ? n2 : sqrtf(fabsf(n2)), r = 1.0f / sqrtf(fabsf(m));
        float s = (GT4_F32(FRICTION) * n) * r;
        if (HF(H_FRICTION_MAX) < s) s = HF(H_FRICTION_MAX);
        out[0] = s * t[0]; out[1] = s * t[1]; out[2] = s * t[2];
    }
}

/* func_0045E288: normal impulse of a point hitting the wall. */
static void wall_impulse(uint32_t h, float out[2], const float m[4], const float cv[2])
{
    float e = GT4_F32(RESTITUTION);
    float n[2] = { -HF(H_WALL_DIR + 4), HF(H_WALL_DIR) };
    uint32_t eh = edge(h, (int)HU(H_POINT));
    float a = GT4_F32(eh), b = GT4_F32(eh + 4);
    float nl0 = m[0] * n[0] + m[2] * n[1], nl1 = m[1] * n[0] + m[3] * n[1];
    float r0 = m[0] * b + m[1] * -a, r1 = m[2] * b + m[3] * -a;
    float k = -(a * nl1 - b * nl0) * HF(H_INV_INERTIA);
    float den = HF(H_INV_MASS) + (r0 * n[0] + r1 * n[1]) * k, j = 0;
    if (den != 0) j = (-(e + 1.0f) * (cv[0] * n[0] + cv[1] * n[1])) / den;
    out[0] = j * n[0];
    out[1] = j * n[1];
    if (j < 0) j = -j;
    HF(H_IMPACT) = HF(H_IMPACT) + j / 100000.0f;
    gt4_car_impact(HU(H_CAR), 0, (int32_t)HU(H_POINT), j);   /* impact on the car: damage, vibration */
}

/* func_0045E450: the point back onto the wall line. */
static void unstick(uint32_t h)
{
    uint32_t w = HU(H_WALL), p = point(h, (int)HU(H_POINT));
    float dx = HF(H_WALL_DIR), dy = HF(H_WALL_DIR + 4), wx = GT4_F32(w), wy = -GT4_F32(w + 8);
    float ax = GT4_F32(p + P_A), ay = GT4_F32(p + P_A + 4), rx = ax - wx, ry = ay - wy, l2, s;
    if (!(0 < rx * dy - ry * dx)) return;
    l2 = dx * dx + dy * dy;
    s = l2 == 0 ? 1.0f : (rx * dx + ry * dy) / l2;
    HF(H_POS) = HF(H_POS) + ((s * HF(H_WALL_DIR) + wx) - ax);
    HF(H_POS + 4) = HF(H_POS + 4) + ((s * HF(H_WALL_DIR + 4) + wy) - ay);
    place_a(h);
}

/* func_0045E028: response to a point hitting a wall. */
static void point_hit(uint32_t h)
{
    float s = gt4_sinf(HF(H_YAW)), c = gt4_cosf(HF(H_YAW));
    float m[4] = { s, -c, c, s }, cv[2], imp[2], fr[3], ang[2], inv_mass;
    uint32_t e = edge(h, (int)HU(H_POINT)), p;
    float a = -GT4_F32(e), b = GT4_F32(e + 4), w = HF(H_YAW_RATE);
    float rx = m[0] * b + m[1] * a, ry = m[2] * b + m[3] * a;
    cv[1] = HF(H_VEL + 4) - w * ry;
    cv[0] = HF(H_VEL) - w * rx;
    wall_impulse(h, imp, m, cv);
    friction(h, imp, cv, fr);
    ang[0] = m[0] * imp[0] + m[2] * imp[1];
    ang[1] = m[1] * imp[0] + m[3] * imp[1];
    imp[0] = imp[0] + fr[0];
    imp[1] = imp[1] + fr[1];
    unstick(h);
    p = point(h, (int)HU(H_POINT));
    GT4_U32(p + P_FLAGS) |= 1;
    inv_mass = HF(H_INV_MASS);
    HF(H_VEL) = HF(H_VEL) + imp[0] * inv_mass;
    HF(H_VEL + 4) = HF(H_VEL + 4) + imp[1] * inv_mass;
    {
        float vz = HF(H_VEL + 8), nz = vz + fr[2] * inv_mass;
        HF(H_VEL + 8) = nz;
        if (vz < 0 ? 0 < nz : nz < 0) HU(H_VEL + 8) = 0;
    }
    e = edge(h, (int)HU(H_POINT));
    HF(H_YAW_RATE) = HF(H_YAW_RATE) + (GT4_F32(e) * ang[1] - GT4_F32(e + 4) * ang[0]) * HF(H_INV_INERTIA);
}

/* func_0045E768: normal impulse of an edge meeting a wall's corner (arm
   from the car's centre to the corner, nw the edge's normal in the
   world). */
static void corner_impulse(uint32_t h, float out[2], const float cv[2], const float nw[2], const float arm[2])
{
    float e = GT4_F32(RESTITUTION), c = arm[0] * nw[1] - arm[1] * nw[0];
    float den = HF(H_INV_MASS) + (c * c) * HF(H_INV_INERTIA), j = 0;
    if (den != 0) j = (-(e + 1.0f) * (cv[0] * nw[0] + cv[1] * nw[1])) / den;
    out[0] = j * nw[0];
    out[1] = j * nw[1];
    if (j < 0) j = -j;
    HF(H_IMPACT) = HF(H_IMPACT) + j / 100000.0f;
    gt4_car_impact(HU(H_CAR), 0, (int32_t)(HU(H_CORNER) + 8), j);
}

/* func_0045E570: response to an edge hitting a wall's corner. */
static void corner_hit(uint32_t h)
{
    float s = gt4_sinf(HF(H_YAW)), c = gt4_cosf(HF(H_YAW));
    float m[4] = { s, -c, c, s }, arm[2], nw[2], cv[2], imp[2], fr[3], inv_mass, torque;
    uint32_t e = h + H_EDGE_HITS + HU(H_CORNER) * EDGE_SIZE, w = GT4_U32(e + E_WALL);
    float n0 = GT4_F32(e + E_NORMAL), n1 = GT4_F32(e + E_NORMAL + 4);
    arm[0] = GT4_F32(w) - HF(H_POS);
    arm[1] = -GT4_F32(w + 8) - HF(H_POS + 4);
    nw[0] = m[0] * n0 + m[1] * n1;
    nw[1] = m[2] * n0 + m[3] * n1;
    cv[0] = HF(H_VEL) - HF(H_YAW_RATE) * arm[1];
    cv[1] = HF(H_VEL + 4) + HF(H_YAW_RATE) * arm[0];
    corner_impulse(h, imp, cv, nw, arm);
    friction(h, imp, cv, fr);
    torque = arm[0] * imp[1] - arm[1] * imp[0];
    imp[0] = imp[0] + fr[0];
    imp[1] = imp[1] + fr[1];
    inv_mass = HF(H_INV_MASS);
    HF(H_VEL) = HF(H_VEL) + imp[0] * inv_mass;
    HF(H_VEL + 4) = HF(H_VEL + 4) + imp[1] * inv_mass;
    HF(H_YAW_RATE) = HF(H_YAW_RATE) + torque * HF(H_INV_INERTIA);
    {
        float vz = HF(H_VEL + 8), nz = vz + fr[2] * inv_mass;
        HF(H_VEL + 8) = nz;
        if (vz < 0 ? 0 < nz : nz < 0) HU(H_VEL + 8) = 0;
    }
}

void gt4_walls_corner_hit(uint32_t h) { corner_hit(h); }

/* func_0045CA60: a car held against a wall at a standstill turns with its
   steering (the yaw it would have) instead of stopping dead. */
static void wall_turn(uint32_t h, float t)
{
    uint32_t car = HU(H_CAR);
    int touching;
    if (GT4_U8(car + 0x56a) || GT4_U8(car + 0x56b)) return;   /* func_003451B8 */
    touching = HU(H_HIT_CORNER) || HU(H_HIT_POINT) || HU(H_HIT_ANY);
    for (int k = 0; !touching && k < 8; k++) {
        uint32_t p = point(h, k), e = edge(h, k);
        touching = (GT4_U32(p + P_FLAGS) & 1) || GT4_U32(p + P_HIT) || GT4_U32(e + 0x1c) || GT4_U32(e + E_HIT);
    }
    if (!(fabsf(HF(H_VEL)) < 1.0f) || !(fabsf(HF(H_VEL + 4)) < 1.0f) || !(t < K(0x3dcccccc)) || !touching) return;
    HF(H_YAW_RATE) = HF(H_YAW_RATE) + (GT4_F32(car + 0x560) * 20.0f) * HF(H_DT);   /* func_00344680 */
    predict(h);
}

/* func_0045C928: yaw brought back once it is more than about seven turns out. */
static float yaw_wrap(float x)
{
    const float pi = K(0x40490fda), two_pi = K(0x40c90fda);
    int n = (int)((fabsf(x) + pi) / two_pi);
    if (n < 8) return x;
    return x < 0 ? x + (float)n * two_pi : x + (float)n * K(0xc0c90fda);
}

/* func_0045C9F8 */
static void finish(uint32_t h)
{
    float r = HF(H_YAW_RATE), lim = K(0x427b53d0);
    HF(H_IN_POS + 4) = HF(H_POS_B + 4);
    HF(H_IN_POS + 8) = HF(H_POS_B + 8);
    HF(H_IN_VEL) = HF(H_VEL);
    HF(H_IN_VEL + 4) = HF(H_VEL + 4);
    HF(H_IN_VEL + 8) = HF(H_VEL + 8);
    HF(H_IN_POS) = HF(H_POS_B);
    HF(H_IN_ANGLES + 8) = yaw_wrap(HF(H_YAW_B));
    if (!(fabsf(r) < lim)) r = r < 0 ? -lim : lim;    /* func_0045C9B0 */
    HF(H_IN_RATES + 8) = r;
}

/* func_0045CC60: strength of the last hit for the view shake. */
static void shake(uint32_t h, uint32_t car)
{
    float f = HF(H_IMPACT);
    if (0 < f && (!HU(H_IMPACT_TIME) || HF(H_IMPACT_PEAK) < f)) {
        HU(H_IMPACT_TIME) = 60;
        HF(H_IMPACT_PEAK) = HF(H_IMPACT);
    } else {
        int32_t t = (int32_t)HU(H_IMPACT_TIME) - 1;
        HU(H_IMPACT_TIME) = t < 0 ? 0 : t;
        HF(H_IMPACT_PEAK) = HF(H_IMPACT_PEAK) * K(0x3f7ae147);
    }
    f = HF(H_IMPACT);
    if (1.0f < f) { HF(H_IMPACT) = 1.0f; f = 1.0f; }
    if ((int32_t)HU(H_SHAKE_TIME) > 0) {
        int32_t t = (int32_t)HU(H_SHAKE_TIME) - 1;
        if (HF(H_SHAKE) < f) {
            if (GT4_F32(car + CAR_VIEW_SHAKE) < f) { GT4_F32(car + CAR_VIEW_SHAKE) = f; f = HF(H_IMPACT); }
            HF(H_SHAKE) = f;
            HU(H_SHAKE_TIME) = 23;
        } else {
            HU(H_SHAKE_TIME) = t;
            if (!t) GT4_U32(car + CAR_VIEW_SHAKE) = 0;
        }
    } else if (0 < f) {
        GT4_F32(car + CAR_VIEW_SHAKE) = f;
        HU(H_SHAKE_TIME) = 23;
        HF(H_SHAKE) = HF(H_IMPACT);
    }
}

/* func_0045BF20: one car's frame against the walls. */
static void car_walls(uint32_t conductor, int i)
{
    uint32_t h = conductor + WALLS + 0xc + (uint32_t)i * HULL_SIZE;
    uint32_t car = conductor + 0x6698 + (uint32_t)i * CAR_SIZE;
    float t = 1.0f;
    begin(h, GT4_F32(car + CAR_DT));
    set_cars(h, 0);
    for (int pass = 0; pass < 2; pass++) {
        car_contacts(h);
        slide(h);
        predict(h);
        HU(0x3f0) = 0;                               /* func_0045C648 */
        HF(H_SENSOR_T) = 2.0f;
        HU(H_HIT_POINT) = 0;
        HU(H_HIT_CORNER) = 0;
        HU(H_SENSOR) = 0;
        if (!first_hit(h)) {
            sensor_check(h, 2.0f);
            break;
        }
        t = HF(H_HIT_T);
        sensor_check(h, t);
        advance(h, t);
        if (HU(H_HIT_POINT)) point_hit(h);
        else corner_hit(h);                           /* an edge hits a wall's corner */
    }
    wall_turn(h, t);
    {
        float x0, x1, y0, y1;
        uint32_t p = point(h, 0);
        x0 = x1 = GT4_F32(p + P_B);
        y0 = y1 = GT4_F32(p + P_B + 4);
        for (int k = 1; k < 8; k++) {   /* points 0..6: the original counts from 1 */
            float x = GT4_F32(p + P_B), y = GT4_F32(p + P_B + 4);
            if (x < x0) x0 = x;
            if (x1 < x) x1 = x;
            if (y < y0) y0 = y;
            if (y1 < y) y1 = y;
            p = point(h, k);
        }
        for_walls(h, x0, y0, x1, y1, segment_push);   /* func_0045DF80 */
    }
    finish(h);
    shake(h, car);
}

/* func_0045DF80: the outline at B pushed out of the walls around it
   (points 0..6: the original counts from 1). */
static void push_out(uint32_t h)
{
    float x0, x1, y0, y1;
    uint32_t p = point(h, 0);
    x0 = x1 = GT4_F32(p + P_B);
    y0 = y1 = GT4_F32(p + P_B + 4);
    for (int k = 1; k < 8; k++) {
        float x = GT4_F32(p + P_B), y = GT4_F32(p + P_B + 4);
        if (x < x0) x0 = x;
        if (x1 < x) x1 = x;
        if (y < y0) y0 = y;
        if (y1 < y) y1 = y;
        p = point(h, k);
    }
    for_walls(h, x0, y0, x1, y1, segment_push);
}

/* func_0045CBB8: the pose and the outline at A moved by (dx, dy). */
static void shift(uint32_t h, float dx, float dy)
{
    HF(H_POS) = HF(H_POS) + dx;
    HF(H_POS + 4) = HF(H_POS + 4) + dy;
    for (int k = 0; k < 8; k++) {
        uint32_t p = point(h, k);
        GT4_F32(p + P_A) = GT4_F32(p + P_A) + dx;
        GT4_F32(p + P_A + 4) = GT4_F32(p + P_A + 4) + dy;
    }
}

/* func_0045FAD0: two cars already overlapping at A are pushed apart, half
   each: the deepest of the other's points inside this outline, measured
   along the line between their centres. Only for cars near enough (the
   square of the distance under this pair's limit, heights within 4 m). */
static void overlap(uint32_t h, uint32_t j)
{
    uint32_t o = HU(H_HULLS) + j * HULL_SIZE;
    float cx, cy, deep = 0, px = 0, py = 0;
    if (!HU(H_CARS) || !GT4_U32(o + H_CARS)) return;
    if (4.0f < fabsf(HF(H_POS + 8) - GT4_F32(o + H_POS + 8))) return;
    cx = GT4_F32(o + H_POS) - HF(H_POS);
    cy = GT4_F32(o + H_POS + 4) - HF(H_POS + 4);
    if (HF(H_PAIR_REACH + j * 4) < cx * cx + cy * cy) return;
    for (int q = 0; q < 8; q++) {
        uint32_t b = point(o, q), a0 = point(h, 7), a1 = point(h, 0);
        float bx = GT4_F32(b + P_A), by = GT4_F32(b + P_A + 4);
        float ax = GT4_F32(a0 + P_A), ay = GT4_F32(a0 + P_A + 4), nx = GT4_F32(a1 + P_A), ny = GT4_F32(a1 + P_A + 4);
        float rx = bx - ax, ry = by - ay, ex = nx - ax, ey = ny - ay;
        float dy_ = 0, dx_ = 0, d = 0;
        int found = 0, edges = 0;
        if (!(ex * ry - ey * rx < 0)) {
            for (;;) {
                if (!found) {
                    float s0 = cx * ry - cy * rx, s1, den, f = 0;
                    if (!(s0 < 0)) {
                        s1 = cx * (by - ny) - cy * (bx - nx);
                        if (!(0 < s1)) {
                            den = s0 - s1;
                            if (den != 0) f = s0 / den;
                            found = 1;
                            dy_ = f * ey - ry;
                            dx_ = f * ex - rx;
                            d = dx_ * cx + dy_ * cy;
                        }
                    }
                }
                edges++;
                if (edges >= 8) break;
                a0 = a1;
                a1 = a1 + POINT_SIZE;
                ax = GT4_F32(a0 + P_A); ay = GT4_F32(a0 + P_A + 4);
                nx = GT4_F32(a1 + P_A); ny = GT4_F32(a1 + P_A + 4);
                rx = bx - ax; ry = by - ay; ex = nx - ax; ey = ny - ay;
                if (ex * ry - ey * rx < 0) break;
            }
        }
        if (edges == 8 && deep < d) {
            deep = d;
            px = dx_;
            py = dy_;
        }
    }
    if (0 < deep) {
        py = py * 0.5f;
        px = px * 0.5f;
        shift(o, px, py);
        shift(h, -px, -py);
    }
}

/* func_0045E9D8: points held against another car's edge (flag 2) keep to
   it: the gap closed half by each car, and the closing speed across the
   edge shared out. A point off the ends of the edge lets go; one outside
   it lets go but still shares the speed. */
static void car_contacts(uint32_t h)
{
    if (!HU(H_CARS)) return;
    for (int k = 0; k < 8; k++) {
        uint32_t p = point(h, k), fl = GT4_U32(p + P_FLAGS), o, e, e0, e1;
        float rx, ry, dx, dy, inv, t, vx, vy;
        if (!(fl & 2)) continue;
        o = HU(H_HULLS) + GT4_U32(p + P_CAR) * HULL_SIZE;
        e = GT4_U32(p + P_CAR_EDGE);
        e0 = point(o, (int)e);
        e1 = point(o, (int)((e + 1) & 7));
        rx = GT4_F32(p + P_A) - GT4_F32(e0 + P_A);
        ry = GT4_F32(p + P_A + 4) - GT4_F32(e0 + P_A + 4);
        dx = GT4_F32(e1 + P_A) - GT4_F32(e0 + P_A);
        dy = GT4_F32(e1 + P_A + 4) - GT4_F32(e0 + P_A + 4);
        inv = 1.0f / (dx * dx + dy * dy);
        t = (rx * dx + ry * dy) * inv;
        if (t < 0 || 1.0f < t) {
            GT4_U32(p + P_FLAGS) = fl & ~2u;
            continue;
        }
        if (0 < rx * dy - ry * dx) {
            GT4_U32(p + P_FLAGS) = fl & ~2u;
        } else {
            float cx = dx * t - rx, cy = dy * t - ry;
            cx = cx * 0.5f;
            cy = cy * 0.5f;
            HF(H_POS + 4) = HF(H_POS + 4) + cy;
            HF(H_POS) = HF(H_POS) + cx;
            GT4_F32(o + H_POS) = GT4_F32(o + H_POS) - cx;
            GT4_F32(o + H_POS + 4) = GT4_F32(o + H_POS + 4) - cy;
        }
        vx = HF(H_VEL) - GT4_F32(o + H_VEL);
        vy = HF(H_VEL + 4) - GT4_F32(o + H_VEL + 4);
        if (vx * dy - vy * dx < 0) {
            float s = (vx * dx + vy * dy) * inv, tx, ty;
            tx = dx * s - vx;
            ty = dy * s - vy;
            tx = tx * 0.5f;
            ty = ty * 0.5f;
            HF(H_VEL + 4) = HF(H_VEL + 4) + ty;
            HF(H_VEL) = HF(H_VEL) + tx;
            GT4_F32(o + H_VEL) = GT4_F32(o + H_VEL) - tx;
            GT4_F32(o + H_VEL + 4) = GT4_F32(o + H_VEL + 4) - ty;
        }
    }
}

/* The car-against-car hit found by the pair test (func_0045ECB0): this
   car's edge H_HIT_EDGE met the other car's (H_HIT_CAR) point
   H_HIT_OTHER; H_HIT_ARM is where, in this car's frame. */

/* func_0045F5F8: the impulse of the hit, along the normal of this car's
   edge (out) and its size (returned). m, mo: the two cars' rotations. */
static float car_impulse(uint32_t h, float out[2], const float m[4], const float mo[4])
{
    uint32_t o = HU(H_HULLS) + HU(H_HIT_CAR) * HULL_SIZE;
    uint32_t ea = edge(h, (int)HU(H_HIT_EDGE)), eb = edge(o, (int)HU(H_HIT_OTHER));
    float e = GT4_F32(CAR_RESTITUTION);
    float dx = HF(H_HIT_ARM), dy = HF(H_HIT_ARM + 4);
    float ra0 = m[0] * dy + m[1] * -dx, ra1 = m[2] * dy + m[3] * -dx;
    float nx = GT4_F32(ea + E_NORMAL), ny = GT4_F32(ea + E_NORMAL + 4);
    float cva[2], cvb[2], wa[2], wb[2], rb0, rb1, ka, kb, rel[2], den, s[2], j = 0;
    cva[1] = HF(H_VEL + 4) - HF(H_YAW_RATE) * ra1;
    cva[0] = HF(H_VEL) - HF(H_YAW_RATE) * ra0;
    wa[0] = m[0] * nx + m[1] * ny;
    wa[1] = m[2] * nx + m[3] * ny;
    ka = -(dx * ny - dy * nx);
    rb0 = mo[0] * GT4_F32(eb + 4) + mo[1] * -GT4_F32(eb);
    rb1 = mo[2] * GT4_F32(eb + 4) + mo[3] * -GT4_F32(eb);
    cvb[0] = GT4_F32(o + H_VEL) - GT4_F32(o + H_YAW_RATE) * rb0;
    wb[0] = mo[0] * wa[0] + mo[2] * wa[1];
    cvb[1] = GT4_F32(o + H_VEL + 4) - GT4_F32(o + H_YAW_RATE) * rb1;
    ka = ka * HF(H_INV_INERTIA);
    wb[1] = mo[1] * wa[0] + mo[3] * wa[1];
    rel[0] = cva[0] - cvb[0];
    rel[1] = cva[1] - cvb[1];
    kb = -(GT4_F32(eb) * wb[1] - GT4_F32(eb + 4) * wb[0]);
    rb0 = mo[0] * GT4_F32(eb + 4) + mo[1] * -GT4_F32(eb);
    kb = kb * GT4_F32(o + H_INV_INERTIA);
    rb1 = mo[2] * GT4_F32(eb + 4) + mo[3] * -GT4_F32(eb);
    s[0] = ka * ra0 + kb * rb0;
    s[1] = ka * ra1 + kb * rb1;
    den = (HF(H_INV_MASS) + GT4_F32(o + H_INV_MASS)) + (s[0] * wa[0] + s[1] * wa[1]);
    if (den != 0) j = (-(e + 1.0f) * (rel[0] * wa[0] + rel[1] * wa[1])) / den;
    out[0] = j * wa[0];
    out[1] = j * wa[1];
    return j;
}

/* func_0045F928: two outlines still into each other along the edge's
   normal are moved apart, half each. */
static void car_separate(uint32_t h, const float m[4], const float mo[4])
{
    uint32_t o = HU(H_HULLS) + HU(H_HIT_CAR) * HULL_SIZE;
    uint32_t ea = edge(h, (int)HU(H_HIT_EDGE)), eb = edge(o, (int)HU(H_HIT_OTHER));
    float pb0 = mo[0] * GT4_F32(eb) + mo[1] * GT4_F32(eb + 4);
    float pb1 = mo[2] * GT4_F32(eb) + mo[3] * GT4_F32(eb + 4);
    float nx = m[0] * GT4_F32(ea + E_NORMAL) + m[1] * GT4_F32(ea + E_NORMAL + 4);
    float ny = m[2] * GT4_F32(ea + E_NORMAL) + m[3] * GT4_F32(ea + E_NORMAL + 4);
    float px = (GT4_F32(o + H_POS) - HF(H_POS)) + pb0, py = (GT4_F32(o + H_POS + 4) - HF(H_POS + 4)) + pb1;
    float d = (nx * px + ny * py) + GT4_F32(ea + E_OFFSET), cx, cy;
    if (d <= 0) return;
    d = -d;
    cy = d * ny;
    cx = d * nx;
    cy = cy * 0.5f;
    cx = cx * 0.5f;
    GT4_F32(o + H_POS) = GT4_F32(o + H_POS) + cx;
    GT4_F32(o + H_POS + 4) = GT4_F32(o + H_POS + 4) + cy;
    HF(H_POS) = HF(H_POS) - cx;
    HF(H_POS + 4) = HF(H_POS + 4) - cy;
}

/* func_0045F340: this car's edge hit the other car's point: the impulse to
   both (speed and spin), the two pushed apart, the point held against
   the edge from now on (flag 2), and the knock for damage and sound. */
static void car_hit(uint32_t h)
{
    uint32_t o = HU(H_HULLS) + HU(H_HIT_CAR) * HULL_SIZE, q = point(o, (int)HU(H_HIT_OTHER));
    float m[4], mo[4], imp[2], la[2], lb[2], f, invm;
    m[0] = gt4_sinf(HF(H_YAW));
    m[2] = gt4_cosf(HF(H_YAW));
    m[1] = -m[2];
    m[3] = m[0];
    mo[0] = gt4_sinf(GT4_F32(o + H_YAW));
    mo[2] = gt4_cosf(GT4_F32(o + H_YAW));
    mo[1] = -mo[2];
    mo[3] = mo[0];
    f = car_impulse(h, imp, m, mo);
    if (f < 0) f = -f;
    f = f / 15000.0f;
    HF(H_IMPACT) = HF(H_IMPACT) + f;
    GT4_F32(o + H_IMPACT) = GT4_F32(o + H_IMPACT) + f;
    car_separate(h, m, mo);
    GT4_U32(q + P_FLAGS) |= 2;
    GT4_U32(q + P_CAR) = HU(H_INDEX);
    GT4_U32(q + P_CAR_EDGE) = HU(H_HIT_EDGE);
    invm = HF(H_INV_MASS);
    la[0] = m[0] * imp[0] + m[2] * imp[1];
    HF(H_VEL + 4) = HF(H_VEL + 4) + imp[1] * invm;
    HF(H_VEL) = HF(H_VEL) + imp[0] * invm;
    la[1] = m[1] * imp[0] + m[3] * imp[1];
    HF(H_YAW_RATE) = HF(H_YAW_RATE) + (HF(H_HIT_ARM) * la[1] - HF(H_HIT_ARM + 4) * la[0]) * HF(H_INV_INERTIA);
    invm = GT4_F32(o + H_INV_MASS);
    GT4_F32(o + H_VEL) = GT4_F32(o + H_VEL) - imp[0] * invm;
    GT4_F32(o + H_VEL + 4) = GT4_F32(o + H_VEL + 4) - imp[1] * invm;
    lb[0] = mo[0] * imp[0] + mo[2] * imp[1];
    lb[1] = mo[1] * imp[0] + mo[3] * imp[1];
    GT4_F32(o + H_YAW_RATE) = GT4_F32(o + H_YAW_RATE)
                            - (GT4_F32(q + P_LOCAL) * lb[1] - GT4_F32(q + P_LOCAL + 4) * lb[0]) * GT4_F32(o + H_INV_INERTIA);
    gt4_car_impact(HU(H_CAR), 1, (int32_t)(HU(H_HIT_EDGE) + 8), f);   /* the knock: damage, vibration */
    gt4_car_impact(GT4_U32(o + H_CAR), 1, (int32_t)HU(H_HIT_OTHER), f);
}

/* func_0045ECB0: the first time in the move A -> B that one of car j's
   points crosses one of this car's edges from outside, earlier than this
   car's own first hit: which edge (H_HIT_EDGE), whose point (H_HIT_CAR,
   H_HIT_OTHER), when (H_HIT_T) and where in this car's frame
   (H_HIT_ARM). Only for cars whose moves come near each other. */
static void pair_test(uint32_t h, uint32_t j)
{
    uint32_t o = HU(H_HULLS) + j * HULL_SIZE;
    float bx, by, sx, sy, s2, dx, dy, t, cx, cy, ca, sa, cb, sb, c0, s0, c1, s1;
    float l0x, l0y, l1x, l1y, best = 2.0f, arm_x = 0, arm_y = 0;
    int bp = -1, be = -1;
    if (!HU(H_CARS) || !GT4_U32(o + H_CARS)) return;
    if (4.0f < fabsf(HF(H_POS + 8) - GT4_F32(o + H_POS + 8))) return;
    bx = GT4_F32(o + H_BOX + 8) - HF(H_BOX);
    by = GT4_F32(o + H_BOX + 0xc) - HF(H_BOX + 4);
    if (bx < 0 || by < 0) return;
    if (GT4_F32(o + H_BOX_SIZE) + HF(H_BOX_SIZE) < bx) return;
    if (GT4_F32(o + H_BOX_SIZE + 4) + HF(H_BOX_SIZE + 4) < by) return;
    /* nearest the two centres come over the move */
    sx = GT4_F32(o + H_STEP) - HF(H_STEP);
    sy = GT4_F32(o + H_STEP + 4) - HF(H_STEP + 4);
    s2 = sx * sx + sy * sy;
    dx = GT4_F32(o + H_POS) - HF(H_POS);
    dy = GT4_F32(o + H_POS + 4) - HF(H_POS + 4);
    t = -dx * sx - dy * sy;
    if (t <= 0) { cx = dx; cy = dy; }
    else if (s2 <= t) { cx = dx + sx; cy = dy + sy; }
    else { t = t / s2; cy = dy + sy * t; cx = dx + sx * t; }
    if (HF(H_PAIR_REACH + j * 4) < cx * cx + cy * cy) return;
    /* the other car's pose in this car's frame, at A and at B */
    {
        float px = GT4_F32(o + H_POS_B) - HF(H_POS_B), py = GT4_F32(o + H_POS_B + 4) - HF(H_POS_B + 4);
        float yaw0 = GT4_F32(o + H_YAW) - HF(H_YAW), yaw1 = GT4_F32(o + H_YAW_B) - HF(H_YAW_B);
        ca = gt4_cosf(-HF(H_YAW));
        sa = gt4_sinf(-HF(H_YAW));
        cb = gt4_cosf(-HF(H_YAW_B));
        sb = gt4_sinf(-HF(H_YAW_B));
        l0y = ca * dx + sa * dy;
        l1y = cb * px + sb * py;
        l0x = sa * dx - ca * dy;
        l1x = sb * px - cb * py;
        c0 = gt4_cosf(yaw0);
        s0 = gt4_sinf(yaw0);
        c1 = gt4_cosf(yaw1);
        s1 = gt4_sinf(yaw1);
    }
    for (int q = 0; q < 8; q++) {
        uint32_t eb = edge(o, q);
        float ex = GT4_F32(eb), ey = GT4_F32(eb + 4), qx0, qy0, qx1, qy1;
        if (GT4_U32(point(o, q) + P_FLAGS)) continue;
        qx0 = (s0 * ex - c0 * ey) + l0x;
        qy0 = (c0 * ex + s0 * ey) + l0y;
        qx1 = (s1 * ex - c1 * ey) + l1x;
        qy1 = (c1 * ex + s1 * ey) + l1y;
        for (int k = 0; k < 8; k++) {
            uint32_t ea = edge(h, k);
            float nx = GT4_F32(ea + E_NORMAL), ny = GT4_F32(ea + E_NORMAL + 4), off = GT4_F32(ea + E_OFFSET);
            float d1 = (qx1 * nx + qy1 * ny) + off, d0 = (qx0 * nx + qy0 * ny) + off;
            float mx, my, a, b, f, at;
            if (0 < d1 || d0 < 0) continue;
            mx = qx1 - qx0;
            my = qy1 - qy0;
            a = (GT4_F32(ea) - qx0) * my - (GT4_F32(ea + 4) - qy0) * mx;
            b = (GT4_F32(ea + 8) - qx0) * my - (GT4_F32(ea + 0xc) - qy0) * mx;
            if (a < 0 && b < 0) continue;
            if (0 < a && 0 < b) continue;
            f = 1.0f;
            if (d0 - d1 != 0) f = d0 / (d0 - d1);
            at = (HF(H_T1) - HF(H_T0)) * f + HF(H_T0);
            if (at < 0 || 1.0f < at || !(at < best)) continue;
            best = at;
            bp = q;
            be = k;
            arm_y = qy0 + my * f;
            arm_x = qx0 + mx * f;
        }
    }
    if (bp < 0) return;
    if ((HU(0x3f0) || HU(H_HIT_POINT)) && HF(H_HIT_T) < best) return;
    HU(H_HIT_EDGE) = (uint32_t)be;
    HF(H_HIT_T) = best;
    HU(H_HIT_CAR) = j;
    HU(H_HIT_OTHER) = (uint32_t)bp;
    HU(0x3f0) = 1;
    HF(H_HIT_ARM) = arm_x;
    HF(H_HIT_ARM + 4) = arm_y;
    HU(0x440) = 1;
    if (be < 3) HU(H_HIT_FRONT) = 1;
    else if ((uint32_t)(be - 4) < 3) HU(H_HIT_REAR) = 1;
    GT4_U32(o + 0x440) = 1;
    if (bp < 4) GT4_U32(o + H_HIT_FRONT) = 1;
    else GT4_U32(o + H_HIT_REAR) = 1;
}

/* func_00343BF0: a car the others go through (a ghost, or a driver of
   kind 4) */
static int passable(uint32_t car)
{
    return GT4_U8(car + 0x545) || GT4_U8(car + 0x14 + 0x1c) == 4;   /* func_00463BA8 */
}

/* func_0045BB60: all the cars' frame together, against the walls and each
   other. Each pass moves every car to the first hit of any of them (a
   wall's, 0x200 + car, or a car's), answers that one hit, and goes on. */
static void cars_walls(uint32_t solver)
{
    uint32_t conductor = GT4_U32(solver);
    int n = (int)GT4_U32(solver + 4), which;
    float best, t = 1.0f;
#define HULL(i) (solver + 0xc + (uint32_t)(i) * HULL_SIZE)
    for (int i = 0; i < n; i++) {
        uint32_t car = conductor + 0x6698 + (uint32_t)i * CAR_SIZE;   /* func_0045B8A8 */
        begin(HULL(i), GT4_F32(car + CAR_DT));
        set_cars(HULL(i), !passable(car));
    }
    for (int pass = 0; pass < 2; pass++) {
        n = (int)GT4_U32(solver + 4);
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                if (i != j) overlap(HULL(i), (uint32_t)j);
        for (int i = 0; i < n; i++) {                                 /* func_0045C578 */
            car_contacts(HULL(i));
            slide(HULL(i));
        }
        for (int i = 0; i < n; i++) predict(HULL(i));
        which = -1;
        best = 2.0f;
        for (int i = 0; i < n; i++) {
            uint32_t h = HULL(i);
            HU(0x3f0) = 0;                                            /* func_0045C648 */
            HF(H_SENSOR_T) = 2.0f;
            HU(H_HIT_POINT) = 0;
            HU(H_HIT_CORNER) = 0;
            HU(H_SENSOR) = 0;
            if (first_hit(h) && HF(H_HIT_T) < best) {
                best = HF(H_HIT_T);
                which = i + 0x200;
            }
            for (int j = 0; j < n; j++)
                if (i != j) pair_test(h, (uint32_t)j);
            if (HU(0x3f0) && HF(H_HIT_T) < best) {
                best = HF(H_HIT_T);
                which = i;
            }
        }
        for (int i = 0; i < n; i++) sensor_check(HULL(i), best);
        if (which < 0) break;
        t = best;
        for (int i = 0; i < n; i++) advance(HULL(i), best);
        if (which & 0x200) {
            uint32_t h = HULL(which & ~0x200);
            if (HU(H_HIT_POINT)) point_hit(h);
            else corner_hit(h);                                       /* an edge hits a wall's corner */
        } else {
            car_hit(HULL(which));
        }
    }
    n = (int)GT4_U32(solver + 4);
    for (int i = 0; i < n; i++) {
        uint32_t h = HULL(i);
        wall_turn(h, t);
        push_out(h);
        finish(h);
        shake(h, conductor + 0x6698 + (uint32_t)i * CAR_SIZE);
    }
#undef HULL
}

/* func_003F4B20: the cars' state into the solver. */
static void walls_in(uint32_t conductor)
{
    int n = (int)GT4_U32(conductor + CONDUCTOR_CARS);
    for (int i = 0; i < n; i++) {
        uint32_t car = conductor + 0x6698 + (uint32_t)i * CAR_SIZE, h = conductor + WALLS + 0xc + (uint32_t)i * HULL_SIZE;
        for (int k = 0; k < 3; k++) {
            HF(H_IN_POS + k * 4) = GT4_F32(car + CAR_POSITION + k * 4);
            HF(H_IN_VEL + k * 4) = GT4_F32(car + CAR_VELOCITY + k * 4);
        }
        HF(H_IN_ANGLES + 4) = GT4_F32(car + CAR_ANGLES + 4);
        HF(H_IN_RATES + 8) = GT4_F32(car + CAR_YAW_RATE);
        HF(H_IN_ANGLES + 0) = GT4_F32(car + CAR_ANGLES + 0);
        HF(H_IN_RATES + 4) = GT4_F32(car + CAR_ROLL_RATE);
        HF(H_IN_ANGLES + 8) = GT4_F32(car + CAR_ANGLES + 8);
        HF(H_IN_RATES + 0) = GT4_F32(car + CAR_PITCH_RATE);
        HU(H_GROUND_ID) = GT4_U32(car + 0x5ac);
    }
}

static float setup_sqrt(float x) { return x == 0 ? x : sqrtf(fabsf(x)); }

/* div.s as the EE does it: x/0 is the largest float, signed as x^0 */
static float ee_div(float a, float b)
{
    uint32_t sa, sb;
    if (b != 0) return a / b;
    memcpy(&sa, &a, 4);
    memcpy(&sb, &b, 4);
    return ((sa ^ sb) >> 31) ? K(0xff7fffffu) : K(0x7f7fffffu);
}

/* func_0045C0F8: the solver's constants (D_00846630..3C) from k. */
static void solver_constants(float k)
{
    float a = k * 0.0f, b = k * K(0x40e00000u), c = k * K(0x3e4ccccc);
    GT4_F32(0x00846634u) = a + K(0x3d4ccccc);
    GT4_F32(0x00846638u) = K(0x41200000u) - b;
    GT4_F32(0x0084663cu) = c + K(0x3f000000u);
    GT4_F32(0x00846630u) = a + K(0x3f000000u);
}

/* func_0045C170: car i's hull h: its eight-sided outline (corners at
   h+8 + k*0x44, from the half width p0, front p4, half length p8/2 and the
   chamfers p8/5), each side's normal and plane (h+0x228 + k*0x2C), the
   radius (h+0x388), the height p48, the mass p114 times D_00846638. */
static void hull_setup(uint32_t h, uint32_t i, uint32_t course, uint32_t car,
                       float w, float f, float l, float cf, float cr, float ht, float m)
{
    float hl, r = 0, x0, x1, x2, y0, y1, y2, inv_h, inv_m, mass;
    GT4_U32(h + 0x44c) = i;
    GT4_F32(h + 0x48c) = w;
    GT4_F32(h + 0x490) = f;
    GT4_F32(h + 0x4a0) = cf;
    GT4_F32(h + 0x4a4) = cr;
    GT4_F32(h + 0x494) = hl = l * K(0x3f000000u);
    GT4_F32(h + 0x498) = ht;
    GT4_U32(h + 0x488) = course;
    mass = m * GT4_F32(0x00846638u);                     /* func_0045C0D8 */
    inv_h = ee_div(K(0x3f800000u), ht);
    x0 = w - cf;                                         /* f9 */
    x1 = cf - hl;                                        /* f7 */
    y0 = cr - f;                                         /* f3 */
    y1 = hl - cr;                                        /* f8 */
    x2 = hl - cf;                                        /* f1 */
    y2 = cr - hl;                                        /* f2 */
    inv_m = ee_div(K(0x3f800000u), mass);
    GT4_F32(h + 0x94) = w;
    GT4_F32(h + 0x118) = -hl;
    GT4_F32(h + 0x1e4) = hl;
    GT4_F32(h + 0xd8) = x0;
    GT4_F32(h + 0x90) = x1;
    GT4_F32(h + 0) = inv_h;
    GT4_F32(h + 0x1a4) = -f;
    GT4_F32(h + 0x1e8) = y0;
    GT4_F32(h + 0x1a0) = y1;
    GT4_F32(h + 0x4c) = x2;
    GT4_F32(h + 0x15c) = y2;
    GT4_F32(h + 4) = inv_m;
    GT4_F32(h + 0x49c) = mass;
    GT4_F32(h + 8) = hl;
    GT4_F32(h + 0xc) = x0;
    GT4_F32(h + 0x50) = w;
    GT4_F32(h + 0xd4) = -hl;
    GT4_F32(h + 0x11c) = y0;
    GT4_F32(h + 0x160) = -f;
    GT4_U32(h + 0x450) = car;
    for (uint32_t k = 0; k < 8; k++) {
        uint32_t p = h + 8 + k * 0x44, q = h + 8 + ((k + 1) & 7) * 0x44, s = h + k * 0x2c;
        float x = GT4_F32(p), y = GT4_F32(p + 4), len, dx, dy, nx, ny;
        len = setup_sqrt(x * x + y * y);
        GT4_F32(p + 8) = ee_div(x, len);
        GT4_F32(p + 0xc) = ee_div(y, len);
        GT4_U32(p + 0x20) = 0;
        GT4_U32(p + 0x2c) = 0;
        GT4_U32(s + 0x244) = 0;
        GT4_U32(s + 0x228) = GT4_U32(p);
        GT4_U32(s + 0x22c) = GT4_U32(p + 4);
        GT4_U32(s + 0x230) = GT4_U32(q);
        GT4_U32(s + 0x234) = GT4_U32(q + 4);
        dx = GT4_F32(q) - x;
        dy = GT4_F32(q + 4) - y;
        len = setup_sqrt(dy * dy + (-dx) * (-dx));
        nx = ee_div(-dx, len);
        ny = ee_div(dy, len);
        GT4_F32(s + 0x238) = ny;
        GT4_F32(s + 0x23c) = nx;
        GT4_F32(s + 0x240) = -ny * x - nx * y;
        len = x * x + y * y;
        if (r < len) r = len;
    }
    GT4_F32(h + 0x388) = setup_sqrt(r);
    GT4_U32(h + 0x430) = 1;
    GT4_U32(h + 0x4ec) = 0;
    GT4_U32(h + 0x3ec) = 0;
    GT4_U32(h + 0x4e0) = 0;
    GT4_U32(h + 0x4e4) = 0;
    GT4_U32(h + 0x4e8) = 0;
}

/* func_0045B8C0: the solver of the cars against each other (conductor +
   0xDA6C): its constants, each car's hull from its parameters
   (func_0045B978), and each pair's reach (func_0045BAE0, func_0045C438:
   the sum of the two radii and its square). */
void gt4_walls_setup(uint32_t cond)
{
    uint32_t s = cond + WALLS, course = GT4_U32(cond + 0xcbd8u);
    int n;
    solver_constants(0.0f);
    GT4_U32(s + 8) = course;
    GT4_U32(s) = cond;
    GT4_U32(s + 4) = GT4_U32(cond + CONDUCTOR_CARS);
    for (uint32_t i = 0; (int32_t)i < (int32_t)GT4_U32(s + 4); i++) {
        uint32_t car = cond + 0x6698u + i * CAR_SIZE, p = GT4_U32(car + CAR_PARAMS);
        float l = GT4_F32(p + 8);
        hull_setup(s + 0xc + i * HULL_SIZE, i, course, car, GT4_F32(p), GT4_F32(p + 4), l,
                   l * K(0x3e4ccccc), l * K(0x3e4ccccc), GT4_F32(p + 0x48), GT4_F32(p + 0x114));
    }
    n = (int)GT4_U32(GT4_U32(s) + CONDUCTOR_CARS);
    GT4_U32(s + 4) = (uint32_t)n;
    for (int a = 0; a < n; a++) {
        uint32_t h = s + 0xc + (uint32_t)a * HULL_SIZE;
        GT4_U32(h + 0x4a8) = s + 0xc;
        for (int b = 0; b < n; b++) {
            float d = GT4_F32(h + 0x388) + GT4_F32(s + 0xc + (uint32_t)b * HULL_SIZE + 0x388);
            GT4_F32(h + 0x4ac + (uint32_t)b * 4) = d;
            GT4_F32(h + 0x4c4 + (uint32_t)b * 4) = d * d;
        }
    }
}

void gt4_walls_in(uint32_t conductor) { walls_in(conductor); }

/* func_003F4C18: back into the cars, with the kick for the view. */
static void walls_out(uint32_t conductor)
{
    int n = (int)GT4_U32(conductor + CONDUCTOR_CARS);
    for (int i = 0; i < n; i++) {
        uint32_t car = conductor + 0x6698 + (uint32_t)i * CAR_SIZE, h = conductor + WALLS + 0xc + (uint32_t)i * HULL_SIZE;
        uint32_t b = conductor + SHAKE_BLOCK + (uint32_t)i * 0x30;
        if (!GT4_U8(SHAKE_ON)) GT4_U32(b + 0x24) = 0;
        else if (GT4_U32(b + 0x24)) GT4_U32(b + 0x24) = 0;
        else {
            float d[3], fx = 0, fy = 0, m;
            for (int k = 0; k < 3; k++) d[k] = HF(H_IN_VEL + k * 4) - GT4_F32(car + CAR_VELOCITY + k * 4);
            for (int k = 0; k < 3; k++) {
                fy = fy + GT4_F32(car + CAR_MATRIX + 0xc + k * 4) * d[k];
                fx = fx + GT4_F32(car + CAR_MATRIX + k * 4) * d[k];
            }
            m = fx * fx + fy * fy;
            m = m == 0 ? m : sqrtf(fabsf(m));
            if (GT4_F32(SHAKE_PARAM + 0x20) < m) {
                float g = 1.0f, cap = GT4_F32(SHAKE_PARAM + 0x24);
                if (cap < m) { g = cap / m; m = cap; }
                g = g * ((GT4_F32(SHAKE_PARAM + 0x28) + GT4_F32(SHAKE_PARAM + 0x30) * (m - GT4_F32(SHAKE_PARAM + 0x20))) / m);
                GT4_U32(b + 0x24) = 1;
                GT4_F32(b + 0x1c) = fx * g;
                GT4_F32(b + 0x20) = fy * g;
            }
        }
        for (int k = 0; k < 3; k++) {
            GT4_F32(car + CAR_POSITION + k * 4) = HF(H_IN_POS + k * 4);
            GT4_F32(car + CAR_VELOCITY + k * 4) = HF(H_IN_VEL + k * 4);
        }
        GT4_F32(car + CAR_ANGLES + 8) = HF(H_IN_ANGLES + 8);
        GT4_F32(car + CAR_YAW_RATE) = HF(H_IN_RATES + 8);
    }
}

/* func_0034DA70: a race with cars against each other. */
void gt4_walls_cars(uint32_t conductor)
{
    walls_in(conductor);
    cars_walls(conductor + WALLS);
    walls_out(conductor);
}

/* func_0034DAA8 */
void gt4_walls(uint32_t conductor, int first)
{
    walls_in(conductor);
    for (int i = first; i < (int)GT4_U32(conductor + CONDUCTOR_CARS); i++) car_walls(conductor, i);
    walls_out(conductor);
}

/* The replay cameras' wall check, in the course's frame (x, height, z). */
typedef struct { float b[3], a[3]; int hit; float at[3], t; } camera_segment;

/* func_003FE4C8: the move b -> a crossing wall w from its front (the side
   its left normal faces) to its back, between its two ends */
static int camera_cross(uint32_t w, camera_segment *s)
{
    uint32_t v = wall_next(w);
    float wx = GT4_F32(w), wz = GT4_F32(w + 8), nx = GT4_F32(v), nz = GT4_F32(v + 8);
    float ex = nx - wx, ez = nz - wz, fa, fb, dx, dz, t;
    fa = ex * (s->a[2] - wz) - ez * (s->a[0] - wx);
    if (fa < 0) return 0;
    fb = ex * (s->b[2] - wz) - ez * (s->b[0] - wx);
    if (0 < fb) return 0;
    dx = s->a[0] - s->b[0];
    dz = s->a[2] - s->b[2];
    if (dx * (wz - s->a[2]) - dz * (wx - s->a[0]) < 0) return 0;
    if (0 < dx * (nz - s->a[2]) - dz * (nx - s->a[0])) return 0;
    t = fa / (fa - fb);
    for (int k = 0; k < 3; k++) s->at[k] = s->a[k] - (s->a[k] - s->b[k]) * t;
    s->t = t;
    return 1;
}

/* func_00395A38 -> func_003E6A58 with func_003FE680: the walls listed by
   ground triangle id (found from 10 km up at the box's middle when unset)
   that come into the box x0..x1, z0..z1; the first crossing is kept */
static void camera_walls(uint32_t mesh, int32_t id, float x0, float z0, float x1, float z1, camera_segment *s)
{
    uint32_t list, from, to;
    if (id < 0) {
        gt4_ray_hit hit;
        float p[3] = { (x0 + x1) * 0.5f, 10000.0f, (z0 + z1) * 0.5f };
        if (!gt4_course_ray_down(mesh, &hit, p)) return;
        id = (int32_t)((uint16_t)hit.tri | (uint32_t)(uint16_t)hit.leaf << 16);
    }
    if ((int16_t)((uint32_t)id >> 16) < 0) return;
    list = GT4_U32(GT4_U32(mesh + 0x78) + ((uint32_t)id & 0xffff) * 12 + 8);
    from = list & 0xffffff;
    to = from + (list >> 24);
    for (uint32_t i = from; i < to; i++) {
        int16_t idx = (int16_t)GT4_U16(GT4_U32(mesh + 0x8c) + i * 2);
        uint32_t w = GT4_U32(mesh + 0x84) + (uint32_t)(idx * 16);
        float x = GT4_F32(w), z = GT4_F32(w + 8);
        int out = (x < x0) | (x1 < x) << 1 | (z < z0) << 2 | (z1 < z) << 3;
        if (out) {
            uint32_t v = wall_next(w);
            x = GT4_F32(v); z = GT4_F32(v + 8);
            if (out & ((x < x0) | (x1 < x) << 1 | (z < z0) << 2 | (z1 < z) << 3)) continue;
        }
        if (!s->hit) s->hit = camera_cross(w, s);
    }
}

/* func_003FCB20: the ground triangle under p (from 10 m up), -1 none */
int32_t gt4_camera_ground_id(uint32_t course, const float p[3])
{
    gt4_ray_hit hit;
    float q[3] = { p[0], p[1] + 10.0f, p[2] };
    if (!gt4_course_ray_down(GT4_U32(GT4_U32(course + 4) + 0xdc), &hit, q)) return -1;
    return (int32_t)((uint16_t)hit.tri | (uint32_t)(uint16_t)hit.leaf << 16);
}

/* func_003FE7B0: from -> to stretched by margin, in steps of 4 m (ten at
   most), each step's walls in the box of its move: at a wall out is
   pulled back margin toward from, 1; otherwise out ends at the last step
   over ground (*id its triangle) and 0 */
int gt4_camera_wall(uint32_t course, float margin, const float from[3], const float to[3], float out[3], int32_t *id)
{
    uint32_t mesh = GT4_U32(GT4_U32(course + 4) + 0xdc);
    float dir[3], prev[3], len, total, along = 0;
    int last = 0, steps = 0;
    for (int k = 0; k < 3; k++) { out[k] = to[k]; dir[k] = to[k] - from[k]; prev[k] = from[k]; }
    len = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (len < 1e-4f) return 0;
    for (int k = 0; k < 3; k++) dir[k] *= (len + margin) / len;
    total = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    for (;;) {
        camera_segment s;
        int32_t ground;
        along += 4;
        if (total <= along) { along = total; last = 1; }
        for (int k = 0; k < 3; k++) { s.b[k] = prev[k]; s.a[k] = dir[k] * (along / total) + from[k]; }
        s.hit = 0;
        camera_walls(mesh, *id, fminf(s.b[0], s.a[0]), fminf(s.b[2], s.a[2]), fmaxf(s.b[0], s.a[0]), fmaxf(s.b[2], s.a[2]), &s);
        if (s.hit) {
            float v[3], l;
            for (int k = 0; k < 3; k++) v[k] = s.at[k] - from[k];
            l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
            if (0.001f < l) for (int k = 0; k < 3; k++) v[k] *= fmaxf(0, l - margin) / l;
            for (int k = 0; k < 3; k++) out[k] = from[k] + v[k];
            return 1;
        }
        ground = gt4_camera_ground_id(course, s.a);
        if (ground < 0) { for (int k = 0; k < 3; k++) out[k] = prev[k]; return 0; }
        *id = ground;
        for (int k = 0; k < 3; k++) out[k] = prev[k] = s.a[k];
        if (last) return 0;
        if (++steps >= 9) last = 1;
    }
}
