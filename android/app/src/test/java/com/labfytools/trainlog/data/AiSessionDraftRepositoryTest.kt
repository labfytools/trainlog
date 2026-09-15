/*
 * Regression coverage for AiSessionDraftRepositoryTest.
 *
 * Exercises production contracts without owning runtime behavior or persistent formats.
 */
package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.TrackingMode
import org.json.JSONArray
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import java.util.UUID
import java.io.File
import java.nio.file.Files

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class AiSessionDraftRepositoryTest {
    private lateinit var context: Context
    private lateinit var databaseName: String
    private lateinit var repository: TrainlogRepository

    @Before
    fun setUp() {
        context = ApplicationProvider.getApplicationContext()
        databaseName = "ai-drafts-${UUID.randomUUID()}.db"
        repository = TrainlogRepository(context, databaseName)
    }

    @After
    fun tearDown() {
        repository.close()
        context.deleteDatabase(databaseName)
    }

    @Test
    fun importReplayAndDeleteTombstoneNeverDuplicateOrResurrect() {
        val exercise = exercise()
        val json = artifact(exercise.exerciseId)

        assertEquals(AiSessionDraftImportResult.Applied(1, 0), repository.applyAiSessionDraftsJson(json))
        assertEquals(AiSessionDraftImportResult.Applied(0, 1), repository.applyAiSessionDraftsJson(json))
        assertEquals(1, repository.listAiSessionDrafts().size)

        assertEquals(ActiveDraftMutationResult.Saved, repository.deleteAiSessionDraft(DRAFT_ID))
        assertTrue(repository.listAiSessionDrafts().isEmpty())
        assertEquals(AiSessionDraftImportResult.Applied(0, 1), repository.applyAiSessionDraftsJson(json))
        assertTrue(repository.listAiSessionDrafts().isEmpty())
        assertTrue(repository.loadActiveSessionDraft() is ActiveDraftLoadResult.None)
        assertTrue(repository.listSessions().isEmpty())
    }

    @Test
    fun pendingDraftIsListed() {
        val exercise = exercise()
        assertEquals(AiSessionDraftImportResult.Applied(1, 0),
            repository.applyAiSessionDraftsJson(artifact(exercise.exerciseId)))

        assertEquals(listOf(DRAFT_ID), repository.listAiSessionDrafts().map { it.draftId })
    }

    @Test
    fun startedDraftIsExcludedFromPendingList() {
        val exercise = exercise()
        repository.applyAiSessionDraftsJson(artifact(exercise.exerciseId))

        assertEquals(StartAiSessionDraftResult.Started, repository.startAiSessionDraft(DRAFT_ID))
        assertTrue(repository.listAiSessionDrafts().isEmpty())
    }

    @Test
    fun deletedDraftIsExcludedFromPendingList() {
        val exercise = exercise()
        repository.applyAiSessionDraftsJson(artifact(exercise.exerciseId))

        assertEquals(ActiveDraftMutationResult.Saved, repository.deleteAiSessionDraft(DRAFT_ID))
        assertTrue(repository.listAiSessionDrafts().isEmpty())
    }

    @Test
    fun twoPendingDraftsUseDeterministicPlannedDateOrder() {
        val exercise = exercise()
        val laterDraftId = "aid_22345678-1234-4abc-8abc-123456789abc"
        val laterEntryId = "sxe_bbcdefab-cdef-4abc-8abc-abcdefabcdef"
        val drafts = JSONArray()
            .put(JSONObject(artifact(exercise.exerciseId, draftId = laterDraftId,
                entryId = laterEntryId, plannedFor = "2026-09-16")).getJSONArray("drafts").getJSONObject(0))
            .put(JSONObject(artifact(exercise.exerciseId, plannedFor = "2026-09-15"))
                .getJSONArray("drafts").getJSONObject(0))
        val json = JSONObject().put("format", "trainlog-ai-session-drafts").put("version", 1)
            .put("generated_at", "2026-09-14T08:00:00Z").put("drafts", drafts).toString()

        assertEquals(AiSessionDraftImportResult.Applied(2, 0), repository.applyAiSessionDraftsJson(json))
        assertEquals(listOf(DRAFT_ID, laterDraftId), repository.listAiSessionDrafts().map { it.draftId })
    }

    @Test
    fun explicitStartCopiesTargetsOnlyAndTombstonesProposal() {
        val exercise = exercise()
        assertEquals(AiSessionDraftImportResult.Applied(1, 0),
            repository.applyAiSessionDraftsJson(artifact(exercise.exerciseId)))

        assertEquals(StartAiSessionDraftResult.Started, repository.startAiSessionDraft(DRAFT_ID))
        val active = (repository.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded).draft
        assertEquals(1, active.exercises.size)
        assertEquals(3, active.exercises.single().plan?.sets)
        assertEquals(10, active.exercises.single().plan?.reps)
        assertTrue(active.exercises.single().sets.isEmpty())
        assertTrue(repository.listAiSessionDrafts().isEmpty())
        assertTrue(repository.listSessions().isEmpty())
        assertEquals(AiSessionDraftImportResult.Applied(0, 1),
            repository.applyAiSessionDraftsJson(artifact(exercise.exerciseId)))
    }

    @Test
    fun activeDraftConflictLeavesBothDraftsUnchanged() {
        val exercise = exercise()
        repository.applyAiSessionDraftsJson(artifact(exercise.exerciseId))
        assertEquals(ActiveDraftMutationResult.Saved, repository.startActiveSessionDraft())
        val before = repository.loadActiveSessionDraft()

        assertEquals(StartAiSessionDraftResult.ExistingActiveDraft, repository.startAiSessionDraft(DRAFT_ID))
        assertEquals(before, repository.loadActiveSessionDraft())
        assertEquals(listOf(DRAFT_ID), repository.listAiSessionDrafts().map { it.draftId })
    }

    @Test
    fun strictParserRejectsUnknownKeysAndNonUtcTimeWithoutWrites() {
        val exercise = exercise()
        val unknown = JSONObject(artifact(exercise.exerciseId)).put("extra", true).toString()
        assertTrue(repository.applyAiSessionDraftsJson(unknown) is AiSessionDraftImportResult.Invalid)
        val nonUtc = artifact(exercise.exerciseId).replace("2026-09-14T08:00:00Z", "2026-09-14T10:00:00+02:00")
        assertTrue(repository.applyAiSessionDraftsJson(nonUtc) is AiSessionDraftImportResult.Invalid)
        assertTrue(repository.listAiSessionDrafts().isEmpty())
    }

    @Test
    fun syncInboxImportsExactCompanionFilenameWithExistingPrerequisites() {
        val exercise = exercise()
        val directory = Files.createTempDirectory("trainlog-ai-inbox-").toFile()
        try {
            File(directory, "trainlog-ai-session-drafts-v1.json").writeText(artifact(exercise.exerciseId))
            val inbox = SyncCatalogInbox(context, repository)
            assertEquals(null, inbox.importAiSessionDraftsFromDirectoryForTest(directory))
            assertEquals(listOf(DRAFT_ID), repository.listAiSessionDrafts().map { it.draftId })
        } finally {
            directory.deleteRecursively()
        }
    }

    @Test
    fun importsAliasNonzeroFieldsAndUpperTargetBoundsUsingCanonicalExercise() {
        val source = exercise("Source")
        val canonical = exercise("Canonique")
        val aliases = JSONObject().put("format", "trainlog-exercise-aliases").put("version", 1)
            .put("aliases", JSONArray().put(JSONObject()
                .put("source_exercise_id", source.exerciseId)
                .put("canonical_exercise_id", canonical.exerciseId))).toString()
        assertEquals(ExerciseAliasImportResult.Applied(1, 0), repository.applyExerciseAliasesJson(aliases))
        SQLiteDatabase.openDatabase(context.getDatabasePath(databaseName).path, null,
            SQLiteDatabase.OPEN_READWRITE).use { db ->
            db.execSQL("PRAGMA ignore_check_constraints=ON")
            db.execSQL("UPDATE exercises SET data_fields=3 WHERE exercise_id=?", arrayOf(canonical.exerciseId))
        }

        assertEquals(AiSessionDraftImportResult.Applied(1, 0),
            repository.applyAiSessionDraftsJson(artifact(source.exerciseId, 3, 99, 999)))
        val entry = repository.listAiSessionDrafts().single().entries.single()
        assertEquals(canonical.exerciseId, entry.exercise.exerciseId)
        assertEquals(3, entry.exercise.dataFields)
        assertEquals(99, entry.plan?.sets)
        assertEquals(999, entry.plan?.reps)
    }

    @Test
    fun rejectsUnknownAliasAssistanceAndRepsAboveSharedLimit() {
        val exercise = exercise()
        assertTrue(repository.applyAiSessionDraftsJson(
            artifact("ex_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa")) is AiSessionDraftImportResult.Invalid)
        assertTrue(repository.applyAiSessionDraftsJson(
            artifact(exercise.exerciseId).replace("\"load_mode\":\"none\"", "\"load_mode\":\"assistance\""))
            is AiSessionDraftImportResult.Invalid)
        assertTrue(repository.applyAiSessionDraftsJson(
            artifact(exercise.exerciseId, targetReps = 1000)) is AiSessionDraftImportResult.Invalid)
    }

    @Test fun `replay validates full companion and optional text is canonical`() {
        val exercise = exercise()
        val valid = artifact(exercise.exerciseId)
        assertTrue(repository.applyAiSessionDraftsJson(valid) is AiSessionDraftImportResult.Applied)
        val invalidReplay = valid.replace("\"reps\":10", "\"reps\":1000")
        assertTrue(repository.applyAiSessionDraftsJson(invalidReplay) is AiSessionDraftImportResult.Invalid)
        assertTrue(repository.applyAiSessionDraftsJson(
            valid.replace("Haut du corps", " Haut du corps")) is AiSessionDraftImportResult.Invalid)
    }

    @Test fun `optional title and notes limits count Unicode code points atomically`() {
        val exercise = exercise()
        val glyph = "\uD83D\uDE00"
        assertTrue(repository.applyAiSessionDraftsJson(artifact(exercise.exerciseId,
            title = glyph.repeat(120), notes = glyph.repeat(2000))) is AiSessionDraftImportResult.Applied)
        assertEquals(1, repository.listAiSessionDrafts().size)

        val first = JSONObject(artifact(exercise.exerciseId, draftId =
            "aid_22345678-1234-4abc-8abc-123456789abc", entryId =
            "sxe_bbcdefab-cdef-4abc-8abc-abcdefabcdef")).getJSONArray("drafts").getJSONObject(0)
        val tooLongTitle = JSONObject(artifact(exercise.exerciseId, draftId =
            "aid_32345678-1234-4abc-8abc-123456789abc", entryId =
            "sxe_cbcdefab-cdef-4abc-8abc-abcdefabcdef", title = glyph.repeat(121)))
            .getJSONArray("drafts").getJSONObject(0)
        val tooLongNotes = JSONObject(artifact(exercise.exerciseId, draftId =
            "aid_42345678-1234-4abc-8abc-123456789abc", entryId =
            "sxe_dbcdefab-cdef-4abc-8abc-abcdefabcdef", notes = glyph.repeat(2001)))
            .getJSONArray("drafts").getJSONObject(0)
        fun companion(second: JSONObject) = JSONObject().put("format", "trainlog-ai-session-drafts")
            .put("version", 1).put("generated_at", "2026-09-14T08:00:00Z")
            .put("drafts", JSONArray().put(first).put(second)).toString()
        assertTrue(repository.applyAiSessionDraftsJson(companion(tooLongTitle)) is AiSessionDraftImportResult.Invalid)
        assertTrue(repository.applyAiSessionDraftsJson(companion(tooLongNotes)) is AiSessionDraftImportResult.Invalid)
        assertEquals(1, repository.listAiSessionDrafts().size)
        assertTrue(repository.applyAiSessionDraftsJson(artifact(exercise.exerciseId,
            draftId = "aid_52345678-1234-4abc-8abc-123456789abc", entryId =
            "sxe_ebcdefab-cdef-4abc-8abc-abcdefabcdef", title = "x".repeat(120),
            notes = "x".repeat(2000))) is AiSessionDraftImportResult.Applied)
    }

    private fun exercise(name: String = "Développé test", dataFields: Int = 0) = (repository.createExercise(NewExerciseProfile(
        name = name, recordingMode = RecordingMode.SETS,
        trackingMode = TrackingMode.REPS, dataFields = dataFields,
    )) as CreateExerciseResult.Created).exercise

    private fun artifact(
        exerciseId: String,
        dataFields: Int = 0,
        targetSets: Int = 3,
        targetReps: Int = 10,
        draftId: String = DRAFT_ID,
        entryId: String = ENTRY_ID,
        plannedFor: String = "2026-09-15",
        title: String = "Haut du corps",
        notes: String = "Proposition de test",
    ): String {
        val target = JSONObject().put("sets", targetSets).put("reps", targetReps)
            .put("duration_seconds", JSONObject.NULL).put("weight_kg", JSONObject.NULL)
        val entry = JSONObject()
            .put("entry_id", entryId)
            .put("position", 0)
            .put("exercise_id", exerciseId)
            .put("recording_mode", "sets")
            .put("tracking_mode", "reps")
            .put("data_fields", dataFields)
            .put("equipment_id", JSONObject.NULL)
            .put("load_mode", "none")
            .put("rest_seconds", 90)
            .put("target", target)
        val draft = JSONObject()
            .put("draft_id", draftId)
            .put("created_at", "2026-09-14T08:00:00Z")
            .put("planned_for", plannedFor)
            .put("session_type", "training")
            .put("title", title)
            .put("notes", notes)
            .put("entries", JSONArray().put(entry))
        return JSONObject()
            .put("format", "trainlog-ai-session-drafts")
            .put("version", 1)
            .put("generated_at", "2026-09-14T08:00:00Z")
            .put("drafts", JSONArray().put(draft))
            .toString()
    }

    companion object {
        private const val DRAFT_ID = "aid_12345678-1234-4abc-8abc-123456789abc"
        private const val ENTRY_ID = "sxe_abcdefab-cdef-4abc-8abc-abcdefabcdef"
    }
}
