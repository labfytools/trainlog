package com.labfytools.trainlog.data

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import java.util.UUID
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class ProgramsProjectionRepositoryTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    private fun program(title: String = "Cycle lisible"): JSONObject =
        JSONObject()
            .put("program_id", "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa")
            .put("revision_id", "pgr_bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb")
            .put("title", title)
            .put("note", "Lecture seule")
            .put("state", "active")
            .put("start_date", "2026-09-21")
            .put("end_date", JSONObject.NULL)
            .put("created_at", "2026-09-18T10:00:00Z")
            .put("updated_at", "2026-09-18T11:00:00Z")
            .put("source_format", "trainlog-program")
            .put("source_version", 1)
            .put("source_payload_sha256", "a".repeat(64))
            .put(
                "sessions",
                JSONArray().put(
                    JSONObject()
                        .put("program_session_id", "pgs_cccccccc-cccc-4ccc-8ccc-cccccccccccc")
                        .put("title", "Séance A")
                        .put("session_type", "training")
                        .put("planned_for", JSONObject.NULL)
                        .put("note", JSONObject.NULL)
                        .put(
                            "occurrences",
                            JSONArray().put(
                                JSONObject()
                                    .put("entry_id", "pge_dddddddd-dddd-4ddd-8ddd-dddddddddddd")
                                    .put("exercise_id", "ex_eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee")
                                    .put("equipment_id", JSONObject.NULL)
                                    .put("load_mode", "external")
                                    .put("rest_seconds", 90)
                                    .put("target_sets", 3)
                                    .put("target_reps", 8)
                                    .put("target_duration_seconds", JSONObject.NULL)
                                    .put("target_weight_kg", 40.0)
                                    .put("notes", JSONObject.NULL),
                            ),
                        ),
                ),
            )

    private fun artifact(programs: JSONArray, deletions: JSONArray = JSONArray()): String =
        JSONObject()
            .put("format", "trainlog-programs")
            .put("version", 1)
            .put("generated_at", "2026-09-18T12:00:00Z")
            .put("programs", programs)
            .put("deletions", deletions)
            .toString()

    private fun deletion(): JSONObject =
        JSONObject()
            .put("deletion_id", "pgd_ffffffff-ffff-4fff-8fff-ffffffffffff")
            .put("program_id", "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa")
            .put("predecessor_revision_id", "pgr_bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb")
            .put("revision_id", "pgr_11111111-1111-4111-8111-111111111111")
            .put("requested_at", "2026-09-18T12:30:00Z")

    @Test
    fun projectionReplayConflictDeletionAndRestartAreDurable() {
        val name = "programs-${UUID.randomUUID()}.db"
        var repository = TrainlogRepository(context, name)
        try {
            assertEquals(ProgramsImportResult.Applied(1, 0, 0), repository.applyProgramsV1Json(artifact(JSONArray().put(program()))))
            assertEquals(ProgramsImportResult.Applied(0, 0, 1), repository.applyProgramsV1Json(artifact(JSONArray().put(program()))))
            val summary = repository.listSyncedPrograms().single()
            assertEquals(1, summary.sessionCount)
            val detail = repository.getSyncedProgram(summary.programId)!!
            assertEquals("ex_eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee", detail.sessions.single().occurrences.single().exerciseName)

            val conflict = repository.applyProgramsV1Json(artifact(JSONArray().put(program("Divergent"))))
            assertTrue(conflict is ProgramsImportResult.Invalid)
            assertEquals("Cycle lisible", repository.listSyncedPrograms().single().title)

            assertEquals(ProgramsImportResult.Applied(0, 1, 0), repository.applyProgramsV1Json(artifact(JSONArray(), JSONArray().put(deletion()))))
            assertTrue(repository.listSyncedPrograms().isEmpty())
        } finally {
            repository.close()
        }

        repository = TrainlogRepository(context, name)
        try {
            assertEquals(ProgramsImportResult.Applied(0, 0, 1), repository.applyProgramsV1Json(artifact(JSONArray().put(program()))))
            assertTrue(repository.listSyncedPrograms().isEmpty())
            assertTrue(repository.listEquipment().isNotEmpty())
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }

    @Test
    fun malformedNestedDocumentDoesNotPartiallyDelete() {
        val name = "programs-invalid-${UUID.randomUUID()}.db"
        val repository = TrainlogRepository(context, name)
        try {
            repository.applyProgramsV1Json(artifact(JSONArray().put(program())))
            val malformed = program().apply { getJSONArray("sessions").getJSONObject(0).remove("title") }
            val result = repository.applyProgramsV1Json(artifact(JSONArray().put(malformed), JSONArray().put(deletion())))
            assertTrue(result is ProgramsImportResult.Invalid)
            assertEquals(1, repository.listSyncedPrograms().size)
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }
}
