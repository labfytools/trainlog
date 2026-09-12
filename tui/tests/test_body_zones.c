#include <sqlite3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "trainlog/body_zone_catalog.h"
#include "trainlog/catalog.h"
#include "trainlog/database.h"

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return 1; } } while (0)

static int taxonomy_contract(void)
{
    size_t index;
    CHECK(trainlog_body_zone_catalog_count() == 11U);
    for (index = 0U; index < trainlog_body_zone_catalog_count(); ++index) {
        const TrainlogBodyZone *zone = trainlog_body_zone_catalog_at(index);
        const TrainlogBodyZone *ancestors[11];
        size_t other;
        CHECK(zone != NULL);
        CHECK(trainlog_body_zone_catalog_lookup(zone->zone_id) == zone);
        CHECK(trainlog_body_zone_catalog_ancestors(zone->zone_id, ancestors, 11U) <= 2U);
        if (zone->parent_zone_id != NULL)
            CHECK(trainlog_body_zone_catalog_lookup(zone->parent_zone_id) != NULL);
        for (other = index + 1U; other < trainlog_body_zone_catalog_count(); ++other) {
            const TrainlogBodyZone *candidate = trainlog_body_zone_catalog_at(other);
            CHECK(candidate != NULL);
            CHECK(strcmp(zone->zone_id, candidate->zone_id) != 0);
            CHECK(zone->sort_order != candidate->sort_order);
        }
    }
    CHECK(trainlog_body_zone_catalog_lookup("full_body")->parent_zone_id == NULL);
    CHECK(trainlog_body_zone_catalog_lookup("upper_body")->is_group);
    CHECK(trainlog_body_zone_catalog_lookup("lower_body")->is_group);
    CHECK(trainlog_body_zone_catalog_is_descendant("chest", "upper_body"));
    CHECK(!trainlog_body_zone_catalog_is_descendant("core", "upper_body"));
    return 0;
}

