package com.labfytools.trainlog.data

import android.content.ContentValues
import android.content.Context
import android.database.sqlite.SQLiteConstraintException
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteOpenHelper
import com.labfytools.trainlog.model.ActiveSessionDraft
import com.labfytools.trainlog.model.BodyObservationDraft
import com.labfytools.trainlog.model.BodyObservationSummary
import com.labfytools.trainlog.model.ExerciseDataFields
import com.labfytools.trainlog.model.ExerciseProfile
import com.labfytools.trainlog.model.ExerciseEditInput
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionDraft
import com.labfytools.trainlog.model.SessionDraftForm
import com.labfytools.trainlog.model.SessionExerciseDraft
import com.labfytools.trainlog.model.SessionSummary
import com.labfytools.trainlog.model.SessionDetail
import com.labfytools.trainlog.model.SessionExerciseDetail
import com.labfytools.trainlog.model.SessionSetDraft
import com.labfytools.trainlog.model.SessionType
import com.labfytools.trainlog.model.TrackingMode
import org.json.JSONArray
import org.json.JSONObject
import java.text.Normalizer
import java.time.OffsetDateTime
import java.util.Locale
import java.util.UUID

sealed interface CreateExerciseResult {
    data class Created(
        val exercise: ExerciseProfile,
    ) : CreateExerciseResult

    data object Conflict :
        CreateExerciseResult

    data object Invalid :
        CreateExerciseResult
}

sealed interface EditExerciseResult {
    data class Saved(
        val exercise: ExerciseProfile,
    ) : EditExerciseResult

    data object InvalidNameOrProfile : EditExerciseResult
    data object Conflict : EditExerciseResult
    data object IncompatibleProfileChange : EditExerciseResult
    data object DatabaseError : EditExerciseResult
}


sealed interface PcCatalogImportResult {
    data class Applied(
        val imported: Int,
        val reconciled: Int,
        val skipped: Int,
    ) : PcCatalogImportResult

    data class Invalid(
        val message: String,
    ) : PcCatalogImportResult

    data object DatabaseError :
        PcCatalogImportResult
}

/** Read-only diagnostic emitted by the production PC-catalog importer. */
data class PcCatalogExerciseDecision(
    val exerciseId: String,
    val name: String,
    val recordingMode: String,
    val trackingMode: String,
    val dataFields: Int,
    val lookup: String,
    val decision: String,
)

sealed interface EquipmentAssociationImportResult {
    data class Applied(val updated: Int) : EquipmentAssociationImportResult
    data class Invalid(val message: String) : EquipmentAssociationImportResult
    data object DatabaseError : EquipmentAssociationImportResult
}

sealed interface EquipmentDefinitionImportResult {
    data class Applied(val imported: Int, val skipped: Int) : EquipmentDefinitionImportResult
    data class Invalid(val message: String) : EquipmentDefinitionImportResult
    data object DatabaseError : EquipmentDefinitionImportResult
}

sealed interface MobileSessionImportResult {
    /** CONTRACT: counters describe persistent mutations, not artifact size. */
    data class Applied(
        val sessionsAdded: Int,
        val sessionsSkipped: Int,
        val bodyObservationsAdded: Int,
        val bodyObservationsSkipped: Int,
    ) : MobileSessionImportResult
    data class Invalid(val message: String) : MobileSessionImportResult
    data object DatabaseError : MobileSessionImportResult
}

sealed interface CreateEquipmentResult {
    data class Created(val equipment: EquipmentCatalogEntry) : CreateEquipmentResult
    data object Invalid : CreateEquipmentResult
    data object Conflict : CreateEquipmentResult
    data class DatabaseError(val message: String) : CreateEquipmentResult
}

sealed interface SaveBodyObservationResult {
    data class Saved(
        val observationId: String,
    ) : SaveBodyObservationResult

    data object Invalid :
        SaveBodyObservationResult

    data object DatabaseError :
        SaveBodyObservationResult
}

sealed interface SaveSessionResult {
    data class Saved(
        val sessionId: String,
    ) : SaveSessionResult

    data object Invalid :
        SaveSessionResult

    data object DatabaseError :
        SaveSessionResult
}

sealed interface ActiveDraftLoadResult {
    data class Loaded(
        val draft: ActiveSessionDraft,
        val warning: String? = null,
    ) : ActiveDraftLoadResult

    data object None : ActiveDraftLoadResult

    data class Error(
        val message: String,
    ) : ActiveDraftLoadResult
}

sealed interface ActiveDraftMutationResult {
    data object Saved : ActiveDraftMutationResult

    data class Error(
        val message: String,
    ) : ActiveDraftMutationResult
}

sealed interface FinalizeActiveDraftResult {
    data class Saved(
        val sessionId: String,
    ) : FinalizeActiveDraftResult

    data class Invalid(
        val message: String,
    ) : FinalizeActiveDraftResult

    data class DatabaseError(
        val message: String,
    ) : FinalizeActiveDraftResult
}

