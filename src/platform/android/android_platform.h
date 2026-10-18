/*
 * Internal surface of the Android backend.
 *
 * Game code (src/core) never sees this header; it only uses
 * include/platform/platform.h. Everything here is shared between the files in
 * src/platform/android/.
 */
#ifndef OPENGT4_ANDROID_PLATFORM_H
#define OPENGT4_ANDROID_PLATFORM_H

#include <android/log.h>
#include <stdint.h>

#define OPENGT4_ANDROID_LOG_TAG "OpenGT4"

#ifndef OPENGT4_ANDROID_VERSION
#define OPENGT4_ANDROID_VERSION "0.0.0-local"
#endif

#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, OPENGT4_ANDROID_LOG_TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, OPENGT4_ANDROID_LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, OPENGT4_ANDROID_LOG_TAG, __VA_ARGS__)

/* "OpenGT4 Android <version> (based on OpenGT4, unofficial)" - GPLv3 + NOTICE §7. */
const char *plat_android_banner(void);

/* ---- log ---- */
/* One line to both logcat and the game's log file. logcat needs adb and the
   player cannot share it, so every Android line goes to the file too. */
void plat_android_log(int priority, const char *format, ...);

/* ---- on-screen pad (android_touch.c) ---- */
/* The touch PS2 controls. They are presented to the game as a virtual
   controller, so the shared device adapter and the game's key configuration
   stay untouched. */
void plat_android_touch_init(void);
/* Reads the frame's fingers into the controller; call once per frame. */
void plat_android_touch_update(void);
/* Draws the pad over the finished frame, before the present. */
void plat_android_touch_draw(void);
void plat_android_touch_shutdown(void);

/* ---- video (android_video.c) ---- */
/* Real GLES version after the context was created; 0 when there is no context. */
int plat_android_es_major(void);
int plat_android_es_minor(void);
/* One-line driver summary for the launcher/JNI; empty means "no context". */
const char *plat_android_gl_info(void);
/* Non-zero when a real ES 3.2 context is current; 0 means the ES 3.0 fallback
   was used (a warning was logged). */
int plat_android_es32(void);

/* ---- helpers ---- */
/* Is the fd seekable and how large is it? Returns 1 on success. */
int plat_android_fd_probe(int fd, uint64_t *size);

#endif /* OPENGT4_ANDROID_PLATFORM_H */
