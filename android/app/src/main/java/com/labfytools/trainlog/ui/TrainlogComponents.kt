package com.labfytools.trainlog.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
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
import androidx.compose.material3.Button
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.DialogProperties
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors
import com.labfytools.trainlog.ui.theme.TrainlogTypography

@Composable
fun TrainlogScreen(
    subtitle: String,
    scrollKey: String = subtitle,
    content: @Composable ColumnScope.() -> Unit,
) {
    val colors =
        LocalTrainlogColors.current

    Column(
        modifier =
            Modifier
                .background(colors.background)
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
        BasicText(
            text = subtitle.lowercase().replaceFirstChar { it.titlecase() },
            modifier = Modifier.padding(bottom = 18.dp),
            style = TrainlogTypography.title.copy(color = colors.text),
        )

        content()
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
            colors.accent
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
            text = title,
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
                    .background(colors.surfaceAlt)
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
                .heightIn(min = 48.dp)
                .height(IntrinsicSize.Min)
                .background(Color.Transparent)
                .clickable(onClick = onClick)
    ) {
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

/** Compact pictogram action with a mandatory accessible name. */
@Composable
fun TrainlogIconAction(
    icon: ImageVector,
    contentDescription: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    accent: Color? = null,
) {
    val colors = LocalTrainlogColors.current
    IconButton(onClick = onClick, modifier = modifier.heightIn(min = 48.dp)) {
        Icon(icon, contentDescription = contentDescription,
            tint = accent ?: colors.accent)
    }
}

@Composable
fun TrainlogPrimaryAction(label: String, description: String, onClick: () -> Unit) {
    val colors = LocalTrainlogColors.current
    Column(Modifier.fillMaxWidth().padding(bottom = 12.dp)) {
        Button(onClick = onClick, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) {
            Text(label)
        }
        if (description.isNotBlank()) {
            BasicText(
                description,
                Modifier.padding(top = 4.dp),
                TrainlogTypography.small.copy(color = colors.muted),
            )
        }
    }
}

/** Compact horizontally scrollable choices; selection is textual and colored. */
@Composable
fun TrainlogChoiceChips(
    choices: List<Pair<String, String>>,
    selectedId: String,
    onSelected: (String) -> Unit,
) {
    Row(
        Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        choices.forEach { (id, label) ->
            FilterChip(
                selected = id == selectedId,
                onClick = { onSelected(id) },
                label = { Text(if (id == selectedId) "✓ $label" else label) },
                modifier = Modifier.heightIn(min = 48.dp),
            )
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
    testTag: String? = null,
    singleLine: Boolean = true,
    minLines: Int = 1,
    maxLines: Int = if (singleLine) 1 else Int.MAX_VALUE,
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
            singleLine = singleLine,
            minLines = minLines,
            maxLines = maxLines,
            keyboardOptions = keyboardOptions,
            cursorBrush =
                SolidColor(colors.accent),
            textStyle =
                (if (keyboardOptions.keyboardType == KeyboardType.Number ||
                    keyboardOptions.keyboardType == KeyboardType.Decimal) {
                    TrainlogTypography.numeric
                } else {
                    TrainlogTypography.normal
                }).copy(
                    color = colors.text,
                ),
            modifier =
                Modifier
                    .fillMaxWidth()
                    .heightIn(min = 48.dp)
                    .onFocusChanged {
                        focused = it.isFocused
                    }
                    .then(
                        if (testTag == null) {
                            Modifier
                        } else {
                            Modifier.testTag(testTag)
                        }
                    )
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

/** Shared destructive commit gate. CONTRACT: dismissal and Back are safe,
 * outside taps cannot confirm, and only the explicit labelled button invokes
 * the destructive callback. */
@Composable
fun DestructiveConfirmationDialog(
    title: String,
    detail: String,
    confirmLabel: String,
    onCancel: () -> Unit,
    onConfirm: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = onCancel,
        title = { Text(title) },
        text = { Text(detail) },
        dismissButton = { TextButton(onClick = onCancel) { Text("Annuler") } },
        confirmButton = { TextButton(onClick = onConfirm) { Text(confirmLabel) } },
        properties = DialogProperties(dismissOnBackPress = true, dismissOnClickOutside = false),
    )
}
