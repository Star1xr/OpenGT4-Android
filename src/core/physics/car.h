/* Car state in the game's layout (0x10a0 bytes per car, the array starts at
   conductor + 0x6698). Offsets are from the car; the original code mostly
   addresses a block at car + 0x104, so both are noted. Names are provisional
   until docs/analysis/PHYSICS.md confirms them. */
#ifndef OPENGT4_PHYS_CAR_H
#define OPENGT4_PHYS_CAR_H

#include "phys.h"

enum {
    CAR_SIZE          = 0x10a0,
    CAR_SESSION       = 0x004,  /* u32: session block (+4: flags 0x68 tyre wear, 0x69 heat?) */
    CAR_COURSE        = 0x008,  /* u32: course object ([+4]+0xdc: collision mesh) */
    CAR_PARAMS        = 0x010,  /* u32: address of the car parameters */
    CAR_POSITION      = 0x104,  /* position, m */
    CAR_MATRIX        = 0x110,  /* body rotation, 3 rows of 3 floats (+0x18: side axis?) */
    CAR_140           = 0x140,  /* two more rows (ground frame?) at 0x140 and 0x14c */
    CAR_148           = 0x148,  /* +0x104+0x44: factor of the weight term */
    CAR_158           = 0x158,
    CAR_160           = 0x160,
    CAR_WHEELS        = 0x164,  /* wheel array */
    CAR_SUSP_REACH    = 0x514,  /* [wheel]: travel that puts the tyre on the ground */
    CAR_LOAD_TARGET   = 0x524,  /* [wheel]: vertical force the wheel asks for */
    CAR_CAMBER_FORCE  = 0x534,  /* [wheel]: camber thrust, N */
    CAR_546           = 0x546,  /* +0x442 u8: driver kind? (3, 6 skip the clutch throttle cut) */
    CAR_PLAYER        = 0x544,  /* +0x440 u8: player (view block index) */
    CAR_VIBRATION_ON  = 0x545,  /* +0x441 u8 */
    CAR_TIME_SCALE    = 0x548,  /* +0x444 */
    CAR_VIEW_BLEND    = 0x554,  /* not in the +0x104 block */
    CAR_DT            = 0x54c,  /* +0x448: seconds per step */
    CAR_RATE          = 0x550,  /* +0x44c: steps per second */
    CAR_56A           = 0x56a,  /* +0x466 s8: mode (1, 2 = surface factors) */
    CAR_56B           = 0x56b,  /* +0x467 u8 */
    CAR_56C           = 0x56c,  /* +0x468: steering? */
    CAR_AVERAGE_SLIP  = 0x570,  /* +0x46c */
    CAR_THROTTLE_REQ  = 0x58c,  /* +0x488: throttle asked by the controls */
    CAR_BRAKE_PEDAL   = 0x590,  /* +0x48c */
    CAR_SUSP_SHORT    = 0x5b9,  /* +0x4b5 u8: bit per wheel that cannot reach the ground */
    CAR_5B6           = 0x5b6,  /* +0x4b2 u8 */
    CAR_HANDBRAKE     = 0x594,  /* +0x490 */
    CAR_THROTTLE      = 0x598,  /* +0x494: 0..1 */
    CAR_5BE           = 0x5be,  /* +0x4ba u16: nonzero = run func_0035C0B8 */
    CAR_5C0           = 0x5c0,  /* +0x4bc u8: nonzero = hold the car still */
    CAR_ECONOMY_TIMER = 0x5c1,  /* +0x4bd u8 */
    CAR_ECONOMY       = 0x5c2,  /* +0x4be u16: fuel economy shown */
    CAR_FUEL_USED     = 0x5c4,  /* +0x4c0 */
    CAR_ECONOMY_FUEL  = 0x5c8,  /* +0x4c4 */
    CAR_ECONOMY_DIST  = 0x5cc,  /* +0x4c8 */
    CAR_FUEL_TANK     = 0x5d0,  /* +0x4cc: > 0 = fuel mass decreases */
    CAR_EXTRA_MASS    = 0x5d4,  /* +0x4d0: fuel mass? */
    CAR_MASS_A        = 0x5d8,  /* +0x4d4: params mass + 0.74 * extra */
    CAR_MASS_B        = 0x5dc,  /* +0x4d8 */
    CAR_FUEL_BURST    = 0x5e0,  /* +0x4dc */
    CAR_FUEL_BURST_LEFT = 0x5e4,/* +0x4e0 */
    CAR_FUEL_BURST_N  = 0x5e8,  /* +0x4e4 u8 */
    CAR_ENGINE_TORQUE = 0x5ec,  /* +0x4e8: N*m */
    CAR_ENGINE_BRAKE  = 0x5f0,  /* +0x4ec: N*m */
    CAR_THROTTLE_IN   = 0x5f4,  /* +0x4f0 */
    CAR_ENGINE_RPM    = 0x5fe,  /* +0x4fa s16 */
    CAR_ENGINE_SPEED  = 0x600,  /* +0x4fc: rad/s */
    CAR_THROTTLE_OUT  = 0x604,  /* +0x500 */
    CAR_ENGINE_SPEED_IN = 0x610,/* +0x50c: engine speed when the clutch state was checked */
    CAR_WHEEL_ENGINE_SPEED = 0x614, /* +0x510: engine speed implied by the driven wheels */
    CAR_CLUTCH_REQ    = 0x618,  /* +0x514 */
    CAR_61C           = 0x61c,  /* +0x518: engine/wheel ratio for gearbox kinds 1, 2 */
    CAR_CLUTCH_TORQUE = 0x620,  /* +0x51c: |torque through the clutch| */
    CAR_GEAR          = 0x624,  /* +0x520 u8: 0 reverse, 1.. forward */
    CAR_LIMITER       = 0x625,  /* +0x521 u8: rev limiter cutting */
    CAR_CLUTCH_STATE  = 0x626,  /* +0x522 u8: 0 open, 1 engaged, 2 slipping, 3 shifting */
    CAR_62A           = 0x62a,  /* +0x526 u8: set to 12 on engagement */
    CAR_FRONT_SPEED   = 0x630,  /* +0x52c: [diff] driven speed, front then rear */
    CAR_REAR_SPEED    = 0x634,  /* +0x530 */
    CAR_DRIVE_TORQUE  = 0x638,  /* +0x534: [diff] drive torque, front then rear */
    CAR_SPEED         = 0x640,  /* +0x53c: m/s along the body */
    CAR_LAT_SPEED     = 0x644,  /* +0x540: m/s across */
    CAR_VELOCITY      = 0x648,  /* +0x544: velocity vector, m/s */
    CAR_654           = 0x654,  /* +0x550: acceleration along the body */
    CAR_LAT_ACCEL     = 0x658,  /* +0x554: acceleration across the body */
    CAR_LAT_G_RATE    = 0x660,  /* +0x55c */
    CAR_LAT_G         = 0x664,  /* +0x560: filtered lateral acceleration */
    CAR_VERT_ACCEL    = 0x65c,  /* +0x558: acceleration along the body's vertical axis */
    CAR_668           = 0x668,  /* +0x564 */
    CAR_ANGLES        = 0x66c,  /* +0x568: body angles (pitch?, roll?, yaw) */
    CAR_GROUND_PITCH  = 0x678,  /* +0x574: pitch of the ground under the car */
    CAR_GROUND_ROLL   = 0x67c,  /* +0x578: roll of the ground (subtracted from the body roll) */
    CAR_67C           = 0x67c,
    CAR_YAW_RATE      = 0x680,  /* +0x57c: rad/s, within +-2 pi */
    CAR_PITCH_RATE    = 0x684,  /* +0x580 (provisional) */
    CAR_ROLL_RATE     = 0x688,  /* +0x584 (provisional) */
    CAR_YAW_ACCEL     = 0x68c,  /* +0x588 */
    CAR_GRIP_USE      = 0x690,  /* +0x58c: lateral acceleration against the grip */
    CAR_698           = 0x698,  /* +0x594: weight of the extra downforce */
    CAR_SLIP_TARGET   = 0x6a0,  /* +0x59c: slipstream target (0 = none) */
    CAR_SLIP          = 0x6a4,  /* +0x5a0: slipstream amount 0..1 */
    CAR_DRAG          = 0x6a8,  /* +0x5a4: drag force, N (negative forward) */
    CAR_DOWNFORCE     = 0x6ac,  /* +0x5a8: front, rear, N */
    CAR_ROLLING       = 0x6b4,  /* +0x5b0: rolling resistance, N */
    CAR_6B8           = 0x6b8,  /* +0x5b4 */
    CAR_6BC           = 0x6bc,  /* +0x5b8 */
    CAR_6C0           = 0x6c0,  /* +0x5bc */
    CAR_6C7           = 0x6c7,  /* +0x5c3 s8: 2 = drag factor 20 */
    CAR_ABS_SPEED     = 0x6c8,  /* +0x5c4 */
    CAR_TOP_SPEED     = 0x6cc,  /* +0x5c8 */
    CAR_SPEEDO        = 0x6d0,  /* +0x5cc: speed shown */
    CAR_DISTANCE      = 0x6d4,  /* +0x5d0: distance travelled, m */
    CAR_ODO_FRACTION  = 0x6d8,  /* +0x5d4: metres not yet counted (the count is the u32 at CAR_ENGINE_WEAR) */
    CAR_ENGINE_WEAR   = 0x6dc,  /* gt4_engine_wear */
    CAR_COUNTDOWN     = 0x6ff,  /* +0x5fb u8: counts down to 0 (0xff = held) */
    CAR_VIEW_YAW_LAG  = 0x700,  /* +0x5fc */
    CAR_VIEW_PITCH_LAG = 0x704, /* +0x600 */
    CAR_VIBRATION     = 0x708,  /* +0x604 [3] u8: vibration outputs 0..127 */
    CAR_70A           = 0x70a,  /* +0x606 u8 */
    CAR_TACHO         = 0x738,  /* tachometer needle, rpm */
    CAR_TACHO_RATE    = 0x73c,
    CAR_YAW_FILTER    = 0x740,  /* smoothed yaw acceleration */
    CAR_YAW_FILTER_RATE = 0x744,
    CAR_GEAR_REQUEST  = 0x774,  /* [4] s8 */
    CAR_7B0           = 0x7b0,  /* distance in mode 1 */
    CAR_INPUTS        = 0x778,  /* [4]: filtered inputs (2 throttle, 3 brake) */
    CAR_GROUND        = 0x7b8,  /* [wheel] ground contact blocks, GROUND_SIZE each */
    CAR_8BC           = 0x8bc,  /* stored angles blended into the view */
    CAR_REF_A         = 0x888,  /* reference point ahead of the centre */
    CAR_REF_B         = 0x894,  /* the same in the other frame */
    CAR_STEER_SHARE   = 0x8a0,  /* steering as 0..1, 0.5 straight */
    CAR_REF_A_PREV    = 0x8a4,
    CAR_REF_B_PREV    = 0x8b0,
    CAR_YAW_TURNS     = 0x8c4,  /* whole turns taken out of the yaw */
    CAR_WHEEL_TURNS   = 0x910,  /* [wheel] whole turns taken out of the rotation */
    CAR_STEER_SHARE_PREV = 0x990,
    GROUND_SIZE       = 0x34,
    /* Ground block: the point the wheel is over and what lies below it. */
    GROUND_X          = 0x00,   /* query point (course frame, y up) */
    GROUND_Y          = 0x04,
    GROUND_Z          = 0x08,
    GROUND_ID         = 0x0c,   /* u32: triangle | leaf << 16 */
    GROUND_HIT_Y      = 0x10,   /* height of the ground below */
    GROUND_U          = 0x14,   /* per-vertex values at the hit, 0..1 */
    GROUND_V          = 0x18,
    GROUND_NORMAL     = 0x1c,   /* unit normal (course frame) */
    GROUND_PLANE      = 0x28,   /* -dot(normal, vertex) */
    GROUND_MISS       = 0x30,   /* u8: 1 = nothing below */
    GROUND_SURFACE    = 0x31,   /* u8: surface kind of the triangle */
    GROUND_KIND       = 0x33,   /* u8: second triangle byte */
};

