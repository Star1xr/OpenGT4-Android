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

    /**
     * Reads the disc the way the game does and reports what it found.
     *
     * This opens the volume inside the image, so it reads several megabytes: call
     * it from a background thread. The message is either the disc description
     * ("Gran Turismo 4 - NTSC-U / SCUS-97328 / v2.00") or the reason it was
     * rejected, and it is meant to be shown to the user.
     *
     * @param path native path, normally /proc/self/fd/<n>
     * @param out  out[0] = 1 when the disc was accepted, 0 otherwise
     */
    external fun nativeValidateDisc(path: String, out: IntArray): String

    /**
     * Video options of the launcher, from video.ini and its .game_options file.
     *
     * @param video path of video.ini
     * @param saves directory of the player profiles (career values win)
     * @param out   out[0..7] = width, height, fullscreen, vsync, upscaling,
     *              ui filter, text sharpness, aspect ratio
     * @return 1 on success
     */
    external fun nativeLoadOptions(video: String, saves: String, out: IntArray): Int

    /**
     * Writes the video options back (group 1: video fields only).
     *
     * @param values the same eight fields as [nativeLoadOptions]
     */
    external fun nativeSaveOptions(video: String, saves: String, group: Int, values: IntArray): Int

    /** GLES driver summary of this device; empty until a context exists. */
    external fun nativeGlInfo(): String
}
