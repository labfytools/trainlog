#include "trainlog/web_heart_rate.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sqlite3.h>
#include <yyjson.h>

#include "database_internal.h"

#define HEART_RATE_SAMPLE_MAX 100000U
#define TIMELINE_EVENT_MAX 2048U
#define GUIDANCE_PHASE_MAX 128U
#define GUIDANCE_EVENT_MAX 4096U

static bool prepare(TrainlogDatabase *database, const char *sql, sqlite3_stmt **statement) {
    return sqlite3_prepare_v2(database->connection, sql, -1, statement, NULL) == SQLITE_OK;
}

static bool valid_context_id(const char *value) {
    size_t length;
    if (value == NULL) {
        return false;
    }
    length = strlen(value);
    if (length < 4U || length >= 64U) {
        return false;
    }
    return strncmp(value, "se_", 3U) == 0 || strncmp(value, "sl_", 3U) == 0;
}

static bool add_nullable_string(yyjson_mut_doc *document,
                                yyjson_mut_val *object,
                                const char *key,
                                const char *value) {
    return value == NULL ? yyjson_mut_obj_add_null(document, object, key)
                         : yyjson_mut_obj_add_strcpy(document, object, key, value);
}

static bool add_nullable_column_string(yyjson_mut_doc *document,
                                       yyjson_mut_val *object,
                                       const char *key,
                                       sqlite3_stmt *statement,
                                       int column) {
    return sqlite3_column_type(statement, column) == SQLITE_NULL
               ? yyjson_mut_obj_add_null(document, object, key)
               : yyjson_mut_obj_add_strcpy(
                     document, object, key, (const char *)sqlite3_column_text(statement, column));
}

static bool add_nullable_column_int(yyjson_mut_doc *document,
                                    yyjson_mut_val *object,
                                    const char *key,
                                    sqlite3_stmt *statement,
                                    int column) {
    return sqlite3_column_type(statement, column) == SQLITE_NULL
               ? yyjson_mut_obj_add_null(document, object, key)
               : yyjson_mut_obj_add_sint(
                     document, object, key, sqlite3_column_int64(statement, column));
}

static bool append_event(yyjson_mut_doc *document,
                         yyjson_mut_val *events,
                         const char *type,
                         const char *at,
                         const char *end_at,
                         const char *label,
                         const char *entry_id,
                         const char *exercise_id) {
    yyjson_mut_val *event = yyjson_mut_obj(document);
    if (event == NULL || !yyjson_mut_obj_add_strcpy(document, event, "type", type) ||
        !yyjson_mut_obj_add_strcpy(document, event, "at", at) ||
        !add_nullable_string(document, event, "end_at", end_at) ||
        !add_nullable_string(document, event, "label", label) ||
        !add_nullable_string(document, event, "entry_id", entry_id) ||
        !add_nullable_string(document, event, "exercise_id", exercise_id) ||
        !yyjson_mut_arr_append(events, event)) {
        return false;
    }
    return true;
}

