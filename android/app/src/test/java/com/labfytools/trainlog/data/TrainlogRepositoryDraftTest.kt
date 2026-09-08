package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.BodyObservationDraft
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
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.io.File
import java.nio.file.Files
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
                    equipmentId = "leg_press",
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
                selectedEquipmentId = "treadmill",
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
    fun repeatedContinuousExercisePersistsDistinctOccurrencesAcrossReopenAndFinalize() {
        val repo = openRepository()
        val marche = createExercise(repo, "Marche", RecordingMode.CONTINUOUS, TrackingMode.DURATION)
        val first = SessionExerciseDraft(exercise = marche, continuousDurationSeconds = 600)
        val second = SessionExerciseDraft(exercise = marche, continuousDurationSeconds = 900)
        val draft = ActiveSessionDraft(exercises = listOf(first, second))
        assertEquals(ActiveDraftMutationResult.Saved, repo.saveActiveSessionDraft(draft))
        repo.close(); repository = null
        val reopened = openRepository()
        val restored = loadDraft(reopened)
        assertEquals(listOf(600, 900), restored.exercises.map { it.continuousDurationSeconds })
        assertEquals(listOf(marche.exerciseId, marche.exerciseId), restored.exercises.map { it.exercise.exerciseId })
        assertEquals(2, restored.exercises.map { it.entryId }.toSet().size)
        assertTrue(reopened.finalizeActiveSessionDraft() is FinalizeActiveDraftResult.Saved)
        val sessionId = reopened.listSessions().single().sessionId
        val detail = reopened.getSessionDetail(sessionId)!!
        assertEquals(listOf(600, 900), detail.exercises.map { it.continuousDurationSeconds })
        assertEquals(2, detail.exercises.map { it.entryId }.toSet().size)
        assertTrue(
            reopened.setCompletedSessionEquipment(
                sessionId,
                detail.exercises[0].entryId,
                "treadmill",
            ),
        )
        assertTrue(
            reopened.setCompletedSessionEquipment(
                sessionId,
                detail.exercises[1].entryId,
                "leg_press",
            ),
        )
        val edited = reopened.getSessionDetail(sessionId)!!
        assertTrue(edited.exercises[0].equipmentDisplayName != null)
        assertTrue(edited.exercises[1].equipmentDisplayName != null)
    }

    @Test
    fun weightedMachineSetsAndCustomEquipmentSurviveDraftFinalizeAndReopen() {
        val repo = openRepository()
        val exercise = createExercise(repo, "Presse", RecordingMode.SETS, TrackingMode.REPS)
        val custom = repo.createCustomEquipment("Presse personnelle")
        assertTrue(custom is CreateEquipmentResult.Created)
        val equipmentId = (custom as CreateEquipmentResult.Created).equipment.equipmentId
        val draft = ActiveSessionDraft(
            exercises = listOf(SessionExerciseDraft(
                exercise = exercise, equipmentId = equipmentId,
                sets = listOf(SessionSetDraft(10, weightKg = 12.5), SessionSetDraft(8, weightKg = 15.0)),
            )),
            form = SessionDraftForm(selectedExercise = exercise, selectedEquipmentId = equipmentId, weightText = "12,5;15"),
        )
        assertEquals(ActiveDraftMutationResult.Saved, repo.saveActiveSessionDraft(draft))
        repo.close(); repository = null
        val reopened = openRepository()
        assertEquals(draft.exercises, loadDraft(reopened).exercises)
        assertTrue(reopened.listEquipment().any { it.equipmentId == equipmentId })
        assertTrue(reopened.finalizeActiveSessionDraft() is FinalizeActiveDraftResult.Saved)
        val detail = reopened.getSessionDetail(reopened.listSessions().single().sessionId)!!
        assertEquals(listOf(12.5, 15.0), detail.exercises.single().sets.map { it.weightKg })
    }

    @Test
    fun androidCustomEquipmentDefinitionsExportWithTheirV2SessionReferences() {
        val repo = openRepository()
        val first = repo.createCustomEquipment("Presse Android") as CreateEquipmentResult.Created
        val second = repo.createCustomEquipment("Tirage Android") as CreateEquipmentResult.Created
        val exercise = createExercise(repo, "Développé Android", RecordingMode.SETS, TrackingMode.REPS)
        assertTrue(repo.saveSession(SessionDraft(exercises = listOf(
            SessionExerciseDraft(
                exercise = exercise,
                equipmentId = first.equipment.equipmentId,
                sets = listOf(SessionSetDraft(reps = 8, weightKg = 40.0)),
            ),
        ))) is SaveSessionResult.Saved)

        val definitions = JSONObject(repo.buildEquipmentDefinitionsJson())
        assertEquals("trainlog-equipment-definitions", definitions.getString("format"))
        assertEquals(1, definitions.getInt("version"))
        val exported = (0 until definitions.getJSONArray("equipment").length()).map {
            definitions.getJSONArray("equipment").getJSONObject(it)
        }
        assertEquals(setOf(first.equipment.equipmentId, second.equipment.equipmentId),
            exported.map { it.getString("equipment_id") }.toSet())
        assertTrue(exported.none { it.getString("equipment_id") == "leg_press" })
        assertTrue(exported.all { item ->
            item.keys().asSequence().toSet() == setOf(
                "equipment_id", "display_name", "label_name", "equipment_type", "load_semantics",
            ) && item.getString("display_name").isNotBlank() &&
                item.getString("equipment_type") == "custom_machine" &&
                item.getString("load_semantics") == "external"
        })

        val mobile = JSONObject(repo.buildMobileExportV2Json())
        val entry = mobile.getJSONArray("sessions").getJSONObject(0)
            .getJSONArray("exercises").getJSONObject(0)
        assertEquals(first.equipment.equipmentId, entry.getString("equipment_id"))

        SQLiteDatabase.openDatabase(
            context.getDatabasePath(databaseName).path,
            null,
            SQLiteDatabase.OPEN_READWRITE,
        ).use { db ->
            db.execSQL("UPDATE equipment SET display_name='' WHERE equipment_id=?", arrayOf(second.equipment.equipmentId))
        }
        try {
            repo.buildEquipmentDefinitionsJson()
            fail("Une définition persistée invalide doit empêcher l'export.")
        } catch (_: IllegalStateException) {
            /* Expected: exporting a partial custom-definition snapshot is forbidden. */
        }
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
    fun catalogCompatibleDifferentIdentityRekeysDraftAndKeepsRawForm() {
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
        assertEquals(
            PcCatalogImportResult.Applied(imported = 0, reconciled = 1, skipped = 0),
            repo.applyPcCatalogJson(catalog.toString()),
        )
        val restored = loadDraft(repo)
        assertEquals(canonicalId, restored.exercises.single().exercise.exerciseId)
        assertEquals(canonicalId, restored.form.selectedExercise?.exerciseId)
        assertEquals("12,", restored.form.durationText)
        assertEquals("5,", restored.form.speedText)
        assertFalse(repo.listExercises().any { it.exerciseId == local.exerciseId })
        assertEquals(1, repo.listExercises().count { it.exerciseId == canonicalId })
    }

    @Test
    fun marcheReconciliationKeepsRicherProfileAndTwoOccurrenceValues() {
        val repo = openRepository()
        val androidId = "ex_23212d79-52ce-4195-914d-dd983f133936"
        val desktopId = "ex_b1e6ffc6-75b5-45ff-a3c0-e7433c58013d"
        val androidCatalog = JSONObject()
            .put("format", "trainlog-pc-catalog")
            .put("version", 1)
            .put(
                "exercises",
                org.json.JSONArray().put(
                    JSONObject()
                        .put("exercise_id", androidId)
                        .put("name", "Marche")
                        .put("recording_mode", "continuous")
                        .put("tracking_mode", "duration")
                        .put(
                            "data_fields",
                            ExerciseDataFields.SPEED_KMH or ExerciseDataFields.DISTANCE_KM,
                        ),
                ),
            )
        assertTrue(repo.applyPcCatalogJson(androidCatalog.toString()) is PcCatalogImportResult.Applied)
        val local = repo.listExercises().single { it.exerciseId == androidId }
        val customEquipment =
            (repo.createCustomEquipment("Tapis Marche personnalisé") as
                CreateEquipmentResult.Created).equipment
        val saved = repo.saveSession(
            SessionDraft(
                exercises = listOf(
                    SessionExerciseDraft(
                        exercise = local,
                        equipmentId = customEquipment.equipmentId,
                        continuousDurationSeconds = 900,
                        speedKmh = 5.5,
                        distanceKm = 1.2,
                    ),
                    SessionExerciseDraft(
                        exercise = local,
                        equipmentId = customEquipment.equipmentId,
                        continuousDurationSeconds = 300,
                        speedKmh = 4.0,
                        distanceKm = 0.3,
                    ),
                ),
            ),
        ) as SaveSessionResult.Saved
        assertTrue(
            repo.saveBodyObservation(
                BodyObservationDraft(bodyWeightKg = 83.7, waistCm = 84.4),
            ) is SaveBodyObservationResult.Saved,
        )
        val before = repo.getSessionDetail(saved.sessionId)!!.exercises
        val entryIds = before.map { it.entryId }

        val catalog = JSONObject()
            .put("format", "trainlog-pc-catalog")
            .put("version", 1)
            .put(
                "exercises",
                org.json.JSONArray().put(
                    JSONObject()
                        .put("exercise_id", desktopId)
                        .put("name", "Marche")
                        .put("recording_mode", "continuous")
                        .put("tracking_mode", "duration")
                        .put("data_fields", ExerciseDataFields.SPEED_KMH),
                ),
            )

        assertEquals(
            PcCatalogImportResult.Applied(imported = 0, reconciled = 1, skipped = 0),
            repo.applyPcCatalogJson(catalog.toString()),
        )
        val canonical = repo.listExercises().single { it.exerciseId == desktopId }
        assertEquals(
            ExerciseDataFields.SPEED_KMH or ExerciseDataFields.DISTANCE_KM,
            canonical.dataFields,
        )
        assertFalse(repo.listExercises().any { it.exerciseId == androidId })
        val after = repo.getSessionDetail(saved.sessionId)!!.exercises
        assertEquals(entryIds, after.map { it.entryId })
        assertEquals(listOf(900, 300), after.map { it.continuousDurationSeconds })
        assertEquals(listOf(5.5, 4.0), after.map { it.speedKmh })
        assertEquals(listOf(1.2, 0.3), after.map { it.distanceKm })
        assertTrue(after.all { it.equipmentDisplayName == customEquipment.displayName })
        assertTrue(after.all { it.exerciseId == desktopId })

        /* F: exercise the real Android consumers in outbound protocol order,
         * then prove that a second complete state is semantically stable. */
        val definitions = JSONObject(repo.buildEquipmentDefinitionsJson())
        val mobile = JSONObject(repo.buildMobileExportV2Json())
        val associations = JSONObject(repo.buildEquipmentAssociationsJson())
        val beforeSemantic = listOf(
            definitions.getJSONArray("equipment").toString(),
            mobile.getJSONArray("exercises").toString(),
            mobile.getJSONArray("sessions").toString(),
            mobile.getJSONArray("body_observations").toString(),
            associations.getJSONArray("associations").toString(),
        )
        assertEquals(
            EquipmentDefinitionImportResult.Applied(imported = 0, skipped = 1),
            repo.applyPcEquipmentDefinitionsJson(definitions.toString()),
        )
        assertEquals(
            PcCatalogImportResult.Applied(imported = 0, reconciled = 0, skipped = 1),
            repo.applyPcCatalogJson(catalog.toString()),
        )
        assertEquals(
            MobileSessionImportResult.Applied(0, 1, 0, 1),
            repo.applyPcMobileExportV2Json(mobile.toString()),
        )
        assertEquals(
            EquipmentAssociationImportResult.Applied(0),
            repo.applyPcEquipmentAssociationsJson(associations.toString()),
        )
        val replayedDefinitions = JSONObject(repo.buildEquipmentDefinitionsJson())
        val replayedMobile = JSONObject(repo.buildMobileExportV2Json())
        val replayedAssociations = JSONObject(repo.buildEquipmentAssociationsJson())
        assertEquals(
            beforeSemantic,
            listOf(
                replayedDefinitions.getJSONArray("equipment").toString(),
                replayedMobile.getJSONArray("exercises").toString(),
                replayedMobile.getJSONArray("sessions").toString(),
                replayedMobile.getJSONArray("body_observations").toString(),
                replayedAssociations.getJSONArray("associations").toString(),
            ),
        )
        assertEquals(1, repo.listExercises().count { it.exerciseId == desktopId })
        assertEquals(entryIds, repo.getSessionDetail(saved.sessionId)!!.exercises.map { it.entryId })
    }

    @Test
    fun catalogNameEqualityDoesNotMergeIncomparableDataFields() {
        val repo = openRepository()
        val local = createExercise(
            repo,
            "Marche",
            RecordingMode.CONTINUOUS,
            TrackingMode.DURATION,
            ExerciseDataFields.SPEED_KMH,
        )
        val foreignId = "ex_${UUID.randomUUID()}"
        val catalog = JSONObject()
            .put("format", "trainlog-pc-catalog")
            .put("version", 1)
            .put(
                "exercises",
                org.json.JSONArray().put(
                    JSONObject()
                        .put("exercise_id", foreignId)
                        .put("name", "Marche")
                        .put("recording_mode", "continuous")
                        .put("tracking_mode", "duration")
                        .put("data_fields", ExerciseDataFields.DISTANCE_KM),
                ),
            )

        assertTrue(repo.applyPcCatalogJson(catalog.toString()) is PcCatalogImportResult.Invalid)
        assertEquals(local.exerciseId, repo.listExercises().single().exerciseId)
        assertEquals(ExerciseDataFields.SPEED_KMH, repo.listExercises().single().dataFields)
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
    fun pcMobileV2AcceptsOlderSubsetSnapshotWithoutInventingDistance() {
        val repo = openRepository()
        val walk = createExercise(
            repo,
            "Marche riche",
            RecordingMode.CONTINUOUS,
            TrackingMode.DURATION,
            ExerciseDataFields.SPEED_KMH or ExerciseDataFields.DISTANCE_KM,
        )
        val entryId = "sxe_subset_walk"
        val artifact = JSONObject()
            .put("format", "trainlog-mobile-export")
            .put("version", 2)
            .put("generated_at", "2026-09-08T16:00:00+02:00")
            .put(
                "exercises",
                org.json.JSONArray().put(
                    JSONObject()
                        .put("exercise_id", walk.exerciseId)
                        .put("name", walk.name)
                        .put("recording_mode", "continuous")
                        .put("tracking_mode", "duration")
                        .put("data_fields", walk.dataFields),
                ),
            )
            .put(
                "sessions",
                org.json.JSONArray().put(
                    JSONObject()
                        .put("session_id", "se_subset_walk")
                        .put("started_at", "2026-09-08T15:45:00+02:00")
                        .put("session_type", "training")
                        .put(
                            "exercises",
                            org.json.JSONArray().put(
                                JSONObject()
                                    .put("entry_id", entryId)
                                    .put("position", 0)
                                    .put("exercise_id", walk.exerciseId)
                                    .put("name", walk.name)
                                    .put("recording_mode", "continuous")
                                    .put("tracking_mode", "duration")
                                    .put("data_fields", ExerciseDataFields.SPEED_KMH)
                                    .put("load_mode", "none")
                                    .put("rest_seconds", 0)
                                    .put("equipment_id", JSONObject.NULL)
                                    .put(
                                        "continuous",
                                        JSONObject()
                                            .put("duration_seconds", 600)
                                            .put("speed_kmh", 5.8),
                                    ),
                            ),
                        ),
                ),
            )
            .put("body_observations", org.json.JSONArray())

        assertEquals(
            MobileSessionImportResult.Applied(1, 0, 0, 0),
            repo.applyPcMobileExportV2Json(artifact.toString()),
        )
        val detail = repo.getSessionDetail("se_subset_walk")!!.exercises.single()
        assertEquals(entryId, detail.entryId)
        assertEquals(ExerciseDataFields.SPEED_KMH, detail.dataFields)
        assertEquals(600, detail.continuousDurationSeconds)
        assertEquals(5.8, detail.speedKmh!!, 0.0)
        assertEquals(null, detail.distanceKm)
        assertEquals(
            MobileSessionImportResult.Applied(0, 1, 0, 0),
            repo.applyPcMobileExportV2Json(artifact.toString()),
        )
    }

    @Test
    fun realPcExportersAndAndroidImportersAreStrictlyIdempotentForThreePasses() {
        val repo = openRepository()
        val androidWalkId = "ex_23212d79-52ce-4195-914d-dd983f133936"
        val desktopWalkId = "ex_b1e6ffc6-75b5-45ff-a3c0-e7433c58013d"
        val legPressId = "ex_b432623f-bfe9-4daf-a653-60ec7fdffbde"
        val pcOnlyId = "ex_7e7cf906-2214-4066-bcb7-c16382d83b3b"
        val customEquipmentId = "eq_44444444-4444-4444-8444-444444444444"
        val pcSessionId = "se_11111111-1111-4111-8111-111111111111"
        val pcRichWalkEntryId = "sxe_22222222-2222-4222-8222-222222222221"
        val pcLegacyWalkEntryId = "sxe_22222222-2222-4222-8222-222222222222"
        val pcLegPressEntryId = "sxe_22222222-2222-4222-8222-222222222223"
        val fixtureDirectory = Files.createTempDirectory("trainlog-pc-android-idempotence-").toFile()

        try {
            val fixtureScript = findRepositoryFile(
                "tests/create_pc_android_idempotence_fixture.py",
            )
            val process = ProcessBuilder(
                "python3",
                fixtureScript.path,
                fixtureDirectory.path,
            ).redirectErrorStream(true).start()
            val fixtureOutput = process.inputStream.bufferedReader().use { it.readText() }
            assertEquals(fixtureOutput, 0, process.waitFor())
            assertTrue(fixtureOutput, fixtureOutput.contains("PASS pc_android_idempotence_fixture"))

            val initialCatalog = JSONObject()
                .put("format", "trainlog-pc-catalog")
                .put("version", 1)
                .put(
                    "exercises",
                    org.json.JSONArray()
                        .put(
                            JSONObject()
                                .put("exercise_id", androidWalkId)
                                .put("name", "Marche")
                                .put("recording_mode", "continuous")
                                .put("tracking_mode", "duration")
                                .put(
                                    "data_fields",
                                    ExerciseDataFields.SPEED_KMH or ExerciseDataFields.DISTANCE_KM,
                                ),
                        )
                        .put(
                            JSONObject()
                                .put("exercise_id", legPressId)
                                .put("name", "Leg press")
                                .put("recording_mode", "sets")
                                .put("tracking_mode", "reps")
                                .put("data_fields", ExerciseDataFields.NONE),
                        ),
                )
            assertEquals(
                PcCatalogImportResult.Applied(imported = 2, reconciled = 0, skipped = 0),
                repo.applyPcCatalogJson(initialCatalog.toString()),
            )
            val initialWalk = repo.listExercises().single { it.exerciseId == androidWalkId }
            val initialLegPress = repo.listExercises().single { it.exerciseId == legPressId }
            val localEquipment =
                (repo.createCustomEquipment("Équipement Android conservé") as
                    CreateEquipmentResult.Created).equipment
            val localSession = repo.saveSession(
                SessionDraft(
                    exercises = listOf(
                        SessionExerciseDraft(
                            entryId = "sxe_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa1",
                            exercise = initialWalk,
                            equipmentId = localEquipment.equipmentId,
                            continuousDurationSeconds = 720,
                            speedKmh = 5.1,
                            distanceKm = 1.02,
                        ),
                        SessionExerciseDraft(
                            entryId = "sxe_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa2",
                            exercise = initialWalk,
                            equipmentId = localEquipment.equipmentId,
                            continuousDurationSeconds = 240,
                            speedKmh = 4.2,
                            distanceKm = 0.28,
                        ),
                        SessionExerciseDraft(
                            entryId = "sxe_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa3",
                            exercise = initialLegPress,
                            equipmentId = localEquipment.equipmentId,
                            sets = listOf(
                                SessionSetDraft(reps = 10, weightKg = 75.0),
                                SessionSetDraft(reps = 8, weightKg = 82.5),
                            ),
                        ),
                    ),
                ),
            ) as SaveSessionResult.Saved
            assertEquals(
                ActiveDraftMutationResult.Saved,
                repo.saveActiveSessionDraft(
                    ActiveSessionDraft(
                        exercises = listOf(
                            SessionExerciseDraft(
                                entryId = "sxe_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa4",
                                exercise = initialWalk,
                                equipmentId = localEquipment.equipmentId,
                                continuousDurationSeconds = 180,
                                speedKmh = 4.8,
                                distanceKm = 0.24,
                            ),
                        ),
                        form = SessionDraftForm(
                            selectedExercise = initialWalk,
                            selectedEquipmentId = localEquipment.equipmentId,
                            durationText = "180",
                            speedText = "4.8",
                            distanceText = "0.24",
                        ),
                    ),
                ),
            )

            val definitions = File(
                fixtureDirectory,
                "trainlog-pc-equipment-definitions-v1.json",
            ).readText()
            val catalog = File(
                fixtureDirectory,
                "trainlog-pc-catalog-v1.json",
            ).readText()
            val mobile = File(
                fixtureDirectory,
                "trainlog-pc-mobile-export-v2.json",
            ).readText()
            val associations = File(
                fixtureDirectory,
                "trainlog-equipment-associations-v2.json",
            ).readText()

            val firstDecisions = mutableListOf<PcCatalogExerciseDecision>()
            assertEquals(
                EquipmentDefinitionImportResult.Applied(imported = 1, skipped = 0),
                repo.applyPcEquipmentDefinitionsJson(definitions),
            )
            assertEquals(
                PcCatalogImportResult.Applied(imported = 1, reconciled = 1, skipped = 1),
                repo.applyPcCatalogJson(catalog, firstDecisions::add),
            )
            assertEquals(
                MobileSessionImportResult.Applied(1, 0, 1, 0),
                repo.applyPcMobileExportV2Json(mobile),
            )
            assertEquals(
                EquipmentAssociationImportResult.Applied(updated = 0),
                repo.applyPcEquipmentAssociationsJson(associations),
            )
            assertEquals(
                mapOf(
                    desktopWalkId to "existing-reconciled",
                    legPressId to "existing-identical",
                    pcOnlyId to "inserted",
                ),
                firstDecisions.associate { it.exerciseId to it.decision },
            )
            assertEquals(
                "normalized_name:$androidWalkId",
                firstDecisions.single { it.exerciseId == desktopWalkId }.lookup,
            )

            val pcSession = repo.getSessionDetail(pcSessionId)!!
            assertEquals(
                listOf(pcRichWalkEntryId, pcLegacyWalkEntryId, pcLegPressEntryId),
                pcSession.exercises.map { it.entryId },
            )
            assertEquals(listOf(900, 300), pcSession.exercises.take(2).map { it.continuousDurationSeconds })
            assertEquals(listOf(5.5, 4.0), pcSession.exercises.take(2).map { it.speedKmh })
            assertEquals(listOf(1.2, null), pcSession.exercises.take(2).map { it.distanceKm })
            assertEquals(
                listOf(80.0, 85.0),
                pcSession.exercises.last().sets.map { it.weightKg },
            )
            assertTrue(pcSession.exercises.all { it.equipmentDisplayName == "Machine PC custom" })
            val localAfterReconciliation = repo.getSessionDetail(localSession.sessionId)!!
            assertEquals(
                listOf(
                    "sxe_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa1",
                    "sxe_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa2",
                    "sxe_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa3",
                ),
                localAfterReconciliation.exercises.map { it.entryId },
            )
            assertEquals(
                listOf(desktopWalkId, desktopWalkId, legPressId),
                localAfterReconciliation.exercises.map { it.exerciseId },
            )
            assertEquals(desktopWalkId, loadDraft(repo).exercises.single().exercise.exerciseId)
            assertFalse(repo.listExercises().any { it.exerciseId == androidWalkId })
            assertTrue(repo.listEquipment().any { it.equipmentId == customEquipmentId })
            assertTrue(repo.listEquipment().any { it.equipmentId == localEquipment.equipmentId })

            val afterFirst = snapshotBusinessTables(context.getDatabasePath(databaseName).path)

            repeat(2) { replayIndex ->
                val replayDecisions = mutableListOf<PcCatalogExerciseDecision>()
                assertEquals(
                    EquipmentDefinitionImportResult.Applied(imported = 0, skipped = 1),
                    repo.applyPcEquipmentDefinitionsJson(definitions),
                )
                assertEquals(
                    PcCatalogImportResult.Applied(imported = 0, reconciled = 0, skipped = 3),
                    repo.applyPcCatalogJson(catalog, replayDecisions::add),
                )
                assertEquals(
                    MobileSessionImportResult.Applied(0, 1, 0, 1),
                    repo.applyPcMobileExportV2Json(mobile),
                )
                assertEquals(
                    EquipmentAssociationImportResult.Applied(updated = 0),
                    repo.applyPcEquipmentAssociationsJson(associations),
                )
                assertEquals(
                    setOf(desktopWalkId, legPressId, pcOnlyId),
                    replayDecisions.map { it.exerciseId }.toSet(),
                )
                assertTrue(replayDecisions.all { it.decision == "existing-identical" })
                assertEquals(
                    "business tables changed on replay ${replayIndex + 2}",
                    afterFirst,
                    snapshotBusinessTables(context.getDatabasePath(databaseName).path),
                )
            }
        } finally {
            fixtureDirectory.walkBottomUp().forEach { it.delete() }
        }
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
    fun completedSessionKeepsEquipmentAcrossReopenAndCompanionExport() {
        val first = openRepository()
        val exercise = createExercise(first, "Presse test", RecordingMode.SETS, TrackingMode.REPS)
        val saved = first.saveSession(
            SessionDraft(
                exercises = listOf(
                    SessionExerciseDraft(
                        exercise = exercise,
                        equipmentId = "leg_press",
                        sets = listOf(SessionSetDraft(reps = 10)),
                    ),
                ),
            ),
        )
        assertTrue(saved is SaveSessionResult.Saved)
        val sessionId = (saved as SaveSessionResult.Saved).sessionId
        val exported = JSONObject(first.buildEquipmentAssociationsJson()).getJSONArray("associations")
        assertEquals("set", exported.getJSONObject(0).getString("state"))
        assertEquals("leg_press", exported.getJSONObject(0).getString("equipment_id"))
        first.close()
        repository = null
        val reopened = openRepository()
        val again = JSONObject(reopened.buildEquipmentAssociationsJson()).getJSONArray("associations")
        assertEquals(sessionId, again.getJSONObject(0).getString("session_id"))
        assertEquals("leg_press", again.getJSONObject(0).getString("equipment_id"))
        val cleared = JSONObject()
            .put("format", "trainlog-equipment-associations")
            .put("version", 1)
            .put("generated_at", "2026-01-01T00:00:00+00:00")
            .put("associations", org.json.JSONArray().put(JSONObject()
                .put("session_id", sessionId)
                .put("exercise_id", exercise.exerciseId)
                .put("state", "cleared")))
        assertTrue(reopened.applyPcEquipmentAssociationsJson(cleared.toString()) is EquipmentAssociationImportResult.Invalid)
        val afterClear = JSONObject(reopened.buildEquipmentAssociationsJson()).getJSONArray("associations")
        assertEquals("set", afterClear.getJSONObject(0).getString("state"))
        assertEquals("leg_press", afterClear.getJSONObject(0).getString("equipment_id"))

        val differentExercise = JSONObject(reopened.buildEquipmentAssociationsJson())
        val association = differentExercise.getJSONArray("associations").getJSONObject(0)
        association.put("exercise_id", "ex_different")
        assertTrue(reopened.applyPcEquipmentAssociationsJson(differentExercise.toString()) is EquipmentAssociationImportResult.Invalid)
        val afterExerciseConflict = JSONObject(reopened.buildEquipmentAssociationsJson()).getJSONArray("associations")
        assertEquals("leg_press", afterExerciseConflict.getJSONObject(0).getString("equipment_id"))
    }

    @Test
    fun customEquipmentDefinitionsAreAdditiveIdempotentAndConflictConservatively() {
        val repo = openRepository()
        val root = JSONObject().put("format", "trainlog-equipment-definitions").put("version", 1)
            .put("generated_at", "2026-03-01T10:00:00+01:00")
        val definition = JSONObject().put("equipment_id", "eq_remote")
            .put("display_name", "Presse voyage").put("label_name", "Presse")
            .put("equipment_type", "custom_machine").put("load_semantics", "external")
        root.put("equipment", org.json.JSONArray().put(definition))
        assertTrue(repo.applyPcEquipmentDefinitionsJson(root.toString()) is EquipmentDefinitionImportResult.Applied)
        val replay = repo.applyPcEquipmentDefinitionsJson(root.toString())
        assertTrue(replay is EquipmentDefinitionImportResult.Applied)
        assertEquals(1, (replay as EquipmentDefinitionImportResult.Applied).skipped)

        root.put("equipment", org.json.JSONArray())
        assertTrue(repo.applyPcEquipmentDefinitionsJson(root.toString()) is EquipmentDefinitionImportResult.Applied)
        assertTrue(repo.listEquipment().any { it.equipmentId == "eq_remote" })

        definition.put("display_name", "Nom divergent")
        root.put("equipment", org.json.JSONArray().put(definition))
        assertTrue(repo.applyPcEquipmentDefinitionsJson(root.toString()) is EquipmentDefinitionImportResult.Invalid)
        assertEquals("Presse voyage", repo.listEquipment().single { it.equipmentId == "eq_remote" }.displayName)

        definition.put("equipment_id", "leg_press")
        assertTrue(repo.applyPcEquipmentDefinitionsJson(root.toString()) is EquipmentDefinitionImportResult.Invalid)
    }

    @Test
    fun customEquipmentDefinitionWithNoneSemanticsRoundTripsAndReplaysIdempotently() {
        val repo = openRepository()
        val definition = JSONObject()
            .put("equipment_id", "eq_unloaded_remote")
            .put("display_name", "Agrès sans charge")
            .put("label_name", "Agrès")
            .put("equipment_type", "custom_station")
            .put("load_semantics", "none")
        val root = JSONObject()
            .put("format", "trainlog-equipment-definitions")
            .put("version", 1)
            .put("generated_at", "2026-03-01T10:00:00+01:00")
            .put("equipment", org.json.JSONArray().put(definition))

        val first = repo.applyPcEquipmentDefinitionsJson(root.toString())
        assertEquals(EquipmentDefinitionImportResult.Applied(1, 0), first)
        assertEquals(
            EquipmentLoadSemantics.NONE,
            repo.listEquipment().single { it.equipmentId == "eq_unloaded_remote" }.loadSemantics,
        )

        val exported = JSONObject(repo.buildEquipmentDefinitionsJson()).getJSONArray("equipment")
        val roundTripped = (0 until exported.length())
            .map { exported.getJSONObject(it) }
            .single { it.getString("equipment_id") == "eq_unloaded_remote" }
        assertEquals("none", roundTripped.getString("load_semantics"))
        assertEquals(
            EquipmentDefinitionImportResult.Applied(0, 1),
            repo.applyPcEquipmentDefinitionsJson(root.toString()),
        )
    }

    @Test
    fun equipmentDefinitionsV1RejectMalformedMetadataAndUnsupportedSemanticsAtomically() {
        val repo = openRepository()
        fun definition(id: String, semantics: String) = JSONObject()
            .put("equipment_id", id)
            .put("display_name", "Équipement $id")
            .put("label_name", "Équipement")
            .put("equipment_type", "custom_station")
            .put("load_semantics", semantics)
        fun artifact(items: org.json.JSONArray, generatedAt: Any = "2026-03-01T10:00:00+01:00") = JSONObject()
            .put("format", "trainlog-equipment-definitions")
            .put("version", 1)
            .put("generated_at", generatedAt)
            .put("equipment", items)

        for (semantics in listOf("bodyweight", "cardio")) {
            val result = repo.applyPcEquipmentDefinitionsJson(
                artifact(org.json.JSONArray().put(definition("eq_$semantics", semantics))).toString(),
            )
            assertTrue(result is EquipmentDefinitionImportResult.Invalid)
            assertFalse(repo.listEquipment().any { it.equipmentId == "eq_$semantics" })
        }

        val malformedMetadata = artifact(org.json.JSONArray().put(definition("eq_bad_time", "external")), 42)
        assertTrue(repo.applyPcEquipmentDefinitionsJson(malformedMetadata.toString()) is EquipmentDefinitionImportResult.Invalid)

        val stringVersion = artifact(org.json.JSONArray().put(definition("eq_string_version", "external"))).put("version", "1")
        assertTrue(repo.applyPcEquipmentDefinitionsJson(stringVersion.toString()) is EquipmentDefinitionImportResult.Invalid)

        val unknownRootKey = artifact(org.json.JSONArray().put(definition("eq_unknown_root", "external"))).put("extra", true)
        assertTrue(repo.applyPcEquipmentDefinitionsJson(unknownRootKey.toString()) is EquipmentDefinitionImportResult.Invalid)

        val unknownItemKey = definition("eq_unknown_item", "external").put("extra", true)
        assertTrue(repo.applyPcEquipmentDefinitionsJson(
            artifact(org.json.JSONArray().put(unknownItemKey)).toString(),
        ) is EquipmentDefinitionImportResult.Invalid)

        val atomic = artifact(org.json.JSONArray()
            .put(definition("eq_atomic_valid", "external"))
            .put(definition("eq_atomic_invalid", "cardio")))
        assertTrue(repo.applyPcEquipmentDefinitionsJson(atomic.toString()) is EquipmentDefinitionImportResult.Invalid)
        assertFalse(repo.listEquipment().any { it.equipmentId in setOf("eq_atomic_valid", "eq_atomic_invalid") })

        for (field in listOf("equipment_id", "display_name", "label_name", "equipment_type", "load_semantics")) {
            for (invalidValue in listOf<Any>(42, true)) {
                val invalid = definition("eq_bad_${field}_${invalidValue}", "external")
                    .put(field, invalidValue)
                val strictAtomic = artifact(org.json.JSONArray()
                    .put(definition("eq_strict_valid_${field}_${invalidValue}", "external"))
                    .put(invalid))
                assertTrue(repo.applyPcEquipmentDefinitionsJson(strictAtomic.toString()) is EquipmentDefinitionImportResult.Invalid)
                assertFalse(repo.listEquipment().any {
                    it.equipmentId == "eq_strict_valid_${field}_${invalidValue}"
                })
            }
        }
    }

    @Test
    fun pcMobileExportV2RejectsMalformedShapesAsInvalidAndDoesNotPartiallyImport() {
        val repo = openRepository()
        val exercise = createExercise(repo, "Import V2", RecordingMode.SETS, TrackingMode.REPS)
        fun entry(id: String) = JSONObject()
            .put("exercise_id", exercise.exerciseId).put("name", exercise.name)
            .put("recording_mode", "sets").put("tracking_mode", "reps").put("data_fields", 0)
            .put("load_mode", "none").put("rest_seconds", 0)
            .put("entry_id", id).put("position", 0).put("equipment_id", JSONObject.NULL)
            .put("sets", org.json.JSONArray().put(JSONObject().put("reps", 8)))
        fun session(id: String, sessionEntry: JSONObject) = JSONObject()
            .put("session_id", id).put("started_at", "2026-03-02T10:00:00+01:00")
            .put("session_type", "training").put("exercises", org.json.JSONArray().put(sessionEntry))
        fun artifact(sessions: org.json.JSONArray) = JSONObject()
            .put("format", "trainlog-mobile-export").put("version", 2)
            .put("generated_at", "2026-03-02T11:00:00+01:00")
            .put("exercises", org.json.JSONArray().put(JSONObject()
                .put("exercise_id", exercise.exerciseId).put("name", exercise.name)
                .put("recording_mode", "sets").put("tracking_mode", "reps").put("data_fields", 0)))
            .put("sessions", sessions).put("body_observations", org.json.JSONArray())

        val valid = artifact(org.json.JSONArray().put(session("se_pc_valid", entry("en_pc_valid"))))
        assertEquals(MobileSessionImportResult.Applied(1, 0, 0, 0), repo.applyPcMobileExportV2Json(valid.toString()))

        /* Regression: a body observation emitted by the desktop V2 artifact
         * must survive Android's real importer/exporter unchanged.  A repeat
         * is an idempotent replay; changing one business metric remains an
         * explicit conflict rather than a last-writer-wins update. */
        valid.getJSONArray("body_observations").put(
            JSONObject()
                .put("observation_id", "bo_pc_round_trip")
                .put("observed_at", "2026-03-02T11:30:00.123456+01:00")
                .put("body_weight_kg", 83.7)
                .put("left_calf_cm", 40.0),
        )
        assertEquals(MobileSessionImportResult.Applied(0, 1, 1, 0), repo.applyPcMobileExportV2Json(valid.toString()))
        val androidRoundTrip = JSONObject(repo.buildMobileExportV2Json())
        val exportedBody = androidRoundTrip.getJSONArray("body_observations").getJSONObject(0)
        assertEquals("bo_pc_round_trip", exportedBody.getString("observation_id"))
        assertEquals("2026-03-02T11:30:00.123456+01:00", exportedBody.getString("observed_at"))
        assertEquals(83.7, exportedBody.getDouble("body_weight_kg"), 0.0)
        assertEquals(40.0, exportedBody.getDouble("left_calf_cm"), 0.0)
        assertEquals(MobileSessionImportResult.Applied(0, 1, 0, 1), repo.applyPcMobileExportV2Json(androidRoundTrip.toString()))

        val changedBody = JSONObject(androidRoundTrip.toString())
        changedBody.getJSONArray("body_observations").getJSONObject(0).put("body_weight_kg", 84.7)
        assertTrue(repo.applyPcMobileExportV2Json(changedBody.toString()) is MobileSessionImportResult.Invalid)

        val incompatibleProfile = entry("en_incompatible_profile")
            .put("tracking_mode", "duration")
            .put("sets", org.json.JSONArray().put(JSONObject().put("duration_seconds", 8)))
        assertTrue(repo.applyPcMobileExportV2Json(
            artifact(org.json.JSONArray().put(session("se_incompatible_profile", incompatibleProfile))).toString(),
        ) is MobileSessionImportResult.Invalid)
        assertEquals(1, JSONObject(repo.buildMobileExportV2Json()).getJSONArray("sessions").length())

        val unknownNestedKey = entry("en_unknown").put("unexpected", true)
        assertTrue(repo.applyPcMobileExportV2Json(
            artifact(org.json.JSONArray().put(session("se_unknown", unknownNestedKey))).toString(),
        ) is MobileSessionImportResult.Invalid)

        val malformedSet = entry("en_bad_set")
        malformedSet.put("sets", org.json.JSONArray().put(JSONObject().put("reps", "eight")))
        assertTrue(repo.applyPcMobileExportV2Json(
            artifact(org.json.JSONArray().put(session("se_bad_set", malformedSet))).toString(),
        ) is MobileSessionImportResult.Invalid)

        val missingNestedData = entry("en_missing_sets").also { it.remove("sets") }
        val atomicArtifact = artifact(org.json.JSONArray()
            .put(session("se_atomic_first", entry("en_atomic_first")))
            .put(session("se_atomic_bad", missingNestedData)))
        assertTrue(repo.applyPcMobileExportV2Json(atomicArtifact.toString()) is MobileSessionImportResult.Invalid)
        val sessionIds = JSONObject(repo.buildMobileExportV2Json()).getJSONArray("sessions")
        assertEquals(1, sessionIds.length())
        assertEquals("se_pc_valid", sessionIds.getJSONObject(0).getString("session_id"))
    }

    @Test
    fun pcEquipmentAssociationsEnforceExactShapesAndRetainV1AndV2Compatibility() {
        val repo = openRepository()
        val exercise = createExercise(repo, "Association stricte", RecordingMode.SETS, TrackingMode.REPS)
        val saved = repo.saveSession(SessionDraft(exercises = listOf(SessionExerciseDraft(
            exercise = exercise, equipmentId = "leg_press", sets = listOf(SessionSetDraft(reps = 6)),
        )))) as SaveSessionResult.Saved
        val exported = JSONObject(repo.buildEquipmentAssociationsJson())
        assertEquals(EquipmentAssociationImportResult.Applied(0), repo.applyPcEquipmentAssociationsJson(exported.toString()))

        val association = exported.getJSONArray("associations").getJSONObject(0)
        val v1 = JSONObject(exported.toString()).put("version", 1)
        v1.getJSONArray("associations").getJSONObject(0).remove("entry_id")
        assertEquals(EquipmentAssociationImportResult.Applied(0), repo.applyPcEquipmentAssociationsJson(v1.toString()))

        val unknownRoot = JSONObject(exported.toString()).put("unexpected", true)
        assertTrue(repo.applyPcEquipmentAssociationsJson(unknownRoot.toString()) is EquipmentAssociationImportResult.Invalid)

        val unknownItem = JSONObject(exported.toString())
        unknownItem.getJSONArray("associations").getJSONObject(0).put("unexpected", true)
        assertTrue(repo.applyPcEquipmentAssociationsJson(unknownItem.toString()) is EquipmentAssociationImportResult.Invalid)

        val missingSetEquipment = JSONObject(exported.toString())
        missingSetEquipment.getJSONArray("associations").getJSONObject(0).remove("equipment_id")
        assertTrue(repo.applyPcEquipmentAssociationsJson(missingSetEquipment.toString()) is EquipmentAssociationImportResult.Invalid)

        val clearedWithEquipment = JSONObject(exported.toString())
        clearedWithEquipment.getJSONArray("associations").getJSONObject(0).put("state", "cleared")
        assertTrue(repo.applyPcEquipmentAssociationsJson(clearedWithEquipment.toString()) is EquipmentAssociationImportResult.Invalid)
        assertEquals(saved.sessionId, association.getString("session_id"))
        assertEquals("leg_press", JSONObject(repo.buildEquipmentAssociationsJson())
            .getJSONArray("associations").getJSONObject(0).getString("equipment_id"))
    }

    @Test
    fun versionSevenMigrationPreservesEquipmentHistoryAndActiveDraftReferences() {
        val first = openRepository()
        val custom = first.createCustomEquipment("Machine v7") as CreateEquipmentResult.Created
        val exercise = createExercise(first, "Exercice v7", RecordingMode.SETS, TrackingMode.REPS)
        assertTrue(
            first.saveSession(
                SessionDraft(
                    exercises = listOf(
                        SessionExerciseDraft(
                            exercise = exercise,
                            equipmentId = custom.equipment.equipmentId,
                            sets = listOf(SessionSetDraft(reps = 7, weightKg = 42.5)),
                        ),
                    ),
                ),
            ) is SaveSessionResult.Saved,
        )
        assertEquals(
            ActiveDraftMutationResult.Saved,
            first.saveActiveSessionDraft(
                ActiveSessionDraft(
                    exercises = listOf(
                        SessionExerciseDraft(
                            exercise = exercise,
                            equipmentId = custom.equipment.equipmentId,
                            sets = listOf(SessionSetDraft(reps = 5, weightKg = 30.0)),
                        ),
                    ),
                    form = SessionDraftForm(selectedEquipmentId = custom.equipment.equipmentId),
                ),
            ),
        )
        first.close()
        repository = null

        /* Recreate the v7 constraint text on an otherwise exact v7-shaped
         * fixture; v8 must widen it without losing any graph member. */
        SQLiteDatabase.openDatabase(
            context.getDatabasePath(databaseName).path,
            null,
            SQLiteDatabase.OPEN_READWRITE,
        ).use { db ->
            db.execSQL("PRAGMA writable_schema = ON;")
            db.execSQL(
                "UPDATE sqlite_master SET sql = replace(sql, \"'none', \" , '') " +
                    "WHERE type = 'table' AND name IN ('equipment', 'catalog_exercise_equipment');",
            )
            db.execSQL("PRAGMA writable_schema = OFF;")
            db.execSQL("PRAGMA user_version = 7;")
        }

        val migrated = openRepository()
        assertEquals(custom.equipment.equipmentId, loadDraft(migrated).exercises.single().equipmentId)
        SQLiteDatabase.openDatabase(
            context.getDatabasePath(databaseName).path,
            null,
            SQLiteDatabase.OPEN_READONLY,
        ).use { db ->
            db.rawQuery("PRAGMA user_version;", null).use { cursor ->
                assertTrue(cursor.moveToFirst())
                assertEquals(8, cursor.getInt(0))
            }
            db.rawQuery(
                "SELECT eq.equipment_id, ps.reps, ps.weight_kg FROM session_exercises se " +
                    "JOIN equipment eq ON eq.id = se.equipment_row_id " +
                    "JOIN performed_sets ps ON ps.session_exercise_row_id = se.id;",
                null,
            ).use { cursor ->
                assertTrue(cursor.moveToFirst())
                assertEquals(custom.equipment.equipmentId, cursor.getString(0))
                assertEquals(7, cursor.getInt(1))
                assertEquals(42.5, cursor.getDouble(2), 0.0)
            }
            db.rawQuery("PRAGMA foreign_key_check;", null).use { cursor ->
                assertFalse(cursor.moveToFirst())
            }
        }
    }

    @Test
    fun approvedAndroidLegPressIdentityMigrationPreservesHistoryDraftAndEquipment() {
        val initial = openRepository()
        initial.listExercises() /* Materialize the test database before raw fixture inserts. */
        initial.close()
        repository = null
        val path = context.getDatabasePath(databaseName).path
        SQLiteDatabase.openDatabase(path, null, SQLiteDatabase.OPEN_READWRITE).use { db ->
            db.execSQL(
                "INSERT INTO exercises(exercise_id,name,normalized_name,recording_mode,tracking_mode,data_fields) " +
                    "VALUES(?,?,?,?,?,?);",
                arrayOf<Any>(
                    "ex_d68a1af1-7247-4fb3-a48b-da8516906a29",
                    "Leg press",
                    "leg press",
                    "sets",
                    "reps",
                    0,
                ),
            )
            val exerciseRow = db.rawQuery(
                "SELECT id FROM exercises WHERE exercise_id=?;",
                arrayOf("ex_d68a1af1-7247-4fb3-a48b-da8516906a29"),
            ).use { cursor -> cursor.moveToFirst(); cursor.getLong(0) }
            val equipmentRow = db.rawQuery(
                "SELECT id FROM equipment WHERE equipment_id='leg_press';",
                null,
            ).use { cursor -> cursor.moveToFirst(); cursor.getLong(0) }
            db.execSQL(
                "INSERT INTO sessions(session_id,started_at,session_type) VALUES(?,?,?);",
                arrayOf("se_leg_press_migration", "2026-09-08T15:00:00+02:00", "training"),
            )
            val sessionRow = db.rawQuery(
                "SELECT id FROM sessions WHERE session_id='se_leg_press_migration';",
                null,
            ).use { cursor -> cursor.moveToFirst(); cursor.getLong(0) }
            db.execSQL(
                "INSERT INTO session_exercises(session_row_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields,equipment_row_id,entry_id) " +
                    "VALUES(?,?,?,?,?,?,?,?);",
                arrayOf<Any>(sessionRow, exerciseRow, 0, "sets", "reps", 0, equipmentRow, "sxe_leg_press_migration"),
            )
            val occurrence = db.rawQuery(
                "SELECT id FROM session_exercises WHERE entry_id='sxe_leg_press_migration';",
                null,
            ).use { cursor -> cursor.moveToFirst(); cursor.getLong(0) }
            db.execSQL(
                "INSERT INTO performed_sets(session_exercise_row_id,position,reps,weight_kg) VALUES(?,?,?,?);",
                arrayOf<Any>(occurrence, 0, 9, 80.0),
            )
            db.execSQL(
                "INSERT INTO active_session_draft(id,session_type,selected_exercise_row_id,selected_exercise_label,selected_equipment_id,set_count_text,reps_text,duration_text,speed_text,distance_text,updated_at) " +
                    "VALUES(1,?,?,?,?,?,?,?,?,?,?);",
                arrayOf<Any>("training", exerciseRow, "Leg press", "leg_press", "1", "7", "", "", "", "2026-09-08T15:00:00+02:00"),
            )
            db.execSQL(
                "INSERT INTO draft_session_exercises(draft_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields,equipment_row_id,entry_id) " +
                    "VALUES(?,?,?,?,?,?,?,?);",
                arrayOf<Any>(1, exerciseRow, 0, "sets", "reps", 0, equipmentRow, "sxe_draft_leg_press_migration"),
            )
            val draftRow = db.rawQuery(
                "SELECT id FROM draft_session_exercises WHERE entry_id='sxe_draft_leg_press_migration';",
                null,
            ).use { cursor -> cursor.moveToFirst(); cursor.getLong(0) }
            db.execSQL(
                "INSERT INTO draft_performed_sets(draft_exercise_row_id,position,reps,weight_kg) VALUES(?,?,?,?);",
                arrayOf<Any>(draftRow, 0, 7, 60.0),
            )
        }

        val migrated = openRepository()
        val detail = migrated.getSessionDetail("se_leg_press_migration")!!
        assertEquals("sxe_leg_press_migration", detail.exercises.single().entryId)
        assertEquals("ex_b432623f-bfe9-4daf-a653-60ec7fdffbde", detail.exercises.single().exerciseId)
        assertEquals(80.0, detail.exercises.single().sets.single().weightKg!!, 0.0)
        assertNotNull(detail.exercises.single().equipmentDisplayName)
        val draft = loadDraft(migrated)
        assertEquals("ex_b432623f-bfe9-4daf-a653-60ec7fdffbde", draft.exercises.single().exercise.exerciseId)
        assertEquals("sxe_draft_leg_press_migration", draft.exercises.single().entryId)
        assertEquals(60.0, draft.exercises.single().sets.single().weightKg!!, 0.0)
        migrated.close(); repository = null

        val reopened = openRepository()
        assertEquals(1, reopened.listExercises().count {
            it.exerciseId == "ex_b432623f-bfe9-4daf-a653-60ec7fdffbde"
        })
        assertFalse(reopened.listExercises().any {
            it.exerciseId == "ex_d68a1af1-7247-4fb3-a48b-da8516906a29"
        })
    }

    @Test
    fun versionFourMigrationPreservesCompletedBodyAndDurableDraftData() {
        createVersionFourFixture(context.getDatabasePath(databaseName).path)
        val repo = openRepository()

        assertEquals(1, repo.listExercises().size)
        assertEquals(1, repo.listSessions().size)
        assertEquals(1, repo.listBodyObservations().size)
        val loadedDraft = loadDraft(repo)
        assertEquals("Fixture", loadedDraft.form.selectedExercise?.name)
        assertEquals(1, loadedDraft.exercises.size)
        SQLiteDatabase.openDatabase(
            context.getDatabasePath(databaseName).path,
            null,
            SQLiteDatabase.OPEN_READONLY,
        ).use { db ->
            db.rawQuery("PRAGMA user_version;", null).use { cursor ->
                assertTrue(cursor.moveToFirst())
                assertEquals(8, cursor.getInt(0))
            }
            db.rawQuery("SELECT weight_kg FROM performed_sets WHERE id = 1;", null).use { cursor ->
                assertTrue(cursor.moveToFirst())
                assertTrue(cursor.isNull(0))
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

    private fun findRepositoryFile(relativePath: String): File {
        var directory = File(requireNotNull(System.getProperty("user.dir"))).canonicalFile
        repeat(5) {
            val candidate = File(directory, relativePath)
            if (candidate.isFile) return candidate
            directory = directory.parentFile ?: directory
        }
        fail("Repository file not found: $relativePath")
        throw AssertionError("unreachable")
    }

    /** Exact raw snapshots make idempotence cover every persisted business
     * table, including stable IDs and FKs that higher-level projections omit. */
    private fun snapshotBusinessTables(path: String): Map<String, List<List<String>>> {
        val snapshot = linkedMapOf<String, List<List<String>>>()
        SQLiteDatabase.openDatabase(path, null, SQLiteDatabase.OPEN_READONLY).use { db ->
            val tables = mutableListOf<String>()
            db.rawQuery(
                "SELECT name FROM sqlite_master WHERE type='table' " +
                    "AND name NOT LIKE 'sqlite_%' AND name<>'android_metadata' ORDER BY name;",
                null,
            ).use { cursor ->
                while (cursor.moveToNext()) tables += cursor.getString(0)
            }
            for (table in tables) {
                val quotedTable = quoteSqlIdentifier(table)
                val columns = mutableListOf<String>()
                db.rawQuery("PRAGMA table_info($quotedTable);", null).use { cursor ->
                    while (cursor.moveToNext()) columns += cursor.getString(1)
                }
                val orderBy = columns.joinToString(",") { quoteSqlIdentifier(it) }
                val rows = mutableListOf<List<String>>()
                db.rawQuery("SELECT * FROM $quotedTable ORDER BY $orderBy;", null).use { cursor ->
                    while (cursor.moveToNext()) {
                        rows += (0 until cursor.columnCount).map { index ->
                            when (cursor.getType(index)) {
                                android.database.Cursor.FIELD_TYPE_NULL -> "null"
                                android.database.Cursor.FIELD_TYPE_INTEGER -> "integer:${cursor.getLong(index)}"
                                android.database.Cursor.FIELD_TYPE_FLOAT ->
                                    "float:${java.lang.Double.toHexString(cursor.getDouble(index))}"
                                android.database.Cursor.FIELD_TYPE_STRING -> "string:${cursor.getString(index)}"
                                android.database.Cursor.FIELD_TYPE_BLOB ->
                                    "blob:${cursor.getBlob(index).contentToString()}"
                                else -> throw AssertionError("Unknown SQLite field type")
                            }
                        }
                    }
                }
                snapshot[table] = rows
            }
        }
        return snapshot
    }

    private fun quoteSqlIdentifier(value: String): String =
        "\"${value.replace("\"", "\"\"")}\""

    /** A real v4 shape: completed history plus the v4 durable draft tables. */
    private fun createVersionFourFixture(path: String) {
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
            db.execSQL(
                "CREATE TABLE active_session_draft(id INTEGER PRIMARY KEY CHECK(id = 1), " +
                    "session_type TEXT NOT NULL, selected_exercise_row_id INTEGER REFERENCES exercises(id) ON DELETE SET NULL, " +
                    "selected_exercise_label TEXT, set_count_text TEXT NOT NULL, reps_text TEXT NOT NULL, " +
                    "duration_text TEXT NOT NULL, speed_text TEXT NOT NULL, distance_text TEXT NOT NULL, updated_at TEXT NOT NULL);"
            )
            db.execSQL(
                "CREATE TABLE draft_session_exercises(id INTEGER PRIMARY KEY, draft_id INTEGER NOT NULL " +
                    "REFERENCES active_session_draft(id) ON DELETE CASCADE, exercise_row_id INTEGER NOT NULL " +
                    "REFERENCES exercises(id) ON DELETE RESTRICT, position INTEGER NOT NULL, recording_mode TEXT NOT NULL, " +
                    "tracking_mode TEXT NOT NULL, data_fields INTEGER NOT NULL, UNIQUE(draft_id, position), UNIQUE(draft_id, exercise_row_id));"
            )
            db.execSQL(
                "CREATE TABLE draft_performed_sets(id INTEGER PRIMARY KEY, draft_exercise_row_id INTEGER NOT NULL " +
                    "REFERENCES draft_session_exercises(id) ON DELETE CASCADE, position INTEGER NOT NULL, reps INTEGER, " +
                    "duration_seconds INTEGER, UNIQUE(draft_exercise_row_id, position));"
            )
            db.execSQL(
                "CREATE TABLE draft_continuous_activity(id INTEGER PRIMARY KEY, draft_exercise_row_id INTEGER NOT NULL UNIQUE " +
                    "REFERENCES draft_session_exercises(id) ON DELETE CASCADE, duration_seconds INTEGER NOT NULL, speed_kmh REAL, distance_km REAL);"
            )
            db.execSQL(
                "INSERT INTO active_session_draft VALUES(1, 'training', 1, 'Fixture', '3', '3x10', '', '', '', '2026-01-02T03:04:05+01:00');"
            )
            db.execSQL("INSERT INTO draft_session_exercises VALUES(1, 1, 1, 0, 'sets', 'reps', 0);")
            db.execSQL("INSERT INTO draft_performed_sets VALUES(1, 1, 0, 10, NULL);")
            db.execSQL("PRAGMA user_version = 4;")
        }
    }
}
