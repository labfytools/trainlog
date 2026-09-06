package com.labfytools.trainlog.data

import android.content.ContentValues
import android.content.Context
import android.database.sqlite.SQLiteConstraintException
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteOpenHelper
import com.labfytools.trainlog.model.BodyObservationDraft
import com.labfytools.trainlog.model.BodyObservationSummary
import com.labfytools.trainlog.model.ExerciseProfile
import com.labfytools.trainlog.model.NewExerciseProfile
import com.labfytools.trainlog.model.RecordingMode
import com.labfytools.trainlog.model.SessionDraft
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

class TrainlogRepository(
    context: Context,
) {
    private val database =
        TrainlogDatabaseHelper(
            context.applicationContext
        )

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
                        byId.recordingMode !=
                            recording ||
                        byId.trackingMode !=
                            tracking ||
                        byId.dataFields !=
                            dataFields
                    ) {
                        return PcCatalogImportResult.Invalid(
                            "Conflit de profil catalogue PC."
                        )
                    }

                    skipped += 1
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
                        byName.recordingMode !=
                            recording ||
                        byName.trackingMode !=
                            tracking ||
                        byName.dataFields !=
                            dataFields
                    ) {
                        return PcCatalogImportResult.Invalid(
                            "Conflit de profil pour $name."
                        )
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
                        }

                    db.update(
                        "exercises",
                        values,
                        "id = ?",
                        arrayOf(
                            byName.rowId
                                .toString()
                        )
                    )

                    reconciled += 1
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
        val recordingMode: RecordingMode,
        val trackingMode: TrackingMode,
        val dataFields: Int,
    )

    private fun findExerciseRow(
        db: SQLiteDatabase,
        selection: String,
        arguments: Array<String>,
    ): ExerciseRow? {
        db.query(
            "exercises",
            arrayOf(
                "id",
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
                    recordingMode =
                        if (
                            cursor.getString(1) ==
                            "continuous"
                        ) {
                            RecordingMode.CONTINUOUS
                        } else {
                            RecordingMode.SETS
                        },
                    trackingMode =
                        if (
                            cursor.getString(2) ==
                            "duration"
                        ) {
                            TrackingMode.DURATION
                        } else {
                            TrackingMode.REPS
                        },
                    dataFields =
                        cursor.getInt(3),
                )
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
                e.name,
                se.recording_mode,
                se.tracking_mode,
                se.data_fields
            FROM session_exercises AS se
            JOIN sessions AS s
                ON s.id = se.session_row_id
            JOIN exercises AS e
                ON e.id = se.exercise_row_id
            WHERE s.session_id = ?
            ORDER BY se.position ASC;
            """.trimIndent(),
            arrayOf(sessionId),
        ).use { cursor ->
            while (cursor.moveToNext()) {
                val sessionExerciseRowId =
                    cursor.getLong(0)

                val name =
                    cursor.getString(1)

                val recording =
                    when (
                        cursor.getString(2)
                    ) {
                        "continuous" ->
                            RecordingMode.CONTINUOUS

                        else ->
                            RecordingMode.SETS
                    }

                val tracking =
                    when (
                        cursor.getString(3)
                    ) {
                        "duration" ->
                            TrackingMode.DURATION

                        else ->
                            TrackingMode.REPS
                    }

                val dataFields =
                    cursor.getInt(4)

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
                                exerciseName =
                                    name,
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
                                )
                        }
                    }

                    exercises +=
                        SessionExerciseDetail(
                            exerciseName =
                                name,
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

private class TrainlogDatabaseHelper(
    context: Context,
) : SQLiteOpenHelper(
    context,
    "trainlog-android.db",
    null,
    3,
) {
    override fun onConfigure(
        db: SQLiteDatabase,
    ) {
        super.onConfigure(db)

        db.setForeignKeyConstraintsEnabled(
            true
        )
    }

    override fun onCreate(
        db: SQLiteDatabase,
    ) {
        createExerciseTable(db)
        createSessionTables(db)
        createBodyTable(db)
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

        if (version != newVersion) {
            error(
                "Unsupported Android DB upgrade " +
                    "$oldVersion -> $newVersion"
            )
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
}
