/*
 * Android MainActivity.
 *
 * Connects the application lifecycle to Trainlog UI without owning domain or persistence behavior.
 */
package com.labfytools.trainlog

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.viewModels
import androidx.compose.runtime.CompositionLocalProvider
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.data.SyncExporter
import com.labfytools.trainlog.data.SyncCatalogInbox
import com.labfytools.trainlog.data.SyncRequestOutbox
import com.labfytools.trainlog.ui.TrainlogApp
import com.labfytools.trainlog.ui.TrainlogAppState
import com.labfytools.trainlog.ui.LanguagePresentation
import com.labfytools.trainlog.ui.LanguageSettingsOwner
import com.labfytools.trainlog.ui.LocalLanguagePresentation
import com.labfytools.trainlog.ui.theme.TrainlogTheme

class MainActivity : ComponentActivity() {
    private val appState by viewModels<TrainlogAppState>()
    private lateinit var languageSettings: LanguageSettingsOwner
    override fun onCreate(
        savedInstanceState: Bundle?
    ) {
        super.onCreate(savedInstanceState)
        languageSettings = LanguageSettingsOwner(applicationContext)

        val repository =
            TrainlogRepository(
                applicationContext
            )

        val exporter =
            SyncExporter(
                applicationContext,
                repository,
            )

        val inbox =
            SyncCatalogInbox(
                applicationContext,
                repository,
            )

        val requestOutbox =
            SyncRequestOutbox(
                applicationContext
            )

        setContent {
            /* CONTRACT: reading this observable property makes a language
             * selection recompose the existing activity immediately. */
            val language = languageSettings.language
            CompositionLocalProvider(
                LocalLanguagePresentation provides LanguagePresentation(language, languageSettings::select),
            ) {
                TrainlogTheme {
                    TrainlogApp(
                    repository = repository,
                    exporter = exporter,
                    inbox = inbox,
                    requestOutbox =
                        requestOutbox,
                    appState = appState,
                    )
                }
            }
        }
    }
}
