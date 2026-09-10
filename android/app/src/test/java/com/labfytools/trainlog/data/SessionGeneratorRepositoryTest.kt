package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.SessionDraft
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionSetDraft
import org.json.JSONArray
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.time.OffsetDateTime
import java.util.UUID

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class SessionGeneratorRepositoryTest {
    private lateinit var context: Context
    private lateinit var databaseName: String
    private var repository: TrainlogRepository? = null

    @Before
    fun setUp() {
        context = ApplicationProvider.getApplicationContext()
        databaseName = "generator-repository-${UUID.randomUUID()}.db"
    }

    @After
    fun tearDown() {
        repository?.close()
        context.deleteDatabase(databaseName)
    }

    @Test
    fun previewIsReadOnlyAndAcceptCreatesOnlyAnOrdinaryTargetDraft() {
        val repo = openRepository()
        val before = mutableTableCounts()

        val generated = repo.generateSessionPreview(request())
        assertTrue(generated is SessionGenerationResult.Generated)
        val preview = (generated as SessionGenerationResult.Generated).preview
        assertTrue(preview.exercises.isNotEmpty())
        assertEquals(before, mutableTableCounts())

        assertEquals(AcceptGeneratedSessionResult.Accepted, repo.acceptGeneratedSession(preview))
        val loaded = repo.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded
        assertEquals(preview.exercises.size, loaded.draft.exercises.size)
        loaded.draft.exercises.zip(preview.exercises).forEach { (draft, proposed) ->
            assertEquals(proposed.exerciseId, draft.exercise.exerciseId)
            assertEquals(proposed.equipmentId, draft.equipmentId)
            assertEquals(proposed.plan, draft.plan)
            assertTrue(draft.sets.isEmpty())
        }
        assertTrue(repo.listSessions().isEmpty())

        val acceptedCounts = mutableTableCounts()
        assertEquals(AcceptGeneratedSessionResult.ExistingActiveDraft, repo.acceptGeneratedSession(preview))
        assertEquals(acceptedCounts, mutableTableCounts())

        repo.close()
        repository = null
        val reopened = openRepository()
        val restored = (reopened.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded).draft
        assertEquals(loaded.draft.exercises.map { it.plan }, restored.exercises.map { it.plan })

        val withActuals = restored.copy(exercises = restored.exercises.map { exercise ->
            val repetitions = requireNotNull(exercise.plan?.reps)
            exercise.copy(sets = List(requireNotNull(exercise.plan).sets) {
                SessionSetDraft(reps = repetitions, weightKg = exercise.plan.weightKg)
            })
        })
        assertEquals(ActiveDraftMutationResult.Saved, reopened.saveActiveSessionDraft(withActuals))
        val completed = reopened.finalizeActiveSessionDraft()
        assertTrue(completed is FinalizeActiveDraftResult.Saved)
        val sessionId = (completed as FinalizeActiveDraftResult.Saved).sessionId
        assertTrue(reopened.loadActiveSessionDraft() is ActiveDraftLoadResult.None)
        assertEquals(
            preview.exercises.map { it.plan },
            reopened.getSessionDetail(sessionId)!!.exercises.map { it.plan },
        )
    }

    @Test
    fun generationStreamsHistoryBeyondLegacyOccurrenceAndSetPreviewLimits() {
        val repo = openRepository()
        val exercise = exactExercise(repo)
        repeat(33) {
            val saved = repo.saveSession(SessionDraft(listOf(SessionExerciseDraft(
                exercise = exercise,
                equipmentId = EQUIPMENT_ID,
                sets = listOf(SessionSetDraft(reps = 10), SessionSetDraft(reps = 10)),
            ))))
            assertTrue(saved is SaveSessionResult.Saved)
        }
        val before = mutableTableCounts()
        val generated = repo.generateSessionPreview(request(referenceTime = OffsetDateTime.now().plusMinutes(1).toString()))
        assertTrue(generated is SessionGenerationResult.Generated)
        val exposure = (generated as SessionGenerationResult.Generated).preview.exposure
        assertEquals(66, exposure.within24h.primarySetCount)
        assertEquals(33, exposure.within24h.sessionCount)
        assertEquals(before, mutableTableCounts())
    }

    @Test
    fun invalidStoredTimestampFailsSpecificallyAndNeverWrites() {
        val repo = openRepository()
        val exercise = exactExercise(repo)
        assertTrue(repo.saveSession(SessionDraft(listOf(SessionExerciseDraft(
            exercise = exercise,
            equipmentId = EQUIPMENT_ID,
            sets = listOf(SessionSetDraft(reps = 10)),
        )))) is SaveSessionResult.Saved)
        directDatabase().use { it.execSQL("UPDATE sessions SET started_at='corrupt-time';") }
        val before = mutableTableCounts()

        val result = repo.generateSessionPreview(request())

        assertTrue(result is SessionGenerationResult.DatabaseError)
        assertTrue((result as SessionGenerationResult.DatabaseError).message.contains("invalid stored session timestamp"))
        assertEquals(before, mutableTableCounts())
        assertTrue(repo.loadActiveSessionDraft() is ActiveDraftLoadResult.None)
    }

    @Test
    fun doseEditRequalifiesObservedLoadAndDropsItWhenDoseNoLongerQualifies() {
        val repo = openRepository()
        val exercise = exactExercise(repo)
        assertTrue(repo.saveSession(SessionDraft(listOf(SessionExerciseDraft(
            exercise = exercise,
            equipmentId = EQUIPMENT_ID,
            sets = listOf(
                SessionSetDraft(reps = 10, weightKg = 50.0),
                SessionSetDraft(reps = 10, weightKg = 50.0),
            ),
        )))) is SaveSessionResult.Saved)
        val preview = (repo.generateSessionPreview(request(
            referenceTime = OffsetDateTime.now().plusMinutes(1).toString(),
        )) as SessionGenerationResult.Generated).preview
        assertEquals(50.0, preview.exercises.single().plan.weightKg!!, 0.0)

        val edited = repo.editGeneratedDose(
            preview, 0, targetSets = 3, targetRepetitions = 10,
            restSeconds = preview.exercises.single().plan.restSeconds,
            manualWeightKg = null,
        )

        assertTrue(edited is SessionGenerationResult.Generated)
        val changed = (edited as SessionGenerationResult.Generated).preview.exercises.single()
        assertEquals(3, changed.plan.sets)
        assertNull(changed.plan.weightKg)
        assertNull(changed.loadSourceSessionId)
        assertTrue(changed.estimatedSeconds > preview.exercises.single().estimatedSeconds)
    }

    @Test
    fun explicitEmptyEquipmentAvailabilityProducesAnEmptyReadOnlyPreview() {
        val repo = openRepository()
        val before = mutableTableCounts()
        val result = repo.generateSessionPreview(request().copy(availableEquipmentIds = emptySet()))
        assertTrue(result is SessionGenerationResult.Generated)
        assertTrue((result as SessionGenerationResult.Generated).preview.exercises.isEmpty())
        assertEquals(before, mutableTableCounts())
        assertFalse(result.preview.insufficientResolvedCandidates.not())
    }

    @Test
    fun preferencesAndExclusionsPassThroughWithoutCallerCandidateInjection() {
        val repo = openRepository()
        val before = mutableTableCounts()
        val preferred = repo.generateSessionPreview(request().copy(
            preferredExerciseIds = setOf(EXERCISE_ID),
        )) as SessionGenerationResult.Generated
        assertTrue("preferred_exercise" in preferred.preview.exercises.single().rationaleCodes)

        val excluded = repo.generateSessionPreview(request().copy(
            preferredExerciseIds = setOf(EXERCISE_ID),
            excludedExerciseIds = setOf(EXERCISE_ID),
        )) as SessionGenerationResult.Generated
        assertTrue(excluded.preview.exercises.isEmpty())

        val pattern = preferred.preview.exercises.single().patternIds.single()
        val excludedPattern = repo.generateSessionPreview(request().copy(
            excludedPatternIds = setOf(pattern),
        )) as SessionGenerationResult.Generated
        assertTrue(excludedPattern.preview.exercises.isEmpty())
        assertEquals(before, mutableTableCounts())
    }

    @Test
    fun formOptionsComeFromTheLoadedPolicyAndCanonicalBodyZones() {
        val repo = openRepository()
        val options = repo.sessionGenerationFormOptions()
        assertEquals(
            setOf(
                "full_body", "upper_body", "chest", "back", "shoulders", "arms", "core",
                "lower_body", "glutes", "thighs", "calves",
            ),
            options.zoneIds.toSet(),
        )
        assertEquals(setOf("general", "strength", "hypertrophy", "endurance"), options.goalIds.toSet())
        assertEquals(listOf(30, 45, 60), options.durationPresets)
        assertEquals(10..120, options.customMinutes)
        assertTrue(options.zoneIds.all { id -> repo.listBodyZones().any { it.zoneId == id } })
    }

    private fun openRepository(): TrainlogRepository =
        TrainlogRepository(context, databaseName).also { opened ->
            repository = opened
            // Opening SQLite is lazy; install the exact runtime half of the
            // bundled scientific identity before exercising candidate assembly.
            opened.listEquipment()
            if (opened.listExercises().none { it.exerciseId == EXERCISE_ID }) {
                val catalog = JSONObject()
                    .put("format", "trainlog-pc-catalog")
                    .put("version", 1)
                    .put("exercises", JSONArray().put(JSONObject()
                        .put("exercise_id", EXERCISE_ID)
                        .put("name", "Leg extension")
                        .put("recording_mode", "sets")
                        .put("tracking_mode", "reps")
                        .put("data_fields", 0)))
                check(opened.applyPcCatalogJson(catalog.toString()) is PcCatalogImportResult.Applied)
            }
        }

    private fun exactExercise(repo: TrainlogRepository) =
        requireNotNull(repo.listExercises().singleOrNull { it.exerciseId == EXERCISE_ID })

    private fun request(referenceTime: String = OffsetDateTime.now().plusMinutes(1).toString()) =
        SessionGenerationRequest(
            zoneId = "thighs",
            goalId = "general",
            durationMinutes = 30,
            referenceTime = referenceTime,
            availableEquipmentIds = setOf(EQUIPMENT_ID),
        )

    private fun directDatabase(): SQLiteDatabase = SQLiteDatabase.openDatabase(
        context.getDatabasePath(databaseName).absolutePath,
        null,
        SQLiteDatabase.OPEN_READWRITE,
    )

    private fun mutableTableCounts(): Map<String, Long> = directDatabase().use { db ->
        listOf(
            "sessions", "session_exercises", "performed_sets", "active_session_draft",
            "draft_session_exercises", "draft_performed_sets",
        ).associateWith { table ->
            db.rawQuery("SELECT COUNT(*) FROM $table;", null).use { cursor ->
                check(cursor.moveToFirst())
                cursor.getLong(0)
            }
        }
    }

    private companion object {
        const val EXERCISE_ID = "ex_1872246a-39ae-44dc-b58d-f87e90ca49ab"
        const val EQUIPMENT_ID = "leg_extension"
    }
}
