package com.labfytools.trainlog.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.text.BasicText
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors
import com.labfytools.trainlog.ui.theme.TrainlogTypography

private val FullAsciiBanner =
    """
TTTTT RRRR   AAA  IIIII N   N L       OOO   GGG
  T   R   R A   A   I   NN  N L      O   O G
  T   RRRR  AAAAA   I   N N N L      O   O G  GG
  T   R  R  A   A   I   N  NN L      O   O G   G
  T   R   R A   A IIIII N   N LLLLL   OOO   GGG
""".trimIndent()

@Composable
fun TrainlogScreen(
    subtitle: String,
    content: @Composable ColumnScope.() -> Unit,
) {
    val colors =
        LocalTrainlogColors.current

    Column(
        modifier =
            Modifier
                .background(colors.background)
                .statusBarsPadding()
                .navigationBarsPadding()
                .imePadding()
                .verticalScroll(
                    rememberScrollState()
                )
                .padding(
                    PaddingValues(
                        horizontal = 16.dp,
                        vertical = 18.dp,
                    )
                ),
    ) {
        TrainlogBanner(
            subtitle = subtitle
        )

        content()
    }
}

@Composable
private fun TrainlogBanner(
    subtitle: String
) {
    val colors =
        LocalTrainlogColors.current

    BoxWithConstraints(
        modifier =
            Modifier
                .fillMaxWidth()
                .padding(
                    bottom = 18.dp
                )
    ) {
        val wide =
            maxWidth >= 560.dp

        Column(
            modifier =
                Modifier.fillMaxWidth(),
            horizontalAlignment =
                Alignment.CenterHorizontally,
        ) {
            BasicText(
                text =
                    if (wide) {
                        FullAsciiBanner
                    } else {
                        "T R A I N L O G"
                    },
                style =
                    TrainlogTypography.banner.copy(
                        color = colors.accent,
                        fontWeight = FontWeight.Bold,
                        fontSize =
                            if (wide) {
                                16.sp
                            } else {
                                24.sp
                            },
                    ),
            )

            BasicText(
                text = ":: $subtitle ::",
                modifier =
                    Modifier.padding(
                        top = 6.dp
                    ),
                style =
                    TrainlogTypography.normal.copy(
                        color = colors.muted,
                    ),
            )
        }
    }
}

@Composable
fun TrainlogFrame(
    title: String,
    modifier: Modifier = Modifier,
    active: Boolean = true,
    content: @Composable ColumnScope.() -> Unit,
) {
    val colors =
        LocalTrainlogColors.current

    val borderColor =
        if (active) {
            colors.warning
        } else {
            colors.muted
        }

    Column(
        modifier =
            modifier
                .fillMaxWidth()
                .padding(
                    bottom = 14.dp
                )
                .border(
                    width = 1.dp,
                    color = borderColor,
                )
                .padding(
                    horizontal = 14.dp,
                    vertical = 12.dp,
                )
    ) {
        BasicText(
            text = title.uppercase(),
            modifier =
                Modifier.padding(
                    bottom = 12.dp
                ),
            style =
                TrainlogTypography.normal.copy(
                    color = borderColor,
                    fontWeight = FontWeight.Bold,
                ),
        )

        content()
    }
}

@Composable
fun TrainlogAction(
    label: String,
    description: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    accent: Color? = null,
) {
    val colors =
        LocalTrainlogColors.current

    val actualAccent =
        accent ?: colors.accent

    Box(
        modifier =
            modifier
                .fillMaxWidth()
                .padding(
                    vertical = 5.dp
                )
                .border(
                    width = 1.dp,
                    color = colors.surfaceAlt,
                )
                .background(
                    colors.surface
                )
                .clickable(
                    onClick = onClick
                )
                .padding(
                    horizontal = 14.dp,
                    vertical = 14.dp,
                )
    ) {
        Column {
            BasicText(
                text = label,
                style =
                    TrainlogTypography.normal.copy(
                        color = actualAccent,
                        fontWeight = FontWeight.Bold,
                    ),
            )

            BasicText(
                text = description,
                modifier =
                    Modifier.padding(
                        top = 4.dp
                    ),
                style =
                    TrainlogTypography.small.copy(
                        color = colors.text,
                    ),
            )
        }
    }
}

@Composable
fun TrainlogInfo(
    text: String,
    color: Color? = null,
) {
    val colors =
        LocalTrainlogColors.current

    BasicText(
        text = text,
        modifier =
            Modifier.padding(
                vertical = 4.dp
            ),
        style =
            TrainlogTypography.normal.copy(
                color =
                    color ?: colors.text,
            ),
    )
}
