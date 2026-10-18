package org.opengt4.android.bridge

/**
 * Bridge to the native library (src/platform/android/android_jni.c).
 *
 * libSDL2 is loaded first so that libopengt4.so's DT_NEEDED entry resolves.
 */
object NativeBridge {

    init {
        System.loadLibrary("SDL2")
        System.loadLibrary("opengt4")
    }

    /** Application banner: "OpenGT4 Android <version> (based on OpenGT4, unofficial)". */
    external fun nativeVersion(): String

    /**
     * Probes a disc fd: is it seekable and how large is the file?
     *
     * @param fd  raw fd handed out by SAF (must stay open for the process lifetime)
     * @param out out[0] = 1 when seekable, out[1] = size in bytes
     * @return 1 on success, 0 on failure
     */
    external fun nativeDiscProbe(fd: Int, out: LongArray): Int

    /** GLES driver summary of this device; empty until a context exists. */
    external fun nativeGlInfo(): String
}
