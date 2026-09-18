package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionDraft
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionSetDraft
import com.labfytools.trainlog.model.TrackingMode
import java.nio.file.Files
import java.util.UUID
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [35])
class SyncGenerationServiceTest {
    private val context: Context = ApplicationProvider.getApplicationContext()

    @Test
    fun acknowledgedOutboundRemainsResumableUntilInboundIsTerminal() {
        val sourceName = "generation-resume-source-${UUID.randomUUID()}.db"
        val destinationName = "generation-resume-destination-${UUID.randomUUID()}.db"
        val root = Files.createTempDirectory("trainlog-generation-resume-").toFile()
        val source = TrainlogRepository(context, sourceName)
        val destination = TrainlogRepository(context, destinationName)
        try {
            val sourceService = SyncGenerationService(source)
            val destinationService = SyncGenerationService(destination)
            val runId = "sy_${UUID.randomUUID()}"
            val outbound =
                sourceService.capture(
                    java.io.File(root, "source-owned"),
                    destinationService.peerId(),
                    runId,
                )
            val outboundDirectory =
                sourceService.publish(outbound, java.io.File(root, "source-objects"))
            sourceService.acceptAcknowledgement(
                destinationService.consume(outboundDirectory).toByteArray()
            )

            val coordinator = SyncGenerationForegroundCoordinator(source)
            assertEquals(
                outbound.generationId,
                coordinator.resumableGeneration(runId, destinationService.peerId())?.generationId,
            )

            val inbound =
                destinationService.capture(
                    java.io.File(root, "destination-owned"),
                    sourceService.peerId(),
                    runId,
                )
            sourceService.consume(
                destinationService.publish(inbound, java.io.File(root, "destination-objects"))
            )
            assertEquals(null, coordinator.resumableGeneration(runId, destinationService.peerId()))
        } finally {
            source.close()
            destination.close()
            context.deleteDatabase(sourceName)
            context.deleteDatabase(destinationName)
            root.deleteRecursively()
        }
    }

    @Test
    fun v19MigrationPreservesDataAndInventsNoGenerationState() {
        val name = "generation-migration-${UUID.randomUUID()}.db"
        TrainlogRepository(context, name).also {
            assertTrue(
                it.createExercise(
                    NewExerciseProfile(
                        "Migration sentinel",
                        RecordingMode.SETS,
                        TrackingMode.REPS,
                        0,
                    )
                ) is CreateExerciseResult.Created
            )
            it.close()
        }
        val path = context.getDatabasePath(name)
        SQLiteDatabase.openDatabase(path.absolutePath, null, SQLiteDatabase.OPEN_READWRITE).use { db
            ->
            listOf(
                    "sync_generation_archives",
                    "sync_causal_publications",
                    "sync_acknowledgements",
                    "sync_consumed_generations",
                    "sync_generation_artifacts",
                    "sync_generations",
                    "sync_peer_identity",
                )
                .forEach { db.execSQL("DROP TABLE $it") }
            db.execSQL("PRAGMA user_version=19")
        }
        val migrated = TrainlogRepository(context, name).also { it.listExercises() }
        try {
            SQLiteDatabase.openDatabase(path.absolutePath, null, SQLiteDatabase.OPEN_READONLY)
                .use { db ->
                    db.rawQuery("PRAGMA user_version", null).use {
                        assertTrue(it.moveToFirst())
                        assertEquals(22, it.getInt(0))
                    }
                    db.rawQuery("SELECT COUNT(*) FROM exercises", null).use {
                        assertTrue(it.moveToFirst())
                        assertTrue(it.getInt(0) > 0)
                    }
                    listOf(
                            "sync_generations",
                            "sync_consumed_generations",
                            "sync_acknowledgements",
                            "sync_causal_publications",
                        )
                        .forEach { table ->
                            db.rawQuery("SELECT COUNT(*) FROM $table", null).use {
                                assertTrue(it.moveToFirst())
                                assertEquals(0, it.getInt(0))
                            }
                        }
                }
        } finally {
            migrated.close()
            context.deleteDatabase(name)
        }
    }

