package com.labfytools.trainlog.data

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.provider.Settings
import androidx.test.core.app.ApplicationProvider
import java.io.File
import java.nio.file.Files
import java.util.UUID
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class DirectExchangeStorageTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    @Test fun `missing permission backend refuses publication explicitly`() {
        val result = DirectExchangePublisher { null }.snapshot()
        assertTrue(result is DirectExchangeSnapshotResult.Error)
        assertTrue((result as DirectExchangeSnapshotResult.Error).message.contains("Accès fichiers requis"))
    }

    @Test fun `manifest and authorization action expose all files access`() {
        val permission = context.packageManager.getPackageInfo(
            context.packageName,
            PackageManager.GET_PERMISSIONS,
        ).requestedPermissions.orEmpty()
        assertTrue(permission.contains(Manifest.permission.MANAGE_EXTERNAL_STORAGE))
        val intent = directStoragePermissionIntent(context)
        assertTrue(
            intent.action == Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION ||
                intent.action == Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION,
        )
        if (intent.action == Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION) {
            assertEquals("package:${context.packageName}", intent.dataString)
        }
    }

    @Test fun `directory is created and canonical file is atomically replaced`() {
        val root = Files.createTempDirectory("trainlog-direct-storage-").toFile()
        val directory = File(root, "Documents/Trainlog")
        try {
            val backend = directExchangeDirectoryForPath(directory)
            assertNotNull(backend)
            assertTrue(directory.isDirectory)
            val publisher = DirectExchangePublisher { backend }
            repeat(10) { publication ->
                val snapshot = (publisher.snapshot() as DirectExchangeSnapshotResult.Ready).snapshot
                assertEquals(null, snapshot.writeJson("artifact.json", "value-$publication"))
            }
            assertEquals("value-9", File(directory, "artifact.json").readText())
            assertEquals(listOf("artifact.json"), directory.listFiles().orEmpty().map(File::getName))
            assertFalse(directory.listFiles().orEmpty().any { it.name.matches(Regex("artifact \\(\\d+\\)\\.json")) })
        } finally {
            root.deleteRecursively()
        }
    }

    @Test fun `historical suffixes and Download backup remain untouched`() {
        val root = Files.createTempDirectory("trainlog-direct-legacy-").toFile()
        val active = File(root, "Documents/Trainlog").apply { mkdirs() }
        val legacy = File(root, "Download/Trainlog").apply { mkdirs() }
        val recovery = File(root, "Download/Trainlog-saf-recovery-20260914").apply { mkdirs() }
        File(active, "artifact.json").writeText("old")
        File(active, "artifact (1).json").writeText("history")
        File(legacy, "keep.json").writeText("legacy")
        File(recovery, "keep.json").writeText("recovery")
        try {
            val backend = checkNotNull(directExchangeDirectoryForPath(active))
            val snapshot = (DirectExchangePublisher { backend }.snapshot() as DirectExchangeSnapshotResult.Ready).snapshot
            assertEquals(null, snapshot.writeJson("artifact.json", "new"))
            assertEquals("new", File(active, "artifact.json").readText())
            assertEquals("history", File(active, "artifact (1).json").readText())
            assertEquals("legacy", File(legacy, "keep.json").readText())
            assertEquals("recovery", File(recovery, "keep.json").readText())
        } finally {
            root.deleteRecursively()
        }
    }

    @Test fun `fresh database installs catalog before aliases in one inbox pass`() {
        val directory = Files.createTempDirectory("trainlog-direct-fresh-").toFile()
        val databaseName = "direct-fresh-${UUID.randomUUID()}.db"
        val sourceId = "ex_11111111-1111-4111-8111-111111111111"
        val canonicalId = "ex_22222222-2222-4222-8222-222222222222"
        val repository = TrainlogRepository(context, databaseName)
        try {
            fun exercise(id: String, name: String) = JSONObject()
                .put("exercise_id", id).put("name", name)
                .put("recording_mode", "sets").put("tracking_mode", "reps")
                .put("data_fields", 0)
            File(directory, "trainlog-pc-catalog-v1.json").writeText(
                JSONObject().put("format", "trainlog-pc-catalog").put("version", 1)
                    .put("exercises", JSONArray().put(exercise(sourceId, "Source"))
                        .put(exercise(canonicalId, "Canonique"))).toString(),
            )
            File(directory, "trainlog-exercise-aliases-v1.json").writeText(
                JSONObject().put("format", "trainlog-exercise-aliases").put("version", 1)
                    .put("aliases", JSONArray().put(JSONObject()
                        .put("source_exercise_id", sourceId)
                        .put("canonical_exercise_id", canonicalId))).toString(),
            )
            val result = SyncCatalogInbox(context, repository)
                .importPcCatalogFromDirectoryForTest(directory)
            assertTrue(result is CatalogInboxResult.Imported)
            assertEquals(listOf(canonicalId), repository.listExercises().map { it.exerciseId })
        } finally {
            repository.close()
            context.deleteDatabase(databaseName)
            directory.deleteRecursively()
        }
    }

    @Test fun `fresh catalog row adopts only initial resolved machine identity`() {
        val databaseName = "direct-profile-${UUID.randomUUID()}.db"
        val id = "ex_33333333-3333-4333-8333-333333333333"
        val repository = TrainlogRepository(context, databaseName)
        try {
            val catalog = JSONObject().put("format", "trainlog-pc-catalog").put("version", 1)
                .put("exercises", JSONArray().put(JSONObject().put("exercise_id", id)
                    .put("name", "Machine").put("recording_mode", "sets")
                    .put("tracking_mode", "reps").put("data_fields", 0)))
            assertTrue(repository.applyPcCatalogJson(catalog.toString()) is PcCatalogImportResult.Applied)
            val history = JSONArray().put(JSONObject().put("revision_id", "pr_legacy_v1")
                .put("parent_revision_id", JSONObject.NULL).put("recording_mode", "sets")
                .put("tracking_mode", "reps").put("data_fields", 0).put("legacy_seed", true))
            val item = JSONObject().put("exercise_id", id).put("recording_mode", "sets")
                .put("tracking_mode", "reps").put("data_fields", 0)
                .put("load_semantics", "external").put("machine_variant", "plate_loaded")
                .put("machine_provenance", JSONObject.NULL).put("scientific_profile_id", "sp_test_v1")
                .put("science_state", "unresolved").put("legacy_equipment_id", "test_machine")
                .put("revision_id", "pr_legacy_v1").put("parent_revision_id", JSONObject.NULL)
                .put("legacy_seed", true).put("history", history)
            val profile = JSONObject().put("format", "trainlog-exercise-profile-state")
                .put("version", 1).put("generated_at", "2026-09-14T18:00:00+02:00")
                .put("exercises", JSONArray().put(item))
            assertTrue(repository.applyExerciseProfileStateJson(profile.toString()) is ExerciseProfileStateImportResult.Applied)
            val published = JSONObject(repository.buildExerciseProfileStateJson())
                .getJSONArray("exercises").getJSONObject(0)
            assertEquals("unresolved", published.getString("science_state"))
            assertEquals("sp_test_v1", published.getString("scientific_profile_id"))

            val changed = JSONObject(profile.toString())
            changed.getJSONArray("exercises").getJSONObject(0).put("machine_variant", "changed")
            assertEquals(ExerciseProfileStateImportResult.Conflict(id),
                repository.applyExerciseProfileStateJson(changed.toString()))
        } finally {
            repository.close()
            context.deleteDatabase(databaseName)
        }
    }
}
