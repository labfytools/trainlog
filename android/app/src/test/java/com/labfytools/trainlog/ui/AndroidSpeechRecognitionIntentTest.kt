package com.labfytools.trainlog.ui

import android.speech.RecognizerIntent
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class AndroidSpeechRecognitionIntentTest {
    @Test fun `intent carries bounded best effort silence hints`() {
        val intent=buildTrainlogRecognitionIntent("fr-FR",false)
        assertEquals(RecognizerIntent.ACTION_RECOGNIZE_SPEECH,intent.action)
        assertEquals(1500L,intent.getLongExtra(
            RecognizerIntent.EXTRA_SPEECH_INPUT_COMPLETE_SILENCE_LENGTH_MILLIS,-1L))
        assertEquals(1100L,intent.getLongExtra(
            RecognizerIntent.EXTRA_SPEECH_INPUT_POSSIBLY_COMPLETE_SILENCE_LENGTH_MILLIS,-1L))
        assertFalse(intent.hasExtra(RecognizerIntent.EXTRA_PREFER_OFFLINE))
        assertTrue(intent.getBooleanExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS,false))
    }
}