enum {
    WHEEL_SIZE        = 0xec,
    WHEEL_STEER       = 0x00,   /* angle the wheel points at (rad) */
    WHEEL_CAMBER      = 0x04,
    WHEEL_CONTACT_Y   = 0x08,   /* lateral position of the contact, m (left negative) */
    WHEEL_MOUNT       = 0x0c,   /* mount point in the body, 3 floats */
    WHEEL_STEER_ADD   = 0x1c,   /* added (world mode 1) or used instead (mode 2) */
    WHEEL_SPIN        = 0x20,   /* circumferential speed omega * r, m/s */
    WHEEL_ROTATION    = 0x24,   /* wheel angle, rad (within +-2 pi) */
    WHEEL_VEL_X       = 0x28,   /* velocity at the wheel, two components (m/s) */
    WHEEL_VEL_Y       = 0x2c,
    WHEEL_VEL_ANGLE   = 0x30,   /* atan2(-y, x) */
    WHEEL_SLIP        = 0x34,   /* slip angle */
    WHEEL_38          = 0x38,   /* input of the tyre coefficient tables */
    WHEEL_3C          = 0x3c,
    WHEEL_44          = 0x44,   /* s8 */
    WHEEL_45          = 0x45,   /* s8: < 0 flips the force */
    WHEEL_40          = 0x40,
    WHEEL_FLAGS       = 0x46,   /* u8: 1 no load, 2 braking, 4/8 axle driven */
    WHEEL_LOAD        = 0x48,   /* vertical load, N */
    WHEEL_GRIP        = 0x4c,   /* force limit of the tyre, N */
    WHEEL_GRIP_FACTOR = 0x50,   /* from tyre wear/heat */
    WHEEL_COEF_A      = 0x54,   /* from the tyre tables at WHEEL_38 */
    WHEEL_COEF_B      = 0x58,
    WHEEL_SIDE_CURVE  = 0x5c,   /* side force curve value at the slip */
    WHEEL_VEL_WEIGHT  = 0x60,   /* 0 below 1 km/h .. 1 from 10 km/h */
    WHEEL_BRAKE_IN    = 0x64,   /* brake input */
    WHEEL_FORCE_LAT   = 0x68,   /* side force, N */
    WHEEL_FORCE_6C    = 0x6c,   /* second component of the friction circle, N */
    WHEEL_70          = 0x70,
    WHEEL_74          = 0x74,
    WHEEL_BRAKE       = 0x78,   /* brake torque */
    WHEEL_7C          = 0x7c,
    WHEEL_80          = 0x80,
    WHEEL_TORQUE      = 0x84,   /* net torque on the wheel */
    WHEEL_SURFACE     = 0x89,   /* u8: surface kind under the wheel */
    WHEEL_CONTACT_TIMER = 0xb3, /* u8: 6 on the ground, counts down in the air */
    WHEEL_B4          = 0xb4,   /* vibration terms */
    WHEEL_B8          = 0xb8,
    WHEEL_BC          = 0xbc,
    WHEEL_8C          = 0x8c,
    WHEEL_90          = 0x90,
    WHEEL_TRAVEL      = 0x94,   /* suspension travel, m (negative = compressed) */
    WHEEL_TRAVEL_V    = 0x98,   /* its speed, m/s */
    WHEEL_SPRING      = 0x9c,   /* spring and bump stop force, N */
    WHEEL_DAMPER      = 0xa0,   /* damper force, N */
    WHEEL_C8          = 0xc8,
    WHEEL_SIDE_FORCE  = 0xd4,   /* sideways force on the body, N */
    WHEEL_MOUNT_V     = 0xd8,   /* vertical speed of the mount */
    WHEEL_CONTACT     = 0xdc,   /* u8: 0 air, 1 ground, 2 touched down, 3 lifted off */
    WHEEL_DD          = 0xdd,   /* u8: copied from the tyre parameters */
};

