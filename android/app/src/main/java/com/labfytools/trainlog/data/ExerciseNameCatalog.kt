package com.labfytools.trainlog.data

import android.content.Context
import org.json.JSONObject
import java.text.Normalizer
import java.util.Locale
import java.util.UUID

/** Strict shared owner map for presentation names of known stable exercise IDs. */
object ExerciseNameCatalog {
    private const val ASSET_NAME = "exercise-names-v1.json"

    fun load(context: Context): Map<String, String> {
        val root = context.assets.open(ASSET_NAME).bufferedReader().use {
            JSONObject(it.readText())
        }
        check(root.keys().asSequence().toSet() == setOf("format", "version", "names"))
        check(root.getString("format") == "trainlog-exercise-names-v1")
        check(root.getInt("version") == 1)

        val names = root.getJSONArray("names")
        val result = linkedMapOf<String, String>()
        val canonicalOwners = mutableSetOf<String>()
        val previousOwners = mutableSetOf<String>()
        for (index in 0 until names.length()) {
            val item = names.getJSONObject(index)
            check(item.keys().asSequence().toSet() == setOf(
                "exercise_id", "display_name", "normalized_name", "previous_display_names",
            ))
            val exerciseId = item.getString("exercise_id")
            check(exerciseId.startsWith("ex_"))
            val uuid = UUID.fromString(exerciseId.removePrefix("ex_"))
            check(uuid.version() == 4 && "ex_$uuid" == exerciseId)
            val displayName = item.getString("display_name")
            val normalizedName = normalizeManifestName(displayName)
            check(displayName == displayName.trim() && item.getString("normalized_name") == normalizedName)
            check(result.put(exerciseId, displayName) == null)
            check(canonicalOwners.add(normalizedName))

            val previous = item.getJSONArray("previous_display_names")
            check(previous.length() > 0)
            val localPrevious = mutableSetOf<String>()
            for (previousIndex in 0 until previous.length()) {
                val oldName = previous.getString(previousIndex)
                val oldNormalized = normalizeManifestName(oldName)
                check(oldName == oldName.trim() && oldNormalized != normalizedName)
                check(localPrevious.add(oldNormalized) && previousOwners.add(oldNormalized))
            }
        }
        check(canonicalOwners.intersect(previousOwners).isEmpty())
        return result
    }

    private fun normalizeManifestName(value: String): String {
        val normalized = Normalizer.normalize(value.lowercase(Locale.ROOT), Normalizer.Form.NFC)
            .replace(Regex("\\s+"), " ")
            .trim()
        check(normalized.isNotEmpty())
        return normalized
    }
}
