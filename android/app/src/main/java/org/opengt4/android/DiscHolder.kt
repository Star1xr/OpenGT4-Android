package org.opengt4.android

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.ParcelFileDescriptor
import android.provider.OpenableColumns
import org.opengt4.android.bridge.NativeBridge
import java.io.FileNotFoundException

/**
 * Secilen disc'i tutar: SAF'in verdigi fd surec yasadikca acik kalir, oyun
 * `/proc/self/fd/<n>` yolunu dogrudan okur. Kopya/cikarma yok (docs/ANDROID.md).
 *
 * `/proc/self/fd/<n>` kernel tarafindan ayni dentry ile yeniden acilir; yol
 * izni gerekmez, dosya icin tekrar izin sorulmaz. Saglayici aranabilir fd
 * vermezse (bulut saglayicilari pipe doner) acik hata verilir.
 */
object DiscHolder {

    private const val prefsName = "opengt4_launcher"
    private const val keyUri = "disc_uri"

    /** Bir GT4 diski en az bu kadar bayt olmali (yanlis dosya secimini eler). */
    private const val minBytes = 100L * 1024L * 1024L

    private var descriptor: ParcelFileDescriptor? = null

    var name: String = ""
        private set
    var bytes: Long = -1L
        private set

    /** null basarili; aksi halde strings.xml'deki hata anahtari. */
    var error: String? = null
        private set

    val ready: Boolean get() = descriptor != null

    /** Oyunun `argv[1]` olarak alacagi yol. */
    fun procPath(): String? = descriptor?.let { "/proc/self/fd/${it.fd}" }

    fun savedUri(ctx: Context): String? = prefs(ctx).getString(keyUri, null)

    fun open(ctx: Context, uri: Uri, persistable: Boolean): Boolean {
        close()
        if (persistable) {
            try {
                ctx.contentResolver.takePersistableUriPermission(
                    uri, Intent.FLAG_GRANT_READ_URI_PERMISSION
                )
            } catch (ignored: SecurityException) {
                // Bazi saglayicilar kalici izin vermez; oturum boyunca yine calisir.
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

    /** Kayitli disc'i tekrar acar (kalici izin varsa yeniden baslar). */
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
