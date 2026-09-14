/**
 * @file test_schema_v16_migration.c
 * @brief Occurrence-owned tracking-mode migration and future-profile proof.
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sqlite3.h>
#include "trainlog/database.h"

#define CHECK(x) do { if (!(x)) { (void)fprintf(stderr, \
    "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #x); return false; } } while (0)

static const char *const FIXTURE =
    "PRAGMA foreign_keys=ON;"
    "CREATE TABLE exercises(id INTEGER PRIMARY KEY,exercise_id TEXT UNIQUE,name TEXT,"
    "normalized_name TEXT UNIQUE,tracking_mode TEXT NOT NULL,recording_mode TEXT NOT NULL,"
    "data_fields INTEGER NOT NULL);"
    "CREATE TABLE sessions(id INTEGER PRIMARY KEY,session_id TEXT UNIQUE,started_at TEXT,"
    "ended_at TEXT,session_type TEXT,notes TEXT);"
    "CREATE TABLE session_exercises(id INTEGER PRIMARY KEY,entry_id TEXT UNIQUE,"
    "session_row_id INTEGER REFERENCES sessions(id),exercise_row_id INTEGER REFERENCES exercises(id),"
    "recording_mode TEXT,data_fields INTEGER,position INTEGER,load_mode TEXT,rest_seconds INTEGER,"
    "target_sets INTEGER,target_reps INTEGER,target_duration_seconds INTEGER,target_weight_kg REAL,"
    "equipment_id TEXT,notes TEXT,UNIQUE(session_row_id,position));"
    "CREATE TABLE performed_sets(id INTEGER PRIMARY KEY,session_exercise_row_id INTEGER REFERENCES session_exercises(id),"
    "position INTEGER,reps INTEGER,duration_seconds INTEGER,weight_kg REAL);"
    "CREATE TABLE continuous_activity(id INTEGER PRIMARY KEY,session_exercise_row_id INTEGER REFERENCES session_exercises(id),"
    "duration_seconds INTEGER,speed_kmh REAL,distance_km REAL);"
    "CREATE TABLE max_results(session_exercise_row_id INTEGER PRIMARY KEY REFERENCES session_exercises(id),max_weight_kg REAL);"
    "CREATE TABLE exercise_body_zones(exercise_row_id INTEGER REFERENCES exercises(id),zone_id TEXT,role TEXT,PRIMARY KEY(exercise_row_id,zone_id));"
    "CREATE TABLE exercise_body_zone_sync(exercise_row_id INTEGER PRIMARY KEY REFERENCES exercises(id),synced_state TEXT);"
    "CREATE TABLE exercise_feedback(id INTEGER PRIMARY KEY,feedback_id TEXT UNIQUE,session_exercise_row_id INTEGER REFERENCES session_exercises(id) ON DELETE CASCADE,observed_at TEXT,raw_text TEXT);"
    "CREATE TABLE exercise_feedback_revisions(revision_id TEXT PRIMARY KEY,feedback_id TEXT REFERENCES exercise_feedback(feedback_id) ON DELETE CASCADE,created_at TEXT,raw_text TEXT);"
    "CREATE TABLE session_followups(id INTEGER PRIMARY KEY,followup_id TEXT UNIQUE,session_row_id INTEGER REFERENCES sessions(id),observed_at TEXT,raw_text TEXT);"
    "CREATE TABLE session_followup_revisions(revision_id TEXT PRIMARY KEY,followup_id TEXT REFERENCES session_followups(followup_id),created_at TEXT,raw_text TEXT);"
    "INSERT INTO exercises VALUES(7,'ex_b35fff35-9c97-4c82-9053-7d1f2a23d3ac','Planche face sol','planche face sol','reps','sets',0);"
    "INSERT INTO exercises VALUES(8,'ex_e32c40a6-72b4-4233-a0db-41b27e990c96','Planche gauche sol','planche gauche sol','reps','sets',0);"
    "INSERT INTO exercises VALUES(9,'ex_999fd503-ce89-49f9-979d-c19e6adbe80d','Planche droite sol','planche droite sol','reps','sets',0);"
    "INSERT INTO sessions VALUES(11,'se_old','2026-09-01T10:00:00Z',NULL,'training','note');"
    "INSERT INTO session_exercises VALUES(13,'sxe_old',11,7,'sets',0,0,'none',30,1,3,NULL,NULL,NULL,'occurrence');"
    "INSERT INTO session_exercises VALUES(14,'sxe_left_old',11,8,'sets',0,1,'none',30,1,3,NULL,NULL,NULL,'occurrence gauche');"
    "INSERT INTO performed_sets VALUES(17,13,0,3,NULL,NULL);"
    "INSERT INTO performed_sets VALUES(18,14,0,3,NULL,NULL);"
    "INSERT INTO exercise_feedback VALUES(19,'fb_old',13,'2026-09-01T10:01:00Z','stable');"
    "INSERT INTO exercise_feedback_revisions VALUES('fr_old','fb_old','2026-09-01T10:02:00Z','révisé');"
    "INSERT INTO session_followups VALUES(23,'fu_old',11,'2026-09-01T12:00:00Z','suivi');"
    "INSERT INTO session_followup_revisions VALUES('ur_old','fu_old','2026-09-01T12:01:00Z','suivi révisé');"
    "PRAGMA user_version=15;";

static bool scalar(sqlite3 *db, const char *sql, int expected)
{
    sqlite3_stmt *s = NULL; bool ok = false;
    if (sqlite3_prepare_v2(db, sql, -1, &s, NULL) == SQLITE_OK &&
        sqlite3_step(s) == SQLITE_ROW) ok = sqlite3_column_int(s, 0) == expected;
    (void)sqlite3_finalize(s); return ok;
}

#undef CHECK
#define CHECK(x) do { if (!(x)) { (void)fprintf(stderr, \
    "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #x); goto cleanup; } } while (0)

static bool run_test(void)
{
    char path[] = "/tmp/trainlog-v16-XXXXXX";
    int fd = mkstemp(path); sqlite3 *raw = NULL; TrainlogDatabase *db = NULL;
    bool ok = false;
    TrainlogPersistedExerciseDetail old[1] = {{0}};
    TrainlogSessionSummary summary;
    size_t count = 0U; int version = 0;
    TrainlogExercise changed;
    TrainlogSetInput new_set = {.duration_seconds = 30};
    TrainlogSessionExerciseInput input = {0};
    TrainlogSessionInput session = {0};
    char diagnostic[512] = {0};
    CHECK(fd >= 0); CHECK(close(fd) == 0); fd = -1;
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, FIXTURE, NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK); raw = NULL;
    if (trainlog_database_open_with_diagnostic(path, &db, diagnostic,
            sizeof(diagnostic)) != TRAINLOG_STATUS_OK) {
        (void)fprintf(stderr, "open diagnostic: %s\n", diagnostic);
        CHECK(false);
    }
    CHECK(trainlog_database_schema_version(db, &version) == TRAINLOG_STATUS_OK && version == 18);
    trainlog_database_close(db); db = NULL;
    CHECK(sqlite3_open_v2(path, &raw, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM exercises e LEFT JOIN exercise_profile_state s ON s.exercise_row_id=e.id WHERE s.revision_id IS NULL OR s.revision_id<>'pr_legacy_v1' OR s.legacy_seed<>1", 0));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM exercises e LEFT JOIN exercise_profile_revisions r ON r.exercise_row_id=e.id AND r.revision_id='pr_legacy_v1' AND r.legacy_seed=1 WHERE r.revision_id IS NULL", 0));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM pragma_table_info('session_exercises') WHERE name='tracking_mode' AND \"notnull\"=1", 1));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM pragma_table_info('session_exercises') WHERE name='tracking_mode' AND dflt_value IS NULL", 1));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM session_exercises WHERE id=13 AND entry_id='sxe_old' AND tracking_mode='reps'", 1));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM performed_sets WHERE id=17 AND reps=3 AND duration_seconds IS NULL", 1));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM exercise_feedback WHERE id=19 AND feedback_id='fb_old'", 1));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM exercise_feedback_revisions WHERE revision_id='fr_old'", 1));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM session_followups WHERE id=23", 1));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM session_followup_revisions WHERE revision_id='ur_old'", 1));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM pragma_table_info('ai_session_drafts') WHERE name='archive_status'", 1));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM pragma_table_info('ai_session_draft_entries') WHERE name='target_sets'", 1));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM pragma_table_info('ai_session_draft_imports') WHERE name='payload_sha256'", 1));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM ai_session_drafts", 0));
    CHECK(sqlite3_close(raw) == SQLITE_OK); raw = NULL;
    CHECK(trainlog_database_open(path, &db) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_update_exercise_profiled(db, "ex_b35fff35-9c97-4c82-9053-7d1f2a23d3ac", "Planche face sol",
        "planche face sol", TRAINLOG_TRACKING_DURATION, TRAINLOG_RECORDING_SETS, 0U,
        NULL, NULL, 0U) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_update_exercise_profiled(db, "ex_e32c40a6-72b4-4233-a0db-41b27e990c96", "Planche gauche sol",
        "planche gauche sol", TRAINLOG_TRACKING_DURATION, TRAINLOG_RECORDING_SETS, 0U,
        NULL, NULL, 0U) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_update_exercise_profiled(db, "ex_999fd503-ce89-49f9-979d-c19e6adbe80d", "Planche droite sol",
        "planche droite sol", TRAINLOG_TRACKING_DURATION, TRAINLOG_RECORDING_SETS, 0U,
        NULL, NULL, 0U) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_get_session_details(db, "se_old", &summary, old, 1U, &count) == TRAINLOG_STATUS_OK);
    CHECK(count == 1U && old[0].tracking_mode == TRAINLOG_TRACKING_REPS &&
        old[0].actual_set_count == 1U && old[0].actual_sets[0].reps == 3);
    trainlog_database_free_session_details(old, count);
    CHECK(trainlog_database_get_exercise_profile(db, "ex_b35fff35-9c97-4c82-9053-7d1f2a23d3ac", &changed) == TRAINLOG_STATUS_OK);
    (void)snprintf(input.entry_id, sizeof(input.entry_id), "sxe_new");
    (void)snprintf(input.exercise_id, sizeof(input.exercise_id), "ex_b35fff35-9c97-4c82-9053-7d1f2a23d3ac");
    input.recording_mode = changed.recording_mode; input.tracking_mode = changed.tracking_mode;
    input.load_mode = TRAINLOG_LOAD_NONE; input.sets = &new_set; input.set_count = 1U;
    (void)snprintf(session.session_id, sizeof(session.session_id), "se_new");
    (void)snprintf(session.started_at, sizeof(session.started_at), "2026-09-02T10:00:00Z");
    session.session_type = TRAINLOG_SESSION_TRAINING; session.exercises = &input; session.exercise_count = 1U;
    CHECK(trainlog_database_insert_session(db, &session) == TRAINLOG_STATUS_OK);
    trainlog_database_close(db); db = NULL;
    CHECK(sqlite3_open_v2(path, &raw, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM session_exercises se JOIN performed_sets ps ON ps.session_exercise_row_id=se.id WHERE se.entry_id='sxe_old' AND se.tracking_mode='reps' AND ps.reps=3 AND ps.duration_seconds IS NULL", 1));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM session_exercises se JOIN performed_sets ps ON ps.session_exercise_row_id=se.id WHERE se.entry_id='sxe_left_old' AND se.tracking_mode='reps' AND ps.reps=3 AND ps.duration_seconds IS NULL", 1));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM exercises WHERE exercise_id IN('ex_b35fff35-9c97-4c82-9053-7d1f2a23d3ac','ex_e32c40a6-72b4-4233-a0db-41b27e990c96','ex_999fd503-ce89-49f9-979d-c19e6adbe80d') AND recording_mode='sets' AND tracking_mode='duration' AND data_fields=0", 3));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM session_exercises se JOIN performed_sets ps ON ps.session_exercise_row_id=se.id WHERE se.entry_id='sxe_new' AND se.tracking_mode='duration' AND ps.reps IS NULL AND ps.duration_seconds=30", 1));
    CHECK(sqlite3_close(raw) == SQLITE_OK); raw = NULL;
    ok = true;
cleanup:
    if (raw != NULL) (void)sqlite3_close(raw);
    if (db != NULL) trainlog_database_close(db);
    if (fd >= 0) (void)close(fd);
    if (unlink(path) != 0) ok = false;
    return ok;
}

static bool rollback_on_invalid_backfill(void)
{
    char path[] = "/tmp/trainlog-v16-invalid-XXXXXX";
    int fd = mkstemp(path); sqlite3 *raw = NULL; TrainlogDatabase *db = NULL;
    bool ok = false;
    char diagnostic[512] = {0};
    CHECK(fd >= 0); CHECK(close(fd) == 0); fd = -1;
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, FIXTURE, NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "UPDATE exercises SET tracking_mode='invalid';",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK); raw = NULL;
    CHECK(trainlog_database_open_with_diagnostic(path, &db, diagnostic,
        sizeof(diagnostic)) == TRAINLOG_STATUS_DATABASE_ERROR);
    CHECK(db == NULL);
    CHECK(sqlite3_open_v2(path, &raw, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK);
    CHECK(scalar(raw, "SELECT user_version FROM pragma_user_version", 15));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM pragma_table_info('session_exercises') WHERE name='tracking_mode'", 0));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM session_exercises WHERE id=13 AND entry_id='sxe_old'", 1));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM performed_sets WHERE id=17 AND reps=3", 1));
    CHECK(sqlite3_close(raw) == SQLITE_OK); raw = NULL;
    ok = true;
cleanup:
    if (raw != NULL) (void)sqlite3_close(raw);
    if (db != NULL) trainlog_database_close(db);
    if (fd >= 0) (void)close(fd);
    if (unlink(path) != 0) ok = false;
    return ok;
}

static bool normalize_v17_prototype_in_place(void)
{
    char path[] = "/tmp/trainlog-v17-pr1-XXXXXX";
    int fd = mkstemp(path); sqlite3 *raw = NULL; TrainlogDatabase *db = NULL;
    bool ok = false;
    char diagnostic[512] = {0};
    CHECK(fd >= 0); CHECK(close(fd) == 0); fd = -1;
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, FIXTURE, NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK); raw = NULL;
    CHECK(trainlog_database_open(path, &db) == TRAINLOG_STATUS_OK);
    trainlog_database_close(db); db = NULL;
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw,
        "DELETE FROM exercise_profile_revisions WHERE exercise_row_id=9;"
        "UPDATE exercise_profile_state SET revision_id='pr1|sets|reps|0',parent_revision_id=NULL,legacy_seed=1 WHERE exercise_row_id=9;"
        "INSERT INTO exercise_profile_revisions VALUES(9,'pr1|sets|reps|0',NULL,'sets','reps',0,1);",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK); raw = NULL;
    CHECK(trainlog_database_open_with_diagnostic(path, &db, diagnostic,
        sizeof(diagnostic)) == TRAINLOG_STATUS_OK);
    trainlog_database_close(db); db = NULL;
    CHECK(sqlite3_open_v2(path, &raw, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM exercise_profile_revisions WHERE exercise_row_id=9", 2));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM exercise_profile_revisions WHERE exercise_row_id=9 AND revision_id='pr_legacy_v1' AND parent_revision_id IS NULL AND legacy_seed=1", 1));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM exercise_profile_state WHERE exercise_row_id=9 AND revision_id='pr2_4444be84-5f1c-58c9-9bd2-4093a8a4cf9b' AND parent_revision_id='pr_legacy_v1' AND legacy_seed=0", 1));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM exercise_profile_revisions WHERE revision_id GLOB 'pr1*'", 0));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM session_exercises WHERE id=13 AND tracking_mode='reps'", 1));
    CHECK(sqlite3_close(raw) == SQLITE_OK); raw = NULL;
    /* A second open is data-idempotent. */
    CHECK(trainlog_database_open(path, &db) == TRAINLOG_STATUS_OK);
    trainlog_database_close(db); db = NULL;
    CHECK(sqlite3_open_v2(path, &raw, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM exercise_profile_revisions WHERE exercise_row_id=9", 2));
    CHECK(sqlite3_exec(raw,
        "DELETE FROM exercise_profile_revisions WHERE exercise_row_id=9;"
        "UPDATE exercise_profile_state SET revision_id='pr1|sets|reps|00',parent_revision_id=NULL,legacy_seed=1 WHERE exercise_row_id=9;"
        "INSERT INTO exercise_profile_revisions VALUES(9,'pr1|sets|reps|00',NULL,'sets','reps',0,1);",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK); raw = NULL;
    CHECK(trainlog_database_open_with_diagnostic(path, &db, diagnostic,
        sizeof(diagnostic)) == TRAINLOG_STATUS_DATABASE_ERROR);
    CHECK(db == NULL);
    CHECK(sqlite3_open_v2(path, &raw, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK);
    CHECK(scalar(raw, "SELECT COUNT(*) FROM exercise_profile_state WHERE exercise_row_id=9 AND revision_id='pr1|sets|reps|00'", 1));
    CHECK(scalar(raw, "SELECT COUNT(*) FROM exercise_profile_revisions WHERE exercise_row_id=9 AND revision_id='pr1|sets|reps|00'", 1));
    CHECK(sqlite3_close(raw) == SQLITE_OK); raw = NULL;
    ok = true;
cleanup:
    if (raw != NULL) (void)sqlite3_close(raw);
    if (db != NULL) trainlog_database_close(db);
    if (fd >= 0) (void)close(fd);
    if (unlink(path) != 0) ok = false;
    return ok;
}

int main(void) { return run_test() && rollback_on_invalid_backfill() &&
    normalize_v17_prototype_in_place() ? 0 : 1; }
