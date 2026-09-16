package com.labfytools.trainlog.ui

import android.content.Context
import android.content.res.Configuration
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.data.SyncReceiptResult
import java.util.Locale
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class SyncReceiptPresentationTest {
    private val base: Context = ApplicationProvider.getApplicationContext()

    private fun englishContext(): Context = base.createConfigurationContext(
        Configuration(base.resources.configuration).apply {
            setLocale(Locale.forLanguageTag(AppLanguage.ENGLISH.tag))
        },
    )

    @Test fun `english success never exposes opaque french receipt summary`() {
        val raw = "Synchronisation terminée : résumé brut du PC."
        val receipt = SyncReceiptResult.Received("sy_test", true, raw)

        val visible = visibleReceiptStatus(
            englishContext(),
            receipt,
            "2 new, 3 reconciled, 4 present",
        )

        assertTrue(visible.contains("Synchronization complete"))
        assertTrue(visible.contains("2 new, 3 reconciled, 4 present"))
        assertFalse(visible.contains(raw))
        assertFalse(visible.contains("Synchronisation"))
        /* INVARIANT: presentation neither rewrites nor consumes compatibility
         * payload; the parser-owned value remains byte-exact on the receipt. */
        assertEquals(raw, receipt.summary)
    }

    @Test fun `english failure never exposes opaque french receipt summary`() {
        val raw = "Échec de synchronisation : diagnostic brut du PC."
        val receipt = SyncReceiptResult.Received("sy_test", false, raw)

        val visible = visibleReceiptStatus(englishContext(), receipt)

        assertTrue(visible.contains("PC synchronization failed"))
        assertFalse(visible.contains(raw))
        assertFalse(visible.contains("Échec"))
        assertEquals(raw, receipt.summary)
    }
}