enum {
    PARAMS_AXLE_DIST  = 0x010,  /* [position]: distance of the axle from the centre of mass */
    PARAMS_HALF_TRACK = 0x028,  /* [position]: half track */
    PARAMS_ROLL_HEIGHT = 0x040, /* [position] */
    PARAMS_50         = 0x050,  /* distance of the reference point */
    PARAMS_STEER_MAX  = 0x054,
    PARAMS_ROLL_TRANSFER = 0x034,
    PARAMS_PITCH_TRANSFER = 0x038,
    PARAMS_LAYOUT     = 0x030,  /* u8: drive layout index */
    PARAMS_MASS       = 0x048,
    PARAMS_MASS_B     = 0x04c,
    PARAMS_YAW_INERTIA = 0x118, /* 1 / yaw inertia */
    PARAMS_PITCH_ARM  = 0x11c,  /* front, rear: pitch acceleration per N */
    PARAMS_ROLL_ARM   = 0x124,  /* front, rear: roll acceleration per N */
    PARAMS_12C        = 0x12c,  /* [position] */
    PARAMS_ENGINE     = 0x134,  /* engine block, ENGINE_* (gt4_engine) */
    PARAMS_DRIVE      = 0x234,  /* drivetrain block, DRIVE_* */
    PARAMS_LOAD_REF   = 0x2d4,  /* per axle: reference of the load curve */
    PARAMS_INV_RADIUS = 0x2cc,  /* per axle: 1 / wheel radius */
    PARAMS_2C4        = 0x2c4,  /* per axle */
    PARAMS_2DC        = 0x2dc,  /* per axle */
    PARAMS_WHEEL_INERTIA = 0x300, /* per axle: mass equivalent of the free wheels */
    PARAMS_AXLES      = 0x308,  /* suspension blocks per axle, AXLE_* */
    PARAMS_CAMBER_CURVE = 0x3b8,/* per axle (12 bytes): grip against camber, gt4_curve */
    PARAMS_TYRES      = 0x410,  /* tyre parameter blocks, TYRE_SIZE each */
    PARAMS_TYRE_INDEX = 0xf98,  /* [axle * 24 + surface]: tyre block index */
    PARAMS_SLIP_SCALE = 0xfcc,
    PARAMS_AXLE_GRIP  = 0xfd0,  /* per axle: grip multiplier */
    PARAMS_BRAKE      = 0x1010, /* per axle: brake torque per unit of input */
    PARAMS_CAMBER     = 0xfe4,  /* per axle: static camber */
    PARAMS_CAMBER_GAIN = 0xfec, /* per axle: camber per metre of travel */
    PARAMS_ARM        = 0xffc,  /* [position]: suspension arm length (0 = straight travel) */
    PARAMS_CASTER     = 0x1004,
    PARAMS_CG_HEIGHT  = 0x1060,
    PARAMS_HANDBRAKE  = 0x1018,
    PARAMS_FUEL_RATE  = 0x104c,
    PARAMS_DRAG       = 0x1068, /* N per (m/s)^2 */
    PARAMS_DOWNFORCE  = 0x106c, /* front, rear: N per (m/s)^2 */
    PARAMS_AERO_KIND  = 0x1077, /* u8 */
    PARAMS_AERO_EXTRA = 0x1084,
    PARAMS_FUEL_BURST = 0x10d4,
};

