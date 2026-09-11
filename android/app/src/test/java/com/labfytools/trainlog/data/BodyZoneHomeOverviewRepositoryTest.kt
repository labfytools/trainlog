package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
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
class BodyZoneHomeOverviewRepositoryTest {
    private val context: Context = ApplicationProvider.getApplicationContext()
    private val databaseName = "body-focus-${UUID.randomUUID()}.db"
    private val repository = TrainlogRepository(context, databaseName)

    @After fun close() { repository.close(); context.deleteDatabase(databaseName) }

    @Test fun suppliedRankingSecondaryModifierAndUnsupportedZone() {
        editDb { db ->
            clearMappings(db)
            exercise(db, CHEST, "Pectoraux test", "chest")
            exercise(db, BACK, "Dos test", "back", listOf("shoulders"))
            exercise(db, SHOULDERS, "Épaules test", "shoulders")
            exercise(db, THIGHS, "Cuisses test", "thighs")
            work(db, "chest-old", "2026-09-01T12:00:00Z", CHEST)
            work(db, "shoulders-old", "2026-09-03T12:00:00Z", SHOULDERS)
            work(db, "thighs-recent", "2026-09-08T12:00:00Z", THIGHS)
            work(db, "back-recent", "2026-09-10T12:00:00Z", BACK)
        }

        val result = repository.getBodyZoneHomeOverview(now())
        assertEquals(listOf("chest", "shoulders", "thighs"), result.recommendations.map { it.zoneId })
        assertTrue(result.recommendations.take(2).map { it.zoneId }.containsAll(listOf("chest", "shoulders")))
        assertFalse(result.recommendations.any { it.zoneId == "back" || it.zoneId == "calves" })
        val shoulders = result.zones.single { it.zoneId == "shoulders" }
        assertEquals(0, shoulders.primaryWork7Days)
        assertEquals(1, shoulders.secondaryWork7Days)
        assertEquals("2026-09-03T12:00:00Z", shoulders.lastPrimaryExposure)
        assertEquals("2026-09-10T12:00:00Z", shoulders.lastSecondaryExposure)
        assertEquals(BodyZoneHomeState.UNSUPPORTED, result.zones.single { it.zoneId == "calves" }.state)
        assertEquals(listOf("chest", "back", "shoulders", "arms", "core", "glutes", "thighs", "calves"), result.zones.map { it.zoneId })
    }

    @Test fun aliasHistoryCountsOnceAndParentRelationsNeverEnterOverview() {
        editDb { db ->
            clearMappings(db)
            exercise(db, CANONICAL, "Canonique", "chest")
            exercise(db, RETIRED, "Ancien", null)
            db.execSQL(
                "INSERT INTO exercise_aliases(source_exercise_id,canonical_exercise_id) VALUES(?,?)",
                arrayOf(RETIRED, CANONICAL),
            )
            // A corrupt grouping relation is impossible through production APIs;
            // this fixture proves the read boundary still cannot aggregate it.
            val canonicalRow = rowId(db, CANONICAL)
            db.execSQL("INSERT INTO exercise_body_zones(exercise_row_id,zone_id,role) VALUES(?,'upper_body','secondary')", arrayOf(canonicalRow))
            work(db, "retired-work", "2026-09-10T12:00:00Z", RETIRED)
        }

        val result = repository.getBodyZoneHomeOverview(now())
        val chest = result.zones.single { it.zoneId == "chest" }
        assertEquals(1, chest.primaryWork7Days)
        assertEquals(1, chest.primaryWork30Days)
        assertEquals(1, chest.recentSessionCount)
        assertEquals(1, chest.availableExerciseCount)
        assertFalse(result.zones.any { it.zoneId == "upper_body" || it.zoneId == "lower_body" || it.zoneId == "full_body" })
    }

    @Test fun emptyHistoryIsHonestAndOnlySupportedZonesAreSuggested() {
        editDb { db ->
            clearMappings(db)
            exercise(db, CHEST, "Pectoraux test", "chest")
            exercise(db, BACK, "Dos test", "back")
        }
        val result = repository.getBodyZoneHomeOverview(now())
        assertFalse(result.hasTrainingHistory)
        assertEquals(listOf("back", "chest"), result.recommendations.map { it.zoneId })
        assertTrue(result.recommendations.all { it.availableExerciseCount > 0 })
        assertNull(result.zones.single { it.zoneId == "chest" }.lastPrimaryExposure)
        assertEquals(BodyZoneHomeState.UNSUPPORTED, result.zones.single { it.zoneId == "calves" }.state)
    }