static int relation_and_filter_contract(void)
{
    char path[] = "/tmp/trainlog-body-zones-XXXXXX";
    int fd = mkstemp(path);
    sqlite3 *raw = NULL;
    TrainlogDatabase *database = NULL;
    TrainlogExercise created;
    TrainlogExercise lower;
    TrainlogExercise results[8];
    TrainlogExerciseBodyZone relations[4];
    const char *secondary[] = {"shoulders", "arms"};
    const char *duplicate[] = {"arms", "arms"};
    const char *same_as_primary[] = {"chest"};
    const char *orphan_secondary[] = {"arms"};
    size_t count = 0U;
    CHECK(fd >= 0);
    CHECK(close(fd) == 0);
    CHECK(trainlog_database_open(path, &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_catalog_create_exercise_profiled_with_zones(database,
        "Chest custom", TRAINLOG_TRACKING_REPS, TRAINLOG_RECORDING_SETS, 0U,
        "chest", secondary, 2U, &created) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_list_exercise_body_zones(database, created.exercise_id,
        relations, 4U, &count) == TRAINLOG_STATUS_OK);
    CHECK(count == 3U);
    CHECK(trainlog_database_list_exercise_body_zones(database, "ex_missing",
        relations, 4U, &count) == TRAINLOG_STATUS_NOT_FOUND);
    CHECK(relations[0].role == TRAINLOG_BODY_ZONE_PRIMARY);
    CHECK(strcmp(relations[0].zone_id, "chest") == 0);
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw,
        "INSERT INTO exercise_body_zones(exercise_row_id,zone_id,role) "
        "SELECT id,'back','primary' FROM exercises WHERE exercise_id LIKE 'ex_%';",
        NULL, NULL, NULL) == SQLITE_CONSTRAINT);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    raw = NULL;
    CHECK(trainlog_database_replace_exercise_body_zones(database, created.exercise_id,
        "chest", same_as_primary, 1U) == TRAINLOG_STATUS_INVALID_ARGUMENT);
    CHECK(trainlog_database_replace_exercise_body_zones(database, created.exercise_id,
        "chest", duplicate, 2U) == TRAINLOG_STATUS_INVALID_ARGUMENT);
    CHECK(trainlog_database_replace_exercise_body_zones(database, created.exercise_id,
        NULL, orphan_secondary, 1U) == TRAINLOG_STATUS_INVALID_ARGUMENT);
    CHECK(trainlog_database_replace_exercise_body_zones(database, created.exercise_id,
        "upper_body", NULL, 0U) == TRAINLOG_STATUS_INVALID_ARGUMENT);
    CHECK(trainlog_database_replace_exercise_body_zones(database, created.exercise_id,
        "unknown", NULL, 0U) == TRAINLOG_STATUS_INVALID_ARGUMENT);
    CHECK(trainlog_database_list_exercises_filtered(database, "chest", "upper_body",
        true, false, false, results, 8U, &count) == TRAINLOG_STATUS_OK);
    CHECK(count == 1U && strcmp(results[0].exercise_id, created.exercise_id) == 0);
    CHECK(trainlog_database_list_exercises_filtered(database, "", "chest",
        false, false, false, results, 8U, &count) == TRAINLOG_STATUS_OK && count == 3U);
    CHECK(trainlog_database_list_exercises_filtered(database, "", "arms",
        false, false, false, results, 8U, &count) == TRAINLOG_STATUS_OK && count == 3U);
    CHECK(trainlog_database_list_exercises_filtered(database, "", "arms",
        false, true, false, results, 8U, &count) == TRAINLOG_STATUS_OK && count == 1U);
    CHECK(trainlog_database_list_exercises_filtered(database, "chest", "back",
        true, false, false, results, 8U, &count) == TRAINLOG_STATUS_OK && count == 0U);
    CHECK(trainlog_database_list_exercises_filtered(database, "", "back",
        true, false, true, results, 8U, &count) == TRAINLOG_STATUS_INVALID_ARGUMENT);
    CHECK(trainlog_catalog_create_exercise_profiled_with_zones(database,
        "Glute custom", TRAINLOG_TRACKING_REPS, TRAINLOG_RECORDING_SETS, 0U,
        "glutes", NULL, 0U, &lower) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_catalog_create_exercise_profiled_with_zones(database,
        "Thigh custom", TRAINLOG_TRACKING_REPS, TRAINLOG_RECORDING_SETS, 0U,
        "thighs", NULL, 0U, &lower) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_catalog_create_exercise_profiled_with_zones(database,
        "Calf custom", TRAINLOG_TRACKING_REPS, TRAINLOG_RECORDING_SETS, 0U,
        "calves", NULL, 0U, &lower) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_list_exercises_filtered(database, "", "lower_body",
        true, false, false, results, 8U, &count) == TRAINLOG_STATUS_OK && count == 3U);
    CHECK(trainlog_database_update_exercise_profiled(database, created.exercise_id,
        "Chest custom renamed", "chest custom renamed", TRAINLOG_TRACKING_REPS,
        TRAINLOG_RECORDING_SETS, 0U, "back", NULL, 0U) ==
        TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_list_exercise_body_zones(database, created.exercise_id,
        relations, 4U, &count) == TRAINLOG_STATUS_OK && count == 1U &&
        strcmp(relations[0].zone_id, "back") == 0);
    CHECK(trainlog_database_replace_exercise_body_zones(database, created.exercise_id,
        NULL, NULL, 0U) == TRAINLOG_STATUS_OK);
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    /* Inject against the real row with a prepared statement so the corruption
     * fixture never embeds its generated UUID. */
    {
        sqlite3_stmt *corrupt = NULL;
        CHECK(sqlite3_prepare_v2(raw,
            "INSERT INTO exercise_body_zones(exercise_row_id,zone_id,role) "
            "SELECT id,'arms','secondary' FROM exercises WHERE exercise_id=?1;",
            -1, &corrupt, NULL) == SQLITE_OK);
        CHECK(sqlite3_bind_text(corrupt, 1, created.exercise_id, -1,
            SQLITE_TRANSIENT) == SQLITE_OK);
        CHECK(sqlite3_step(corrupt) == SQLITE_DONE);
        CHECK(sqlite3_finalize(corrupt) == SQLITE_OK);
    }
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    raw = NULL;
    CHECK(trainlog_database_list_exercise_body_zones(database, created.exercise_id,
        relations, 4U, &count) == TRAINLOG_STATUS_DATABASE_ERROR);
    CHECK(trainlog_database_list_exercises_filtered(database, "", "chest",
        false, false, false, results, 8U, &count) == TRAINLOG_STATUS_DATABASE_ERROR);
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "DELETE FROM exercise_body_zones WHERE role='secondary';",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_exec(raw,
        "INSERT INTO exercise_body_zones(exercise_row_id,zone_id,role) "
        "SELECT id,'upper_body','primary' FROM exercises "
        "WHERE normalized_name='chest custom renamed';",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    raw = NULL;
    CHECK(trainlog_database_list_exercise_body_zones(database, created.exercise_id,
        relations, 4U, &count) == TRAINLOG_STATUS_DATABASE_ERROR);
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw, "DELETE FROM exercise_body_zones WHERE zone_id='upper_body';",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    raw = NULL;
    CHECK(trainlog_database_list_exercises_filtered(database, "", NULL,
        true, false, true, results, 8U, &count) == TRAINLOG_STATUS_OK);
    CHECK(count == 3U);
    trainlog_database_close(database);
    database = NULL;
    CHECK(trainlog_database_open(path, &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_list_exercise_body_zones(database, created.exercise_id,
        relations, 4U, &count) == TRAINLOG_STATUS_OK && count == 0U);
    trainlog_database_close(database);
    CHECK(unlink(path) == 0);
    return 0;
}

static int migration_preserves_identity_and_history(void)
{
    char path[] = "/tmp/trainlog-body-zones-v10-XXXXXX";
    int fd = mkstemp(path);
    sqlite3 *raw = NULL;
    TrainlogDatabase *database = NULL;
    TrainlogExerciseBodyZone relations[4];
    size_t count = 0U;
    int version = 0;
    CHECK(fd >= 0);
    CHECK(close(fd) == 0);
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    CHECK(sqlite3_exec(raw,
        "PRAGMA foreign_keys=ON;"
        "CREATE TABLE exercises(id INTEGER PRIMARY KEY,exercise_id TEXT NOT NULL UNIQUE,"
        "name TEXT NOT NULL,normalized_name TEXT NOT NULL UNIQUE,tracking_mode TEXT NOT NULL,"
        "recording_mode TEXT NOT NULL,data_fields INTEGER NOT NULL);"
        "CREATE TABLE sessions(id INTEGER PRIMARY KEY,session_id TEXT NOT NULL UNIQUE);"
        "CREATE TABLE session_exercises(id INTEGER PRIMARY KEY,exercise_row_id INTEGER,entry_id TEXT,equipment_id TEXT);"
        "INSERT INTO exercises VALUES(7,'ex_b432623f-bfe9-4daf-a653-60ec7fdffbde',"
        "'Leg press','leg press','reps','sets',0);"
        "INSERT INTO sessions VALUES(3,'se_preserved');"
        "PRAGMA user_version=10;", NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    raw = NULL;
    CHECK(trainlog_database_open(path, &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_schema_version(database, &version) == TRAINLOG_STATUS_OK && version == 15);
    CHECK(trainlog_database_list_exercise_body_zones(database,
        "ex_b432623f-bfe9-4daf-a653-60ec7fdffbde", relations, 4U, &count) == TRAINLOG_STATUS_OK);
    CHECK(count == 2U);
    trainlog_database_close(database);
    CHECK(sqlite3_open(path, &raw) == SQLITE_OK);
    {
        sqlite3_stmt *statement = NULL;
        CHECK(sqlite3_prepare_v2(raw,
            "SELECT (SELECT COUNT(*) FROM exercises WHERE id=7 AND "
            "exercise_id='ex_b432623f-bfe9-4daf-a653-60ec7fdffbde'),"
            "(SELECT COUNT(*) FROM sessions WHERE id=3 AND session_id='se_preserved');",
            -1, &statement, NULL) == SQLITE_OK);
        CHECK(sqlite3_step(statement) == SQLITE_ROW);
        CHECK(sqlite3_column_int(statement, 0) == 1);
        CHECK(sqlite3_column_int(statement, 1) == 1);
        CHECK(sqlite3_finalize(statement) == SQLITE_OK);
        statement = NULL;
        CHECK(sqlite3_prepare_v2(raw, "PRAGMA foreign_key_check;", -1,
            &statement, NULL) == SQLITE_OK);
        CHECK(sqlite3_step(statement) == SQLITE_DONE);
        CHECK(sqlite3_finalize(statement) == SQLITE_OK);
    }
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    CHECK(unlink(path) == 0);
    return 0;
}

int main(void)
{
    CHECK(taxonomy_contract() == 0);
    CHECK(relation_and_filter_contract() == 0);
    CHECK(migration_preserves_identity_and_history() == 0);
    return 0;
}