enum {
    ENGINE_REDLINE    = 0xcc,   /* u16 rpm: rev limiter */
    ENGINE_IDLE       = 0xce,   /* u16 rpm */
    ENGINE_CAP        = 0xdb,   /* u8: power cap on */
    ENGINE_CAP_POWER  = 0xdc,
    ENGINE_CAP_SOFT   = 0xe0,
    ENGINE_TURBO      = 0xfc,   /* nonzero: turbo model */
};

enum {
    DRIVE_KIND        = 0x00,   /* u8: which wheels drive (0/4-7 rear, 1/2/9 front, 3/10 all, 8 split) */
    DRIVE_SPLIT       = 0x04,   /* front share for kind 8 */
    DRIVE_ENGINE_ROT  = 0x0c,   /* engine rotating inertia, reflected through the ratio */
    DRIVE_DIFF_TYPE   = 0x10,   /* [diff] u8: 0 open, 1 locked, 3/4 torque sensing, 5 viscous, 2/6/7 */
    DRIVE_GEARBOX     = 0x12,   /* u8: gearbox kind (0 manual, 1, 2, 3, 4) */
    DRIVE_CLUTCH_RPM  = 0x2c,   /* u16 */
    DRIVE_RATIOS      = 0x40,   /* [gear]: overall ratio (Ka: -13.86, 13.52, 7.93, 5.22, 3.76, 2.97) */
    DRIVE_RADIUS      = 0x90,   /* [axle]: wheel radius (Ka 0.256 m) */
    DRIVE_RADIUS2     = 0x98,   /* [axle] */
    DRIVE_CLUTCH_B0   = 0xb0,   /* clutch torque in state 3 */
    DRIVE_DIFF_INERTIA = 0xb4,  /* [diff]: 1 / rotating inertia */
    DRIVE_INERTIA_F   = 0xbc,   /* drive line inertia, front */
    DRIVE_INERTIA_R   = 0xc0,
    DRIVE_ENGINE_INERTIA = 0xc8,/* 1 / engine inertia */
};

enum {
    AXLE_SIZE         = 0x58,
    AXLE_BUMP         = 0x00,   /* travel where the bump stop starts */
    AXLE_MIN          = 0x04,   /* travel limits */
    AXLE_MAX          = 0x08,
    AXLE_0C           = 0x0c,
    AXLE_10           = 0x10,
    AXLE_SPRING       = 0x14,   /* N/m (Ka front 19404) */
    AXLE_BUMP_V0      = 0x18,   /* damper, compression side */
    AXLE_BUMP_C       = 0x1c,
    AXLE_BUMP_K       = 0x20,
    AXLE_REB_V0       = 0x24,   /* damper, extension side */
    AXLE_REB_C        = 0x28,
    AXLE_REB_K        = 0x2c,
    AXLE_ARB          = 0x30,   /* anti-roll bar */
    AXLE_BUMP_RATE    = 0x34,   /* bump stop, N/m^2 */
    AXLE_MASS         = 0x38,   /* unsprung mass, kg (Ka 39.25) */
    AXLE_UNSPRUNG     = 0x3c,   /* its weight, N (39.25 * 9.8) */
    AXLE_BUMP_DAMP    = 0x40,
    AXLE_MAX_ALT      = 0x44,   /* travel limit when CAR_5BE is set */
};

enum {
    TYRE_SIZE         = 0x1ec,
    TYRE_MU_SCALE     = 0x000,
    TYRE_MU           = 0x008,  /* [surface]: friction coefficient */
    TYRE_SIDE_CURVE   = 0x06c,  /* gt4_curve: side force against slip */
    TYRE_6C_CURVE     = 0x0bc,  /* gt4_curve: the friction circle's other component */
    TYRE_COEF_A       = 0x0f8,  /* gt4_curve */
    TYRE_COEF_B       = 0x170,  /* gt4_curve (read at TYRE_COEF_A's points) */
    TYRE_LOAD_CURVE   = 0x1ac,  /* gt4_curve: grip per load against relative load */
    TYRE_BY_VELOCITY  = 0x1d9,  /* u8: slip from the lateral velocity, not the angle */
    TYRE_1DA          = 0x1da,  /* u8 */
    TYRE_CIRCLE       = 0x1da,  /* u8: 1 = clamp the side force alone */
};

enum {
    LAYOUT_KIND       = 0x00,   /* 1: no camber term */
    LAYOUT_WHEELS     = 0x01,
    LAYOUT_DIFFS      = 0x02,
    LAYOUT_FRONT_WHEELS = 0x04,
    LAYOUT_REAR_WHEELS = 0x05,
    LAYOUT_DIFF_WHEELS = 0x08,  /* [diff]: wheels on it */
    LAYOUT_DIFF_WHEEL_A = 0x0c, /* [diff]: first wheel */
    LAYOUT_DIFF_INDEX = 0x10,   /* [diff]: index into the drive block's per-diff tables */
    LAYOUT_AXLE       = 0x14,   /* [wheel]: 0 front, 1 rear */
    LAYOUT_SIDE       = 0x1c,   /* [wheel]: 0 left, 1 right */
    LAYOUT_DIFF_WHEEL_B = 0x24, /* [first wheel]: second wheel (0x7f = none) */
    LAYOUT_POSITION   = 0x2c,   /* [wheel]: index of its axle distance and half track */
    LAYOUT_HANDBRAKE  = 0x3c,   /* [wheel] s8: > 0 front, < 0 rear (handbrake) */
    LAYOUT_44         = 0x44,   /* [wheel] s8: 2 = camber thrust */
};

