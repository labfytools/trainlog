package com.labfytools.trainlog.ui

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.speech.RecognitionListener
import android.speech.RecognizerIntent
import android.speech.SpeechRecognizer

class AndroidSpeechRecognitionAdapter(context: Context) : SpeechRecognitionAdapter {
    private val appContext = context.applicationContext
    private var recognizer: SpeechRecognizer? = null
    override val available: Boolean get() = SpeechRecognizer.isRecognitionAvailable(appContext)

    override fun start(languageTag: String, listener: SpeechRecognitionAdapter.Listener) {
        val instance = recognizer ?: SpeechRecognizer.createSpeechRecognizer(appContext).also { recognizer = it }
        instance.setRecognitionListener(object : RecognitionListener {
            override fun onPartialResults(results: Bundle) = listener.onPartial(best(results))
            override fun onResults(results: Bundle) = listener.onFinal(best(results))
            override fun onError(error: Int) = listener.onError("Reconnaissance interrompue (code $error).")
            override fun onReadyForSpeech(params: Bundle?) = Unit
            override fun onBeginningOfSpeech() = Unit
            override fun onRmsChanged(rmsdB: Float) = Unit
            override fun onBufferReceived(buffer: ByteArray?) = Unit
            override fun onEndOfSpeech() = Unit
            override fun onEvent(eventType: Int, params: Bundle?) = Unit
        })
        val intent = buildTrainlogRecognitionIntent(languageTag,
            android.os.Build.VERSION.SDK_INT >= 31 && SpeechRecognizer.isOnDeviceRecognitionAvailable(appContext))
        instance.startListening(intent)
    }
    override fun stop() { recognizer?.stopListening() }
    override fun destroy() { recognizer?.destroy(); recognizer = null }
    private fun best(bundle: Bundle): String =
        bundle.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION)?.firstOrNull().orEmpty()
}

internal const val COMPLETE_SILENCE_MILLIS = 1500L
internal const val POSSIBLY_COMPLETE_SILENCE_MILLIS = 1100L

/** Best-effort recognizer hints only: engines may ignore silence extras.
 * CONTRACT: Trainlog does not synthesize results or automatically restart a
 * naturally completed recognizer session. The listener owns real engine text. */
internal fun buildTrainlogRecognitionIntent(languageTag: String, preferOffline: Boolean): Intent =
    Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH).apply {
        putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_FREE_FORM)
        putExtra(RecognizerIntent.EXTRA_LANGUAGE, languageTag)
        putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, true)
        putExtra(RecognizerIntent.EXTRA_SPEECH_INPUT_COMPLETE_SILENCE_LENGTH_MILLIS,
            COMPLETE_SILENCE_MILLIS)
        putExtra(RecognizerIntent.EXTRA_SPEECH_INPUT_POSSIBLY_COMPLETE_SILENCE_LENGTH_MILLIS,
            POSSIBLY_COMPLETE_SILENCE_MILLIS)
        if (preferOffline) putExtra(RecognizerIntent.EXTRA_PREFER_OFFLINE, true)
    }
