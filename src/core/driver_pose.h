#ifndef OPENGT4_DRIVER_POSE_H
#define OPENGT4_DRIVER_POSE_H
#include "driver_model.h"
/* Published skeleton order used by the original HumanModel. */
enum gt4_driver_bone_id {
    GT4_PELVIS,GT4_LTHIGH,GT4_LSHANK,GT4_LFOOT,GT4_RTHIGH,GT4_RSHANK,GT4_RFOOT,
    GT4_LOWER_SPINE,GT4_MIDDLE_SPINE,GT4_SCAPULA,GT4_LUPARM,GT4_LFOREARM,
    GT4_RUPARM,GT4_RFOREARM,GT4_CRANIUM,GT4_LCLAVICLE,GT4_RCLAVICLE,
    GT4_LHAND,GT4_RHAND,GT4_NECK
};
typedef struct {
    float position[3],rotation[4]; /* xyz and xyzw quaternion */
    float end[3],length,inverse_length;
    float branch[2][3]; /* pelvis hips / scapula clavicle origins */
} gt4_driver_segment;
typedef struct {gt4_driver_segment bone[GT4_DRIVER_BONES];} gt4_driver_skeleton;
/* 00419F80 -> 00419D80, constructed from the MDLS's sixteen dimensions.
   This is the authored neutral skeleton, not a seated race posture. */
int gt4_driver_skeleton_init(const float dimensions[16],gt4_driver_skeleton *out);
/* 00418A58 -> 004875B0, column-major model poses before inverse binding. */
int gt4_driver_skeleton_blend(gt4_driver_skeleton *out,const gt4_driver_skeleton *from,const gt4_driver_skeleton *to,float factor);
int gt4_driver_skeleton_matrices(const gt4_driver_skeleton *s,float out[GT4_DRIVER_BONES][16]);
/* Channels consumed by 003B1DB0's DriverPosture control update. The legacy
   field name pedal[2] denotes the lever gates/rotations (record +20/+24),
   distinct from foot compression. This layer does not read physics memory. */
typedef struct {
    float steering,hand_grasp,pedal[2],grasp_left,grasp_right;
} gt4_driver_control_input;
typedef struct {
    int steering_enabled,grasp_enabled;
    float steering,pedal[2],grasp[2];
} gt4_driver_controls;
/* control_type belongs to DriverPosture, not CARINFO's driver/model type.
   Original disabled-grasp updates preserve the two previous grasp values. */
int gt4_driver_controls_update(unsigned control_type,const gt4_driver_control_input *input,
                              gt4_driver_controls *state);
/* 00420510: shared arm/leg solver. Axes are authored unit reference axes;
   guide supplies the elbow/knee bend plane. Lengths are passed separately,
   as in the original caller, rather than inferred from model vertices. */
typedef struct {
    float root[3],target[3],guide[3],axis[2][3];
    float length[2],reach_margin,min_bend;
} gt4_driver_limb_target;
/* A valid solution may report unreachable (the original reach status).
   Malformed input leaves both segments and the status unchanged. */
int gt4_driver_solve_limb(const gt4_driver_limb_target *target,
                          gt4_driver_segment *first,gt4_driver_segment *second,int *reachable);
/* DriverPosture fields consumed by 0041E678 (pelvis/thighs/shanks/feet).
   The names describe their measured effect; caller builds the native targets. */
typedef struct {
    float spread[2],shank_twist[2],foot_twist[2],foot_roll[2];
} gt4_driver_leg_controls;
int gt4_driver_solve_legs(gt4_driver_skeleton *s,const float feet[2][3],
                          const gt4_driver_leg_controls *controls,int reachable[2]);
typedef struct {
    float root[3],target[3],guide[3],axis[2][3],length[3];
} gt4_driver_chain_target;
/* 00420AA8 / 00420190: three-segment spine solver. */
int gt4_driver_solve_chain(const gt4_driver_chain_target *target,gt4_driver_segment chain[3],int *reachable);
typedef struct {
    float root[3],rotation[4],end[3],guide[3];
} gt4_driver_torso_target;
typedef struct {
    float twist,roll,pelvis_twist,look_gain;
    int twist_locked;
} gt4_driver_torso_controls;
/* 0041CB20: pelvis/spine/scapula, optional world-space look target. */
int gt4_driver_solve_torso(gt4_driver_skeleton *s,const gt4_driver_torso_target *target,
                           const gt4_driver_torso_controls *controls,const float look[3]);
