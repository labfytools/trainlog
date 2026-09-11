package com.labfytools.trainlog.ui.theme

import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.sp

data class TrainlogColors(
    val background: Color,
    val surface: Color,
    val surfaceAlt: Color,
    val text: Color,
    val accent: Color,
    val success: Color,
    val warning: Color,
    val error: Color,
    val muted: Color,
    val graph: Color,
    val crust: Color,
    val mantle: Color,
    val lavender: Color,
    val info: Color,
    val notice: Color,
)

private val TrainlogDarkColors =
    TrainlogColors(
        background = Color(0xFF1E1E2E),
        surface = Color(0xFF313244),
        surfaceAlt = Color(0xFF45475A),
        text = Color(0xFFCDD6F4),
        accent = Color(0xFFB4BEFE),
        success = Color(0xFFA6E3A1),
        warning = Color(0xFFF9E2AF),
        error = Color(0xFFF38BA8),
        muted = Color(0xFFBAC2DE),
        graph = Color(0xFFF5C2E7),
        crust = Color(0xFF11111B),
        mantle = Color(0xFF181825),
        lavender = Color(0xFFB4BEFE),
        info = Color(0xFF89B4FA),
        notice = Color(0xFFFAB387),
    )

val LocalTrainlogColors =
    staticCompositionLocalOf {
        TrainlogDarkColors
    }

object TrainlogTypography {
    val normal =
        TextStyle(
            fontFamily = FontFamily.Default,
            fontSize = 16.sp,
            lineHeight = 24.sp,
        )

    val small =
        TextStyle(
            fontFamily = FontFamily.Default,
            fontSize = 14.sp,
            lineHeight = 20.sp,
        )

    val title =
        TextStyle(
            fontFamily = FontFamily.Default,
            fontSize = 22.sp,
            lineHeight = 28.sp,
            fontWeight = FontWeight.SemiBold,
        )

    val banner =
        TextStyle(
            fontFamily = FontFamily.Default,
            fontSize = 18.sp,
            lineHeight = 24.sp,
            fontWeight = FontWeight.SemiBold,
        )

    val section = TextStyle(fontFamily = FontFamily.Default, fontSize = 16.sp, lineHeight = 22.sp, fontWeight = FontWeight.SemiBold)
    val value = TextStyle(fontFamily = FontFamily.Default, fontSize = 24.sp, lineHeight = 30.sp, fontWeight = FontWeight.Medium, fontFeatureSettings = "tnum")
    val numeric = TextStyle(fontFamily = FontFamily.Monospace, fontSize = 16.sp, lineHeight = 24.sp)
}

@Composable
fun TrainlogTheme(
    content: @Composable () -> Unit
) {
    val scheme = darkColorScheme(
        primary = TrainlogDarkColors.lavender,
        onPrimary = TrainlogDarkColors.crust,
        background = TrainlogDarkColors.background,
        onBackground = TrainlogDarkColors.text,
        surface = TrainlogDarkColors.surface,
        onSurface = TrainlogDarkColors.text,
        surfaceVariant = TrainlogDarkColors.surfaceAlt,
        onSurfaceVariant = TrainlogDarkColors.muted,
        error = TrainlogDarkColors.error,
    )
    CompositionLocalProvider(LocalTrainlogColors provides TrainlogDarkColors) {
        MaterialTheme(colorScheme = scheme, content = content)
    }
}
