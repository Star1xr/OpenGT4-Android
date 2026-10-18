package org.opengt4.android

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import org.opengt4.android.bridge.NativeBridge
import org.opengt4.android.ui.LauncherScreen
import org.opengt4.android.ui.theme.OpenGt4Theme

/**
 * Launcher: disc secimi + dogrulama + Play. Oyun ayri Activity'de (GameActivity)
 * SDLSurface uzerinde kosar; launcher Compose'dur (AGENTS.md karari).
 */
class LauncherActivity : ComponentActivity() {

    private var discName by mutableStateOf("")
    private var discBytes by mutableStateOf(-1L)
    private var ready by mutableStateOf(false)
    private var busy by mutableStateOf(false)
    private var errorKey by mutableStateOf<String?>(null)
    private var banner by mutableStateOf("")

    private val pickDisc = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri == null) return@registerForActivityResult
        busy = true
        val ok = DiscHolder.open(this, uri, persistable = true)
        sync(ok)
        busy = false
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        banner = runCatching { NativeBridge.nativeVersion() }.getOrDefault("")
        // Ayni oturumda yeniden acilirsa kalici izinle disc hazir kalir.
        sync(DiscHolder.reopenSaved(this))

        setContent {
            OpenGt4Theme {
                LauncherScreen(
                    discName = discName,
                    discBytes = discBytes,
                    ready = ready,
                    busy = busy,
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
        if (!DiscHolder.ready) return
        startActivity(Intent(this, GameActivity::class.java))
    }
}
