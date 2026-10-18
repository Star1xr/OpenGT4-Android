/*
 * JNI bridge: org.opengt4.android.bridge.NativeBridge (Kotlin) <-> C.
 *
 * Only thin wrappers live here; the game logic stays on the C side. Disc
 * validation and the launcher options come from the same code the PC build uses
 * (src/app/launcher_config.c), so a disc is accepted or rejected here exactly as
 * it would be there.
 *
 * The disc fd probe enforces the launcher's "read in place, never copy" rule:
 * the fd handed out by SAF stays open for the process lifetime and the game
 * opens /proc/self/fd/<n> directly, so the kernel reopens the same file and no
 * copy is ever made.
 */
#include "platform/platform.h"
#include "android_platform.h"
#include "launcher.h"

#include <jni.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

const char *plat_android_banner(void)
{
    static char banner[160];
    static int ready;
    if (!ready) {
        snprintf(banner, sizeof banner,
                 "OpenGT4 Android %s (based on OpenGT4, unofficial)",
                 OPENGT4_ANDROID_VERSION);
        ready = 1;
    }
    return banner;
}

int plat_android_fd_probe(int fd, uint64_t *size)
{
    if (fd < 0) return 0;
    char link[64];
    snprintf(link, sizeof link, "/proc/self/fd/%d", fd);
    /* Providers that cannot seek (pipes, sockets) fail here with ENXIO. */
    int copy = open(link, O_RDONLY | O_CLOEXEC);
    if (copy < 0) return 0;
    off_t end = lseek(copy, 0, SEEK_END);
    int ok = end >= 0 && lseek(copy, 0, SEEK_SET) == 0;
    if (ok && size) *size = (uint64_t)end;
    close(copy);
    return ok;
}

static jstring to_java_string(JNIEnv *env, const char *text)
{
    return (*env)->NewStringUTF(env, text ? text : "");
}

/* Copies a Java string into a fixed buffer; empty result on failure. */
static int to_c_string(JNIEnv *env, jstring text, char *out, size_t size)
{
    out[0] = 0;
    if (!text || size == 0) return 0;
    const char *chars = (*env)->GetStringUTFChars(env, text, NULL);
    if (!chars) return 0;
    snprintf(out, size, "%s", chars);
    (*env)->ReleaseStringUTFChars(env, text, chars);
    return out[0] != 0;
}

JNIEXPORT jstring JNICALL
Java_org_opengt4_android_bridge_NativeBridge_nativeVersion(JNIEnv *env, jobject thiz)
{
    (void)thiz;
    return to_java_string(env, plat_android_banner());
}

JNIEXPORT jstring JNICALL
Java_org_opengt4_android_bridge_NativeBridge_nativeGlInfo(JNIEnv *env, jobject thiz)
{
    (void)thiz;
    return to_java_string(env, plat_android_gl_info());
}

JNIEXPORT jint JNICALL
Java_org_opengt4_android_bridge_NativeBridge_nativeDiscProbe(JNIEnv *env, jobject thiz,
                                                             jint fd, jlongArray out)
{
    (void)thiz;
    uint64_t size = 0;
    int ok = plat_android_fd_probe((int)fd, &size);
    if (out && (*env)->GetArrayLength(env, out) >= 2) {
        jlong values[2];
        values[0] = ok ? 1 : 0;
        values[1] = (jlong)size;
        (*env)->SetLongArrayRegion(env, out, 0, 2, values);
    }
    if (ok) {
        ALOGI("disc fd %d is seekable, %llu bytes", (int)fd, (unsigned long long)size);
    } else {
        ALOGW("disc fd %d is not seekable (provider may return a pipe)", (int)fd);
    }
    return ok ? 1 : 0;
}

/*
 * Reads the disc the way the game does: SYSTEM.CNF, the boot file, CORE.GT4 and
 * the volume. Returns the description ("Gran Turismo 4 - ...") or the reason the
 * disc was rejected; out[0] carries the verdict. This reads several megabytes
 * from the image, so Kotlin calls it off the main thread.
 */