class TrainlogRepository(
    context: Context,
    databaseName: String =
        ANDROID_DATABASE_NAME,
) {
    private val applicationContext = context.applicationContext
    private val database =
        TrainlogDatabaseHelper(
            applicationContext,
            databaseName,
        )

    fun close() {
        database.close()
    }

    fun listEquipment(): List<EquipmentCatalogEntry> {
        val output = mutableListOf<EquipmentCatalogEntry>()
        database.readableDatabase.query(
            "equipment",
            arrayOf("equipment_id", "label_name", "display_name", "equipment_type", "load_semantics"),
            null, null, null, null, "display_name COLLATE NOCASE, equipment_id",
        ).use { cursor ->
            while (cursor.moveToNext()) {
                val id = cursor.getString(0)
                val aliases = mutableListOf<String>()
                database.readableDatabase.rawQuery(
                    "SELECT alias FROM equipment_aliases ea JOIN equipment e ON e.id = ea.equipment_row_id WHERE e.equipment_id = ? ORDER BY alias COLLATE NOCASE;",
                    arrayOf(id),
                ).use { aliasCursor -> while (aliasCursor.moveToNext()) aliases += aliasCursor.getString(0) }
                output += EquipmentCatalogEntry(
                    equipmentId = id,
                    labelName = cursor.getString(1),
                    displayName = cursor.getString(2),
                    aliases = aliases,
                    type = cursor.getString(3),
                    loadSemantics = EquipmentLoadSemantics.valueOf(cursor.getString(4).uppercase(Locale.ROOT)),
                )
            }
        }
        return output
    }

    fun searchEquipment(query: String): List<EquipmentCatalogEntry> =
        EquipmentCatalog.search(listEquipment(), query)

    /** Custom equipment is local user data, not an edit to the bundled manifest. */
    fun createCustomEquipment(name: String): CreateEquipmentResult {
        val displayName = name.trim().replace(Regex("\\s+"), " ")
        if (displayName.isBlank() || displayName.length > 120) return CreateEquipmentResult.Invalid
        return try {
            val db = database.writableDatabase
            val duplicate = db.rawQuery(
                "SELECT 1 FROM equipment WHERE lower(display_name) = lower(?) LIMIT 1;", arrayOf(displayName),
            ).use { it.moveToFirst() }
            if (duplicate) return CreateEquipmentResult.Conflict
            val entry = EquipmentCatalogEntry(
                equipmentId = "eq_" + UUID.randomUUID(), labelName = "", displayName = displayName,
                aliases = emptyList(), type = "custom_machine", loadSemantics = EquipmentLoadSemantics.EXTERNAL,
            )
            val values = ContentValues().apply {
                put("equipment_id", entry.equipmentId); put("label_name", entry.labelName)
                put("display_name", entry.displayName); put("equipment_type", entry.type)
                put("load_semantics", "external")
            }
            db.insertOrThrow("equipment", null, values)
            CreateEquipmentResult.Created(entry)
        } catch (error: SQLiteConstraintException) { CreateEquipmentResult.Conflict
        } catch (error: Exception) { CreateEquipmentResult.DatabaseError(error.message ?: "erreur SQLite") }
    }

    fun listExercises(): List<ExerciseProfile> {
        val output =
            mutableListOf<ExerciseProfile>()

        database.readableDatabase.query(
            "exercises",
            arrayOf(
                "exercise_id",
                "name",
                "normalized_name",
                "recording_mode",
                "tracking_mode",
                "data_fields",
            ),
            null,
            null,
            null,
            null,
            "name COLLATE NOCASE, exercise_id",
        ).use { cursor ->
            val idIndex =
                cursor.getColumnIndexOrThrow(
                    "exercise_id"
                )

            val nameIndex =
                cursor.getColumnIndexOrThrow(
                    "name"
                )

            val normalizedIndex =
                cursor.getColumnIndexOrThrow(
                    "normalized_name"
                )

            val recordingIndex =
                cursor.getColumnIndexOrThrow(
                    "recording_mode"
                )

            val trackingIndex =
                cursor.getColumnIndexOrThrow(
                    "tracking_mode"
                )

            val fieldsIndex =
                cursor.getColumnIndexOrThrow(
                    "data_fields"
                )

            while (cursor.moveToNext()) {
                output +=
                    ExerciseProfile(
                        exerciseId =
                            cursor.getString(
                                idIndex
                            ),
                        name =
                            cursor.getString(
                                nameIndex
                            ),
                        normalizedName =
                            cursor.getString(
                                normalizedIndex
                            ),
                        recordingMode =
                            when (
                                cursor.getString(
                                    recordingIndex
                                )
                            ) {
                                "continuous" ->
                                    RecordingMode.CONTINUOUS

                                else ->
                                    RecordingMode.SETS
                            },
                        trackingMode =
                            when (
                                cursor.getString(
                                    trackingIndex
                                )
                            ) {
                                "duration" ->
                                    TrackingMode.DURATION

                                else ->
                                    TrackingMode.REPS
                            },
                        dataFields =
                            cursor.getInt(
                                fieldsIndex
                            ),
                    )
            }
        }

        return output
    }

    fun createExercise(
        input: NewExerciseProfile,
    ): CreateExerciseResult {
        if (!input.validate()) {
            return CreateExerciseResult.Invalid
        }

        val name =
            input.name.trim()

        val normalized =
            normalizeName(name)

        if (normalized.isEmpty()) {
            return CreateExerciseResult.Invalid
        }

        val exercise =
            ExerciseProfile(
                exerciseId =
                    "ex_" +
                        UUID.randomUUID()
                            .toString(),
                name = name,
                normalizedName =
                    normalized,
                recordingMode =
                    input.recordingMode,
                trackingMode =
                    input.trackingMode,
                dataFields =
                    input.dataFields,
            )

        val sql =
            """
            INSERT INTO exercises(
                exercise_id,
                name,
                normalized_name,
                recording_mode,
                tracking_mode,
                data_fields
            ) VALUES(?, ?, ?, ?, ?, ?);
            """.trimIndent()

        return try {
            database.writableDatabase.execSQL(
                sql,
                arrayOf<Any?>(
                    exercise.exerciseId,
                    exercise.name,
                    exercise.normalizedName,
                    exercise.recordingMode
                        .wireValue,
                    exercise.trackingMode
                        .wireValue,
                    exercise.dataFields,
                ),
            )

            CreateExerciseResult.Created(
                exercise
            )
        } catch (
            error: SQLiteConstraintException
        ) {
            CreateExerciseResult.Conflict
        }
    }

    /**
     * WHY: completed sessions and active drafts snapshot profile fields, but
     * keeping referenced catalog profiles immutable prevents a later catalog
     * sync/profile edit from appearing to change an established exercise.
     * A rename remains safe because all relationships use the unchanged row.
     */
    fun canEditExerciseProfile(
        exerciseId: String,
    ): Boolean {
        val db = database.readableDatabase
        val rowId = lookupExerciseRowIdOrNull(db, exerciseId) ?: return false
        return !exerciseHasReferences(db, rowId)
    }

    fun editExercise(
        input: ExerciseEditInput,
    ): EditExerciseResult {
        if (!input.validateProfile()) {
            return EditExerciseResult.InvalidNameOrProfile
        }

        val name = input.name.trim()
        val normalized = normalizeName(name)
        if (normalized.isEmpty()) {
            return EditExerciseResult.InvalidNameOrProfile
        }

        val db = database.writableDatabase
        return try {
            db.beginTransaction()
            val current = findExerciseRow(db, "exercise_id = ?", arrayOf(input.exerciseId))
                ?: return EditExerciseResult.DatabaseError
            val profileChanged =
                current.recordingMode != input.recordingMode ||
                    current.trackingMode != input.trackingMode ||
                    current.dataFields != input.dataFields
            if (profileChanged && exerciseHasReferences(db, current.rowId)) {
                return EditExerciseResult.IncompatibleProfileChange
            }

            val nameOwner = findExerciseRow(db, "normalized_name = ?", arrayOf(normalized))
            if (nameOwner != null && nameOwner.rowId != current.rowId) {
                return EditExerciseResult.Conflict
            }

            val values = ContentValues().apply {
                put("name", name)
                put("normalized_name", normalized)
                put("recording_mode", input.recordingMode.wireValue)
                put("tracking_mode", input.trackingMode.wireValue)
                put("data_fields", input.dataFields)
            }
            if (db.update("exercises", values, "id = ?", arrayOf(current.rowId.toString())) != 1) {
                return EditExerciseResult.DatabaseError
            }
            db.setTransactionSuccessful()
            EditExerciseResult.Saved(
                ExerciseProfile(
                    exerciseId = input.exerciseId,
                    name = name,
                    normalizedName = normalized,
                    recordingMode = input.recordingMode,
                    trackingMode = input.trackingMode,
                    dataFields = input.dataFields,
                ),
            )
        } catch (error: SQLiteConstraintException) {
            EditExerciseResult.Conflict
        } catch (error: Exception) {
            EditExerciseResult.DatabaseError
        } finally {
            if (db.inTransaction()) {
                db.endTransaction()
            }
        }
    }

    fun loadActiveSessionDraft():
        ActiveDraftLoadResult =
        try {
            val restored =
                loadActiveSessionDraft(
                    database.readableDatabase
                )

            if (restored == null) {
                ActiveDraftLoadResult.None
            } else {
                ActiveDraftLoadResult.Loaded(
                    draft = restored.draft,
                    warning = restored.warning,
                )
            }
        } catch (error: Exception) {
            ActiveDraftLoadResult.Error(
                error.message
                    ?: "Lecture du brouillon impossible."
            )
        }

    fun startActiveSessionDraft():
        ActiveDraftMutationResult {
        return when (
            loadActiveSessionDraft()
        ) {
            is ActiveDraftLoadResult.Loaded ->
                ActiveDraftMutationResult.Saved

            is ActiveDraftLoadResult.Error ->
                ActiveDraftMutationResult.Error(
                    "Le brouillon existant ne peut pas être lu."
                )

            ActiveDraftLoadResult.None ->
                saveActiveSessionDraft(
                    ActiveSessionDraft()
                )
        }
    }

    fun saveActiveSessionDraft(
        draft: ActiveSessionDraft,
    ): ActiveDraftMutationResult {
        if (
            draft.exercises.any {
                !validateSessionExercise(it)
            } ||
            /* exercise_id identifies the catalogue movement. Repeated passages
             * are valid; only their stable occurrence IDs must be unique. */
            draft.exercises
                .map {
                    it.entryId
                }
                .any { it.isBlank() } ||
            draft.exercises
                .map {
                    it.entryId
                }
                .distinct()
                .size !=
            draft.exercises.size ||
            listOf(
                draft.form.setCountText,
                draft.form.repsText,
                draft.form.durationText,
                draft.form.speedText,
                draft.form.distanceText,
            ).any {
                it.length > MAX_DRAFT_FORM_TEXT_LENGTH
            }
        ) {
            return ActiveDraftMutationResult.Error(
                "Brouillon de séance invalide."
            )
        }

        var db: SQLiteDatabase? = null
        var transactionOpen = false
        return try {
            db = database.writableDatabase
            db.beginTransaction()
            transactionOpen = true
            persistActiveSessionDraft(
                db,
                draft,
            )
            db.setTransactionSuccessful()
            db.endTransaction()
            transactionOpen = false
            ActiveDraftMutationResult.Saved
        } catch (error: Exception) {
            if (transactionOpen && db?.inTransaction() == true) {
                try {
                    db.endTransaction()
                } catch (endError: Exception) {
                    error.addSuppressed(endError)
                }
            }
            ActiveDraftMutationResult.Error(
                error.message
                    ?: "Enregistrement du brouillon impossible."
            )
        }
    }

    fun discardActiveSessionDraft():
        ActiveDraftMutationResult {
        return try {
            database.writableDatabase.delete(
                "active_session_draft",
                "id = ?",
                arrayOf(ACTIVE_DRAFT_ID.toString()),
            )
            ActiveDraftMutationResult.Saved
        } catch (error: Exception) {
            ActiveDraftMutationResult.Error(
                error.message
                    ?: "Suppression du brouillon impossible."
            )
        }
    }

    fun finalizeActiveSessionDraft():
        FinalizeActiveDraftResult {
        var db: SQLiteDatabase? = null
        var transactionOpen = false
        return try {
            db = database.writableDatabase
            db.beginTransaction()
            transactionOpen = true
            val active =
                loadActiveSessionDraft(db)
            if (active == null) {
                db.endTransaction()
                transactionOpen = false
                return FinalizeActiveDraftResult.Invalid(
                    "Aucune séance en cours."
                )
            }

            val completed =
                SessionDraft(
                    exercises =
                        active.draft.exercises,
                    sessionType =
                        active.draft.sessionType,
                )

            if (
                completed.exercises.isEmpty() ||
                completed.exercises.any {
                    !validateSessionExercise(it)
                }
            ) {
                db.endTransaction()
                transactionOpen = false
                return FinalizeActiveDraftResult.Invalid(
                    "La séance en cours est invalide."
                )
            }

            val sessionId =
                insertCompletedSession(
                    db,
                    completed,
                )

            /*
             * INVARIANT: completion and draft deletion share this transaction.
             * A crash or constraint failure therefore leaves the retryable draft
             * and never exposes a completed/draft duplicate pair.
             */
            val deleted =
                db.delete(
                    "active_session_draft",
                    "id = ?",
                    arrayOf(
                        ACTIVE_DRAFT_ID
                            .toString()
                    ),
                )

            check(deleted == 1) {
                "Le brouillon finalisé n'a pas été supprimé."
            }

            db.setTransactionSuccessful()
            db.endTransaction()
            transactionOpen = false
            FinalizeActiveDraftResult.Saved(
                sessionId
            )
        } catch (error: Exception) {
            if (transactionOpen && db?.inTransaction() == true) {
                try {
                    db.endTransaction()
                } catch (endError: Exception) {
                    error.addSuppressed(endError)
                }
            }
            FinalizeActiveDraftResult.DatabaseError(
                error.message
                    ?: "Finalisation de la séance impossible."
            )
        }
    }

    fun saveSession(
        draft: SessionDraft,
    ): SaveSessionResult {
        if (
            draft.exercises.isEmpty() ||
            draft.exercises.any {
                !validateSessionExercise(it)
            }
        ) {
            return SaveSessionResult.Invalid
        }

        val db =
            database.writableDatabase

        val sessionId =
            "se_" +
                UUID.randomUUID()
                    .toString()

        val startedAt =
            OffsetDateTime.now()
                .toString()

        db.beginTransaction()

        return try {
            val sessionValues =
                ContentValues().apply {
                    put(
                        "session_id",
                        sessionId
                    )

                    put(
                        "started_at",
                        startedAt
                    )

                    put(
                        "session_type",
                        draft.sessionType
                            .wireValue
                    )
                }

            val sessionRowId =
                db.insertOrThrow(
                    "sessions",
                    null,
                    sessionValues
                )

            draft.exercises.forEachIndexed {
                    exerciseIndex,
                    exerciseDraft ->

                val exerciseRowId =
                    lookupExerciseRowId(
                        db,
                        exerciseDraft.exercise.exerciseId
                    )

                val exerciseValues =
                    ContentValues().apply {
                        put(
                            "session_row_id",
                            sessionRowId
                        )

                        put(
                            "exercise_row_id",
                            exerciseRowId
                        )

                        put(
                            "position",
                            exerciseIndex
                        )

                        put(
                            "recording_mode",
                            exerciseDraft.exercise
                                .recordingMode
                                .wireValue
                        )

                        put(
                            "tracking_mode",
                            exerciseDraft.exercise
                                .trackingMode
                                .wireValue
                        )

                        put(
                            "data_fields",
                            exerciseDraft.exercise
                                .dataFields
                        )
                        put("entry_id", exerciseDraft.entryId)
                        val equipmentRowId = lookupEquipmentRowIdOrNull(
                            db,
                            exerciseDraft.equipmentId,
                        )
                        check(exerciseDraft.equipmentId == null || equipmentRowId != null) {
                            "Unknown equipment: ${exerciseDraft.equipmentId}"
                        }
                        if (equipmentRowId == null) putNull("equipment_row_id") else put("equipment_row_id", equipmentRowId)
                    }

                val sessionExerciseRowId =
                    db.insertOrThrow(
                        "session_exercises",
                        null,
                        exerciseValues
                    )

                if (
                    exerciseDraft.exercise
                        .recordingMode ==
                    RecordingMode.CONTINUOUS
                ) {
                    val continuousValues =
                        ContentValues().apply {
                            put(
                                "session_exercise_row_id",
                                sessionExerciseRowId
                            )

                            put(
                                "duration_seconds",
                                exerciseDraft
                                    .continuousDurationSeconds
                            )

                            exerciseDraft.speedKmh
                                ?.let {
                                    put(
                                        "speed_kmh",
                                        it
                                    )
                                }

                            exerciseDraft.distanceKm
                                ?.let {
                                    put(
                                        "distance_km",
                                        it
                                    )
                                }
                        }

                    db.insertOrThrow(
                        "continuous_activity",
                        null,
                        continuousValues
                    )
                } else {
                    exerciseDraft.sets
                        .forEachIndexed {
                                setIndex,
                                set ->

                            val setValues =
                                ContentValues().apply {
                                    put(
                                        "session_exercise_row_id",
                                        sessionExerciseRowId
                                    )

                                    put(
                                        "position",
                                        setIndex
                                    )
                                    set.weightKg?.let { put("weight_kg", it) }

                                    if (
                                        exerciseDraft.exercise
                                            .trackingMode ==
                                        TrackingMode.REPS
                                    ) {
                                        put(
                                            "reps",
                                            set.reps
                                        )
                                    } else {
                                        put(
                                            "duration_seconds",
                                            set.durationSeconds
                                        )
                                    }
                                }

                            db.insertOrThrow(
                                "performed_sets",
                                null,
                                setValues
                            )
                        }
                }
            }

            db.setTransactionSuccessful()

            SaveSessionResult.Saved(
                sessionId
            )
        } catch (
            error: Exception
        ) {
            SaveSessionResult.DatabaseError
        } finally {
            db.endTransaction()
        }
    }

    fun listSessions(): List<SessionSummary> {
        val output =
            mutableListOf<SessionSummary>()

        database.readableDatabase.rawQuery(
            """
            SELECT
                s.session_id,
                s.started_at,
                s.session_type,
                COUNT(se.id)
            FROM sessions AS s
            LEFT JOIN session_exercises AS se
                ON se.session_row_id = s.id
            GROUP BY s.id
            ORDER BY s.started_at DESC, s.id DESC;
            """.trimIndent(),
            null,
        ).use { cursor ->
            while (
                cursor.moveToNext()
            ) {
                output +=
                    SessionSummary(
                        sessionId =
                            cursor.getString(0),
                        startedAt =
                            cursor.getString(1),
                        exerciseCount =
                            cursor.getInt(3),
                        sessionType =
                            SessionType.fromWire(
                                cursor.getString(2)
                            ),
                    )
            }
        }

        return output
    }




    fun applyPcCatalogJson(
        json: String,
        exerciseDecisionObserver: ((PcCatalogExerciseDecision) -> Unit)? = null,
    ): PcCatalogImportResult {
        val root =
            try {
                JSONObject(json)
            } catch (
                error: Exception
            ) {
                return PcCatalogImportResult.Invalid(
                    "Catalogue PC JSON invalide."
                )
            }

        if (
            root.optString("format") !=
                "trainlog-pc-catalog" ||
            root.optInt("version", -1) !=
                1
        ) {
            return PcCatalogImportResult.Invalid(
                "Catalogue PC non supporté."
            )
        }

        val exercises =
            root.optJSONArray(
                "exercises"
            )
                ?: return PcCatalogImportResult.Invalid(
                    "Catalogue PC sans exercices."
                )

        val db =
            database.writableDatabase

        var imported = 0
        var reconciled = 0
        var skipped = 0

        db.beginTransaction()

        return try {
            for (
                index in
                0 until exercises.length()
            ) {
                val item =
                    exercises
                        .getJSONObject(
                            index
                        )

                val exerciseId =
                    item.getString(
                        "exercise_id"
                    )

                val name =
                    item.getString(
                        "name"
                    )

                val normalized =
                    normalizeName(
                        name
                    )

                val recording =
                    when (
                        item.getString(
                            "recording_mode"
                        )
                    ) {
                        "sets" ->
                            RecordingMode.SETS

                        "continuous" ->
                            RecordingMode.CONTINUOUS

                        else ->
                            return PcCatalogImportResult.Invalid(
                                "recording_mode invalide."
                            )
                    }

                val tracking =
                    when (
                        item.getString(
                            "tracking_mode"
                        )
                    ) {
                        "reps" ->
                            TrackingMode.REPS

                        "duration" ->
                            TrackingMode.DURATION

                        else ->
                            return PcCatalogImportResult.Invalid(
                                "tracking_mode invalide."
                            )
                    }

                val dataFields =
                    item.getInt(
                        "data_fields"
                    )

                fun traceDecision(lookup: String, decision: String) {
                    exerciseDecisionObserver?.invoke(
                        PcCatalogExerciseDecision(
                            exerciseId = exerciseId,
                            name = name,
                            recordingMode = recording.wireValue,
                            trackingMode = tracking.wireValue,
                            dataFields = dataFields,
                            lookup = lookup,
                            decision = decision,
                        )
                    )
                }

                if (
                    !NewExerciseProfile(
                        name = name,
                        recordingMode = recording,
                        trackingMode = tracking,
                        dataFields = dataFields,
                    ).validate()
                ) {
                    traceDecision("not-run:invalid-profile", "conflict")
                    return PcCatalogImportResult.Invalid(
                        "Profil catalogue PC invalide pour $name."
                    )
                }

                val byId =
                    findExerciseRow(
                        db,
                        "exercise_id = ?",
                        arrayOf(
                            exerciseId
                        )
                    )

                if (byId != null) {
                    if (
                        !exerciseProfilesAreReconcilable(
                            byId,
                            recording,
                            tracking,
                            dataFields,
                        )
                    ) {
                        traceDecision(
                            "exercise_id:${byId.exerciseId}",
                            "conflict",
                        )
                        return PcCatalogImportResult.Invalid(
                            "Conflit de profil catalogue PC pour $name."
                        )
                    }

                    /* CONTRACT: a catalog name is mutable metadata.  Identity
                     * reconciliation always prefers exercise_id, so an update
                     * retains the row used by completed sessions and drafts. */
                    val nameOwner =
                        findExerciseRow(
                            db,
                            "normalized_name = ?",
                            arrayOf(normalized),
                        )
                    val mergedNameOwner =
                        nameOwner != null &&
                        nameOwner.rowId != byId.rowId
                    if (mergedNameOwner) {
                        val retired = checkNotNull(nameOwner)
                        if (
                            !exerciseProfilesAreReconcilable(
                                byId,
                                retired.recordingMode,
                                retired.trackingMode,
                                retired.dataFields,
                            ) ||
                            !catalogEquipmentProfilesAreCompatible(
                                db,
                                retired.exerciseId,
                                byId.exerciseId,
                            )
                        ) {
                            traceDecision(
                                "exercise_id:${byId.exerciseId};" +
                                    "normalized_name:${retired.exerciseId}",
                                "conflict",
                            )
                            return PcCatalogImportResult.Invalid(
                                "Conflit de nom/profil catalogue PC pour $name."
                            )
                        }

                        /* CONTRACT: the incoming PC identity is canonical on
                         * Android.  Every row-ID owner is moved before the
                         * duplicate row disappears; occurrence/session IDs and
                         * all child values remain unchanged. */
                        mergeExerciseRows(
                            db = db,
                            canonical = byId,
                            retired = retired,
                        )
                    }

                    val richerFields =
                        byId.dataFields or dataFields or
                            (nameOwner?.dataFields ?: 0)
                    if (
                        byId.name == name.trim() &&
                        byId.normalizedName == normalized &&
                        byId.dataFields == richerFields &&
                        !mergedNameOwner
                    ) {
                        skipped += 1
                        traceDecision(
                            "exercise_id:${byId.exerciseId}",
                            "existing-identical",
                        )
                        continue
                    }

                    val values = ContentValues().apply {
                        put("name", name.trim())
                        put("normalized_name", normalized)
                        put("data_fields", richerFields)
                    }
                    if (
                        db.update(
                            "exercises",
                            values,
                            "id = ?",
                            arrayOf(byId.rowId.toString()),
                        ) != 1
                    ) {
                        return PcCatalogImportResult.DatabaseError
                    }

                    reconciled += 1
                    traceDecision(
                        if (mergedNameOwner) {
                            "exercise_id:${byId.exerciseId};" +
                                "normalized_name:${nameOwner.exerciseId}"
                        } else {
                            "exercise_id:${byId.exerciseId}"
                        },
                        "existing-reconciled",
                    )
                    continue
                }

                val byName =
                    findExerciseRow(
                        db,
                        "normalized_name = ?",
                        arrayOf(
                            normalized
                        )
                    )

                if (byName != null) {
                    if (
                        !exerciseProfilesAreReconcilable(
                            byName,
                            recording,
                            tracking,
                            dataFields,
                        ) ||
                        !catalogEquipmentProfilesAreCompatible(
                            db,
                            byName.exerciseId,
                            exerciseId,
                        )
                    ) {
                        traceDecision(
                            "normalized_name:${byName.exerciseId}",
                            "conflict",
                        )
                        return PcCatalogImportResult.Invalid(
                            "Conflit de nom/profil entre ${byName.exerciseId} et $exerciseId."
                        )
                    }

                    /* WHY: re-keying the existing Android row preserves every
                     * completed/draft FK directly.  Only the stable catalogue
                     * identity and its string-keyed equipment metadata move. */
                    mergeCatalogEquipmentIdentity(
                        db,
                        byName.exerciseId,
                        exerciseId,
                    )
                    val values = ContentValues().apply {
                        put("exercise_id", exerciseId)
                        put("name", name.trim())
                        put("normalized_name", normalized)
                        put("data_fields", byName.dataFields or dataFields)
                    }
                    if (
                        db.update(
                            "exercises",
                            values,
                            "id = ?",
                            arrayOf(byName.rowId.toString()),
                        ) != 1
                    ) {
                        return PcCatalogImportResult.DatabaseError
                    }

                    reconciled += 1
                    traceDecision(
                        "normalized_name:${byName.exerciseId}",
                        "existing-reconciled",
                    )
                    continue
                }

                val values =
                    ContentValues().apply {
                        put(
                            "exercise_id",
                            exerciseId
                        )

                        put(
                            "name",
                            name
                        )

                        put(
                            "normalized_name",
                            normalized
                        )

                        put(
                            "recording_mode",
                            recording.wireValue
                        )

                        put(
                            "tracking_mode",
                            tracking.wireValue
                        )

                        put(
                            "data_fields",
                            dataFields
                        )
                    }

                db.insertOrThrow(
                    "exercises",
                    null,
                    values
                )

                imported += 1
                traceDecision("none", "inserted")
            }

            db.setTransactionSuccessful()

            PcCatalogImportResult.Applied(
                imported =
                    imported,
                reconciled =
                    reconciled,
                skipped =
                    skipped,
            )
        } catch (
            error: Exception
        ) {
            PcCatalogImportResult.DatabaseError
        } finally {
            db.endTransaction()
        }
    }

    private data class ExerciseRow(
        val rowId: Long,
        val exerciseId: String,
        val name: String,
        val normalizedName: String,
        val recordingMode: RecordingMode,
        val trackingMode: TrackingMode,
        val dataFields: Int,
    )

    private fun exerciseProfilesAreReconcilable(
        row: ExerciseRow,
        recordingMode: RecordingMode,
        trackingMode: TrackingMode,
        dataFields: Int,
    ): Boolean =
        row.recordingMode == recordingMode &&
            row.trackingMode == trackingMode &&
            (
                dataFields and row.dataFields.inv() == 0 ||
                    row.dataFields and dataFields.inv() == 0
            )

    private fun catalogEquipmentProfilesAreCompatible(
        db: SQLiteDatabase,
        retiredExerciseId: String,
        canonicalExerciseId: String,
    ): Boolean =
        db.rawQuery(
            "SELECT 1 FROM catalog_exercise_equipment retired " +
                "JOIN catalog_exercise_equipment canonical " +
                "ON canonical.equipment_row_id=retired.equipment_row_id " +
                "WHERE retired.exercise_id=? AND canonical.exercise_id=? " +
                "AND retired.load_semantics<>canonical.load_semantics LIMIT 1;",
            arrayOf(retiredExerciseId, canonicalExerciseId),
        ).use { !it.moveToFirst() }

    private fun mergeCatalogEquipmentIdentity(
        db: SQLiteDatabase,
        retiredExerciseId: String,
        canonicalExerciseId: String,
    ) {
        /* INVARIANT: equal equipment relationships coalesce; a differing load
         * semantic was rejected before this helper is called. */
        db.execSQL(
            "INSERT OR IGNORE INTO catalog_exercise_equipment(" +
                "exercise_id,equipment_row_id,load_semantics) " +
                "SELECT ?,equipment_row_id,load_semantics " +
                "FROM catalog_exercise_equipment WHERE exercise_id=?;",
            arrayOf(canonicalExerciseId, retiredExerciseId),
        )
        db.execSQL(
            "DELETE FROM catalog_exercise_equipment WHERE exercise_id=?;",
            arrayOf(retiredExerciseId),
        )
    }

    private fun mergeExerciseRows(
        db: SQLiteDatabase,
        canonical: ExerciseRow,
        retired: ExerciseRow,
    ) {
        db.execSQL(
            "UPDATE session_exercises SET exercise_row_id=? WHERE exercise_row_id=?;",
            arrayOf(canonical.rowId, retired.rowId),
        )
        db.execSQL(
            "UPDATE draft_session_exercises SET exercise_row_id=? WHERE exercise_row_id=?;",
            arrayOf(canonical.rowId, retired.rowId),
        )
        db.execSQL(
            "UPDATE active_session_draft SET selected_exercise_row_id=? " +
                "WHERE selected_exercise_row_id=?;",
            arrayOf(canonical.rowId, retired.rowId),
        )
        db.execSQL(
            "INSERT OR IGNORE INTO exercise_equipment(exercise_row_id,equipment_row_id) " +
                "SELECT ?,equipment_row_id FROM exercise_equipment WHERE exercise_row_id=?;",
            arrayOf(canonical.rowId, retired.rowId),
        )
        db.execSQL(
            "DELETE FROM exercise_equipment WHERE exercise_row_id=?;",
            arrayOf(retired.rowId),
        )
        mergeCatalogEquipmentIdentity(
            db,
            retired.exerciseId,
            canonical.exerciseId,
        )
        db.execSQL(
            "DELETE FROM exercises WHERE id=?;",
            arrayOf(retired.rowId),
        )
    }

    private fun findExerciseRow(
        db: SQLiteDatabase,
        selection: String,
        arguments: Array<String>,
    ): ExerciseRow? {
        db.query(
            "exercises",
            arrayOf(
                "id",
                "exercise_id",
                "name",
                "normalized_name",
                "recording_mode",
                "tracking_mode",
                "data_fields",
            ),
            selection,
            arguments,
            null,
            null,
            null,
        ).use {
            cursor ->
                if (
                    !cursor.moveToFirst()
                ) {
                    return null
                }

                return ExerciseRow(
                    rowId =
                        cursor.getLong(0),
                    exerciseId = cursor.getString(1),
                    name = cursor.getString(2),
                    normalizedName = cursor.getString(3),
                    recordingMode =
                        if (
                            cursor.getString(4) ==
                            "continuous"
                        ) {
                            RecordingMode.CONTINUOUS
                        } else {
                            RecordingMode.SETS
                        },
                    trackingMode =
                        if (
                            cursor.getString(5) ==
                            "duration"
                        ) {
                            TrackingMode.DURATION
                        } else {
                            TrackingMode.REPS
                        },
                    dataFields =
                        cursor.getInt(6),
                )
        }
    }

    private fun lookupExerciseRowIdOrNull(
        db: SQLiteDatabase,
        exerciseId: String,
    ): Long? =
        db.query(
            "exercises",
            arrayOf("id"),
            "exercise_id = ?",
            arrayOf(exerciseId),
            null,
            null,
            null,
        ).use { cursor ->
            if (cursor.moveToFirst()) cursor.getLong(0) else null
        }

    private fun lookupEquipmentRowIdOrNull(
        db: SQLiteDatabase,
        equipmentId: String?,
    ): Long? {
        if (equipmentId == null) {
            return null
        }
        return db.query(
            "equipment",
            arrayOf("id"),
            "equipment_id = ?",
            arrayOf(equipmentId),
            null,
            null,
            null,
        ).use { cursor ->
            if (cursor.moveToFirst()) cursor.getLong(0) else null
        }
    }

    private fun exerciseHasReferences(
        db: SQLiteDatabase,
        exerciseRowId: Long,
    ): Boolean {
        /* INVARIANT: both completed and active-draft records own a catalog-row
         * reference.  Profile mutation is admitted only while neither exists. */
        return db.rawQuery(
            """
            SELECT EXISTS(
                SELECT 1 FROM session_exercises WHERE exercise_row_id = ?
                UNION ALL
                SELECT 1 FROM draft_session_exercises WHERE exercise_row_id = ?
            );
            """.trimIndent(),
            arrayOf(exerciseRowId.toString(), exerciseRowId.toString()),
        ).use { cursor ->
            cursor.moveToFirst() && cursor.getInt(0) != 0
        }
    }

    fun buildMobileExportJson(): String {
        val root = JSONObject()
        root.put("format", "trainlog-mobile-export")
        root.put("version", 1)
        root.put("generated_at", OffsetDateTime.now().toString())

        val exerciseArray = JSONArray()
        listExercises().forEach { exercise ->
            exerciseArray.put(
                JSONObject()
                    .put("exercise_id", exercise.exerciseId)
                    .put("name", exercise.name)
                    .put("recording_mode", exercise.recordingMode.wireValue)
                    .put("tracking_mode", exercise.trackingMode.wireValue)
                    .put("data_fields", exercise.dataFields)
            )
        }
        root.put("exercises", exerciseArray)

        val db = database.readableDatabase
        val sessionArray = JSONArray()
        /* CONTRACT: only completed `sessions` are part of mobile export v1;
         * active draft tables are intentionally outside the frozen artifact. */
        db.rawQuery(
            "SELECT id, session_id, started_at, session_type FROM sessions ORDER BY started_at ASC, id ASC;",
            null,
        ).use { sessions ->
            while (sessions.moveToNext()) {
                val sessionRowId = sessions.getLong(0)
                val session = JSONObject()
                    .put("session_id", sessions.getString(1))
                    .put("started_at", sessions.getString(2))
                    .put("session_type", sessions.getString(3))
                val sessionExercises = JSONArray()
                db.rawQuery(
                    "SELECT se.id, e.exercise_id, e.name, se.recording_mode, se.tracking_mode, se.data_fields " +
                        "FROM session_exercises AS se JOIN exercises AS e ON e.id = se.exercise_row_id " +
                        "WHERE se.session_row_id = ? ORDER BY se.position ASC;",
                    arrayOf(sessionRowId.toString()),
                ).use { exerciseCursor ->
                    while (exerciseCursor.moveToNext()) {
                        val sessionExerciseRowId = exerciseCursor.getLong(0)
                        val recording = exerciseCursor.getString(3)
                        val tracking = exerciseCursor.getString(4)
                        val item = JSONObject()
                            .put("exercise_id", exerciseCursor.getString(1))
                            .put("name", exerciseCursor.getString(2))
                            .put("recording_mode", recording)
                            .put("tracking_mode", tracking)
                            .put("data_fields", exerciseCursor.getInt(5))
                            .put("load_mode", "none")
                            .put("rest_seconds", 0)
                        if (recording == "continuous") {
                            db.query(
                                "continuous_activity",
                                arrayOf("duration_seconds", "speed_kmh", "distance_km"),
                                "session_exercise_row_id = ?",
                                arrayOf(sessionExerciseRowId.toString()),
                                null, null, null,
                            ).use { continuous ->
                                if (!continuous.moveToFirst()) {
                                    error("Missing continuous activity")
                                }
                                val payload = JSONObject()
                                    .put("duration_seconds", continuous.getInt(0))
                                if (!continuous.isNull(1)) {
                                    payload.put("speed_kmh", continuous.getDouble(1))
                                }
                                if (!continuous.isNull(2)) {
                                    payload.put("distance_km", continuous.getDouble(2))
                                }
                                item.put("continuous", payload)
                            }
                        } else {
                            val sets = JSONArray()
                            db.query(
                                "performed_sets",
                                arrayOf("reps", "duration_seconds"),
                                "session_exercise_row_id = ?",
                                arrayOf(sessionExerciseRowId.toString()),
                                null, null, "position ASC",
                            ).use { setCursor ->
                                while (setCursor.moveToNext()) {
                                    val set = JSONObject()
                                    if (tracking == "reps") {
                                        set.put("reps", setCursor.getInt(0))
                                    } else {
                                        set.put("duration_seconds", setCursor.getInt(1))
                                    }
                                    sets.put(set)
                                }
                            }
                            item.put("sets", sets)
                        }
                        sessionExercises.put(item)
                    }
                }
                session.put("exercises", sessionExercises)
                sessionArray.put(session)
            }
        }
        root.put("sessions", sessionArray)

        val bodyArray = JSONArray()
        db.rawQuery(
            "SELECT observation_id, observed_at, body_weight_kg, neck_cm, shoulders_cm, chest_cm, waist_cm, hips_cm, " +
                "left_arm_cm, right_arm_cm, left_forearm_cm, right_forearm_cm, left_thigh_cm, right_thigh_cm, left_calf_cm, right_calf_cm " +
                "FROM body_observations ORDER BY observed_at ASC, id ASC;",
            null,
        ).use { cursor ->
            val names = arrayOf(
                "body_weight_kg", "neck_cm", "shoulders_cm", "chest_cm", "waist_cm", "hips_cm",
                "left_arm_cm", "right_arm_cm", "left_forearm_cm", "right_forearm_cm",
                "left_thigh_cm", "right_thigh_cm", "left_calf_cm", "right_calf_cm",
            )
            while (cursor.moveToNext()) {
                val item = JSONObject()
                    .put("observation_id", cursor.getString(0))
                    .put("observed_at", cursor.getString(1))
                for (index in names.indices) {
                    val column = index + 2
                    if (!cursor.isNull(column)) {
                        item.put(names[index], cursor.getDouble(column))
                    }
                }
                bodyArray.put(item)
            }
        }
        root.put("body_observations", bodyArray)
        return root.toString()
    }

    /**
     * V2 is a new artifact, never a reinterpretation of frozen V1.  Each
     * session occurrence carries its durable Android entry_id, ordering,
     * equipment and actual load so repeated catalogue exercises round-trip.
     */
    fun buildMobileExportV2Json(): String {
        val root = JSONObject(buildMobileExportJson())
        root.put("version", 2)
        val db = database.readableDatabase
        val sessions = root.getJSONArray("sessions")
        for (sessionIndex in 0 until sessions.length()) {
            val session = sessions.getJSONObject(sessionIndex)
            val items = session.getJSONArray("exercises")
            db.rawQuery(
                "SELECT se.id,se.entry_id,se.position,eq.equipment_id FROM session_exercises se " +
                    "JOIN sessions s ON s.id=se.session_row_id LEFT JOIN equipment eq ON eq.id=se.equipment_row_id " +
                    "WHERE s.session_id=? ORDER BY se.position ASC;",
                arrayOf(session.getString("session_id")),
            ).use { entries ->
                var itemIndex = 0
                while (entries.moveToNext()) {
                    val item = items.getJSONObject(itemIndex++)
                    val rowId = entries.getLong(0)
                    item.put("entry_id", entries.getString(1))
                    item.put("position", entries.getInt(2))
                    if (entries.isNull(3)) item.put("equipment_id", JSONObject.NULL)
                    else item.put("equipment_id", entries.getString(3))
                    if (item.has("sets")) {
                        val sets = item.getJSONArray("sets")
                        db.query("performed_sets", arrayOf("weight_kg"),
                            "session_exercise_row_id=?", arrayOf(rowId.toString()), null, null, "position ASC").use { cursor ->
                            var setIndex = 0
                            while (cursor.moveToNext()) {
                                if (!cursor.isNull(0)) sets.getJSONObject(setIndex).put("weight_kg", cursor.getDouble(0))
                                setIndex++
                            }
                        }
                    }
                }
            }
        }
        return root.toString()
    }

    /** Apply the same V2 session artifact emitted by desktop, keyed by entry_id. */
    fun applyPcMobileExportV2Json(json: String): MobileSessionImportResult {
        val root = try { JSONObject(json) } catch (_: Exception) {
            return MobileSessionImportResult.Invalid("Snapshot séances JSON invalide.")
        }
        validatePcMobileExportV2(root)?.let { return MobileSessionImportResult.Invalid(it) }
        val sessions = root.getJSONArray("sessions")
        val db = database.writableDatabase
        var sessionsAdded = 0
        var sessionsSkipped = 0
        var bodyObservationsAdded = 0
        var bodyObservationsSkipped = 0
        return try {
            db.beginTransaction()
            for (i in 0 until sessions.length()) {
                val session = sessions.getJSONObject(i)
                val sessionId = session.optString("session_id")
                val startedAt = session.optString("started_at")
                val type = session.optString("session_type")
                val entries = session.optJSONArray("exercises")
                if (sessionId.isBlank() || startedAt.isBlank() || type !in setOf("training", "max_test") || entries == null) {
                    return MobileSessionImportResult.Invalid("Session V2 invalide.")
                }
                for (index in 0 until entries.length()) {
                    val entry = entries.getJSONObject(index)
                    val exerciseId = entry.optString("exercise_id")
                    val exerciseRow = findExerciseRow(db, "exercise_id = ?", arrayOf(exerciseId))
                        ?: return MobileSessionImportResult.Invalid("Exercice V2 inconnu : $exerciseId")
                    if (
                        entry.optString("recording_mode") != exerciseRow.recordingMode.wireValue ||
                        entry.optString("tracking_mode") != exerciseRow.trackingMode.wireValue ||
                        entry.optInt("data_fields", -1) and
                            exerciseRow.dataFields.inv() != 0
                    ) {
                        return MobileSessionImportResult.Invalid("Profil V2 incompatible : $exerciseId")
                    }
                }
                val existingRowId = db.rawQuery("SELECT id FROM sessions WHERE session_id=?", arrayOf(sessionId)).use {
                    if (it.moveToFirst()) it.getLong(0) else null
                }
                if (existingRowId != null) {
                    if (pcSessionV2Matches(db, existingRowId, session)) {
                        sessionsSkipped += 1
                        continue
                    }
                    return MobileSessionImportResult.Invalid("Conflit de contenu pour la séance $sessionId")
                }
                val rowId = run {
                    val values = ContentValues().apply { put("session_id", sessionId); put("started_at", startedAt); put("session_type", type) }
                    db.insertOrThrow("sessions", null, values)
                }
                val seen = mutableSetOf<String>()
                for (index in 0 until entries.length()) {
                    val entry = entries.getJSONObject(index)
                    val entryId = entry.optString("entry_id")
                    val exerciseId = entry.optString("exercise_id")
                    val position = entry.optInt("position", -1)
                    if (entryId.isBlank() || !seen.add(entryId) || exerciseId.isBlank() || position < 0) {
                        return MobileSessionImportResult.Invalid("Identité d'entrée V2 invalide.")
                    }
                    val exerciseRow = findExerciseRow(db, "exercise_id = ?", arrayOf(exerciseId))
                        ?: return MobileSessionImportResult.Invalid("Exercice V2 inconnu : $exerciseId")
                    val recording = entry.optString("recording_mode")
                    val tracking = entry.optString("tracking_mode")
                    if (recording !in setOf("sets", "continuous") || tracking !in setOf("reps", "duration")) {
                        return MobileSessionImportResult.Invalid("Profil V2 invalide.")
                    }
                    val equipmentId = if (entry.isNull("equipment_id")) null else entry.optString("equipment_id")
                    val equipmentRowId = if (equipmentId == null) null else lookupEquipmentRowIdOrNull(db, equipmentId)
                    if (equipmentId != null && (equipmentId.isBlank() || equipmentRowId == null)) {
                        return MobileSessionImportResult.Invalid("Équipement V2 inconnu : $equipmentId")
                    }
                    val values = ContentValues().apply {
                        put("entry_id", entryId); put("session_row_id", rowId); put("exercise_row_id", exerciseRow.rowId)
                        put("position", position); put("recording_mode", recording); put("tracking_mode", tracking)
                        put("data_fields", entry.optInt("data_fields", 0))
                        if (equipmentRowId == null) putNull("equipment_row_id") else put("equipment_row_id", equipmentRowId)
                    }
                    val occurrence = db.insertOrThrow("session_exercises", null, values)
                    if (recording == "continuous") {
                        val c = entry.optJSONObject("continuous") ?: return MobileSessionImportResult.Invalid("Activité continue manquante.")
                        db.insertOrThrow("continuous_activity", null, ContentValues().apply {
                            put("session_exercise_row_id", occurrence); put("duration_seconds", c.optInt("duration_seconds", 0))
                            if (c.has("speed_kmh")) put("speed_kmh", c.getDouble("speed_kmh")); if (c.has("distance_km")) put("distance_km", c.getDouble("distance_km"))
                        })
                    } else {
                        val sets = entry.optJSONArray("sets") ?: return MobileSessionImportResult.Invalid("Séries manquantes.")
                        for (setIndex in 0 until sets.length()) {
                            val set = sets.getJSONObject(setIndex)
                            db.insertOrThrow("performed_sets", null, ContentValues().apply {
                                put("session_exercise_row_id", occurrence); put("position", setIndex)
                                if (tracking == "reps") put("reps", set.optInt("reps", -1)) else put("duration_seconds", set.optInt("duration_seconds", 0))
                                if (set.has("weight_kg")) put("weight_kg", set.getDouble("weight_kg"))
                            })
                        }
                    }
                }
                sessionsAdded += 1
            }
            val body = root.optJSONArray("body_observations")
                ?: return MobileSessionImportResult.Invalid("Mesures corporelles manquantes.")
            val metrics = arrayOf("body_weight_kg", "neck_cm", "shoulders_cm", "chest_cm", "waist_cm", "hips_cm",
                "left_arm_cm", "right_arm_cm", "left_forearm_cm", "right_forearm_cm", "left_thigh_cm", "right_thigh_cm",
                "left_calf_cm", "right_calf_cm")
            for (index in 0 until body.length()) {
                val item = body.getJSONObject(index)
                val id = item.optString("observation_id")
                val observedAt = item.optString("observed_at")
                if (id.isBlank() || observedAt.isBlank() || metrics.none { item.has(it) }) {
                    return MobileSessionImportResult.Invalid("Observation corporelle V2 invalide.")
                }
                val existing = db.rawQuery(
                    "SELECT observed_at,${metrics.joinToString(",")} FROM body_observations WHERE observation_id=?",
                    arrayOf(id)).use { cursor ->
                    if (!cursor.moveToFirst()) null else List(1 + metrics.size) { column ->
                        if (cursor.isNull(column)) null else if (column == 0) cursor.getString(column) else cursor.getDouble(column)
                    }
                }
                val expected: List<Any?> = listOf(observedAt) + metrics.map { metric ->
                    if (item.has(metric)) item.getDouble(metric) else null
                }
                if (existing != null) {
                    if (existing != expected) return MobileSessionImportResult.Invalid("Conflit observation corporelle : $id")
                    bodyObservationsSkipped += 1
                    continue
                }
                db.insertOrThrow("body_observations", null, ContentValues().apply {
                    put("observation_id", id); put("observed_at", observedAt)
                    metrics.forEach { metric -> if (item.has(metric)) put(metric, item.getDouble(metric)) }
                })
                bodyObservationsAdded += 1
            }
            db.setTransactionSuccessful()
            MobileSessionImportResult.Applied(
                sessionsAdded = sessionsAdded,
                sessionsSkipped = sessionsSkipped,
                bodyObservationsAdded = bodyObservationsAdded,
                bodyObservationsSkipped = bodyObservationsSkipped,
            )
        } catch (_: Exception) { MobileSessionImportResult.DatabaseError
        } finally { db.endTransaction() }
    }

    private fun validatePcMobileExportV2(root: JSONObject): String? {
        val rootKeys = setOf("format", "version", "generated_at", "exercises", "sessions", "body_observations")
        if (!root.hasExactKeys(rootKeys) || root.value("format") != "trainlog-mobile-export" ||
            !root.value("version").isJsonInt(2, 2) || !root.value("generated_at").isNonemptyJsonString() ||
            root.value("exercises") !is JSONArray || root.value("sessions") !is JSONArray ||
            root.value("body_observations") !is JSONArray) {
            return "Snapshot séances V2 invalide."
        }
        return try {
            val exerciseIds = mutableSetOf<String>()
            val exerciseProfiles = mutableMapOf<String, Triple<String, String, Int>>()
            val exerciseKeys = setOf("exercise_id", "name", "recording_mode", "tracking_mode", "data_fields")
            val exercises = root.getJSONArray("exercises")
            for (index in 0 until exercises.length()) {
                val item = exercises.opt(index) as? JSONObject ?: return "Catalogue exercices V2 invalide."
                val id = item.value("exercise_id")
                if (!item.hasExactKeys(exerciseKeys) || !id.isNonemptyJsonString() || !exerciseIds.add(id as String) ||
                    !item.value("name").isNonemptyJsonString() || !validJsonProfile(item)) {
                    return "Catalogue exercices V2 invalide."
                }
                exerciseProfiles[id] = Triple(
                    item.getString("recording_mode"),
                    item.getString("tracking_mode"),
                    item.getInt("data_fields"),
                )
            }

            val sessionKeys = setOf("session_id", "started_at", "session_type", "exercises")
            val entryBaseKeys = setOf("exercise_id", "name", "recording_mode", "tracking_mode", "data_fields",
                "load_mode", "rest_seconds", "entry_id", "position", "equipment_id")
            val sessionIds = mutableSetOf<String>()
            val sessions = root.getJSONArray("sessions")
            for (sessionIndex in 0 until sessions.length()) {
                val session = sessions.opt(sessionIndex) as? JSONObject ?: return "Session V2 invalide."
                val sessionId = session.value("session_id")
                val entries = session.value("exercises") as? JSONArray
                if (!session.hasExactKeys(sessionKeys) || !sessionId.isNonemptyJsonString() ||
                    !sessionIds.add(sessionId as String) || !session.value("started_at").isNonemptyJsonString() ||
                    session.value("session_type") !in setOf("training", "max_test") || entries == null || entries.length() == 0) {
                    return "Session V2 invalide."
                }
                val entryIds = mutableSetOf<String>()
                val positions = mutableSetOf<Int>()
                for (entryIndex in 0 until entries.length()) {
                    val entry = entries.opt(entryIndex) as? JSONObject ?: return "Entrée de séance V2 invalide."
                    val recording = entry.value("recording_mode")
                    val tracking = entry.value("tracking_mode")
                    val expectedKeys = entryBaseKeys + if (recording == "continuous") setOf("continuous") else setOf("sets")
                    val entryId = entry.value("entry_id")
                    val exerciseId = entry.value("exercise_id")
                    val positionValue = entry.value("position")
                    if (!entry.hasExactKeys(expectedKeys) || !entryId.isNonemptyJsonString() || !entryIds.add(entryId as String) ||
                        !exerciseId.isNonemptyJsonString() || exerciseId !in exerciseIds || !entry.value("name").isNonemptyJsonString() ||
                        !validJsonProfile(entry) || entry.value("load_mode") != "none" || !entry.value("rest_seconds").isJsonInt(0, 0) ||
                        !positionValue.isJsonInt(0, 100000) || !positions.add((positionValue as Number).toInt()) ||
                        !(entry.value("equipment_id") === JSONObject.NULL || entry.value("equipment_id").isNonemptyJsonString())) {
                        return "Entrée de séance V2 invalide."
                    }
                    val catalogProfile = exerciseProfiles[exerciseId]
                        ?: return "Exercice de séance V2 absent du catalogue."
                    val entryFields = entry.getInt("data_fields")
                    if (
                        entry.getString("recording_mode") != catalogProfile.first ||
                        entry.getString("tracking_mode") != catalogProfile.second ||
                        entryFields and catalogProfile.third.inv() != 0
                    ) {
                        /* CONTRACT: richer current catalog metadata must not
                         * rewrite an older occurrence snapshot. */
                        return "Profil historique V2 incompatible avec le catalogue."
                    }
                    if (recording == "continuous") {
                        val continuous = entry.value("continuous") as? JSONObject ?: return "Activité continue V2 invalide."
                        val allowed = setOf("duration_seconds", "speed_kmh", "distance_km")
                        val fields = (entry.value("data_fields") as Number).toInt()
                        if (!continuous.hasOnlyKeys(allowed, setOf("duration_seconds")) ||
                            !continuous.value("duration_seconds").isJsonInt(1, 86400) ||
                            continuous.has("speed_kmh") != (fields and 1 != 0) ||
                            continuous.has("distance_km") != (fields and 2 != 0) ||
                            (continuous.has("speed_kmh") && !continuous.value("speed_kmh").isPositiveJsonNumber()) ||
                            (continuous.has("distance_km") && !continuous.value("distance_km").isPositiveJsonNumber())) {
                            return "Activité continue V2 invalide."
                        }
                    } else {
                        val sets = entry.value("sets") as? JSONArray ?: return "Séries V2 invalides."
                        if (sets.length() == 0) return "Séries V2 invalides."
                        for (setIndex in 0 until sets.length()) {
                            val set = sets.opt(setIndex) as? JSONObject ?: return "Série V2 invalide."
                            val valueKey = if (tracking == "reps") "reps" else "duration_seconds"
                            val minimum = if (tracking == "reps") 0 else 1
                            val maximum = if (tracking == "reps") 10000 else 86400
                            if (!set.hasOnlyKeys(setOf(valueKey, "weight_kg"), setOf(valueKey)) ||
                                !set.value(valueKey).isJsonInt(minimum, maximum) ||
                                (set.has("weight_kg") && !set.value("weight_kg").isPositiveJsonNumber())) {
                                return "Série V2 invalide."
                            }
                        }
                    }
                }
            }

            val metricKeys = setOf("body_weight_kg", "neck_cm", "shoulders_cm", "chest_cm", "waist_cm", "hips_cm",
                "left_arm_cm", "right_arm_cm", "left_forearm_cm", "right_forearm_cm", "left_thigh_cm", "right_thigh_cm",
                "left_calf_cm", "right_calf_cm")
            val bodyIds = mutableSetOf<String>()
            val body = root.getJSONArray("body_observations")
            for (index in 0 until body.length()) {
                val item = body.opt(index) as? JSONObject ?: return "Observation corporelle V2 invalide."
                val id = item.value("observation_id")
                val presentMetrics = item.keys().asSequence().toSet() intersect metricKeys
                if (!item.hasOnlyKeys(metricKeys + setOf("observation_id", "observed_at"), setOf("observation_id", "observed_at")) ||
                    !id.isNonemptyJsonString() || !bodyIds.add(id as String) || !item.value("observed_at").isNonemptyJsonString() ||
                    presentMetrics.isEmpty() || presentMetrics.any { !item.value(it).isPositiveJsonNumber() }) {
                    return "Observation corporelle V2 invalide."
                }
            }
            null
        } catch (_: Exception) {
            "Snapshot séances V2 invalide."
        }
    }

    private fun validJsonProfile(item: JSONObject): Boolean {
        val recording = item.value("recording_mode")
        val tracking = item.value("tracking_mode")
        val dataFields = item.value("data_fields")
        return recording in setOf("sets", "continuous") &&
            tracking in setOf("reps", "duration") &&
            !(recording == "continuous" && tracking != "duration") &&
            dataFields.isJsonInt(0, ExerciseDataFields.KNOWN_MASK) &&
            !(recording == "sets" && (dataFields as Number).toInt() != 0)
    }

    private fun JSONObject.value(key: String): Any? = if (has(key)) get(key) else null
    private fun JSONObject.hasExactKeys(expected: Set<String>): Boolean = keys().asSequence().toSet() == expected
    private fun JSONObject.hasOnlyKeys(allowed: Set<String>, required: Set<String>): Boolean {
        val actual = keys().asSequence().toSet()
        return actual.all { it in allowed } && required.all { it in actual }
    }
    private fun Any?.isNonemptyJsonString(): Boolean = this is String && isNotEmpty()
    private fun Any?.isJsonInt(minimum: Int, maximum: Int): Boolean {
        if (this !is Number) return false
        val number = toDouble()
        return number.isFinite() && number % 1.0 == 0.0 && number >= minimum && number <= maximum
    }
    private fun Any?.isPositiveJsonNumber(): Boolean = this is Number && toDouble().isFinite() && toDouble() > 0.0

    private fun pcSessionV2Matches(db: SQLiteDatabase, rowId: Long, session: JSONObject): Boolean {
        val headerMatches = db.rawQuery("SELECT started_at,session_type FROM sessions WHERE id=?", arrayOf(rowId.toString())).use {
            it.moveToFirst() && it.getString(0) == session.optString("started_at") && it.getString(1) == session.optString("session_type")
        }
        if (!headerMatches) return false
        val incoming = session.optJSONArray("exercises") ?: return false
        val rows = mutableListOf<Long>()
        val metadata = mutableListOf<List<Any?>>()
        db.rawQuery(
            "SELECT se.id,se.entry_id,se.position,e.exercise_id,se.recording_mode,se.tracking_mode,se.data_fields,eq.equipment_id " +
                "FROM session_exercises se JOIN exercises e ON e.id=se.exercise_row_id LEFT JOIN equipment eq ON eq.id=se.equipment_row_id " +
                "WHERE se.session_row_id=? ORDER BY se.position", arrayOf(rowId.toString())).use { cursor ->
            while (cursor.moveToNext()) {
                rows += cursor.getLong(0)
                metadata += listOf(cursor.getString(1), cursor.getInt(2), cursor.getString(3), cursor.getString(4),
                    cursor.getString(5), cursor.getInt(6), if (cursor.isNull(7)) null else cursor.getString(7))
            }
        }
        if (rows.size != incoming.length()) return false
        for (index in rows.indices) {
            val item = incoming.getJSONObject(index)
            val expected = listOf(item.optString("entry_id"), item.optInt("position", -1), item.optString("exercise_id"),
                item.optString("recording_mode"), item.optString("tracking_mode"), item.optInt("data_fields", -1),
                if (item.isNull("equipment_id")) null else item.optString("equipment_id"))
            if (metadata[index] != expected) return false
            if (item.optString("recording_mode") == "continuous") {
                val value = item.optJSONObject("continuous") ?: return false
                val current = db.rawQuery("SELECT duration_seconds,speed_kmh,distance_km FROM continuous_activity WHERE session_exercise_row_id=?",
                    arrayOf(rows[index].toString())).use { cursor ->
                    if (!cursor.moveToFirst()) null else listOf(cursor.getInt(0), if (cursor.isNull(1)) null else cursor.getDouble(1), if (cursor.isNull(2)) null else cursor.getDouble(2))
                }
                if (current != listOf(value.optInt("duration_seconds", -1), value.optDoubleOrNull("speed_kmh"), value.optDoubleOrNull("distance_km"))) return false
            } else {
                val sets = item.optJSONArray("sets") ?: return false
                val current = mutableListOf<List<Any?>>()
                db.rawQuery("SELECT reps,duration_seconds,weight_kg FROM performed_sets WHERE session_exercise_row_id=? ORDER BY position",
                    arrayOf(rows[index].toString())).use { cursor -> while (cursor.moveToNext()) current += listOf(
                    if (cursor.isNull(0)) null else cursor.getInt(0), if (cursor.isNull(1)) null else cursor.getInt(1),
                    if (cursor.isNull(2)) null else cursor.getDouble(2)) }
                val wanted = (0 until sets.length()).map { setIndex -> sets.getJSONObject(setIndex).let { set ->
                    listOf(if (set.has("reps")) set.getInt("reps") else null,
                        if (set.has("duration_seconds")) set.getInt("duration_seconds") else null,
                        set.optDoubleOrNull("weight_kg")) } }
                if (current != wanted) return false
            }
        }
        return true
    }

    /**
     * CONTRACT: this companion artifact is deliberately outside frozen mobile
     * export v1. Version 2 keys an occurrence by `(session_id, entry_id)`;
     * `exercise_id` is corroborating catalogue identity, never an occurrence
     * key. `cleared` is intentional state, unlike an absent artifact which
     * conveys no equipment signal.
     */
    fun buildEquipmentAssociationsJson(): String {
        val root = JSONObject()
            .put("format", "trainlog-equipment-associations")
            .put("version", 2)
            .put("generated_at", OffsetDateTime.now().toString())
        val associations = JSONArray()
        database.readableDatabase.rawQuery(
            "SELECT s.session_id, se.entry_id, e.exercise_id, eq.equipment_id FROM session_exercises se " +
                "JOIN sessions s ON s.id = se.session_row_id " +
                "JOIN exercises e ON e.id = se.exercise_row_id " +
                "LEFT JOIN equipment eq ON eq.id = se.equipment_row_id " +
                "ORDER BY s.started_at ASC, s.id ASC, se.position ASC;",
            null,
        ).use { cursor ->
            while (cursor.moveToNext()) {
                val item = JSONObject()
                    .put("session_id", cursor.getString(0))
                    .put("entry_id", cursor.getString(1))
                    .put("exercise_id", cursor.getString(2))
                if (cursor.isNull(3)) item.put("state", "cleared") else {
                    item.put("state", "set")
                    item.put("equipment_id", cursor.getString(3))
                }
                associations.put(item)
            }
        }
        return root.put("associations", associations).toString()
    }

    /** Snapshot only user-created definitions; bundled manifest rows are never
     * exported as mutable data and absence never requests deletion. */
    fun buildEquipmentDefinitionsJson(): String {
        val supplied = EquipmentCatalog.load(applicationContext).map { it.equipmentId }.toSet()
        val items = JSONArray()
        listEquipment().filter { it.equipmentId !in supplied }.forEach { entry ->
            /* A companion is authoritative metadata, never a repair for an
             * arbitrary session reference.  Refuse corrupt persisted custom
             * rows before publication so desktop never receives a partial
             * definition snapshot. */
            check(
                entry.equipmentId.isNotBlank() &&
                    entry.displayName.isNotBlank() &&
                    entry.displayName.length <= 120 &&
                    entry.type.isNotBlank()
            ) {
                "Définition équipement personnalisée invalide : ${entry.equipmentId}"
            }
            items.put(JSONObject()
                .put("equipment_id", entry.equipmentId)
                .put("display_name", entry.displayName)
                .put("label_name", entry.labelName)
                .put("equipment_type", entry.type)
                .put("load_semantics", entry.loadSemantics.name.lowercase(Locale.ROOT)))
        }
        return JSONObject()
            .put("format", "trainlog-equipment-definitions")
            .put("version", 1)
            .put("generated_at", OffsetDateTime.now().toString())
            .put("equipment", items).toString()
    }

    fun applyPcEquipmentDefinitionsJson(json: String): EquipmentDefinitionImportResult {
        val root = try { JSONObject(json) } catch (_: Exception) {
            return EquipmentDefinitionImportResult.Invalid("Définitions équipement JSON invalides.")
        }
        if (root.value("format") != "trainlog-equipment-definitions" || !root.value("version").isJsonInt(1, 1) ||
            root.keys().asSequence().toSet() != setOf("format", "version", "generated_at", "equipment") ||
            !root.value("generated_at").isNonemptyJsonString()) {
            return EquipmentDefinitionImportResult.Invalid("Définitions équipement non supportées.")
        }
        val items = root.optJSONArray("equipment")
            ?: return EquipmentDefinitionImportResult.Invalid("Tableau equipment manquant.")
        val supplied = EquipmentCatalog.load(applicationContext).map { it.equipmentId }.toSet()
        val parsed = mutableListOf<EquipmentCatalogEntry>()
        val seen = mutableSetOf<String>()
        try {
            for (index in 0 until items.length()) {
                val item = items.getJSONObject(index)
                if (item.keys().asSequence().toSet() != setOf("equipment_id", "display_name", "label_name", "equipment_type", "load_semantics")) {
                    return EquipmentDefinitionImportResult.Invalid("Définition équipement invalide.")
                }
                val fields = listOf(
                    "equipment_id",
                    "display_name",
                    "label_name",
                    "equipment_type",
                    "load_semantics",
                )
                if (fields.any { item.value(it) !is String }) {
                    return EquipmentDefinitionImportResult.Invalid("Définition équipement invalide.")
                }
                val id = item.value("equipment_id") as String
                val display = item.value("display_name") as String
                val label = item.value("label_name") as String
                val type = item.value("equipment_type") as String
                val semanticsValue = item.value("load_semantics") as String
                if (semanticsValue !in setOf("none", "external", "assistance")) {
                    return EquipmentDefinitionImportResult.Invalid("Sémantique équipement invalide.")
                }
                val semantics = try { EquipmentLoadSemantics.valueOf(semanticsValue.uppercase(Locale.ROOT)) }
                    catch (_: Exception) { return EquipmentDefinitionImportResult.Invalid("Sémantique équipement invalide.") }
                if (id.isBlank() || !seen.add(id) || id in supplied || display.isBlank() || display.length > 120 || type.isBlank()) {
                    return EquipmentDefinitionImportResult.Invalid("Identité équipement invalide ou réservée : $id")
                }
                parsed += EquipmentCatalogEntry(id, label, display, emptyList(), type, semantics)
            }
        } catch (_: Exception) { return EquipmentDefinitionImportResult.Invalid("Définition équipement invalide.") }

        val db = database.writableDatabase
        return try {
            var imported = 0
            var skipped = 0
            db.beginTransaction()
            for (entry in parsed) {
                val existing = db.rawQuery(
                    "SELECT label_name,display_name,equipment_type,load_semantics FROM equipment WHERE equipment_id=?",
                    arrayOf(entry.equipmentId)).use { cursor ->
                    if (cursor.moveToFirst()) listOf(cursor.getString(0), cursor.getString(1), cursor.getString(2), cursor.getString(3)) else null
                }
                val expected = listOf(entry.labelName, entry.displayName, entry.type, entry.loadSemantics.name.lowercase(Locale.ROOT))
                if (existing != null && existing != expected) {
                    return EquipmentDefinitionImportResult.Invalid("Conflit définition équipement : ${entry.equipmentId}")
                }
                if (existing != null) { skipped++; continue }
                db.insertOrThrow("equipment", null, ContentValues().apply {
                    put("equipment_id", entry.equipmentId); put("label_name", entry.labelName)
                    put("display_name", entry.displayName); put("equipment_type", entry.type)
                    put("load_semantics", entry.loadSemantics.name.lowercase(Locale.ROOT))
                })
                imported++
            }
            db.setTransactionSuccessful()
            EquipmentDefinitionImportResult.Applied(imported, skipped)
        } catch (_: Exception) { EquipmentDefinitionImportResult.DatabaseError
        } finally { db.endTransaction() }
    }

    fun applyPcEquipmentAssociationsJson(json: String): EquipmentAssociationImportResult {
        val root = try { JSONObject(json) } catch (_: Exception) {
            return EquipmentAssociationImportResult.Invalid("Extension équipement JSON invalide.")
        }
        val versionValue = root.value("version")
        if (!root.hasExactKeys(setOf("format", "version", "generated_at", "associations")) ||
            root.value("format") != "trainlog-equipment-associations" ||
            !versionValue.isJsonInt(1, 2) || !root.value("generated_at").isNonemptyJsonString()) {
            return EquipmentAssociationImportResult.Invalid("Extension équipement non supportée.")
        }
        val version = (versionValue as Number).toInt()
        val items = root.optJSONArray("associations")
            ?: return EquipmentAssociationImportResult.Invalid("Associations équipement manquantes.")
        val known = listEquipment().map { it.equipmentId }.toSet()
        val validatedIdentities = mutableSetOf<String>()
        try {
            for (index in 0 until items.length()) {
                val item = items.opt(index) as? JSONObject
                    ?: return EquipmentAssociationImportResult.Invalid("Association équipement invalide.")
                val sessionId = item.value("session_id")
                val exerciseId = item.value("exercise_id")
                val entryId = if (version == 2) item.value("entry_id") else null
                val state = item.value("state")
                val required = mutableSetOf("session_id", "exercise_id", "state")
                if (version == 2) required += "entry_id"
                if (state == "set") required += "equipment_id"
                if (!item.hasExactKeys(required) || !sessionId.isNonemptyJsonString() ||
                    !exerciseId.isNonemptyJsonString() || (version == 2 && !entryId.isNonemptyJsonString()) ||
                    state !in setOf("set", "cleared")) {
                    return EquipmentAssociationImportResult.Invalid("Association équipement invalide.")
                }
                val identity = "${sessionId as String}\u0000${if (version == 2) entryId as String else exerciseId as String}"
                if (!validatedIdentities.add(identity)) {
                    return EquipmentAssociationImportResult.Invalid("Association équipement dupliquée.")
                }
                if (state == "set") {
                    val equipmentId = item.value("equipment_id")
                    if (!equipmentId.isNonemptyJsonString() || equipmentId !in known) {
                        return EquipmentAssociationImportResult.Invalid("Équipement inconnu : $equipmentId")
                    }
                }
            }
        } catch (_: Exception) {
            return EquipmentAssociationImportResult.Invalid("Association équipement invalide.")
        }
        val db = database.writableDatabase
        val seen = mutableSetOf<String>()
        db.beginTransaction()
        try {
            for (index in 0 until items.length()) {
                val item = items.getJSONObject(index)
                val sessionId = item.optString("session_id")
                val exerciseId = item.optString("exercise_id")
                val entryId = if (version == 2) item.optString("entry_id") else null
                val state = item.optString("state")
                val identity = "$sessionId\u0000${entryId ?: exerciseId}"
                if (sessionId.isBlank() || exerciseId.isBlank() || !seen.add(identity) || state !in setOf("set", "cleared") || (version == 2 && entryId.isNullOrBlank())) {
                    return EquipmentAssociationImportResult.Invalid("Association équipement invalide.")
                }
                val equipmentId = if (state == "set") item.optString("equipment_id") else null
                if (state == "set" && (equipmentId.isNullOrBlank() || equipmentId !in known)) {
                    return EquipmentAssociationImportResult.Invalid("Équipement inconnu : $equipmentId")
                }
                val row = db.rawQuery(
                    if (version == 2) {
                        "SELECT se.id,e.exercise_id,eq.equipment_id FROM session_exercises se JOIN sessions s ON s.id=se.session_row_id JOIN exercises e ON e.id=se.exercise_row_id LEFT JOIN equipment eq ON eq.id=se.equipment_row_id WHERE s.session_id=? AND se.entry_id=?;"
                    } else {
                        "SELECT se.id,eq.equipment_id FROM session_exercises se JOIN sessions s ON s.id=se.session_row_id JOIN exercises e ON e.id=se.exercise_row_id LEFT JOIN equipment eq ON eq.id=se.equipment_row_id WHERE s.session_id=? AND e.exercise_id=?;"
                    },
                    if (version == 2) arrayOf(sessionId, entryId) else arrayOf(sessionId, exerciseId),
                ).use { cursor ->
                    val first = if (cursor.moveToFirst()) {
                        if (version == 2) {
                            Triple(cursor.getLong(0), cursor.getString(1), if (cursor.isNull(2)) null else cursor.getString(2))
                        } else {
                            Triple(cursor.getLong(0), exerciseId, if (cursor.isNull(1)) null else cursor.getString(1))
                        }
                    } else null
                    if (version == 1 && first != null && cursor.moveToNext()) null else first
                }
                    ?: return EquipmentAssociationImportResult.Invalid("Entrée de séance inconnue : $sessionId/$exerciseId")
                if (row.second != exerciseId) {
                    return EquipmentAssociationImportResult.Invalid("Conflit exercice association : $sessionId/${entryId ?: exerciseId}")
                }
                if (row.third != equipmentId) {
                    return EquipmentAssociationImportResult.Invalid("Conflit association équipement : $sessionId/${entryId ?: exerciseId}")
                }
                /* Equal state is an idempotent replay; association snapshots
                 * never overwrite divergent local edits. */
                continue
            }
            db.setTransactionSuccessful()
            return EquipmentAssociationImportResult.Applied(0)
        } catch (_: Exception) {
            return EquipmentAssociationImportResult.DatabaseError
        } finally { db.endTransaction() }
    }

    fun saveBodyObservation(
        draft: BodyObservationDraft,
    ): SaveBodyObservationResult {
        if (
            !draft.hasAnyMetric() ||
            !draft.allValuesPositive()
        ) {
            return SaveBodyObservationResult.Invalid
        }

        val observationId =
            "bo_" +
                UUID.randomUUID()
                    .toString()

        val observedAt =
            OffsetDateTime.now()
                .toString()

        val values =
            ContentValues().apply {
                put(
                    "observation_id",
                    observationId
                )

                put(
                    "observed_at",
                    observedAt
                )

                putOptionalDouble(
                    "body_weight_kg",
                    draft.bodyWeightKg
                )

                putOptionalDouble(
                    "neck_cm",
                    draft.neckCm
                )

                putOptionalDouble(
                    "shoulders_cm",
                    draft.shouldersCm
                )

                putOptionalDouble(
                    "chest_cm",
                    draft.chestCm
                )

                putOptionalDouble(
                    "waist_cm",
                    draft.waistCm
                )

                putOptionalDouble(
                    "hips_cm",
                    draft.hipsCm
                )

                putOptionalDouble(
                    "left_arm_cm",
                    draft.leftArmCm
                )

                putOptionalDouble(
                    "right_arm_cm",
                    draft.rightArmCm
                )

                putOptionalDouble(
                    "left_forearm_cm",
                    draft.leftForearmCm
                )

                putOptionalDouble(
                    "right_forearm_cm",
                    draft.rightForearmCm
                )

                putOptionalDouble(
                    "left_thigh_cm",
                    draft.leftThighCm
                )

                putOptionalDouble(
                    "right_thigh_cm",
                    draft.rightThighCm
                )

                putOptionalDouble(
                    "left_calf_cm",
                    draft.leftCalfCm
                )

                putOptionalDouble(
                    "right_calf_cm",
                    draft.rightCalfCm
                )
            }

        return try {
            database.writableDatabase
                .insertOrThrow(
                    "body_observations",
                    null,
                    values
                )

            SaveBodyObservationResult.Saved(
                observationId
            )
        } catch (
            error: Exception
        ) {
            SaveBodyObservationResult.DatabaseError
        }
    }

    fun listBodyObservations(
        limit: Int = 20,
    ): List<BodyObservationSummary> {
        if (limit <= 0) {
            return emptyList()
        }

        val output =
            mutableListOf<
                BodyObservationSummary
            >()

        database.readableDatabase.rawQuery(
            """
            SELECT
                observation_id,
                observed_at,
                body_weight_kg,
                (
                    (body_weight_kg IS NOT NULL) +
                    (neck_cm IS NOT NULL) +
                    (shoulders_cm IS NOT NULL) +
                    (chest_cm IS NOT NULL) +
                    (waist_cm IS NOT NULL) +
                    (hips_cm IS NOT NULL) +
                    (left_arm_cm IS NOT NULL) +
                    (right_arm_cm IS NOT NULL) +
                    (left_forearm_cm IS NOT NULL) +
                    (right_forearm_cm IS NOT NULL) +
                    (left_thigh_cm IS NOT NULL) +
                    (right_thigh_cm IS NOT NULL) +
                    (left_calf_cm IS NOT NULL) +
                    (right_calf_cm IS NOT NULL)
                ) AS metric_count
            FROM body_observations
            ORDER BY observed_at DESC, id DESC
            LIMIT ?;
            """.trimIndent(),
            arrayOf(
                limit.toString()
            ),
        ).use { cursor ->
            while (
                cursor.moveToNext()
            ) {
                output +=
                    BodyObservationSummary(
                        observationId =
                            cursor.getString(0),
                        observedAt =
                            cursor.getString(1),
                        bodyWeightKg =
                            if (
                                cursor.isNull(2)
                            ) {
                                null
                            } else {
                                cursor.getDouble(2)
                            },
                        metricCount =
                            cursor.getInt(3),
                    )
            }
        }

        return output
    }

    fun getSessionDetail(
        sessionId: String,
    ): SessionDetail? {
        val db =
            database.readableDatabase

        val summary =
            db.rawQuery(
                """
                SELECT
                    s.session_id,
                    s.started_at,
                    s.session_type,
                    COUNT(se.id)
                FROM sessions AS s
                LEFT JOIN session_exercises AS se
                    ON se.session_row_id = s.id
                WHERE s.session_id = ?
                GROUP BY s.id;
                """.trimIndent(),
                arrayOf(sessionId),
            ).use { cursor ->
                if (!cursor.moveToFirst()) {
                    null
                } else {
                    SessionSummary(
                        sessionId =
                            cursor.getString(0),
                        startedAt =
                            cursor.getString(1),
                        exerciseCount =
                            cursor.getInt(3),
                        sessionType =
                            SessionType.fromWire(
                                cursor.getString(2)
                            ),
                    )
                }
            } ?: return null

        val exercises =
            mutableListOf<
                SessionExerciseDetail
            >()

        db.rawQuery(
            """
            SELECT
                se.id,
                se.entry_id,
                e.exercise_id,
                e.name,
                se.recording_mode,
                se.tracking_mode,
                se.data_fields,
                eq.display_name
            FROM session_exercises AS se
            JOIN sessions AS s
                ON s.id = se.session_row_id
            JOIN exercises AS e
                ON e.id = se.exercise_row_id
            LEFT JOIN equipment AS eq
                ON eq.id = se.equipment_row_id
            WHERE s.session_id = ?
            ORDER BY se.position ASC;
            """.trimIndent(),
            arrayOf(sessionId),
        ).use { cursor ->
            while (cursor.moveToNext()) {
                val sessionExerciseRowId =
                    cursor.getLong(0)

                val entryId = cursor.getString(1)
                val exerciseId = cursor.getString(2)
                val name = cursor.getString(3)

                val recording =
                    when (
                        cursor.getString(4)
                    ) {
                        "continuous" ->
                            RecordingMode.CONTINUOUS

                        else ->
                            RecordingMode.SETS
                    }

                val tracking =
                    when (
                        cursor.getString(5)
                    ) {
                        "duration" ->
                            TrackingMode.DURATION

                        else ->
                            TrackingMode.REPS
                    }

                val dataFields =
                    cursor.getInt(6)
                val equipmentDisplayName = if (cursor.isNull(7)) null else cursor.getString(7)

                if (
                    recording ==
                    RecordingMode.CONTINUOUS
                ) {
                    db.query(
                        "continuous_activity",
                        arrayOf(
                            "duration_seconds",
                            "speed_kmh",
                            "distance_km",
                        ),
                        "session_exercise_row_id = ?",
                        arrayOf(
                            sessionExerciseRowId
                                .toString()
                        ),
                        null,
                        null,
                        null,
                    ).use {
                            continuous ->

                        if (
                            !continuous
                                .moveToFirst()
                        ) {
                            error(
                                "Missing continuous activity"
                            )
                        }

                        exercises +=
                            SessionExerciseDetail(
                                entryId = entryId,
                                exerciseId = exerciseId,
                                exerciseName =
                                    name,
                                equipmentDisplayName = equipmentDisplayName,
                                recordingMode =
                                    recording,
                                trackingMode =
                                    tracking,
                                dataFields =
                                    dataFields,
                                continuousDurationSeconds =
                                    continuous
                                        .getInt(0),
                                speedKmh =
                                    if (
                                        continuous
                                            .isNull(1)
                                    ) {
                                        null
                                    } else {
                                        continuous
                                            .getDouble(1)
                                    },
                                distanceKm =
                                    if (
                                        continuous
                                            .isNull(2)
                                    ) {
                                        null
                                    } else {
                                        continuous
                                            .getDouble(2)
                                    },
                            )
                    }
                } else {
                    val sets =
                        mutableListOf<
                            SessionSetDraft
                        >()

                    db.query(
                        "performed_sets",
                        arrayOf(
                            "reps",
                            "duration_seconds",
                            "weight_kg",
                        ),
                        "session_exercise_row_id = ?",
                        arrayOf(
                            sessionExerciseRowId
                                .toString()
                        ),
                        null,
                        null,
                        "position ASC",
                    ).use { setCursor ->
                        while (
                            setCursor
                                .moveToNext()
                        ) {
                            sets +=
                                SessionSetDraft(
                                    reps =
                                        if (
                                            setCursor
                                                .isNull(0)
                                        ) {
                                            0
                                        } else {
                                            setCursor
                                                .getInt(0)
                                        },
                                    durationSeconds =
                                        if (
                                            setCursor
                                                .isNull(1)
                                        ) {
                                            0
                                        } else {
                                            setCursor
                                                .getInt(1)
                                        },
                                    weightKg = if (setCursor.isNull(2)) null else setCursor.getDouble(2),
                                )
                        }
                    }

                    exercises +=
                        SessionExerciseDetail(
                            entryId = entryId,
                            exerciseId = exerciseId,
                            exerciseName =
                                name,
                            equipmentDisplayName = equipmentDisplayName,
                            recordingMode =
                                recording,
                            trackingMode =
                                tracking,
                            dataFields =
                                dataFields,
                            sets = sets,
                        )
                }
            }
        }

        return SessionDetail(
            summary = summary,
            exercises = exercises,
        )
    }

    fun setCompletedSessionEquipment(
        sessionId: String,
        entryId: String,
        equipmentId: String?,
    ): Boolean {
        val db = database.writableDatabase
        val equipmentRowId = lookupEquipmentRowIdOrNull(db, equipmentId)
        if (equipmentId != null && equipmentRowId == null) return false
        return try {
            val values = ContentValues()
            if (equipmentRowId == null) values.putNull("equipment_row_id") else values.put("equipment_row_id", equipmentRowId)
            db.update(
                "session_exercises", values,
                "id = (SELECT se.id FROM session_exercises se JOIN sessions s ON s.id=se.session_row_id " +
                    "WHERE s.session_id=? AND se.entry_id=?)",
                arrayOf(sessionId, entryId),
            ) == 1
        } catch (_: Exception) { false }
    }

    private fun loadActiveSessionDraft(
        db: SQLiteDatabase,
    ): ActiveDraftRestore? {
        val header =
            db.rawQuery(
                """
                SELECT
                    d.session_type,
                    d.set_count_text,
                    d.reps_text,
                    d.duration_text,
                    d.speed_text,
                    d.distance_text,
                    d.updated_at,
                    d.selected_exercise_label,
                    d.selected_equipment_id,
                    e.exercise_id,
                    e.name,
                    e.normalized_name,
                    e.recording_mode,
                    e.tracking_mode,
                    e.data_fields
                FROM active_session_draft AS d
                LEFT JOIN exercises AS e
                    ON e.id = d.selected_exercise_row_id
                WHERE d.id = ?;
                """.trimIndent(),
                arrayOf(ACTIVE_DRAFT_ID.toString()),
            ).use { cursor ->
                if (!cursor.moveToFirst()) {
                    null
                } else {
                    val missingSelection =
                        cursor.isNull(9) &&
                            !cursor.isNull(7)
                    val selected =
                        if (cursor.isNull(9)) {
                            null
                        } else {
                            exerciseProfileFromCursor(
                                cursor,
                                9,
                            )
                        }

                    ActiveDraftHeader(
                        sessionType =
                            SessionType.fromWire(
                                cursor.getString(0)
                            ),
                        form = SessionDraftForm(
                            selectedExercise = selected,
                            selectedEquipmentId = if (cursor.isNull(8)) null else cursor.getString(8),
                            setCountText = cursor.getString(1),
                            repsText = cursor.getString(2),
                            durationText = cursor.getString(3),
                            speedText = cursor.getString(4),
                            distanceText = cursor.getString(5),
                        ),
                        updatedAt =
                            cursor.getString(6),
                        warning =
                            if (missingSelection) {
                                "L'exercice en cours de saisie n'existe plus ; " +
                                    "seule la sélection a été annulée. " +
                                    "La saisie partielle et les exercices ajoutés " +
                                    "sont conservés."
                            } else {
                                null
                            },
                    )
                }
            } ?: return null

        val exercises =
            mutableListOf<SessionExerciseDraft>()

        db.rawQuery(
            """
            SELECT
                de.id,
                e.exercise_id,
                e.name,
                e.normalized_name,
                de.recording_mode,
                de.tracking_mode,
                de.data_fields,
                eq.equipment_id,
                de.entry_id
            FROM draft_session_exercises AS de
            JOIN exercises AS e
                ON e.id = de.exercise_row_id
            LEFT JOIN equipment AS eq
                ON eq.id = de.equipment_row_id
            WHERE de.draft_id = ?
            ORDER BY de.position ASC;
            """.trimIndent(),
            arrayOf(ACTIVE_DRAFT_ID.toString()),
        ).use { cursor ->
            while (cursor.moveToNext()) {
                val rowId = cursor.getLong(0)
                val exercise =
                    ExerciseProfile(
                        exerciseId =
                            cursor.getString(1),
                        name = cursor.getString(2),
                        normalizedName =
                            cursor.getString(3),
                        recordingMode =
                            recordingModeFromWire(
                                cursor.getString(4)
                            ),
                        trackingMode =
                            trackingModeFromWire(
                                cursor.getString(5)
                            ),
                            dataFields =
                                cursor.getInt(6),
                    )
                val equipmentId = if (cursor.isNull(7)) null else cursor.getString(7)
                val entryId = cursor.getString(8)

                if (
                    exercise.recordingMode ==
                    RecordingMode.CONTINUOUS
                ) {
                    val continuous =
                        db.query(
                            "draft_continuous_activity",
                            arrayOf(
                                "duration_seconds",
                                "speed_kmh",
                                "distance_km",
                            ),
                            "draft_exercise_row_id = ?",
                            arrayOf(rowId.toString()),
                            null,
                            null,
                            null,
                        ).use { item ->
                            check(item.moveToFirst()) {
                                "Activité continue du brouillon manquante."
                            }

                            SessionExerciseDraft(
                                entryId = entryId,
                                exercise = exercise,
                                equipmentId = equipmentId,
                                continuousDurationSeconds =
                                    item.getInt(0),
                                speedKmh =
                                    if (item.isNull(1)) {
                                        null
                                    } else {
                                        item.getDouble(1)
                                    },
                                distanceKm =
                                    if (item.isNull(2)) {
                                        null
                                    } else {
                                        item.getDouble(2)
                                    },
                            )
                        }
                    exercises += continuous
                } else {
                    val sets =
                        mutableListOf<SessionSetDraft>()
                    db.query(
                        "draft_performed_sets",
                        arrayOf(
                            "reps",
                            "duration_seconds",
                            "weight_kg",
                        ),
                        "draft_exercise_row_id = ?",
                        arrayOf(rowId.toString()),
                        null,
                        null,
                        "position ASC",
                    ).use { setCursor ->
                        while (setCursor.moveToNext()) {
                            sets +=
                                SessionSetDraft(
                                    reps =
                                        if (setCursor.isNull(0)) {
                                            0
                                        } else {
                                            setCursor.getInt(0)
                                        },
                                    durationSeconds =
                                        if (setCursor.isNull(1)) {
                                            0
                                        } else {
                                            setCursor.getInt(1)
                                        },
                                    weightKg = if (setCursor.isNull(2)) null else setCursor.getDouble(2),
                                )
                        }
                    }
                    exercises +=
                        SessionExerciseDraft(
                            entryId = entryId,
                            exercise = exercise,
                            equipmentId = equipmentId,
                            sets = sets,
                        )
                }
            }
        }

        return ActiveDraftRestore(
            draft = ActiveSessionDraft(
                exercises = exercises,
                sessionType = header.sessionType,
                form = header.form,
                updatedAt = header.updatedAt,
            ),
            warning = header.warning,
        )
    }

    private fun persistActiveSessionDraft(
        db: SQLiteDatabase,
        draft: ActiveSessionDraft,
    ) {
        val selectedRowId =
            draft.form.selectedExercise
                ?.let {
                    lookupExerciseRowId(
                        db,
                        it.exerciseId,
                    )
                }
        val now = OffsetDateTime.now().toString()
        val values =
            ContentValues().apply {
                put("session_type", draft.sessionType.wireValue)
                putOptionalString("selected_equipment_id", draft.form.selectedEquipmentId)
                if (selectedRowId == null) {
                    putNull("selected_exercise_row_id")
                    putNull("selected_exercise_label")
                } else {
                    put("selected_exercise_row_id", selectedRowId)
                    put(
                        "selected_exercise_label",
                        draft.form.selectedExercise.name,
                    )
                }
                put("set_count_text", draft.form.setCountText)
                put("reps_text", draft.form.repsText)
                put("duration_text", draft.form.durationText)
                put("speed_text", draft.form.speedText)
                put("distance_text", draft.form.distanceText)
                put("updated_at", now)
            }

        val updated =
            db.update(
                "active_session_draft",
                values,
                "id = ?",
                arrayOf(ACTIVE_DRAFT_ID.toString()),
            )

        if (updated == 0) {
            values.put("id", ACTIVE_DRAFT_ID)
            db.insertOrThrow(
                "active_session_draft",
                null,
                values,
            )
        }

        db.delete(
            "draft_session_exercises",
            "draft_id = ?",
            arrayOf(ACTIVE_DRAFT_ID.toString()),
        )

        draft.exercises.forEachIndexed {
                index,
                exerciseDraft ->
            val exerciseRowId =
                lookupExerciseRowId(
                    db,
                    exerciseDraft.exercise.exerciseId,
                )
            val exerciseValues =
                ContentValues().apply {
                    put("draft_id", ACTIVE_DRAFT_ID)
                    put("exercise_row_id", exerciseRowId)
                    put("position", index)
                    put(
                        "recording_mode",
                        exerciseDraft.exercise
                            .recordingMode.wireValue,
                    )
                    put(
                        "tracking_mode",
                        exerciseDraft.exercise
                            .trackingMode.wireValue,
                    )
                    put(
                        "data_fields",
                        exerciseDraft.exercise.dataFields,
                    )
                    put("entry_id", exerciseDraft.entryId)
                    val equipmentRowId = lookupEquipmentRowIdOrNull(
                        db,
                        exerciseDraft.equipmentId,
                    )
                    check(exerciseDraft.equipmentId == null || equipmentRowId != null) {
                        "Unknown equipment: ${exerciseDraft.equipmentId}"
                    }
                    if (equipmentRowId == null) putNull("equipment_row_id") else put("equipment_row_id", equipmentRowId)
                }
            val draftExerciseRowId =
                db.insertOrThrow(
                    "draft_session_exercises",
                    null,
                    exerciseValues,
                )

            if (
                exerciseDraft.exercise.recordingMode ==
                RecordingMode.CONTINUOUS
            ) {
                val continuousValues =
                    ContentValues().apply {
                        put(
                            "draft_exercise_row_id",
                            draftExerciseRowId,
                        )
                        put(
                            "duration_seconds",
                            exerciseDraft.continuousDurationSeconds,
                        )
                        exerciseDraft.speedKmh?.let {
                            put("speed_kmh", it)
                        }
                        exerciseDraft.distanceKm?.let {
                            put("distance_km", it)
                        }
                    }
                db.insertOrThrow(
                    "draft_continuous_activity",
                    null,
                    continuousValues,
                )
            } else {
                exerciseDraft.sets.forEachIndexed {
                        setIndex,
                        set ->
                    val setValues =
                        ContentValues().apply {
                            put(
                                "draft_exercise_row_id",
                                draftExerciseRowId,
                            )
                            put("position", setIndex)
                            set.weightKg?.let { put("weight_kg", it) }
                            if (
                                exerciseDraft.exercise.trackingMode ==
                                TrackingMode.REPS
                            ) {
                                put("reps", set.reps)
                            } else {
                                put(
                                    "duration_seconds",
                                    set.durationSeconds,
                                )
                            }
                        }
                    db.insertOrThrow(
                        "draft_performed_sets",
                        null,
                        setValues,
                    )
                }
            }
        }
    }

    private fun insertCompletedSession(
        db: SQLiteDatabase,
        draft: SessionDraft,
    ): String {
        val sessionId =
            "se_" + UUID.randomUUID().toString()
        /* Preserve the existing Android meaning: started_at is assigned when
         * the completed session is saved, not when its draft is first opened. */
        val startedAt = OffsetDateTime.now().toString()
        val sessionValues =
            ContentValues().apply {
                put("session_id", sessionId)
                put("started_at", startedAt)
                put("session_type", draft.sessionType.wireValue)
            }
        val sessionRowId =
            db.insertOrThrow(
                "sessions",
                null,
                sessionValues,
            )

        draft.exercises.forEachIndexed {
                exerciseIndex,
                exerciseDraft ->
            val exerciseRowId =
                lookupExerciseRowId(
                    db,
                    exerciseDraft.exercise.exerciseId,
                )
            val exerciseValues =
                ContentValues().apply {
                    put("session_row_id", sessionRowId)
                    put("exercise_row_id", exerciseRowId)
                    put("position", exerciseIndex)
                    put(
                        "recording_mode",
                        exerciseDraft.exercise.recordingMode.wireValue,
                    )
                    put(
                        "tracking_mode",
                        exerciseDraft.exercise.trackingMode.wireValue,
                    )
                    put("data_fields", exerciseDraft.exercise.dataFields)
                    put("entry_id", exerciseDraft.entryId)
                    val equipmentRowId = lookupEquipmentRowIdOrNull(
                        db,
                        exerciseDraft.equipmentId,
                    )
                    check(exerciseDraft.equipmentId == null || equipmentRowId != null) {
                        "Unknown equipment: ${exerciseDraft.equipmentId}"
                    }
                    if (equipmentRowId == null) putNull("equipment_row_id") else put("equipment_row_id", equipmentRowId)
                }
            val sessionExerciseRowId =
                db.insertOrThrow(
                    "session_exercises",
                    null,
                    exerciseValues,
                )

            if (
                exerciseDraft.exercise.recordingMode ==
                RecordingMode.CONTINUOUS
            ) {
                val continuousValues =
                    ContentValues().apply {
                        put("session_exercise_row_id", sessionExerciseRowId)
                        put(
                            "duration_seconds",
                            exerciseDraft.continuousDurationSeconds,
                        )
                        exerciseDraft.speedKmh?.let { put("speed_kmh", it) }
                        exerciseDraft.distanceKm?.let { put("distance_km", it) }
                    }
                db.insertOrThrow(
                    "continuous_activity",
                    null,
                    continuousValues,
                )
            } else {
                exerciseDraft.sets.forEachIndexed {
                        setIndex,
                        set ->
                    val setValues =
                        ContentValues().apply {
                            put("session_exercise_row_id", sessionExerciseRowId)
                            put("position", setIndex)
                            set.weightKg?.let { put("weight_kg", it) }
                            if (
                                exerciseDraft.exercise.trackingMode ==
                                TrackingMode.REPS
                            ) {
                                put("reps", set.reps)
                            } else {
                                put("duration_seconds", set.durationSeconds)
                            }
                        }
                    db.insertOrThrow(
                        "performed_sets",
                        null,
                        setValues,
                    )
                }
            }
        }

        return sessionId
    }

    private fun exerciseProfileFromCursor(
        cursor: android.database.Cursor,
        offset: Int,
    ): ExerciseProfile =
        ExerciseProfile(
            exerciseId = cursor.getString(offset),
            name = cursor.getString(offset + 1),
            normalizedName = cursor.getString(offset + 2),
            recordingMode =
                recordingModeFromWire(
                    cursor.getString(offset + 3)
                ),
            trackingMode =
                trackingModeFromWire(
                    cursor.getString(offset + 4)
                ),
            dataFields = cursor.getInt(offset + 5),
        )

    private fun recordingModeFromWire(
        value: String,
    ): RecordingMode =
        if (value == "continuous") {
            RecordingMode.CONTINUOUS
        } else {
            RecordingMode.SETS
        }

    private fun trackingModeFromWire(
        value: String,
    ): TrackingMode =
        if (value == "duration") {
            TrackingMode.DURATION
        } else {
            TrackingMode.REPS
        }

    private data class ActiveDraftHeader(
        val sessionType: SessionType,
        val form: SessionDraftForm,
        val updatedAt: String,
        val warning: String?,
    )

    private data class ActiveDraftRestore(
        val draft: ActiveSessionDraft,
        val warning: String?,
    )

    private fun validateSessionExercise(
        draft: SessionExerciseDraft,
    ): Boolean {
        return when (
            draft.exercise.recordingMode
        ) {
            RecordingMode.CONTINUOUS -> {
                if (
                    draft.continuousDurationSeconds <= 0 ||
                    draft.sets.isNotEmpty()
                ) {
                    false
                } else {
                    val wantsSpeed =
                        draft.exercise.dataFields and
                            1 != 0

                    val wantsDistance =
                        draft.exercise.dataFields and
                            2 != 0

                    (
                        (wantsSpeed ==
                            (draft.speedKmh != null)) &&
                            (
                                !wantsSpeed ||
                                    draft.speedKmh!! > 0.0
                            ) &&
                            (wantsDistance ==
                                (draft.distanceKm != null)) &&
                            (
                                !wantsDistance ||
                                    draft.distanceKm!! > 0.0
                            )
                    )
                }
            }

            RecordingMode.SETS -> {
                if (
                    draft.sets.isEmpty() ||
                    draft.continuousDurationSeconds != 0 ||
                    draft.speedKmh != null ||
                    draft.distanceKm != null
                ) {
                    false
                } else {
                    when (
                        draft.exercise.trackingMode
                    ) {
                        TrackingMode.REPS ->
                            draft.sets.all {
                                it.reps >= 0 &&
                                    it.durationSeconds == 0
                            }

                        TrackingMode.DURATION ->
                            draft.sets.all {
                                it.durationSeconds > 0 &&
                                    it.reps == 0
                            }
                    }
                }
            }
        }
    }

    private fun lookupExerciseRowId(
        db: SQLiteDatabase,
        exerciseId: String,
    ): Long {
        db.query(
            "exercises",
            arrayOf("id"),
            "exercise_id = ?",
            arrayOf(exerciseId),
            null,
            null,
            null,
        ).use { cursor ->
            if (!cursor.moveToFirst()) {
                error(
                    "Exercise not found: $exerciseId"
                )
            }

            return cursor.getLong(0)
        }
    }

    private fun normalizeName(
        value: String,
    ): String {
        val decomposed =
            Normalizer.normalize(
                value.trim()
                    .lowercase(
                        Locale.ROOT
                    ),
                Normalizer.Form.NFD,
            )

        return decomposed
            .replace(
                Regex("\\p{Mn}+"),
                "",
            )
            .replace(
                Regex("\\s+"),
                " ",
            )
            .trim()
    }
}