    @Test
    fun lateSemanticFailureRollsBackWholeAndroidGeneration() {
        val sourceName = "generation-rollback-source-${UUID.randomUUID()}.db"
        val destinationName = "generation-rollback-destination-${UUID.randomUUID()}.db"
        val root = Files.createTempDirectory("trainlog-generation-rollback-").toFile()
        val source = TrainlogRepository(context, sourceName)
        val destination = TrainlogRepository(context, destinationName)
        try {
            val exercise =
                (source.createExercise(
                        NewExerciseProfile(
                            "Rollback exercise",
                            RecordingMode.SETS,
                            TrackingMode.REPS,
                            0,
                        )
                    ) as CreateExerciseResult.Created)
                    .exercise
            assertTrue(
                source.saveSession(
                    SessionDraft(
                        listOf(
                            SessionExerciseDraft(
                                exercise = exercise,
                                sets = listOf(SessionSetDraft(reps = 5)),
                            )
                        )
                    )
                ) is SaveSessionResult.Saved
            )
            val destinationService = SyncGenerationService(destination)
            val identity =
                destinationService.capture(root, "peer_11111111-1111-4111-8111-111111111111")
            val peer =
                JSONObject(java.io.File(identity.stagingDirectory, "manifest.json").readText())
                    .getJSONObject("producer")
                    .getString("peer_id")
            val sourceService = SyncGenerationService(source)
            val captured = sourceService.capture(root, peer)
            val published = sourceService.publish(captured, java.io.File(root, "objects"))
            val bad = "{}".toByteArray()
            java.io.File(published, "feedback-v2.json").writeBytes(bad)
            val manifest = JSONObject(java.io.File(published, "manifest.json").readText())
            val artifacts = manifest.getJSONArray("artifacts")
            for (index in 0 until artifacts.length()) if (
                artifacts.getJSONObject(index).getString("logical_name") == "feedback"
            )
                artifacts
                    .getJSONObject(index)
                    .put("size", bad.size)
                    .put(
                        "sha256",
                        java.security.MessageDigest.getInstance("SHA-256").digest(bad).joinToString(
                            ""
                        ) {
                            "%02x".format(it)
                        },
                    )
            java.io.File(published, "manifest.json").writeText(manifest.toString())
            assertEquals(
                "rejected",
                JSONObject(destinationService.consume(published)).getString("result"),
            )
            assertTrue(destination.listSessions().isEmpty())
            SQLiteDatabase.openDatabase(
                    context.getDatabasePath(destinationName).absolutePath,
                    null,
                    SQLiteDatabase.OPEN_READONLY,
                )
                .use { db ->
                    db.rawQuery(
                            "SELECT COUNT(*) FROM sync_consumed_generations WHERE result='rejected'",
                            null,
                        )
                        .use {
                            assertTrue(it.moveToFirst())
                            assertEquals(1, it.getInt(0))
                        }
                }
        } finally {
            source.close()
            destination.close()
            context.deleteDatabase(sourceName)
            context.deleteDatabase(destinationName)
            root.deleteRecursively()
        }
    }

    @Test
    fun causalOperationBytesRemainImmutableAcrossAndroidGenerations() {
        val name = "generation-causal-${UUID.randomUUID()}.db"
        val root = Files.createTempDirectory("trainlog-generation-causal-").toFile()
        val repo = TrainlogRepository(context, name)
        try {
            val exercise =
                (repo.createExercise(
                        NewExerciseProfile(
                            "Causal generation",
                            RecordingMode.SETS,
                            TrackingMode.REPS,
                            0,
                        )
                    ) as CreateExerciseResult.Created)
                    .exercise
            assertTrue(
                repo.saveSession(
                    SessionDraft(
                        listOf(
                            SessionExerciseDraft(
                                exercise = exercise,
                                sets = listOf(SessionSetDraft(reps = 4)),
                            )
                        )
                    )
                ) is SaveSessionResult.Saved
            )
            val sessionId = repo.listSessions().single().sessionId
            assertTrue(
                repo.deleteCausally("session", sessionId, "peer_android_generation")
                    is CausalDeleteResult.Applied
            )
            val service = SyncGenerationService(repo)
            val peer = "peer_33333333-3333-4333-8333-333333333333"
            val first = service.capture(root, peer)
            val second = service.capture(root, peer)
            fun operation(captured: CapturedSyncGeneration) =
                JSONObject(
                        java.io
                            .File(captured.stagingDirectory, "causal-deletions-v1.json")
                            .readText()
                    )
                    .getJSONArray("operations")
                    .getJSONObject(0)
            assertEquals(operation(first).toString(), operation(second).toString())
            assertTrue(operation(first).isNull("publication_context"))
            SQLiteDatabase.openDatabase(
                    context.getDatabasePath(name).absolutePath,
                    null,
                    SQLiteDatabase.OPEN_READONLY,
                )
                .use { db ->
                    db.rawQuery(
                            "SELECT SUM(first_emission),COUNT(*)-SUM(first_emission) FROM sync_causal_publications",
                            null,
                        )
                        .use {
                            assertTrue(it.moveToFirst())
                            assertEquals(1, it.getInt(0))
                            assertEquals(1, it.getInt(1))
                        }
                }
        } finally {
            repo.close()
            context.deleteDatabase(name)
            root.deleteRecursively()
        }
    }