    @Test fun exactRollingInstantsAndTargetsOrDraftsDoNotQualify() {
        editDb { db ->
            clearMappings(db)
            exercise(db, CHEST, "Pectoraux test", "chest")
            work(db, "boundary", "2026-09-04T14:00:00+02:00", CHEST)
            work(db, "outside", "2026-09-04T11:59:59.999999999Z", CHEST)
            continuousWork(db, "continuous", "2026-09-10T09:00:00Z", CHEST)
            maximumWork(db, "maximum", "2026-09-10T10:00:00Z", CHEST)
            val session = session(db, "target", "2026-09-11T11:00:00Z")
            db.execSQL(
                """INSERT INTO session_exercises(session_row_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields,entry_id,target_sets)
                   VALUES(?,?,0,'sets','reps',0,'target-only',9)""".trimIndent(),
                arrayOf(session, rowId(db, CHEST)),
            )
        }
        val result = repository.getBodyZoneHomeOverview(now())
        val chest = result.zones.single { it.zoneId == "chest" }
        assertEquals(3, chest.primaryWork7Days)
        assertEquals(4, chest.primaryWork30Days)
        assertEquals(4, chest.recentSessionCount)
    }

    private fun now() = TrainlogTimestamp.parse("2026-09-11T12:00:00Z")!!

    private fun editDb(block: (SQLiteDatabase) -> Unit) {
        repository.listExercises()
        SQLiteDatabase.openDatabase(context.getDatabasePath(databaseName).path, null, SQLiteDatabase.OPEN_READWRITE).use { db ->
            db.execSQL("PRAGMA foreign_keys=ON")
            block(db)
        }
    }

    private fun clearMappings(db: SQLiteDatabase) {
        db.execSQL("DELETE FROM exercise_body_zones")
        db.execSQL("DELETE FROM exercise_aliases")
    }

    private fun exercise(db: SQLiteDatabase, id: String, name: String, primary: String?, secondary: List<String> = emptyList()) {
        db.execSQL(
            "INSERT INTO exercises(exercise_id,name,normalized_name,recording_mode,tracking_mode,data_fields) VALUES(?,?,?,'sets','reps',0)",
            arrayOf(id, name, name.lowercase()),
        )
        val row = rowId(db, id)
        primary?.let { db.execSQL("INSERT INTO exercise_body_zones(exercise_row_id,zone_id,role) VALUES(?,?,'primary')", arrayOf<Any>(row, it)) }
        secondary.forEach { db.execSQL("INSERT INTO exercise_body_zones(exercise_row_id,zone_id,role) VALUES(?,?,'secondary')", arrayOf<Any>(row, it)) }
    }

    private fun work(db: SQLiteDatabase, id: String, at: String, exerciseId: String) {
        val session = session(db, id, at)
        db.execSQL(
            """INSERT INTO session_exercises(session_row_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields,entry_id)
               VALUES(?,?,0,'sets','reps',0,?)""".trimIndent(),
            arrayOf<Any>(session, rowId(db, exerciseId), "entry-$id"),
        )
        db.execSQL(
            "INSERT INTO performed_sets(session_exercise_row_id,position,reps) SELECT id,0,8 FROM session_exercises WHERE entry_id=?",
            arrayOf("entry-$id"),
        )
    }

    private fun continuousWork(db: SQLiteDatabase, id: String, at: String, exerciseId: String) {
        val session = session(db, id, at)
        db.execSQL(
            """INSERT INTO session_exercises(session_row_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields,entry_id)
               VALUES(?,?,0,'continuous','duration',0,?)""".trimIndent(),
            arrayOf<Any>(session, rowId(db, exerciseId), "entry-$id"),
        )
        db.execSQL("INSERT INTO continuous_activity(session_exercise_row_id,duration_seconds) SELECT id,600 FROM session_exercises WHERE entry_id=?", arrayOf("entry-$id"))
    }

    private fun maximumWork(db: SQLiteDatabase, id: String, at: String, exerciseId: String) {
        val session = session(db, id, at)
        db.execSQL(
            """INSERT INTO session_exercises(session_row_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields,entry_id)
               VALUES(?,?,0,'sets','reps',0,?)""".trimIndent(),
            arrayOf<Any>(session, rowId(db, exerciseId), "entry-$id"),
        )
        db.execSQL("INSERT INTO max_results(session_exercise_row_id,max_weight_kg) SELECT id,100.0 FROM session_exercises WHERE entry_id=?", arrayOf("entry-$id"))
    }

    private fun session(db: SQLiteDatabase, id: String, at: String): Long {
        db.execSQL("INSERT INTO sessions(session_id,started_at,session_type) VALUES(?,?,'training')", arrayOf("session-$id", at))
        return db.rawQuery("SELECT id FROM sessions WHERE session_id=?", arrayOf("session-$id")).use { it.moveToFirst(); it.getLong(0) }
    }

    private fun rowId(db: SQLiteDatabase, id: String): Long =
        db.rawQuery("SELECT id FROM exercises WHERE exercise_id=?", arrayOf(id)).use { it.moveToFirst(); it.getLong(0) }

    companion object {
        private const val CHEST = "ex_10000000-0000-4000-8000-000000000001"
        private const val BACK = "ex_10000000-0000-4000-8000-000000000002"
        private const val SHOULDERS = "ex_10000000-0000-4000-8000-000000000003"
        private const val THIGHS = "ex_10000000-0000-4000-8000-000000000004"
        private const val CANONICAL = "ex_10000000-0000-4000-8000-000000000005"
        private const val RETIRED = "ex_10000000-0000-4000-8000-000000000006"
    }
}
