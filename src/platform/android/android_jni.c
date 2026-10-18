/*
 * JNI bridge: org.opengt4.android.bridge.NativeBridge (Kotlin) <-> C.
 *
 * Only thin wrappers live here; the game logic stays on the C side. The disc fd
 * probe enforces the launcher's "read in place, never copy" rule: the fd handed
 * out by SAF stays open for the process lifetime and the game opens
 * /proc/self/fd/<n> directly, so the kernel reopens the same file and no copy
 * is ever made.
 */
#include "platform/platform.h"
#include "android_platform.h"

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
