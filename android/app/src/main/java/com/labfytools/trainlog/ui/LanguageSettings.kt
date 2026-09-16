package com.labfytools.trainlog.ui

import android.content.Context
import android.content.res.Configuration
import androidx.compose.runtime.Composable
import androidx.compose.runtime.Immutable
import androidx.compose.runtime.Stable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.platform.LocalContext
import androidx.annotation.PluralsRes
import androidx.annotation.StringRes
import com.labfytools.trainlog.R
import java.util.Locale
import java.text.NumberFormat

enum class AppLanguage(val tag: String) {
    FRENCH("fr"),
    ENGLISH("en");

    companion object {
        fun fromStoredTag(tag: String?): AppLanguage = entries.firstOrNull { it.tag == tag } ?: FRENCH
    }
}

/**
 * WHY: language is device-local presentation state and must never enter the
 * canonical workout database or an exchange artifact.
 * CONTRACT: this owner persists exactly one BCP-47 tag in its own preferences
 * file; French is used when the preference is absent or invalid.
 * INVARIANT: changing [language] updates Compose synchronously and does not
 * call TrainlogRepository, SQLite, import, export, or synchronization code.
 */
@Stable
class LanguageSettingsOwner(context: Context) {
    private val preferences = context.applicationContext.getSharedPreferences(PREFERENCES_NAME, Context.MODE_PRIVATE)

    var language: AppLanguage by mutableStateOf(AppLanguage.fromStoredTag(preferences.getString(KEY_LANGUAGE, null)))
        private set

    fun select(value: AppLanguage) {
        if (language == value) return
        preferences.edit().putString(KEY_LANGUAGE, value.tag).apply()
        language = value
    }

    companion object {
        internal const val PREFERENCES_NAME = "trainlog_presentation_settings"
        internal const val KEY_LANGUAGE = "language_tag"
    }
}

@Immutable
data class LanguagePresentation(
    val language: AppLanguage,
    val select: (AppLanguage) -> Unit,
)

val LocalLanguagePresentation = staticCompositionLocalOf<LanguagePresentation> {
    /* Preview and focused component tests have no Activity owner. Their
     * presentation contract is the same deterministic French default. */
    LanguagePresentation(AppLanguage.FRENCH) {}
}

@Composable
fun localizedContext(): Context {
    val base = LocalContext.current
    val language = LocalLanguagePresentation.current.language
    val configuration = Configuration(base.resources.configuration).apply {
        setLocale(Locale.forLanguageTag(language.tag))
    }
    return base.createConfigurationContext(configuration)
}

@Composable
fun presentationLocale(): Locale = Locale.forLanguageTag(LocalLanguagePresentation.current.language.tag)

@Composable
fun uiString(@StringRes id: Int, vararg arguments: Any): String =
    localizedContext().resources.getString(id, *arguments)

@Composable
fun uiQuantity(@PluralsRes id: Int, quantity: Int, vararg arguments: Any): String =
    localizedContext().resources.getQuantityString(id, quantity, *arguments)

@Composable
fun presentationNumber(value: Double, maximumFractionDigits: Int = 2): String =
    NumberFormat.getNumberInstance(presentationLocale()).apply {
        isGroupingUsed = false
        minimumFractionDigits = 0
        this.maximumFractionDigits = maximumFractionDigits
    }.format(value)

/**
 * CONTRACT: body-zone IDs are canonical catalog data; only their visible labels are localized.
 * Unknown future IDs deliberately retain the catalog label instead of changing identity data.
 */
fun localizedBodyZoneName(context: Context, zoneId: String, catalogLabel: String): String {
    val id = when (zoneId) {
        "full_body" -> R.string.zone_full_body
        "upper_body" -> R.string.zone_upper_body
        "chest" -> R.string.zone_chest
        "back" -> R.string.zone_back
        "shoulders" -> R.string.zone_shoulders
        "arms" -> R.string.zone_arms
        "core" -> R.string.zone_core
        "lower_body" -> R.string.zone_lower_body
        "glutes" -> R.string.zone_glutes
        "thighs" -> R.string.zone_thighs
        "calves" -> R.string.zone_calves
        else -> return catalogLabel
    }
    return context.getString(id)
}

/**
 * WHY: repository diagnostics are persistence/domain values, not translated payload fields.
 * CONTRACT: classify them at the Android presentation boundary without rewriting repository state.
 */
fun localizedRepositoryMessage(context: Context, raw: String): String {
    val normalized = raw.lowercase(Locale.ROOT)
    val id = when {
        "seule la sélection" in normalized -> R.string.draft_selection_missing
        listOf("introuvable", "not found", "missing").any(normalized::contains) -> R.string.repository_item_missing
        listOf("base locale", "base de données", "database", "sqlite").any(normalized::contains) -> R.string.repository_database_error
        listOf("invalide", "invalid", "incompatible").any(normalized::contains) -> R.string.repository_invalid_data
        listOf("accès fichiers", "autorisation", "permission", "file access").any(normalized::contains) -> R.string.repository_file_access
        else -> R.string.repository_operation_error
    }
    return context.getString(id)
}