/* Drive layout table (D_00620478): 84 bytes per layout; byte 1 = wheel count. */
#define GT4_LAYOUTS 0x00620478u
static inline uint32_t gt4_car_layout(uint32_t car)
{
    return GT4_LAYOUTS + GT4_U8(GT4_U32(car + CAR_PARAMS) + PARAMS_LAYOUT) * 84u;
}
static inline int gt4_car_wheels(uint32_t car) { return GT4_U8(gt4_car_layout(car) + LAYOUT_WHEELS); }
static inline uint32_t gt4_car_wheel(uint32_t car, int i) { return car + CAR_WHEELS + (uint32_t)i * WHEEL_SIZE; }

/* 00350530: the shared wall/car/prop contact notification. */
void gt4_car_impact(uint32_t car,int32_t kind,int32_t detail,float force);

/* Result of a course ray query (func_003E5A30), 0x30 bytes as the game has it. */
typedef struct {
    float    pos[3];
    uint32_t surface;           /* triangle byte 6 & 0x1f */
    uint32_t kind;              /* triangle byte 7 */
    float    u, v;              /* vertex bytes 0xe / 255 and (0xf & 0x7f) / 127, interpolated */
    float    normal[3];
    float    plane;
    int16_t  tri, leaf;
} gt4_ray_hit;

int   gt4_course_ray_down(uint32_t mesh, gt4_ray_hit *out, const float p[3]); /* func_003E5A30 */
void  gt4_car_ground_query(uint32_t car);         /* func_00344458 */
void  gt4_walls(uint32_t conductor, int first);   /* func_0034DAA8 */
void  gt4_walls_cars(uint32_t conductor);         /* func_0034DA70 */
int32_t gt4_camera_ground_id(uint32_t course, const float p[3]);   /* func_003FCB20 */
int   gt4_camera_wall(uint32_t course, float margin, const float from[3], const float to[3], float out[3], int32_t *id); /* func_003FE7B0 */
void  gt4_ai_drive(uint32_t car, uint32_t cmd);   /* func_003F3400 */
void  gt4_car_controls(uint32_t car, uint32_t cmd); /* func_00349F48 */
void  gt4_car_wheel_setup(uint32_t car);          /* func_00344420 */
void  gt4_car_ground_points(uint32_t car);        /* func_003697A8 */
void  gt4_car_ground_info(uint32_t car);          /* func_00369888 */
float gt4_line_point_distance(uint32_t lines, int i); /* func_0046BDC0 */
int   gt4_line_nearest(uint32_t lines, float x, float y, float d);   /* func_0046B108 */
int   gt4_line_course_point(uint32_t lines, int i, float d, float out[3]);   /* func_0034C518 (line part) */
void  gt4_line_start(uint32_t lines, int i, uint32_t pos, uint32_t dir, uint32_t out);   /* func_0046BEC0 */
int   gt4_line_rolling_start(uint32_t lines, uint32_t want, uint32_t ground, uint32_t px, uint32_t py,
                             uint32_t pz, uint32_t heading, int clamp, float d);   /* func_0046ADE8 */
float gt4_line_ground(uint32_t course, float x, float z, float h, uint32_t *id, int *miss);   /* func_0046A0C8 */
void  gt4_car_keep_state(uint32_t car);           /* func_00343ED0 */
void  gt4_car_messages(uint32_t car);             /* func_0035CA60 */
void  gt4_car_idle_count(uint32_t car);           /* func_0035D1D0 */
int32_t gt4_cars_gap(uint32_t cond, uint32_t i, uint32_t j, float *d);   /* func_00366018 */
void  gt4_cars_relations(uint32_t obj);           /* func_004146E8 */
void  gt4_car_brake_heat(uint32_t car);           /* func_003F38D8 */
void  gt4_car_wheel_noise(uint32_t car);          /* func_0035A678 */
void  gt4_car_race_end(uint32_t car);
void  gt4_race_gaps(uint32_t conductor);           /* func_00365E80 */
void  gt4_race_finish_handover(uint32_t conductor); /* func_00366588 */
/* The race object (race = the conductor's +0, race.c): its event queue at
   +0xF4 (read by the presentation), its messages, the start countdown,
   and its parts before and after each step */
