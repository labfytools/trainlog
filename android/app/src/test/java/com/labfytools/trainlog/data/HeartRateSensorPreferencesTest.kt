package com.labfytools.trainlog.data

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class HeartRateSensorPreferencesTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    @Test
    fun selectionIsDurableAndClearable() {
        val preferences = HeartRateSensorPreferences(context)
        preferences.clear()
        assertNull(preferences.selected())

        preferences.select("AA:BB:CC:DD:EE:FF", "Synthetic HR")
        assertEquals(
            SelectedHeartRateSensor("AA:BB:CC:DD:EE:FF", "Synthetic HR"),
            HeartRateSensorPreferences(context).selected(),
        )

        preferences.clear()
        assertNull(HeartRateSensorPreferences(context).selected())
    }
}
