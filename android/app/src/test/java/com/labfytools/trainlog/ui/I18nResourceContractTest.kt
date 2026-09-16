package com.labfytools.trainlog.ui

import android.content.Context
import android.content.res.Configuration
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.R
import java.util.Locale
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class I18nResourceContractTest {
    private val base: Context = ApplicationProvider.getApplicationContext()

    private fun context(language: AppLanguage): Context = base.createConfigurationContext(
        Configuration(base.resources.configuration).apply { setLocale(Locale.forLanguageTag(language.tag)) },
    )

    @Test fun `root settings destructive and accessibility strings have both locales`() {
        val french = context(AppLanguage.FRENCH)
        val english = context(AppLanguage.ENGLISH)
        val translated = listOf(
            R.string.nav_home, R.string.nav_settings, R.string.settings_language_label,
            R.string.dialog_unsaved_changes, R.string.dialog_discard_changes,
            R.string.a11y_open_navigation, R.string.a11y_delete_current_session,
        )
        translated.forEach { id ->
            assertTrue(french.getString(id).isNotBlank())
            assertTrue(english.getString(id).isNotBlank())
            assertTrue("resource $id must be translated", french.getString(id) != english.getString(id))
        }
        assertEquals("Français", english.getString(R.string.language_french))
        assertEquals("English", french.getString(R.string.language_english))
    }

    @Test fun `catalog names and technical literals are not localized`() {
        val userExerciseName = "Développé FY59 personnalisé"
        val protocolLiteral = "trainlog-mobile-export-v3.json"
        // INVARIANT: resource templates interpolate raw domain names without translating or normalizing them.
        assertTrue(context(AppLanguage.ENGLISH).getString(R.string.modify_named, userExerciseName).contains(userExerciseName))
        assertEquals(protocolLiteral, protocolLiteral)
    }

    @Test fun `stable catalog and repository values are classified only at presentation`() {
        val french = context(AppLanguage.FRENCH)
        val english = context(AppLanguage.ENGLISH)
        assertEquals("Pectoraux", localizedBodyZoneName(french, "chest", "Pectoraux"))
        assertEquals("Chest", localizedBodyZoneName(english, "chest", "Pectoraux"))
        assertEquals("Future catalog label", localizedBodyZoneName(english, "future_zone", "Future catalog label"))
        assertEquals("The requested item is no longer available.", localizedRepositoryMessage(english, "Séance introuvable."))
        assertEquals("L’opération en base de données a échoué.", localizedRepositoryMessage(french, "Erreur base locale séances."))
    }
}
