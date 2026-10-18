package org.opengt4.android

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.ParcelFileDescriptor
import android.provider.OpenableColumns
import org.opengt4.android.bridge.NativeBridge
import java.io.FileNotFoundException

/**
 * Holds the selected disc: the fd handed out by SAF stays open for the process
 * lifetime and the game reads /proc/self/fd/<n> directly. No copy, no extraction
 * (see docs/ANDROID.md).
 *
 * The kernel reopens that path against the same dentry, so no path permission is
 * required. Providers that hand out a non-seekable fd (cloud providers usually
 * return a pipe) are rejected with a clear error.
 */
object DiscHolder {

    private const val prefsName = "opengt4_launcher"
    private const val keyUri = "disc_uri"

    /** A GT4 disc image is at least this large; filters out accidental picks. */
    private const val minBytes = 100L * 1024L * 1024L

    private var descriptor: ParcelFileDescriptor? = null

    var name: String = ""
        private set
    var bytes: Long = -1L
        private set

    /** null on success, otherwise an error key from strings.xml. */
    var error: String? = null
        private set

    /**
     * Result of the full disc check (SYSTEM.CNF, boot file, CORE.GT4, volume).
     * null means it has not been checked in this session, which is the state
     * after reopening a saved disc at startup; the launcher only checks when the
     * user picks a disc.
     */
    var validated: Boolean? = null
        private set

    /** Description or rejection message coming from the shared launcher code. */
    var validation: String? = null
        private set

    val ready: Boolean get() = descriptor != null

    /** The path handed to the game as `argv[1]`. */
    fun procPath(): String? = descriptor?.let { "/proc/self/fd/${it.fd}" }

    /**
     * Records the full disc check. Only the native side can decide this: it is
     * the same code path the PC launcher uses.
     */
    fun setValidation(ok: Boolean, message: String) {
        validated = ok
        validation = message
    }

    fun savedUri(ctx: Context): String? = prefs(ctx).getString(keyUri, null)

    fun open(ctx: Context, uri: Uri, persistable: Boolean): Boolean {
        close()
        if (persistable) {
            try {
                ctx.contentResolver.takePersistableUriPermission(
                    uri, Intent.FLAG_GRANT_READ_URI_PERMISSION
                )
            } catch (ignored: SecurityException) {
                // Some providers do not grant a persistent permission; the disc
                // still works for this session.
            }
        }
        val opened = try {
            ctx.contentResolver.openFileDescriptor(uri, "r")
        } catch (e: FileNotFoundException) {
            null
        } catch (e: SecurityException) {
            null
        }
        if (opened == null) {
            error = "open"
            return false
        }
        val out = LongArray(2)
        if (NativeBridge.nativeDiscProbe(opened.fd, out) != 1 || out[0] != 1L) {
            opened.close()
            error = "seek"
            return false
        }
        if (out[1] < minBytes) {
            opened.close()
            error = "small"
            return false
        }
        descriptor = opened
        bytes = out[1]
        name = displayName(ctx, uri)
        error = null
        prefs(ctx).edit().putString(keyUri, uri.toString()).apply()
        return true
    }

    /** Reopens the saved disc (works again as long as the permission persists). */
    fun reopenSaved(ctx: Context): Boolean {
        val saved = savedUri(ctx) ?: return false
        return open(ctx, Uri.parse(saved), persistable = false)
    }

    fun forget(ctx: Context) {
        close()
        name = ""
        bytes = -1L
        error = null
        prefs(ctx).edit().remove(keyUri).apply()
    }

    private fun close() {
        try {
            descriptor?.close()
        } catch (ignored: Exception) {
        }
        descriptor = null
        /* A different file must be checked again. */
        validated = null
        validation = null
    }

    private fun prefs(ctx: Context) = ctx.getSharedPreferences(prefsName, Context.MODE_PRIVATE)

    private fun displayName(ctx: Context, uri: Uri): String {
        try {
            ctx.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)
                ?.use { cursor ->
                    if (cursor.moveToFirst() && !cursor.isNull(0)) return cursor.getString(0)
                }
        } catch (ignored: Exception) {
        }
        return uri.lastPathSegment ?: "disc.iso"
    }
}