static bool add_samples(TrainlogDatabase *database,
                        yyjson_mut_doc *document,
                        yyjson_mut_val *capture,
                        const char *capture_id) {
    static const char COUNT_SQL[] = "SELECT COUNT(*) FROM heart_rate_samples WHERE capture_id=?;";
    static const char SAMPLE_SQL[] =
        "SELECT s.sequence,s.observed_at,s.bpm,r.rr_index,r.value_1024 "
        "FROM heart_rate_samples s LEFT JOIN heart_rate_rr_intervals r "
        "ON r.capture_id=s.capture_id AND r.sample_sequence=s.sequence "
        "WHERE s.capture_id=? ORDER BY s.sequence,r.rr_index;";
    sqlite3_stmt *count_statement = NULL;
    sqlite3_stmt *statement = NULL;
    yyjson_mut_val *samples = yyjson_mut_arr(document);
    yyjson_mut_val *sample = NULL;
    yyjson_mut_val *rr = NULL;
    sqlite3_int64 current_sequence = -1;
    sqlite3_int64 count;
    int step;

    if (samples == NULL || !prepare(database, COUNT_SQL, &count_statement) ||
        sqlite3_bind_text(count_statement, 1, capture_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(count_statement) != SQLITE_ROW) {
        if (count_statement != NULL) {
            (void)sqlite3_finalize(count_statement);
        }
        return false;
    }
    count = sqlite3_column_int64(count_statement, 0);
    if (sqlite3_step(count_statement) != SQLITE_DONE ||
        sqlite3_finalize(count_statement) != SQLITE_OK || count < 0 ||
        (uint64_t)count > HEART_RATE_SAMPLE_MAX ||
        !yyjson_mut_obj_add_sint(document, capture, "sample_count", count) ||
        !prepare(database, SAMPLE_SQL, &statement) ||
        sqlite3_bind_text(statement, 1, capture_id, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        if (statement != NULL) {
            (void)sqlite3_finalize(statement);
        }
        return false;
    }

    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        sqlite3_int64 sequence = sqlite3_column_int64(statement, 0);
        if (sequence != current_sequence) {
            sample = yyjson_mut_obj(document);
            rr = yyjson_mut_arr(document);
            if (sample == NULL || rr == NULL ||
                !yyjson_mut_obj_add_sint(document, sample, "sequence", sequence) ||
                !yyjson_mut_obj_add_strcpy(document,
                                           sample,
                                           "observed_at",
                                           (const char *)sqlite3_column_text(statement, 1)) ||
                !yyjson_mut_obj_add_sint(
                    document, sample, "bpm", sqlite3_column_int64(statement, 2)) ||
                !yyjson_mut_obj_add_val(document, sample, "rr_1024", rr) ||
                !yyjson_mut_arr_append(samples, sample)) {
                (void)sqlite3_finalize(statement);
                return false;
            }
            current_sequence = sequence;
        }
        if (sqlite3_column_type(statement, 4) != SQLITE_NULL &&
            !yyjson_mut_arr_add_sint(document, rr, sqlite3_column_int64(statement, 4))) {
            (void)sqlite3_finalize(statement);
            return false;
        }
    }
    if (step != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK ||
        !yyjson_mut_obj_add_val(document, capture, "samples", samples)) {
        return false;
    }
    return true;
}

static bool add_session_events(TrainlogDatabase *database,
                               yyjson_mut_doc *document,
                               yyjson_mut_val *events,
                               const char *session_id) {
    static const char SESSION_SQL[] =
        "SELECT started_at,ended_at FROM sessions WHERE session_id=?;";
    static const char EXERCISE_SQL[] =
        "SELECT t.entry_id,t.exercise_id,e.name,t.started_at,t.ended_at "
        "FROM session_exercise_timeline t "
        "JOIN sessions s ON s.session_id=t.session_id "
        "JOIN session_exercises se ON se.session_row_id=s.id AND se.entry_id=t.entry_id "
        "JOIN exercises e ON e.id=se.exercise_row_id AND e.exercise_id=t.exercise_id "
        "WHERE t.session_id=? ORDER BY t.started_at,t.entry_id;";
    sqlite3_stmt *statement = NULL;
    size_t count = 0U;
    int step;

    if (!prepare(database, SESSION_SQL, &statement) ||
        sqlite3_bind_text(statement, 1, session_id, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_step(statement) != SQLITE_ROW ||
        !append_event(document,
                      events,
                      "session",
                      (const char *)sqlite3_column_text(statement, 0),
                      sqlite3_column_type(statement, 1) == SQLITE_NULL
                          ? NULL
                          : (const char *)sqlite3_column_text(statement, 1),
                      NULL,
                      NULL,
                      NULL) ||
        sqlite3_step(statement) != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK) {
        if (statement != NULL) {
            (void)sqlite3_finalize(statement);
        }
        return false;
    }

    statement = NULL;
    if (!prepare(database, EXERCISE_SQL, &statement) ||
        sqlite3_bind_text(statement, 1, session_id, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        if (statement != NULL) {
            (void)sqlite3_finalize(statement);
        }
        return false;
    }
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        if (++count > TIMELINE_EVENT_MAX ||
            !append_event(document,
                          events,
                          "exercise",
                          (const char *)sqlite3_column_text(statement, 3),
                          sqlite3_column_type(statement, 4) == SQLITE_NULL
                              ? NULL
                              : (const char *)sqlite3_column_text(statement, 4),
                          (const char *)sqlite3_column_text(statement, 2),
                          (const char *)sqlite3_column_text(statement, 0),
                          (const char *)sqlite3_column_text(statement, 1))) {
            (void)sqlite3_finalize(statement);
            return false;
        }
    }
    return step == SQLITE_DONE && sqlite3_finalize(statement) == SQLITE_OK;
}

static bool add_sleep_events(TrainlogDatabase *database,
                             yyjson_mut_doc *document,
                             yyjson_mut_val *events,
                             const char *entry_id) {
    static const char EVENT_SQL[] =
        "SELECT e.event_type,e.start_at,e.end_at "
        "FROM sleep_diary_entries d JOIN sleep_diary_events e "
        "ON e.revision_id=d.current_revision_id "
        "WHERE d.entry_id=? AND d.deleted=0 ORDER BY e.start_at,e.event_id;";
    static const char INTAKE_SQL[] =
        "SELECT i.taken_at,i.medication_name,i.quantity "
        "FROM sleep_diary_entries d JOIN sleep_medication_intakes i "
        "ON i.revision_id=d.current_revision_id "
        "WHERE d.entry_id=? AND d.deleted=0 ORDER BY i.taken_at,i.intake_id;";
    sqlite3_stmt *statement = NULL;
    size_t count = 0U;
    int step;

    if (!prepare(database, EVENT_SQL, &statement) ||
        sqlite3_bind_text(statement, 1, entry_id, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        if (statement != NULL) {
            (void)sqlite3_finalize(statement);
        }
        return false;
    }
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        if (++count > TIMELINE_EVENT_MAX ||
            !append_event(document,
                          events,
                          (const char *)sqlite3_column_text(statement, 0),
                          (const char *)sqlite3_column_text(statement, 1),
                          sqlite3_column_type(statement, 2) == SQLITE_NULL
                              ? NULL
                              : (const char *)sqlite3_column_text(statement, 2),
                          NULL,
                          NULL,
                          NULL)) {
            (void)sqlite3_finalize(statement);
            return false;
        }
    }
    if (step != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK) {
        return false;
    }

    statement = NULL;
    if (!prepare(database, INTAKE_SQL, &statement) ||
        sqlite3_bind_text(statement, 1, entry_id, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        if (statement != NULL) {
            (void)sqlite3_finalize(statement);
        }
        return false;
    }
    while ((step = sqlite3_step(statement)) == SQLITE_ROW) {
        char label[256];
        const char *name = (const char *)sqlite3_column_text(statement, 1);
        int quantity = sqlite3_column_int(statement, 2);
        if (quantity > 1) {
            (void)snprintf(label, sizeof(label), "%s ×%d", name, quantity);
        } else {
            (void)snprintf(label, sizeof(label), "%s", name);
        }
        if (++count > TIMELINE_EVENT_MAX ||
            !append_event(document,
                          events,
                          "medication",
                          (const char *)sqlite3_column_text(statement, 0),
                          NULL,
                          label,
                          NULL,
                          NULL)) {
            (void)sqlite3_finalize(statement);
            return false;
        }
    }
    return step == SQLITE_DONE && sqlite3_finalize(statement) == SQLITE_OK;
}

static bool add_guidance(TrainlogDatabase *database,
                         yyjson_mut_doc *document,
                         yyjson_mut_val *root,
                         const char *session_id) {
    static const char RUN_SQL[] = "SELECT run_id,started_at,ended_at FROM cardio_guidance_runs "
                                  "WHERE session_id=? ORDER BY ended_at DESC,run_id DESC LIMIT 1;";
    static const char PHASE_SQL[] =
        "SELECT phase_id,entry_id,position,kind,target_min_bpm,target_max_bpm,"
        "calibration_id,calibration_observed_peak_bpm,minimum_percent,maximum_percent,"
        "exit_kind,exit_seconds,exit_bpm,started_at,ended_at,final_instruction "
        "FROM cardio_guidance_phases WHERE run_id=? ORDER BY position,phase_id;";
    static const char EVENT_SQL[] =
        "SELECT sequence,observed_at,instruction,bpm,target_min_bpm,target_max_bpm "
        "FROM cardio_guidance_events WHERE run_id=? AND phase_id=? ORDER BY sequence;";
    sqlite3_stmt *run_statement = NULL;
    sqlite3_stmt *phase_statement = NULL;
    const char *run_id;
    yyjson_mut_val *guidance;
    yyjson_mut_val *phases;
    size_t phase_count = 0U;
    int step;

    if (!prepare(database, RUN_SQL, &run_statement) ||
        sqlite3_bind_text(run_statement, 1, session_id, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        if (run_statement != NULL) {
            (void)sqlite3_finalize(run_statement);
        }
        return false;
    }
    step = sqlite3_step(run_statement);
    if (step == SQLITE_DONE) {
        (void)sqlite3_finalize(run_statement);
        return yyjson_mut_obj_add_null(document, root, "guidance");
    }
    if (step != SQLITE_ROW) {
        (void)sqlite3_finalize(run_statement);
        return false;
    }

    run_id = (const char *)sqlite3_column_text(run_statement, 0);
    guidance = yyjson_mut_obj(document);
    phases = yyjson_mut_arr(document);
    if (guidance == NULL || phases == NULL ||
        !yyjson_mut_obj_add_strcpy(document, guidance, "run_id", run_id) ||
        !yyjson_mut_obj_add_strcpy(document,
                                   guidance,
                                   "started_at",
                                   (const char *)sqlite3_column_text(run_statement, 1)) ||
        !yyjson_mut_obj_add_strcpy(
            document, guidance, "ended_at", (const char *)sqlite3_column_text(run_statement, 2))) {
        (void)sqlite3_finalize(run_statement);
        return false;
    }
    {
        char stable_run_id[64];
        if (snprintf(stable_run_id, sizeof(stable_run_id), "%s", run_id) < 0 ||
            strlen(run_id) >= sizeof(stable_run_id) || sqlite3_step(run_statement) != SQLITE_DONE ||
            sqlite3_finalize(run_statement) != SQLITE_OK ||
            !prepare(database, PHASE_SQL, &phase_statement) ||
            sqlite3_bind_text(phase_statement, 1, stable_run_id, -1, SQLITE_TRANSIENT) !=
                SQLITE_OK) {
            if (phase_statement != NULL) {
                (void)sqlite3_finalize(phase_statement);
            }
            return false;
        }

        while ((step = sqlite3_step(phase_statement)) == SQLITE_ROW) {
            yyjson_mut_val *phase = yyjson_mut_obj(document);
            yyjson_mut_val *target = NULL;
            yyjson_mut_val *exit_condition = yyjson_mut_obj(document);
            yyjson_mut_val *instruction_events = yyjson_mut_arr(document);
            sqlite3_stmt *event_statement = NULL;
            const char *phase_id = (const char *)sqlite3_column_text(phase_statement, 0);
            char stable_phase_id[64];
            size_t event_count = 0U;
            int event_step;

            if (++phase_count > GUIDANCE_PHASE_MAX || phase == NULL || exit_condition == NULL ||
                instruction_events == NULL || strlen(phase_id) >= sizeof(stable_phase_id) ||
                snprintf(stable_phase_id, sizeof(stable_phase_id), "%s", phase_id) < 0 ||
                !yyjson_mut_obj_add_strcpy(document, phase, "phase_id", phase_id) ||
                !yyjson_mut_obj_add_strcpy(document,
                                           phase,
                                           "entry_id",
                                           (const char *)sqlite3_column_text(phase_statement, 1)) ||
                !yyjson_mut_obj_add_sint(
                    document, phase, "position", sqlite3_column_int64(phase_statement, 2)) ||
                !yyjson_mut_obj_add_strcpy(document,
                                           phase,
                                           "kind",
                                           (const char *)sqlite3_column_text(phase_statement, 3))) {
                (void)sqlite3_finalize(phase_statement);
                return false;
            }

            if (sqlite3_column_type(phase_statement, 4) == SQLITE_NULL) {
                if (!yyjson_mut_obj_add_null(document, phase, "target")) {
                    (void)sqlite3_finalize(phase_statement);
                    return false;
                }
            } else {
                target = yyjson_mut_obj(document);
                if (target == NULL ||
                    !yyjson_mut_obj_add_sint(document,
                                             target,
                                             "minimum_bpm",
                                             sqlite3_column_int64(phase_statement, 4)) ||
                    !yyjson_mut_obj_add_sint(document,
                                             target,
                                             "maximum_bpm",
                                             sqlite3_column_int64(phase_statement, 5)) ||
                    !add_nullable_column_string(
                        document, target, "calibration_id", phase_statement, 6) ||
                    !add_nullable_column_int(
                        document, target, "calibration_observed_peak_bpm", phase_statement, 7) ||
                    !add_nullable_column_int(
                        document, target, "minimum_percent", phase_statement, 8) ||
                    !add_nullable_column_int(
                        document, target, "maximum_percent", phase_statement, 9) ||
                    !yyjson_mut_obj_add_val(document, phase, "target", target)) {
                    (void)sqlite3_finalize(phase_statement);
                    return false;
                }
            }

            if (!yyjson_mut_obj_add_strcpy(
                    document,
                    exit_condition,
                    "kind",
                    (const char *)sqlite3_column_text(phase_statement, 10)) ||
                !add_nullable_column_int(
                    document, exit_condition, "seconds", phase_statement, 11) ||
                !add_nullable_column_int(document, exit_condition, "bpm", phase_statement, 12) ||
                !yyjson_mut_obj_add_val(document, phase, "exit_condition", exit_condition) ||
                !yyjson_mut_obj_add_strcpy(
                    document,
                    phase,
                    "started_at",
                    (const char *)sqlite3_column_text(phase_statement, 13)) ||
                !yyjson_mut_obj_add_strcpy(
                    document,
                    phase,
                    "ended_at",
                    (const char *)sqlite3_column_text(phase_statement, 14)) ||
                !yyjson_mut_obj_add_strcpy(
                    document,
                    phase,
                    "final_instruction",
                    (const char *)sqlite3_column_text(phase_statement, 15)) ||
                !prepare(database, EVENT_SQL, &event_statement) ||
                sqlite3_bind_text(event_statement, 1, stable_run_id, -1, SQLITE_TRANSIENT) !=
                    SQLITE_OK ||
                sqlite3_bind_text(event_statement, 2, stable_phase_id, -1, SQLITE_TRANSIENT) !=
                    SQLITE_OK) {
                if (event_statement != NULL) {
                    (void)sqlite3_finalize(event_statement);
                }
                (void)sqlite3_finalize(phase_statement);
                return false;
            }

            while ((event_step = sqlite3_step(event_statement)) == SQLITE_ROW) {
                yyjson_mut_val *event = yyjson_mut_obj(document);
                if (++event_count > GUIDANCE_EVENT_MAX || event == NULL ||
                    !yyjson_mut_obj_add_sint(
                        document, event, "sequence", sqlite3_column_int64(event_statement, 0)) ||
                    !yyjson_mut_obj_add_strcpy(
                        document,
                        event,
                        "observed_at",
                        (const char *)sqlite3_column_text(event_statement, 1)) ||
                    !yyjson_mut_obj_add_strcpy(
                        document,
                        event,
                        "instruction",
                        (const char *)sqlite3_column_text(event_statement, 2)) ||
                    !add_nullable_column_int(document, event, "bpm", event_statement, 3) ||
                    !add_nullable_column_int(
                        document, event, "target_minimum_bpm", event_statement, 4) ||
                    !add_nullable_column_int(
                        document, event, "target_maximum_bpm", event_statement, 5) ||
                    !yyjson_mut_arr_append(instruction_events, event)) {
                    (void)sqlite3_finalize(event_statement);
                    (void)sqlite3_finalize(phase_statement);
                    return false;
                }
            }
            if (event_step != SQLITE_DONE || sqlite3_finalize(event_statement) != SQLITE_OK ||
                !yyjson_mut_obj_add_val(document, phase, "events", instruction_events) ||
                !yyjson_mut_arr_append(phases, phase)) {
                (void)sqlite3_finalize(phase_statement);
                return false;
            }
        }
        if (step != SQLITE_DONE || sqlite3_finalize(phase_statement) != SQLITE_OK) {
            return false;
        }
    }

    return yyjson_mut_obj_add_val(document, guidance, "phases", phases) &&
           yyjson_mut_obj_add_val(document, root, "guidance", guidance);
}

static bool add_calibration(TrainlogDatabase *database,
                            yyjson_mut_doc *document,
                            yyjson_mut_val *root,
                            const char *session_id) {
    static const char SQL[] =
        "SELECT calibration_id,observed_peak_bpm,effort_end_at "
        "FROM cardio_calibrations WHERE session_id=? ORDER BY ended_at DESC LIMIT 1;";
    static const char RECOVERY_SQL[] =
        "SELECT target_offset_seconds,observed_at,bpm FROM cardio_calibration_recovery "
        "WHERE calibration_id=? ORDER BY target_offset_seconds;";
    sqlite3_stmt *statement = NULL;
    sqlite3_stmt *recovery_statement = NULL;
    yyjson_mut_val *calibration;
    yyjson_mut_val *recovery;
    int step;
    char calibration_id[64];

    if (!prepare(database, SQL, &statement) ||
        sqlite3_bind_text(statement, 1, session_id, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        if (statement != NULL) {
            (void)sqlite3_finalize(statement);
        }
        return false;
    }
    step = sqlite3_step(statement);
    if (step == SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        return yyjson_mut_obj_add_null(document, root, "calibration");
    }
    if (step != SQLITE_ROW ||
        strlen((const char *)sqlite3_column_text(statement, 0)) >= sizeof(calibration_id) ||
        snprintf(calibration_id,
                 sizeof(calibration_id),
                 "%s",
                 (const char *)sqlite3_column_text(statement, 0)) < 0) {
        (void)sqlite3_finalize(statement);
        return false;
    }

    calibration = yyjson_mut_obj(document);
    recovery = yyjson_mut_arr(document);
    if (calibration == NULL || recovery == NULL ||
        !yyjson_mut_obj_add_strcpy(document, calibration, "calibration_id", calibration_id) ||
        !yyjson_mut_obj_add_sint(
            document, calibration, "observed_peak_bpm", sqlite3_column_int64(statement, 1)) ||
        !yyjson_mut_obj_add_strcpy(document,
                                   calibration,
                                   "effort_end_at",
                                   (const char *)sqlite3_column_text(statement, 2)) ||
        sqlite3_step(statement) != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK ||
        !prepare(database, RECOVERY_SQL, &recovery_statement) ||
        sqlite3_bind_text(recovery_statement, 1, calibration_id, -1, SQLITE_TRANSIENT) !=
            SQLITE_OK) {
        if (recovery_statement != NULL) {
            (void)sqlite3_finalize(recovery_statement);
        }
        return false;
    }
    while ((step = sqlite3_step(recovery_statement)) == SQLITE_ROW) {
        yyjson_mut_val *point = yyjson_mut_obj(document);
        if (point == NULL ||
            !yyjson_mut_obj_add_sint(
                document, point, "offset_seconds", sqlite3_column_int64(recovery_statement, 0)) ||
            !yyjson_mut_obj_add_strcpy(document,
                                       point,
                                       "observed_at",
                                       (const char *)sqlite3_column_text(recovery_statement, 1)) ||
            !yyjson_mut_obj_add_sint(
                document, point, "bpm", sqlite3_column_int64(recovery_statement, 2)) ||
            !yyjson_mut_arr_append(recovery, point)) {
            (void)sqlite3_finalize(recovery_statement);
            return false;
        }
    }
    return step == SQLITE_DONE && sqlite3_finalize(recovery_statement) == SQLITE_OK &&
           yyjson_mut_obj_add_val(document, calibration, "recovery", recovery) &&
           yyjson_mut_obj_add_val(document, root, "calibration", calibration);
}

TrainlogStatus trainlog_web_heart_rate_timeline_json(TrainlogDatabase *database,
                                                     const char *context_id,
                                                     char *output,
                                                     size_t capacity,
                                                     size_t *output_size) {
    static const char CAPTURE_SQL[] =
        "SELECT capture_id,context_kind,started_at,ended_at,sensor_name "
        "FROM heart_rate_captures WHERE context_id=? AND ended_at IS NOT NULL "
        "ORDER BY ended_at DESC,capture_id DESC LIMIT 1;";
    sqlite3_stmt *statement = NULL;
    yyjson_mut_doc *document;
    yyjson_mut_val *root;
    yyjson_mut_val *capture;
    yyjson_mut_val *events;
    const char *context_kind;
    char capture_id[64];
    char kind[16];
    char *json;
    size_t json_size;
    yyjson_write_err error;
    int step;
    bool ok;

    if (database == NULL || !valid_context_id(context_id) || output == NULL || capacity < 2U ||
        output_size == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    *output_size = 0U;
    output[0] = '\0';

    document = yyjson_mut_doc_new(NULL);
    root = document == NULL ? NULL : yyjson_mut_obj(document);
    events = document == NULL ? NULL : yyjson_mut_arr(document);
    if (document == NULL || root == NULL || events == NULL) {
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    yyjson_mut_doc_set_root(document, root);
    if (!yyjson_mut_obj_add_uint(document, root, "api_version", 1U) ||
        !yyjson_mut_obj_add_strcpy(document, root, "context_id", context_id) ||
        !prepare(database, CAPTURE_SQL, &statement) ||
        sqlite3_bind_text(statement, 1, context_id, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        if (statement != NULL) {
            (void)sqlite3_finalize(statement);
        }
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    step = sqlite3_step(statement);
    if (step == SQLITE_DONE) {
        (void)sqlite3_finalize(statement);
        if (!yyjson_mut_obj_add_bool(document, root, "available", false) ||
            !yyjson_mut_obj_add_null(document, root, "capture") ||
            !yyjson_mut_obj_add_val(document, root, "events", events) ||
            !yyjson_mut_obj_add_null(document, root, "guidance") ||
            !yyjson_mut_obj_add_null(document, root, "calibration")) {
            yyjson_mut_doc_free(document);
            return TRAINLOG_STATUS_SYSTEM_ERROR;
        }
    } else if (step == SQLITE_ROW) {
        const char *capture_text = (const char *)sqlite3_column_text(statement, 0);
        const char *kind_text = (const char *)sqlite3_column_text(statement, 1);
        if (strlen(capture_text) >= sizeof(capture_id) || strlen(kind_text) >= sizeof(kind) ||
            snprintf(capture_id, sizeof(capture_id), "%s", capture_text) < 0 ||
            snprintf(kind, sizeof(kind), "%s", kind_text) < 0) {
            (void)sqlite3_finalize(statement);
            yyjson_mut_doc_free(document);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
        context_kind = kind;
        capture = yyjson_mut_obj(document);
        if (capture == NULL || !yyjson_mut_obj_add_bool(document, root, "available", true) ||
            !yyjson_mut_obj_add_strcpy(document, capture, "capture_id", capture_id) ||
            !yyjson_mut_obj_add_strcpy(document, capture, "context_kind", context_kind) ||
            !yyjson_mut_obj_add_strcpy(
                document, capture, "started_at", (const char *)sqlite3_column_text(statement, 2)) ||
            !yyjson_mut_obj_add_strcpy(
                document, capture, "ended_at", (const char *)sqlite3_column_text(statement, 3)) ||
            !add_nullable_column_string(document, capture, "sensor_name", statement, 4) ||
            sqlite3_step(statement) != SQLITE_DONE || sqlite3_finalize(statement) != SQLITE_OK ||
            !add_samples(database, document, capture, capture_id) ||
            !yyjson_mut_obj_add_val(document, root, "capture", capture)) {
            yyjson_mut_doc_free(document);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }

        if (strcmp(context_kind, "sleep") == 0) {
            ok = add_sleep_events(database, document, events, context_id) &&
                 yyjson_mut_obj_add_null(document, root, "guidance") &&
                 yyjson_mut_obj_add_null(document, root, "calibration");
        } else if (strcmp(context_kind, "session") == 0 || strcmp(context_kind, "cardio") == 0) {
            ok = add_session_events(database, document, events, context_id) &&
                 (strcmp(context_kind, "cardio") != 0 ||
                  (add_guidance(database, document, root, context_id) &&
                   add_calibration(database, document, root, context_id))) &&
                 (strcmp(context_kind, "cardio") == 0 ||
                  (yyjson_mut_obj_add_null(document, root, "guidance") &&
                   yyjson_mut_obj_add_null(document, root, "calibration")));
        } else {
            ok = false;
        }
        if (!ok || !yyjson_mut_obj_add_val(document, root, "events", events)) {
            yyjson_mut_doc_free(document);
            return TRAINLOG_STATUS_DATABASE_ERROR;
        }
    } else {
        (void)sqlite3_finalize(statement);
        yyjson_mut_doc_free(document);
        return TRAINLOG_STATUS_DATABASE_ERROR;
    }

    json = yyjson_mut_write_opts(document, YYJSON_WRITE_NOFLAG, NULL, &json_size, &error);
    yyjson_mut_doc_free(document);
    if (json == NULL || json_size + 2U > capacity) {
        free(json);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    (void)memcpy(output, json, json_size);
    output[json_size] = '\n';
    output[json_size + 1U] = '\0';
    *output_size = json_size + 1U;
    free(json);
    return TRAINLOG_STATUS_OK;
}
