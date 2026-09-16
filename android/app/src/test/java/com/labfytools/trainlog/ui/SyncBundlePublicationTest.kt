/*
 * Regression coverage for SyncBundlePublicationTest.
 *
 * Exercises production contracts without owning runtime behavior or persistent formats.
 */
package com.labfytools.trainlog.ui

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.data.SaveFeedbackResult
import com.labfytools.trainlog.data.SaveSessionResult
import com.labfytools.trainlog.data.SyncExporter
import com.labfytools.trainlog.data.SyncRequestOutbox
import com.labfytools.trainlog.data.SyncRequestResult
import com.labfytools.trainlog.data.TrainlogRepository
import com.labfytools.trainlog.data.DirectExchangeDirectoryAccess
import com.labfytools.trainlog.data.DirectExchangeDocument
import com.labfytools.trainlog.data.DIRECT_EXCHANGE_LOG_TAG
import com.labfytools.trainlog.data.DirectExchangePublisher
import com.labfytools.trainlog.data.DirectExchangeSnapshotResult
import com.labfytools.trainlog.model.SessionDraft
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionSetDraft
import java.util.UUID
import java.io.ByteArrayOutputStream
import java.io.OutputStream
import java.io.File
import org.json.JSONObject
import org.json.JSONArray
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import org.robolectric.shadows.ShadowLog
import kotlinx.coroutines.runBlocking

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class SyncBundlePublicationTest {
    private lateinit var context: Context
    private lateinit var databaseName: String
    private lateinit var repository: TrainlogRepository
    private lateinit var directory: FakeSafDirectory

    @Before fun setUp() {
        context = ApplicationProvider.getApplicationContext()
        databaseName = "sync-bundle-${UUID.randomUUID()}.db"
        repository = TrainlogRepository(context, databaseName)
        directory = FakeSafDirectory()
    }

    @After fun tearDown() {
        repository.close()
        context.deleteDatabase(databaseName)
    }

    @Test fun `request publishes and replaces one complete current canonical bundle`() = runBlocking {
        val identities = listOf(
            "ex_999fd503-ce89-49f9-979d-c19e6adbe80d" to "Planche droite sol",
            "ex_b35fff35-9c97-4c82-9053-7d1f2a23d3ac" to "Planche face sol",
            "ex_e32c40a6-72b4-4233-a0db-41b27e990c96" to "Planche gauche sol",
        )
        val catalog = JSONObject().put("format", "trainlog-pc-catalog").put("version", 1)
            .put("exercises", JSONArray().also { items -> identities.forEach { (id, name) ->
                items.put(JSONObject().put("exercise_id", id).put("name", name)
                    .put("recording_mode", "sets").put("tracking_mode", "duration")
                    .put("data_fields", 0))
            } })
        repository.applyPcCatalogJson(catalog.toString())
        val exercises = identities.map { (id, _) ->
            repository.listExercises().single { it.exerciseId == id }
        }
        /* The canonical BODY ZONES fixture exists in the authorized SAF tree;
         * no MediaStore candidate is provided, reproducing an ownerless row
         * omitted from the application's scoped MediaStore view. */
        directory.seed("trainlog-exercise-body-zones-v1.json", "stale canonical")
        (1..31).forEach { copy ->
            directory.seed("trainlog-exercise-body-zones-v1 ($copy).json", "historical copy")
        }
        val exporter = SyncExporter(context, repository) { directory }
        val outbox = SyncRequestOutbox { directory }
        assertTrue(exporter.exportMobileBundle() is com.labfytools.trainlog.data.SyncExportResult.Exported)
        assertEquals(1, directory.enumerationCount)
        val entry = SessionExerciseDraft(
            exercise = exercises.first(),
            sets = listOf(SessionSetDraft(durationSeconds = 30)),
        )
        val saved = repository.saveSession(SessionDraft(listOf(entry))) as SaveSessionResult.Saved
        val feedback = repository.saveExerciseFeedback(
            saved.sessionId,
            entry.entryId,
            "ressenti ajouté après la publication précédente",
            "2026-09-13T12:59:00+02:00",
        )
        assertTrue(feedback is SaveFeedbackResult.Saved)

        val writesBeforeFirstClick = directory.writeCounts.toMap()
        assertTrue(publishBundleAndRequest(exporter, outbox) is SyncRequestResult.Requested)
        assertEquals(2, directory.enumerationCount)
        val required = listOf(
            "trainlog-mobile-export-v3.json",
            "trainlog-mobile-equipment-definitions-v1.json",
            "trainlog-equipment-associations-v2.json",
            "trainlog-exercise-aliases-v1.json",
            "trainlog-exercise-body-zones-v1.json",
            "trainlog-training-feedback-v2.json",
            "trainlog-exercise-profile-state-v1.json",
            "trainlog-sync-request-v1.json",
        )
        required.forEach { assertEquals(it, 1, directory.canonicalCount(it)) }
        required.forEach { name ->
            assertEquals("one click must write $name exactly once", 1,
                directory.writeCount(name) - (writesBeforeFirstClick[name] ?: 0))
        }
        val profile = JSONObject(readCanonical("trainlog-exercise-profile-state-v1.json"))
        val profileItems = profile.getJSONArray("exercises")
        identities.forEach { (id, _) ->
            val item = (0 until profileItems.length()).map { profileItems.getJSONObject(it) }
                .single { it.getString("exercise_id") == id }
            assertEquals("sets", item.getString("recording_mode"))
            assertEquals("duration", item.getString("tracking_mode"))
            assertEquals(0, item.getInt("data_fields"))
            assertEquals("pr_legacy_v1", item.getJSONArray("history").getJSONObject(0).getString("revision_id"))
            assertEquals(item.getString("revision_id"), item.getJSONArray("history")
                .getJSONObject(item.getJSONArray("history").length() - 1).getString("revision_id"))
        }
        assertTrue(readCanonical("trainlog-training-feedback-v2.json")
            .contains("ressenti ajouté après la publication précédente"))

        /* Re-publication updates exact canonical objects instead of creating
         * numbered duplicates, and preserves stable feedback/revision IDs. */
        val feedbackBefore = JSONObject(readCanonical("trainlog-training-feedback-v2.json"))
        ShadowLog.clear()
        val bodyCreatesBefore = directory.createCount("trainlog-exercise-body-zones-v1.json")
        val writesBeforeSecondClick = directory.writeCounts.toMap()
        assertTrue(publishBundleAndRequest(exporter, outbox) is SyncRequestResult.Requested)
        assertEquals(3, directory.enumerationCount)
        val phases = ShadowLog.getLogsForTag(DIRECT_EXCHANGE_LOG_TAG).map { it.msg }
        assertEquals(1, phases.count { it.startsWith("SYNC_BUNDLE coordinator.export.begin") })
        val snapshotPhases = phases.filter { it.startsWith("DIRECT_STORAGE snapshot.success ") }
        assertEquals(1, snapshotPhases.size)
        assertTrue("SAF snapshot ran on main: $snapshotPhases",
            snapshotPhases.none { it.contains("thread=main") })
        val writeBegins = phases.filter { it.contains(" direct.write.success") }
        assertEquals(required.size, writeBegins.size)
        assertTrue("SAF write ran on main: $writeBegins",
            writeBegins.none { it.contains("thread=main") })
        val bodyWritten = phases.indexOfFirst { it.startsWith("BODY_ZONES direct.write.success") }
        val requestStart = phases.indexOfFirst { it == "SYNC_REQUEST coordinator.request.begin" }
        assertTrue("BODY ZONES write did not complete", bodyWritten >= 0)
        assertTrue("request must be published after the complete export", requestStart > bodyWritten)
        assertTrue("outbound path must not call MediaStore insert/update/pending: $phases",
            phases.none { it.contains(" insert.") || it.contains(" publish.") || it.contains("IS_PENDING") })
        assertEquals(bodyCreatesBefore, directory.createCount("trainlog-exercise-body-zones-v1.json"))
        assertEquals(0, directory.canonicalCount("trainlog-exercise-body-zones-v1 (32).json"))
        (1..31).forEach { copy ->
            assertEquals(1, directory.canonicalCount("trainlog-exercise-body-zones-v1 ($copy).json"))
        }
        required.forEach { assertEquals(it, 1, directory.canonicalCount(it)) }
        required.forEach { name ->
            assertEquals("repeated click must write $name exactly once", 1,
                directory.writeCount(name) - (writesBeforeSecondClick[name] ?: 0))
        }
        val feedbackAfter = JSONObject(readCanonical("trainlog-training-feedback-v2.json"))
        assertEquals(feedbackBefore.getJSONArray("exercise_feedback").toString(),
            feedbackAfter.getJSONArray("exercise_feedback").toString())
    }

    @Test fun `created exact document updates transaction index without rescan`() {
        val publisher = DirectExchangePublisher { directory }
        val opened = publisher.snapshot() as DirectExchangeSnapshotResult.Ready
        assertEquals(1, directory.enumerationCount)
        assertEquals(null, opened.snapshot.writeJson("foo.json", "first"))
        assertEquals(null, opened.snapshot.writeJson("foo.json", "second"))
        assertEquals(1, directory.enumerationCount)
        assertEquals(1, directory.createCount("foo.json"))
        assertEquals(2, directory.writeCount("foo.json"))
        assertEquals("second", directory.read("foo.json"))
        assertEquals(0, directory.canonicalCount("foo (1).json"))
    }

    @Test fun `presentation language cannot change synchronization payload`() = runBlocking {
        val exporter = SyncExporter(context, repository) { directory }
        assertTrue(exporter.exportMobileBundle() is com.labfytools.trainlog.data.SyncExportResult.Exported)
        val frenchPayload = JSONObject(readCanonical("trainlog-mobile-export-v3.json")).apply {
            remove("generated_at")
        }.toString()

        LanguageSettingsOwner(context).select(AppLanguage.ENGLISH)
        assertTrue(exporter.exportMobileBundle() is com.labfytools.trainlog.data.SyncExportResult.Exported)

        val englishPayload = JSONObject(readCanonical("trainlog-mobile-export-v3.json")).apply {
            remove("generated_at")
        }.toString()
        assertEquals(frenchPayload, englishPayload)
        assertFalse(readCanonical("trainlog-mobile-export-v3.json").contains("language_tag"))
    }

    @Test fun `empty directory creates exact canonical name and final content`() {
        val opened = DirectExchangePublisher { directory }.snapshot() as DirectExchangeSnapshotResult.Ready

        assertEquals(null, opened.snapshot.writeJson("artifact.json", "new export"))

        assertEquals(1, directory.canonicalCount("artifact.json"))
        assertEquals("new export", directory.read("artifact.json"))
        assertFalse(directory.names().any { it.matches(Regex("artifact \\(\\d+\\)\\.json")) })
    }

    @Test fun `ten publications rewrite one canonical document without numbered copies`() {
        repeat(10) { publication ->
            val opened = DirectExchangePublisher { directory }.snapshot() as DirectExchangeSnapshotResult.Ready
            assertEquals(null, opened.snapshot.writeJson("artifact.json", "export-$publication"))
        }

        assertEquals(1, directory.createCount("artifact.json"))
        assertEquals(10, directory.writeCount("artifact.json"))
        assertEquals("export-9", directory.read("artifact.json"))
        assertEquals(setOf("artifact.json"), directory.names())
    }

    @Test fun `historical numbered copies are never selected instead of canonical`() {
        directory.seed("artifact (1).json", "history one")
        directory.seed("artifact (2).json", "history two")
        directory.seed("artifact.json", "old canonical")
        val opened = DirectExchangePublisher { directory }.snapshot() as DirectExchangeSnapshotResult.Ready

        assertEquals(null, opened.snapshot.writeJson("artifact.json", "new canonical"))

        assertEquals("new canonical", directory.read("artifact.json"))
        assertEquals("history one", directory.read("artifact (1).json"))
        assertEquals("history two", directory.read("artifact (2).json"))
        assertEquals(0, directory.writeCount("artifact (1).json"))
        assertEquals(0, directory.writeCount("artifact (2).json"))
    }

    @Test fun `historical numbered copy alone does not become canonical target`() {
        directory.seed("artifact (1).json", "history")
        val opened = DirectExchangePublisher { directory }.snapshot() as DirectExchangeSnapshotResult.Ready

        assertEquals(null, opened.snapshot.writeJson("artifact.json", "canonical"))

        assertEquals("canonical", directory.read("artifact.json"))
        assertEquals("history", directory.read("artifact (1).json"))
    }

    @Test fun `provider rename fails explicitly before writing conflict copy`() {
        directory.createdNameOverride = "artifact (1).json"
        val opened = DirectExchangePublisher { directory }.snapshot() as DirectExchangeSnapshotResult.Ready

        assertEquals(
            "DIRECT_STORAGE_CANONICAL_NAME_CONFLICT: attendu=artifact.json obtenu=artifact (1).json",
            opened.snapshot.writeJson("artifact.json", "must not be written"),
        )

        assertEquals(0, directory.writeCount("artifact (1).json"))
        assertEquals(0, directory.canonicalCount("artifact.json"))
    }

    @Test fun `raw exact lookup recovers canonical omitted from ordinary discovery`() {
        directory.seedRawOnly("artifact.json", "old canonical")
        directory.seed("artifact (1).json", "history")
        repeat(10) { publication ->
            val opened = DirectExchangePublisher { directory }.snapshot() as DirectExchangeSnapshotResult.Ready
            assertEquals(null, opened.snapshot.writeJson("artifact.json", "export-$publication"))
        }
        assertEquals(0, directory.createCount("artifact.json"))
        assertEquals(10, directory.writeCount("artifact.json"))
        assertEquals("export-9", directory.read("artifact.json"))
        assertEquals("history", directory.read("artifact (1).json"))
        assertFalse(directory.names().contains("artifact (2).json"))
    }

    @Test fun `multiple raw exact canonical rows fail deterministically`() {
        directory.seedRawOnly("artifact.json", "one")
        directory.rawExactDuplicates = 2
        val opened = DirectExchangePublisher { directory }.snapshot() as DirectExchangeSnapshotResult.Ready
        assertEquals(
            "DIRECT_STORAGE_DUPLICATE_CANONICAL: nom=artifact.json count=2",
            opened.snapshot.writeJson("artifact.json", "new"),
        )
        assertEquals(0, directory.writeCount("artifact.json"))
    }

    private fun readCanonical(name: String): String = directory.read(name)

    private class FakeSafDirectory : DirectExchangeDirectoryAccess {
        private val contents = linkedMapOf<String, ByteArray>()
        private val creates = mutableMapOf<String, Int>()
        val writeCounts = mutableMapOf<String, Int>()
        var enumerationCount = 0
            private set
        var createdNameOverride: String? = null
        var rawExactDuplicates = 1
        private val discoveryHidden = mutableSetOf<String>()

        fun seed(name: String, value: String) { contents[name] = value.toByteArray() }
        fun seedRawOnly(name: String, value: String) {
            seed(name, value)
            discoveryHidden += name
        }
        fun read(name: String): String = contents.getValue(name).toString(Charsets.UTF_8)
        fun canonicalCount(name: String): Int = if (contents.containsKey(name)) 1 else 0
        fun createCount(name: String): Int = creates[name] ?: 0
        fun writeCount(name: String): Int = writeCounts[name] ?: 0
        fun names(): Set<String> = contents.keys

        override fun listDirectChildren(): List<DirectExchangeDocument> {
            enumerationCount++
            return contents.keys.filterNot(discoveryHidden::contains).map(::document)
        }

        override fun findExact(displayName: String): List<DirectExchangeDocument> =
            if (contents.containsKey(displayName)) {
                List(rawExactDuplicates) { document(displayName) }
            } else {
                emptyList()
            }

        override fun createJson(displayName: String): DirectExchangeDocument {
            val actualName = createdNameOverride ?: displayName
            check(!contents.containsKey(actualName))
            creates[displayName] = createCount(displayName) + 1
            contents[actualName] = byteArrayOf()
            return document(actualName)
        }

        override fun openForRewrite(document: DirectExchangeDocument): OutputStream =
            object : ByteArrayOutputStream() {
                override fun close() {
                    super.close()
                    contents[document.displayName] = toByteArray()
                    writeCounts[document.displayName] = writeCount(document.displayName) + 1
                }
            }

        private fun document(name: String) = DirectExchangeDocument(
            File("/storage/emulated/0/Documents/Trainlog", name),
            name,
        )
    }
}
