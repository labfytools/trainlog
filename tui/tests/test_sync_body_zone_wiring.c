#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <sqlite3.h>

#include "trainlog/database.h"
#include "trainlog/mtp.h"
#include "trainlog/sync.h"
#include "trainlog/usb.h"

#define CHECK(value) do { \
    if (!(value)) { \
        (void)fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #value); \
        return false; \
    } \
} while (0)

static const char *const SOURCE_ID =
    "ex_43c7375f-934c-4650-930c-45807d2f2929";
static const char *const CANONICAL_ID =
    "ex_2d488c08-194c-4051-a3c9-34471646c1d3";
static const char *const EXERCISE_NAME = "Sync wiring exercise";
static char remote_root[4096];
static const char *remote_mobile_name = "trainlog-mobile-export-v2.json";
static unsigned int remote_mobile_version = 2U;

static bool copy_file(const char *source, const char *target)
{
    FILE *input = fopen(source, "rb");
    FILE *output;
    char buffer[4096];
    size_t count;
    bool copied = true;

    if (input == NULL) {
        return false;
    }
    output = fopen(target, "wb");
    if (output == NULL) {
        (void)fclose(input);
        return false;
    }
    while ((count = fread(buffer, 1U, sizeof(buffer), input)) != 0U) {
        if (fwrite(buffer, 1U, count, output) != count) {
            copied = false;
            break;
        }
    }
    if (ferror(input) != 0) {
        copied = false;
    }
    if (fclose(input) != 0) {
        copied = false;
    }
    if (fclose(output) != 0) {
        copied = false;
    }
    return copied;
}

