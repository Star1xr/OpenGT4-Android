/* Internal interface between the shared GL renderer (render_gl.c), the video
   backends (video_gl.c, src/platform/android/android_video.c) and the 3D
   renderer. */
#ifndef OPENGT4_VIDEO_GL_H
#define OPENGT4_VIDEO_GL_H

#include "platform/platform.h"
#include "gl.h"

/* ---- backend entry points of the shared renderer (render_gl.c) ----
   The renderer owns the frame target, the MSAA scene target, the batched 2D
   drawing and the frame readback. It never creates a window or a context:
   each backend does that, then hands over the drawable size and calls the
   frame pair. upscale_mode is kept only if a resolver is registered. */
int  video_gl_start(int upscale_mode);
void video_gl_set_drawable(int width, int height);
void video_gl_frame_begin(void);
void video_gl_frame_end(void);
/* 0 while an offline capture wants the frame kept off the screen. */
int  video_gl_should_present(void);
void video_gl_shutdown(void);
/* FSR 1 on the desktop (video_gl.c); Android registers none, and the scene is
   copied at its own size. */
void video_gl_set_resolver(GLuint (*resolve)(GLuint source, int w, int h, int output_w, int output_h),
                           int (*available)(void));

/* Draws pending 2D quads (before 3D, captures and state changes). */
void   video_flush2d(void);
/* Bound frontend rendering to its native 60 Hz; race timing is separate. */
void video_set_menu_pacing(int enabled);
/* Opt-in offline movie export: one original 60 Hz update per real frame. */
int video_export_fixed_step(void);
/* Menu 2D maps to the whole PS2 framebuffer; race HUD has wide layout. */
void video_set_menu_projection(int enabled);
GLuint video_texture_id(const plat_texture *t);
/* Native or FSR 1 ultra quality / quality / balanced / performance (0..4). */
int video_set_upscale(int mode);
/* Size the GPU draws 3D scenes at, and its sample count. 0 keeps the drawable
   size and 4x MSAA, which is what the desktop backend wants; a backend with a
   resolution setting of its own (Android) renders the scene there and lets the
   blit scale it to the output. */
void video_set_render_size(int width, int height);
void video_set_samples(int samples);
/* Original display ratio: 0 = 4:3, 1 = 16:9. */
void video_set_aspect(int widescreen);
float video_projection_x(void);
int video_fsr_available(void);
void video_begin_scene(void);
void video_end_scene(void);
/* UI filtering: 0 linear, 1 sharp cubic, 2 nearest. Text is separate. */
void video_ui_quality(int filter,float text_sharpness);
int video_set_vsync(int enabled);
/* A rectangle of the virtual screen as a GL viewport in frame pixels. */
void   video_viewport(float x, float y, float w, float h, int *vx, int *vy, int *vw, int *vh);
/* Overlay drawing in framebuffer pixels: one unit is one pixel, x runs right
   from the left edge, y runs down from the top edge, no aspect fit and no
   letterbox. Overlays (the on-screen pad) use this so they keep the same size
   and the same place at any Resolution, panel or display ratio. begin()
   returns the frame size the overlay is laid out in; end() restores the view
   the game was drawing with, so the pair has to wrap the overlay's quads. */
void   video_ui_pixels_begin(int *width, int *height);
void   video_ui_pixels_end(void);
/* Copies what the frame holds in a rectangle of the virtual screen into t
   (created or resized as needed; NULL makes one). Drawn back with
   plat_draw_quad4 and v from 1 (top) to 0: GL rows run bottom up. */
float video_view_height(void);
plat_texture *video_capture_frame(plat_texture *t);
plat_texture *video_grab(plat_texture *t, float x, float y, float w, float h);

/* ResultLicense 004A7D90/004A8CB8. Resident GPU capture and cached CLUT. */
void video_license_image(const plat_texture *t,float x,float y,float w,float h,
                         const uint8_t lut[256][3],const uint8_t mix[3][3]);
/* A clipped 2D HUD inside a race viewport. NULL panel via h=0 restores. */
void video_ui_panel(float y,float h);
#endif
