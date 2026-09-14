package com.labfytools.trainlog.data

import android.content.Context
import android.os.Build
import org.json.JSONObject
import java.time.OffsetDateTime
import java.util.UUID
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

sealed interface SyncRequestResult {
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
) {
    constructor(context: Context) : this(
        DirectExchangePublisher { directExchangeDirectory(context) },
    )

    internal constructor(directoryProvider: () -> DirectExchangeDirectoryAccess?) : this(
        DirectExchangePublisher(directoryProvider),
    )

    suspend fun requestSync(): SyncRequestResult = withContext(Dispatchers.IO) {
        when (val opened = publisher.snapshot()) {
            is DirectExchangeSnapshotResult.Ready -> requestSync(opened.snapshot)
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

        val requestId =
            "sr_" +
                UUID.randomUUID()
                    .toString()

        val payload =
            JSONObject()
                .put(
                    "format",
                    "trainlog-sync-request",
                )
                .put(
                    "version",
                    1,
                )
                .put(
                    "request_id",
                    requestId,
                )
                .put(
                    "requested_at",
                    OffsetDateTime.now()
                        .toString(),
                )
                .toString()

        val displayName =
            "trainlog-sync-request-v1.json"
        val error = snapshot.writeJson(displayName, payload)
        return if (error == null) {
            SyncRequestResult.Requested(requestId)
        } else {
            SyncRequestResult.Error(error)
        }
    }
}
