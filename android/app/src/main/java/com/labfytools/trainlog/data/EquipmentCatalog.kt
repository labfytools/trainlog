package com.labfytools.trainlog.data

import android.content.Context
import org.json.JSONObject
import java.text.Normalizer
import java.util.Locale

enum class EquipmentLoadSemantics {
    NONE,
    EXTERNAL,
    ASSISTANCE,
    BODYWEIGHT,
    CARDIO,
}

data class EquipmentCatalogEntry(
    val equipmentId: String,
    val labelName: String,
    val displayName: String,
    val aliases: List<String>,
    val type: String,
    val loadSemantics: EquipmentLoadSemantics,
)

data class ExerciseEquipmentCatalogRelation(
    val exerciseId: String,
    val equipmentId: String,
    val loadSemantics: EquipmentLoadSemantics,
)

/**
 * CONTRACT: `catalog/equipment-v1.json` is shared repository data. This
 * reader deliberately has no fallback list: a missing or malformed manifest
 * is an explicit deployment error, never a silently divergent Android list.
 */
object EquipmentCatalog {
    private const val ASSET_NAME = "equipment-v1.json"

    fun load(context: Context): List<EquipmentCatalogEntry> {
        val root = context.assets.open(ASSET_NAME).bufferedReader().use {
            JSONObject(it.readText())
        }
        check(root.getString("format") == "trainlog-equipment-catalog")
        check(root.getInt("version") == 1)
        val seen = mutableSetOf<String>()
        return buildList {
            val entries = root.getJSONArray("equipment")
            for (index in 0 until entries.length()) {
                val item = entries.getJSONObject(index)
                val id = item.getString("id").trim()
                check(id.isNotEmpty()) { "equipment_id vide" }
                check(seen.add(id)) { "equipment_id dupliqué: $id" }
                val aliases = item.getJSONArray("aliases")
                val parsedAliases = List(aliases.length()) { aliases.getString(it).trim() }
                check(parsedAliases.all { it.isNotEmpty() }) { "alias vide pour $id" }
                val labelName = item.getString("label_name").trim()
                val displayName = item.getString("display_name").trim()
                check(displayName.isNotEmpty()) { "display_name vide pour $id" }
                val type = item.getString("type").trim()
                check(type.isNotEmpty()) { "type vide pour $id" }
                add(
                    EquipmentCatalogEntry(
                        equipmentId = id,
                        labelName = labelName,
                        displayName = displayName,
                        aliases = parsedAliases,
                        type = type,
                        loadSemantics = EquipmentLoadSemantics.valueOf(
                            item.getString("load_semantics").uppercase(Locale.ROOT),
                        ),
                    ),
                )
            }
        }
    }

    fun exerciseEquipmentRelations(
        context: Context,
    ): List<ExerciseEquipmentCatalogRelation> {
        val root = context.assets.open(ASSET_NAME).bufferedReader().use {
            JSONObject(it.readText())
        }
        val knownEquipment = load(context).map { it.equipmentId }.toSet()
        val relations = root.getJSONArray("exercise_equipment")
        return List(relations.length()) { index ->
            val item = relations.getJSONObject(index)
            val equipmentId = item.getString("equipment_id")
            check(equipmentId in knownEquipment)
            ExerciseEquipmentCatalogRelation(
                exerciseId = item.getString("exercise_id"),
                equipmentId = equipmentId,
                loadSemantics = EquipmentLoadSemantics.valueOf(
                    item.getString("load_semantics").uppercase(Locale.ROOT),
                ),
            )
        }
    }

    fun search(
        entries: List<EquipmentCatalogEntry>,
        query: String,
    ): List<EquipmentCatalogEntry> {
        val needle = normalize(query)
        if (needle.isEmpty()) return entries
        return entries.filter { entry ->
            sequenceOf(entry.displayName, entry.labelName)
                .plus(entry.aliases.asSequence())
                .any { normalize(it).contains(needle) }
        }
    }

    private fun normalize(value: String): String =
        Normalizer.normalize(value, Normalizer.Form.NFD)
            .replace("\\p{M}+".toRegex(), "")
            .lowercase(Locale.ROOT)
            .trim()
}
