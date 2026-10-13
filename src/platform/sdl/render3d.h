/*
 * GPU renderer for converted GT4 models (OpenGL 3.3). It reproduces the
 * rules of the software rasterizer in showroom.c (the reference): PS2 blend
 * equation (A-B)*C+D, alpha coefficients (128 = 1), alpha test with its fail
 * action, destination alpha test, texture addressing modes, CarFace
 * lighting and the spherical environment reflection. See docs/RENDERER.md.
 */
#ifndef OPENGT4_RENDER3D_H
#define OPENGT4_RENDER3D_H

#include "model.h"
#include "tex1.h"

typedef struct r3d_mesh r3d_mesh;

/* An object placement and camera: world = rot * p + pos; the camera at cam
   with right/up/forward axes; focal in pixels of the 640x448 view. */
typedef struct {
    float rot[9], pos[3], cam[3], r[3], u[3], f[3];
    float focal;
    float focal_y;      /* vertical focal when the projection's differs (0: focal) */
    float jitter[2];    /* clip XY offset for authored CarFace sample */
    float near_clip;    /* 0: 0.2 */
    int   unlit;        /* course world: vertex colours only */
    float color_scale;  /* unlit: the vertex colours' scale (0: 1) */
    int   shadow;       /* contact shadow: vertex colours, their alpha times shadow_alpha, at most 1 (VU1 0x2A1) */
    float shadow_alpha; /* 0: 0.55 */
    float shadow_rgb[3]; /* shadow: added to the vertex colours (func_004A6100) */
    float env_color[2];  /* reflections: their colour times [0] + [1] * min(I.N, 0) / |I| (0, 0: as it is) */
    int   brake;         /* the brake pedal down: the cars' lit lamp branch (car callback 0) */
    /* func_0049AB58: the projection made with its near and far planes this
       much further, so that what is drawn comes out nearer in depth */
    float depth_bias;
    int   mirror;       /* reflected in the floor (y = 0): the car's floor reflection */
    int   screen_reflect; /* rear-view projection reverses triangle winding */
    /* Optional 20 column-major pose * inverse-bind matrices. The mesh and
       textures stay resident; only this palette changes between frames. */
    const float (*skin_palette)[16];
    float grasp; /* recorded hand closure 0..1; resident authored morph only */
    int authored_color;float color_mul[3],color_add[3]; /* 004A60E0/004A6100 */
    int wheel_blur; /* 00453E68: 0 sharp, 1 authored bank, >1 accumulation */
    /* The GS's fog as the VU1 programs give it to each vertex (004A40B0):
       beyond fog_distance of view depth the colour goes to fog_rgb, by at
       most 1 - fog_max: F = max*255 + 0.1 + (255 - max*255) * min(w, d) / w,
       colour * F + fog * (255 - F). A draw's own fog colour (commands 33-35)
       replaces fog_rgb. Off when fog is 0. */
    int fog;
    float fog_distance, fog_max, fog_rgb[3];
    float wheel_interval; /* absolute exposed radians */
    /* a colour filter on what is drawn: c' = (grey + (c - grey) * [3]) *
       [0..2] + [4], mixed in by [5] (0: none); the Dive's (func_004007D8) */
    float grade[6];
    /* the Time Trial's ghost: 1 writes the opaque parts' depth alone, 2
       then blends them over the scene at ghost_alpha (its nearest surface
       only); 0 as it is */
    int ghost;
    float ghost_alpha;
    const uint8_t *model_list; /* Vls0 u16 count + ordered u16 model indices */
} r3d_view;

int       r3d_available(void);
typedef struct { unsigned draws,triangles,culled_groups,wheel_passes,wheel_cache_hits; } r3d_stats;
r3d_stats r3d_frame_stats(void);
/* Upload (main thread, GL context current). The model is not kept. */
r3d_mesh *r3d_mesh_create(const gt4_model *m);
/* Body/brake subsets using the same texture table; references survive either
   mesh being destroyed first. No duplicate GPU texture upload. */
r3d_mesh *r3d_mesh_create_shared(const gt4_model *m,const r3d_mesh *textures);
void      r3d_mesh_destroy(r3d_mesh *mesh);
/* Starts drawing into the rectangle (x, y, w, h) of the 640x480 virtual
   screen; clears depth and fills it with the background image if given. */
void      r3d_begin(float x, float y, float w, float h, const tex1_image *environment);
/* The environment image's pixels changed (same pointer): upload it again
   at the next r3d_begin. */
void      r3d_environment_changed(void);
void      r3d_draw(r3d_mesh *mesh, const r3d_view *view);
/* Reuse a rigid single-material vertex buffer for small procedural props.
   No allocation, texture upload or GPU readback during updates. */
