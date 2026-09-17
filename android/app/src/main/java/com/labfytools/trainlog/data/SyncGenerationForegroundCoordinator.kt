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
internal class SyncGenerationForegroundCoordinator(private val repository: TrainlogRepository) {
    private val service = SyncGenerationService(repository)

    private fun publish(path: File, value: String) {
        val temporary = File(path.parentFile, ".${path.name}.tmp-${UUID.randomUUID()}")
        FileOutputStream(temporary).use { stream ->
            stream.write(value.toByteArray()); stream.flush(); stream.fd.sync()
        }
        if (!temporary.renameTo(path)) throw SyncGenerationException("cannot publish ${path.name}")
    }

    private fun await(path: File, deadline: Long): File {
        while (!path.isFile) {
            if (System.nanoTime() >= deadline) throw SyncGenerationException("timeout waiting for ${path.name}")
            Thread.sleep(50)
        }
        return path
    }

    fun run(directory: File, timeout: Duration = Duration.ofMinutes(5), afterPublication: (() -> Unit)? = null): ForegroundGenerationResult = try {
        if (!directory.isDirectory) throw SyncGenerationException("exchange directory is unavailable")
        val peer = service.peerId()
        publish(File(directory, "android-peer-v1.json"), JSONObject()
            .put("format", "trainlog-sync-peer").put("version", 1).put("peer_id", peer)
            .put("capabilities", JSONArray(listOf("generation-manifest-v1", "generation-ack-v1",
                "causal-delete-v1", "mobile-history-v4", "execution-draft-v1"))).toString())
        val deadline = System.nanoTime() + timeout.toNanos()
        val request = JSONObject(await(File(directory, "request-v1.json"), deadline).readText())
        if (request.getString("android_peer_id") != peer) throw SyncGenerationException("request targets another Android peer")
        val captured = service.capture(directory, request.getString("desktop_peer_id"), request.getString("run_id"))
        val published = service.publish(captured, File(directory, "android-objects"))
        publish(File(directory, "android-generation-v1.json"), JSONObject()
            .put("format", "trainlog-sync-generation-reference").put("version", 1)
            .put("run_id", captured.runId).put("generation_id", captured.generationId)
            .put("manifest_sha256", captured.manifestSha256)
            .put("relative_path", published.relativeTo(directory).path).toString())
        afterPublication?.invoke()
        service.acceptAcknowledgement(await(File(directory, "desktop-consumption-ack-v1.json"), deadline).readBytes())
        val reference = JSONObject(await(File(directory, "desktop-generation-v1.json"), deadline).readText())
        val acknowledgement = service.consume(File(directory, reference.getString("relative_path")))
        publish(File(directory, "android-consumption-ack-v1.json"), acknowledgement)
        ForegroundGenerationResult.Completed(captured.runId)
    } catch (error: Exception) {
        ForegroundGenerationResult.Error(error.message ?: "generation synchronization failed")
    }
}
