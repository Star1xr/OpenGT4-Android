package org.opengt4.android

import org.libsdl.app.SDLActivity
import java.io.File

/**
 * The game itself: SDLActivity plus the native `libopengt4.so`.
 *
 * SDL treats the last library from getLibraries() as the entry point library and
 * calls SDL_main inside it. The arguments follow the PC CLI contract of the game
 * (see src/app/main.c):
 *   argv[1] = ISO (via the SAF fd, /proc/self/fd/<n>), --save-dir, --config-file,
 *   --touch-controls (auto, on, off) and --input-devices (the launcher's device
 *   flags).
 */
class GameActivity : SDLActivity() {

    override fun getLibraries(): Array<String> = arrayOf("SDL2", "opengt4")

    override fun getArguments(): Array<String> {
        val iso = DiscHolder.procPath().orEmpty()
        val saves = File(filesDir, "saves").apply { mkdirs() }.absolutePath
        val videoIni = File(filesDir, "video.ini").absolutePath
        val touchMode = intent.getStringExtra(EXTRA_TOUCH_MODE) ?: TouchMode.AUTO.id
        val devices = intent.getIntExtra(EXTRA_INPUT_DEVICES, 0)
        return arrayOf(
            iso, "--save-dir", saves, "--config-file", videoIni,
            "--touch-controls", touchMode, "--input-devices", devices.toString(),
        )
    }

    companion object {
        const val EXTRA_TOUCH_MODE = "touch_mode"
        const val EXTRA_INPUT_DEVICES = "input_devices"
    }
}
