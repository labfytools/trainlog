package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.ExerciseDataFields
import com.labfytools.trainlog.model.ExerciseEditInput
import com.labfytools.trainlog.model.ExerciseProfile
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionDraft
import com.labfytools.trainlog.model.SessionDraftForm
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionSetDraft
import com.labfytools.trainlog.model.SessionType
import com.labfytools.trainlog.model.TrackingMode
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.util.UUID

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class TrainlogRepositoryDraftTest {
    private lateinit var context: Context
    private lateinit var databaseName: String
    private var repository: TrainlogRepository? = null

    @Before
    fun setUp() {
        context = ApplicationProvider.getApplicationContext()
        databaseName = "draft-test-${UUID.randomUUID()}.db"
    }

    @After
    fun tearDown() {
        repository?.close()
        context.deleteDatabase(databaseName)
    }

    @Test
    fun durableDraftRestoresEveryExerciseShapeAndRawForm() {
        val first = openRepository()
        val reps = createExercise(first, "Tractions", RecordingMode.SETS, TrackingMode.REPS)
        val duration = createExercise(first, "Gainage", RecordingMode.SETS, TrackingMode.DURATION)
        val continuous = createExercise(
            first,
            "Course",
            RecordingMode.CONTINUOUS,
            TrackingMode.DURATION,
            ExerciseDataFields.SPEED_KMH or ExerciseDataFields.DISTANCE_KM,
        )
        val expected = ActiveSessionDraft(
            exercises = listOf(
                SessionExerciseDraft(
                    exercise = reps,
                    sets = listOf(4, 5, 6, 7).map { SessionSetDraft(reps = it) },
                ),
                SessionExerciseDraft(
                    exercise = duration,
                    sets = listOf(20, 35, 50).map { SessionSetDraft(durationSeconds = it) },
                ),
                SessionExerciseDraft(
                    exercise = continuous,
                    continuousDurationSeconds = 1_800,
                    speedKmh = 8.5,
                    distanceKm = 4.25,
                ),
            ),
            sessionType = SessionType.MAX_TEST,
            form = SessionDraftForm(
                selectedExercise = reps,
                setCountText = "4",
                repsText = "4,5,6,",
                durationText = "31",
                speedText = "8,",
                distanceText = "4.",
            ),
        )

        assertEquals(ActiveDraftMutationResult.Saved, first.saveActiveSessionDraft(expected))
        first.close()
        repository = null

        val restored = loadDraft(openRepository())
        assertEquals(SessionType.MAX_TEST, restored.sessionType)
        assertEquals(expected.exercises, restored.exercises)
        assertEquals(expected.form, restored.form)
        assertTrue(restored.updatedAt.isNotBlank())
    }

    @Test
    fun removingExerciseAndDiscardingDraftDoNotDeleteCatalog() {
        val repo = openRepository()
        val kept = createExercise(repo, "Vélo", RecordingMode.CONTINUOUS, TrackingMode.DURATION)
        val removed = createExercise(repo, "Pompes", RecordingMode.SETS, TrackingMode.REPS)
        val initial = ActiveSessionDraft(
            exercises = listOf(
                SessionExerciseDraft(
                    exercise = removed,
                    sets = listOf(SessionSetDraft(reps = 12)),
                ),
                SessionExerciseDraft(
                    exercise = kept,
                    continuousDurationSeconds = 900,
                ),
            ),
        )
        assertEquals(ActiveDraftMutationResult.Saved, repo.saveActiveSessionDraft(initial))
        assertEquals(
            ActiveDraftMutationResult.Saved,
            repo.saveActiveSessionDraft(initial.copy(exercises = initial.exercises.drop(1))),
        )

        repo.close()
        repository = null
        val fresh = openRepository()
        val restored = loadDraft(fresh)
        assertEquals(listOf(kept.exerciseId), restored.exercises.map { it.exercise.exerciseId })
        assertTrue(fresh.listExercises().any { it.exerciseId == removed.exerciseId })
        assertEquals(ActiveDraftMutationResult.Saved, fresh.discardActiveSessionDraft())
        assertEquals(ActiveDraftLoadResult.None, fresh.loadActiveSessionDraft())
        assertEquals(2, fresh.listExercises().size)
        assertTrue(fresh.listSessions().isEmpty())
    }

    @Test
    fun finalizeIsAtomicAndDraftNeverExportsBeforeCompletion() {
        val repo = openRepository()
        val exercise = createExercise(repo, "Squat", RecordingMode.SETS, TrackingMode.REPS)
        val draft = ActiveSessionDraft(
            exercises = listOf(
                SessionExerciseDraft(
                    exercise = exercise,
                    sets = listOf(8, 7, 6).map { SessionSetDraft(reps = it) },
                )
            )
        )
        assertEquals(ActiveDraftMutationResult.Saved, repo.saveActiveSessionDraft(draft))
        assertEquals(0, JSONObject(repo.buildMobileExportJson()).getJSONArray("sessions").length())
        val result = repo.finalizeActiveSessionDraft()
        assertTrue(result is FinalizeActiveDraftResult.Saved)
        assertEquals(ActiveDraftLoadResult.None, repo.loadActiveSessionDraft())
        assertEquals(1, repo.listSessions().size)
        val exported = JSONObject(repo.buildMobileExportJson()).getJSONArray("sessions")
        assertEquals(1, exported.length())
        assertEquals(3, exported.getJSONObject(0).getJSONArray("exercises").getJSONObject(0).getJSONArray("sets").length())
        assertTrue(
            repo.finalizeActiveSessionDraft() is
                FinalizeActiveDraftResult.Invalid
        )
        assertEquals(1, repo.listSessions().size)
    }

    @Test
    fun finalizationFailureRollsBackCompletedRowsAndKeepsDraft() {
        val repo = openRepository()
        val exercise = createExercise(repo, "Row", RecordingMode.SETS, TrackingMode.REPS)
        assertEquals(
            ActiveDraftMutationResult.Saved,
            repo.saveActiveSessionDraft(
                ActiveSessionDraft(
                    exercises = listOf(
                        SessionExerciseDraft(
                            exercise = exercise,
                            sets = listOf(SessionSetDraft(reps = 10)),
                        )
                    )
                )
            ),
        )
        SQLiteDatabase.openDatabase(
            context.getDatabasePath(databaseName).path,
            null,
            SQLiteDatabase.OPEN_READWRITE,
        ).use {
            it.execSQL(
                "CREATE TRIGGER reject_completed_draft BEFORE INSERT ON session_exercises " +
                    "BEGIN SELECT RAISE(ABORT, 'forced finalization failure'); END;"
            )
        }

        val result = repo.finalizeActiveSessionDraft()
        assertTrue(result is FinalizeActiveDraftResult.DatabaseError)
        assertTrue(repo.listSessions().isEmpty())
        assertEquals(1, loadDraft(repo).exercises.size)
    }

    @Test
    fun catalogIdentityReconciliationKeepsDraftAndRawForm() {
        val repo = openRepository()
        val local = createExercise(repo, "Marche", RecordingMode.CONTINUOUS, TrackingMode.DURATION)
        val raw = SessionDraftForm(
            selectedExercise = local,
            durationText = "12,",
            speedText = "5,",
        )
        assertEquals(
            ActiveDraftMutationResult.Saved,
            repo.saveActiveSessionDraft(
                ActiveSessionDraft(
                    exercises = listOf(
                        SessionExerciseDraft(
                            exercise = local,
                            continuousDurationSeconds = 600,
                        )
                    ),
                    form = raw,
                )
            ),
        )
        val canonicalId = "ex_${UUID.randomUUID()}"
        val catalog = JSONObject()
            .put("format", "trainlog-pc-catalog")
            .put("version", 1)
            .put(
                "exercises",
                org.json.JSONArray().put(
                    JSONObject()
                        .put("exercise_id", canonicalId)
                        .put("name", "Marche")
                        .put("recording_mode", "continuous")
                        .put("tracking_mode", "duration")
                        .put("data_fields", 0)
                )
            )
        assertTrue(repo.applyPcCatalogJson(catalog.toString()) is PcCatalogImportResult.Applied)
        val restored = loadDraft(repo)
        assertEquals(canonicalId, restored.exercises.single().exercise.exerciseId)
        assertEquals(canonicalId, restored.form.selectedExercise?.exerciseId)
        assertEquals("12,", restored.form.durationText)
        assertEquals("5,", restored.form.speedText)
    }

    @Test
    fun renameKeepsStableIdHistoryAndActiveDraftReferences() {
        val repo = openRepository()
        val original = createExercise(repo, "un marche", RecordingMode.CONTINUOUS, TrackingMode.DURATION)
        val completed =
            repo.saveSession(
                SessionDraft(
                    exercises = listOf(
                        SessionExerciseDraft(
                            exercise = original,
                            continuousDurationSeconds = 300,
                        ),
                    ),
                ),
            )
        assertTrue(completed is SaveSessionResult.Saved)
        assertEquals(
            ActiveDraftMutationResult.Saved,
            repo.saveActiveSessionDraft(
                ActiveSessionDraft(
                    exercises = listOf(
                        SessionExerciseDraft(
                            exercise = original,
                            continuousDurationSeconds = 600,
                        ),
                    ),
                    form = SessionDraftForm(selectedExercise = original),
                ),
            ),
        )

        val result = repo.editExercise(
            ExerciseEditInput(
                original.exerciseId,
                "  Marche  ",
                original.recordingMode,
                original.trackingMode,
                original.dataFields,
            ),
        )
        assertTrue(result is EditExerciseResult.Saved)
        result as EditExerciseResult.Saved
        assertEquals(original.exerciseId, result.exercise.exerciseId)
        assertEquals("Marche", result.exercise.name)
        assertEquals("marche", result.exercise.normalizedName)
        val restored = loadDraft(repo)
        assertEquals(original.exerciseId, restored.exercises.single().exercise.exerciseId)
        assertEquals("Marche", restored.exercises.single().exercise.name)
        assertEquals(original.exerciseId, restored.form.selectedExercise?.exerciseId)
        val detail = repo.getSessionDetail((completed as SaveSessionResult.Saved).sessionId)
        assertNotNull(detail)
        assertEquals("Marche", detail?.exercises?.single()?.exerciseName)
        SQLiteDatabase.openDatabase(
            context.getDatabasePath(databaseName).path,
            null,
            SQLiteDatabase.OPEN_READONLY,
        ).use { db ->
            db.rawQuery(
                """
                SELECT e.exercise_id
                FROM session_exercises AS se
                JOIN exercises AS e ON e.id = se.exercise_row_id
                LIMIT 1;
                """.trimIndent(),
                null,
            ).use { cursor ->
                assertTrue(cursor.moveToFirst())
                assertEquals(original.exerciseId, cursor.getString(0))
            }
        }
    }

    @Test
    fun renameRejectsDuplicateAndInvalidNamesAndLocksReferencedProfile() {
        val repo = openRepository()
        val referenced = createExercise(repo, "Marche", RecordingMode.CONTINUOUS, TrackingMode.DURATION)
        createExercise(repo, "Course", RecordingMode.CONTINUOUS, TrackingMode.DURATION)
        assertEquals(
            ActiveDraftMutationResult.Saved,
            repo.saveActiveSessionDraft(
                ActiveSessionDraft(
                    exercises = listOf(
                        SessionExerciseDraft(
                            exercise = referenced,
                            continuousDurationSeconds = 30,
                        ),
                    ),
                ),
            ),
        )
        assertFalse(repo.canEditExerciseProfile(referenced.exerciseId))
        assertEquals(
            EditExerciseResult.Conflict,
            repo.editExercise(
                ExerciseEditInput(
                    referenced.exerciseId,
                    " course ",
                    referenced.recordingMode,
                    referenced.trackingMode,
                    referenced.dataFields,
                ),
            ),
        )
        assertEquals(
            EditExerciseResult.InvalidNameOrProfile,
            repo.editExercise(
                ExerciseEditInput(
                    referenced.exerciseId,
                    "   ",
                    referenced.recordingMode,
                    referenced.trackingMode,
                    referenced.dataFields,
                ),
            ),
        )
        assertEquals(
            EditExerciseResult.IncompatibleProfileChange,
            repo.editExercise(
                ExerciseEditInput(
                    referenced.exerciseId,
                    referenced.name,
                    RecordingMode.SETS,
                    TrackingMode.REPS,
                    ExerciseDataFields.NONE,
                ),
            ),
        )
    }

    @Test
    fun pcCatalogRenameUpdatesSameRowWithoutDuplicate() {
        val repo = openRepository()
        val local = createExercise(repo, "un marche", RecordingMode.CONTINUOUS, TrackingMode.DURATION)
        val catalog = JSONObject()
            .put("format", "trainlog-pc-catalog")
            .put("version", 1)
            .put(
                "exercises",
                org.json.JSONArray().put(
                    JSONObject()
                        .put("exercise_id", local.exerciseId)
                        .put("name", "Marche")
                        .put("recording_mode", "continuous")
                        .put("tracking_mode", "duration")
                        .put("data_fields", 0),
                ),
            )
        assertTrue(repo.applyPcCatalogJson(catalog.toString()) is PcCatalogImportResult.Applied)
        assertEquals(1, repo.listExercises().size)
        assertEquals(local.exerciseId, repo.listExercises().single().exerciseId)
        assertEquals("Marche", repo.listExercises().single().name)
    }

    @Test
    fun missingSelectedExerciseClearsOnlyFormAndReturnsDiagnostic() {
        val repo = openRepository()
        val added = createExercise(repo, "Conservé", RecordingMode.SETS, TrackingMode.REPS)
        val selected = createExercise(repo, "Supprimé", RecordingMode.SETS, TrackingMode.REPS)
        assertEquals(
            ActiveDraftMutationResult.Saved,
            repo.saveActiveSessionDraft(
                ActiveSessionDraft(
                    exercises = listOf(
                        SessionExerciseDraft(
                            exercise = added,
                            sets = listOf(SessionSetDraft(reps = 8)),
                        )
                    ),
                    form = SessionDraftForm(
                        selectedExercise = selected,
                        repsText = "4,5,6,",
                    ),
                )
            ),
        )
        SQLiteDatabase.openDatabase(
            context.getDatabasePath(databaseName).path,
            null,
            SQLiteDatabase.OPEN_READWRITE,
        ).use { db ->
            db.execSQL("PRAGMA foreign_keys = ON;")
            db.delete(
                "exercises",
                "exercise_id = ?",
                arrayOf(selected.exerciseId),
            )
        }

        val result = repo.loadActiveSessionDraft()
        assertTrue(result is ActiveDraftLoadResult.Loaded)
        result as ActiveDraftLoadResult.Loaded
        assertNotNull(result.warning)
        assertEquals(listOf(added.exerciseId), result.draft.exercises.map { it.exercise.exerciseId })
        assertEquals(null, result.draft.form.selectedExercise)
        assertEquals("4,5,6,", result.draft.form.repsText)
    }

    @Test
    fun databaseOpenFailureIsReturnedInsteadOfEscapingMutationApis() {
        val blocked = TrainlogRepository(
            context,
            "/proc/trainlog-draft-${UUID.randomUUID()}.db",
        )
        val save = blocked.saveActiveSessionDraft(ActiveSessionDraft())
        val finalize = blocked.finalizeActiveSessionDraft()
        assertTrue(save is ActiveDraftMutationResult.Error)
        assertTrue(finalize is FinalizeActiveDraftResult.DatabaseError)
        blocked.close()
    }

    @Test
    fun versionThreeMigrationPreservesCompletedAndBodyData() {
        createVersionThreeFixture(context.getDatabasePath(databaseName).path)
        val repo = openRepository()

        assertEquals(1, repo.listExercises().size)
        assertEquals(1, repo.listSessions().size)
        assertEquals(1, repo.listBodyObservations().size)
        assertEquals(ActiveDraftLoadResult.None, repo.loadActiveSessionDraft())
        SQLiteDatabase.openDatabase(
            context.getDatabasePath(databaseName).path,
            null,
            SQLiteDatabase.OPEN_READONLY,
        ).use { db ->
            db.rawQuery("PRAGMA user_version;", null).use { cursor ->
                assertTrue(cursor.moveToFirst())
                assertEquals(4, cursor.getInt(0))
            }
            db.rawQuery("PRAGMA foreign_key_check;", null).use { cursor ->
                assertFalse(cursor.moveToFirst())
            }
        }
    }

    private fun openRepository(): TrainlogRepository {
        return TrainlogRepository(context, databaseName).also { repository = it }
    }

    private fun loadDraft(repo: TrainlogRepository): ActiveSessionDraft {
        val result = repo.loadActiveSessionDraft()
        assertTrue(result is ActiveDraftLoadResult.Loaded)
        return (result as ActiveDraftLoadResult.Loaded).draft
    }

    private fun createExercise(
        repo: TrainlogRepository,
        name: String,
        recordingMode: RecordingMode,
        trackingMode: TrackingMode,
        dataFields: Int = ExerciseDataFields.NONE,
    ): ExerciseProfile {
        val result = repo.createExercise(
            NewExerciseProfile(name, recordingMode, trackingMode, dataFields)
        )
        assertTrue(result is CreateExerciseResult.Created)
        return (result as CreateExerciseResult.Created).exercise
    }

    private fun createVersionThreeFixture(path: String) {
        SQLiteDatabase.openOrCreateDatabase(path, null).use { db ->
            db.execSQL(
                "CREATE TABLE exercises(id INTEGER PRIMARY KEY, exercise_id TEXT NOT NULL UNIQUE, " +
                    "name TEXT NOT NULL, normalized_name TEXT NOT NULL UNIQUE, recording_mode TEXT NOT NULL, " +
                    "tracking_mode TEXT NOT NULL, data_fields INTEGER NOT NULL DEFAULT 0);"
            )
            db.execSQL(
                "CREATE TABLE sessions(id INTEGER PRIMARY KEY, session_id TEXT NOT NULL UNIQUE, " +
                    "started_at TEXT NOT NULL, session_type TEXT NOT NULL);"
            )
            db.execSQL(
                "CREATE TABLE session_exercises(id INTEGER PRIMARY KEY, session_row_id INTEGER NOT NULL " +
                    "REFERENCES sessions(id) ON DELETE CASCADE, exercise_row_id INTEGER NOT NULL " +
                    "REFERENCES exercises(id) ON DELETE RESTRICT, position INTEGER NOT NULL, " +
                    "recording_mode TEXT NOT NULL, tracking_mode TEXT NOT NULL, data_fields INTEGER NOT NULL, " +
                    "UNIQUE(session_row_id, position));"
            )
            db.execSQL(
                "CREATE TABLE performed_sets(id INTEGER PRIMARY KEY, session_exercise_row_id INTEGER NOT NULL " +
                    "REFERENCES session_exercises(id) ON DELETE CASCADE, position INTEGER NOT NULL, " +
                    "reps INTEGER, duration_seconds INTEGER, UNIQUE(session_exercise_row_id, position));"
            )
            db.execSQL(
                "CREATE TABLE continuous_activity(id INTEGER PRIMARY KEY, session_exercise_row_id INTEGER NOT NULL UNIQUE " +
                    "REFERENCES session_exercises(id) ON DELETE CASCADE, duration_seconds INTEGER NOT NULL, " +
                    "speed_kmh REAL, distance_km REAL);"
            )
            db.execSQL(
                "CREATE TABLE body_observations(id INTEGER PRIMARY KEY, observation_id TEXT NOT NULL UNIQUE, " +
                    "observed_at TEXT NOT NULL, body_weight_kg REAL, neck_cm REAL, shoulders_cm REAL, chest_cm REAL, " +
                    "waist_cm REAL, hips_cm REAL, left_arm_cm REAL, right_arm_cm REAL, left_forearm_cm REAL, " +
                    "right_forearm_cm REAL, left_thigh_cm REAL, right_thigh_cm REAL, left_calf_cm REAL, right_calf_cm REAL);"
            )
            db.execSQL(
                "INSERT INTO exercises VALUES(1, 'ex_fixture', 'Fixture', 'fixture', 'sets', 'reps', 0);"
            )
            db.execSQL(
                "INSERT INTO sessions VALUES(1, 'se_fixture', '2026-01-02T03:04:05+01:00', 'training');"
            )
            db.execSQL(
                "INSERT INTO session_exercises VALUES(1, 1, 1, 0, 'sets', 'reps', 0);"
            )
            db.execSQL("INSERT INTO performed_sets VALUES(1, 1, 0, 9, NULL);")
            db.execSQL(
                "INSERT INTO body_observations(id, observation_id, observed_at, body_weight_kg) " +
                    "VALUES(1, 'bo_fixture', '2026-01-02T03:04:05+01:00', 70.5);"
            )
            db.execSQL("PRAGMA user_version = 3;")
        }
    }
}
