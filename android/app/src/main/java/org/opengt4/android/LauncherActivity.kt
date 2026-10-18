package org.opengt4.android

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.opengt4.android.bridge.NativeBridge
import org.opengt4.android.bridge.VideoOptions
import org.opengt4.android.ui.LauncherScreen
import org.opengt4.android.ui.theme.OpenGt4Theme
import java.io.File

/**
 * Launcher: disc selection, validation and Play. The game runs in its own
 * Activity (GameActivity) on an SDLSurface; the launcher is Compose.
 *
 * The disc is checked with the same code the PC launcher uses, and the check
 * reads the image, so it runs off the main thread. video.ini and the player
 * profiles are the files the game itself reads.
 */
class LauncherActivity : ComponentActivity() {

    private var discName by mutableStateOf("")
    private var discBytes by mutableStateOf(-1L)
    private var ready by mutableStateOf(false)
    private var busy by mutableStateOf(false)
    private var errorKey by mutableStateOf<String?>(null)
    private var banner by mutableStateOf("")
    private var validationText by mutableStateOf<String?>(null)
    private var validated by mutableStateOf<Boolean?>(null)

    /** Incremented on every pick; the check runs when it changes. */
    private var checkTicket by mutableStateOf(0L)

    /** Options as loaded from video.ini; the settings UI arrives in phase 5. */
    private var videoOptions by mutableStateOf<VideoOptions?>(null)

    private val videoFile: String get() = File(filesDir, "video.ini").absolutePath
    private val savesDir: String get() = File(filesDir, "saves").absolutePath

    private val pickDisc = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri == null) return@registerForActivityResult
        busy = true
        validated = null
        validationText = null
        val opened = DiscHolder.open(this, uri, persistable = true)
        sync(opened)
        if (opened) checkTicket++ else busy = false
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        banner = runCatching { NativeBridge.nativeVersion() }.getOrDefault("")
        // A saved disc stays ready across launches while the permission lasts.
        sync(DiscHolder.reopenSaved(this))

        setContent {
            OpenGt4Theme {
                LauncherScreen(
                    discName = discName,
                    discBytes = discBytes,
                    ready = ready,
                    busy = busy,
                    validated = validated,
                    validationText = validationText,
                    errorText = errorKey?.let { key ->
                        when (key) {
                            "seek" -> getString(R.string.error_not_seekable)
                            "small" -> getString(R.string.error_too_small)
                            else -> getString(R.string.error_open)
                        }
                    },
                    banner = banner,
                    onSelect = { pickDisc.launch(arrayOf("*/*")) },
                    onPlay = { play() },
                )

                // Video options: the files are small and no disc is touched.
                LaunchedEffect(Unit) {
                    val values = IntArray(8)
                    val ok = withContext(Dispatchers.IO) {
                        NativeBridge.nativeLoadOptions(videoFile, savesDir, values)
                    }
                    if (ok != 1) return@LaunchedEffect
                    videoOptions = VideoOptions.fromArray(values)
                    // First run: write the defaults so the file exists and can
                    // be edited; later runs keep whatever is in it.
                    if (!File(videoFile).exists()) {
                        withContext(Dispatchers.IO) {
                            NativeBridge.nativeSaveOptions(videoFile, savesDir, 1, values)
                        }
                    }
                }

                // Full disc check, started by the picker above.
                LaunchedEffect(checkTicket) {
                    if (checkTicket == 0L) return@LaunchedEffect
                    val path = DiscHolder.procPath() ?: return@LaunchedEffect
                    val out = IntArray(1)
                    val message = withContext(Dispatchers.IO) {
                        NativeBridge.nativeValidateDisc(path, out)
                    }
                    DiscHolder.setValidation(out[0] == 1, message)
                    validated = out[0] == 1
                    validationText = message
                    busy = false
                }
            }
        }
    }

    private fun sync(ok: Boolean) {
        ready = ok && DiscHolder.ready
        discName = if (ready) DiscHolder.name else ""
        discBytes = if (ready) DiscHolder.bytes else -1L
        errorKey = DiscHolder.error
    }

    private fun play() {
        if (!DiscHolder.ready || validated == false) return
        startActivity(Intent(this, GameActivity::class.java))
    }
}
