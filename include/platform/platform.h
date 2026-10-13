/*
 * Platform abstraction layer.
 *
 * Game code (src/core) must only touch hardware through these interfaces.
 * A port to a new system = implementing this header under src/platform/<name>/.
 *
 * Rules for core code:
 *  - no PS2 memory-layout assumptions, no 32-bit pointer assumptions
 *  - no direct file, GPU, audio or input calls
 */
#ifndef OPENGT4_PLATFORM_H
#define OPENGT4_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- time ---- */
uint64_t plat_time_ns(void);
void     plat_sleep_ms(uint32_t ms);

/* ---- filesystem (reads the user's own game files: disc image, extracted data) ---- */
typedef struct plat_file plat_file;
plat_file *plat_file_open(const char *path);
uint64_t   plat_file_size(plat_file *f);
/* Reads n bytes at an absolute 64-bit offset (disc images exceed 4 GB).
   Returns the number of bytes read. */
size_t     plat_file_read_at(plat_file *f, uint64_t offset, void *dst, size_t n);
void       plat_file_close(plat_file *f);
/* the free space for the user on the disk holding path (bytes); 0 unknown */
int        plat_disk_free(const char *path, uint64_t *bytes);
/* Native album storage replaces the memory card device. Basenames only. */
typedef struct { char name[128]; uint64_t bytes,modified; } plat_directory_entry;
int plat_directory_list(const char *path,const char *suffix,plat_directory_entry *out,int capacity);
int plat_file_write(const char *path,const void *data,size_t bytes);
int plat_file_remove(const char *path);
uint8_t *plat_png_read(const char *path,int *width,int *height);
int plat_jpeg_png(const uint8_t *jpeg,size_t size,uint8_t **png,size_t *length);

/* ---- video ---- */
typedef struct {
    int width, height;
    int fullscreen;
    int vsync;
    int upscale;     /* 0 native; 1..4 FSR 1 ultra quality/quality/balanced/performance */
} plat_video_desc;

int  plat_video_init(const plat_video_desc *desc);
void plat_video_begin_frame(void);
void plat_video_end_frame(void);
void plat_video_shutdown(void);

/* 2D drawing. Coordinates are in a virtual screen set by plat_video_set_view
   (the game's UI uses 640x448); the backend scales it to the window. */
typedef struct plat_texture plat_texture;
void          plat_video_set_view(float width, float height);
plat_texture *plat_texture_create(int width, int height, const uint8_t *rgba);
void plat_texture_update(plat_texture *t,const uint8_t *rgba);
void          plat_texture_destroy(plat_texture *t);
/* Textured quad (t may be NULL for a solid colour). uv in 0..1 of the texture.
   color multiplies the texture, RGBA 0..255. Alpha-blended. */
void          plat_draw_quad(const plat_texture *t, float x, float y, float w, float h,
                             float u0, float v0, float u1, float v1,
                             const uint8_t color[4]);
/* Solid convex quad, vertices TL/TR/BL/BR; also accepts a degenerate
   triangle. Native vector fills use the same batched 2D renderer. */
void plat_draw_solid_quad(const float xy[4][2],const uint8_t color[4]);
/* Fixed RGB blend coefficient in 0..1, without RGBA byte quantization.
   Used for captured menu surfaces and solid transition overlays. */
void plat_draw_quad_blend(const plat_texture *t,float x,float y,float w,float h,
                           float u0,float v0,float u1,float v1,
                           const uint8_t color[4],float factor);
/* Same, with one colour per corner: top-left, top-right, bottom-left, bottom-right. */
void          plat_draw_quad4(const plat_texture *t, float x, float y, float w, float h,
                              float u0, float v0, float u1, float v1,
                              const uint8_t colors[4][4]);
void plat_draw_rotated(const plat_texture *t,float x,float y,float w,float h,
                       float cx,float cy,float degrees,const uint8_t color[4]);
/* Baseline-relative horizontal shear, in screen units per vertical unit. */
void plat_draw_glyph(const plat_texture *t, float x, float y, float w, float h,
                     float baseline, float shear, float u0, float v0, float u1, float v1,
                     const uint8_t color[4]);
/* Restricts drawing to a rectangle of the virtual screen; w <= 0 removes it. */
void          plat_set_clip(float x, float y, float w, float h);

/* ---- audio ---- */
typedef struct {
    int sample_rate;
    int channels;
} plat_audio_desc;

typedef void (*plat_audio_cb)(int16_t *out, int frames, void *user);
int  plat_audio_init(const plat_audio_desc *desc, plat_audio_cb cb, void *user);
int plat_audio_set_music(plat_audio_cb cb,void *user);
void plat_audio_fade_music(float seconds);
/* Stops the music source only while `user` still owns it. */
void plat_audio_release_music(void *user);
/* Frames of the current music source the device has consumed since
   plat_audio_set_music; a clock for pictures that follow the sound. */
uint64_t plat_audio_music_position(void);
void plat_audio_shutdown(void);
/* Copies interleaved PCM for asynchronous mixed playback. Returns 1 on success. */
int plat_audio_play(const int16_t *pcm, size_t frames, int rate, int channels);

/* ---- input ---- */
#define PLAT_MAX_AXES    8
#define PLAT_MAX_BUTTONS 32

typedef struct {
    float    axes[PLAT_MAX_AXES];   /* -1..1 (steering, throttle, brake, ...) */
    uint32_t buttons;               /* bitmask */
} plat_pad_state;

int  plat_input_init(void);
int  plat_input_poll(int index, plat_pad_state *out);
/* Force feedback: signed torque, -1..1. No-op on devices without FFB. */
void plat_input_set_ffb(int index, float torque);
/* Raw SDL steering wheel in this player's port (controller kind 4). */
int plat_input_is_wheel(int index);
void plat_input_shutdown(void);

/* ---- app lifecycle ---- */
int  plat_should_quit(void);
/* Synchronous dialog (presented inside the game by the app). confirm=0: OK; confirm=1: Yes/No.
   Returns 1 accepted, 0 declined, -1 on backend error. */
int plat_dialog(const char *title, const char *message, int confirm);

#ifdef __cplusplus
}
#endif

#endif /* OPENGT4_PLATFORM_H */
