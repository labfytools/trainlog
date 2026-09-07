package com.labfytools.trainlog.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.BasicText
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors
import com.labfytools.trainlog.ui.theme.TrainlogTypography

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
                        vertical = 12.dp,
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

    Column(
        modifier =
            Modifier
                .fillMaxWidth()
                .padding(bottom = 18.dp)
    ) {
        /* WHY: TUI and Android share this compact plaque rather than separate
         * brand treatments. The terminal box becomes flat spacing on touch. */
        BasicText(
            text = "◆ TRAINLOG ◆",
            style =
                TrainlogTypography.banner.copy(
                    color = colors.accent,
                    fontWeight = FontWeight.Bold,
                    fontSize = 21.sp,
                ),
        )

        BasicText(
            text = subtitle,
            modifier = Modifier.padding(top = 5.dp),
            style =
                TrainlogTypography.small.copy(
                    color = colors.muted,
                    fontWeight = FontWeight.Bold,
                ),
        )
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

    val accent =
        if (active) {
            colors.warning
        } else {
            colors.muted
        }

    Column(
        modifier =
            modifier
                .fillMaxWidth()
                .padding(bottom = 16.dp)
    ) {
        BasicText(
            text = title.uppercase(),
            style =
                TrainlogTypography.small.copy(
                    color = accent,
                    fontWeight =
                        FontWeight.Bold,
                ),
        )

        Box(
            modifier =
                Modifier
                    .fillMaxWidth()
                    .padding(
                        top = 6.dp,
                        bottom = 8.dp,
                    )
                    .height(1.dp)
                    .background(accent)
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

    Row(
        modifier =
            modifier
                .fillMaxWidth()
                .padding(vertical = 3.dp)
                .height(IntrinsicSize.Min)
                .background(colors.surface)
                .clickable(onClick = onClick)
    ) {
        Box(
            modifier =
                Modifier
                    .width(3.dp)
                    .fillMaxHeight()
                    .background(actualAccent)
        )

        Column(
            modifier =
                Modifier.padding(
                    horizontal = 12.dp,
                    vertical = 10.dp,
                )
        ) {
            BasicText(
                text = label,
                style =
                    TrainlogTypography.normal.copy(
                        color = actualAccent,
                        fontWeight =
                            FontWeight.Bold,
                    ),
            )

            if (description.isNotEmpty()) {
                BasicText(
                    text = description,
                    modifier =
                        Modifier.padding(top = 2.dp),
                    style =
                        TrainlogTypography.small.copy(
                            color = colors.text,
                        ),
                )
            }
        }
    }
}

@Composable
fun TrainlogInputField(
    label: String,
    value: String,
    onValueChange: (String) -> Unit,
    keyboardOptions: KeyboardOptions =
        KeyboardOptions.Default,
) {
    val colors =
        LocalTrainlogColors.current

    var focused by
        remember {
            mutableStateOf(false)
        }

    Column(
        modifier =
            Modifier.padding(bottom = 10.dp)
    ) {
        BasicText(
            text = label.uppercase(),
            modifier =
                Modifier.padding(bottom = 4.dp),
            style =
                TrainlogTypography.small.copy(
                    color =
                        if (focused) {
                            colors.accent
                        } else {
                            colors.muted
                        },
                    fontWeight =
                        if (focused) {
                            FontWeight.Bold
                        } else {
                            FontWeight.Normal
                        },
                ),
        )

        BasicTextField(
            value = value,
            onValueChange = onValueChange,
            singleLine = true,
            keyboardOptions = keyboardOptions,
            cursorBrush =
                SolidColor(colors.accent),
            textStyle =
                TrainlogTypography.normal.copy(
                    color = colors.text,
                ),
            modifier =
                Modifier
                    .fillMaxWidth()
                    .onFocusChanged {
                        focused = it.isFocused
                    }
                    .background(colors.surface)
                    .padding(
                        horizontal = 12.dp,
                        vertical = 10.dp,
                    ),
        )

        Box(
            modifier =
                Modifier
                    .fillMaxWidth()
                    .height(
                        if (focused) {
                            2.dp
                        } else {
                            1.dp
                        }
                    )
                    .background(
                        if (focused) {
                            colors.accent
                        } else {
                            colors.surfaceAlt
                        }
                    )
        )
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
            Modifier.padding(vertical = 3.dp),
        style =
            TrainlogTypography.normal.copy(
                color =
                    color ?: colors.text,
            ),
    )
}
