package com.labfytools.trainlog.data

import android.util.Log
import java.io.File
import java.io.FileOutputStream
import java.nio.file.AtomicMoveNotSupportedException
import java.nio.file.Files
import java.nio.file.StandardCopyOption
import java.time.Duration
import java.util.UUID
import org.json.JSONArray
import org.json.JSONObject

internal sealed interface ForegroundGenerationResult {
    data class Completed(val runId: String) : ForegroundGenerationResult

    data class Error(val message: String) : ForegroundGenerationResult
}

internal fun interface SyncGenerationTrace {
    fun record(runId: String?, phase: String, artifact: String?, generationId: String?, result: String?)
}

internal object AndroidSyncGenerationTrace : SyncGenerationTrace {
    override fun record(
        runId: String?, phase: String, artifact: String?, generationId: String?, result: String?
    ) {
        Log.i(
            "TrainlogSyncGeneration",
            listOfNotNull(
                "run_id=${runId ?: "pending"}",
                "phase=$phase",
                artifact?.let { "artifact=$it" },
                generationId?.let { "generation_id=$it" },
                result?.let { "result=$it" },
            ).joinToString(" "),
        )
    }
}

/** Owns the bounded, user-initiated Android side of a generation conversation. */
internal class SyncGenerationCoordinator(
    private val repository: TrainlogRepository,
    private val visibility: MtpPublicationVisibility = ImmediateMtpPublicationVisibility,
    private val trace: SyncGenerationTrace = AndroidSyncGenerationTrace,
) {
    private val service = SyncGenerationService(repository)

    private fun phase(
        runId: String?,
        phase: String,
        artifact: String? = null,
        generationId: String? = null,
        result: String? = null,
    ) = trace.record(runId, phase, artifact, generationId, result)

    companion object {
        /* WHY: UI and background triggers share one repository protocol and
         * must never create concurrent runs. CONTRACT: this process-wide lock
         * guards only one bounded conversation; SQLite remains the durable
         * replay authority after process restart. */
        private val conversationLock = java.util.concurrent.locks.ReentrantLock()
    }

    internal fun resumableGeneration(runId: String, consumerPeerId: String? = null): CapturedSyncGeneration? =
        repository.inSyncGenerationTransaction { db ->
            db.rawQuery(
                "SELECT generation_id,manifest_sha256,staging_path FROM sync_generations " +
                    "WHERE run_id=? AND (status IN('captured','published','waiting_acknowledgement') " +
                    "OR (status='acknowledged' AND NOT EXISTS(" +
                    "SELECT 1 FROM sync_consumed_generations c WHERE c.run_id=sync_generations.run_id " +
                    "AND c.result IN('consumed','rejected')))) " +
                    (if (consumerPeerId == null) "" else "AND consumer_peer_id=? ") +
                    "LIMIT 2",
                if (consumerPeerId == null) arrayOf(runId) else arrayOf(runId, consumerPeerId),
            ).use { cursor ->
                if (!cursor.moveToFirst()) null
                else {
                    val captured =
                        CapturedSyncGeneration(
                            cursor.getString(0),
                            runId,
                            cursor.getString(1),
                            File(cursor.getString(2)),
                        )
                    if (cursor.moveToNext())
                        throw SyncGenerationException("ambiguous resumable generation")
                    captured
                }
            }
        }

    internal fun hasActionableRequest(directory: File): Boolean {
        val path = File(directory, "request-v1.json")
        if (!path.isFile || path.length() !in 1..64 * 1024) return false
        val request = runCatching { JSONObject(path.readText()) }.getOrNull() ?: return false
        val runId = request.optString("run_id")
        val desktopPeerId = request.optString("desktop_peer_id")
        if (
            request.optString("format") != "trainlog-sync-generation-request" ||
                request.optInt("version", -1) != 1 ||
                !runId.matches(Regex("^sy_[0-9a-f-]{36}$")) ||
                !desktopPeerId.matches(Regex("^peer_[0-9a-f-]{36}$")) ||
                request.optString("android_peer_id") != service.peerId()
        ) return false
        if (resumableGeneration(runId, desktopPeerId) != null) return true
        return !repository.inSyncGenerationTransaction { db ->
            db.rawQuery(
                "SELECT 1 FROM sync_generations WHERE run_id=? LIMIT 1",
                arrayOf(runId),
            ).use { it.moveToFirst() }
        }
    }

    internal fun hasReplayableAcknowledgement(directory: File): Boolean {
        val request =
            runCatching { JSONObject(File(directory, "request-v1.json").readText()) }.getOrNull()
                ?: return false
        val reference =
            runCatching { JSONObject(File(directory, "desktop-generation-v1.json").readText()) }
                .getOrNull() ?: return false
        val acknowledgement =
            runCatching { JSONObject(File(directory, "android-consumption-ack-v1.json").readText()) }
                .getOrNull() ?: return false
        /* WHY: a network failure may occur after Core commits consumption but
         * before Drive receives its ACK. CONTRACT: only the exact durable ACK
         * correlated to the current request and desktop reference is replayed.
         * INVARIANT: replay transports existing evidence and never recaptures
         * a generation or repeats a domain mutation. */
        return request.optString("format") == "trainlog-sync-generation-request" &&
            request.optInt("version", -1) == 1 &&
            request.optString("android_peer_id") == service.peerId() &&
            reference.optString("format") == "trainlog-sync-generation-reference" &&
            reference.optInt("version", -1) == 1 &&
            acknowledgement.optString("format") == "trainlog-sync-ack" &&
            acknowledgement.optInt("version", -1) == 1 &&
            acknowledgement.optString("run_id") == request.optString("run_id") &&
            reference.optString("run_id") == request.optString("run_id") &&
            acknowledgement.optString("generation_id") == reference.optString("generation_id") &&
            acknowledgement.optString("manifest_sha256") == reference.optString("manifest_sha256") &&
            acknowledgement.optString("producer_peer_id") == request.optString("desktop_peer_id") &&
            acknowledgement.optString("consumer_peer_id") == service.peerId() &&
            acknowledgement.optString("result") in setOf("consumed", "rejected")
    }

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
        /* WHY: Drive conversations reuse coordination filenames, and
         * File.renameTo() does not reliably replace an existing destination
         * on Android private storage. CONTRACT: readers observe either the
         * previous complete JSON document or its complete replacement.
         * INVARIANT: the temporary file is in the destination directory, so
         * the preferred move is same-filesystem and atomic when supported. */
        try {
            Files.move(
                temporary.toPath(),
                path.toPath(),
                StandardCopyOption.ATOMIC_MOVE,
                StandardCopyOption.REPLACE_EXISTING,
            )
        } catch (_: AtomicMoveNotSupportedException) {
            Files.move(
                temporary.toPath(),
                path.toPath(),
                StandardCopyOption.REPLACE_EXISTING,
            )
        } catch (_: Exception) {
            temporary.delete()
            throw SyncGenerationException("cannot publish ${path.name}")
        }
    }

    /* WHY: transports must advertise a stable installation before a desktop
     * can address its first request. CONTRACT: discovery publishes no domain
     * facts and does not acquire the conversation lock. INVARIANT: run() uses
     * this exact descriptor, so idle discovery cannot fork capabilities. */
    internal fun publishPeer(directory: File): String {
        if (!directory.isDirectory)
            throw SyncGenerationException("exchange directory is unavailable")
        val peer = service.peerId()
        val path = File(directory, "android-peer-v1.json")
        val descriptor =
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
                            "session-preparations-v2",
                            "programs-v1",
                        )
                    ),
                )
                .toString()
        /* WHY: the foreground listener probes every five seconds, while some
         * Android MediaProvider implementations cannot atomically replace an
         * unchanged file that is simultaneously exposed over MTP. CONTRACT:
         * an identical canonical descriptor is already a complete durable
         * publication. INVARIANT: changed or unreadable bytes still use the
         * crash-safe replacement path above. */
        if (runCatching { path.isFile && path.readText() == descriptor }.getOrDefault(false)) {
            visibility.confirm(listOf(path))
            return peer
        }
        publish(path, descriptor)
        visibility.confirm(listOf(path))
        return peer
    }

    private fun await(path: File, deadline: Long, pump: (() -> Unit)? = null): File {
        while (!path.isFile) {
            if (System.nanoTime() >= deadline)
                throw SyncGenerationException("timeout waiting for ${path.name}")
            pump?.invoke()
            Thread.sleep(50)
        }
        return path
    }

    private fun awaitCorrelated(
        path: File,
        deadline: Long,
        predicate: (JSONObject) -> Boolean,
        pump: (() -> Unit)? = null,
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
            pump?.invoke()
            Thread.sleep(50)
        }
    }

    fun run(
        directory: File,
        timeout: Duration = Duration.ofMinutes(5),
        afterPeerPublication: (() -> Unit)? = null,
        afterPublication: (() -> Unit)? = null,
        pollTransport: (() -> Unit)? = null,
        afterAcknowledgement: (() -> Unit)? = null,
    ): ForegroundGenerationResult {
        if (!conversationLock.tryLock()) {
            return ForegroundGenerationResult.Error("Synchronization already in progress.")
        }
        return try {
            val peer = publishPeer(directory)
            phase(null, "peer_published", "android-peer-v1.json")
            /* WHY: trainlog-syncd is intentionally driven by the established
             * Android request artifact, not by polling generation internals.
             * CONTRACT: each explicit foreground attempt publishes the peer
             * identity before its caller emits one fresh request signal.
             * INVARIANT: a failed attempt's request, run, generations, and
             * ACK evidence remain immutable; retry creates new correlation. */
            afterPeerPublication?.invoke()
            phase(null, "legacy_trigger_published", "trainlog-sync-request-v1.json")
            val deadline = System.nanoTime() + timeout.toNanos()
            val requestRaw =
                awaitCorrelated(File(directory, "request-v1.json"), deadline, { candidate ->
                    val runId = candidate.optString("run_id")
                    runId.startsWith("sy_") &&
                        (resumableGeneration(runId) != null ||
                            !repository.inSyncGenerationTransaction { db ->
                                db.rawQuery(
                                    "SELECT 1 FROM sync_generations WHERE run_id=? LIMIT 1",
                                    arrayOf(runId),
                                ).use { it.moveToFirst() }
                            })
                }, pollTransport)
            val request = JSONObject(requestRaw.toString(Charsets.UTF_8))
            val runId = request.getString("run_id")
            phase(runId, "generation_request_observed", "request-v1.json")
            if (request.getString("android_peer_id") != peer)
                throw SyncGenerationException("request targets another Android peer")
            val archiveAckRaw =
                awaitCorrelated(
                    File(directory, "desktop-archive-acknowledgements-v1.json"),
                    deadline,
                    {
                        it.optString("run_id") == request.getString("run_id") &&
                            it.optString("android_peer_id") == peer
                    },
                    pollTransport,
                )
            reconcileArchiveAcknowledgements(
                archiveAckRaw,
                request.getString("run_id"),
                peer,
                request.getString("desktop_peer_id"),
            )
            phase(runId, "archive_ack_observed", "desktop-archive-acknowledgements-v1.json")
            /* WHY: transport can fail after immutable generation publication.
             * CONTRACT: an explicit retry for the same run republishes that
             * exact generation and resumes ACK handling; it never recaptures
             * mutable application state under an existing identity.
             * INVARIANT: a completed inbound ledger makes the run ineligible;
             * an acknowledged outbound generation remains resumable only to
             * finish its missing inbound half. */
            phase(runId, "generation_capture_started")
            val captured =
                resumableGeneration(
                    request.getString("run_id"),
                    request.getString("desktop_peer_id"),
                ) ?: service.capture(
                    directory,
                    request.getString("desktop_peer_id"),
                    request.getString("run_id"),
                )
            phase(runId, "generation_captured", generationId = captured.generationId)
            val published = service.publish(captured, File(directory, "android-objects"))
            visibility.confirm(
                published.walkTopDown().filter(File::isFile).sortedBy { it.name == "manifest.json" }.toList()
            )
            phase(
                runId,
                "generation_objects_published",
                published.relativeTo(directory).path,
                captured.generationId,
            )
            val generationReference = File(directory, "android-generation-v1.json")
            publish(
                generationReference,
                JSONObject()
                    .put("format", "trainlog-sync-generation-reference")
                    .put("version", 1)
                    .put("run_id", captured.runId)
                    .put("generation_id", captured.generationId)
                    .put("manifest_sha256", captured.manifestSha256)
                    .put("relative_path", published.relativeTo(directory).path)
                    .toString(),
            )
            visibility.confirm(listOf(generationReference))
            phase(
                runId,
                "generation_reference_published",
                generationReference.name,
                captured.generationId,
            )
            afterPublication?.invoke()
            // CONTRACT: durable objects from earlier conversations remain in
            // the exchange directory. Only this run's exact generation ACK
            // can advance its lineage; stale bytes are retained and ignored.
            val desktopAck =
                awaitCorrelated(File(directory, "desktop-consumption-ack-v1.json"), deadline, {
                    it.optString("run_id") == captured.runId &&
                        it.optString("generation_id") == captured.generationId
                }, pollTransport)
            service.acceptAcknowledgement(desktopAck)
            phase(runId, "desktop_ack_observed", "desktop-consumption-ack-v1.json", captured.generationId)
            val referenceRaw =
                awaitCorrelated(File(directory, "desktop-generation-v1.json"), deadline, {
                    it.optString("run_id") == captured.runId
                }, pollTransport)
            val reference = JSONObject(referenceRaw.toString(Charsets.UTF_8))
            phase(
                runId,
                "desktop_generation_observed",
                "desktop-generation-v1.json",
                reference.optString("generation_id"),
            )
            /* WHY: non-atomic transports can expose a newly downloaded
             * reference locally before the corresponding immutable objects
             * have reached this process. CONTRACT: correlation validates the
             * reference first, then one transport refresh stages its committed
             * generation before Core validates hashes and imports it.
             * INVARIANT: the refresh moves bytes only; service.consume remains
             * the sole validation and mutation authority. */
            pollTransport?.invoke()
            val acknowledgement =
                service.consume(File(directory, reference.getString("relative_path")))
            phase(
                runId,
                "desktop_generation_consumed",
                generationId = reference.optString("generation_id"),
            )
            val acknowledgementPath = File(directory, "android-consumption-ack-v1.json")
            publish(acknowledgementPath, acknowledgement)
            visibility.confirm(listOf(acknowledgementPath))
            phase(
                runId,
                "android_ack_published",
                acknowledgementPath.name,
                reference.optString("generation_id"),
            )
            afterAcknowledgement?.invoke()
            phase(runId, "completed", result = "completed")
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
                    visibility.confirm(listOf(File(directory, "android-generation-error-v1.json")))
                } catch (_: Exception) {
                    // Preserve the original local failure when error reporting
                    // cannot itself be durably published.
                }
            }
            phase(
                runCatching {
                    JSONObject(File(directory, "request-v1.json").readText()).optString("run_id")
                }.getOrNull(),
                "error",
                result = error.javaClass.simpleName,
            )
            ForegroundGenerationResult.Error(error.message ?: "generation synchronization failed")
        } finally {
            conversationLock.unlock()
        }
    }
}

/* Compatibility name for focused tests and callers outside the coordinator
 * migration. New triggers should depend on SyncGenerationCoordinator. */
internal typealias SyncGenerationForegroundCoordinator = SyncGenerationCoordinator
