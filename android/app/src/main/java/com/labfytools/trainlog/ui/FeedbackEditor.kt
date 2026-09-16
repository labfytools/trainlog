/*
 * Android FeedbackEditor.
 *
 * Owns this Compose presentation boundary; durable state and domain rules remain in repository and model layers.
 */
package com.labfytools.trainlog.ui

import android.Manifest
import android.content.pm.PackageManager
import android.content.Context
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalContext
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.BasicText
import androidx.compose.foundation.verticalScroll
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import com.labfytools.trainlog.ui.theme.TrainlogTypography
import androidx.core.content.ContextCompat
import com.labfytools.trainlog.data.MAX_FEEDBACK_UTF8_BYTES
import com.labfytools.trainlog.ui.theme.LocalTrainlogColors
import com.labfytools.trainlog.R

@Composable
fun FeedbackEditor(
    onSave: (String) -> String?,
    onCancel: () -> Unit,
    initialText: String = "",
    editing: Boolean = false,
    recognizerFactory: ((Context) -> SpeechRecognitionAdapter)? = null,
) {
    val context = LocalContext.current
    val colors = LocalTrainlogColors.current
    val strings = localizedContext()
    val speechTag = if (LocalLanguagePresentation.current.language == AppLanguage.FRENCH) "fr-FR" else "en-US"
    fun message(value: String): String = when (value) {
        "feedback_microphone_denied" -> strings.getString(R.string.feedback_microphone_denied)
        "feedback_speech_unavailable" -> strings.getString(R.string.feedback_speech_unavailable)
        "feedback_empty" -> strings.getString(R.string.feedback_empty)
        else -> if (value.startsWith("speech_error:")) strings.getString(R.string.speech_interrupted,
            value.substringAfter(':').toIntOrNull() ?: -1) else value
    }
    var state by remember(initialText) { mutableStateOf(DictationState(committedText = initialText)) }
    val controller = remember(initialText) { FeedbackDictationController(
        recognizerFactory?.invoke(context) ?: AndroidSpeechRecognitionAdapter(context),
        onChanged = { state = it }).also {
            if (initialText.isNotEmpty()) it.edit(initialText)
        } }
    val permission = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
        controller.start(granted, speechTag)
    }
    DisposableEffect(controller) { onDispose { controller.destroy() } }
    val transcriptScroll = rememberScrollState()
    /* Scroll only when layout overflow grows. Partial tokens that still fit do
     * not move the viewport, so manual reading is not overridden continuously. */
    LaunchedEffect(transcriptScroll.maxValue, state.phase) {
        if (state.phase == DictationPhase.Listening &&
            transcriptScroll.maxValue > transcriptScroll.value) {
            transcriptScroll.scrollTo(transcriptScroll.maxValue)
        }
    }

    TrainlogFrame(title = strings.getString(R.string.feedback_dictation)) {
        if (state.phase == DictationPhase.Listening) {
            TrainlogInfo(strings.getString(R.string.feedback_recording), color = colors.error)
            TrainlogIconAction(TrainlogIcons.Stop, strings.getString(R.string.feedback_stop),
                onClick = { controller.stop() }, accent = colors.warning,
                modifier = Modifier.testTag("feedback-stop"))
            Box(Modifier.fillMaxWidth().heightIn(min = 96.dp, max = 192.dp)
                .background(colors.surface).verticalScroll(transcriptScroll)
                .padding(12.dp).testTag("feedback-live-transcript")) {
                BasicText(state.visibleText, style = TrainlogTypography.normal.copy(color = colors.text))
            }
        } else {
            TrainlogInputField(strings.getString(R.string.feedback_text), state.visibleText, onValueChange = {
                if (it.toByteArray(Charsets.UTF_8).size <= MAX_FEEDBACK_UTF8_BYTES) controller.edit(it)
            }, testTag = "feedback-edit-transcript", singleLine = false, minLines = 4, maxLines = 8)
            val resume = state.committedText.isNotBlank()
            TrainlogIconAction(TrainlogIcons.Mic,
                strings.getString(if (resume) R.string.feedback_resume else R.string.feedback_start), onClick = {
                if (ContextCompat.checkSelfPermission(context, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED)
                    controller.start(true, speechTag) else permission.launch(Manifest.permission.RECORD_AUDIO)
            }, accent = colors.accent, modifier = Modifier.testTag("feedback-mic"))
            TrainlogAction(strings.getString(if (editing) R.string.save_changes else R.string.save),
                strings.getString(if (editing) R.string.feedback_revision_description else R.string.feedback_create_description), onClick = {
                controller.beginSaving()?.let { text -> onSave(text)?.let(controller::saveFailed) }
            }, accent = colors.success)
            TrainlogAction(strings.getString(R.string.dialog_cancel), strings.getString(R.string.save_nothing),
                onClick = { controller.cancel(); onCancel() }, accent = colors.muted)
        }
        state.message?.let { TrainlogInfo(message(it), color = colors.error) }
    }
}
