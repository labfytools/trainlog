package com.labfytools.trainlog.ui

import com.labfytools.trainlog.data.BodyZoneHomeState
import com.labfytools.trainlog.data.BodyZoneHomeStatus
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class BodyZoneHomePresentationTest {
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
        val text = recommendationReason(status)
        assertEquals("Dernier travail principal : il y a 10 jours", text)
        assertEquals("jamais enregistrée", ageLabel(null))
        assertEquals("aujourd’hui", ageLabel(60))
        assertEquals("il y a 10 jours", ageLabel(10 * 86400L))
        listOf("récupération", "fatigue", "prêt", "séance recommandée").forEach {
            assertTrue(it !in text.lowercase())
        }
    }

    @Test fun exposureLabelsUseNaturalFrenchSingularAndPlural() {
        assertEquals("1 exposition principale", exposureCountLabel(1, "principale"))
        assertEquals("2 expositions principales", exposureCountLabel(2, "principale"))
        assertEquals("1 exposition secondaire", exposureCountLabel(1, "secondaire"))
        assertEquals("3 expositions secondaires", exposureCountLabel(3, "secondaire"))
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

        assertEquals("Priorité relative", homeStateLabel(status))
        assertEquals("2 expositions principales sur 7 j", recommendationReason(status))
        assertFalse(recommendationReason(status).contains("fait(s)"))
        assertFalse(recommendationReason(status).contains("pondéré"))
    }
}
