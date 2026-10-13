#ifndef OPENGT4_RACE_CAR_SOUND_H
#define OPENGT4_RACE_CAR_SOUND_H
/* One car's race sound, rewritten from func_003B5E80 (what each entry
   sounds like from the camera, a frame on the stack) and func_00392640
   (that frame into the sound driver's requests). The car state is the
   game's own (guest memory, as the native race keeps it); the camera is
   handed in, as the native view has no camera objects. */
#include <stdint.h>

/* What 003B5E80 asks the camera manager (world +0x80). */
typedef struct {
    float view[16];         /* the VU0 matrix after vt+0x18: out = v[0..3]x + v[4..7]y + v[8..11]z + v[12..15] */
    float eye_previous[3];  /* vt+0x20/+0x28/+0x30, f12 0: current eye, camera +0x10 */
    float eye[3];           /* vt+0xE0/+0xE8/+0xF0, a1 1: preceding eye, camera +0x7C */
    int continuous;         /* vt+0x100: the camera's +0x16B; 0 restarts the sounds */
    int type;               /* vt+0x108: the camera's vt+0xA8, -1 without one */
    float angle;            /* vt+0x128: the camera's vt+0xC0 in degrees, x 0x3C8EFA35 */
} gt4_car_listener;

/* The sound object an entry holds at +0x2900 (003924D0, 00391AF0): only
   the parts the frame and the requests read back. */
typedef struct {
    int started;            /* +0xB38 */
    uint8_t invert[7][2];   /* 00391AF0 bytes 0/1: +0x14, +0x3F4, +0x2B8, wheels +0x474 + w*0x1A0 */
    float direction[7][3];  /* the same objects' +4..+0xC */
    float center[3];        /* +0x468..+0x470 */
    float wheel[4][11];     /* +0x5E8 + w*0x1A0 + k*4 */
} gt4_car_sound_state;

/* The frame 003B5E80 fills (its stack, the offsets kept): 00392640's a1. */
typedef struct { uint8_t b[0x2D0]; } gt4_car_sound_frame;

/* One 00462A88 or ENGN controller request. */
typedef struct {
    float left, right, pitch;   /* f12, f13, f14 */
    int32_t priority;           /* a1, the volume as 0..7FFFFFFEh */
} gt4_car_sound_tone;
typedef struct {
    int16_t left, right;        /* controller +0x4C, +0x4E */
    int32_t scale;              /* +0x40, 4096 = 1 */
    int32_t priority;           /* +0x3C */
    int32_t reverb_off;         /* +0x50 */
    int16_t rpm;                /* +0x48 */
} gt4_car_sound_engine;
enum {
    GT4_CAR_TURBINE, GT4_CAR_TURBINE2,          /* handles +0x278, +0x298 */
    GT4_CAR_IN_CAR, GT4_CAR_BODY, GT4_CAR_NOS,  /* +0x408, +0x428, +0x448 */
    GT4_CAR_WHEEL0,                             /* +0x488 + w*0x1A0 + k*0x20 */
    GT4_CAR_TONES = GT4_CAR_WHEEL0 + 44
};
/* Wheel slot k's effect, 003921A8: on_skill, on_road, rain_noiz,
   off_roadnoiz, off_skill, pebble, sand, rumble, rumble2, rumble3, rumble3. */
extern const uint8_t gt4_car_wheel_effect[11];
extern const uint8_t gt4_car_tone_effect[5];
typedef struct {
    gt4_car_sound_engine engine[3];     /* +0x28 engine, +0x2C road noise, +0x2CC exhaust */
    gt4_car_sound_tone tone[GT4_CAR_TONES];
    uint8_t tone_on[GT4_CAR_TONES];     /* wheel slots off in D_0088F2C0 are not called */
    struct { uint8_t effect; float left, right; int32_t priority; } shot[11];
    int shots;                          /* 00392EB8 one-shots, in table order */
    int restart;                        /* 00392FC8 + 003924D0 ran first */
} gt4_car_sound_out;

/* Per race (00388758 -> 003922B0; 00391A60): the wheel slots the course
   enables (the session's +0xCBF0) and the three channels' falloff/near
   distance for the output mode (D_006A0250 + mode*24). */
typedef struct {
    uint8_t wheel_on[11];
    float falloff[3], near[3];
    float master;                       /* 00391CD8 * D_006213E0 */
} gt4_car_sound_setup;
void gt4_car_sound_setup_race(gt4_car_sound_setup *setup, uint32_t cond, unsigned output_mode, float master);

void gt4_car_sound_state_reset(gt4_car_sound_state *state);
/* func_003B5E80 for one entry. `target`: vt+0x168 (the camera's car);
   `replay`: its a2 (world +0xCC8 != 0); `random`: 00579358's value when
   the frame needs one (backfire). */
void gt4_car_sound_frame_build(gt4_car_sound_frame *frame, gt4_car_sound_state *state, gt4_car_sound_out *out,
                               uint32_t entry, const gt4_car_listener *camera, int target, int replay,
                               uint32_t random);
/* func_00392640. */
void gt4_car_sound_requests(const gt4_car_sound_frame *frame, gt4_car_sound_state *state,
                            const gt4_car_sound_setup *setup, gt4_car_sound_out *out);
/* The course's ambient sound (func_00398B70 on world +0xE170): the CRSS
   block (course header +0x8C) of five speaker lists -- air, city,
   crowds, cheer, sea (00398A70) -- plus the tunnel and helicopter tones
   and the reverb depth. */
typedef struct {
    int started;            /* +0 */
    float depth;            /* +0x16C: the reverb depth, 0.125 outside */
    float level[512];       /* each speaker's +0xC, by its order in the lists */
} gt4_ambient_state;
typedef struct {
    float left[5], right[5], pitch[5];
    int32_t priority[5];
    float tunnel[4][2];     /* tunnel1_l, tunnel1_r, tunnel2_l, tunnel2_r: f12, f13 */
    int32_t tunnel_priority[2];
    float heri;
    int32_t heri_priority;
    int16_t depth;          /* 0055CD18's EVOL, both cores */
    int restart;            /* 00399300 + 00398B58 ran */
} gt4_ambient_out;
/* func_00457F60: the five groups' left, right, pitch (out[5][3]) heard at
   `eye`, with the camera's right axis and its motion (previous minus
   current). `race` gives the cars' progress for the gated speakers. */
void gt4_ambient_groups(gt4_ambient_state *state, uint32_t crss, uint32_t race, const float eye[3],
                        const float right[3], const float velocity[3], float out[5][3]);
void gt4_ambient_state_reset(gt4_ambient_state *state);
/* func_00398B70 for one viewport. `heri`: the camera manager's vt+0x130
   bit 0; `master`: 00391CD8 x D_006213E0. */
void gt4_ambient_frame(gt4_ambient_state *state, uint32_t race, const gt4_car_listener *camera, const float right[3],
                       int heri, float master, gt4_ambient_out *out);
/* Presentation-only exhaust pulse state (003D6C18/003D6D30).
   Reads the existing physics backfire flag; never writes the car. */
typedef struct { uint8_t b[0x6c]; } gt4_car_backfire;
void gt4_car_backfire_init(gt4_car_backfire *s);
void gt4_car_backfire_step(gt4_car_backfire *s,uint32_t car,float dt);
uint32_t gt4_car_backfire_take(gt4_car_backfire *s);
#endif