    @Test
    fun androidCaptureUsesOneSnapshotAgainstControlledWriter() {
        val name = "generation-snapshot-${UUID.randomUUID()}.db"
        val repo = TrainlogRepository(context, name)
        try {
            val first =
                (repo.createExercise(
                        NewExerciseProfile("Snapshot A", RecordingMode.SETS, TrackingMode.REPS, 0)
                    ) as CreateExerciseResult.Created)
                    .exercise
            assertTrue(
                repo.saveSession(
                    SessionDraft(
                        listOf(
                            SessionExerciseDraft(
                                exercise = first,
                                sets = listOf(SessionSetDraft(reps = 3)),
                            )
                        )
                    )
                ) is SaveSessionResult.Saved
            )
            val attempted = CountDownLatch(1)
            var writer: Thread? = null
            val captured =
                repo.captureSyncGenerationArtifacts {
                    writer =
                        Thread {
                                attempted.countDown()
                                repo.createExercise(
                                    NewExerciseProfile(
                                        "Snapshot B",
                                        RecordingMode.SETS,
                                        TrackingMode.REPS,
                                        0,
                                    )
                                )
                            }
                            .also { it.start() }
                    assertTrue(attempted.await(5, TimeUnit.SECONDS))
                }
            val completedWriter = checkNotNull(writer)
            completedWriter.join(5000)
            assertTrue(!completedWriter.isAlive)
            assertEquals(
                1,
                JSONObject(captured.getValue("catalog")).getJSONArray("exercises").length(),
            )
            assertEquals(
                1,
                JSONObject(captured.getValue("exercise-profile-state"))
                    .getJSONArray("exercises")
                    .length(),
            )
            assertEquals(2, repo.listExercises().size)
        } finally {
            repo.close()
            context.deleteDatabase(name)
        }
    }