void  gt4_race_queue_reset(uint32_t q);                                          /* func_003B7470 */
void  gt4_race_queue_push(uint32_t q, int32_t kind, uint32_t value, int32_t extra);   /* func_003B76F0 */
int   gt4_race_queue_take(uint32_t q, int32_t kind, uint32_t *value, int take);   /* func_003B7668 */
void  gt4_race_queue_age(uint32_t q);                                              /* func_003B75D8 */
int   gt4_race_queue_peek(uint32_t q, int32_t kind, uint32_t *value, int32_t n);  /* func_003B77E0 */
uint32_t gt4_race_message(int32_t kind, int32_t car, int32_t field, int32_t value);
void  gt4_race_post(uint32_t race, int32_t kind, int32_t car, int32_t field, int32_t value);   /* func_003A3148 */
void  gt4_race_records_reset(uint32_t r);                                        /* func_003C12B0 */
void  gt4_race_begin(uint32_t race);                                             /* func_003C0190 */
void  gt4_race_select_start(uint32_t race, uint32_t session_kind);                /* func_00387FB0 +0x43c..0x488 */
void  gt4_race_before_step(uint32_t race);                                       /* func_003C0208 */
void  gt4_race_after_step(uint32_t race);                                        /* func_003C0310 */
void  gt4_race_entry_reset(uint32_t entry, uint32_t mode);                       /* func_003B4E88 */
void  gt4_race_records_start(uint32_t race);                                     /* func_003C0108 */
void  gt4_race_entries_begin(uint32_t race, uint32_t cond);                      /* func_00387FB0's entries */
int   gt4_race_replay(uint32_t race);                                            /* func_003C0E78 */
void  gt4_race_event(uint32_t race, int32_t kind, uint32_t i, int32_t extra);    /* func_0035DF80 */
void  gt4_race_gap(uint32_t race, uint32_t i, uint32_t gap);                     /* func_0035E078 */
void  gt4_race_behind(uint32_t cond, uint32_t i, uint32_t laps);                 /* func_0035E8E8 */
void  gt4_race_pit_message(uint32_t race, uint32_t i, uint32_t v);               /* func_0035E000 */
void  gt4_race_lap_event(uint32_t cond, uint32_t i, uint32_t lap, uint32_t time, int quiet);   /* func_0035E550 */
void  gt4_race_split_event(uint32_t cond, uint32_t i, uint32_t lap, uint32_t split, uint32_t time, int quiet);   /* func_0035E428 */
void  gt4_race_countdown(uint32_t cond);                                         /* func_0034C0E0 */             /* func_00353218 */
void  gt4_car_tyre_wear(uint32_t car);            /* func_00352EA0 */
void  gt4_car_active_aero(uint32_t car);          /* func_003F28B8 */
void  gt4_car_aids(uint32_t car);                 /* func_003F79A0 */
void  gt4_car_steer_geometry(uint32_t car);       /* func_003F7EA8 */
void  gt4_car_slip(uint32_t car);                 /* func_00351778 */
void  gt4_ai_gear(uint32_t obj);
void  gt4_ai_start(uint32_t obj, uint32_t line);   /* func_004639E0 */
uint32_t gt4_shift_rpm(uint32_t car);             /* func_0036BEE0 */
int   gt4_gear_for_speed(uint32_t car, float speed); /* func_0036BF68 */                  /* func_00469E58 */
int   gt4_ai_player_aim(uint32_t obj);             /* func_00465078, player's case */
int   gt4_ai_model(uint32_t obj);                  /* func_00465078 */
void  gt4_ai_steer_to(uint32_t obj, float a);      /* func_00464BB0 */
float gt4_line_heading(uint32_t lines, int index, int from, float d);   /* func_004649F0 */
void  gt4_ai_pedals(uint32_t obj, float v, float k); /* func_00464770 */
void  gt4_car_driver(uint32_t car, uint32_t buf);
void  gt4_step(uint32_t conductor, int replay, uint32_t buf); /* func_0034DB30 */  /* func_00349FD0 (buf: 0x40 bytes of guest memory) */

int      gt4_wheel_axle(uint32_t layout, int i);
uint32_t gt4_tyre_params(uint32_t params, int i, uint8_t surface);

void  gt4_car_engine_update(uint32_t car);        /* func_00357700 */
void  gt4_car_wheel_directions(uint32_t car);     /* func_00357690 */
void  gt4_car_aero(uint32_t car);                 /* func_003F3090 */
void  gt4_car_tyre_grip(uint32_t car);            /* func_00357418 */
void  gt4_car_tyre_forces(uint32_t car);          /* func_00352408 */
void  gt4_car_longitudinal(uint32_t car);         /* func_00351FA8 */
void  gt4_car_wheel_torque(uint32_t car);         /* func_0036D230 */
void  gt4_car_diff_assist(uint32_t car);          /* func_0036D2F8 */
void  gt4_car_clutch(uint32_t car);               /* func_0036D818 */
float gt4_hybrid_idle(uint32_t car);             /* func_00355F80 */
void  gt4_hybrid_front(uint32_t car);            /* func_00356418 */
void  gt4_car_direction_select(uint32_t car);    /* func_00351EB0 */
void  gt4_cars_nearby(uint32_t conductor);       /* func_003F4638 */
int   gt4_ai_line_switch(uint32_t obj, int line, int now);   /* func_00463C28 */
void  gt4_ai_apply(uint32_t obj);                /* func_00464F38 */
void  gt4_ai_pit_throttle(uint32_t obj, int32_t v);   /* func_00463CD8 */
void  gt4_walls_corner_hit(uint32_t hull);       /* func_0045E570 */
int   gt4_tyre_condition(uint32_t car, uint32_t session);   /* func_00356EF0 */
void  gt4_car_driver_mode(uint32_t car, int mode);   /* func_00464C38 */
int   gt4_car_take_over(uint32_t car, int mode);     /* func_00345520 (6), func_003454A8 (5), func_00345598 (7) */
void  gt4_car_finish_drive(uint32_t car, int how);   /* func_00343D78 */
void  gt4_pit_orders(uint32_t car, uint32_t orders);   /* func_0035BFE8 */
int   gt4_car_race_over(uint32_t car);           /* func_00343C20 */
void  gt4_controller_event(uint32_t car, int kind, uint32_t v);   /* func_003F19E0 */
int   gt4_bspec_order(uint32_t cond, int idx, int kind, int value);   /* func_003F1BD8, kinds 4..10 */
int   gt4_pit_menu_ask(uint32_t car);            /* func_0035BC70 */
void  gt4_pit_menu_touch(uint32_t car);          /* func_0035BCC8 */
int   gt4_pit_menu_gone(uint32_t car);           /* func_0035BCE8 */
void  gt4_pit_menu_choose(uint32_t car, int front, int rear, int feed, int mode);   /* func_0035BF08 */
float gt4_input_curve(uint32_t table, float *slope, float x);   /* func_0035A9D0 */
void  gt4_split_clutch(uint32_t car);            /* func_00348368 */
void  gt4_split_rpm(uint32_t car);               /* func_003694D0 */
void  gt4_car_differentials(uint32_t car);        /* func_0036E4B8 */
void  gt4_car_tachometer(uint32_t car);           /* func_00350ED0 */
void  gt4_car_gearbox(uint32_t car);              /* func_003519B8 */
float gt4_engine_power_cap(uint32_t engine, float torque, float speed); /* func_0034EA68 */
float gt4_engine_boost(uint32_t engine, int rpm0, int rpm1);         /* func_0034ECF0 */
void  gt4_converter(uint32_t drive, float *speed, float *ratio);       /* func_003F1FB0 */
float gt4_slip_fold(float a, float *side);     /* func_00352218: into [0, pi/2], side +-1 */
/* SpecDB rows for the car record: the nth row (0 first) with this id in a
   table (index as in the garage keys); returns its width, 0 when there is
   none; with out NULL only says whether it exists. */
