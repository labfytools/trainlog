package com.labfytools.trainlog.data

import android.content.Context
import org.json.JSONObject

enum class BodyZoneKind { GROUP, LEAF, STANDALONE }

data class BodyZone(
    val zoneId: String,
    val displayName: String,
    val parentZoneId: String?,
    val sortOrder: Int,
    val kind: BodyZoneKind,
)

data class InitialExerciseBodyZones(
    val exerciseId: String,
    val primaryZoneId: String,
    val secondaryZoneIds: List<String>,
)

/**
 * CONTRACT: Android reads the repository-level body-zone manifest directly.
 * There is deliberately no Kotlin fallback list: IDs, hierarchy and initial
 * identity mappings cannot silently diverge from the generated C catalogue.
 */
class BodyZoneCatalog private constructor(
    val zones: List<BodyZone>,
    val initialMappings: List<InitialExerciseBodyZones>,
) {
    private val byId = zones.associateBy { it.zoneId }

    fun lookup(zoneId: String): BodyZone? = byId[zoneId]

    fun children(zoneId: String): List<BodyZone> =
        zones.filter { it.parentZoneId == zoneId }

    fun ancestors(zoneId: String): List<BodyZone> = buildList {
        var current = lookup(zoneId)
        while (current?.parentZoneId != null) {
            current = lookup(current.parentZoneId)
            checkNotNull(current) { "Parent de zone absent" }
            add(current)
        }
    }

    fun descendantsAndSelf(zoneId: String): Set<String> {
        checkNotNull(lookup(zoneId)) { "zone_id inconnu: $zoneId" }
        val output = linkedSetOf(zoneId)
        var changed = true
        while (changed) {
            changed = false
            zones.forEach { zone ->
                if (zone.parentZoneId in output && output.add(zone.zoneId)) changed = true
            }
        }
        return output
    }

    companion object {
        private const val ASSET_NAME = "body-zones-v1.json"

        fun load(context: Context): BodyZoneCatalog {
            val root = context.assets.open(ASSET_NAME).bufferedReader().use {
                JSONObject(it.readText())
            }
            check(root.keys().asSequence().toSet() == setOf(
                "format", "version", "zones", "exercise_mappings",
            ))
            check(root.getString("format") == "trainlog-body-zone-catalog")
            check(root.getInt("version") == 1)
            val array = root.getJSONArray("zones")
            val seenIds = mutableSetOf<String>()
            val seenOrders = mutableSetOf<Int>()
            val zones = List(array.length()) { index ->
                val item = array.getJSONObject(index)
                check(item.keys().asSequence().toSet() == setOf(
                    "zone_id", "display_name", "parent_zone_id", "sort_order", "kind",
                ))
                val zone = BodyZone(
                    zoneId = item.getString("zone_id"),
                    displayName = item.getString("display_name").trim(),
                    parentZoneId = if (item.isNull("parent_zone_id")) null else item.getString("parent_zone_id"),
                    sortOrder = item.getInt("sort_order"),
                    kind = BodyZoneKind.valueOf(item.getString("kind").uppercase()),
                )
                check(zone.zoneId.matches(Regex("[a-z][a-z0-9_]*")))
                check(zone.displayName.isNotEmpty() && zone.sortOrder >= 0)
                check(seenIds.add(zone.zoneId) && seenOrders.add(zone.sortOrder))
                zone
            }.sortedBy { it.sortOrder }
            val byId = zones.associateBy { it.zoneId }
            zones.forEach { zone ->
                check(zone.parentZoneId == null || zone.parentZoneId in byId)
                val seen = mutableSetOf(zone.zoneId)
                var parent = zone.parentZoneId
                while (parent != null) {
                    check(seen.add(parent)) { "Boucle dans la hiérarchie corporelle" }
                    parent = byId.getValue(parent).parentZoneId
                }
                check((zone.kind == BodyZoneKind.GROUP) == zones.any { it.parentZoneId == zone.zoneId })
            }
            val mappingsArray = root.getJSONArray("exercise_mappings")
            val seenExercises = mutableSetOf<String>()
            val mappings = List(mappingsArray.length()) { index ->
                val item = mappingsArray.getJSONObject(index)
                check(item.keys().asSequence().toSet() == setOf(
                    "exercise_id", "exercise_name", "primary_zone_id",
                    "secondary_zone_ids", "decision_source",
                ))
                val secondary = item.getJSONArray("secondary_zone_ids")
                val mapping = InitialExerciseBodyZones(
                    exerciseId = item.getString("exercise_id"),
                    primaryZoneId = item.getString("primary_zone_id"),
                    secondaryZoneIds = List(secondary.length()) { secondary.getString(it) },
                )
                check(seenExercises.add(mapping.exerciseId))
                check(item.getString("exercise_name").isNotBlank())
                check(item.getString("decision_source").isNotBlank())
                check(byId[mapping.primaryZoneId]?.kind != null &&
                    byId.getValue(mapping.primaryZoneId).kind != BodyZoneKind.GROUP)
                check(mapping.secondaryZoneIds.toSet().size == mapping.secondaryZoneIds.size)
                check(mapping.primaryZoneId !in mapping.secondaryZoneIds)
                check(mapping.secondaryZoneIds.all {
                    byId[it]?.kind != null && byId.getValue(it).kind != BodyZoneKind.GROUP
                })
                mapping
            }
            return BodyZoneCatalog(zones, mappings)
        }
    }
}
