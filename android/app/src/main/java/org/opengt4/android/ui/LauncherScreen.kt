package org.opengt4.android.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import org.opengt4.android.ConnectedDevice
import org.opengt4.android.DeviceKind
import org.opengt4.android.InputDevices
import org.opengt4.android.R
import org.opengt4.android.TouchMode
import java.util.Locale

/**
 * Launcher: select a disc, check it, play. The check is the game's own
 * validation code, so the message is the disc description or the reason it was
 * rejected. Video/audio/pad settings (phase 5) reuse the same INI files as the
 * PC launcher (src/app/launcher_config.c).
 */
@Composable
fun LauncherScreen(
    discName: String,
    discBytes: Long,
    ready: Boolean,
    busy: Boolean,
    validated: Boolean?,
    validationText: String?,
    errorText: String?,
    banner: String,
    logAvailable: Boolean,
    deviceList: List<ConnectedDevice>,
    touchMode: TouchMode,
    onSelect: () -> Unit,
    onPlay: () -> Unit,
    onShareLog: () -> Unit,
    onTouchMode: (TouchMode) -> Unit,
) {
    Scaffold { inner ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(inner)
                .verticalScroll(rememberScrollState())
                .padding(24.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            Text(
                text = "OpenGT4",
                style = MaterialTheme.typography.displaySmall,
                fontWeight = FontWeight.Bold,
            )
            Text(
                text = stringResource(R.string.app_subtitle),
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )

            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(
                    containerColor = MaterialTheme.colorScheme.surfaceVariant
                ),
            ) {
                Column(
                    modifier = Modifier.fillMaxWidth().padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(6.dp),
                ) {
                    Text(
                        text = stringResource(R.string.disc_section),
                        style = MaterialTheme.typography.labelLarge,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    Text(
                        text = if (discName.isEmpty()) stringResource(R.string.disc_none) else discName,
                        style = MaterialTheme.typography.titleMedium,
                    )
                    if (discBytes > 0) {
                        Text(
                            text = formatBytes(discBytes),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    if (validationText != null) {
                        Text(
                            text = validationText,
                            style = MaterialTheme.typography.bodySmall,
                            color = if (validated == true) {
                                MaterialTheme.colorScheme.onSurfaceVariant
                            } else {
                                MaterialTheme.colorScheme.error
                            },
                        )
                    }
                }
            }

            Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                Button(onClick = onSelect, enabled = !busy) {
                    Text(stringResource(R.string.disc_select))
                }
                if (ready) {
                    OutlinedButton(onClick = onSelect, enabled = !busy) {
                        Text(stringResource(R.string.disc_change))
                    }
                }
            }

            Button(
                onClick = onPlay,
                enabled = ready && !busy && validated != false,
                modifier = Modifier.fillMaxWidth().height(56.dp),
            ) {
                if (busy) {
                    CircularProgressIndicator(modifier = Modifier.width(20.dp).height(20.dp))
                    Spacer(Modifier.width(12.dp))
                }
                Text(stringResource(R.string.disc_play))
            }

            // The on-screen pad is the only pad until a device takes over, so
            // the devices the system reports and the player's own choice are
            // both here.
            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(
                    containerColor = MaterialTheme.colorScheme.surfaceVariant
                ),
            ) {
                Column(
                    modifier = Modifier.fillMaxWidth().padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(6.dp),
                ) {
                    Text(
                        text = stringResource(R.string.controls_section),
                        style = MaterialTheme.typography.labelLarge,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    if (deviceList.isEmpty()) {
                        Text(
                            text = stringResource(R.string.devices_none),
                            style = MaterialTheme.typography.bodyMedium,
                        )
                    } else {
                        deviceList.forEach { device ->
                            Text(
                                text = deviceLabel(device),
                                style = MaterialTheme.typography.bodyMedium,
                            )
                        }
                    }
                    Spacer(Modifier.height(6.dp))
                    Text(
                        text = stringResource(R.string.touch_section),
                        style = MaterialTheme.typography.bodyMedium,
                    )
                    Row(
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        for (mode in TouchMode.values()) {
                            if (mode == touchMode) {
                                Button(onClick = { onTouchMode(mode) }) {
                                    Text(touchModeLabel(mode))
                                }
                            } else {
                                OutlinedButton(onClick = { onTouchMode(mode) }) {
                                    Text(touchModeLabel(mode))
                                }
                            }
                        }
                    }
                    Text(
                        text = stringResource(
                            if (padUsed(deviceList, touchMode)) R.string.pad_active
                            else R.string.pad_hidden
                        ),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }

            // The game's log from the last run, when there is one.
            if (logAvailable) {
                OutlinedButton(onClick = onShareLog, modifier = Modifier.fillMaxWidth()) {
                    Text(stringResource(R.string.share_log))
                }
            }

            if (errorText != null) {
                Text(
                    text = errorText,
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.error,
                )
            }

            Spacer(Modifier.height(8.dp))
            Text(
                text = stringResource(R.string.notice),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            if (banner.isNotEmpty()) {
                Text(
                    text = banner,
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            Spacer(Modifier.height(16.dp))
            Text(
                text = "GPLv3, based on OpenGT4",
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.align(Alignment.CenterHorizontally),
            )
        }
    }
}

private fun padUsed(devices: List<ConnectedDevice>, mode: TouchMode): Boolean = when (mode) {
    TouchMode.ON -> true
    TouchMode.OFF -> false
    TouchMode.AUTO -> InputDevices.flags(devices) == 0
}

@Composable
private fun deviceLabel(device: ConnectedDevice): String {
    val kind = stringResource(
        when (device.kind) {
            DeviceKind.GAMEPAD -> R.string.device_gamepad
            DeviceKind.KEYBOARD -> R.string.device_keyboard
            else -> R.string.device_mouse
        }
    )
    return "$kind: ${device.name}"
}

@Composable
private fun touchModeLabel(mode: TouchMode): String = stringResource(
    when (mode) {
        TouchMode.AUTO -> R.string.touch_auto
        TouchMode.ON -> R.string.touch_on
        TouchMode.OFF -> R.string.touch_off
    }
)

private fun formatBytes(bytes: Long): String {
    val gb = bytes.toDouble() / (1024.0 * 1024.0 * 1024.0)
    return if (gb >= 1.0) {
        String.format(Locale.US, "%.2f GB", gb)
    } else {
        String.format(Locale.US, "%.0f MB", bytes.toDouble() / (1024.0 * 1024.0))
    }
}
