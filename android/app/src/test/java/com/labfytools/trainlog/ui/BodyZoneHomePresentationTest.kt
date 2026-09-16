/*
 * Regression coverage for BodyZoneHomePresentationTest.
 *
 * Exercises production contracts without owning runtime behavior or persistent formats.
 */
package com.labfytools.trainlog.ui

import android.content.Context
import android.content.res.Configuration
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.data.BodyZoneHomeState
import com.labfytools.trainlog.data.BodyZoneHomeStatus
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.util.Locale

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class BodyZoneHomePresentationTest {
    private val context: Context = ApplicationProvider.getApplicationContext<Context>().createConfigurationContext(
        Configuration(ApplicationProvider.getApplicationContext<Context>().resources.configuration).apply { setLocale(Locale.FRENCH) })
    @Test fun detailWordingIsExposureOnlyAndIncludesEvidence() {
        val status = BodyZoneHomeStatus(
            zoneId = "chest",
            displayName = "Pectoraux",
            lastPrimaryExposure = "2026-09-01T12:00:00Z",
            lastSecondaryExposure = null,
            primaryExposureAgeSeconds = 10 * 86400L,
            secondaryExposureAgeSeconds = null,
            primaryWork7Days = 0,
            secondaryWork7Days = 1,
            primaryWork30Days = 2,
            secondaryWork30Days = 3,
            recentSessionCount = 2,
            availableExerciseCount = 4,
            state = BodyZoneHomeState.PRIORITIZE,
            reasons = listOf("Exposition primaire moins récente."),
        )
        val text = recommendationReason(status, context)
        assertEquals("Dernier travail principal : il y a 10 jours", text)
        assertEquals("jamais enregistrée", ageLabel(null, context))
        assertEquals("aujourd’hui", ageLabel(60, context))
        assertEquals("il y a 10 jours", ageLabel(10 * 86400L, context))
        listOf("récupération", "fatigue", "prêt", "séance recommandée").forEach {
            assertTrue(it !in text.lowercase())
        }
    }

    @Test fun recentWorkLabelsAreNeutralFactualCounts() {
        assertEquals("Travail principal sur 7 j : 1", recentWorkLabel(1, true, context))
        assertEquals("Travail principal sur 7 j : 2", recentWorkLabel(2, true, context))
        assertEquals("Travail secondaire sur 7 j : 1", recentWorkLabel(1, false, context))
        assertEquals("Travail secondaire sur 7 j : 3", recentWorkLabel(3, false, context))
    }

    @Test fun recentPrioritizedExposureIsPresentedAsRelativePriority() {
        val status = BodyZoneHomeStatus(
            zoneId = "chest",
            displayName = "Pectoraux",
            lastPrimaryExposure = "2026-09-10T12:00:00Z",
            lastSecondaryExposure = null,
            primaryExposureAgeSeconds = 86400L,
            secondaryExposureAgeSeconds = null,
            primaryWork7Days = 2,
            secondaryWork7Days = 1,
            primaryWork30Days = 3,
            secondaryWork30Days = 1,
            recentSessionCount = 2,
            availableExerciseCount = 4,
            state = BodyZoneHomeState.PRIORITIZE,
            reasons = emptyList(),
        )

        assertEquals("Priorité relative", homeStateLabel(status, context))
        assertEquals("Travail principal sur 7 j : 2", recommendationReason(status, context))
        assertFalse(recommendationReason(status, context).contains("exposition"))
        assertFalse(recommendationReason(status, context).contains("fait(s)"))
        assertFalse(recommendationReason(status, context).contains("pondéré"))
    }
}