int r3d_mesh_update_positions(r3d_mesh *mesh,const float (*positions)[3],unsigned vertices,float alpha);
/* ModelFace's authored light; separate from the showroom/course lighting. */
typedef struct { float direction[3],ambient[3],diffuse[3],specular[3]; } r3d_ui_light;
void      r3d_draw_ui(r3d_mesh *mesh,const r3d_view *view,const r3d_ui_light *light);
/* Up to four directional lights, each ambient + diffuse + specular (the
   showroom studio's light.mot lights). */
typedef struct { int count; float direction[4][3],ambient[4][3],diffuse[4][3],specular[4][3];
                 float specular_scale; /* 0: 1 */
                 /* Above 0: the sum as the race's VU1 programs make it for a
                    car (0x23, 0x52, 0x83, chosen by the material's shine below
                    6, below 12, or more): ambient + diffuse * N.L + specular *
                    (N.H)^4, ^8 or ^16, limited to this value (the vertex
                    colour's 255 in the lights' own units), and the whole of
                    it times the texel. */
                 float vertex_limit; } r3d_lights;
void      r3d_draw_lit(r3d_mesh *mesh,const r3d_view *view,const r3d_lights *lights);
/* Studio silhouette on y=0, drawn/blurred on the GPU at native half size.
   Uses authored light position/darkness; blur approximates the PS2 jitter. */
int       r3d_floor_shadow(r3d_mesh *mesh,const r3d_view *view,const float light[3],float darkness,float blur);
/* Model framing may overflow its widget; keep the inherited UI clip. */
void      r3d_begin_ui(float x,float y,float w,float h);
/* the same with an environment picture for the reflections */
void      r3d_begin_ui_environment(float x,float y,float w,float h,const tex1_image *environment);
/* Clears the depth: what was drawn stays behind what follows. */
void      r3d_clear_depth(void);
/* CarFace passes, 00217058: copy_page selects a regular layer, otherwise
   a black reflected layer. FIX is truncated to 1/128 at composition. */
int       r3d_begin_layer(int copy_page);
void      r3d_end_layer(float opacity,int additive);
/* 00216E78 +0x178..0x1a0: merge sample i with FIX(1/(i+1)), then
   compose the final accumulated target. Intermediate samples stay on GPU. */
void      r3d_end_layer_sample(float opacity,int additive,int index,int samples);
/* An offscreen square (size x size, its own depth, cleared to black):
   draws go there until r3d_end_target reads it back (rgba: size*size*4,
   top row first; NULL: discard). Views use the same focal convention
   (focal 320 / focal_y 224: 90 degrees across). */
int       r3d_begin_target(int size);
void      r3d_end_target(uint8_t *rgba);
/* The view's alpha set to `alpha` (0..1 for the GS's 0..128), colours kept
   (func_00342370: the frame's alpha cleared before a mask is drawn). */
void      r3d_clear_alpha(float alpha);
/* As r3d_begin_target, keeping the view's own horizontal scale, so the
   square holds the same field as the view it will be laid over. */
int       r3d_begin_target_view(int size);
/* Ends that target and adds its picture over the view through the frame's
   alpha: frame += picture * rgb * alpha, the GS's (Cs - 0) * Ad + Cd
   (ALPHA 0x58), the frame's alpha left at one. Before that, `blur` times
   the picture is copied half a texel across (each pixel the mean of four,
   func_003411D0's loop), and it is stretched over the view from its first
   size - (blur - 0.5) texels, sampled linearly. */
void      r3d_end_target_add(const float rgb[3], int blur);
void      r3d_end(void);
/* Squares already in view space (x right, y up, z towards the viewer),
   each 4 corners in the GS's strip order (0 1 2, 1 3 2) of 9 floats: x y z,
   u v, r g b a; the texel times the colour (the GS's MODULATE), by mode:
   0 blended over the frame by alpha, (Cs - Cd) * As + Cd (ALPHA 0x44),
   1 added, Cs * As + Cd (0x48), 2 put as they are where the texel has
   alpha (ALPHA 0x64 with FIX 128; the alpha test's level not read);
   depth tested, not written. The tyres' smoke, the leaves. */
typedef struct r3d_sprite_texture r3d_sprite_texture;
r3d_sprite_texture *r3d_sprite_texture_create(const tex1_image *image);
void      r3d_sprite_texture_destroy(r3d_sprite_texture *t);
void      r3d_draw_sprites(const r3d_sprite_texture *t,const r3d_view *view,const float *corners,int squares,int mode);

#endif
