package com.labfytools.trainlog.data

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionDraftForm
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionSetDraft
import com.labfytools.trainlog.model.TrackingMode
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.util.UUID
import java.util.zip.ZipInputStream
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class AndroidBackupServiceTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    @Test
    fun manifestUsesInjectedCanonicalProductVersion() {
        val name = "backup-version-${UUID.randomUUID()}.db"
        val repository = TrainlogRepository(context, name)
        try {
            val output = ByteArrayOutputStream()
            val service = AndroidBackupService(context, "0.1.3-test")
            assertTrue(service.create(repository, output) is AndroidBackupResult.Success)
            var manifest: JSONObject? = null
            ZipInputStream(ByteArrayInputStream(output.toByteArray())).use { zip ->
                while (true) {
                    val entry = zip.nextEntry ?: break
                    if (entry.name == "manifest.json") {
                        manifest = JSONObject(zip.readBytes().toString(Charsets.UTF_8))
                    }
                }
            }
            assertEquals("0.1.3-test", checkNotNull(manifest).getString("product_version"))
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }

    @Test
    fun completeBackupRestoresDraftPeerLedgersAndPreferences() {
        val name = "backup-${UUID.randomUUID()}.db"
        var repository = TrainlogRepository(context, name)
        val service = AndroidBackupService(context)
        try {
            val exercise =
                (repository.createExercise(
                        NewExerciseProfile(
                            "Backup exercise",
                            RecordingMode.SETS,
                            TrackingMode.REPS,
                            0,
                        )
                    ) as CreateExerciseResult.Created)
                    .exercise
            assertEquals(
                ActiveDraftMutationResult.Saved,
                repository.saveActiveSessionDraft(
                    ActiveSessionDraft(
                        exercises =
                            listOf(
                                SessionExerciseDraft(
                                    exercise = exercise,
                                    sets = listOf(SessionSetDraft(reps = 9, weightKg = 31.5)),
                                )
                            ),
                        form = SessionDraftForm(selectedExercise = exercise, weightText = "31,"),
                    )
                ),
            )
            val peer = SyncGenerationService(repository).peerId()
            context
                .getSharedPreferences("trainlog_presentation_settings", Context.MODE_PRIVATE)
                .edit()
                .putString("language", "en")
                .commit()
            val output = ByteArrayOutputStream()
            assertTrue(service.create(repository, output) is AndroidBackupResult.Success)
            val verified = service.verify(ByteArrayInputStream(output.toByteArray())).getOrThrow()
            assertTrue(service.restore(repository, verified, name) is AndroidBackupResult.Success)
            repository = TrainlogRepository(context, name)
            val draft = repository.loadActiveSessionDraft()
            assertTrue(draft is ActiveDraftLoadResult.Loaded)
            assertEquals("31,", (draft as ActiveDraftLoadResult.Loaded).draft.form.weightText)
            assertEquals(peer, SyncGenerationService(repository).peerId())
            assertEquals(
                "en",
                context
                    .getSharedPreferences("trainlog_presentation_settings", Context.MODE_PRIVATE)
                    .getString("language", null),
            )
        } finally {
            repository.close()
            context.deleteDatabase(name)
        }
    }

    @Test
    fun corruptAndTraversalContainersAreRejectedBeforeRestore() {
        val service = AndroidBackupService(context)
        val output = ByteArrayOutputStream()
        java.util.zip.ZipOutputStream(output).use { zip ->
            zip.putNextEntry(java.util.zip.ZipEntry("../database.sqlite"))
            zip.write(byteArrayOf(1))
            zip.closeEntry()
        }
        assertTrue(service.verify(ByteArrayInputStream(output.toByteArray())).isFailure)
    }
}
