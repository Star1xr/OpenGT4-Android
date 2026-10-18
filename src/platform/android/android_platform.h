/*
 * Android backend'in ic yuzu.
 *
 * Oyun kodu (src/core) bu basligi gormez; yalnizca include/platform/platform.h
 * kullanir. Buradakiler src/platform/android/* arasinda paylasilir.
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

/* "OpenGT4 Android <surum> (based on OpenGT4, unofficial)" - GPLv3 + NOTICE §7. */
const char *plat_android_banner(void);

/* ---- video (android_video.c) ---- */
/* Baglamdan sonra gercek GLES surumu. 0: baglam yok. */
int plat_android_es_major(void);
int plat_android_es_minor(void);
/* Launcher/JNI icin tek satirlik surucu ozeti (bossiz degil: baglam yok demek). */
const char *plat_android_gl_info(void);
/* Gercek ES 3.2 baglami kuruldu mu? 0 ise 3.0 fallback (uyari loglanmistir). */
int plat_android_es32(void);

/* ---- fayda ---- */
/* fd aranabilir mi (seek) ve boyutu; basarida 1. */
int plat_android_fd_probe(int fd, uint64_t *size);

#endif /* OPENGT4_ANDROID_PLATFORM_H */
