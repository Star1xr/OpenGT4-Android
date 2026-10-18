package org.opengt4.android

import org.libsdl.app.SDLActivity
import java.io.File

/**
 * The game itself: SDLActivity plus the native `libopengt4.so`.
 *
 * SDL treats the last library from getLibraries() as the entry point library and
 * calls SDL_main inside it. The arguments follow the PC CLI contract of the game
 * (see src/app/main.c):
 *   argv[1] = ISO (via the SAF fd, /proc/self/fd/<n>), --save-dir, --config-file
 */
class GameActivity : SDLActivity() {

    override fun getLibraries(): Array<String> = arrayOf("SDL2", "opengt4")

    override fun getArguments(): Array<String> {
        val iso = DiscHolder.procPath().orEmpty()
        val saves = File(filesDir, "saves").apply { mkdirs() }.absolutePath
        val videoIni = File(filesDir, "video.ini").absolutePath
        return arrayOf(iso, "--save-dir", saves, "--config-file", videoIni)
    }
}
