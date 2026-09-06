package com.labfytools.trainlog.data

import android.content.ContentUris
import android.content.ContentValues
import android.content.Context
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.MediaStore

sealed interface SyncExportResult {
    data class Exported(
        val displayPath: String,
        val bytes: Int,
    ) : SyncExportResult

    data object Unsupported :
        SyncExportResult

    data class Error(
        val message: String,
    ) : SyncExportResult
}

class SyncExporter(
    context: Context,
    private val repository:
        TrainlogRepository,
) {
    private val appContext =
        context.applicationContext

    fun exportMobileBundle():
        SyncExportResult {
        if (
            Build.VERSION.SDK_INT <
            Build.VERSION_CODES.Q
        ) {
            return SyncExportResult.Unsupported
        }

        val json =
            repository.buildMobileExportJson()

        val bytes =
            json.toByteArray(
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
            Environment.DIRECTORY_DOWNLOADS +
                "/Trainlog/"

        val displayName =
            "trainlog-mobile-export-v1.json"

        val existing =
            findExisting(
                collection,
                displayName,
                relativePath
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

            if (inserted == null) {
                return SyncExportResult.Error(
                    "Création du fichier impossible."
                )
            }

            targetUri = inserted
            created = true
        }

        try {
            val stream =
                resolver.openOutputStream(
                    targetUri,
                    "wt"
                )

            if (stream == null) {
                if (created) {
                    resolver.delete(
                        targetUri,
                        null,
                        null
                    )
                }

                return SyncExportResult.Error(
                    "Flux d'écriture indisponible."
                )
            }

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

            return SyncExportResult.Exported(
                displayPath =
                    "Download/Trainlog/" +
                        displayName,
                bytes =
                    bytes.size,
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

            return SyncExportResult.Error(
                error.message
                    ?: "Erreur d'export."
            )
        }
    }

    private fun findExisting(
        collection: Uri,
        displayName: String,
        relativePath: String,
    ): Uri? {
        val projection =
            arrayOf(
                MediaStore
                    .MediaColumns
                    ._ID
            )

        val selection =
            (
                MediaStore
                    .MediaColumns
                    .DISPLAY_NAME +
                    " = ? AND " +
                    MediaStore
                        .MediaColumns
                        .RELATIVE_PATH +
                    " = ?"
            )

        val cursor =
            appContext
                .contentResolver
                .query(
                    collection,
                    projection,
                    selection,
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

            val itemId =
                cursor.getLong(0)

            return ContentUris.withAppendedId(
                collection,
                itemId
            )
        } finally {
            cursor.close()
        }
    }
}
