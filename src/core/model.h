#ifndef OPENGT4_MODEL_H
#define OPENGT4_MODEL_H
#include "tex1.h"
typedef struct { float p[3],uv[2],rgb[3],normal[3],ambient[3],specular[3],shine,alpha; } gt4_vertex;
typedef struct {
    uint32_t color_mask;
    uint8_t blend,fixed_alpha,depth_write,depth_test;
    uint8_t alpha_test,alpha_func,alpha_ref,destination_test,destination_bit;
    uint8_t cull,depth_func,alpha_fail;
    /* Commands 33-35 of 00455708 (004A4050): the fog colour this draw
       fogs towards instead of the scene's (additive shapes fog to black). */
    uint8_t fog_override,fog_rgb[3];
} gt4_draw_state;
typedef struct { gt4_vertex v[3]; int texture; unsigned kind; uint8_t blend_enabled; gt4_draw_state state;
                 uint16_t source_model; /* MDLS model index, for Vls0 selection */
                 int wheel_blur_texture; /* 0 none; authored alternate texture index + 1 */
                 uint8_t attachment; /* 0 body, 1..4 road wheels, 5+ steering frames */
                 uint8_t lamp; /* car callback 0 (the brake): 0 always, 1 its released branch, 2 its pressed one */ } gt4_triangle;
#define GT4_MODEL_STEERING_MAX 8
/* Optional driver skin stream. Static cars/courses allocate none of this.
   Indices name MDLS bind-matrix slots, not the skeleton's published order. */
typedef struct { uint8_t bone[4]; float weight[4]; } gt4_skin_vertex;
typedef struct { gt4_skin_vertex v[3]; } gt4_skin_triangle;
/* Authored second endpoint of a rigid human tween (position, normal, UV).
   Hands and the distant pit crew use this resident stream. */
typedef struct {float p[3],normal[3],uv[2];} gt4_morph_vertex;
typedef struct {gt4_morph_vertex v[3];} gt4_morph_triangle;
typedef struct {
    gt4_triangle *triangles; int count,capacity;
    unsigned source_models;
    tex1_image *textures; int ntextures;
    int unsupported;
    gt4_skin_triangle *skin; int skin_capacity;
    gt4_morph_triangle *morph;int morph_capacity;float grasp_scale,phase_scale;
    /* CAR4 callback 8: authored matrix immediately before its local Z turn.
       Decoded triangles retain their neutral coordinates for static callers. */
    float steering_frame[GT4_MODEL_STEERING_MAX][16];unsigned steering_count;
} gt4_model;
/* Optional diagnostics of one conversion (surveys, tests). */
typedef struct {
    unsigned ops[256];        /* command opcodes executed */
    unsigned callbacks[256];  /* callback ids met by commands 6/77 */
    int      fail_op;         /* unknown opcode that stopped conversion, -1 none, -2 budget */
    size_t   fail_offset;
    int      fail_line;       /* converter source line of a bounds/format failure */
    float    samples[8][17];  /* first transform commands met: opcode then arguments */
    int      nsamples;
} gt4_model_stats;
/* Static menu geometry, converted once; not a VU/GS execution backend. */
gt4_model *gt4_model_decode_car_wing(const uint8_t *bytes,size_t size,int color,int installed);
gt4_model *gt4_model_decode(const uint8_t *bytes,size_t size,int color);
gt4_model *gt4_model_decode_part(const uint8_t *bytes,size_t size,int color,int model_index);
gt4_model *gt4_model_decode_stats(const uint8_t *bytes,size_t size,int color,int model_index,gt4_model_stats *stats);
/* CAR4 rim MDLS: WheelContext returns branch 0 and binds its Tex1 bank.
   Ordinary MDLS callers have no such context and retain their default path. */
/* Tire position bounds over all authored shapes: min/max radius, max |x|. */
int gt4_model_tire_bounds(const uint8_t *bytes,size_t size,float bounds[3]);
gt4_model *gt4_model_decode_wheel_part(const uint8_t *bytes,size_t size,int model_index);
/* Human MDLS: also read the authored skin block following each VIF packet.
   Geometry is returned in the authored bind pose; no guessed animation. */
gt4_model *gt4_model_decode_skin_part(const uint8_t *bytes,size_t size,int model_index,gt4_model_stats *stats);
/* Recognizes the asset's left/rightGrasp * scale VM expression, preserving
   both authored endpoints. Unknown programs fail instead of inventing a pose. */
gt4_model *gt4_model_decode_hand_part(const uint8_t *bytes,size_t size,int model_index,gt4_model_stats *stats);
/* BC_WALK/BC_SQUAT: validated authored time VM; cache every branch once. */
gt4_model *gt4_model_decode_pit_walk_part(const uint8_t *bytes,size_t size,int model_index,int branch,gt4_model_stats *stats);
/* One branch of the model's callback (the after fire's frames). */
gt4_model *gt4_model_decode_branch(const uint8_t *bytes,size_t size,int model_index,int branch);
/* A course world model set (all models): integer vertices are 1/512 fixed point. */
gt4_model *gt4_model_decode_world(const uint8_t *bytes,size_t size,gt4_model_stats *stats);
/* Merge static instances and transfer ownership of part textures.
   Rigid hand morphs are drawn separately; this interface rejects them. */
int gt4_model_attach(gt4_model *m,gt4_model *part,const float transforms[][6],int count);
/* Cached steering geometry in callback-local coordinates. Texture indices
   refer to the parent; the returned part owns only its triangles. */
gt4_model *gt4_model_steering_part(const gt4_model *m,unsigned frame);
/* 00453758 -> 004A79D8: authored frame * local RotateZ, input in radians. */
int gt4_model_steering_matrix(const gt4_model *m,unsigned frame,float angle,float matrix[16]);
void gt4_model_free(gt4_model *m);
#endif
