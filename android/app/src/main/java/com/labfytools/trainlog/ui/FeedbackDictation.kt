/*
 * Android FeedbackDictation.
 *
 * Owns this Compose presentation boundary; durable state and domain rules remain in repository and model layers.
 */
package com.labfytools.trainlog.ui

/** Explicit, platform-independent dictation state used by Compose and tests. */
enum class DictationPhase { Idle, Listening, Editing, Saving, Error }

data class DictationState(
    val phase: DictationPhase = DictationPhase.Idle,
    val committedText: String = "",
    val partialText: String = "",
    val message: String? = null,
) {
    val visibleText: String get() = appendSegment(committedText, partialText)
}

/**
 * CONTRACT: recognition events transport text only. Implementations must not
 * record, cache, or persist microphone audio. destroy() releases callbacks and
 * the platform recognizer; it is safe to call more than once.
 */
interface SpeechRecognitionAdapter {
    val available: Boolean
    fun start(languageTag: String, listener: Listener)
    fun stop()
    fun destroy()
    interface Listener {
        fun onPartial(text: String)
        fun onFinal(text: String)
        fun onError(message: String)
    }
}

class FeedbackDictationController(
    private val recognizer: SpeechRecognitionAdapter,
    private val onChanged: (DictationState) -> Unit = {},
) : SpeechRecognitionAdapter.Listener {
    var state = DictationState()
        private set

    private fun update(value: DictationState) { state = value; onChanged(value) }
    fun edit(text: String) { if (state.phase != DictationPhase.Listening) update(state.copy(
        phase = DictationPhase.Editing, committedText = text, partialText = "", message = null)) }

    fun start(permissionGranted: Boolean, languageTag: String = "fr-FR") {
        if (!permissionGranted) {
            update(state.copy(phase = DictationPhase.Error, partialText = "",
                message = "feedback_microphone_denied"))
        } else if (!recognizer.available) {
            update(state.copy(phase = DictationPhase.Error, partialText = "",
                message = "feedback_speech_unavailable"))
        } else {
            update(state.copy(phase = DictationPhase.Listening, partialText = "", message = null))
            recognizer.start(languageTag, this)
        }
    }

    fun stop() {
        if (state.phase == DictationPhase.Listening) {
            recognizer.stop()
            update(state.copy(phase = DictationPhase.Editing, committedText = state.visibleText,
                partialText = ""))
        }
    }

    override fun onPartial(text: String) { if (state.phase == DictationPhase.Listening)
        update(state.copy(partialText = text)) }
    override fun onFinal(text: String) { if (state.phase == DictationPhase.Listening)
        update(state.copy(phase = DictationPhase.Editing,
            committedText = appendSegment(state.committedText, text), partialText = "")) }
    override fun onError(message: String) {
        update(state.copy(phase = DictationPhase.Error, committedText = state.visibleText,
            partialText = "", message = message))
    }
    fun beginSaving(): String? {
        val text = state.visibleText.trim()
        if (text.isBlank()) { update(state.copy(phase = DictationPhase.Error,
            message = "feedback_empty")); return null }
        update(state.copy(phase = DictationPhase.Saving, committedText = text, partialText = ""))
        return text
    }
    fun saveFailed(message: String) { update(state.copy(phase = DictationPhase.Error, message = message)) }
    fun cancel() { recognizer.stop(); update(DictationState()) }
    fun destroy() { recognizer.destroy() }
}

internal fun appendSegment(base: String, segment: String): String = when {
    base.isBlank() -> segment
    segment.isBlank() -> base
    else -> base.trimEnd() + " " + segment.trimStart()
}