/* 0041C6F8: choose the elbow's bend guide in the clavicle's coordinate frame. */
int gt4_driver_elbow_guide(float out[3],const gt4_driver_segment *clavicle,const float side_axis[3],
                           const float root[3],const float hand[3],float elbow_control,float elevation);
typedef struct {
    float rotation[4],hand[2][3],normal[2][3],wrist_axis[2][3];
} gt4_driver_arm_target;
typedef struct {
    float elbow[2],wrist_aim[2],wrist_twist[2],hand_twist[2],clavicle_angle[2],clavicle_roll[2];
    int auto_clavicle,wrist_mode,normal_enabled[2],simple_hand_twist;
} gt4_driver_arm_controls;
/* 0041D140: clavicles, arms and hands, including wrist aiming and reach
   correction. Auto-clavicle updates the two angle controls. The original
   simple-hand branch preserves reach flags; the full branch publishes the
   last shared IK status in both slots. Malformed input is transactional. */
int gt4_driver_solve_arms(gt4_driver_skeleton *s,const gt4_driver_arm_target *target,
                          gt4_driver_arm_controls *controls,int reachable[2]);
typedef struct {
    float blend,base_pitch,roll_gain,roll_bias,yaw_range,up[3],angles[3];
    int neck_yaw_locked;
} gt4_driver_head_controls;
/* 0041EFE0: neck/head, cone limits and roll relative to the supplied up.
   Blend and Euler-angle state are mutable. Absent/zero-length look targets
   preserve the cone reach flag, as in the original. */
int gt4_driver_solve_head(gt4_driver_skeleton *s,gt4_driver_head_controls *controls,
                          const float look[3],int *reachable);
enum {GT4_DRIVER_TORSO=1,GT4_DRIVER_ARMS=2,GT4_DRIVER_LEGS=4,GT4_DRIVER_HEAD=8};
typedef struct {
    gt4_driver_torso_target torso;
    gt4_driver_arm_target arms;
    float feet[2][3];
} gt4_driver_pose_targets;
typedef struct {
    gt4_driver_torso_controls torso;
    gt4_driver_arm_controls arms;
    gt4_driver_leg_controls legs;
    gt4_driver_head_controls head;
    unsigned enabled;
} gt4_driver_posture;
/* Defaults from 0041C1C8 / 00418F88, not a race seating approximation. */
void gt4_driver_posture_init(gt4_driver_posture *posture,gt4_driver_pose_targets *targets);
/* 0041F560's dispatch order. The complete solve scopes the original rounding
   toward zero and restores the caller's mode on success or failure. Reach
   slots: head, left/right leg, left/right arm. */
int gt4_driver_posture_solve(gt4_driver_skeleton *s,gt4_driver_posture *posture,
                             const gt4_driver_pose_targets *targets,const float look[3],int reachable[5]);
/* 00414C90's vehicle descriptor. Geometry remains in its authored local
   frame until the target builder applies position/rotation. gear[0..2]
   are the three authored hand positions selected by gear_phase. */
typedef struct {
    float position[3],rotation[4],feet[2][3],seat[3],wheel[3],gear[3][3],rest[2][3];
    float normal[3][3],wheel_radius,wheel_basis[9];
    int shift_side,hand_mode;
    float shift_length,shift_angle,gear_phase;
} gt4_driver_geometry;
void gt4_driver_geometry_init(gt4_driver_geometry *geometry);
/* 00415240: builds the wheel plane from its authored normal. */
int gt4_driver_geometry_wheel(gt4_driver_geometry *geometry,const float normal[3]);
enum {GT4_GRIP_WHEEL,GT4_GRIP_REST,GT4_GRIP_SIMPLE,GT4_GRIP_FREE};
typedef struct {
    float position[3],normal[3],axis[3],twist,aim,extra_twist;
    float shift_offset[3],phase_twist,phase_unused,radius_offset,depth_offset,shift_twist;
    int normal_mode;
} gt4_driver_grip;
/* Original virtual hand targets 00415D38 / 00415510 / 004154A0 / 00415648.
   side is the published left/right hand (0/1). Angles and phase are original
   DriverPosture channels; no time curve is synthesized by this interface.
   Evaluators preserve fields that the original branch does not write. */
