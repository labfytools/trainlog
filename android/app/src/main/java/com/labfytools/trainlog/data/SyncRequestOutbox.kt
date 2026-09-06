package com.labfytools.trainlog.data

import android.content.ContentUris
import android.content.ContentValues
import android.content.Context
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.MediaStore
import org.json.JSONObject
import java.time.OffsetDateTime
import java.util.UUID

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

class SyncRequestOutbox(
    context: Context,
) {
    private val appContext =
        context.applicationContext

    fun requestSync():
        SyncRequestResult {
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

        val bytes =
            payload.toByteArray(
                Charsets.UTF_8
            )

        val resolver =
            appContext.contentResolver

        val collection =
            MediaStore.Downloads
                .getContentUri(
                    MediaStore
                        .VOLUME_EXTERNAL_PRIMARY
                )

        val relativePath =
            Environment
                .DIRECTORY_DOWNLOADS +
                "/Trainlog/"

        val displayName =
            "trainlog-sync-request-v1.json"

        val existing =
            findExisting(
                collection,
                displayName,
                relativePath,
            )

        val targetUri: Uri
        val created: Boolean

        if (existing != null) {
            targetUri = existing
            created = false
        } else {
            val values =
                ContentValues().apply {
                    put(
                        MediaStore
                            .MediaColumns
                            .DISPLAY_NAME,
                        displayName
                    )

                    put(
                        MediaStore
                            .MediaColumns
                            .MIME_TYPE,
                        "application/json"
                    )

                    put(
                        MediaStore
                            .MediaColumns
                            .RELATIVE_PATH,
                        relativePath
                    )

                    put(
                        MediaStore
                            .MediaColumns
                            .IS_PENDING,
                        1
                    )
                }

            val inserted =
                resolver.insert(
                    collection,
                    values
                )
                ?: return SyncRequestResult.Error(
                    "Création de la demande impossible."
                )

            targetUri = inserted
            created = true
        }

        return try {
            val stream =
                resolver.openOutputStream(
                    targetUri,
                    "wt"
                )
                ?: return SyncRequestResult.Error(
                    "Écriture de la demande impossible."
                )

            stream.use {
                it.write(bytes)
                it.flush()
            }

            if (created) {
                val finished =
                    ContentValues().apply {
                        put(
                            MediaStore
                                .MediaColumns
                                .IS_PENDING,
                            0
                        )
                    }

                resolver.update(
                    targetUri,
                    finished,
                    null,
                    null
                )
            }

            SyncRequestResult.Requested(
                requestId
            )
        } catch (
            error: Exception
        ) {
            if (created) {
                resolver.delete(
                    targetUri,
                    null,
                    null
                )
            }

            SyncRequestResult.Error(
                error.message
                    ?: "Demande de synchronisation impossible."
            )
        }
    }

    private fun findExisting(
        collection: Uri,
        displayName: String,
        relativePath: String,
    ): Uri? {
        val cursor =
            appContext
                .contentResolver
                .query(
                    collection,
                    arrayOf(
                        MediaStore
                            .MediaColumns
                            ._ID
                    ),
                    (
                        MediaStore
                            .MediaColumns
                            .DISPLAY_NAME +
                            " = ? AND " +
                            MediaStore
                                .MediaColumns
                                .RELATIVE_PATH +
                            " = ?"
                    ),
                    arrayOf(
                        displayName,
                        relativePath,
                    ),
                    null,
                )
                ?: return null

        try {
            if (!cursor.moveToFirst()) {
                return null
            }

            return ContentUris
                .withAppendedId(
                    collection,
                    cursor.getLong(0)
                )
        } finally {
            cursor.close()
        }
    }
}
