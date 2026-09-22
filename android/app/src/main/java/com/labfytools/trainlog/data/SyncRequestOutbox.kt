/*
 * Android SyncRequestOutbox.
 *
 * Owns this data-layer boundary while keeping UI state, canonical desktop history, and exchange contracts separate.
 */
package com.labfytools.trainlog.data

import android.content.Context
import android.os.Build
import org.json.JSONObject
import java.time.OffsetDateTime
import java.util.UUID
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

internal const val FULL_GENERATION_REQUEST_NAME =
    "trainlog-sync-full-generation-request-v1.json"
internal const val LEGACY_SYNC_REQUEST_NAME = "trainlog-sync-request-v1.json"

internal data class SyncRequestIntent(
    val requestId: String,
    val requestedAt: String,
) {
    fun payload(): String =
        JSONObject()
            .put("format", "trainlog-sync-request")
            .put("version", 1)
            .put("request_id", requestId)
            .put("requested_at", requestedAt)
            .toString()
}

sealed interface SyncRequestResult {
    /** The full-generation coordinator completed both durable peer ACKs. */
    data class Completed(
        val runId: String,
    ) : SyncRequestResult

    data class Requested(
        val requestId: String,
    ) : SyncRequestResult

    data object Unsupported :
        SyncRequestResult

    data class Error(
        val message: String,
    ) : SyncRequestResult
}

class SyncRequestOutbox private constructor(
    private val publisher: DirectExchangePublisher,
    private val visibility: MtpPublicationVisibility,
) {
    constructor(context: Context) : this(
        DirectExchangePublisher { directExchangeDirectory(context) },
        AndroidMtpPublicationVisibility(context),
    )

    internal constructor(directoryProvider: () -> DirectExchangeDirectoryAccess?) : this(
        DirectExchangePublisher(directoryProvider),
        ImmediateMtpPublicationVisibility,
    )

    suspend fun requestSync(): SyncRequestResult = withContext(Dispatchers.IO) {
        when (val opened = publisher.snapshot()) {
            is DirectExchangeSnapshotResult.Ready -> requestSync(opened.snapshot)
            is DirectExchangeSnapshotResult.Error -> SyncRequestResult.Error(opened.message)
        }
    }

    internal fun requestFullGeneration(): SyncRequestResult {
        return when (val opened = publisher.snapshot()) {
            is DirectExchangeSnapshotResult.Ready -> {
                val intent = newIntent()
                publishFullGeneration(opened.snapshot, intent)
            }
            is DirectExchangeSnapshotResult.Error -> SyncRequestResult.Error(opened.message)
        }
    }

    internal fun requestSync(snapshot: DirectExchangeSnapshot): SyncRequestResult {
        if (
            Build.VERSION.SDK_INT <
            Build.VERSION_CODES.Q
        ) {
            return SyncRequestResult.Unsupported
        }

        val intent = newIntent()
        return publishLegacy(snapshot, intent)
    }

    internal fun newIntent(): SyncRequestIntent =
        SyncRequestIntent(
            requestId = "sr_${UUID.randomUUID()}",
            requestedAt = OffsetDateTime.now().toString(),
        )

    internal fun publishFullGeneration(
        snapshot: DirectExchangeSnapshot,
        intent: SyncRequestIntent,
    ): SyncRequestResult = publish(snapshot, FULL_GENERATION_REQUEST_NAME, intent)

    internal fun publishLegacy(
        snapshot: DirectExchangeSnapshot,
        intent: SyncRequestIntent,
    ): SyncRequestResult = publish(snapshot, LEGACY_SYNC_REQUEST_NAME, intent)

    private fun publish(
        snapshot: DirectExchangeSnapshot,
        displayName: String,
        intent: SyncRequestIntent,
    ): SyncRequestResult {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            return SyncRequestResult.Unsupported
        }
        val error = snapshot.writeJson(displayName, intent.payload())
        return if (error == null) {
            try {
                visibility.confirm(listOf(java.io.File(canonicalExchangeDirectory(), displayName)))
                SyncRequestResult.Requested(intent.requestId)
            } catch (error: Exception) {
                SyncRequestResult.Error(error.message ?: "Publication MTP impossible.")
            }
        } else {
            SyncRequestResult.Error(error)
        }
    }
}