TrainlogStatus trainlog_usb_list_mtp_devices(
    TrainlogUsbDevice *output,
    size_t capacity,
    size_t *output_count
)
{
    if (output_count == NULL || output == NULL || capacity < 1U) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    (void)memset(output, 0, sizeof(*output));
    output->bus_number = 1U;
    output->device_number = 2U;
    output->mtp = true;
    *output_count = 1U;
    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_mtp_list_storages(
    unsigned int bus_number,
    unsigned int device_number,
    TrainlogMtpStorage *output,
    size_t capacity,
    size_t *output_count
)
{
    (void)bus_number;
    (void)device_number;
    if (output_count == NULL || output == NULL || capacity < 1U) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    (void)memset(output, 0, sizeof(*output));
    output->storage_id = 3U;
    *output_count = 1U;
    return TRAINLOG_STATUS_OK;
}

static void set_entry(
    TrainlogMtpEntry *entry,
    uint32_t item_id,
    uint32_t parent_id,
    const char *name,
    bool folder
)
{
    (void)memset(entry, 0, sizeof(*entry));
    entry->item_id = item_id;
    entry->parent_id = parent_id;
    entry->storage_id = 3U;
    entry->folder = folder;
    entry->modification_unix_seconds = 1U;
    (void)snprintf(entry->name, sizeof(entry->name), "%s", name);
}

TrainlogStatus trainlog_mtp_list_folder(
    unsigned int bus_number,
    unsigned int device_number,
    uint32_t storage_id,
    uint32_t parent_folder_id,
    TrainlogMtpEntry *output,
    size_t capacity,
    size_t *output_count
)
{
    (void)bus_number;
    (void)device_number;
    (void)storage_id;
    if (output_count == NULL || output == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    if (parent_folder_id == UINT32_MAX) {
        if (capacity < 1U) {
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }
        set_entry(&output[0], 1U, UINT32_MAX, "Download", true);
        *output_count = 1U;
        return TRAINLOG_STATUS_OK;
    }
    if (parent_folder_id == 1U) {
        if (capacity < 1U) {
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }
        set_entry(&output[0], 2U, 1U, "Trainlog", true);
        *output_count = 1U;
        return TRAINLOG_STATUS_OK;
    }
    if (parent_folder_id == 2U) {
        if (capacity < 3U) {
            return TRAINLOG_STATUS_INVALID_ARGUMENT;
        }
        set_entry(&output[0], 10U, 2U, remote_mobile_name, false);
        set_entry(&output[1], 11U, 2U,
                  "trainlog-exercise-body-zones-v1.json", false);
        set_entry(&output[2], 12U, 2U,
                  "trainlog-equipment-associations-v2.json", false);
        *output_count = 3U;
        return TRAINLOG_STATUS_OK;
    }
    *output_count = 0U;
    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_mtp_receive_file(
    unsigned int bus_number,
    unsigned int device_number,
    uint32_t item_id,
    const char *local_path
)
{
    char source[4096];
    const char *name;
    int written;
    (void)bus_number;
    (void)device_number;

    name = item_id == 10U ? remote_mobile_name :
        item_id == 11U ? "trainlog-exercise-body-zones-v1.json" :
        item_id == 12U ? "trainlog-equipment-associations-v2.json" : NULL;
    if (name == NULL || local_path == NULL) {
        return TRAINLOG_STATUS_NOT_FOUND;
    }
    written = snprintf(source, sizeof(source), "%s/%s", remote_root, name);
    if (written < 0 || (size_t)written >= sizeof(source) ||
        !copy_file(source, local_path)) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_mtp_send_text_file(
    unsigned int bus_number,
    unsigned int device_number,
    uint32_t storage_id,
    uint32_t parent_folder_id,
    const char *local_path,
    const char *remote_filename,
    uint32_t *output_item_id
)
{
    (void)bus_number;
    (void)device_number;
    (void)storage_id;
    (void)parent_folder_id;
    {
        char target[4096];
        int written = snprintf(target, sizeof(target), "%s/%s", remote_root,
                               remote_filename);
        if (local_path == NULL || remote_filename == NULL || written < 0 ||
            (size_t)written >= sizeof(target) || !copy_file(local_path, target)) {
            return TRAINLOG_STATUS_SYSTEM_ERROR;
        }
    }
    if (output_item_id != NULL) {
        *output_item_id = 99U;
    }
    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_mtp_delete_object(
    unsigned int bus_number,
    unsigned int device_number,
    uint32_t item_id
)
{
    (void)bus_number;
    (void)device_number;
    (void)item_id;
    return TRAINLOG_STATUS_OK;
}

static bool write_artifacts(
    const char *mobile_id,
    const char *zone_id,
    bool historical_alias_data
)
{
    char path[4096];
    FILE *file;
    int written = snprintf(path, sizeof(path),
        "%s/%s", remote_root, remote_mobile_name);
    CHECK(written >= 0 && (size_t)written < sizeof(path));
    file = fopen(path, "wb");
    CHECK(file != NULL);
    if (historical_alias_data) {
        CHECK(fprintf(file,
            "{\"format\":\"trainlog-mobile-export\",\"version\":%u,"
            "\"generated_at\":\"2032-01-01T00:00:00+00:00\","
            "\"exercises\":["
            "{\"exercise_id\":\"%s\",\"name\":\"Retired sync wiring exercise\","
            "\"recording_mode\":\"sets\",\"tracking_mode\":\"reps\",\"data_fields\":0},"
            "{\"exercise_id\":\"%s\",\"name\":\"%s\","
            "\"recording_mode\":\"sets\",\"tracking_mode\":\"reps\",\"data_fields\":0}],"
            "\"sessions\":[{\"session_id\":\"se_alias_sync_fixture\","
            "\"started_at\":\"2032-01-01T01:00:00+00:00\",\"session_type\":\"training\","
            "\"exercises\":["
            "{\"entry_id\":\"sxe_alias_historical_a\",\"position\":0,"
            "\"exercise_id\":\"%s\",\"name\":\"Retired sync wiring exercise\","
            "\"recording_mode\":\"sets\",\"tracking_mode\":\"reps\",\"data_fields\":0,"
            "\"load_mode\":\"none\",\"rest_seconds\":0,\"target\":null,"
            "\"equipment_id\":null,\"sets\":[{\"reps\":8}]},"
            "{\"entry_id\":\"sxe_alias_historical_b\",\"position\":1,"
            "\"exercise_id\":\"%s\",\"name\":\"%s\","
            "\"recording_mode\":\"sets\",\"tracking_mode\":\"reps\",\"data_fields\":0,"
            "\"load_mode\":\"none\",\"rest_seconds\":0,\"target\":null,"
            "\"equipment_id\":null,\"sets\":[{\"reps\":9}]}]}],"
            "\"body_observations\":[]}",
            remote_mobile_version, SOURCE_ID, CANONICAL_ID, EXERCISE_NAME,
            SOURCE_ID, CANONICAL_ID, EXERCISE_NAME) > 0);
    } else {
        CHECK(fprintf(file,
            "{\"format\":\"trainlog-mobile-export\",\"version\":%u,"
            "\"generated_at\":\"2032-01-01T00:00:00+00:00\","
            "\"exercises\":[{\"exercise_id\":\"%s\",\"name\":\"%s\","
            "\"recording_mode\":\"sets\",\"tracking_mode\":\"reps\","
            "\"data_fields\":0}],\"sessions\":[],\"body_observations\":[]}",
            remote_mobile_version, mobile_id, EXERCISE_NAME) > 0);
    }
    CHECK(fclose(file) == 0);

    written = snprintf(path, sizeof(path),
        "%s/trainlog-exercise-body-zones-v1.json", remote_root);
    CHECK(written >= 0 && (size_t)written < sizeof(path));
    file = fopen(path, "wb");
    CHECK(file != NULL);
    CHECK(fprintf(file,
        "{\"format\":\"trainlog-exercise-body-zones\",\"version\":1,"
        "\"generated_at\":\"2032-01-01T00:00:00+00:00\","
        "\"exercises\":[{\"exercise_id\":\"%s\","
        "\"primary_zone_id\":\"back\",\"secondary_zone_ids\":[\"arms\"]},"
        "{\"exercise_id\":\"%s\",\"primary_zone_id\":\"back\","
        "\"secondary_zone_ids\":[\"arms\"]}]}",
        zone_id, CANONICAL_ID) > 0);
    CHECK(fclose(file) == 0);

    written = snprintf(path, sizeof(path),
        "%s/trainlog-equipment-associations-v2.json", remote_root);
    CHECK(written >= 0 && (size_t)written < sizeof(path));
    file = fopen(path, "wb");
    CHECK(file != NULL);
    CHECK(fprintf(file,
        "{\"format\":\"trainlog-equipment-associations\",\"version\":2,"
        "\"generated_at\":\"2032-01-01T00:00:00+00:00\",\"associations\":%s}",
        historical_alias_data
            ? "[{\"session_id\":\"se_alias_sync_fixture\",\"entry_id\":\"sxe_alias_historical_a\",\"exercise_id\":\"ex_43c7375f-934c-4650-930c-45807d2f2929\",\"state\":\"cleared\"},{\"session_id\":\"se_alias_sync_fixture\",\"entry_id\":\"sxe_alias_historical_b\",\"exercise_id\":\"ex_2d488c08-194c-4051-a3c9-34471646c1d3\",\"state\":\"cleared\"}]"
            : "[]") > 0);
    CHECK(fclose(file) == 0);
    return true;
}

static bool prepare_database(
    const char *case_root,
    bool persistent_alias,
    bool initial_zone,
    char *database_path,
    size_t database_path_size
)
{
    TrainlogDatabase *database = NULL;
    const char *no_secondary[1] = {NULL};
    char data_directory[4096];
    int written;

    CHECK(mkdir(case_root, 0700) == 0);
    CHECK(setenv("XDG_DATA_HOME", case_root, 1) == 0);
    written = snprintf(data_directory, sizeof(data_directory),
                       "%s/trainlog", case_root);
    CHECK(written >= 0 && (size_t)written < sizeof(data_directory));
    CHECK(mkdir(data_directory, 0700) == 0);
    written = snprintf(database_path, database_path_size,
                       "%s/trainlog.db", data_directory);
    CHECK(written >= 0 && (size_t)written < database_path_size);
    CHECK(trainlog_database_open(database_path, &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(
        database, CANONICAL_ID, EXERCISE_NAME, "sync wiring exercise",
        TRAINLOG_TRACKING_REPS, TRAINLOG_RECORDING_SETS,
        UINT32_C(0)) == TRAINLOG_STATUS_OK);
    if (initial_zone) {
        CHECK(trainlog_database_replace_exercise_body_zones(
            database, CANONICAL_ID, "chest", no_secondary, 0U) ==
            TRAINLOG_STATUS_OK);
    }
    if (persistent_alias) {
        CHECK(trainlog_database_insert_exercise_profiled(
            database, SOURCE_ID, "Retired sync wiring exercise",
            "retired sync wiring exercise", TRAINLOG_TRACKING_REPS,
            TRAINLOG_RECORDING_SETS, UINT32_C(0)) ==
            TRAINLOG_STATUS_OK);
        CHECK(trainlog_database_merge_exercises(
            database, SOURCE_ID, CANONICAL_ID) == TRAINLOG_STATUS_OK);
    }
    trainlog_database_close(database);
    return true;
}

static bool database_state(
    const char *database_path,
    bool source_should_resolve,
    const char *expected_primary,
    const char *expected_secondary,
    size_t expected_zone_count
)
{
    TrainlogDatabase *database = NULL;
    TrainlogExerciseBodyZone zones[4];
    char resolved[TRAINLOG_ID_MAX + 1U];
    size_t count = 0U;
    size_t index;
    bool found_primary = false;
    bool found_secondary = expected_secondary == NULL;
    sqlite3 *raw = NULL;
    sqlite3_stmt *statement = NULL;

    CHECK(trainlog_database_open(database_path, &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_resolve_exercise_id(
        database, SOURCE_ID, resolved, sizeof(resolved)) ==
        (source_should_resolve ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_NOT_FOUND));
    if (source_should_resolve) {
        CHECK(strcmp(resolved, CANONICAL_ID) == 0);
    }
    CHECK(trainlog_database_list_exercise_body_zones(
        database, CANONICAL_ID, zones, 4U, &count) == TRAINLOG_STATUS_OK);
    CHECK(count == expected_zone_count);
    for (index = 0U; index < count; ++index) {
        if (zones[index].role == TRAINLOG_BODY_ZONE_PRIMARY &&
            strcmp(zones[index].zone_id, expected_primary) == 0) {
            found_primary = true;
        }
        if (expected_secondary != NULL &&
            zones[index].role == TRAINLOG_BODY_ZONE_SECONDARY &&
            strcmp(zones[index].zone_id, expected_secondary) == 0) {
            found_secondary = true;
        }
    }
    CHECK(found_primary && found_secondary);
    trainlog_database_close(database);

    /* INVARIANT: alias coalescing updates one canonical row; it never
     * resurrects the retired creator or duplicates mappings/aliases. */
    CHECK(sqlite3_open(database_path, &raw) == SQLITE_OK);
    CHECK(sqlite3_prepare_v2(raw, "SELECT COUNT(*) FROM exercises", -1,
                             &statement, NULL) == SQLITE_OK);
    CHECK(sqlite3_step(statement) == SQLITE_ROW);
    CHECK(sqlite3_column_int(statement, 0) == 6);
    CHECK(sqlite3_finalize(statement) == SQLITE_OK);
    statement = NULL;
    if (source_should_resolve) {
        CHECK(sqlite3_prepare_v2(raw,
            "SELECT COUNT(*) FROM exercise_aliases WHERE source_exercise_id=?1 "
            "AND canonical_exercise_id=?2", -1, &statement, NULL) == SQLITE_OK);
        CHECK(sqlite3_bind_text(statement, 1, SOURCE_ID, -1, SQLITE_STATIC) == SQLITE_OK);
        CHECK(sqlite3_bind_text(statement, 2, CANONICAL_ID, -1, SQLITE_STATIC) == SQLITE_OK);
        CHECK(sqlite3_step(statement) == SQLITE_ROW);
        CHECK(sqlite3_column_int(statement, 0) == 1);
        CHECK(sqlite3_finalize(statement) == SQLITE_OK);
    }
    CHECK(sqlite3_close(raw) == SQLITE_OK);
    return true;
}

static bool successful_journal(const char *case_root, const char *sync_id)
{
    char path[4096];
    char content[8192];
    FILE *file;
    size_t count;
    int written = snprintf(path, sizeof(path), "%s/trainlog/sync_runs/%s.json",
                           case_root, sync_id);
    CHECK(written >= 0 && (size_t)written < sizeof(path));
    file = fopen(path, "rb");
    CHECK(file != NULL);
    count = fread(content, 1U, sizeof(content) - 1U, file);
    CHECK(ferror(file) == 0);
    CHECK(fclose(file) == 0);
    content[count] = '\0';
    CHECK(strstr(content, "\"status\":\"success\"") != NULL);
    return true;
}

static bool file_contains(const char *name, const char *needle, bool expected)
{
    char path[4096];
    char content[65536];
    FILE *file;
    size_t count;
    int written = snprintf(path, sizeof(path), "%s/%s", remote_root, name);
    CHECK(written >= 0 && (size_t)written < sizeof(path));
    file = fopen(path, "rb");
    CHECK(file != NULL);
    count = fread(content, 1U, sizeof(content) - 1U, file);
    CHECK(ferror(file) == 0);
    CHECK(fclose(file) == 0);
    content[count] = '\0';
    CHECK((strstr(content, needle) != NULL) == expected);
    return true;
}

static bool canonical_alias_exports(void)
{
    /* CONTRACT: retired identities are published only by the alias companion;
     * every catalogue, session, zone, and equipment reference uses live B. */
    static const char *const canonical_outputs[] = {
        "trainlog-pc-catalog-v1.json",
        "trainlog-pc-mobile-export-v3.json",
        "trainlog-exercise-body-zones-v1.json",
        "trainlog-equipment-associations-v2.json",
    };
    size_t index;
    for (index = 0U;
         index < sizeof(canonical_outputs) / sizeof(canonical_outputs[0]);
         ++index) {
        CHECK(file_contains(canonical_outputs[index], SOURCE_ID, false));
        CHECK(file_contains(canonical_outputs[index], CANONICAL_ID, true));
    }
    CHECK(file_contains("trainlog-exercise-aliases-v1.json", SOURCE_ID, true));
    CHECK(file_contains("trainlog-exercise-aliases-v1.json", CANONICAL_ID, true));
    return true;
}

static bool run_success_case(
    const char *case_root,
    bool persistent_alias
)
{
    TrainlogSyncReport report;
    char database_path[4096];
    if (persistent_alias) {
        remote_mobile_name = "trainlog-mobile-export-v3.json";
        remote_mobile_version = 3U;
    }
    CHECK(prepare_database(case_root, persistent_alias, false,
                           database_path, sizeof(database_path)));
    CHECK(write_artifacts(SOURCE_ID, SOURCE_ID, persistent_alias));
    {
        TrainlogStatus status = trainlog_sync_run(TRAINLOG_SYNC_TRIGGER_TUI, false,
            persistent_alias ? TRAINLOG_SYNC_BIDIRECTIONAL :
                TRAINLOG_SYNC_ANDROID_TO_PC, &report);
        if (status != TRAINLOG_STATUS_OK) {
            (void)fprintf(stderr, "sync failure: %s\n", report.error);
        }
        CHECK(status == TRAINLOG_STATUS_OK);
    }
    CHECK(report.success);
    CHECK(database_state(database_path, persistent_alias, "back", "arms", 2U));
    CHECK(successful_journal(case_root, report.sync_id));
    if (persistent_alias) {
        sqlite3 *raw = NULL;
        sqlite3_stmt *statement = NULL;
        CHECK(sqlite3_open(database_path, &raw) == SQLITE_OK);
        CHECK(sqlite3_prepare_v2(raw,
            "SELECT COUNT(*) FROM session_exercises se JOIN exercises e "
            "ON e.id=se.exercise_row_id WHERE se.entry_id IN "
            "('sxe_alias_historical_a','sxe_alias_historical_b') "
            "AND e.exercise_id=?1", -1, &statement, NULL) == SQLITE_OK);
        CHECK(sqlite3_bind_text(statement, 1, CANONICAL_ID, -1,
                                SQLITE_STATIC) == SQLITE_OK);
        CHECK(sqlite3_step(statement) == SQLITE_ROW);
        CHECK(sqlite3_column_int(statement, 0) == 2);
        CHECK(sqlite3_finalize(statement) == SQLITE_OK);
        CHECK(sqlite3_close(raw) == SQLITE_OK);
        CHECK(canonical_alias_exports());
    }

    /* Replay exercises the same production argv path and sync baseline. */
    CHECK(trainlog_sync_run(TRAINLOG_SYNC_TRIGGER_TUI, false,
        persistent_alias ? TRAINLOG_SYNC_BIDIRECTIONAL :
            TRAINLOG_SYNC_ANDROID_TO_PC, &report) == TRAINLOG_STATUS_OK);
    CHECK(report.success);
    CHECK(database_state(database_path, persistent_alias, "back", "arms", 2U));
    CHECK(successful_journal(case_root, report.sync_id));
    if (persistent_alias) {
        CHECK(canonical_alias_exports());
        remote_mobile_name = "trainlog-mobile-export-v2.json";
        remote_mobile_version = 2U;
    }
    return true;
}

static bool run_unknown_without_proof_case(const char *case_root)
{
    TrainlogSyncReport report;
    char database_path[4096];
    CHECK(prepare_database(case_root, false, true,
                           database_path, sizeof(database_path)));
    CHECK(write_artifacts(CANONICAL_ID, SOURCE_ID, false));
    CHECK(trainlog_sync_run(TRAINLOG_SYNC_TRIGGER_TUI, false,
        TRAINLOG_SYNC_ANDROID_TO_PC, &report) ==
        TRAINLOG_STATUS_DATABASE_ERROR);
    CHECK(!report.success);
    CHECK(strstr(report.error, "exercice inconnu") != NULL);
    CHECK(database_state(database_path, false, "chest", NULL, 1U));
    return true;
}

static bool run_v3_unknown_with_stale_v2_proof_case(const char *case_root)
{
    TrainlogSyncReport report;
    char database_path[4096];
    FILE *stale;

    /* This file deliberately survives from an earlier V2 run.  A selected V3
     * cycle must not discover it as reconciliation evidence for its companion. */
    stale = fopen("/tmp/trainlog-mobile-export-v2.json", "wb");
    CHECK(stale != NULL);
    CHECK(fprintf(stale,
        "{\"format\":\"trainlog-mobile-export\",\"version\":2,"
        "\"generated_at\":\"2032-01-01T00:00:00+00:00\","
        "\"exercises\":[{\"exercise_id\":\"%s\",\"name\":\"%s\","
        "\"recording_mode\":\"sets\",\"tracking_mode\":\"reps\","
        "\"data_fields\":0}],\"sessions\":[],\"body_observations\":[]}",
        SOURCE_ID, EXERCISE_NAME) > 0);
    CHECK(fclose(stale) == 0);
    remote_mobile_name = "trainlog-mobile-export-v3.json";
    remote_mobile_version = 3U;
    CHECK(prepare_database(case_root, false, true,
                           database_path, sizeof(database_path)));
    CHECK(write_artifacts(CANONICAL_ID, SOURCE_ID, false));
    CHECK(trainlog_sync_run(TRAINLOG_SYNC_TRIGGER_TUI, false,
        TRAINLOG_SYNC_ANDROID_TO_PC, &report) ==
        TRAINLOG_STATUS_DATABASE_ERROR);
    CHECK(!report.success);
    CHECK(strstr(report.error, "exercice inconnu") != NULL);
    CHECK(database_state(database_path, false, "chest", NULL, 1U));
    CHECK(remove("/tmp/trainlog-mobile-export-v2.json") == 0);
    remote_mobile_name = "trainlog-mobile-export-v2.json";
    remote_mobile_version = 2U;
    return true;
}

static bool run_all(void)
{
    char temporary[] = "/tmp/trainlog-sync-body-zone-wiring-XXXXXX";
    char case_a[4096];
    char case_b[4096];
    char case_c[4096];
    char case_d[4096];
    char *root = mkdtemp(temporary);

    CHECK(root != NULL);
    CHECK(snprintf(remote_root, sizeof(remote_root), "%s/remote", root) > 0);
    CHECK(mkdir(remote_root, 0700) == 0);
    CHECK(snprintf(case_a, sizeof(case_a), "%s/case-a", root) > 0);
    CHECK(snprintf(case_b, sizeof(case_b), "%s/case-b", root) > 0);
    CHECK(snprintf(case_c, sizeof(case_c), "%s/case-c", root) > 0);
    CHECK(snprintf(case_d, sizeof(case_d), "%s/case-d", root) > 0);

    CHECK(run_success_case(case_a, false));
    CHECK(run_success_case(case_b, true));
    CHECK(run_unknown_without_proof_case(case_c));
    CHECK(run_v3_unknown_with_stale_v2_proof_case(case_d));
    (void)puts("PASS production sync body-zone V2 proof wiring");
    return true;
}

int main(void)
{
    return run_all() ? EXIT_SUCCESS : EXIT_FAILURE;
}