int gt4_driver_grip_solve(gt4_driver_grip *grip,const gt4_driver_geometry *geometry,
                         unsigned kind,unsigned side,const float angles[2],float blend,float phase,int *normal_enabled);
/* Extra fields of 00416B58's DrivingPosture consumed by 004173B0.
   Controls carry phase values recorded/generated by the original race
   controller; target construction itself has no animation clock. */
typedef struct {
    float twist_channel,phase_keys[3],angles[2][2],blend[2],radius_offset[2],depth_offset[2],hand_twist[2];
    float phase_twist[3][2],pitch,feet_offset[2][3],clavicle_angle[2],clavicle_roll[2];
    float spine_offset[3],spine_world_offset[3],length_offset,shift_offset[3][3],phase_amplitude[3][2],root_offset[3],phase;
    int direct_twist,free_hands,pitch_locked,normal_override[2],normal_mode;
} gt4_driver_target_controls;
void gt4_driver_targets_init(gt4_driver_target_controls *controls);
/* 004173B0: build seated targets and publish mutable wrist/clavicle
   controls. Native transaction with scoped original FPU rounding. */
int gt4_driver_targets_build(const gt4_driver_skeleton *s,gt4_driver_posture *posture,
                            const gt4_driver_geometry *geometry,const gt4_driver_target_controls *controls,
                            gt4_driver_pose_targets *targets);
/* Ordinary CARINFO provider and 003B2708's race-pose adapter. Channels are
   the 64 floats of the original recorded human frame (00386818), not raw
   vehicle steering or a synthesized animation clock. */
unsigned gt4_driver_control_type(const gt4_driver_car *car);
int gt4_driver_record_init(const gt4_driver_car *car,float channels[64]);
int gt4_driver_race_pose(const gt4_driver_car *car,const float channels[64],
                         gt4_driver_skeleton *s,float matrices[GT4_DRIVER_BONES][16]);
/* Presentation-owned controller state. 0040FC78 regrips the wheel;
   00411628 moves the lever through its authored gates. No guest pointers. */
typedef struct {
    float first,second,blend,working,radius,depth,travel;
    unsigned state;
} gt4_driver_hand_motion;
int gt4_driver_hand_step(gt4_driver_hand_motion *hand,unsigned side,int regrip,
                         const gt4_driver_hand_motion *other,unsigned *other_state,
                         float delta,float rate,float dt,float wheel_rate);
/* 0040F850 / 0040F888: Patent/Carriage steering, distinct from modern regrips. */
int gt4_driver_classic_hand_step(gt4_driver_hand_motion *hand,unsigned side,unsigned type,
                                  unsigned *other_state,float steering,float delta,float dt,float wheel_rate);
typedef struct {float gate[2],rate[2],wait,roll[2];} gt4_driver_lever_motion;
int gt4_driver_lever_step(gt4_driver_lever_motion *lever,const int transmission[4],int gear,float dt);
typedef struct {
    float channels[64],steering_rate[4],phase_rate,lever_roll[2],lever_roll_rate[2],hold,turn_hold,classic_right;
    gt4_driver_hand_motion hand[2];gt4_driver_lever_motion lever;
    int transmission[4],gear;unsigned shift_state;int initialized;
} gt4_driver_animation;
typedef struct {
    float dt,steering,steering_fraction,throttle,brake,lever_pedal;
    int gear,upshift,downshift,speed_shift,clutch_blocked;
} gt4_driver_animation_input;
int gt4_driver_animation_init(gt4_driver_animation *animation,const gt4_driver_car *car,
                              const int transmission[4],int gear);
/* 004104B8: recorded finger closure, original hand/gear states and phase.
   phase_other is recorded +C0, selected for gear states >=14. */
int gt4_driver_grasp_step(unsigned hand_state,unsigned gear_state,float phase,float phase_other,float dt,float *grasp);
/* Ordinary and classic wheel controllers, fixed simulation step supplied by
   caller. Carriage's separate gear phases and Nike/Motor Triathlon providers
   remain incomplete. Input lever_pedal is the original car+598 channel. */
int gt4_driver_animation_step(gt4_driver_animation *animation,const gt4_driver_car *car,
                              const gt4_driver_animation_input *input);
#endif
