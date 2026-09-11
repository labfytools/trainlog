package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.ExerciseEditInput
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.TrackingMode
import org.json.JSONArray
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class BodyZonesTest {
    private lateinit var context: Context
    private val databases = mutableListOf<String>()

    @Before
    fun setUp() {
        context = ApplicationProvider.getApplicationContext()
    }

    @After
    fun tearDown() {
        databases.forEach(context::deleteDatabase)
    }

    private fun repository(name: String): TrainlogRepository {
        databases += name
        context.deleteDatabase(name)
        return TrainlogRepository(context, name)
    }

    @Test
    fun canonicalTaxonomyIsUniqueAcyclicAndHasDeclaredGroups() {
        val catalog = BodyZoneCatalog.load(context)
        assertEquals(11, catalog.zones.size)
        assertEquals(catalog.zones.size, catalog.zones.map { it.zoneId }.toSet().size)
        assertEquals(catalog.zones.size, catalog.zones.map { it.sortOrder }.toSet().size)
        assertEquals(null, catalog.lookup("full_body")!!.parentZoneId)
        assertEquals(BodyZoneKind.GROUP, catalog.lookup("upper_body")!!.kind)
        assertEquals(setOf("chest", "back", "shoulders", "arms"),
            catalog.descendantsAndSelf("upper_body") - "upper_body")
        assertEquals(setOf("glutes", "thighs", "calves"),
            catalog.descendantsAndSelf("lower_body") - "lower_body")
        catalog.zones.forEach { zone ->
            assertTrue(catalog.ancestors(zone.zoneId).map { it.zoneId }.distinct().size <= 1)
        }
    }

    @Test
    fun createEditFilterUnclassifiedAndReopenPreserveRelations() {
        val name = "body-zones-model.db"
        val first = repository(name)
        val created = first.createExercise(NewExerciseProfile(
            name = "Chest custom", recordingMode = RecordingMode.SETS,
            trackingMode = TrackingMode.REPS, dataFields = 0,
            primaryZoneId = "chest", secondaryZoneIds = listOf("shoulders", "arms"),
        )) as CreateExerciseResult.Created
        assertEquals("chest", created.exercise.primaryZoneId)
        assertEquals(listOf(created.exercise.exerciseId),
            first.listExercises(zoneId = "upper_body").map { it.exerciseId })
        assertEquals(listOf(created.exercise.exerciseId),
            first.listExercises(query = "che", zoneId = "chest").map { it.exerciseId })
        assertTrue(first.listExercises(query = "lat", zoneId = "back").isEmpty())
        assertTrue(first.createExercise(NewExerciseProfile(
            "Invalid", RecordingMode.SETS, TrackingMode.REPS, 0,
            primaryZoneId = "chest", secondaryZoneIds = listOf("chest"),
        )) is CreateExerciseResult.Invalid)
        assertTrue(first.createExercise(NewExerciseProfile(
            "Unknown", RecordingMode.SETS, TrackingMode.REPS, 0,
            primaryZoneId = "unknown", secondaryZoneIds = emptyList(),
        )) is CreateExerciseResult.Invalid)
        assertTrue(first.createExercise(NewExerciseProfile(
            "Derived group", RecordingMode.SETS, TrackingMode.REPS, 0,
            primaryZoneId = "upper_body", secondaryZoneIds = emptyList(),
        )) is CreateExerciseResult.Invalid)
        assertTrue(first.createExercise(NewExerciseProfile(
            "Orphan secondary", RecordingMode.SETS, TrackingMode.REPS, 0,
            primaryZoneId = null, secondaryZoneIds = listOf("arms"),
        )) is CreateExerciseResult.Invalid)

        listOf("glutes", "thighs", "calves").forEach { zoneId ->
            assertTrue(first.createExercise(NewExerciseProfile(
                "Lower $zoneId", RecordingMode.SETS, TrackingMode.REPS, 0,
                primaryZoneId = zoneId, secondaryZoneIds = emptyList(),
            )) is CreateExerciseResult.Created)
        }
        assertEquals(3, first.listExercises(zoneId = "lower_body").size)

        val edited = first.editExercise(ExerciseEditInput(
            created.exercise.exerciseId, "Chest custom", RecordingMode.SETS,
            TrackingMode.REPS, 0, "back", listOf("arms"),
        ))
        assertTrue(edited is EditExerciseResult.Saved)
        assertTrue(first.listExercises(zoneId = "chest").isEmpty())
        assertEquals(1, first.listExercises(zoneId = "back").size)
        val unclassified = first.createExercise(NewExerciseProfile(
            "Marche custom", RecordingMode.CONTINUOUS, TrackingMode.DURATION, 0,
        )) as CreateExerciseResult.Created
        assertEquals(listOf(unclassified.exercise.exerciseId),
            first.listExercises(unclassifiedOnly = true).map { it.exerciseId })
        var ambiguousFilterRejected = false
        try {
            first.listExercises(zoneId = "back", unclassifiedOnly = true)
        } catch (_: IllegalArgumentException) {
            ambiguousFilterRejected = true
        }
        assertTrue(ambiguousFilterRejected)
        first.close()

        val reopened = TrainlogRepository(context, name)
        val restored = reopened.listExercises().associateBy { it.exerciseId }
        assertEquals("back", restored.getValue(created.exercise.exerciseId).primaryZoneId)
        assertEquals(listOf("arms"), restored.getValue(created.exercise.exerciseId).secondaryZoneIds)
        reopened.close()
    }

    @Test
    fun companionReplaysUpdatesOneSideAndRejectsSimultaneousDivergence() {
        val first = repository("body-zones-sync-a.db")
        val created = first.createExercise(NewExerciseProfile(
            "Sync custom", RecordingMode.SETS, TrackingMode.REPS, 0,
            "chest", listOf("arms"),
        )) as CreateExerciseResult.Created
        val catalog = JSONObject()
            .put("format", "trainlog-pc-catalog").put("version", 1)
            .put("generated_at", "2032-01-01T00:00:00+00:00")
            .put("exercises", JSONArray().put(JSONObject()
                .put("exercise_id", created.exercise.exerciseId)
                .put("name", created.exercise.name)
                .put("recording_mode", "sets").put("tracking_mode", "reps")
                .put("data_fields", 0)))
        val second = repository("body-zones-sync-b.db")
        assertTrue(second.applyPcCatalogJson(catalog.toString()) is PcCatalogImportResult.Applied)
        val initial = first.buildExerciseBodyZonesJson()
        // The creator acknowledges only the exact snapshot that publication
        // made visible; this establishes the missing baseline for a custom row.
        assertTrue(first.applyExerciseBodyZonesJson(initial) is ExerciseBodyZoneImportResult.Applied)
        assertEquals(1, (second.applyExerciseBodyZonesJson(initial) as
            ExerciseBodyZoneImportResult.Applied).updated)
        assertTrue(second.applyExerciseBodyZonesJson(initial) is ExerciseBodyZoneImportResult.Applied)

        assertTrue(second.editExercise(ExerciseEditInput(
            created.exercise.exerciseId, created.exercise.name, RecordingMode.SETS,
            TrackingMode.REPS, 0, "shoulders", listOf("arms"),
        )) is EditExerciseResult.Saved)
        val returned = second.buildExerciseBodyZonesJson()
        assertTrue(second.applyExerciseBodyZonesJson(returned) is ExerciseBodyZoneImportResult.Applied)
        assertEquals(1, (first.applyExerciseBodyZonesJson(returned) as
            ExerciseBodyZoneImportResult.Applied).updated)
        assertEquals("shoulders", first.listExercises().single().primaryZoneId)

        assertTrue(first.editExercise(ExerciseEditInput(
            created.exercise.exerciseId, created.exercise.name, RecordingMode.SETS,
            TrackingMode.REPS, 0, "chest", listOf("arms"),
        )) is EditExerciseResult.Saved)
        assertTrue(second.editExercise(ExerciseEditInput(
            created.exercise.exerciseId, created.exercise.name, RecordingMode.SETS,
            TrackingMode.REPS, 0, "back", listOf("arms"),
        )) is EditExerciseResult.Saved)
        val divergent = first.buildExerciseBodyZonesJson()
        assertTrue(first.applyExerciseBodyZonesJson(divergent) is ExerciseBodyZoneImportResult.Applied)
        assertTrue(second.applyExerciseBodyZonesJson(divergent) is
            ExerciseBodyZoneImportResult.Conflict)
        assertEquals("back", second.listExercises().single().primaryZoneId)
        first.close()
        second.close()
    }

    @Test
    fun companionRejectsInvalidStableIdentityAndTimestamp() {
        val repository = repository("body-zones-invalid-companion.db")
        fun artifact(exerciseId: String, generatedAt: String) = JSONObject()
            .put("format", "trainlog-exercise-body-zones")
            .put("version", 1)
            .put("generated_at", generatedAt)
            .put("exercises", JSONArray().put(JSONObject()
                .put("exercise_id", exerciseId)
                .put("primary_zone_id", JSONObject.NULL)
                .put("secondary_zone_ids", JSONArray())))
            .toString()
        assertTrue(repository.applyExerciseBodyZonesJson(
            artifact("ex_not-a-uuid", "2032-01-01T00:00:00+00:00"),
        ) is ExerciseBodyZoneImportResult.Invalid)
        assertTrue(repository.applyExerciseBodyZonesJson(
            artifact("ex_11111111-1111-4111-8111-111111111111", "sans-offset"),
        ) is ExerciseBodyZoneImportResult.Invalid)
        repository.close()
    }

    @Test
    fun catalogIdentityMergePreservesOneZoneStateAndRejectsTwoDifferentStates() {
        fun pcCatalog(exerciseId: String, name: String) = JSONObject()
            .put("format", "trainlog-pc-catalog").put("version", 1)
            .put("generated_at", "2032-01-01T00:00:00+00:00")
            .put("exercises", JSONArray().put(JSONObject()
                .put("exercise_id", exerciseId).put("name", name)
                .put("recording_mode", "sets").put("tracking_mode", "reps")
                .put("data_fields", 0)))
            .toString()

        val preserving = repository("body-zones-identity-merge.db")
        val canonical = preserving.createExercise(NewExerciseProfile(
            "Canonical old name", RecordingMode.SETS, TrackingMode.REPS, 0,
        )) as CreateExerciseResult.Created
        preserving.createExercise(NewExerciseProfile(
            "Merged name", RecordingMode.SETS, TrackingMode.REPS, 0,
            "chest", listOf("shoulders", "arms"),
        ))
        assertTrue(preserving.applyPcCatalogJson(
            pcCatalog(canonical.exercise.exerciseId, "Merged name"),
        ) is PcCatalogImportResult.Applied)
        val merged = preserving.listExercises().single()
        assertEquals(canonical.exercise.exerciseId, merged.exerciseId)
        assertEquals("chest", merged.primaryZoneId)
        assertEquals(listOf("shoulders", "arms"), merged.secondaryZoneIds)
        preserving.close()

        val conflicting = repository("body-zones-identity-conflict.db")
        val conflictingCanonical = conflicting.createExercise(NewExerciseProfile(
            "Conflicting old name", RecordingMode.SETS, TrackingMode.REPS, 0,
            "chest", emptyList(),
        )) as CreateExerciseResult.Created
        conflicting.createExercise(NewExerciseProfile(
            "Conflicting merged name", RecordingMode.SETS, TrackingMode.REPS, 0,
            "back", emptyList(),
        ))
        assertTrue(conflicting.applyPcCatalogJson(
            pcCatalog(conflictingCanonical.exercise.exerciseId, "Conflicting merged name"),
        ) is PcCatalogImportResult.Invalid)
        assertEquals(2, conflicting.listExercises().size)
        conflicting.close()
    }

    @Test
    fun versionNineFixtureMigratesKnownIdentityWithoutTouchingHistoryDraftMaxOrEquipment() {
        val name = "body-zones-v9.db"
        val initial = repository(name)
        initial.listExercises() // Materialize the complete current schema and bundled equipment.
        initial.close()
        SQLiteDatabase.openDatabase(
            context.getDatabasePath(name).path, null, SQLiteDatabase.OPEN_READWRITE,
        ).use { db ->
            db.execSQL("DROP TABLE exercise_body_zone_sync;")
            db.execSQL("DROP TABLE exercise_body_zones;")
            db.execSQL(
                "INSERT INTO exercises VALUES(42,'ex_9adf7566-f10c-443c-b63b-681d37665693'," +
                    "'Abdominal crunch','abdominal crunch','sets','reps',0);",
            )
            val equipmentRow = db.rawQuery(
                "SELECT id FROM equipment WHERE equipment_id='abdominal';", null,
            ).use { cursor -> assertTrue(cursor.moveToFirst()); cursor.getLong(0) }
            db.execSQL(
                "INSERT INTO sessions(id,session_id,started_at,session_type) " +
                    "VALUES(5,'se_zone_training','2032-01-01T08:00:00+01:00','training')," +
                    "(6,'se_zone_max','2032-01-02T08:00:00+01:00','max_test');",
            )
            db.execSQL(
                "INSERT INTO session_exercises(id,session_row_id,exercise_row_id,position," +
                    "recording_mode,tracking_mode,data_fields,equipment_row_id,entry_id) " +
                    "VALUES(7,5,42,0,'sets','reps',0,?,'sxe_zone_training')," +
                    "(8,6,42,0,'sets','reps',0,?,'sxe_zone_max');",
                arrayOf<Any>(equipmentRow, equipmentRow),
            )
            db.execSQL(
                "INSERT INTO performed_sets(id,session_exercise_row_id,position,reps,weight_kg) " +
                    "VALUES(9,7,0,12,42.5);",
            )
            db.execSQL("INSERT INTO max_results VALUES(8,85.0);")
            db.execSQL(
                "INSERT INTO body_observations(id,observation_id,observed_at,body_weight_kg,waist_cm) " +
                    "VALUES(12,'bo_zone_preserved','2032-01-02T09:00:00+01:00',81.5,91.0);",
            )
            db.execSQL(
                "INSERT INTO equipment(id,equipment_id,label_name,display_name,equipment_type,load_semantics) " +
                    "VALUES(999,'eq_44444444-4444-4444-8444-444444444444'," +
                    "'Machine custom','Machine custom','strength_machine','external');",
            )
            db.execSQL(
                "INSERT INTO active_session_draft(id,session_type,selected_exercise_row_id," +
                    "selected_exercise_label,selected_equipment_id,source_session_id,weight_text," +
                    "max_weight_text,set_count_text,reps_text,duration_text,speed_text,distance_text,updated_at) " +
                    "VALUES(1,'training',42,'Abdominal crunch','abdominal',NULL,'42,5','','1','10','','',''," +
                    "'2032-01-03T08:00:00+01:00');",
            )
            db.execSQL(
                "INSERT INTO draft_session_exercises(id,draft_id,exercise_row_id,position," +
                    "recording_mode,tracking_mode,data_fields,equipment_row_id,entry_id) " +
                    "VALUES(10,1,42,0,'sets','reps',0,?,'sxe_zone_draft');",
                arrayOf<Any>(equipmentRow),
            )
            db.execSQL(
                "INSERT INTO draft_performed_sets(id,draft_exercise_row_id,position,reps,weight_kg) " +
                    "VALUES(11,10,0,10,37.5);",
            )
            db.execSQL("PRAGMA user_version=9;")
        }
        val repository = TrainlogRepository(context, name)
        val exercise = repository.listExercises().single()
        assertEquals("core", exercise.primaryZoneId)
        val training = repository.getSessionDetail("se_zone_training")!!.exercises.single()
        assertEquals("sxe_zone_training", training.entryId)
        assertEquals(42.5, training.sets.single().weightKg!!, 0.0)
        assertEquals("Abdominal — Crunch machine", training.equipmentDisplayName)
        assertEquals(85.0, repository.listLatestExerciseMaxima().single().maxWeightKg, 0.0)
        val draft = repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded
        assertEquals("sxe_zone_draft", draft.draft.exercises.single().entryId)
        assertEquals(37.5, draft.draft.exercises.single().sets.single().weightKg!!, 0.0)
        repository.close()
        SQLiteDatabase.openDatabase(context.getDatabasePath(name).path, null,
            SQLiteDatabase.OPEN_READONLY).use { db ->
            db.rawQuery("PRAGMA user_version", null).use { cursor ->
                assertTrue(cursor.moveToFirst()); assertEquals(12, cursor.getInt(0))
            }
            db.rawQuery("SELECT id FROM exercises", null).use { cursor ->
                assertTrue(cursor.moveToFirst()); assertEquals(42L, cursor.getLong(0))
            }
            db.rawQuery(
                "SELECT body_weight_kg,waist_cm FROM body_observations " +
                    "WHERE id=12 AND observation_id='bo_zone_preserved';",
                null,
            ).use { cursor ->
                assertTrue(cursor.moveToFirst())
                assertEquals(81.5, cursor.getDouble(0), 0.0)
                assertEquals(91.0, cursor.getDouble(1), 0.0)
            }
            db.rawQuery(
                "SELECT label_name,display_name,equipment_type,load_semantics FROM equipment " +
                    "WHERE id=999 AND equipment_id='eq_44444444-4444-4444-8444-444444444444';",
                null,
            ).use { cursor ->
                assertTrue(cursor.moveToFirst())
                assertEquals("Machine custom", cursor.getString(0))
                assertEquals("Machine custom", cursor.getString(1))
                assertEquals("strength_machine", cursor.getString(2))
                assertEquals("external", cursor.getString(3))
            }
            db.rawQuery("PRAGMA integrity_check", null).use { cursor ->
                assertTrue(cursor.moveToFirst()); assertEquals("ok", cursor.getString(0))
            }
            db.rawQuery("PRAGMA foreign_key_check", null).use { assertFalse(it.moveToFirst()) }
        }
    }
}
