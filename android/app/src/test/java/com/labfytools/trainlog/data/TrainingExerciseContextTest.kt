package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.ExerciseEditInput
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.util.UUID

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class TrainingExerciseContextTest {
    private val context: Context = ApplicationProvider.getApplicationContext()
    private val databaseName = "knowledge-context-${UUID.randomUUID()}.db"
    private val repository = TrainlogRepository(context, databaseName)

    @After fun close() { repository.close(); context.deleteDatabase(databaseName) }

    @Test
    fun composesExactIdentityZonesEquipmentMaxAndBoundedOccurrencePages() {
        seedFixture()
        val exerciseId = LEG_PRESS_ID
        val first = repository.getTrainingExerciseContext(exerciseId, occurrenceLimit = 2, setPreviewLimit = 2)!!
        assertEquals("Nom modifiable", first.exercise.name)
        assertEquals(listOf("thighs", "glutes"), first.persistedDirectZoneIds)
        assertTrue("lower_body" in first.persistedZoneIdsWithAncestors)
        assertEquals("thighs", repository.getScientificBodyZoneMapping(exerciseId)?.primaryZoneId)
        assertTrue(first.compatibleEquipment.any { it.equipmentId == "leg_press" })

        val maximum = first.latestExplicitMax!!
        assertEquals(140.0, maximum.maxWeightKg, 0.0)
        assertEquals(EquipmentLoadSemantics.ASSISTANCE, maximum.loadSemantics)
        assertFalse(first.recentPerformance.occurrences.any { occurrence ->
            occurrence.setPreview?.sets.orEmpty().any { it.weightKg == 999.0 } && occurrence.sessionType == com.labfytools.trainlog.model.SessionType.MAX_TEST
        })
        assertEquals(2, first.recentPerformance.occurrences.size)
        val cursor = first.recentPerformance.nextCursor!!
        val second = repository.listExerciseOccurrences(exerciseId, 2, cursor, 2)
        assertEquals(2, second.occurrences.size)
        assertTrue(first.recentPerformance.occurrences.map { it.entryId }.toSet().intersect(second.occurrences.map { it.entryId }.toSet()).isEmpty())

        val assistanceOccurrence = (first.recentPerformance.occurrences + second.occurrences).first { it.entryId == "entry_a" }
        assertEquals(EquipmentLoadSemantics.ASSISTANCE, assistanceOccurrence.loadSemantics)
        assertEquals(listOf(null, 0.0), assistanceOccurrence.setPreview!!.sets.map { it.weightKg })
        val setPage2 = repository.listExerciseOccurrenceSets(exerciseId, "entry_a", 2, assistanceOccurrence.setPreview.nextPosition)
        assertEquals(listOf(22.5), setPage2.sets.map { it.weightKg })
        assertNull(setPage2.nextPosition)

        assertTrue(repository.getExerciseKnowledge(exerciseId) != null)
        assertNull(repository.getTrainingExerciseContext("ex_00000000-0000-4000-8000-000000000000"))
    }

    @Test
    fun retiredAliasResolvesToCanonicalV2EquipmentContextExactlyOnce() {
        seedFixture()
        SQLiteDatabase.openDatabase(context.getDatabasePath(databaseName).path, null, SQLiteDatabase.OPEN_READWRITE).use { db ->
            db.execSQL(
                "INSERT INTO exercise_aliases(source_exercise_id,canonical_exercise_id) VALUES(?,?);",
                arrayOf(RETIRED_LEG_PRESS_ID, LEG_PRESS_ID),
            )
        }

        val context = repository.getTrainingExerciseContext(RETIRED_LEG_PRESS_ID)!!

        assertEquals(LEG_PRESS_ID, context.exercise.exerciseId)
        assertEquals(LEG_PRESS_ID, context.knowledge!!.exerciseId)
        assertEquals(1, context.compatibleEquipment.count { it.equipmentId == "leg_press" })
    }

    @Test
    fun continuousOccurrenceHasNoArtificialSetsAndInvalidPagingFails() {
        seedFixture()
        val continuous = repository.getTrainingExerciseContext(CONTINUOUS_ID)!!.recentPerformance.occurrences.single()
        assertNull(continuous.setPreview)
        assertEquals(1800, continuous.continuousDurationSeconds)
        assertEquals(8.0, continuous.speedKmh!!, 0.0)
        assertTrue(runCatching { repository.listExerciseOccurrences(LEG_PRESS_ID, 0) }.isFailure)
        assertTrue(runCatching { repository.listExerciseOccurrences(LEG_PRESS_ID, 2, ExerciseOccurrenceCursor("bad", "s", "e")) }.isFailure)
        assertTrue(runCatching { repository.listExerciseOccurrenceSets(CONTINUOUS_ID, "entry_cont", 2) }.isFailure)
    }

    @Test
    fun occurrencesAndExplicitMaxOrderByInstantAcrossOffsetsWithStablePagingTies() {
        seedFixture()
        SQLiteDatabase.openDatabase(context.getDatabasePath(databaseName).path, null, SQLiteDatabase.OPEN_READWRITE).use { db ->
            listOf(
                arrayOf<Any?>("offset_local_older", "2026-02-01T10:00:00+02:00", "training", "entry_offset_old"),
                arrayOf<Any?>("offset_utc_newer", "2026-02-01T09:00:00Z", "training", "entry_offset_new"),
                arrayOf<Any?>("tie_a", "2026-02-02T10:00:00+02:00", "training", "entry_tie_a"),
                arrayOf<Any?>("tie_z", "2026-02-02T08:00:00Z", "training", "entry_tie_z"),
                arrayOf<Any?>("max_local_older", "2025-03-01T10:00:00+02:00", "max_test", "entry_max_old"),
                arrayOf<Any?>("max_utc_newer", "2025-03-01T09:00:00Z", "max_test", "entry_max_new"),
            ).forEach { row ->
                db.execSQL("INSERT INTO sessions(session_id,started_at,session_type) VALUES(?,?,?);", row.copyOfRange(0, 3))
                db.execSQL("INSERT INTO session_exercises(session_row_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields,entry_id) SELECT s.id,e.id,0,'sets','reps',0,? FROM sessions s JOIN exercises e ON e.exercise_id=? WHERE s.session_id=?;", arrayOf(row[3], LEG_PRESS_ID, row[0]))
            }
            db.execSQL("INSERT INTO max_results(session_exercise_row_id,max_weight_kg) SELECT id,150.0 FROM session_exercises WHERE entry_id='entry_max_old';")
            db.execSQL("INSERT INTO max_results(session_exercise_row_id,max_weight_kg) SELECT id,160.0 FROM session_exercises WHERE entry_id='entry_max_new';")
        }

        val tieFirst = repository.listExerciseOccurrences(LEG_PRESS_ID, 1)
        assertEquals("tie_z", tieFirst.occurrences.single().sessionId)
        val tieSecond = repository.listExerciseOccurrences(LEG_PRESS_ID, 1, tieFirst.nextCursor)
        assertEquals("tie_a", tieSecond.occurrences.single().sessionId)
        val following = repository.listExerciseOccurrences(LEG_PRESS_ID, 2, tieSecond.nextCursor)
        assertEquals(listOf("offset_utc_newer", "offset_local_older"), following.occurrences.map { it.sessionId })

        SQLiteDatabase.openDatabase(context.getDatabasePath(databaseName).path, null, SQLiteDatabase.OPEN_READWRITE).use { db ->
            db.execSQL("UPDATE sessions SET started_at='2026-03-01T10:00:00+02:00' WHERE session_id='max_local_older';")
            db.execSQL("UPDATE sessions SET started_at='2026-03-01T09:00:00Z' WHERE session_id='max_utc_newer';")
        }
        val maximum = repository.getTrainingExerciseContext(LEG_PRESS_ID)!!.latestExplicitMax!!
        assertEquals("max_utc_newer", maximum.sessionId)
        assertEquals(160.0, maximum.maxWeightKg, 0.0)
    }

    @Test
    fun exceptionalTimestampSpellingsRoundTripWithExactFractionsAndCorruptionFails() {
        seedFixture()
        SQLiteDatabase.openDatabase(context.getDatabasePath(databaseName).path, null, SQLiteDatabase.OPEN_READWRITE).use { db ->
            listOf(
                arrayOf("omit", "2030-01-05T10:00+15:00", "entry_omit"),
                arrayOf("frac_low", "2030-01-04T10:00:00.12345678901234567890Z", "entry_frac_low"),
                arrayOf("frac_high", "2030-01-04T10:00:00.12345678901234567891Z", "entry_frac_high"),
                arrayOf("tie_a", "2030-01-04T12:00:00+02:00", "entry_tie_a2"),
                arrayOf("tie_z", "2030-01-04T10:00:00-00:00", "entry_tie_z2"),
                arrayOf("lower", "2030-01-03t10:00:00z", "entry_lower"),
                arrayOf("high_offset", "2030-01-04T09:00:00+23:59", "entry_high_offset"),
            ).forEach { row ->
                db.execSQL("INSERT INTO sessions(session_id,started_at,session_type) VALUES(?,?,'training');", arrayOf(row[0], row[1]))
                db.execSQL("INSERT INTO session_exercises(session_row_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields,entry_id) SELECT s.id,e.id,0,'sets','reps',0,? FROM sessions s JOIN exercises e ON e.exercise_id=? WHERE s.session_id=?;", arrayOf(row[2], LEG_PRESS_ID, row[0]))
            }
            listOf(
                arrayOf<Any?>("max_offset", "2031-01-02T10:00:00+15:00", "entry_max_offset", 151.0),
                arrayOf<Any?>("max_true", "2031-01-01T20:00:00Z", "entry_max_true", 152.0),
            ).forEach { row ->
                db.execSQL("INSERT INTO sessions(session_id,started_at,session_type) VALUES(?,?,'max_test');", arrayOf(row[0], row[1]))
                db.execSQL("INSERT INTO session_exercises(session_row_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields,entry_id) SELECT s.id,e.id,0,'sets','reps',0,? FROM sessions s JOIN exercises e ON e.exercise_id=? WHERE s.session_id=?;", arrayOf(row[2], LEG_PRESS_ID, row[0]))
                db.execSQL("INSERT INTO max_results(session_exercise_row_id,max_weight_kg) SELECT id,? FROM session_exercises WHERE entry_id=?;", arrayOf(row[3], row[2]))
            }
        }

        val expected = listOf("max_true", "max_offset", "omit", "frac_high", "frac_low", "tie_z", "tie_a", "lower", "high_offset")
        var cursor: ExerciseOccurrenceCursor? = null
        val seen = mutableSetOf<String>()
        expected.forEach { sessionId ->
            val page = repository.listExerciseOccurrences(LEG_PRESS_ID, 1, cursor)
            assertEquals(sessionId, page.occurrences.single().sessionId)
            assertTrue(seen.add(page.occurrences.single().entryId))
            assertEquals(page.occurrences.single().startedAt, page.nextCursor!!.startedAt)
            cursor = page.nextCursor
        }
        while (cursor != null) {
            val page = repository.listExerciseOccurrences(LEG_PRESS_ID, 1, cursor)
            page.occurrences.forEach { assertTrue(seen.add(it.entryId)) }
            cursor = page.nextCursor
        }
        val maximum = repository.getTrainingExerciseContext(LEG_PRESS_ID)!!.latestExplicitMax!!
        assertEquals("max_true", maximum.sessionId)
        assertEquals(152.0, maximum.maxWeightKg, 0.0)

        SQLiteDatabase.openDatabase(context.getDatabasePath(databaseName).path, null, SQLiteDatabase.OPEN_READWRITE).use { db ->
            db.execSQL("UPDATE sessions SET started_at='2030-01-03 10:00:00Z' WHERE session_id='lower';")
        }
        assertTrue(runCatching { repository.listExerciseOccurrences(LEG_PRESS_ID, 1) }.isFailure)
        SQLiteDatabase.openDatabase(context.getDatabasePath(databaseName).path, null, SQLiteDatabase.OPEN_READWRITE).use { db ->
            db.execSQL("UPDATE sessions SET started_at='2030-01-03T10:00:00Z' WHERE session_id='lower';")
            db.execSQL("UPDATE sessions SET started_at='bad-max' WHERE session_id='max_offset';")
        }
        assertTrue(runCatching { repository.getTrainingExerciseContext(LEG_PRESS_ID) }.isFailure)
    }

    private fun seedFixture() {
        repository.listExercises()
        SQLiteDatabase.openDatabase(context.getDatabasePath(databaseName).path, null, SQLiteDatabase.OPEN_READWRITE).use { db ->
            db.execSQL("PRAGMA foreign_keys=ON;")
            db.execSQL("INSERT INTO exercises(exercise_id,name,normalized_name,recording_mode,tracking_mode,data_fields) VALUES(?,?,?,?,?,?);", arrayOf<Any?>(LEG_PRESS_ID, "Nom original", "nom original", "sets", "reps", 0))
            db.execSQL("INSERT INTO exercises(exercise_id,name,normalized_name,recording_mode,tracking_mode,data_fields) VALUES(?,?,?,?,?,?);", arrayOf<Any?>(CONTINUOUS_ID, "Marche test", "marche test", "continuous", "duration", 3))
            db.execSQL("INSERT INTO exercise_body_zones(exercise_row_id,zone_id,role) SELECT id,'thighs','primary' FROM exercises WHERE exercise_id=?;", arrayOf(LEG_PRESS_ID))
            db.execSQL("INSERT INTO exercise_body_zones(exercise_row_id,zone_id,role) SELECT id,'glutes','secondary' FROM exercises WHERE exercise_id=?;", arrayOf(LEG_PRESS_ID))
            listOf(
                arrayOf<Any?>("session_latest", "2026-01-04T10:00:00+00:00", "training"),
                arrayOf<Any?>("session_tied", "2026-01-03T10:00:00+00:00", "training"),
                arrayOf<Any?>("session_max", "2026-01-02T10:00:00+00:00", "max_test"),
                arrayOf<Any?>("session_old", "2026-01-01T10:00:00+00:00", "training"),
                arrayOf<Any?>("session_cont", "2026-01-05T10:00:00+00:00", "training"),
            ).forEach { db.execSQL("INSERT INTO sessions(session_id,started_at,session_type) VALUES(?,?,?);", it) }
            fun occurrence(session: String, entry: String, position: Int, equipment: String? = null): Long {
                db.execSQL(
                    "INSERT INTO session_exercises(session_row_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields,equipment_row_id,entry_id) SELECT s.id,e.id,?,'sets','reps',0,eq.id,? FROM sessions s JOIN exercises e ON e.exercise_id=? LEFT JOIN equipment eq ON eq.equipment_id=? WHERE s.session_id=?;",
                    arrayOf<Any?>(position, entry, LEG_PRESS_ID, equipment, session),
                )
                return db.rawQuery("SELECT id FROM session_exercises WHERE entry_id=?;", arrayOf(entry)).use { it.moveToFirst(); it.getLong(0) }
            }
            val latest = occurrence("session_latest", "entry_latest", 0, "leg_press")
            db.execSQL("INSERT INTO performed_sets(session_exercise_row_id,position,reps,weight_kg) VALUES(?,0,3,999.0);", arrayOf(latest))
            val a = occurrence("session_tied", "entry_a", 0, "assisted_dip_chin_machine")
            db.execSQL("INSERT INTO performed_sets(session_exercise_row_id,position,reps,weight_kg) VALUES(?,0,8,NULL),(?,1,7,0.0),(?,2,6,22.5);", arrayOf(a, a, a))
            occurrence("session_tied", "entry_b", 1, "plate_loaded_leg_press")
            val max = occurrence("session_max", "entry_max", 0, "assisted_dip_chin_machine")
            db.execSQL("INSERT INTO max_results(session_exercise_row_id,max_weight_kg) VALUES(?,140.0);", arrayOf(max))
            occurrence("session_old", "entry_old", 0)
            db.execSQL("INSERT INTO session_exercises(session_row_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields,equipment_row_id,entry_id) SELECT s.id,e.id,0,'continuous','duration',3,eq.id,'entry_cont' FROM sessions s JOIN exercises e ON e.exercise_id=? LEFT JOIN equipment eq ON eq.equipment_id='treadmill' WHERE s.session_id='session_cont';", arrayOf(CONTINUOUS_ID))
            db.execSQL("INSERT INTO continuous_activity(session_exercise_row_id,duration_seconds,speed_kmh,distance_km) SELECT id,1800,8.0,4.0 FROM session_exercises WHERE entry_id='entry_cont';")
        }
        val profile = repository.listExercises().first { it.exerciseId == LEG_PRESS_ID }
        val result = repository.editExercise(ExerciseEditInput(profile.exerciseId, "Nom modifiable", profile.recordingMode, profile.trackingMode, profile.dataFields, profile.primaryZoneId, profile.secondaryZoneIds))
        assertTrue(result is EditExerciseResult.Saved)
    }

    companion object {
        private const val LEG_PRESS_ID = "ex_b432623f-bfe9-4daf-a653-60ec7fdffbde"
        private const val CONTINUOUS_ID = "ex_00000000-0000-4000-8000-000000000001"
        private const val RETIRED_LEG_PRESS_ID = "ex_00000000-0000-4000-8000-000000000099"
    }
}
