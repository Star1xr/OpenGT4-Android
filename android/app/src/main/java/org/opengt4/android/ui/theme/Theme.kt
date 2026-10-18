package org.opengt4.android.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

// The launcher is always dark: the game also starts with a dark interface.
private val OpenGt4Colors = darkColorScheme(
    primary = Color(0xFF3FA9F5),
    onPrimary = Color(0xFF062033),
    primaryContainer = Color(0xFF123A55),
    onPrimaryContainer = Color(0xFFCFE8FA),
    secondary = Color(0xFFE8ECEF),
    onSecondary = Color(0xFF14171B),
    background = Color(0xFF101216),
    onBackground = Color(0xFFE8ECEF),
    surface = Color(0xFF171A20),
    onSurface = Color(0xFFE8ECEF),
    surfaceVariant = Color(0xFF23272F),
    onSurfaceVariant = Color(0xFFA9B1BA),
    outline = Color(0xFF3A4049),
    error = Color(0xFFFF6B6B),
    onError = Color(0xFF2A0A0A),
)

@Composable
fun OpenGt4Theme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = OpenGt4Colors, content = content)
}
