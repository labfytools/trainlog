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
    var state by remember(initialText) { mutableStateOf(DictationState(committedText = initialText)) }
    val controller = remember(initialText) { FeedbackDictationController(
        recognizerFactory?.invoke(context) ?: AndroidSpeechRecognitionAdapter(context),
        onChanged = { state = it }).also {
            if (initialText.isNotEmpty()) it.edit(initialText)
        } }
    val permission = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
        controller.start(granted)
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

    TrainlogFrame(title = "Dictée du ressenti") {
        if (state.phase == DictationPhase.Listening) {
            TrainlogInfo("● Enregistrement en cours", color = colors.error)
            TrainlogIconAction(TrainlogIcons.Stop, "Arrêter la dictée",
                onClick = { controller.stop() }, accent = colors.warning,
                modifier = Modifier.testTag("feedback-stop"))
            Box(Modifier.fillMaxWidth().heightIn(min = 96.dp, max = 192.dp)
                .background(colors.surface).verticalScroll(transcriptScroll)
                .padding(12.dp).testTag("feedback-live-transcript")) {
                BasicText(state.visibleText, style = TrainlogTypography.normal.copy(color = colors.text))
            }
        } else {
            TrainlogInputField("Texte", state.visibleText, onValueChange = {
                if (it.toByteArray(Charsets.UTF_8).size <= MAX_FEEDBACK_UTF8_BYTES) controller.edit(it)
            }, testTag = "feedback-edit-transcript", singleLine = false, minLines = 4, maxLines = 8)
            val resume = state.committedText.isNotBlank()
            TrainlogIconAction(TrainlogIcons.Mic,
                if (resume) "Reprendre la dictée" else "Démarrer la dictée", onClick = {
                if (ContextCompat.checkSelfPermission(context, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED)
                    controller.start(true) else permission.launch(Manifest.permission.RECORD_AUDIO)
            }, accent = colors.accent, modifier = Modifier.testTag("feedback-mic"))
            TrainlogAction(if (editing) "Enregistrer les modifications" else "Enregistrer",
                if (editing) "Créer une nouvelle révision immutable." else "Créer un enregistrement historique immutable.", onClick = {
                controller.beginSaving()?.let { text -> onSave(text)?.let(controller::saveFailed) }
            }, accent = colors.success)
            TrainlogAction("Annuler", "Ne rien enregistrer.",
                onClick = { controller.cancel(); onCancel() }, accent = colors.muted)
        }
        state.message?.let { TrainlogInfo(it, color = colors.error) }
    }
}
