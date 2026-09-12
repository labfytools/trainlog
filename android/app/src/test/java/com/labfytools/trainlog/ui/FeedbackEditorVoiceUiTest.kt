package com.labfytools.trainlog.ui

import android.Manifest
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.assertTextContains
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onAllNodesWithContentDescription
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import com.labfytools.trainlog.ui.theme.TrainlogTheme
import org.junit.Rule
import org.junit.Test
import org.junit.Assert.assertTrue
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.Shadows
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk=[35])
class FeedbackEditorVoiceUiTest {
    @Suppress("DEPRECATION") @get:Rule val compose=createComposeRule()
    private class Fake:SpeechRecognitionAdapter {
        override val available=true
        lateinit var listener:SpeechRecognitionAdapter.Listener
        var stopped=false
        override fun start(languageTag:String,listener:SpeechRecognitionAdapter.Listener){this.listener=listener}
        override fun stop(){stopped=true}
        override fun destroy()=Unit
    }
    @Test fun micStopTranscriptAndResumeExposeAccessibleState() {
        Shadows.shadowOf(RuntimeEnvironment.getApplication()).grantPermissions(Manifest.permission.RECORD_AUDIO)
        val fake=Fake()
        compose.setContent { TrainlogTheme { FeedbackEditor({null},{},recognizerFactory={fake}) } }
        compose.onNodeWithContentDescription("Démarrer la dictée").assertIsDisplayed()
        assertTrue(compose.onAllNodesWithContentDescription("Arrêter la dictée")
            .fetchSemanticsNodes().isEmpty())
        compose.onNodeWithContentDescription("Démarrer la dictée").performClick()
        compose.onNodeWithContentDescription("Arrêter la dictée").assertIsDisplayed()
        val longText="première ligne dictée avec assez de mots pour envelopper\ndeuxième ligne conservée"
        compose.runOnIdle{fake.listener.onPartial(longText)}
        compose.onNodeWithText(longText).assertIsDisplayed()
        compose.onNodeWithContentDescription("Arrêter la dictée").performClick()
        compose.runOnIdle{assertTrue(fake.stopped)}
        compose.onNodeWithTag("feedback-edit-transcript").assertTextContains(longText)
        compose.onNodeWithContentDescription("Reprendre la dictée").assertIsDisplayed()
    }
}
