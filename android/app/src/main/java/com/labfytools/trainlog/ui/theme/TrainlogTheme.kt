package com.labfytools.trainlog.ui.theme

import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
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
)

private val TrainlogDarkColors =
    TrainlogColors(
        background = Color(0xFF1E1E2E),
        surface = Color(0xFF181825),
        surfaceAlt = Color(0xFF313244),
        text = Color(0xFFCDD6F4),
        accent = Color(0xFF94E2D5),
        success = Color(0xFFA6E3A1),
        warning = Color(0xFFF9E2AF),
        error = Color(0xFFF38BA8),
        muted = Color(0xFF89B4FA),
        graph = Color(0xFFF5C2E7),
    )

val LocalTrainlogColors =
    staticCompositionLocalOf {
        TrainlogDarkColors
    }

object TrainlogTypography {
    val normal =
        TextStyle(
            fontFamily = FontFamily.Monospace,
            fontSize = 16.sp,
        )

    val small =
        TextStyle(
            fontFamily = FontFamily.Monospace,
            fontSize = 13.sp,
        )

    val title =
        TextStyle(
            fontFamily = FontFamily.Monospace,
            fontSize = 20.sp,
        )

    val banner =
        TextStyle(
            fontFamily = FontFamily.Monospace,
            fontSize = 24.sp,
        )
}

@Composable
fun TrainlogTheme(
    content: @Composable () -> Unit
) {
    CompositionLocalProvider(
        LocalTrainlogColors provides
            TrainlogDarkColors,
        content = content,
    )
}