typedef struct {
    int  (*row)(void *ctx, uint32_t table, uint32_t id, uint32_t nth, uint8_t *out, size_t cap);
    void *ctx;
    /* string i of the string database (UnistrDB), NULL when there is none */
    const char *(*str)(void *ctx, uint32_t i);
    /* the parts a car takes (SpecDB CARS/<car key>: a 64-bit count, then
       16 bytes each, the part's key and level): malloc'd, NULL if none */
    uint8_t *(*car_parts)(void *ctx, uint64_t car, size_t *size);
} gt4_specdb_src;
uint32_t gt4_race_aspec_points(uint32_t race, int32_t i, const gt4_specdb_src *db); /* func_003C0FF8 */
void  gt4_car_record(uint32_t rec, uint32_t gar, uint32_t mode, const gt4_specdb_src *db); /* func_003F3B68 */
int32_t gt4_session_laps(uint32_t cond);                                  /* func_0034C3E0 */
float gt4_course_checkpoint(uint32_t course, uint32_t k);                 /* func_003C0098 */
void  gt4_car_signal(uint32_t car, uint32_t kind);                        /* func_0035ACD8 */
void  gt4_car_laps(uint32_t cond, uint32_t idx);                          /* func_00359008 */
int   gt4_car_records_times(uint32_t car);                                /* func_00359AF0 */
int   gt4_car_pit_leave(uint32_t car);                                    /* func_00345228 */
int   gt4_car_pit_box(uint32_t car);                                      /* func_00345400: onto the box line */
void  gt4_car_rest(uint32_t car);                                         /* func_00363560 */
float gt4_line_point_value(uint32_t lines, int i, uint32_t off);          /* func_0046BE20 / func_0046BE70 */
float gt4_course_distance_p(uint32_t course, const float p[3], uint32_t *id);    /* func_003955E8 */
float gt4_line_tight_corner(uint32_t lines, float d0, float tight);           /* func_0046AB80 */
void  gt4_pit_tyres(uint32_t cond, const gt4_specdb_src *db);          /* func_0035C4D8 */
int   gt4_tyre_byte(const gt4_specdb_src *db, uint32_t gar, uint32_t off, uint32_t at);   /* func_004462E0 / func_00446320 */
void  gt4_race_init(uint32_t cond, uint32_t mode, const gt4_specdb_src *db, uint32_t scratch);  /* func_0034D490 */
void  gt4_car_record_rows(const gt4_specdb_src *db, uint32_t gar, uint32_t rec);          /* func_004413C8 (no clear) */
/* func_00354FF8: gear ratios x1000 at out for a top speed (km/h); buf: 0x18 bytes of guest memory */
void  gt4_car_auto_ratios(uint32_t rec, int32_t top, uint32_t out, int clear, uint32_t buf);
/* func_00363B10 (init.c); buf: 0x60 bytes of guest memory */
uint32_t gt4_car_init(uint32_t car, uint32_t rec, uint32_t ground, uint32_t a3, uint32_t t0, uint32_t t1,
                      uint32_t idx, uint32_t kind, const float pos[4], int32_t start, int32_t speed,
                      uint32_t wear, uint32_t buf);
/* Course lines (lines.c) */
void  gt4_lines_setup(uint32_t lines, uint32_t course);   /* func_0046B0A8 */
int   gt4_lines_clear(uint32_t lines);                      /* func_0046B9F0 */
float gt4_course_distance(uint32_t course, uint32_t q);   /* func_003955E8: q = x, y, z, ground id */
/* func_003E5818: a course's RNW4 block copied to guest memory at rnw, offsets made addresses */
void  gt4_runway_relocate(uint32_t rnw);
/* func_00444480: a new car's garage entry (0x178 bytes); scratch: 0x680 bytes of guest memory */
int   gt4_garage_new(uint32_t gar, uint64_t car, const gt4_specdb_src *db, uint32_t scratch);
/* What the host gives the step: the SpecDB (the pit stop's tyre change)
   and 0x7F0 bytes of guest memory (that, the steering wheel's state for
   the force feedback and the wheels' effects). Without them those run the original. */
/* The car's controller as the host (the platform, in the game) answers
   it, in place of the controller object of the original (its table at
   *(car+0xC)+0xD0), car by car: a hook returns 1 when it answered for
   that car, 0 (or a NULL hook) leaves it to the car's controller object
   (the AI's, for the other cars). ctx is passed back.
   read (entry 0x28): the 32-byte driver command for this step (zeroed
     before; layout in driver.c); ev is the 32-byte block the step hands
     the controller: +3 its kind, +0x10 the pit crew's settings and +0x14
     a choice waiting (gearbox), for the in-race menus.
   wheel_state (0x20): a steering wheel's [0] response row and [1] end
     stops on (both 1 unless the wheel says otherwise).
   write (0x30): the 16 bytes of outputs after each step: [1] the large
     motor 0..127 ([0] the small one, never set in a race), [2] the
     wheel's force 0x80 centred (0 full one way, 0xFF the other), [0xD..F]
     the wheel's own settings. */
typedef struct {
    void *ctx;
    int (*read)(void *ctx, uint32_t car, uint8_t *cmd, const uint8_t *ev);
    int (*wheel_state)(void *ctx, uint32_t car, int16_t state[2]);
    int (*write)(void *ctx, uint32_t car, const uint8_t *out);
} gt4_phys_pads;
extern gt4_phys_pads gt4_phys_pad;
/* 1 when the car's controller object is the game's own class and has
   nothing to do for table entry off (no device, or held) */
int   gt4_pad_idle(uint32_t car, uint32_t off, uint32_t fn);
extern const gt4_specdb_src *gt4_phys_specdb;
extern uint32_t gt4_phys_scratch;
void  gt4_tyre_blocks(uint32_t params, uint32_t rec, uint32_t which);   /* func_00361570 */
void  gt4_tyre_static_load(uint32_t params, uint32_t which);          /* func_00361798 */
void  gt4_wheel_fresh(uint32_t car, int i);                           /* func_00362E58 */
void  gt4_car_vibration_wheels(uint32_t car);    /* func_0036CEE8 */
void  gt4_car_vibration_engine(uint32_t car);    /* func_00350080 */
void  gt4_car_force_feedback(uint32_t car);      /* func_00353EE8 */
void  gt4_car_wheel_effects(uint32_t car);       /* func_003668F0 */
/* the smoke's share of it (func_00395C70's block of 0x3C bytes at e), for
   the port's own smoke where the game's world is not there; NULL none */
