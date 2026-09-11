package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.util.UUID

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class StatisticsRepositoryTest {
    private val context: Context = ApplicationProvider.getApplicationContext()
    private val databaseName = "statistics-${UUID.randomUUID()}.db"
    private val repository = TrainlogRepository(context, databaseName)

    @After fun close() { repository.close(); context.deleteDatabase(databaseName) }

    @Test fun exactTimeWindowsAndConservativeSeriesSemantics() {
        repository.listEquipment()
        SQLiteDatabase.openDatabase(
            context.getDatabasePath(databaseName).path, null, SQLiteDatabase.OPEN_READWRITE,
        ).use { db ->
            db.execSQL("PRAGMA foreign_keys=ON")
            db.execSQL(
                "INSERT INTO exercises(exercise_id,name,normalized_name,recording_mode,tracking_mode,data_fields) VALUES(?, 'Presse canonique', 'presse canonique', 'sets', 'reps', 0)",
                arrayOf(CANONICAL_ID),
            )
            /* INVARIANT: statistics see the flattened post-merge identity, never
             * resurrect the retired creator ID as a second series. */
            db.execSQL(
                "INSERT INTO exercise_aliases(source_exercise_id,canonical_exercise_id) VALUES(?,?)",
                arrayOf(RETIRED_ID, CANONICAL_ID),
            )

            session(db, "outside", "2026-09-05T02:59:59.99999999999999999999+15:00")
            session(db, "boundary", "2026-09-05T02:30:00+14:30")
            session(db, "fraction-low", "2026-09-06t10:00:00.12345678901234567890z")
            session(db, "fraction-high", "2026-09-06T12:00:00.12345678901234567891+02:00")
            session(db, "equipment", "2026-09-07T10:00:00Z")
            session(db, "assistance", "2026-09-08T10:00:00Z")
            session(db, "target-only", "2026-09-09T10:00:00Z")
            session(db, "maximum", "2026-09-10T10:00:00Z", "max_test")

            occurrence(db, "outside", "out", "leg_press", "external", 10.0)
            occurrence(db, "boundary", "boundary", "leg_press", "external", 20.0)
            occurrence(db, "fraction-low", "low", "leg_press", "external", 30.0)
            occurrence(db, "fraction-high", "high", "leg_press", "external", 40.0)
            occurrence(db, "equipment", "other-equipment", "plate_loaded_leg_press", "external", 50.0)
            occurrence(db, "assistance", "assist", "assisted_dip_chin_machine", "assistance", 60.0, maximum = 160.0)
            occurrence(db, "target-only", "target", "leg_press", "external", null, target = 999.0)
            occurrence(db, "maximum", "max", "leg_press", "external", null, maximum = 100.0)

            db.execSQL("INSERT INTO body_observations(observation_id,observed_at,body_weight_kg) VALUES('before-body','2026-09-04T11:59:59Z',70.0)")
            db.execSQL("INSERT INTO body_observations(observation_id,observed_at,body_weight_kg) VALUES('body-weight','2026-09-05t14:00:00+02:00',71.0)")
            db.execSQL("INSERT INTO body_observations(observation_id,observed_at,waist_cm) VALUES('body-waist','2026-09-06T10:00:00Z',80.0)")
        }

        val now = TrainlogTimestamp.parse("2026-09-11T12:00:00Z")!!
        val overview = repository.loadStatistics(StatisticsPeriod.DAYS_7, now)
        assertEquals(StatisticsPeriod.DAYS_7, overview.period)
        assertEquals(StatisticsSummary(6, 5, 1, 2), overview.summary)
        assertEquals(6, overview.sessionsLast7Days)
        assertEquals(7, overview.sessionsLast30Days)

        val work = overview.performance.filter { it.id.startsWith("work:") }
        val maxima = overview.performance.filter { it.id.startsWith("max:") }
        assertEquals(2, work.size)
        assertEquals(1, maxima.size)
        assertEquals(listOf(20.0, 30.0, 40.0), work.single { ":leg_press:reps:5" in it.id }.points.map { it.value })
        assertEquals(listOf(50.0), work.single { ":plate_loaded_leg_press:reps:5" in it.id }.points.map { it.value })
        assertEquals(listOf(100.0), maxima.single().points.map { it.value })
        assertTrue(overview.performance.all { CANONICAL_ID in it.id })
        assertFalse(overview.performance.any { RETIRED_ID in it.id || it.points.any { point -> point.value == 60.0 || point.value == 999.0 || point.value == 160.0 } })

        assertEquals(listOf(71.0), overview.body.single { it.id == "body_weight_kg" }.points.map { it.value })
        assertEquals(listOf(80.0), overview.body.single { it.id == "waist_cm" }.points.map { it.value })
        assertEquals(6, overview.frequency.sumOf { it.all })
        assertEquals(1, overview.frequency.sumOf { it.maxima })
        /* Three later leg-press loads beat the prior best at the same five-rep
         * dose. The one-point other-equipment class and lone MAX are not
         * falsely classified; no kg values are summed into this graph. */
        assertEquals(3, overview.performanceEvents.sumOf { it.workingImprovements })
        assertEquals(0, overview.performanceEvents.sumOf { it.maxImprovements })
    }

    @Test fun nullEndedObservableHistoryRequiresActualOccurrenceOwnedFacts() {
        repository.listEquipment()
        SQLiteDatabase.openDatabase(
            context.getDatabasePath(databaseName).path, null, SQLiteDatabase.OPEN_READWRITE,
        ).use { db ->
            db.execSQL(
                "INSERT INTO exercises(exercise_id,name,normalized_name,recording_mode,tracking_mode,data_fields) VALUES(?, 'Presse réelle', 'presse reelle', 'sets', 'reps', 0)",
                arrayOf(CANONICAL_ID),
            )
            db.execSQL(
                "INSERT INTO exercises(exercise_id,name,normalized_name,recording_mode,tracking_mode,data_fields) VALUES(?, 'Marche réelle', 'marche reelle', 'continuous', 'duration', 1)",
                arrayOf(CONTINUOUS_ID),
            )

            session(db, "A", "2026-09-08T08:00:00Z")
            session(db, "B", "2026-09-09T08:00:00Z", "max_test")
            session(db, "C", "2026-09-10T08:00:00Z")
            session(db, "D", "2026-09-11T08:00:00Z")
            occurrence(db, "A", "actual-set", "leg_press", "external", 42.5)
            /* SQLite accepts this non-finite REAL under the historic affinity
             * boundary; it is an actual row for frequency, never a graph point. */
            db.execSQL(
                "INSERT INTO performed_sets(session_exercise_row_id,position,reps,weight_kg) SELECT id,1,5,1e999 FROM session_exercises WHERE entry_id='actual-set'",
            )
            occurrence(db, "B", "actual-max", "leg_press", "external", null, maximum = 120.0)
            db.execSQL(
                """INSERT INTO session_exercises(session_row_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields,entry_id,load_mode)
                   SELECT s.id,e.id,0,'continuous','duration',1,'actual-continuous','none' FROM sessions s JOIN exercises e ON e.exercise_id=? WHERE s.session_id='C'""".trimIndent(),
                arrayOf(CONTINUOUS_ID),
            )
            db.execSQL(
                "INSERT INTO continuous_activity(session_exercise_row_id,duration_seconds,distance_km) SELECT id,1800,2.5 FROM session_exercises WHERE entry_id='actual-continuous'",
            )
            occurrence(db, "D", "plan-only", "leg_press", "external", null, target = 999.0)
            db.execSQL(
                "INSERT INTO body_observations(observation_id,observed_at,body_weight_kg) VALUES('body-real','2026-09-10T09:00:00Z',72.0)",
            )
        }

        assertEquals(setOf("A", "B", "C"), repository.listSessions().map { it.sessionId }.toSet())
        val overview = repository.loadStatistics(
            StatisticsPeriod.ALL,
            TrainlogTimestamp.parse("2026-09-11T12:00:00Z")!!,
        )
        assertEquals(3, overview.sessionsLast7Days)
        assertEquals(StatisticsSummary(3, 2, 2, 1), overview.summary)
        assertEquals(3, overview.frequency.sumOf { it.all })
        assertEquals(1, overview.frequency.sumOf { it.maxima })
        assertEquals(listOf(42.5), overview.performance.single { it.id.startsWith("work:") }.points.map { it.value })
        assertEquals(listOf(120.0), overview.performance.single { it.id.startsWith("max:") }.points.map { it.value })
        assertEquals(listOf(72.0), overview.body.single { it.id == "body_weight_kg" }.points.map { it.value })
        assertTrue(overview.performanceEvents.isEmpty())
    }

    @Test fun maximumStatisticsRequireResolvableExternalEquipment() {
        repository.listEquipment()
        SQLiteDatabase.openDatabase(
            context.getDatabasePath(databaseName).path, null, SQLiteDatabase.OPEN_READWRITE,
        ).use { db ->
            db.execSQL(
                "INSERT INTO exercises(exercise_id,name,normalized_name,recording_mode,tracking_mode,data_fields) VALUES(?, 'Presse MAX', 'presse max', 'sets', 'reps', 0)",
                arrayOf(CANONICAL_ID),
            )
            session(db, "external-first", "2026-09-08T08:00:00Z", "max_test")
            session(db, "external-improved", "2026-09-09T08:00:00Z", "max_test")
            session(db, "missing-equipment-first", "2026-09-08T09:00:00Z", "max_test")
            session(db, "missing-equipment-improved", "2026-09-09T09:00:00Z", "max_test")
            occurrence(db, "external-first", "external-first", "leg_press", "external", null, maximum = 100.0)
            occurrence(db, "external-improved", "external-improved", "leg_press", "external", null, maximum = 110.0)
            occurrenceWithoutEquipment(db, "missing-equipment-first", "missing-equipment-first", 120.0)
            occurrenceWithoutEquipment(db, "missing-equipment-improved", "missing-equipment-improved", 130.0)
        }

        val overview = repository.loadStatistics(
            StatisticsPeriod.ALL,
            TrainlogTimestamp.parse("2026-09-11T12:00:00Z")!!,
        )

        val maxima = overview.performance.filter { it.id.startsWith("max:") }
        assertEquals(1, maxima.size)
        assertEquals(listOf(100.0, 110.0), maxima.single().points.map { it.value })
        assertFalse(overview.performance.any { it.id == "max:$CANONICAL_ID:" })
        assertEquals(1, overview.performanceEvents.sumOf { it.maxImprovements })
    }

    @Test fun emptyExternalEquipmentDoesNotCreateComparableStatistics() {
        repository.listEquipment()
        SQLiteDatabase.openDatabase(
            context.getDatabasePath(databaseName).path, null, SQLiteDatabase.OPEN_READWRITE,
        ).use { db ->
            db.execSQL(
                "INSERT INTO exercises(exercise_id,name,normalized_name,recording_mode,tracking_mode,data_fields) VALUES(?, 'Presse équipement vide', 'presse equipement vide', 'sets', 'reps', 0)",
                arrayOf(CANONICAL_ID),
            )
            db.execSQL(
                "INSERT INTO equipment(equipment_id,label_name,display_name,equipment_type,load_semantics) VALUES('', '', 'Équipement vide', 'custom_machine', 'external')",
            )
            session(db, "empty-work-first", "2026-09-08T08:00:00Z")
            session(db, "empty-work-improved", "2026-09-09T08:00:00Z")
            session(db, "empty-max-first", "2026-09-08T09:00:00Z", "max_test")
            session(db, "empty-max-improved", "2026-09-09T09:00:00Z", "max_test")
            occurrence(db, "empty-work-first", "empty-work-first", "", "external", 100.0)
            occurrence(db, "empty-work-improved", "empty-work-improved", "", "external", 110.0)
            occurrence(db, "empty-max-first", "empty-max-first", "", "external", null, maximum = 120.0)
            occurrence(db, "empty-max-improved", "empty-max-improved", "", "external", null, maximum = 130.0)
        }

        val overview = repository.loadStatistics(
            StatisticsPeriod.ALL,
            TrainlogTimestamp.parse("2026-09-11T12:00:00Z")!!,
        )

        assertTrue(overview.performance.isEmpty())
        assertTrue(overview.performanceEvents.isEmpty())
    }

    @Test fun frequencyUsesEachTimestampRepresentedLocalIsoWeek() {
        repository.listEquipment()
        SQLiteDatabase.openDatabase(
            context.getDatabasePath(databaseName).path, null, SQLiteDatabase.OPEN_READWRITE,
        ).use { db ->
            db.execSQL(
                "INSERT INTO exercises(exercise_id,name,normalized_name,recording_mode,tracking_mode,data_fields) VALUES(?, 'Presse semaine', 'presse semaine', 'sets', 'reps', 0)",
                arrayOf(CANONICAL_ID),
            )
            /* The first two rows are the same instant on opposite sides of a
             * local Monday boundary. The final row is locally Sunday even
             * though its canonical instant is already Monday. */
            session(db, "local-monday", "2025-12-29T00:30:00+02:00")
            session(db, "utc-sunday", "2025-12-28T22:30:00Z")
            session(db, "iso-year-monday", "2024-12-30T00:30:00+14:00")
            session(db, "local-sunday", "2026-01-04T23:30:00-02:00")
            occurrence(db, "local-monday", "week-a", "leg_press", "external", 10.0)
            occurrence(db, "utc-sunday", "week-b", "leg_press", "external", 20.0)
            occurrence(db, "iso-year-monday", "week-c", "leg_press", "external", 30.0)
            occurrence(db, "local-sunday", "week-d", "leg_press", "external", 40.0)
        }

        val overview = repository.loadStatistics(
            StatisticsPeriod.ALL,
            TrainlogTimestamp.parse("2026-01-05T12:00:00Z")!!,
        )

        assertEquals(
            listOf(
                StatisticsFrequency("2025-W01", 1, 0),
                StatisticsFrequency("2025-W52", 1, 0),
                StatisticsFrequency("2026-W01", 2, 0),
            ),
            overview.frequency,
        )
    }

    @Test fun exactDoseStrictLaterAndMalformedLegacyRowsRemainConservative() {
        repository.listEquipment()
        SQLiteDatabase.openDatabase(
            context.getDatabasePath(databaseName).path, null, SQLiteDatabase.OPEN_READWRITE,
        ).use { db ->
            db.execSQL(
                "INSERT INTO exercises(exercise_id,name,normalized_name,recording_mode,tracking_mode,data_fields) VALUES(?, 'Presse parité', 'presse parite', 'sets', 'reps', 0)",
                arrayOf(CANONICAL_ID),
            )
            session(db, "A", "2026-09-08T08:00:00Z")
            session(db, "B", "2026-09-09T08:00:00Z")
            occurrence(db, "A", "A", "leg_press", "external", 100.0, reps = 5)
            db.execSQL(
                "INSERT INTO performed_sets(session_exercise_row_id,position,reps,weight_kg) SELECT id,1,10,80.0 FROM session_exercises WHERE entry_id='A'",
            )
            occurrence(db, "B", "B", "leg_press", "external", 90.0, reps = 10)

            /* Equal instants use IDs only for presentation order. Neither the
             * working nor MAX pair may manufacture a later observation. */
            session(db, "tie-work-a", "2026-09-10T08:00:00Z")
            session(db, "tie-work-b", "2026-09-10T08:00:00+00:00")
            occurrence(db, "tie-work-a", "tie-work-a", "leg_press", "external", 50.0, reps = 12)
            occurrence(db, "tie-work-b", "tie-work-b", "leg_press", "external", 60.0, reps = 12)
            session(db, "tie-max-a", "2026-09-10T09:00:00Z", "max_test")
            session(db, "tie-max-b", "2026-09-10T10:00:00+01:00", "max_test")
            occurrence(db, "tie-max-a", "tie-max-a", "leg_press", "none", null, maximum = 100.0)
            occurrence(db, "tie-max-b", "tie-max-b", "leg_press", "none", null, maximum = 110.0)

            session(db, "invalid", "legacy-not-a-timestamp")
            occurrence(db, "invalid", "invalid", "leg_press", "external", 999.0, reps = 10)
            db.execSQL("INSERT INTO body_observations(observation_id,observed_at,body_weight_kg) VALUES('valid-body','2026-09-09T12:00:00Z',71.0)")
            db.execSQL("INSERT INTO body_observations(observation_id,observed_at,body_weight_kg) VALUES('invalid-body','bad-body-time',999.0)")
        }

        val overview = repository.loadStatistics(
            StatisticsPeriod.ALL,
            TrainlogTimestamp.parse("2026-09-11T12:00:00Z")!!,
        )
        assertTrue(overview.hasInvalidData)
        assertEquals(1, overview.performanceEvents.sumOf { it.workingImprovements })
        assertEquals(0, overview.performanceEvents.sumOf { it.maxImprovements })
        assertEquals(listOf(80.0, 90.0), overview.performance
            .single { it.id.endsWith(":reps:10") }.points.map { it.value })
        assertEquals(listOf(71.0), overview.body.single().points.map { it.value })
        assertFalse(overview.performance.any { series -> series.points.any { it.value == 999.0 } })
    }

    private fun session(db: SQLiteDatabase, id: String, timestamp: String, type: String = "training") {
        db.execSQL("INSERT INTO sessions(session_id,started_at,session_type) VALUES(?,?,?)", arrayOf(id, timestamp, type))
    }

    private fun occurrence(
        db: SQLiteDatabase, sessionId: String, entryId: String, equipmentId: String,
        loadMode: String, weight: Double?, target: Double? = null, maximum: Double? = null,
        reps: Int = 5,
    ) {
        db.execSQL(
            """INSERT INTO session_exercises(session_row_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields,equipment_row_id,entry_id,load_mode,target_sets,target_weight_kg)
               SELECT s.id,e.id,0,'sets','reps',0,eq.id,?,?,1,? FROM sessions s JOIN exercises e ON e.exercise_id=? JOIN equipment eq ON eq.equipment_id=? WHERE s.session_id=?""".trimIndent(),
            arrayOf<Any?>(entryId, loadMode, target, CANONICAL_ID, equipmentId, sessionId),
        )
        if (weight != null) db.execSQL(
            "INSERT INTO performed_sets(session_exercise_row_id,position,reps,weight_kg) SELECT id,0,?,? FROM session_exercises WHERE entry_id=?",
            arrayOf<Any?>(reps, weight, entryId),
        )
        if (maximum != null) db.execSQL(
            "INSERT INTO max_results(session_exercise_row_id,max_weight_kg) SELECT id,? FROM session_exercises WHERE entry_id=?",
            arrayOf<Any?>(maximum, entryId),
        )
    }

    private fun occurrenceWithoutEquipment(db: SQLiteDatabase, sessionId: String, entryId: String, maximum: Double) {
        db.execSQL(
            """INSERT INTO session_exercises(session_row_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields,entry_id,load_mode)
               SELECT s.id,e.id,0,'sets','reps',0,?,'external' FROM sessions s JOIN exercises e ON e.exercise_id=? WHERE s.session_id=?""".trimIndent(),
            arrayOf(entryId, CANONICAL_ID, sessionId),
        )
        db.execSQL(
            "INSERT INTO max_results(session_exercise_row_id,max_weight_kg) SELECT id,? FROM session_exercises WHERE entry_id=?",
            arrayOf<Any?>(maximum, entryId),
        )
    }

    companion object {
        private const val CANONICAL_ID = "ex_b432623f-bfe9-4daf-a653-60ec7fdffbde"
        private const val CONTINUOUS_ID = "ex_77777777-7777-4777-8777-777777777777"
        private const val RETIRED_ID = "ex_00000000-0000-4000-8000-000000000099"
    }
}
