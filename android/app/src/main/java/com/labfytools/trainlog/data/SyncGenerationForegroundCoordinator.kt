package com.labfytools.trainlog.data

import java.io.File
import java.io.FileOutputStream
import java.time.Duration
import java.util.UUID
import org.json.JSONArray
import org.json.JSONObject

internal sealed interface ForegroundGenerationResult {
    data class Completed(val runId: String) : ForegroundGenerationResult

    data class Error(val message: String) : ForegroundGenerationResult
}

/** Owns the bounded, user-initiated Android side of a generation conversation. */
internal class SyncGenerationForegroundCoordinator(
    private val repository: TrainlogRepository,
    archiveDirectorySync: ((File) -> Unit)? = null,
) {
    private val service =
        archiveDirectorySync?.let { SyncGenerationService(repository, it) }
            ?: SyncGenerationService(repository)

    private fun requireExactKeys(value: JSONObject, expected: Set<String>, label: String) {
        val actual = value.keys().asSequence().toSet()
        if (actual != expected) throw SyncGenerationException("invalid $label fields")
    }

    private fun reconcileArchiveAcknowledgements(
        raw: ByteArray,
        runId: String,
        androidPeerId: String,
        desktopPeerId: String,
    ) {
        /* WHY: releases before archive-v1 advanced generation status without
         * retaining the consumer's ACK locally. The desktop consumer may
         * return its exact durable ACK so the Android producer can repair
         * that evidence gap without fabricating or weakening proof.
         * CONTRACT: the envelope is strictly correlated and contains at most
         * 32 ACKs; acceptAcknowledgement revalidates every ACK against the
         * immutable local manifest before any archive can become eligible.
         * INVARIANT: a missing, rejected, or ambiguous ACK leaves its
         * generation active and protected. */
        val envelope = JSONObject(raw.toString(Charsets.UTF_8))
        requireExactKeys(
            envelope,
            setOf(
                "format",
                "version",
                "run_id",
                "android_peer_id",
                "desktop_peer_id",
                "acknowledgements",
            ),
            "archive acknowledgement envelope",
        )
        if (
            envelope.getString("format") != "trainlog-sync-archive-acknowledgements" ||
                envelope.getInt("version") != 1 ||
                envelope.getString("run_id") != runId ||
                envelope.getString("android_peer_id") != androidPeerId ||
                envelope.getString("desktop_peer_id") != desktopPeerId
        ) {
            throw SyncGenerationException("archive acknowledgement envelope is not correlated")
        }
        val acknowledgements = envelope.getJSONArray("acknowledgements")
        if (acknowledgements.length() > 32)
            throw SyncGenerationException("too many archive acknowledgements")
        for (index in 0 until acknowledgements.length()) {
            service.acceptAcknowledgement(
                acknowledgements.getJSONObject(index).toString().toByteArray(Charsets.UTF_8)
            )
        }
    }

    private fun publish(path: File, value: String) {
        val temporary = File(path.parentFile, ".${path.name}.tmp-${UUID.randomUUID()}")
        FileOutputStream(temporary).use { stream ->
            stream.write(value.toByteArray())
            stream.flush()
            stream.fd.sync()
        }
        if (!temporary.renameTo(path)) throw SyncGenerationException("cannot publish ${path.name}")
    }

    private fun await(path: File, deadline: Long): File {
        while (!path.isFile) {
            if (System.nanoTime() >= deadline)
                throw SyncGenerationException("timeout waiting for ${path.name}")
            Thread.sleep(50)
        }
        return path
    }

    private fun awaitCorrelated(
        path: File,
        deadline: Long,
        predicate: (JSONObject) -> Boolean,
    ): ByteArray {
        while (true) {
            if (path.isFile && path.length() <= 64 * 1024) {
                try {
                    val raw = path.readBytes()
                    if (predicate(JSONObject(raw.toString(Charsets.UTF_8)))) return raw
                } catch (_: Exception) {
                    // Publication replaces the file atomically, but an old or
                    // concurrently observed object is not evidence of failure.
                }
            }
            if (System.nanoTime() >= deadline)
                throw SyncGenerationException("timeout waiting for correlated ${path.name}")
            Thread.sleep(50)
        }
    }

    fun run(
        directory: File,
        timeout: Duration = Duration.ofMinutes(5),
        afterPublication: (() -> Unit)? = null,
    ): ForegroundGenerationResult =
        try {
            if (!directory.isDirectory)
                throw SyncGenerationException("exchange directory is unavailable")
            val peer = service.peerId()
            publish(
                File(directory, "android-peer-v1.json"),
                JSONObject()
                    .put("format", "trainlog-sync-peer")
                    .put("version", 1)
                    .put("peer_id", peer)
                    .put(
                        "capabilities",
                        JSONArray(
                            listOf(
                                "generation-manifest-v1",
                                "generation-ack-v1",
                                "causal-delete-v1",
                                "mobile-history-v4",
                                "execution-draft-v1",
                                "generation-archive-v1",
                            )
                        ),
                    )
                    .toString(),
            )
            val deadline = System.nanoTime() + timeout.toNanos()
            val requestRaw =
                awaitCorrelated(File(directory, "request-v1.json"), deadline) { candidate ->
                    val runId = candidate.optString("run_id")
                    runId.startsWith("sy_") &&
                        !repository.inSyncGenerationTransaction { db ->
                            db.rawQuery(
                                "SELECT 1 FROM sync_generations WHERE run_id=? LIMIT 1",
                                arrayOf(runId),
                            ).use { it.moveToFirst() }
                        }
                }
            val request = JSONObject(requestRaw.toString(Charsets.UTF_8))
            if (request.getString("android_peer_id") != peer)
                throw SyncGenerationException("request targets another Android peer")
            val archiveAckRaw =
                awaitCorrelated(
                    File(directory, "desktop-archive-acknowledgements-v1.json"),
                    deadline,
                ) {
                    it.optString("run_id") == request.getString("run_id") &&
                        it.optString("android_peer_id") == peer
                }
            reconcileArchiveAcknowledgements(
                archiveAckRaw,
                request.getString("run_id"),
                peer,
                request.getString("desktop_peer_id"),
            )
            val captured =
                service.capture(
                    directory,
                    request.getString("desktop_peer_id"),
                    request.getString("run_id"),
                )
            val published = service.publish(captured, File(directory, "android-objects"))
            publish(
                File(directory, "android-generation-v1.json"),
                JSONObject()
                    .put("format", "trainlog-sync-generation-reference")
                    .put("version", 1)
                    .put("run_id", captured.runId)
                    .put("generation_id", captured.generationId)
                    .put("manifest_sha256", captured.manifestSha256)
                    .put("relative_path", published.relativeTo(directory).path)
                    .toString(),
            )
            afterPublication?.invoke()
            // CONTRACT: durable objects from earlier conversations remain in
            // the exchange directory. Only this run's exact generation ACK
            // can advance its lineage; stale bytes are retained and ignored.
            val desktopAck =
                awaitCorrelated(File(directory, "desktop-consumption-ack-v1.json"), deadline) {
                    it.optString("run_id") == captured.runId &&
                        it.optString("generation_id") == captured.generationId
                }
            service.acceptAcknowledgement(desktopAck)
            val referenceRaw =
                awaitCorrelated(File(directory, "desktop-generation-v1.json"), deadline) {
                    it.optString("run_id") == captured.runId
                }
            val reference = JSONObject(referenceRaw.toString(Charsets.UTF_8))
            val acknowledgement =
                service.consume(File(directory, reference.getString("relative_path")))
            publish(File(directory, "android-consumption-ack-v1.json"), acknowledgement)
            ForegroundGenerationResult.Completed(captured.runId)
        } catch (error: Exception) {
            if (error.message == "generation retention capacity exhausted") {
                try {
                    val request = JSONObject(File(directory, "request-v1.json").readText())
                    publish(
                        File(directory, "android-generation-error-v1.json"),
                        JSONObject()
                            .put("format", "trainlog-sync-generation-error")
                            .put("version", 1)
                            .put("run_id", request.getString("run_id"))
                            .put("android_peer_id", service.peerId())
                            .put("code", "peer_capacity_exhausted")
                            .put("action", "Archive acknowledged generations, then retry explicitly.")
                            .toString(),
                    )
                } catch (_: Exception) {
                    // Preserve the original local failure when error reporting
                    // cannot itself be durably published.
                }
            }
            ForegroundGenerationResult.Error(error.message ?: "generation synchronization failed")
        }
}
