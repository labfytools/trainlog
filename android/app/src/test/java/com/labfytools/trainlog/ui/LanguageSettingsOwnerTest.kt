package com.labfytools.trainlog.ui

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class LanguageSettingsOwnerTest {
    private lateinit var context: Context

    @Before fun setUp() {
        context = ApplicationProvider.getApplicationContext()
        context.getSharedPreferences(LanguageSettingsOwner.PREFERENCES_NAME, Context.MODE_PRIVATE)
            .edit().clear().commit()
    }

    @After fun tearDown() {
        context.getSharedPreferences(LanguageSettingsOwner.PREFERENCES_NAME, Context.MODE_PRIVATE)
            .edit().clear().commit()
    }

    @Test fun missingAndInvalidPreferenceDefaultToFrench() {
        assertEquals(AppLanguage.FRENCH, LanguageSettingsOwner(context).language)
        context.getSharedPreferences(LanguageSettingsOwner.PREFERENCES_NAME, Context.MODE_PRIVATE)
            .edit().putString(LanguageSettingsOwner.KEY_LANGUAGE, "de").commit()
        assertEquals(AppLanguage.FRENCH, LanguageSettingsOwner(context).language)
    }

    @Test fun selectionPersistsAndIsRestoredByANewOwner() {
        val first = LanguageSettingsOwner(context)
        first.select(AppLanguage.ENGLISH)
        assertEquals(AppLanguage.ENGLISH, first.language)
        assertEquals(AppLanguage.ENGLISH, LanguageSettingsOwner(context).language)
    }

    @Test fun languagePreferenceIsDedicatedPresentationState() {
        LanguageSettingsOwner(context).select(AppLanguage.ENGLISH)
        assertEquals(
            setOf(LanguageSettingsOwner.KEY_LANGUAGE),
            context.getSharedPreferences(LanguageSettingsOwner.PREFERENCES_NAME, Context.MODE_PRIVATE).all.keys,
        )
    }
}
