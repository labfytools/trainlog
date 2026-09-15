/*
 * Regression coverage for HistoricalSessionProfileImportTest.
 *
 * Exercises production contracts without owning runtime behavior or persistent formats.
 */
package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.ExerciseDataFields
import com.labfytools.trainlog.model.ExerciseProfile
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.TrackingMode
import java.util.UUID
import org.json.JSONArray
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.annotation.Config
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class HistoricalSessionProfileImportTest {
    private lateinit var context: Context
    private lateinit var databaseName: String
    private lateinit var repository: TrainlogRepository

    @Before fun setUp() {
        context = ApplicationProvider.getApplicationContext()
        databaseName = "historical-profile-${UUID.randomUUID()}.db"
        repository = TrainlogRepository(context, databaseName)
    }

    @After fun tearDown() {
        repository.close()
        context.deleteDatabase(databaseName)
    }

    @Test fun `V2 and V3 import historical profiles independently of current catalog`() {
        for (version in listOf(2, 3)) {
            val same = create("Same $version", RecordingMode.SETS, TrackingMode.REPS, 0)
            val tracking = create("Tracking $version", RecordingMode.SETS, TrackingMode.DURATION, 0)
            val recording = create("Recording $version", RecordingMode.CONTINUOUS, TrackingMode.DURATION, 0)
            val fieldsRemoved = create("Fields removed $version", RecordingMode.CONTINUOUS, TrackingMode.DURATION, 0)
            val fieldsAdded = create("Fields added $version", RecordingMode.CONTINUOUS, TrackingMode.DURATION,
                ExerciseDataFields.SPEED_KMH)
            val cases = listOf(
                Case(same, "sets", "reps", 0, sets = JSONArray().put(JSONObject().put("reps", 8))),
                Case(tracking, "sets", "reps", 0, sets = JSONArray().put(JSONObject().put("reps", 9))),
                Case(recording, "sets", "reps", 0, sets = JSONArray().put(JSONObject().put("reps", 10))),
                Case(fieldsRemoved, "continuous", "duration", ExerciseDataFields.SPEED_KMH,
                    continuous = JSONObject().put("duration_seconds", 600).put("speed_kmh", 8.5)),
                Case(fieldsAdded, "continuous", "duration", 0,
                    continuous = JSONObject().put("duration_seconds", 700)),
            )
            val payload = artifact(version, cases, "se_${version}_historical")
            val revisionsBefore = revisionCount()
            assertEquals(MobileSessionImportResult.Applied(1, 0, 0, 0), import(version, payload))

            val detail = repository.getSessionDetail("se_${version}_historical")!!
            assertEquals(listOf("reps", "reps", "reps", "duration", "duration"),
                detail.exercises.map { it.trackingMode.wireValue })
            assertEquals(listOf("sets", "sets", "sets", "continuous", "continuous"),
                detail.exercises.map { it.recordingMode.wireValue })
            assertEquals(listOf(0, 0, 0, ExerciseDataFields.SPEED_KMH, 0),
                detail.exercises.map { it.dataFields })
            assertEquals(listOf(8, 9, 10), detail.exercises.take(3).map { it.sets.single().reps })
            assertEquals(8.5, detail.exercises[3].speedKmh!!, 0.0)
            assertEquals(null, detail.exercises[4].speedKmh)

            /* INVARIANT: importing occurrence history neither edits CURRENT
             * profiles nor manufactures profile revisions. */
            assertEquals(listOf("reps", "duration", "duration", "duration", "duration"),
                cases.map { expected -> repository.listExercises()
                    .single { it.exerciseId == expected.exercise.exerciseId }.trackingMode.wireValue })
            assertEquals(revisionsBefore, revisionCount())
            assertEquals(MobileSessionImportResult.Applied(0, 1, 0, 0), import(version, payload))
            assertEquals(revisionsBefore, revisionCount())
            assertEquals(1, repository.listSessions().count { it.sessionId == "se_${version}_historical" })
        }
    }

    @Test fun `invalid historical payload and unknown identity remain rejected`() {
        val exercise = create("Strict history", RecordingMode.SETS, TrackingMode.DURATION, 0)
        val invalid = Case(exercise, "sets", "reps", 0,
            sets = JSONArray().put(JSONObject().put("duration_seconds", 30)))
        val invalidResult = import(3, artifact(3, listOf(invalid), "se_invalid_history"))
        assertTrue(invalidResult is MobileSessionImportResult.Invalid)
        assertTrue((invalidResult as MobileSessionImportResult.Invalid).message.contains("Série V2 invalide"))

        val unknown = ExerciseProfile(
            "ex_00000000-0000-4000-8000-000000000099", "Unknown", "unknown",
            RecordingMode.SETS, TrackingMode.REPS, 0,
        )
        val unknownResult = import(2, artifact(2, listOf(Case(unknown, "sets", "reps", 0,
            sets = JSONArray().put(JSONObject().put("reps", 1)))), "se_unknown_history"))
        assertTrue(unknownResult is MobileSessionImportResult.Invalid)
        assertTrue((unknownResult as MobileSessionImportResult.Invalid).message.contains("Exercice V2 inconnu"))
        assertTrue(repository.listSessions().none { it.sessionId in setOf("se_invalid_history", "se_unknown_history") })
    }

    private data class Case(
        val exercise: ExerciseProfile,
        val recording: String,
        val tracking: String,
        val fields: Int,
        val sets: JSONArray? = null,
        val continuous: JSONObject? = null,
    )

    private fun create(name: String, recording: RecordingMode, tracking: TrackingMode, fields: Int): ExerciseProfile =
        (repository.createExercise(NewExerciseProfile(name, recording, tracking, fields)) as CreateExerciseResult.Created).exercise

    private fun artifact(version: Int, cases: List<Case>, sessionId: String): JSONObject {
        val catalog = JSONArray()
        val entries = JSONArray()
        cases.forEachIndexed { index, case ->
            catalog.put(JSONObject().put("exercise_id", case.exercise.exerciseId).put("name", case.exercise.name)
                .put("recording_mode", case.exercise.recordingMode.wireValue)
                .put("tracking_mode", case.exercise.trackingMode.wireValue).put("data_fields", case.exercise.dataFields))
            val entry = JSONObject().put("exercise_id", case.exercise.exerciseId).put("name", case.exercise.name)
                .put("recording_mode", case.recording).put("tracking_mode", case.tracking).put("data_fields", case.fields)
                .put("load_mode", "none").put("rest_seconds", 0)
                .put("entry_id", "sxe_${version}_$index").put("position", index)
                .put("equipment_id", JSONObject.NULL)
            if (version == 3) entry.put("target", JSONObject.NULL)
            case.sets?.let { entry.put("sets", it) }
            case.continuous?.let { entry.put("continuous", it) }
            entries.put(entry)
        }
        return JSONObject().put("format", "trainlog-mobile-export").put("version", version)
            .put("generated_at", "2026-09-13T20:00:00+02:00").put("exercises", catalog)
            .put("sessions", JSONArray().put(JSONObject().put("session_id", sessionId)
                .put("started_at", "2026-09-13T19:00:00+02:00").put("session_type", "training")
                .put("exercises", entries))).put("body_observations", JSONArray())
    }

    private fun import(version: Int, payload: JSONObject): MobileSessionImportResult =
        if (version == 2) repository.applyPcMobileExportV2Json(payload.toString())
        else repository.applyPcMobileExportV3Json(payload.toString())

    private fun revisionCount(): Int = SQLiteDatabase.openDatabase(
        context.getDatabasePath(databaseName).path, null, SQLiteDatabase.OPEN_READONLY,
    ).use { database ->
        database.rawQuery("SELECT COUNT(*) FROM exercise_profile_revisions", null).use { cursor ->
            cursor.moveToFirst(); cursor.getInt(0)
        }
    }
}