    @Test
    fun androidGenerationCapturePublishConsumeAndAckSurviveReopen() {
        val sourceName = "generation-source-${UUID.randomUUID()}.db"
        val destinationName = "generation-destination-${UUID.randomUUID()}.db"
        val desktopDestinationName = "generation-desktop-destination-${UUID.randomUUID()}.db"
        val root = Files.createTempDirectory("trainlog-generation-android-").toFile()
        var source = TrainlogRepository(context, sourceName)
        var destination = TrainlogRepository(context, destinationName)
        var desktopDestination = TrainlogRepository(context, desktopDestinationName)
        try {
            val created =
                source.createExercise(
                    NewExerciseProfile(
                        "Generation exercise",
                        RecordingMode.SETS,
                        TrackingMode.REPS,
                        0,
                    )
                ) as CreateExerciseResult.Created
            val saved =
                source.saveSession(
                    SessionDraft(
                        listOf(
                            SessionExerciseDraft(
                                exercise = created.exercise,
                                sets = listOf(SessionSetDraft(reps = 7, weightKg = 21.5)),
                            )
                        )
                    )
                )
            assertTrue(saved is SaveSessionResult.Saved)
            val destinationService = SyncGenerationService(destination)
            val identityCapture =
                destinationService.capture(root, "peer_11111111-1111-4111-8111-111111111111")
            val destinationPeer =
                JSONObject(
                        java.io.File(identityCapture.stagingDirectory, "manifest.json").readText()
                    )
                    .getJSONObject("producer")
                    .getString("peer_id")
            val sourceService = SyncGenerationService(source)
            val captured = sourceService.capture(root, destinationPeer)
            val published = sourceService.publish(captured, java.io.File(root, "objects"))
            val before =
                published.listFiles()!!.associate { it.name to it.readBytes().contentHashCode() }
            assertEquals(published, sourceService.publish(captured, java.io.File(root, "objects")))
            assertEquals(
                before,
                published.listFiles()!!.associate { it.name to it.readBytes().contentHashCode() },
            )
            val ack = destinationService.consume(published)
            assertEquals("acknowledged", sourceService.acceptAcknowledgement(ack.toByteArray()))
            source.close()
            destination.close()
            source = TrainlogRepository(context, sourceName)
            destination = TrainlogRepository(context, destinationName)
            assertEquals(1, destination.listSessions().size)
            assertEquals(
                "unchanged",
                SyncGenerationService(source).acceptAcknowledgement(ack.toByteArray()),
            )
            assertEquals(ack, SyncGenerationService(destination).consume(published))

            // A desktop-created mutation uses the public C persistence API,
            // then the production desktop generation entry point. This is not
            // an Android generation re-exported through desktop.
            val repositoryRoot =
                generateSequence(java.io.File(checkNotNull(System.getProperty("user.dir")))) {
                        it.parentFile
                    }
                    .first { java.io.File(it, "tools/sync_generation_exchange.py").isFile }
            val desktopDb = java.io.File(root, "desktop-source.sqlite")
            fun run(vararg command: String): String {
                val process =
                    ProcessBuilder(*command)
                        .directory(repositoryRoot)
                        .redirectErrorStream(true)
                        .start()
                val text = process.inputStream.bufferedReader().readText()
                assertEquals(
                    "command failed: ${command.joinToString(" ")}\n$text",
                    0,
                    process.waitFor(),
                )
                return text
            }
            val desktopConsumerDb = java.io.File(root, "desktop-consumer.sqlite")
            run(
                java.io.File(repositoryRoot, "build/tui/sync-generation-fixture").absolutePath,
                desktopConsumerDb.absolutePath,
            )
            val desktopPeer =
                Regex("PEER_ID=(peer_[^\\s]+)")
                    .find(
                        run(
                            "python3",
                            java.io
                                .File(repositoryRoot, "tools/sync_generation_exchange.py")
                                .absolutePath,
                            "peer-id",
                            "--database",
                            desktopConsumerDb.absolutePath,
                        )
                    )!!
                    .groupValues[1]
            val androidToDesktop = sourceService.capture(root, desktopPeer)
            val androidPublished =
                sourceService.publish(
                    androidToDesktop,
                    java.io.File(root, "android-desktop-objects"),
                )
            val androidAckFile = java.io.File(root, "android-desktop-ack.json")
            run(
                "python3",
                java.io.File(repositoryRoot, "tools/sync_generation_exchange.py").absolutePath,
                "consume-desktop",
                androidPublished.absolutePath,
                "--database",
                desktopConsumerDb.absolutePath,
                "--ack-output",
                androidAckFile.absolutePath,
            )
            assertEquals(
                "acknowledged",
                sourceService.acceptAcknowledgement(androidAckFile.readBytes()),
            )
            val androidSessionId = source.listSessions().single().sessionId
            assertEquals(
                "1",
                run(
                        "sqlite3",
                        desktopConsumerDb.absolutePath,
                        "SELECT count(*) FROM sessions WHERE session_id='$androidSessionId';",
                    )
                    .trim(),
            )

            run(
                java.io.File(repositoryRoot, "build/tui/sync-generation-fixture").absolutePath,
                desktopDb.absolutePath,
            )
            val androidPeerCapture =
                SyncGenerationService(desktopDestination)
                    .capture(root, "peer_22222222-2222-4222-8222-222222222222")
            val androidPeer =
                JSONObject(
                        java.io
                            .File(androidPeerCapture.stagingDirectory, "manifest.json")
                            .readText()
                    )
                    .getJSONObject("producer")
                    .getString("peer_id")
            val desktopOwned = java.io.File(root, "desktop-owned")
            val desktopObjects = java.io.File(root, "desktop-objects")
            val captureText =
                run(
                    "python3",
                    java.io.File(repositoryRoot, "tools/sync_generation_exchange.py").absolutePath,
                    "capture-desktop",
                    "--database",
                    desktopDb.absolutePath,
                    "--owned-root",
                    desktopOwned.absolutePath,
                    "--consumer-peer",
                    androidPeer,
                )
            val desktopGeneration =
                Regex("generation_id=(gen_[^ ]+)").find(captureText)!!.groupValues[1]
            run(
                "python3",
                java.io.File(repositoryRoot, "tools/sync_generation_exchange.py").absolutePath,
                "publish",
                "--database",
                desktopDb.absolutePath,
                "--generation-id",
                desktopGeneration,
                "--object-root",
                desktopObjects.absolutePath,
            )
            val desktopPublished = java.io.File(desktopObjects, "generations/$desktopGeneration")
            val desktopAck = SyncGenerationService(desktopDestination).consume(desktopPublished)
            val desktopAckFile =
                java.io.File(root, "desktop-ack.json").apply { writeText(desktopAck) }
            assertTrue(
                run(
                        "python3",
                        java.io
                            .File(repositoryRoot, "tools/sync_generation_exchange.py")
                            .absolutePath,
                        "accept-ack",
                        desktopAckFile.absolutePath,
                        "--database",
                        desktopDb.absolutePath,
                    )
                    .contains("ACK_ACCEPT=acknowledged")
            )
            assertTrue(
                desktopDestination.listSessions().any {
                    it.sessionId == "se_99999999-9999-4999-8999-999999999999"
                }
            )
        } finally {
            source.close()
            destination.close()
            desktopDestination.close()
            context.deleteDatabase(sourceName)
            context.deleteDatabase(destinationName)
            context.deleteDatabase(desktopDestinationName)
            root.deleteRecursively()
        }
    }
}