JNIEXPORT jstring JNICALL
Java_org_opengt4_android_bridge_NativeBridge_nativeValidateDisc(JNIEnv *env, jobject thiz,
                                                               jstring path, jintArray out)
{
    (void)thiz;
    char image[LAUNCHER_PATH + 64];
    char message[512] = "No disc image was selected.";
    int ok = 0;
    if (to_c_string(env, path, image, sizeof image))
        ok = launcher_validate_disc(image, message, sizeof message);
    if (out && (*env)->GetArrayLength(env, out) >= 1) {
        jint verdict = ok ? 1 : 0;
        (*env)->SetIntArrayRegion(env, out, 0, 1, &verdict);
    }
    if (ok) ALOGI("disc: %s", message);
    else ALOGW("disc rejected: %s", message);
    return to_java_string(env, message);
}

/*
 * Launcher video options from video.ini (and <video>.game_options), with the
 * defaults and the career values applied by the shared launcher code. out[0..7]:
 * width, height, fullscreen, vsync, upscaling, ui filter, text sharpness,
 * aspect ratio.
 */
JNIEXPORT jint JNICALL
Java_org_opengt4_android_bridge_NativeBridge_nativeLoadOptions(JNIEnv *env, jobject thiz,
                                                              jstring video, jstring saves,
                                                              jintArray out)
{
    (void)thiz;
    char video_file[LAUNCHER_PATH];
    char saves_dir[LAUNCHER_PATH];
    if (!to_c_string(env, video, video_file, sizeof video_file) ||
        !to_c_string(env, saves, saves_dir, sizeof saves_dir))
        return 0;

    launcher_options options;
    if (!launcher_options_load(&options, video_file, saves_dir)) return 0;

    if (out && (*env)->GetArrayLength(env, out) >= 8) {
        jint values[8] = { options.width, options.height, options.fullscreen, options.vsync,
                           options.upscale, options.filter, options.text, options.aspect };
        (*env)->SetIntArrayRegion(env, out, 0, 8, values);
    }
    ALOGI("video options: %dx%d fullscreen=%d vsync=%d upscaling=%d filter=%d text=%d aspect=%d",
          options.width, options.height, options.fullscreen, options.vsync,
          options.upscale, options.filter, options.text, options.aspect);
    return 1;
}

/*
 * Writes the video options back (group 1). Values are applied on top of the
 * loaded ones, so nothing else in the file is lost. upscaling is forced to 0:
 * the Android build has no FSR, and the file must not claim otherwise.
 */
JNIEXPORT jint JNICALL
Java_org_opengt4_android_bridge_NativeBridge_nativeSaveOptions(JNIEnv *env, jobject thiz,
                                                              jstring video, jstring saves,
                                                              jint group, jintArray values)
{
    (void)thiz;
    if (!values || (*env)->GetArrayLength(env, values) < 8) return 0;
    char video_file[LAUNCHER_PATH];
    char saves_dir[LAUNCHER_PATH];
    if (!to_c_string(env, video, video_file, sizeof video_file) ||
        !to_c_string(env, saves, saves_dir, sizeof saves_dir))
        return 0;

    launcher_options options;
    if (!launcher_options_load(&options, video_file, saves_dir)) return 0;

    jint edited[8];
    (*env)->GetIntArrayRegion(env, values, 0, 8, edited);
    options.width = edited[0];
    options.height = edited[1];
    options.fullscreen = edited[2];
    options.vsync = edited[3];
    options.upscale = 0;
    options.filter = edited[5];
    options.text = edited[6];
    options.aspect = edited[7];

    int ok = launcher_options_save(&options, video_file, saves_dir, (int)group);
    if (ok) ALOGI("video options saved (group %d)", (int)group);
    else ALOGW("video options could not be saved");
    return ok ? 1 : 0;
}