private fun ContentValues.putOptionalDouble(
    key: String,
    value: Double?,
) {
    if (value == null) {
        putNull(key)
    } else {
        put(key, value)
    }
}

private fun ContentValues.putOptionalString(
    key: String,
    value: String?,
) {
    if (value == null) putNull(key) else put(key, value)
}

private fun JSONObject.optDoubleOrNull(key: String): Double? =
    if (has(key) && !isNull(key)) getDouble(key) else null

private fun equipmentAliasNormalize(value: String): String =
    Normalizer.normalize(value, Normalizer.Form.NFD)
        .replace("\\p{M}+".toRegex(), "")
        .lowercase(Locale.ROOT)
        .trim()

private const val ANDROID_DATABASE_NAME =
    "trainlog-android.db"
private const val ACTIVE_DRAFT_ID = 1
private const val MAX_DRAFT_FORM_TEXT_LENGTH = 4096
private const val ANDROID_LEG_PRESS_LEGACY_ID =
    "ex_d68a1af1-7247-4fb3-a48b-da8516906a29"
private const val DESKTOP_LEG_PRESS_CANONICAL_ID =
    "ex_b432623f-bfe9-4daf-a653-60ec7fdffbde"

private class TrainlogDatabaseHelper(
    private val appContext: Context,
    databaseName: String,
) : SQLiteOpenHelper(
            appContext,
    databaseName,
    null,
    8,
) {
    override fun onConfigure(
        db: SQLiteDatabase,
    ) {
        super.onConfigure(db)

        db.setForeignKeyConstraintsEnabled(
            true
        )
    }

    override fun onOpen(
        db: SQLiteDatabase,
    ) {
        super.onOpen(db)
        migrateApprovedLegPressIdentity(db)
    }

    override fun onCreate(
        db: SQLiteDatabase,
    ) {
        createExerciseTable(db)
        createSessionTables(db)
        createBodyTable(db)
        createActiveDraftTables(db)
        createEquipmentTables(db)
        seedEquipment(db)
    }

    override fun onUpgrade(
        db: SQLiteDatabase,
        oldVersion: Int,
        newVersion: Int,
    ) {
        var version = oldVersion

        if (version < 2 && newVersion >= 2) {
            createSessionTables(db)
            version = 2
        }

        if (version < 3 && newVersion >= 3) {
            createBodyTable(db)
            version = 3
        }

        if (version < 4 && newVersion >= 4) {
            /* CONTRACT: v4 is additive. Existing catalog, completed sessions,
             * performed values, and body observations remain untouched. */
            createActiveDraftTables(db)
            version = 4
        }

        if (version < 5 && newVersion >= 5) {
            /* CONTRACT: v5 adds only canonical equipment metadata. Existing
             * exercises, historical rows, and drafts are never rewritten. */
            createEquipmentTables(db)
            seedEquipment(db)
            addEquipmentReferenceColumns(db)
            version = 5
        }

        if (version < 6 && newVersion >= 6) {
            /* v6 keeps historic sets intact while allowing optional per-set load. */
            db.execSQL("ALTER TABLE performed_sets ADD COLUMN weight_kg REAL CHECK(weight_kg >= 0.0);")
            db.execSQL("ALTER TABLE draft_performed_sets ADD COLUMN weight_kg REAL CHECK(weight_kg >= 0.0);")
            version = 6
        }

        if (version < 7 && newVersion >= 7) {
            /* WHY: exercise_id is a catalogue identity, not an occurrence identity.
             * Historic rows get the deterministic same cross-device legacy key. */
            db.execSQL("ALTER TABLE session_exercises ADD COLUMN entry_id TEXT;")
            db.execSQL("UPDATE session_exercises SET entry_id = 'sxe_legacy_' || (SELECT session_id FROM sessions WHERE sessions.id = session_exercises.session_row_id) || '_' || (SELECT exercise_id FROM exercises WHERE exercises.id = session_exercises.exercise_row_id);")
            db.execSQL("CREATE UNIQUE INDEX session_exercises_entry_id_v7 ON session_exercises(entry_id);")
            db.execSQL("ALTER TABLE draft_performed_sets RENAME TO draft_performed_sets_v6;")
            db.execSQL("ALTER TABLE draft_continuous_activity RENAME TO draft_continuous_activity_v6;")
            db.execSQL("ALTER TABLE draft_session_exercises RENAME TO draft_session_exercises_v6;")
            createActiveDraftTables(db)
            db.execSQL("INSERT INTO draft_session_exercises(id,draft_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields,equipment_row_id,entry_id) SELECT id,draft_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields,equipment_row_id,'sxe_draft_legacy_' || id FROM draft_session_exercises_v6;")
            db.execSQL("INSERT INTO draft_performed_sets(id,draft_exercise_row_id,position,reps,duration_seconds,weight_kg) SELECT id,draft_exercise_row_id,position,reps,duration_seconds,weight_kg FROM draft_performed_sets_v6;")
            db.execSQL("INSERT INTO draft_continuous_activity(id,draft_exercise_row_id,duration_seconds,speed_kmh,distance_km) SELECT id,draft_exercise_row_id,duration_seconds,speed_kmh,distance_km FROM draft_continuous_activity_v6;")
            db.execSQL("DROP TABLE draft_performed_sets_v6;")
            db.execSQL("DROP TABLE draft_continuous_activity_v6;")
            db.execSQL("DROP TABLE draft_session_exercises_v6;")
            version = 7
        }

        if (version < 8 && newVersion >= 8) {
            /* CONTRACT: desktop custom definitions may explicitly carry
             * load_semantics=none. Rebuild the complete equipment reference
             * graph with stable row IDs so history and the active draft keep
             * pointing at exactly the same equipment records. */
            migrateEquipmentLoadSemanticsToVersionEight(db)
            version = 8
        }

        if (version != newVersion) {
            error(
                "Unsupported Android DB upgrade " +
                    "$oldVersion -> $newVersion"
            )
        }
    }

    private data class ExerciseIdentityRow(
        val rowId: Long,
        val exerciseId: String,
        val normalizedName: String,
        val recordingMode: String,
        val trackingMode: String,
        val dataFields: Int,
    )

    private fun exerciseIdentityRow(
        db: SQLiteDatabase,
        exerciseId: String,
    ): ExerciseIdentityRow? = db.rawQuery(
        "SELECT id,exercise_id,normalized_name,recording_mode,tracking_mode,data_fields " +
            "FROM exercises WHERE exercise_id=?;",
        arrayOf(exerciseId),
    ).use { cursor ->
        if (!cursor.moveToFirst()) null else ExerciseIdentityRow(
            rowId = cursor.getLong(0),
            exerciseId = cursor.getString(1),
            normalizedName = cursor.getString(2),
            recordingMode = cursor.getString(3),
            trackingMode = cursor.getString(4),
            dataFields = cursor.getInt(5),
        )
    }

    private fun isApprovedLegPressProfile(
        row: ExerciseIdentityRow,
    ): Boolean = row.normalizedName == "leg press" &&
        row.recordingMode == "sets" &&
        row.trackingMode == "reps" &&
        row.dataFields == 0

    /**
     * WHY: the user explicitly designated this one Android-created historic
     * identity as the desktop Leg press identity.  This is intentionally not
     * a generic normalized-name reconciliation: every other different-ID name
     * collision remains an import conflict.
     *
     * INVARIANT: row-ID references are retained whenever the canonical row is
     * absent, preserving completed/draft entries, sets, loads and equipment.
     * If both rows exist, every known reference is moved transactionally before
     * the legacy row is deleted, and incompatible catalog equipment metadata
     * aborts the transaction rather than being silently chosen.
     */
    private fun migrateApprovedLegPressIdentity(
        db: SQLiteDatabase,
    ) {
        val legacy = exerciseIdentityRow(db, ANDROID_LEG_PRESS_LEGACY_ID)
            ?: return /* Already migrated or this database never held it. */
        val canonical = exerciseIdentityRow(db, DESKTOP_LEG_PRESS_CANONICAL_ID)

        check(isApprovedLegPressProfile(legacy)) {
            "Profil Leg press Android inattendu; migration refusée."
        }
        if (canonical != null) {
            check(isApprovedLegPressProfile(canonical) &&
                canonical.normalizedName == legacy.normalizedName &&
                canonical.recordingMode == legacy.recordingMode &&
                canonical.trackingMode == legacy.trackingMode &&
                canonical.dataFields == legacy.dataFields) {
                "Profil Leg press desktop incompatible; migration refusée."
            }
        }

        db.beginTransaction()
        try {
            if (canonical == null) {
                /* Keep the legacy exercise row itself: all foreign-key graph
                 * members therefore retain their row IDs and occurrence IDs. */
                db.execSQL(
                    "UPDATE catalog_exercise_equipment SET exercise_id=? WHERE exercise_id=?;",
                    arrayOf(DESKTOP_LEG_PRESS_CANONICAL_ID, ANDROID_LEG_PRESS_LEGACY_ID),
                )
                db.execSQL(
                    "UPDATE exercises SET exercise_id=? WHERE id=?;",
                    arrayOf<Any>(DESKTOP_LEG_PRESS_CANONICAL_ID, legacy.rowId),
                )
            } else {
                val incompatibleCatalogEquipment = db.rawQuery(
                    "SELECT 1 FROM catalog_exercise_equipment legacy " +
                        "JOIN catalog_exercise_equipment canonical " +
                        "ON canonical.equipment_row_id=legacy.equipment_row_id " +
                        "WHERE legacy.exercise_id=? AND canonical.exercise_id=? " +
                        "AND legacy.load_semantics<>canonical.load_semantics LIMIT 1;",
                    arrayOf(ANDROID_LEG_PRESS_LEGACY_ID, DESKTOP_LEG_PRESS_CANONICAL_ID),
                ).use { it.moveToFirst() }
                check(!incompatibleCatalogEquipment) {
                    "Métadonnées équipement Leg press incompatibles; migration refusée."
                }

                db.execSQL(
                    "UPDATE session_exercises SET exercise_row_id=? WHERE exercise_row_id=?;",
                    arrayOf(canonical.rowId, legacy.rowId),
                )
                db.execSQL(
                    "UPDATE draft_session_exercises SET exercise_row_id=? WHERE exercise_row_id=?;",
                    arrayOf(canonical.rowId, legacy.rowId),
                )
                db.execSQL(
                    "UPDATE active_session_draft SET selected_exercise_row_id=? WHERE selected_exercise_row_id=?;",
                    arrayOf(canonical.rowId, legacy.rowId),
                )
                db.execSQL(
                    "INSERT OR IGNORE INTO exercise_equipment(exercise_row_id,equipment_row_id) " +
                        "SELECT ?,equipment_row_id FROM exercise_equipment WHERE exercise_row_id=?;",
                    arrayOf(canonical.rowId, legacy.rowId),
                )
                db.execSQL("DELETE FROM exercise_equipment WHERE exercise_row_id=?;", arrayOf(legacy.rowId))
                db.execSQL(
                    "INSERT OR IGNORE INTO catalog_exercise_equipment(exercise_id,equipment_row_id,load_semantics) " +
                        "SELECT ?,equipment_row_id,load_semantics FROM catalog_exercise_equipment WHERE exercise_id=?;",
                    arrayOf(DESKTOP_LEG_PRESS_CANONICAL_ID, ANDROID_LEG_PRESS_LEGACY_ID),
                )
                db.execSQL(
                    "DELETE FROM catalog_exercise_equipment WHERE exercise_id=?;",
                    arrayOf(ANDROID_LEG_PRESS_LEGACY_ID),
                )
                db.execSQL("DELETE FROM exercises WHERE id=?;", arrayOf(legacy.rowId))
            }

            check(exerciseIdentityRow(db, ANDROID_LEG_PRESS_LEGACY_ID) == null) {
                "Référence Leg press Android résiduelle après migration."
            }
            check(exerciseIdentityRow(db, DESKTOP_LEG_PRESS_CANONICAL_ID) != null) {
                "Identité Leg press desktop absente après migration."
            }
            db.setTransactionSuccessful()
        } finally {
            db.endTransaction()
        }
    }

    private fun createExerciseTable(
        db: SQLiteDatabase,
    ) {
        db.execSQL(
            """
            CREATE TABLE exercises(
                id INTEGER PRIMARY KEY,
                exercise_id TEXT NOT NULL UNIQUE,
                name TEXT NOT NULL,
                normalized_name TEXT NOT NULL UNIQUE,
                recording_mode TEXT NOT NULL
                    CHECK(
                        recording_mode IN (
                            'sets',
                            'continuous'
                        )
                    ),
                tracking_mode TEXT NOT NULL
                    CHECK(
                        tracking_mode IN (
                            'reps',
                            'duration'
                        )
                    ),
                data_fields INTEGER NOT NULL
                    DEFAULT 0
                    CHECK(
                        data_fields >= 0 AND
                        (data_fields & ~3) = 0
                    ),
                CHECK(
                    recording_mode !=
                        'continuous' OR
                    tracking_mode =
                        'duration'
                ),
                CHECK(
                    recording_mode !=
                        'sets' OR
                    data_fields = 0
                )
            );
            """.trimIndent()
        )
    }

    private fun createSessionTables(
        db: SQLiteDatabase,
    ) {
        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS sessions(
                id INTEGER PRIMARY KEY,
                session_id TEXT NOT NULL UNIQUE,
                started_at TEXT NOT NULL,
                session_type TEXT NOT NULL
                    CHECK(
                        session_type IN (
                            'training',
                            'max_test'
                        )
                    )
            );
            """.trimIndent()
        )

        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS session_exercises(
                id INTEGER PRIMARY KEY,
                session_row_id INTEGER NOT NULL
                    REFERENCES sessions(id)
                    ON DELETE CASCADE,
                exercise_row_id INTEGER NOT NULL
                    REFERENCES exercises(id)
                    ON DELETE RESTRICT,
                position INTEGER NOT NULL
                    CHECK(position >= 0),
                recording_mode TEXT NOT NULL
                    CHECK(
                        recording_mode IN (
                            'sets',
                            'continuous'
                        )
                    ),
                tracking_mode TEXT NOT NULL
                    CHECK(
                        tracking_mode IN (
                            'reps',
                            'duration'
                        )
                    ),
                data_fields INTEGER NOT NULL
                    CHECK(
                        data_fields >= 0 AND
                        (data_fields & ~3) = 0
                    ),
                equipment_row_id INTEGER
                    REFERENCES equipment(id)
                    ON DELETE SET NULL,
                entry_id TEXT NOT NULL UNIQUE,
                UNIQUE(
                    session_row_id,
                    position
                )
            );
            """.trimIndent()
        )

        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS performed_sets(
                id INTEGER PRIMARY KEY,
                session_exercise_row_id INTEGER NOT NULL
                    REFERENCES session_exercises(id)
                    ON DELETE CASCADE,
                position INTEGER NOT NULL
                    CHECK(position >= 0),
                reps INTEGER
                    CHECK(reps >= 0),
                duration_seconds INTEGER
                    CHECK(duration_seconds > 0),
                weight_kg REAL
                    CHECK(weight_kg >= 0.0),
                CHECK(
                    (
                        reps IS NOT NULL AND
                        duration_seconds IS NULL
                    ) OR (
                        reps IS NULL AND
                        duration_seconds IS NOT NULL
                    )
                ),
                UNIQUE(
                    session_exercise_row_id,
                    position
                )
            );
            """.trimIndent()
        )

        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS continuous_activity(
                id INTEGER PRIMARY KEY,
                session_exercise_row_id INTEGER NOT NULL UNIQUE
                    REFERENCES session_exercises(id)
                    ON DELETE CASCADE,
                duration_seconds INTEGER NOT NULL
                    CHECK(duration_seconds > 0),
                speed_kmh REAL
                    CHECK(speed_kmh > 0.0),
                distance_km REAL
                    CHECK(distance_km > 0.0)
            );
            """.trimIndent()
        )
    }


    private fun createBodyTable(
        db: SQLiteDatabase,
    ) {
        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS body_observations(
                id INTEGER PRIMARY KEY,
                observation_id TEXT NOT NULL UNIQUE,
                observed_at TEXT NOT NULL,
                body_weight_kg REAL
                    CHECK(body_weight_kg > 0.0),
                neck_cm REAL
                    CHECK(neck_cm > 0.0),
                shoulders_cm REAL
                    CHECK(shoulders_cm > 0.0),
                chest_cm REAL
                    CHECK(chest_cm > 0.0),
                waist_cm REAL
                    CHECK(waist_cm > 0.0),
                hips_cm REAL
                    CHECK(hips_cm > 0.0),
                left_arm_cm REAL
                    CHECK(left_arm_cm > 0.0),
                right_arm_cm REAL
                    CHECK(right_arm_cm > 0.0),
                left_forearm_cm REAL
                    CHECK(left_forearm_cm > 0.0),
                right_forearm_cm REAL
                    CHECK(right_forearm_cm > 0.0),
                left_thigh_cm REAL
                    CHECK(left_thigh_cm > 0.0),
                right_thigh_cm REAL
                    CHECK(right_thigh_cm > 0.0),
                left_calf_cm REAL
                    CHECK(left_calf_cm > 0.0),
                right_calf_cm REAL
                    CHECK(right_calf_cm > 0.0),
                CHECK(
                    body_weight_kg IS NOT NULL OR
                    neck_cm IS NOT NULL OR
                    shoulders_cm IS NOT NULL OR
                    chest_cm IS NOT NULL OR
                    waist_cm IS NOT NULL OR
                    hips_cm IS NOT NULL OR
                    left_arm_cm IS NOT NULL OR
                    right_arm_cm IS NOT NULL OR
                    left_forearm_cm IS NOT NULL OR
                    right_forearm_cm IS NOT NULL OR
                    left_thigh_cm IS NOT NULL OR
                    right_thigh_cm IS NOT NULL OR
                    left_calf_cm IS NOT NULL OR
                    right_calf_cm IS NOT NULL
                )
            );
            """.trimIndent()
        )
    }

    private fun createActiveDraftTables(
        db: SQLiteDatabase,
    ) {
        /* WHY: unfinished capture must be durable without entering completed
         * history. The singleton check enforces the v1 one-active-draft rule. */
        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS active_session_draft(
                id INTEGER PRIMARY KEY
                    CHECK(id = 1),
                session_type TEXT NOT NULL
                    CHECK(
                        session_type IN (
                            'training',
                            'max_test'
                        )
                    ),
                selected_exercise_row_id INTEGER
                    REFERENCES exercises(id)
                    ON DELETE SET NULL,
                selected_exercise_label TEXT,
                selected_equipment_id TEXT,
                set_count_text TEXT NOT NULL,
                reps_text TEXT NOT NULL,
                duration_text TEXT NOT NULL,
                speed_text TEXT NOT NULL,
                distance_text TEXT NOT NULL,
                updated_at TEXT NOT NULL
            );
            """.trimIndent()
        )

        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS draft_session_exercises(
                id INTEGER PRIMARY KEY,
                draft_id INTEGER NOT NULL
                    REFERENCES active_session_draft(id)
                    ON DELETE CASCADE,
                exercise_row_id INTEGER NOT NULL
                    REFERENCES exercises(id)
                    ON DELETE RESTRICT,
                position INTEGER NOT NULL
                    CHECK(position >= 0),
                recording_mode TEXT NOT NULL
                    CHECK(
                        recording_mode IN (
                            'sets',
                            'continuous'
                        )
                    ),
                tracking_mode TEXT NOT NULL
                    CHECK(
                        tracking_mode IN (
                            'reps',
                            'duration'
                        )
                    ),
                data_fields INTEGER NOT NULL
                    CHECK(
                        data_fields >= 0 AND
                        (data_fields & ~3) = 0
                    ),
                equipment_row_id INTEGER
                    REFERENCES equipment(id)
                    ON DELETE SET NULL,
                entry_id TEXT NOT NULL UNIQUE,
                UNIQUE(draft_id, position)
            );
            """.trimIndent()
        )

        /* INVARIANT: child cascades terminate at the active-draft singleton;
         * neither discard nor child replacement reaches catalog/history rows. */

        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS draft_performed_sets(
                id INTEGER PRIMARY KEY,
                draft_exercise_row_id INTEGER NOT NULL
                    REFERENCES draft_session_exercises(id)
                    ON DELETE CASCADE,
                position INTEGER NOT NULL
                    CHECK(position >= 0),
                reps INTEGER
                    CHECK(reps >= 0),
                duration_seconds INTEGER
                    CHECK(duration_seconds > 0),
                weight_kg REAL
                    CHECK(weight_kg >= 0.0),
                CHECK(
                    (
                        reps IS NOT NULL AND
                        duration_seconds IS NULL
                    ) OR (
                        reps IS NULL AND
                        duration_seconds IS NOT NULL
                    )
                ),
                UNIQUE(draft_exercise_row_id, position)
            );
            """.trimIndent()
        )

        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS draft_continuous_activity(
                id INTEGER PRIMARY KEY,
                draft_exercise_row_id INTEGER NOT NULL UNIQUE
                    REFERENCES draft_session_exercises(id)
                    ON DELETE CASCADE,
                duration_seconds INTEGER NOT NULL
                    CHECK(duration_seconds > 0),
                speed_kmh REAL
                    CHECK(speed_kmh > 0.0),
                distance_km REAL
                    CHECK(distance_km > 0.0)
            );
            """.trimIndent()
        )
    }

    private fun createEquipmentTables(
        db: SQLiteDatabase,
    ) {
        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS equipment(
                id INTEGER PRIMARY KEY,
                equipment_id TEXT NOT NULL UNIQUE,
                label_name TEXT NOT NULL,
                display_name TEXT NOT NULL,
                equipment_type TEXT NOT NULL,
                load_semantics TEXT NOT NULL
                    CHECK(load_semantics IN ('none', 'external', 'assistance', 'bodyweight', 'cardio'))
            );
            """.trimIndent(),
        )
        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS equipment_aliases(
                equipment_row_id INTEGER NOT NULL REFERENCES equipment(id) ON DELETE CASCADE,
                alias TEXT NOT NULL,
                normalized_alias TEXT NOT NULL,
                UNIQUE(equipment_row_id, normalized_alias)
            );
            """.trimIndent(),
        )
        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS exercise_equipment(
                exercise_row_id INTEGER NOT NULL REFERENCES exercises(id) ON DELETE RESTRICT,
                equipment_row_id INTEGER NOT NULL REFERENCES equipment(id) ON DELETE RESTRICT,
                UNIQUE(exercise_row_id, equipment_row_id)
            );
            """.trimIndent(),
        )
        /* WHY: these are logical catalogue capabilities, including exercises
         * which need not yet exist in a user's mutable exercise catalogue. */
        db.execSQL(
            """
            CREATE TABLE IF NOT EXISTS catalog_exercise_equipment(
                exercise_id TEXT NOT NULL,
                equipment_row_id INTEGER NOT NULL
                    REFERENCES equipment(id)
                    ON DELETE RESTRICT,
                load_semantics TEXT NOT NULL
                    CHECK(load_semantics IN ('none', 'external', 'assistance', 'bodyweight', 'cardio')),
                PRIMARY KEY(exercise_id, equipment_row_id)
            );
            """.trimIndent(),
        )
    }

    private fun migrateEquipmentLoadSemanticsToVersionEight(
        db: SQLiteDatabase,
    ) {
        db.execSQL("PRAGMA defer_foreign_keys = ON;")

        db.execSQL("ALTER TABLE equipment_aliases RENAME TO equipment_aliases_v7;")
        db.execSQL("ALTER TABLE exercise_equipment RENAME TO exercise_equipment_v7;")
        db.execSQL("ALTER TABLE catalog_exercise_equipment RENAME TO catalog_exercise_equipment_v7;")
        db.execSQL("ALTER TABLE performed_sets RENAME TO performed_sets_v7;")
        db.execSQL("ALTER TABLE continuous_activity RENAME TO continuous_activity_v7;")
        db.execSQL("ALTER TABLE session_exercises RENAME TO session_exercises_v7;")
        db.execSQL("ALTER TABLE draft_performed_sets RENAME TO draft_performed_sets_v7;")
        db.execSQL("ALTER TABLE draft_continuous_activity RENAME TO draft_continuous_activity_v7;")
        db.execSQL("ALTER TABLE draft_session_exercises RENAME TO draft_session_exercises_v7;")
        db.execSQL("ALTER TABLE equipment RENAME TO equipment_v7;")

        createEquipmentTables(db)
        createSessionTables(db)
        createActiveDraftTables(db)

        db.execSQL(
            "INSERT INTO equipment(id,equipment_id,label_name,display_name,equipment_type,load_semantics) " +
                "SELECT id,equipment_id,label_name,display_name,equipment_type,load_semantics FROM equipment_v7;",
        )
        db.execSQL(
            "INSERT INTO equipment_aliases(equipment_row_id,alias,normalized_alias) " +
                "SELECT equipment_row_id,alias,normalized_alias FROM equipment_aliases_v7;",
        )
        db.execSQL(
            "INSERT INTO exercise_equipment(exercise_row_id,equipment_row_id) " +
                "SELECT exercise_row_id,equipment_row_id FROM exercise_equipment_v7;",
        )
        db.execSQL(
            "INSERT INTO catalog_exercise_equipment(exercise_id,equipment_row_id,load_semantics) " +
                "SELECT exercise_id,equipment_row_id,load_semantics FROM catalog_exercise_equipment_v7;",
        )
        db.execSQL(
            "INSERT INTO session_exercises(id,session_row_id,exercise_row_id,position,recording_mode," +
                "tracking_mode,data_fields,equipment_row_id,entry_id) " +
                "SELECT id,session_row_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields," +
                "equipment_row_id,entry_id FROM session_exercises_v7;",
        )
        db.execSQL(
            "INSERT INTO performed_sets(id,session_exercise_row_id,position,reps,duration_seconds,weight_kg) " +
                "SELECT id,session_exercise_row_id,position,reps,duration_seconds,weight_kg FROM performed_sets_v7;",
        )
        db.execSQL(
            "INSERT INTO continuous_activity(id,session_exercise_row_id,duration_seconds,speed_kmh,distance_km) " +
                "SELECT id,session_exercise_row_id,duration_seconds,speed_kmh,distance_km FROM continuous_activity_v7;",
        )
        db.execSQL(
            "INSERT INTO draft_session_exercises(id,draft_id,exercise_row_id,position,recording_mode," +
                "tracking_mode,data_fields,equipment_row_id,entry_id) " +
                "SELECT id,draft_id,exercise_row_id,position,recording_mode,tracking_mode,data_fields," +
                "equipment_row_id,entry_id FROM draft_session_exercises_v7;",
        )
        db.execSQL(
            "INSERT INTO draft_performed_sets(id,draft_exercise_row_id,position,reps,duration_seconds,weight_kg) " +
                "SELECT id,draft_exercise_row_id,position,reps,duration_seconds,weight_kg FROM draft_performed_sets_v7;",
        )
        db.execSQL(
            "INSERT INTO draft_continuous_activity(id,draft_exercise_row_id,duration_seconds,speed_kmh,distance_km) " +
                "SELECT id,draft_exercise_row_id,duration_seconds,speed_kmh,distance_km FROM draft_continuous_activity_v7;",
        )

        db.execSQL("DROP TABLE performed_sets_v7;")
        db.execSQL("DROP TABLE continuous_activity_v7;")
        db.execSQL("DROP TABLE session_exercises_v7;")
        db.execSQL("DROP TABLE draft_performed_sets_v7;")
        db.execSQL("DROP TABLE draft_continuous_activity_v7;")
        db.execSQL("DROP TABLE draft_session_exercises_v7;")
        db.execSQL("DROP TABLE equipment_aliases_v7;")
        db.execSQL("DROP TABLE exercise_equipment_v7;")
        db.execSQL("DROP TABLE catalog_exercise_equipment_v7;")
        db.execSQL("DROP TABLE equipment_v7;")
    }

    private fun addEquipmentReferenceColumns(
        db: SQLiteDatabase,
    ) {
        /* CONTRACT: v5 retains all historic/draft rows unchanged; equipment
         * is optional because it was not captured before this version. */
        db.execSQL(
            "ALTER TABLE session_exercises ADD COLUMN equipment_row_id INTEGER " +
                "REFERENCES equipment(id) ON DELETE SET NULL;",
        )
        db.execSQL(
            "ALTER TABLE draft_session_exercises ADD COLUMN equipment_row_id INTEGER " +
                "REFERENCES equipment(id) ON DELETE SET NULL;",
        )
        db.execSQL(
            "ALTER TABLE active_session_draft ADD COLUMN selected_equipment_id TEXT;",
        )
    }

    private fun seedEquipment(
        db: SQLiteDatabase,
    ) {
        /* INVARIANT: INSERT OR IGNORE makes application startup idempotent;
         * canonical IDs, rather than display text, are the persistent keys. */
        EquipmentCatalog.load(appContext).forEach { entry ->
            db.execSQL(
                """
                INSERT OR IGNORE INTO equipment(
                    equipment_id, label_name, display_name, equipment_type, load_semantics
                ) VALUES(?, ?, ?, ?, ?);
                """.trimIndent(),
                arrayOf(
                    entry.equipmentId,
                    entry.labelName,
                    entry.displayName,
                    entry.type,
                    entry.loadSemantics.name.lowercase(),
                ),
            )
            db.rawQuery(
                "SELECT id FROM equipment WHERE equipment_id = ?;",
                arrayOf(entry.equipmentId),
            ).use { cursor ->
                check(cursor.moveToFirst())
                val rowId = cursor.getLong(0)
                entry.aliases.forEach { alias ->
                    db.execSQL(
                        """
                        INSERT OR IGNORE INTO equipment_aliases(
                            equipment_row_id, alias, normalized_alias
                        ) VALUES(?, ?, ?);
                        """.trimIndent(),
                        arrayOf<Any>(
                            rowId,
                            alias,
                            equipmentAliasNormalize(alias),
                        ),
                    )
                }
            }
        }
        EquipmentCatalog.exerciseEquipmentRelations(appContext).forEach { relation ->
            db.rawQuery(
                "SELECT id FROM equipment WHERE equipment_id = ?;",
                arrayOf(relation.equipmentId),
            ).use { cursor ->
                check(cursor.moveToFirst()) {
                    "Seeded equipment missing: ${relation.equipmentId}"
                }
                db.execSQL(
                    "INSERT OR IGNORE INTO catalog_exercise_equipment(" +
                        "exercise_id, equipment_row_id, load_semantics) VALUES(?, ?, ?);",
                    arrayOf<Any>(
                        relation.exerciseId,
                        cursor.getLong(0),
                        relation.loadSemantics.name.lowercase(Locale.ROOT),
                    ),
                )
            }
        }
    }
}
