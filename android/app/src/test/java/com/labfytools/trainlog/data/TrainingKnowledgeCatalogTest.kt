package com.labfytools.trainlog.data

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class TrainingKnowledgeCatalogTest {
    private val context: Context = ApplicationProvider.getApplicationContext()
    private val names = listOf(
        "science-references-v1.json", "muscles-v1.json", "joint-actions-v1.json",
        "movement-patterns-v1.json", "exercise-knowledge-v1.json", "equipment-knowledge-v1.json",
    )

    @Test
    fun loadsAllStructuredCatalogsAndRepresentativeMappings() {
        val catalog = TrainingKnowledgeCatalog.load(context)
        assertEquals(listOf(23, 53, 35, 26, 23, 41), listOf(catalog.references.size, catalog.muscles.size, catalog.jointActions.size, catalog.movementPatterns.size, catalog.exercises.size, catalog.equipment.size))
        val legPress = catalog.getExerciseKnowledge("ex_b432623f-bfe9-4daf-a653-60ec7fdffbde")
        assertEquals("knee_dominant", legPress?.interpretation?.patternIds?.single())
        assertTrue(catalog.listExercisesByMovementPattern("knee_dominant").any { it.exerciseId == legPress?.exerciseId })
        assertTrue(catalog.listExercisesByMuscle("quadriceps", MuscleRole.PRIMARY).any { it.exerciseId == legPress?.exerciseId })
        assertEquals("thighs", catalog.getScientificBodyZoneMapping(legPress!!.exerciseId)?.primaryZoneId)
        assertEquals(BodyZoneAuditStatus.CONFIRMED, legPress.bodyZoneAudit.status)
        assertEquals("thighs", legPress.bodyZoneAudit.existingPrimaryZoneId)
        assertTrue(legPress.bodyZoneAudit.sourceRefs.isNotEmpty())

        val multifunction = catalog.getEquipmentKnowledge("rear_delt_pec_fly")
        assertEquals(2, multifunction?.capabilities?.size)
        assertTrue(catalog.listCompatibleExercises("rear_delt_pec_fly").any { it.exerciseId == "ex_4cd2433e-80b1-478a-b8df-73fc6ef80962" })
        assertNotNull(catalog.getMuscle("deltoid_posterior"))
        assertNotNull(catalog.getJointAction("shoulder_horizontal_abduction"))
        assertNotNull(catalog.getReference(legPress.sourceRefs.first()))
    }

    @Test
    fun conditionalAndUnknownRecordsCannotLeakIntoOrdinaryQueries() {
        val catalog = TrainingKnowledgeCatalog.load(context)
        val chestPress = "ex_8552dd77-fcd7-4f06-a1cc-d956eb1009af"
        assertEquals(ExerciseKnowledgeStatus.CONDITIONAL, catalog.getConditionalExerciseKnowledge(chestPress)?.resolutionStatus)
        assertNull(catalog.getScientificBodyZoneMapping(chestPress))
        assertFalse(catalog.queryExercises(KnowledgeExerciseFilters()).any { it.exerciseId == chestPress })
        assertNull(catalog.getExerciseKnowledge("ex_00000000-0000-4000-8000-000000000000"))
    }

    @Test
    fun acceptsAdditiveNonRuntimeKnowledgeRecord() {
        val files = assetFiles()
        val root = JSONObject(files.getValue("science-references-v1.json"))
        root.getJSONArray("references").put(
            JSONObject()
                .put("ref_id", "zz_additive_reference")
                .put("title", "Additive reference fixture")
                .put("authors_or_organization", "Trainlog test")
                .put("year", 2026)
                .put("type", "test_fixture")
                .put("url", "https://example.invalid/additive-reference")
                .put("topics", org.json.JSONArray().put("validation"))
                .put("notes", "Valid additive metadata record.")
                .put("limitations", "Test fixture only.")
                .put("doi", JSONObject.NULL)
                .put("pmid", JSONObject.NULL)
                .put("accessed_on", "2026-09-09"),
        )
        files["science-references-v1.json"] = root.toString()

        val catalog = TrainingKnowledgeCatalog.load(files::getValue, BodyZoneCatalog.load(context))
        assertNotNull(catalog.getReference("zz_additive_reference"))
    }

    @Test
    fun rejectsVersionEnumsDanglingReferencesAndDuplicateKeys() {
        assertInvalid { files -> files["muscles-v1.json"] = files.getValue("muscles-v1.json").replaceFirst("\"version\": 1", "\"version\": 2") }
        assertInvalid { files -> files["muscles-v1.json"] = files.getValue("muscles-v1.json").replaceFirst("\"entity_type\": \"muscle\"", "\"entity_type\": \"organ\"") }
        assertInvalid { files ->
            val root = JSONObject(files.getValue("exercise-knowledge-v1.json")); root.getJSONArray("exercises").getJSONObject(0).getJSONArray("source_refs").put("missing_ref")
            files["exercise-knowledge-v1.json"] = root.toString()
        }
        assertInvalid { files -> files["science-references-v1.json"] = files.getValue("science-references-v1.json").replaceFirst("{", "{\"version\":1,") }
        assertInvalid { files ->
            val root = JSONObject(files.getValue("exercise-knowledge-v1.json"))
            val legPress = root.getJSONArray("exercises").let { rows -> (0 until rows.length()).map(rows::getJSONObject).first { it.getString("exercise_id") == "ex_b432623f-bfe9-4daf-a653-60ec7fdffbde" } }
            val equipment = legPress.getJSONArray("equipment_ids"); val first = equipment.getString(0)
            equipment.put(0, equipment.getString(1)); equipment.put(1, first)
            files["exercise-knowledge-v1.json"] = root.toString()
        }
        assertInvalid { files ->
            val root = JSONObject(files.getValue("exercise-knowledge-v1.json")); val interpretation = root.getJSONArray("exercises").getJSONObject(0).getJSONObject("interpretation")
            interpretation.getJSONArray("secondary_muscle_ids").put(interpretation.getJSONArray("primary_muscle_ids").getString(0))
            files["exercise-knowledge-v1.json"] = root.toString()
        }
        assertInvalid { files ->
            val root = JSONObject(files.getValue("equipment-knowledge-v1.json")); val equipment = root.getJSONArray("equipment").getJSONObject(0)
            equipment.put("confidence", "high"); equipment.put("evidence_type", "manufacturer_statement")
            files["equipment-knowledge-v1.json"] = root.toString()
        }
        assertInvalidAudit { it.getJSONArray("source_refs").put("missing_ref") }
        assertInvalidAudit { it.put("existing_primary_zone_id", "missing_zone") }
        assertInvalidAudit { it.put("status", "compatible") }
    }

    @Test
    fun rejectsInvalidRuntimeIdentitySyntax() {
        assertInvalid { files ->
            val root = JSONObject(files.getValue("exercise-knowledge-v1.json"))
            val row = root.getJSONArray("exercises").getJSONObject(0)
            row.put("exercise_id", row.getString("exercise_id").dropLast(1) + "g")
            files["exercise-knowledge-v1.json"] = root.toString()
        }
        assertInvalid { files ->
            val root = JSONObject(files.getValue("equipment-knowledge-v1.json"))
            val rows = root.getJSONArray("equipment")
            val row = rows.getJSONObject(0)
            val oldId = row.getString("equipment_id")
            val invalidId = "$oldId-"
            row.put("equipment_id", invalidId)
            val exercises = JSONObject(files.getValue("exercise-knowledge-v1.json"))
            val exerciseRows = exercises.getJSONArray("exercises")
            for (index in 0 until exerciseRows.length()) {
                val ids = exerciseRows.getJSONObject(index).getJSONArray("equipment_ids")
                for (idIndex in 0 until ids.length()) if (ids.getString(idIndex) == oldId) ids.put(idIndex, invalidId)
            }
            files["equipment-knowledge-v1.json"] = root.toString()
            files["exercise-knowledge-v1.json"] = exercises.toString()
        }
    }

    @Test
    fun rejectsReverseCompatibilityHighEvidenceAndResolvedAuditGaps() {
        assertInvalid { files ->
            val root = JSONObject(files.getValue("exercise-knowledge-v1.json"))
            val rows = root.getJSONArray("exercises")
            val row = (0 until rows.length()).map(rows::getJSONObject).first { it.getJSONArray("equipment_ids").length() > 0 }
            row.put("equipment_ids", org.json.JSONArray())
            files["exercise-knowledge-v1.json"] = root.toString()
        }
        assertInvalid { files ->
            val references = JSONObject(files.getValue("science-references-v1.json")).getJSONArray("references")
            val nonScientificRef = (0 until references.length()).map(references::getJSONObject)
                .first { it.getString("type") !in setOf("established_anatomy", "emg_evidence", "intervention_evidence") }
                .getString("ref_id")
            val root = JSONObject(files.getValue("equipment-knowledge-v1.json"))
            val rows = root.getJSONArray("equipment")
            val row = rows.getJSONObject(0)
            row.put("confidence", "high")
            row.put("source_refs", org.json.JSONArray().put(nonScientificRef))
            row.put("evidence_type", "mixed_evidence")
            files["equipment-knowledge-v1.json"] = root.toString()
        }
        assertInvalidAudit { audit ->
            audit.put("source_refs", org.json.JSONArray())
            audit.put("status", "confirmed")
        }
    }

    private fun assertInvalid(mutate: (MutableMap<String, String>) -> Unit) {
        val files = assetFiles()
        mutate(files)
        val failed = runCatching { TrainingKnowledgeCatalog.load(files::getValue, BodyZoneCatalog.load(context)) }.isFailure
        assertTrue("catalogue corrompu accepté", failed)
    }

    private fun assetFiles(): MutableMap<String, String> = names.associateWith { name ->
        context.assets.open(name).bufferedReader().use { it.readText() }
    }.toMutableMap()

    private fun assertInvalidAudit(mutate: (JSONObject) -> Unit) = assertInvalid { files ->
        val root = JSONObject(files.getValue("exercise-knowledge-v1.json"))
        mutate(root.getJSONArray("exercises").getJSONObject(0).getJSONObject("body_zone_audit"))
        files["exercise-knowledge-v1.json"] = root.toString()
    }
}
