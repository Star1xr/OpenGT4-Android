package org.opengt4.android.bridge

/**
 * Launcher video options, the same fields the PC launcher reads from video.ini
 * (src/app/launcher_config.c).
 *
 * There is no upscaling field: the Android build has no FSR, and the native side
 * forces that entry to 0 when it writes the file. The size fields are hints; on
 * Android the surface size comes from the display.
 */
data class VideoOptions(
    val width: Int,
    val height: Int,
    val fullscreen: Int,
    val vsync: Int,
    val filter: Int,
    val text: Int,
    val aspect: Int,
) {
    /** The native layout: eight ints, with upscaling kept at 0. */
    fun toArray(): IntArray = intArrayOf(width, height, fullscreen, vsync, 0, filter, text, aspect)

    companion object {
        fun fromArray(values: IntArray): VideoOptions? {
            if (values.size < 8) return null
            return VideoOptions(
                width = values[0],
                height = values[1],
                fullscreen = values[2],
                vsync = values[3],
                filter = values[5],
                text = values[6],
                aspect = values[7],
            )
        }
    }
}
