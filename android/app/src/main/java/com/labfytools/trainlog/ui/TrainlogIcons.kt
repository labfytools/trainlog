/*
 * Android TrainlogIcons.
 *
 * Owns this Compose presentation boundary; durable state and domain rules remain in repository and model layers.
 */
package com.labfytools.trainlog.ui

import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.graphics.vector.path
import androidx.compose.ui.unit.dp

/** Local UI glyphs avoid shipping the large extended Material icon catalog. */
internal object TrainlogIcons {
    val Refresh: ImageVector by lazy {
        ImageVector.Builder("Refresh", 24.dp, 24.dp, 24f, 24f).apply {
            path(fill = SolidColor(Color.Black)) {
                moveTo(17.65f, 6.35f); curveTo(16.2f, 4.9f, 14.21f, 4f, 12f, 4f)
                curveTo(7.58f, 4f, 4.01f, 7.58f, 4.01f, 12f)
                reflectiveCurveTo(7.58f, 20f, 12f, 20f)
                curveTo(15.73f, 20f, 18.84f, 17.45f, 19.73f, 14f)
                horizontalLineTo(17.65f); curveTo(16.83f, 16.33f, 14.61f, 18f, 12f, 18f)
                curveTo(8.69f, 18f, 6f, 15.31f, 6f, 12f); reflectiveCurveTo(8.69f, 6f, 12f, 6f)
                curveTo(13.66f, 6f, 15.14f, 6.69f, 16.22f, 7.78f)
                lineTo(13f, 11f); horizontalLineTo(20f); verticalLineTo(4f); close()
            }
        }.build()
    }

    val BodyMeasurements: ImageVector by lazy {
        ImageVector.Builder("BodyMeasurements", 24.dp, 24.dp, 24f, 24f).apply {
            path(fill = SolidColor(Color.Black)) {
                moveTo(7f, 3f); horizontalLineTo(17f); curveTo(19.21f, 3f, 21f, 4.79f, 21f, 7f)
                verticalLineTo(17f); curveTo(21f, 19.21f, 19.21f, 21f, 17f, 21f)
                horizontalLineTo(7f); curveTo(4.79f, 21f, 3f, 19.21f, 3f, 17f)
                verticalLineTo(7f); curveTo(3f, 4.79f, 4.79f, 3f, 7f, 3f); close()
                moveTo(12f, 6f); curveTo(9.79f, 6f, 8f, 7.79f, 8f, 10f)
                horizontalLineTo(10f); curveTo(10f, 8.9f, 10.9f, 8f, 12f, 8f)
                reflectiveCurveTo(14f, 8.9f, 14f, 10f); horizontalLineTo(16f)
                curveTo(16f, 7.79f, 14.21f, 6f, 12f, 6f); close()
            }
        }.build()
    }

    val Edit: ImageVector by lazy {
        ImageVector.Builder("Edit", 24.dp, 24.dp, 24f, 24f).apply {
            path(fill = SolidColor(Color.Black)) {
                moveTo(3f, 17.25f); verticalLineTo(21f); horizontalLineTo(6.75f)
                lineTo(17.81f, 9.94f); lineTo(14.06f, 6.19f); close()
                moveTo(20.71f, 7.04f); curveTo(21.1f, 6.65f, 21.1f, 6.02f, 20.71f, 5.63f)
                lineTo(18.37f, 3.29f); curveTo(17.98f, 2.9f, 17.35f, 2.9f, 16.96f, 3.29f)
                lineTo(15.13f, 5.12f); lineTo(18.88f, 8.87f); close()
            }
        }.build()
    }

    val Drafts: ImageVector by lazy {
        ImageVector.Builder("Drafts", 24.dp, 24.dp, 24f, 24f).apply {
            path(fill = SolidColor(Color.Black)) {
                moveTo(14f, 2f); horizontalLineTo(6f); curveTo(4.9f, 2f, 4f, 2.9f, 4f, 4f)
                verticalLineTo(20f); curveTo(4f, 21.1f, 4.9f, 22f, 6f, 22f)
                horizontalLineTo(18f); curveTo(19.1f, 22f, 20f, 21.1f, 20f, 20f)
                verticalLineTo(8f); close(); moveTo(13f, 9f); verticalLineTo(3.5f)
                lineTo(18.5f, 9f); close()
            }
        }.build()
    }

    val Mic: ImageVector by lazy {
        ImageVector.Builder(
            name = "Mic",
            defaultWidth = 24.dp,
            defaultHeight = 24.dp,
            viewportWidth = 24f,
            viewportHeight = 24f,
        ).apply {
            path(fill = SolidColor(Color.Black)) {
                moveTo(12f, 14f)
                curveTo(13.66f, 14f, 14.99f, 12.66f, 14.99f, 11f)
                lineTo(15f, 5f)
                curveTo(15f, 3.34f, 13.66f, 2f, 12f, 2f)
                reflectiveCurveTo(9f, 3.34f, 9f, 5f)
                verticalLineToRelative(6f)
                curveTo(9f, 12.66f, 10.34f, 14f, 12f, 14f)
                close()
                moveTo(17.3f, 11f)
                curveTo(17.3f, 14f, 14.76f, 16.1f, 12f, 16.1f)
                reflectiveCurveTo(6.7f, 14f, 6.7f, 11f)
                horizontalLineTo(5f)
                curveTo(5f, 14.41f, 7.72f, 17.23f, 11f, 17.72f)
                verticalLineTo(21f)
                horizontalLineTo(8f)
                verticalLineTo(23f)
                horizontalLineTo(16f)
                verticalLineTo(21f)
                horizontalLineTo(13f)
                verticalLineToRelative(-3.28f)
                curveTo(16.28f, 17.24f, 19f, 14.42f, 19f, 11f)
                horizontalLineToRelative(-1.7f)
                close()
            }
        }.build()
    }

    val Stop: ImageVector by lazy {
        ImageVector.Builder(
            name = "Stop",
            defaultWidth = 24.dp,
            defaultHeight = 24.dp,
            viewportWidth = 24f,
            viewportHeight = 24f,
        ).apply {
            path(fill = SolidColor(Color.Black)) {
                moveTo(6f, 6f)
                horizontalLineTo(18f)
                verticalLineTo(18f)
                horizontalLineTo(6f)
                close()
            }
        }.build()
    }

    val DeleteOutline: ImageVector by lazy {
        ImageVector.Builder(
            name = "DeleteOutline",
            defaultWidth = 24.dp,
            defaultHeight = 24.dp,
            viewportWidth = 24f,
            viewportHeight = 24f,
        ).apply {
            path(fill = SolidColor(Color.Black)) {
                moveTo(6f, 19f)
                curveTo(6f, 20.1f, 6.9f, 21f, 8f, 21f)
                horizontalLineTo(16f)
                curveTo(17.1f, 21f, 18f, 20.1f, 18f, 19f)
                verticalLineTo(7f)
                horizontalLineTo(6f)
                verticalLineTo(19f)
                close()
                moveTo(8f, 9f)
                horizontalLineTo(16f)
                verticalLineTo(19f)
                horizontalLineTo(8f)
                verticalLineTo(9f)
                close()
                moveTo(15.5f, 4f)
                lineTo(14.5f, 3f)
                horizontalLineTo(9.5f)
                lineTo(8.5f, 4f)
                horizontalLineTo(5f)
                verticalLineTo(6f)
                horizontalLineTo(19f)
                verticalLineTo(4f)
                horizontalLineTo(15.5f)
                close()
            }
        }.build()
    }
}
