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

        /* CONTRACT: capture all four artifacts before publishing any of
         * them.  The files are separate for compatibility, but a user edit
         * must not make a newly-written V2 reference a definition assembled
         * from a different logical export state. */
        val definitionsJson: String
        val mobileJson: String
        val associationsJson: String
        val bodyZonesJson: String
        val aliasesJson: String
        val feedbackJson: String
        try {
            definitionsJson = repository.buildEquipmentDefinitionsJson()
            /* V3 is the authoritative mobile session exchange. V1/V2 remain
             * readable by desktop for historic devices but is not published. */
            mobileJson = repository.buildMobileExportV3Json()
            associationsJson = repository.buildEquipmentAssociationsJson()
            bodyZonesJson = repository.buildExerciseBodyZonesJson()
            aliasesJson = repository.buildExerciseAliasesJson()
            feedbackJson = repository.buildTrainingFeedbackJson()
        } catch (error: Exception) {
            return SyncExportResult.Error(
                error.message ?: "Préparation de l'export impossible.",
            )
        }

        val bytes = mobileJson.toByteArray(Charsets.UTF_8)

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
            "trainlog-mobile-export-v3.json"

        /* Definitions are visible before any session file which may reference a
         * custom ID; a failed definition write aborts the bundle. */
        val definitionsError = writeEquipmentDefinitions(definitionsJson)
        if (definitionsError != null) return SyncExportResult.Error(definitionsError)

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

            val bodyZonesError = writeBodyZones(bodyZonesJson)
            if (bodyZonesError != null) return SyncExportResult.Error(bodyZonesError)
            val aliasesError = writeJsonCompanion(
                "trainlog-exercise-aliases-v1.json",
                aliasesJson,
                "alias exercice",
            )
            if (aliasesError != null) return SyncExportResult.Error(aliasesError)
            val feedbackError = writeJsonCompanion(
                "trainlog-training-feedback-v2.json", feedbackJson, "ressentis d’entraînement")
            if (feedbackError != null) return SyncExportResult.Error(feedbackError)
            /* CONTRACT: publication, not JSON construction, establishes the
             * common sync ancestor. Applying the exact local snapshot can only
             * record equal baselines; the strict reconciler never unions zones. */
            when (val acknowledgement = repository.applyExerciseBodyZonesJson(bodyZonesJson)) {
                is ExerciseBodyZoneImportResult.Applied -> Unit
                is ExerciseBodyZoneImportResult.Conflict ->
                    return SyncExportResult.Error(
                        "Conflit pendant l'enregistrement de la baseline zones : " +
                            acknowledgement.exerciseId,
                    )
                is ExerciseBodyZoneImportResult.Invalid ->
                    return SyncExportResult.Error(acknowledgement.message)
                ExerciseBodyZoneImportResult.DatabaseError ->
                    return SyncExportResult.Error(
                        "Enregistrement de la baseline zones impossible.",
                    )
            }
            val companionError = writeEquipmentAssociations(associationsJson)
            if (companionError != null) {
                return SyncExportResult.Error(companionError)
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

    /** Publish the companion separately so frozen mobile-export-v1 stays byte-compatible. */
    private fun writeEquipmentAssociations(json: String): String? {
        val resolver = appContext.contentResolver
        val collection = MediaStore.Downloads.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY)
        val relativePath = Environment.DIRECTORY_DOWNLOADS + "/Trainlog/"
        val name = "trainlog-equipment-associations-v2.json"
        val existing = findExisting(collection, name, relativePath)
        val created = existing == null
        val uri = existing ?: resolver.insert(collection, ContentValues().apply {
            put(MediaStore.MediaColumns.DISPLAY_NAME, name)
            put(MediaStore.MediaColumns.MIME_TYPE, "application/json")
            put(MediaStore.MediaColumns.RELATIVE_PATH, relativePath)
            put(MediaStore.MediaColumns.IS_PENDING, 1)
        }) ?: return "Création de l'extension équipement impossible."
        return try {
            resolver.openOutputStream(uri, "wt")?.use {
                it.write(json.toByteArray(Charsets.UTF_8))
                it.flush()
            } ?: return "Écriture de l'extension équipement impossible."
            if (created) resolver.update(uri, ContentValues().apply {
                put(MediaStore.MediaColumns.IS_PENDING, 0)
            }, null, null)
            null
        } catch (error: Exception) {
            if (created) resolver.delete(uri, null, null)
            error.message ?: "Export extension équipement impossible."
        }
    }

    private fun writeEquipmentDefinitions(json: String): String? {
        val resolver = appContext.contentResolver
        val collection = MediaStore.Downloads.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY)
        val relativePath = Environment.DIRECTORY_DOWNLOADS + "/Trainlog/"
        val name = "trainlog-mobile-equipment-definitions-v1.json"
        val existing = findExisting(collection, name, relativePath)
        val created = existing == null
        val uri = existing ?: resolver.insert(collection, ContentValues().apply {
            put(MediaStore.MediaColumns.DISPLAY_NAME, name)
            put(MediaStore.MediaColumns.MIME_TYPE, "application/json")
            put(MediaStore.MediaColumns.RELATIVE_PATH, relativePath)
            put(MediaStore.MediaColumns.IS_PENDING, 1)
        }) ?: return "Création des définitions équipement impossible."
        return try {
            resolver.openOutputStream(uri, "wt")?.use {
                it.write(json.toByteArray(Charsets.UTF_8)); it.flush()
            } ?: return "Écriture des définitions équipement impossible."
            if (created) resolver.update(uri, ContentValues().apply {
                put(MediaStore.MediaColumns.IS_PENDING, 0)
            }, null, null)
            null
        } catch (error: Exception) {
            if (created) resolver.delete(uri, null, null)
            error.message ?: "Export définitions équipement impossible."
        }
    }

    private fun writeJsonCompanion(name: String, json: String, label: String): String? {
        val resolver = appContext.contentResolver
        val collection = MediaStore.Downloads.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY)
        val relativePath = Environment.DIRECTORY_DOWNLOADS + "/Trainlog/"
        val existing = findExisting(collection, name, relativePath)
        val created = existing == null
        val uri = existing ?: resolver.insert(collection, ContentValues().apply {
            put(MediaStore.MediaColumns.DISPLAY_NAME, name)
            put(MediaStore.MediaColumns.MIME_TYPE, "application/json")
            put(MediaStore.MediaColumns.RELATIVE_PATH, relativePath)
            put(MediaStore.MediaColumns.IS_PENDING, 1)
        }) ?: return "Création du compagnon $label impossible."
        return try {
            resolver.openOutputStream(uri, "wt")?.use {
                it.write(json.toByteArray(Charsets.UTF_8)); it.flush()
            } ?: return "Écriture du compagnon $label impossible."
            if (created) resolver.update(uri, ContentValues().apply {
                put(MediaStore.MediaColumns.IS_PENDING, 0)
            }, null, null)
            null
        } catch (error: Exception) {
            if (created) resolver.delete(uri, null, null)
            error.message ?: "Export du compagnon $label impossible."
        }
    }

    /** One directional-neutral companion is used by both peers. */
    private fun writeBodyZones(json: String): String? {
        val resolver = appContext.contentResolver
        val collection = MediaStore.Downloads.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY)
        val relativePath = Environment.DIRECTORY_DOWNLOADS + "/Trainlog/"
        val name = "trainlog-exercise-body-zones-v1.json"
        val existing = findExisting(collection, name, relativePath)
        val created = existing == null
        val uri = existing ?: resolver.insert(collection, ContentValues().apply {
            put(MediaStore.MediaColumns.DISPLAY_NAME, name)
            put(MediaStore.MediaColumns.MIME_TYPE, "application/json")
            put(MediaStore.MediaColumns.RELATIVE_PATH, relativePath)
            put(MediaStore.MediaColumns.IS_PENDING, 1)
        }) ?: return "Création des zones corporelles impossible."
        return try {
            resolver.openOutputStream(uri, "wt")?.use {
                it.write(json.toByteArray(Charsets.UTF_8)); it.flush()
            } ?: return "Écriture des zones corporelles impossible."
            if (created) resolver.update(uri, ContentValues().apply {
                put(MediaStore.MediaColumns.IS_PENDING, 0)
            }, null, null)
            null
        } catch (error: Exception) {
            if (created) resolver.delete(uri, null, null)
            error.message ?: "Export des zones corporelles impossible."
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
