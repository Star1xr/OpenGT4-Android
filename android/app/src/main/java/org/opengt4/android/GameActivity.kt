package org.opengt4.android

import org.libsdl.app.SDLActivity
import java.io.File

/**
 * Oyunun kendisi: SDLActivity + yerel `libopengt4.so`.
 *
 * Son kutuphane SDL'e gore "main" kutuphanesidir; SDL::SDL_main oradan cagrilir.
 * Argumanlar oyunun PC CLI sozlesmesiyle aynidir (bkz. src/app/main.c):
 *   argv[1] = ISO (SAF fd uzerinden /proc/self/fd/<n>), --save-dir, --config-file
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
