package com.labfytools.trainlog.data

import android.content.Context
import android.database.sqlite.SQLiteDatabase
import androidx.test.core.app.ApplicationProvider
import com.labfytools.trainlog.model.HeartRateContextKind
import com.labfytools.trainlog.model.HeartRateRrInterval
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionDraft
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionSetDraft
import com.labfytools.trainlog.model.SleepDiaryDraft
import com.labfytools.trainlog.model.SleepMedication
import com.labfytools.trainlog.model.TrackingMode
import java.io.File
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
    fun sleepOnlyNightCrossesDesktopAckReplayAndSecondRevisionWithoutSession() {
        val sourceName = "sleep-only-generation-${UUID.randomUUID()}.db"
        val root = Files.createTempDirectory("trainlog-sleep-only-generation-").toFile()
        val desktopDb = java.io.File(root, "desktop.sqlite")
        val repositoryRoot =
            generateSequence(java.io.File(checkNotNull(System.getProperty("user.dir")))) {
                    it.parentFile
                }
                .first { java.io.File(it, "tools/sync_generation_exchange.py").isFile }
        val generationTool =
            java.io.File(repositoryRoot, "tools/sync_generation_exchange.py").absolutePath
        var source = TrainlogRepository(context, sourceName)

        fun run(vararg command: String): String {
            val process =
                ProcessBuilder(*command)
                    .directory(repositoryRoot)
                    .redirectErrorStream(true)
                    .start()
            val output = process.inputStream.bufferedReader().readText()
            assertEquals(
                "command failed: ${command.joinToString(" ")}\n$output",
                0,
                process.waitFor(),
            )
            return output
        }

        try {
            run(
                java.io.File(repositoryRoot, "build/tui/sync-generation-fixture").absolutePath,
                desktopDb.absolutePath,
            )
            val desktopPeer =
                Regex("PEER_ID=(peer_[^\\s]+)")
                    .find(
                        run(
                            "python3",
                            generationTool,
                            "peer-id",
                            "--database",
                            desktopDb.absolutePath,
                        )
                    )!!
                    .groupValues[1]
            val medicationCreated =
                source.saveSleepMedication(
                    SleepMedication(
                        medicationId = "",
                        revisionId = "",
                        createdAt = "2026-09-23T20:00:00+02:00",
                        updatedAt = "2026-09-23T20:00:00+02:00",
                        name = "Synthetic sleep-only medication",
                        defaultDoseValue = 5.0,
                        defaultDoseUnit = "mg",
                        form = "tablet",
                        note = "",
                        active = true,
                    )
                ) as TrainlogRepository.SaveSleepDiaryResult.Saved
            val preBed =
                source.quickSleepMedication(
                    medicationCreated.entryId,
                    "2026-09-23T22:20:00+02:00",
                    quantity = 2,
                ) as TrainlogRepository.SleepQuickActionResult.Applied
            val bed =
                source.quickSleepBedTime("2026-09-23T22:30:00+02:00")
                    as TrainlogRepository.SleepQuickActionResult.Applied
            assertEquals(preBed.receipt.entryId, bed.receipt.entryId)
            val firstMeasurement =
                ParsedHeartRateMeasurement(
                    bpm = 61,
                    sensorContactDetected = true,
                    energyExpended = 7,
                    rrIntervals1024 = listOf(1000),
                )
            assertTrue(
                source.recordLiveHeartRateForActiveSleep(
                    "Synthetic HR",
                    "2026-09-23T22:30:01+02:00",
                    firstMeasurement,
                ) is TrainlogRepository.HeartRateMutationResult.Applied
            )
            assertTrue(
                source.quickSleepWake("2026-09-24T02:17:00+02:00")
                    is TrainlogRepository.SleepQuickActionResult.Applied
            )
            assertTrue(
                source.recordLiveHeartRateForActiveSleep(
                    "Synthetic HR",
                    "2026-09-24T02:17:01+02:00",
                    firstMeasurement.copy(bpm = 65, rrIntervals1024 = listOf(980)),
                ) is TrainlogRepository.HeartRateMutationResult.Applied
            )
            assertTrue(
                source.quickSleepFinalGetUp("2026-09-24T06:31:00+02:00")
                    is TrainlogRepository.SleepQuickActionResult.Applied
            )
            assertTrue(source.listSessions().isEmpty())

            val night = source.listSleepDiary().single()
            val capture =
                JSONObject(source.buildHeartRateV1Json())
                    .getJSONArray("captures")
                    .getJSONObject(0)
            assertEquals(night.entryId, capture.getString("context_id"))
            assertEquals(2, capture.getJSONArray("samples").length())
            assertEquals(2, night.intakes.single().quantity)

            val service = SyncGenerationService(source)
            val captured = service.capture(root, desktopPeer)
            val diaryArtifact =
                JSONObject(java.io.File(captured.stagingDirectory, "sleep-diary-v2.json").readText())
            val heartArtifact =
                JSONObject(java.io.File(captured.stagingDirectory, "heart-rate-v1.json").readText())
            val historyArtifact =
                JSONObject(java.io.File(captured.stagingDirectory, "history-v4.json").readText())
            assertEquals(0, historyArtifact.getJSONArray("sessions").length())
            val publishedNight = diaryArtifact.getJSONArray("entries").getJSONObject(0)
            val publishedCapture = heartArtifact.getJSONArray("captures").getJSONObject(0)
            assertEquals(night.entryId, publishedNight.getString("entry_id"))
            assertEquals(
                2,
                publishedNight.getJSONArray("intakes").getJSONObject(0).getInt("quantity"),
            )
            assertEquals(capture.getString("capture_id"), publishedCapture.getString("capture_id"))

            val published = service.publish(captured, java.io.File(root, "android-objects"))
            val ack = java.io.File(root, "sleep-only-ack.json")
            run(
                "python3",
                generationTool,
                "consume-desktop",
                published.absolutePath,
                "--database",
                desktopDb.absolutePath,
                "--ack-output",
                ack.absolutePath,
            )
            val ackDocument = JSONObject(ack.readText())
            assertEquals(
                "desktop ACK diagnostic: ${ackDocument.getString("diagnostic")}",
                "consumed",
                ackDocument.getString("result"),
            )
            assertEquals("acknowledged", service.acceptAcknowledgement(ack.readBytes()))
            assertEquals("unchanged", service.acceptAcknowledgement(ack.readBytes()))
            run(
                "python3",
                generationTool,
                "consume-desktop",
                published.absolutePath,
                "--database",
                desktopDb.absolutePath,
                "--ack-output",
                java.io.File(root, "sleep-only-replay-ack.json").absolutePath,
            )
            assertEquals(
                // The desktop fixture owns one reserved calibration session;
                // the empty Android history must not add a workout.
                "1|1|2|2|2|1",
                run(
                        "sqlite3",
                        desktopDb.absolutePath,
                        "SELECT (SELECT count(*) FROM sleep_diary_entries WHERE entry_id='${night.entryId}') || '|' || " +
                            "(SELECT count(*) FROM sleep_medication_intakes WHERE revision_id='${night.revisionId}') || '|' || " +
                            "(SELECT quantity FROM sleep_medication_intakes WHERE revision_id='${night.revisionId}') || '|' || " +
                            "(SELECT count(*) FROM heart_rate_samples WHERE capture_id='${capture.getString("capture_id")}') || '|' || " +
                            "(SELECT count(*) FROM heart_rate_rr_intervals WHERE capture_id='${capture.getString("capture_id")}') || '|' || " +
                            "(SELECT count(*) FROM sessions);",
                    )
                    .trim(),
            )

            val revised =
                source.saveSleepDiary(
                    SleepDiaryDraft(
                        entryId = night.entryId,
                        expectedRevision = night.revisionId,
                        nightStartDate = night.nightStartDate,
                        nightEndDate = night.nightEndDate,
                        createdAt = night.createdAt,
                        updatedAt = "2026-09-24T07:00:00+02:00",
                        sleepQuality = night.sleepQuality,
                        wakeQuality = night.wakeQuality,
                        dayForm = night.dayForm,
                        treatmentAndNotes = "Sleep-only revision two",
                        events = night.events,
                        intakes = night.intakes,
                    )
                ) as TrainlogRepository.SaveSleepDiaryResult.Saved
            assertTrue(
                source.validateSleepDiary(
                    revised.entryId,
                    revised.revisionId,
                    "2026-09-24T07:01:00+02:00",
                ) is TrainlogRepository.SaveSleepDiaryResult.Saved
            )
            val second = service.capture(root, desktopPeer)
            val secondPublished =
                service.publish(second, java.io.File(root, "android-objects-second"))
            val secondAck = java.io.File(root, "sleep-only-second-ack.json")
            run(
                "python3",
                generationTool,
                "consume-desktop",
                secondPublished.absolutePath,
                "--database",
                desktopDb.absolutePath,
                "--ack-output",
                secondAck.absolutePath,
            )
            assertEquals("acknowledged", service.acceptAcknowledgement(secondAck.readBytes()))
            assertEquals(
                revised.revisionId,
                run(
                        "sqlite3",
                        desktopDb.absolutePath,
                        "SELECT current_revision_id FROM sleep_diary_entries WHERE entry_id='${night.entryId}';",
                    )
                    .trim(),
            )
            assertTrue(source.listSessions().isEmpty())
        } finally {
            source.close()
            context.deleteDatabase(sourceName)
            root.deleteRecursively()
        }
    }

    @Test
    fun desktopProgramDeletionGenerationIsAckedAndCannotResurrect() {
        val repositoryRoot =
            generateSequence(java.io.File(checkNotNull(System.getProperty("user.dir")))) {
                    it.parentFile
                }
                .first { java.io.File(it, "tools/sync_generation_exchange.py").isFile }
        val fixture = java.io.File(repositoryRoot, "build/tui/sync-generation-fixture")
        val generationTool =
            java.io.File(repositoryRoot, "tools/sync_generation_exchange.py").absolutePath
        val root = Files.createTempDirectory("trainlog-program-deletion-generation-").toFile()
        val desktopDb = java.io.File(root, "desktop.sqlite")
        val desktopOwned = java.io.File(root, "desktop-owned")
        val desktopObjects = java.io.File(root, "desktop-objects")
        val androidName = "program-deletion-generation-${UUID.randomUUID()}.db"
        val programId = "pg_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"
        val deletionId = "program-delete-generation-regression"
        var android = TrainlogRepository(context, androidName)

        fun run(vararg command: String): String {
            val process =
                ProcessBuilder(*command)
                    .directory(repositoryRoot)
                    .redirectErrorStream(true)
                    .start()
            val output = process.inputStream.bufferedReader().readText()
            assertEquals(
                "command failed: ${command.joinToString(" ")}\n$output",
                0,
                process.waitFor(),
            )
            return output
        }

        fun captureAndPublish(service: SyncGenerationService): Pair<String, java.io.File> {
            val captured =
                run(
                    "python3",
                    generationTool,
                    "capture-desktop",
                    "--database",
                    desktopDb.absolutePath,
                    "--owned-root",
                    desktopOwned.absolutePath,
                    "--consumer-peer",
                    service.peerId(),
                )
            val generationId =
                checkNotNull(Regex("generation_id=(gen_[^ ]+)").find(captured)).groupValues[1]
            run(
                "python3",
                generationTool,
                "publish",
                "--database",
                desktopDb.absolutePath,
                "--generation-id",
                generationId,
                "--object-root",
                desktopObjects.absolutePath,
            )
            return generationId to java.io.File(desktopObjects, "generations/$generationId")
        }

        fun acceptAndroidAck(generationId: String, acknowledgement: String): String {
            val acknowledgementFile =
                java.io.File(root, "$generationId-ack.json").apply {
                    writeText(acknowledgement)
                }
            return run(
                "python3",
                generationTool,
                "accept-ack",
                acknowledgementFile.absolutePath,
                "--database",
                desktopDb.absolutePath,
            )
        }

        fun androidDeletionCount(): Int =
            SQLiteDatabase.openDatabase(
                    context.getDatabasePath(androidName).absolutePath,
                    null,
                    SQLiteDatabase.OPEN_READONLY,
                )
                .use { db ->
                    db.rawQuery(
                            "SELECT COUNT(*) FROM synced_program_deletions WHERE program_id=?",
                            arrayOf(programId),
                        )
                        .use {
                            assertTrue(it.moveToFirst())
                            it.getInt(0)
                        }
                }

        try {
            val created = run(fixture.absolutePath, desktopDb.absolutePath, "program-create")
            val createdProgram = JSONObject(created.substringAfter("PROGRAM_CREATE=").trim())
            val initialRevision = createdProgram.getString("revision_id")
            assertEquals(programId, createdProgram.getString("program_id"))

            var service = SyncGenerationService(android)
            val (liveGenerationId, livePublished) = captureAndPublish(service)
            val desktopPeer =
                JSONObject(java.io.File(livePublished, "manifest.json").readText())
                    .getJSONObject("producer")
                    .getString("peer_id")
            val livePrograms =
                JSONObject(java.io.File(livePublished, "programs-v1.json").readText())
            assertEquals(
                programId,
                livePrograms
                    .getJSONArray("programs")
                    .getJSONObject(0)
                    .getString("program_id"),
            )
            assertEquals(0, livePrograms.getJSONArray("deletions").length())
            val liveAck = service.consume(livePublished)
            assertEquals(programId, android.listSyncedPrograms().single().programId)
            assertTrue(
                acceptAndroidAck(liveGenerationId, liveAck).contains("ACK_ACCEPT=acknowledged")
            )

            val programSessionId =
                "pgs_bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb"
            assertEquals(
                StartProgramSessionResult.Started,
                android.startSyncedProgramSession(programId, programSessionId),
            )
            val active =
                (android.loadActiveSessionDraft() as ActiveDraftLoadResult.Loaded).draft
            val performed = active.copy(
                exercises = active.exercises.map { occurrence ->
                    occurrence.copy(
                        sets = listOf(SessionSetDraft(reps = 8, weightKg = 40.0)),
                    )
                },
            )
            assertEquals(
                ActiveDraftMutationResult.Saved,
                android.saveActiveSessionDraft(performed),
            )
            assertTrue(android.finalizeActiveSessionDraft() is FinalizeActiveDraftResult.Saved)
            val completedSessionId = android.listSessions().single().sessionId
            val androidGeneration = service.capture(root, desktopPeer)
            val androidPublished = service.publish(
                androidGeneration,
                java.io.File(root, "android-objects"),
            )
            val desktopAck = java.io.File(root, "android-program-execution-ack.json")
            run(
                "python3",
                generationTool,
                "consume-desktop",
                androidPublished.absolutePath,
                "--database",
                desktopDb.absolutePath,
                "--ack-output",
                desktopAck.absolutePath,
            )
            assertEquals(
                "acknowledged",
                service.acceptAcknowledgement(desktopAck.readBytes()),
            )
            assertEquals(
                "$programSessionId|$completedSessionId|completed",
                run(
                    "sqlite3",
                    desktopDb.absolutePath,
                    "SELECT program_session_id || '|' || session_id || '|' || state " +
                        "FROM program_session_executions;",
                ).trim(),
            )
            val completedDetail = run(
                fixture.absolutePath,
                desktopDb.absolutePath,
                "program-detail",
            )
            assertTrue(completedDetail.contains("\"execution_state\":\"completed\""))

            val deleted =
                run(
                    fixture.absolutePath,
                    desktopDb.absolutePath,
                    "program-delete",
                    initialRevision,
                    deletionId,
                )
            val replayedDelete =
                run(
                    fixture.absolutePath,
                    desktopDb.absolutePath,
                    "program-delete",
                    initialRevision,
                    deletionId,
                )
            assertEquals(deleted, replayedDelete)
            val deletionRevision =
                JSONObject(deleted.substringAfter("PROGRAM_DELETE=").trim())
                    .getString("revision_id")

            val (deletionGenerationId, deletionPublished) = captureAndPublish(service)
            val deletedPrograms =
                JSONObject(java.io.File(deletionPublished, "programs-v1.json").readText())
            assertEquals(0, deletedPrograms.getJSONArray("programs").length())
            assertEquals(1, deletedPrograms.getJSONArray("deletions").length())
            val exportedDeletion = deletedPrograms.getJSONArray("deletions").getJSONObject(0)
            assertEquals(deletionId, exportedDeletion.getString("deletion_id"))
            assertEquals(initialRevision, exportedDeletion.getString("predecessor_revision_id"))
            assertEquals(deletionRevision, exportedDeletion.getString("revision_id"))
            val deletionAck = service.consume(deletionPublished)
            assertTrue(android.listSyncedPrograms().isEmpty())
            assertTrue(
                acceptAndroidAck(deletionGenerationId, deletionAck)
                    .contains("ACK_ACCEPT=acknowledged")
            )
            val acknowledgedDeletion =
                run(
                        "sqlite3",
                        desktopDb.absolutePath,
                        "SELECT generation_id || '|' || (acknowledged_at IS NOT NULL) " +
                            "FROM program_deletions WHERE request_id='$deletionId';",
                    )
                    .trim()
            assertEquals("$deletionGenerationId|1", acknowledgedDeletion)

            android.close()
            android = TrainlogRepository(context, androidName)
            service = SyncGenerationService(android)
            val reopenedDesktopPeer =
                checkNotNull(
                        Regex("PEER_ID=(peer_[^\\s]+)")
                            .find(
                                run(
                                    "python3",
                                    generationTool,
                                    "peer-id",
                                    "--database",
                                    desktopDb.absolutePath,
                                )
                            )
                    )
                    .groupValues[1]
            assertEquals(desktopPeer, reopenedDesktopPeer)
            assertEquals(liveAck, service.consume(livePublished))
            assertTrue(android.listSyncedPrograms().isEmpty())
            assertEquals(1, androidDeletionCount())

            val (cleanGenerationId, cleanPublished) = captureAndPublish(service)
            val cleanPrograms =
                JSONObject(java.io.File(cleanPublished, "programs-v1.json").readText())
            assertEquals(0, cleanPrograms.getJSONArray("programs").length())
            assertEquals(0, cleanPrograms.getJSONArray("deletions").length())
            val cleanAck = service.consume(cleanPublished)
            assertTrue(android.listSyncedPrograms().isEmpty())
            assertEquals(1, androidDeletionCount())
            assertTrue(
                acceptAndroidAck(cleanGenerationId, cleanAck).contains("ACK_ACCEPT=acknowledged")
            )
            assertEquals(
                "1",
                run(
                        "sqlite3",
                        desktopDb.absolutePath,
                        "SELECT COUNT(*) FROM program_deletions WHERE program_id='$programId';",
                    )
                    .trim(),
            )
        } finally {
            android.close()
            context.deleteDatabase(androidName)
            root.deleteRecursively()
        }
    }

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
            val retryRun = "sy_${UUID.randomUUID()}"
            val crossRunResume =
                SyncGenerationCoordinator(source)
                    .resumableGeneration(retryRun, destinationService.peerId())
            assertEquals(outbound.generationId, crossRunResume?.generationId)
            assertEquals(retryRun, crossRunResume?.runId)
            assertEquals(outbound.manifestSha256, crossRunResume?.manifestSha256)
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
    fun crossRunResumeAcceptsExactImmutableAcknowledgementFromOriginalRun() {
        val generationId = "gen_${UUID.randomUUID()}"
        val originalRunId = "sy_${UUID.randomUUID()}"
        val retryRunId = "sy_${UUID.randomUUID()}"
        val digest = "a".repeat(64)
        val captured =
            CapturedSyncGeneration(generationId, retryRunId, digest, java.io.File("unused"))
        val acknowledgement =
            JSONObject()
                .put("run_id", originalRunId)
                .put("generation_id", generationId)
                .put("manifest_sha256", digest)

        assertTrue(acknowledgementMatchesCapturedGeneration(acknowledgement, captured))
        acknowledgement.put("manifest_sha256", "b".repeat(64))
        assertTrue(!acknowledgementMatchesCapturedGeneration(acknowledgement, captured))
    }

    @Test
    fun ledgerBackedSiblingMakesRetainedWaitingBranchesCapacityTerminal() {
        val name = "generation-superseded-capacity-${UUID.randomUUID()}.db"
        val root = Files.createTempDirectory("trainlog-generation-superseded-").toFile()
        val repository = TrainlogRepository(context, name)
        try {
            val service = SyncGenerationService(repository)
            val peer = "peer_11111111-1111-4111-8111-111111111111"
            val branches = (0 until 4).map { service.capture(root, peer) }
            val acknowledged = branches.last()
            repository.inSyncGenerationTransaction { db ->
                db.execSQL(
                    "UPDATE sync_generations SET parent_generation_id=? WHERE consumer_peer_id=?",
                    arrayOf("gen_22222222-2222-4222-8222-222222222222", peer),
                )
                db.execSQL(
                    "UPDATE sync_generations SET status='acknowledged',acknowledged_at=? " +
                        "WHERE generation_id=?",
                    arrayOf("2026-09-24T07:00:00+02:00", acknowledged.generationId),
                )
                db.execSQL(
                    "INSERT INTO sync_acknowledgements VALUES(?,?,?,?,?,?,?,?,?,?,?)",
                    arrayOf(
                        "ack_${UUID.randomUUID()}",
                        acknowledged.generationId,
                        acknowledged.runId,
                        service.peerId(),
                        peer,
                        acknowledged.manifestSha256,
                        "consumed",
                        "sqlite-commit-full",
                        "2026-09-24T07:00:00+02:00",
                        "",
                        "a".repeat(64),
                    ),
                )
            }

            // The three sibling branches remain durable, but the ledger-backed
            // acknowledged sibling proves that none can be the causal tip.
            assertEquals(3, branches.dropLast(1).size)
            assertTrue(service.capture(root, peer).generationId.startsWith("gen_"))
        } finally {
            repository.close()
            context.deleteDatabase(name)
            root.deleteRecursively()
        }
    }

    @Test
    fun coordinatorOnlyTreatsCorrelatedFreshRequestAsActionable() {
        val databaseName = "generation-actionable-${UUID.randomUUID()}.db"
        val root = Files.createTempDirectory("trainlog-generation-actionable-").toFile()
        val repository = TrainlogRepository(context, databaseName)
        try {
            val coordinator = SyncGenerationCoordinator(repository)
            assertEquals(
                BackgroundRequestActionability.NOT_ACTIONABLE,
                coordinator.classifyBackgroundRequest(root, null),
            )
            File(root, "request-v1.json").writeText("not-json")
            assertEquals(
                BackgroundRequestActionability.NOT_ACTIONABLE,
                coordinator.classifyBackgroundRequest(root, null),
            )

            val peer = SyncGenerationService(repository).peerId()
            val request =
                JSONObject()
                    .put("format", "trainlog-sync-generation-request")
                    .put("version", 1)
                    .put("run_id", "sy_${UUID.randomUUID()}")
                    .put("android_peer_id", "peer_${UUID.randomUUID()}")
                    .put("desktop_peer_id", "peer_${UUID.randomUUID()}")
            File(root, "request-v1.json").writeText(request.toString())
            assertEquals(
                BackgroundRequestActionability.NOT_ACTIONABLE,
                coordinator.classifyBackgroundRequest(root, null),
            )

            request.put("android_peer_id", peer)
            File(root, "request-v1.json").writeText(request.toString())
            assertEquals(
                BackgroundRequestActionability.NEW_REQUEST,
                coordinator.classifyBackgroundRequest(root, null),
            )
        } finally {
            repository.close()
            context.deleteDatabase(databaseName)
            root.deleteRecursively()
        }
    }

    @Test
    fun repeatedPeerPublicationLeavesIdenticalCanonicalFileUntouched() {
        val databaseName = "generation-peer-publication-${UUID.randomUUID()}.db"
        val root = Files.createTempDirectory("trainlog-generation-peer-publication-").toFile()
        val repository = TrainlogRepository(context, databaseName)
        try {
            val coordinator = SyncGenerationCoordinator(repository)
            val peer = coordinator.publishPeer(root)
            val descriptor = File(root, "android-peer-v1.json")
            val retainedTimestamp = 1_234_000L
            assertTrue(descriptor.setLastModified(retainedTimestamp))

            assertEquals(peer, coordinator.publishPeer(root))
            assertEquals(retainedTimestamp, descriptor.lastModified())
            assertEquals(peer, JSONObject(descriptor.readText()).getString("peer_id"))
        } finally {
            repository.close()
            context.deleteDatabase(databaseName)
            root.deleteRecursively()
        }
    }

    @Test
    fun coordinatorReplaysOnlyStrictlyCorrelatedDurableAcknowledgement() {
        val databaseName = "generation-ack-replay-${UUID.randomUUID()}.db"
        val root = Files.createTempDirectory("trainlog-generation-ack-replay-").toFile()
        val repository = TrainlogRepository(context, databaseName)
        try {
            val coordinator = SyncGenerationCoordinator(repository)
            val androidPeer = SyncGenerationService(repository).peerId()
            val desktopPeer = "peer_${UUID.randomUUID()}"
            val runId = "sy_${UUID.randomUUID()}"
            val generationId = "gen_${UUID.randomUUID()}"
            val digest = "a".repeat(64)
            File(root, "request-v1.json").writeText(
                JSONObject()
                    .put("format", "trainlog-sync-generation-request")
                    .put("version", 1)
                    .put("run_id", runId)
                    .put("android_peer_id", androidPeer)
                    .put("desktop_peer_id", desktopPeer)
                    .toString()
            )
            File(root, "desktop-generation-v1.json").writeText(
                JSONObject()
                    .put("format", "trainlog-sync-generation-reference")
                    .put("version", 1)
                    .put("run_id", runId)
                    .put("generation_id", generationId)
                    .put("manifest_sha256", digest)
                    .toString()
            )
            val acknowledgement =
                JSONObject()
                    .put("format", "trainlog-sync-ack")
                    .put("version", 1)
                    .put("run_id", runId)
                    .put("generation_id", generationId)
                    .put("manifest_sha256", digest)
                    .put("producer_peer_id", desktopPeer)
                    .put("consumer_peer_id", androidPeer)
                    .put("result", "consumed")
            File(root, "android-consumption-ack-v1.json").writeText(acknowledgement.toString())
            assertEquals(true, coordinator.hasReplayableAcknowledgement(root))

            acknowledgement.put("generation_id", "gen_${UUID.randomUUID()}")
            File(root, "android-consumption-ack-v1.json").writeText(acknowledgement.toString())
            assertEquals(false, coordinator.hasReplayableAcknowledgement(root))
        } finally {
            repository.close()
            context.deleteDatabase(databaseName)
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
                        assertEquals(32, it.getInt(0))
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
            val sleepEntry =
                source.saveSleepDiary(
                    SleepDiaryDraft(
                        nightStartDate = "2026-09-22",
                        nightEndDate = "2026-09-23",
                        createdAt = "2026-09-22T22:00:00+02:00",
                        updatedAt = "2026-09-22T22:00:00+02:00",
                        sleepQuality = null,
                        wakeQuality = null,
                        dayForm = null,
                        treatmentAndNotes = "",
                        events = emptyList(),
                    ),
                ) as TrainlogRepository.SaveSleepDiaryResult.Saved
            assertTrue(
                source.validateSleepDiary(
                    sleepEntry.entryId,
                    sleepEntry.revisionId,
                    "2026-09-23T07:00:00+02:00",
                ) is TrainlogRepository.SaveSleepDiaryResult.Saved,
            )
            val heartCapture =
                source.startHeartRateCapture(
                    HeartRateContextKind.SLEEP,
                    sleepEntry.entryId,
                    "2026-09-22T22:30:00+02:00",
                    "Synthetic HR",
                ) as TrainlogRepository.StartHeartRateCaptureResult.Started
            assertTrue(
                source.appendHeartRateSample(
                    heartCapture.captureId,
                    "2026-09-22T22:30:01+02:00",
                    61,
                    true,
                    7,
                    listOf(HeartRateRrInterval(0, 1024)),
                ) is TrainlogRepository.HeartRateMutationResult.Applied,
            )
            assertTrue(
                source.stopHeartRateCapture(
                    heartCapture.captureId,
                    "2026-09-22T22:31:00+02:00",
                ) is TrainlogRepository.HeartRateMutationResult.Applied,
            )

            val androidToDesktop = sourceService.capture(root, desktopPeer)
            assertEquals(
                1,
                JSONObject(
                        java.io.File(androidToDesktop.stagingDirectory, "heart-rate-v1.json")
                            .readText(),
                    )
                    .getJSONArray("captures")
                    .length(),
            )
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
            val androidAck = JSONObject(androidAckFile.readText())
            assertEquals(
                "desktop ACK diagnostic: ${androidAck.getString("diagnostic")}",
                "consumed",
                androidAck.getString("result"),
            )
            assertEquals(
                "acknowledged",
                sourceService.acceptAcknowledgement(androidAckFile.readBytes()),
            )
            assertEquals(
                "unchanged",
                sourceService.acceptAcknowledgement(androidAckFile.readBytes()),
            )
            assertEquals(
                0,
                JSONObject(source.buildHeartRateV1Json()).getJSONArray("captures").length(),
            )
            assertEquals(
                "1|1|1",
                run(
                        "sqlite3",
                        desktopConsumerDb.absolutePath,
                        "SELECT (SELECT count(*) FROM heart_rate_captures) || '|' || " +
                            "(SELECT count(*) FROM heart_rate_samples) || '|' || " +
                            "(SELECT count(*) FROM heart_rate_rr_intervals);",
                    )
                    .trim(),
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

            /* A second Sleep-only mutation must advance the same diary entry
             * after the first correlated ACK. No new workout is created, and
             * replaying either generation remains strictly idempotent. */
            val revisedSleep =
                source.saveSleepDiary(
                    SleepDiaryDraft(
                        entryId = sleepEntry.entryId,
                        expectedRevision = sleepEntry.revisionId,
                        nightStartDate = "2026-09-22",
                        nightEndDate = "2026-09-23",
                        createdAt = "2026-09-22T22:00:00+02:00",
                        updatedAt = "2026-09-23T08:00:00+02:00",
                        sleepQuality = null,
                        wakeQuality = null,
                        dayForm = null,
                        treatmentAndNotes = "second sleep-only revision",
                        events = emptyList(),
                    ),
                ) as TrainlogRepository.SaveSleepDiaryResult.Saved
            assertTrue(
                source.validateSleepDiary(
                    revisedSleep.entryId,
                    revisedSleep.revisionId,
                    "2026-09-23T08:01:00+02:00",
                ) is TrainlogRepository.SaveSleepDiaryResult.Saved,
            )
            val secondSleepGeneration = sourceService.capture(root, desktopPeer)
            val secondSleepPublished =
                sourceService.publish(
                    secondSleepGeneration,
                    java.io.File(root, "android-desktop-objects"),
                )
            val secondAckFile = java.io.File(root, "android-desktop-second-ack.json")
            run(
                "python3",
                java.io.File(repositoryRoot, "tools/sync_generation_exchange.py").absolutePath,
                "consume-desktop",
                secondSleepPublished.absolutePath,
                "--database",
                desktopConsumerDb.absolutePath,
                "--ack-output",
                secondAckFile.absolutePath,
            )
            assertEquals(
                "acknowledged",
                sourceService.acceptAcknowledgement(secondAckFile.readBytes()),
            )
            assertEquals(
                "unchanged",
                sourceService.acceptAcknowledgement(secondAckFile.readBytes()),
            )
            assertEquals(
                "1|${revisedSleep.revisionId}|1",
                run(
                        "sqlite3",
                        desktopConsumerDb.absolutePath,
                        "SELECT count(*) || '|' || current_revision_id || '|' || " +
                            "(SELECT count(*) FROM sessions WHERE session_id='$androidSessionId') " +
                            "FROM sleep_diary_entries WHERE entry_id='${sleepEntry.entryId}';",
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
