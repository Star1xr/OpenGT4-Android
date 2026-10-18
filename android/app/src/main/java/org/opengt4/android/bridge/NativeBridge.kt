package org.opengt4.android.bridge

/**
 * Yerel kutuphane kopru'su (src/platform/android/android_jni.c).
 *
 * libSDL2 once yuklenir: libopengt4.so'nun DT_NEEDED kaydi boylece cozulur.
 */
object NativeBridge {

    init {
        System.loadLibrary("SDL2")
        System.loadLibrary("opengt4")
    }

    /** Uygulama banner'i: "OpenGT4 Android <surum> (based on OpenGT4, unofficial)". */
    external fun nativeVersion(): String

    /**
     * Disc fd sondasi: fd aranabilir mi ve boyutu ne?
     *
     * @param fd  SAF'in verdigi ham dosya tanimlayicisi (surec yasadikca acik kalmali)
     * @param out out[0] = 1 aranabilir, out[1] = bayt cinsinden boyut
     * @return 1 basarili, 0 hata
     */
    external fun nativeDiscProbe(fd: Int, out: LongArray): Int

    /** Cihazin GLES surumunu dondurur (baglam yoksa bos). */
    external fun nativeGlInfo(): String
}