extern void (*gt4_wheel_smoke)(void *user, uint32_t car, uint32_t e);
extern void *gt4_wheel_smoke_user;
/* the leaves' share (func_00395C98: a wheel on a triangle marked 0x20, with
   the car's velocity at vel) */
extern void (*gt4_wheel_leaves)(void *user, uint32_t car, uint32_t e, uint32_t vel);
float gt4_race_rubber_band(uint32_t self, uint32_t ba, uint32_t bb, float k);   /* func_00365790 */
void  gt4_walls_setup(uint32_t cond);           /* func_0045B8C0 */
void  gt4_walls_in(uint32_t cond);              /* func_003F4B20 */
void  gt4_tt_after_cars(uint32_t cond, const gt4_specdb_src *db);   /* func_003F5580: the Time Trial's method 0xD8 */
int   gt4_car_take_back(uint32_t car);          /* func_003452F0 */
void  gt4_ai_handicap(uint32_t obj, uint32_t n);   /* func_00467148 */
void  gt4_ai_settings_load(uint32_t dst, uint32_t src);   /* func_00468EE8 */
void  gt4_ai_profile_init(uint32_t obj, uint32_t n, uint32_t mode);   /* func_00466C20 */
void  gt4_race_player_styles(uint32_t cond);    /* func_003F1E60 */
void  gt4_input_curve_set(uint32_t t, uint32_t type, int32_t p);   /* func_0035A818 */
void  gt4_drive_settings(uint32_t cond);        /* func_00347A00 */
/* The session's methods 0xD0 (the race's start) and 0xD8 (after the
   cars) for the tables rewritten (Time Trial, Arcade race); 0 for others */
int   gt4_session_start(uint32_t cond, uint32_t a1);
int   gt4_session_after_cars(uint32_t cond, const gt4_specdb_src *db);
/* func_00361908 (buf: 0x20 bytes of guest memory) */
void  gt4_car_params(uint32_t params, uint32_t rec, int32_t mode, uint32_t kind, uint32_t race, uint32_t entry, uint32_t buf);
float gt4_engine_peak_power(uint32_t params, uint32_t wear);   /* func_0034F3F8's peak power (PS) */
void  gt4_surface_bumps(uint32_t cond);   /* func_0035F808 (the car's rating passes cond 0: the byte at 0xCBDD) */
void  gt4_car_gear_select(uint32_t car);          /* func_00351DC8 */
void  gt4_car_yaw(uint32_t car, int airborne);    /* func_00352848 */
void  gt4_car_velocity(uint32_t car, int airborne); /* func_00352C90 */
void  gt4_car_drivetrain(uint32_t car);           /* func_003535B0 */
void  gt4_car_suspension_reach(uint32_t car);     /* func_0036A490 */
void  gt4_car_suspension_update_reach(uint32_t car); /* func_0036A080 */
uint8_t gt4_car_suspension_short(uint32_t car, uint8_t flags); /* func_0036A1D8 */
void  gt4_car_ground_fix(uint32_t car);           /* func_0036BA88 */
void  gt4_car_mounts(uint32_t car);               /* func_00345A20 */
void  gt4_car_view_follow(uint32_t car);          /* func_00345658 */
void  gt4_car_view_sway(uint32_t car);            /* func_0034A668 */
void  gt4_car_view_bob(uint32_t car);             /* func_0034A788 */
void  gt4_car_vibration_reset(uint32_t car);      /* func_00366718 */
int   gt4_car_presence(uint32_t car);             /* func_00366678: 3 racing, 2 faded, 0/1 none */
void  gt4_car_wheel_rotation(uint32_t car);       /* func_00344040 */
void  gt4_car_surface_feel(uint32_t car);         /* func_00367148 */
void  gt4_car_ground_frame(uint32_t car);         /* func_0035D350 */
float gt4_wheel_angle(uint32_t params, int i, float steer);                    /* func_003F7CA8 */
float gt4_wheel_contact_y(uint32_t params, int i, float angle, float travel);  /* func_0036A690 */
float gt4_wheel_camber(uint32_t params, int i, float steer, float travel);     /* func_0036A578 */
uint8_t gt4_ground_surface(uint32_t g);                                        /* func_003F4E40 */
int   gt4_ground_off_road(uint32_t g);                                         /* func_003F4EA0 */
void  gt4_ground_query(uint32_t course, uint32_t g);                           /* func_00395760 */
void  gt4_car_place(uint32_t car, uint32_t id, float x, float y, float h, float heading);   /* func_003630C0 */
void  gt4_car_ground_speeds(uint32_t car);        /* func_0035DE20 */
void  gt4_car_suspension(uint32_t car);           /* func_0036A758 */
void  gt4_car_chassis(uint32_t car);              /* func_00344530 */
void  gt4_car_attitude(uint32_t car);             /* func_0036B0C8 */
void  gt4_rotation_from_angles(float *a, float *b, float *c, float x, float y, float z); /* func_0036C9F8 */
float gt4_car_engine_drive(uint32_t car, float throttle); /* func_003554A0 */
float gt4_wheel_engine_speed(uint32_t car);       /* func_00350E28 */
float gt4_surface_factor(uint32_t car, uint8_t surface);  /* func_00467470 */
float gt4_wheel_direction(float *weight, float x, float y);   /* func_003575E0 */

void gt4_tyre_wear_global(uint32_t target); /* func_00367AF8 */
void gt4_tyre_wear_rules(uint32_t target, uint32_t rules); /* func_00367918 */
int gt4_garage_tuned(uint32_t setup, uint64_t key, const gt4_specdb_src *db, uint32_t scratch); /* func_00444690 */
/* 00445808/00445828/00445AB8: eligible count and RGB indexed by original
   order, not physical row order. random excludes GENERIC_CAR +0x12 bit 1. */
int gt4_garage_paint_palette(uint32_t setup,const gt4_specdb_src *db,int random,uint32_t rgb[32]);
void gt4_garage_event_tyres(uint32_t setup,int type,const gt4_specdb_src *db); /* 003EE3C8 */
void gt4_garage_world_tyres(uint32_t setup,int type,const gt4_specdb_src *db); /* 00387348 */

#endif
