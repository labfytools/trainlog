#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "trainlog/sync_history.h"

#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL line %d\n", __LINE__); return 1; } } while (0)

static bool read_text(
    const char *path,
    char *output,
    size_t capacity
)
{
    FILE *file;
    size_t used;

    file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    used = fread(output, 1U, capacity - 1U, file);
    output[used] = '\0';
    return fclose(file) == 0;
}

static void initialize_failure(
    TrainlogSyncReport *report,
    const char *sync_id,
    TrainlogSyncDirection direction,
    const char *error
)
{
    (void)memset(report, 0, sizeof(*report));
    report->direction = direction;
    (void)snprintf(report->sync_id, sizeof(report->sync_id), "%s", sync_id);
    (void)snprintf(
        report->started_at,
        sizeof(report->started_at),
        "%s",
        "2026-01-02T03:04:05+00:00"
    );
    (void)snprintf(report->error, sizeof(report->error), "%s", error);
    (void)snprintf(report->summary, sizeof(report->summary), "%s", error);
}

int main(void)
{
    char temporary[] = "/tmp/trainlog-sync-history-XXXXXX";
    char history_path[512];
    char run_path[512];
    char text[4096];
    char line[768];
    const TrainlogSyncDirection directions[] = {
        TRAINLOG_SYNC_ANDROID_TO_PC,
        TRAINLOG_SYNC_PC_TO_ANDROID,
        TRAINLOG_SYNC_BIDIRECTIONAL
    };
    const char *const ids[] = {
        "sy_00000000-0000-4000-8000-000000000001",
        "sy_00000000-0000-4000-8000-000000000002",
        "sy_00000000-0000-4000-8000-000000000003"
    };
    const char *const errors[] = {
        "Échec neutre sans indication de sens.",
        "Échec neutre sans indication de sens.",
        "Android→PC indisponible; PC→Android non tenté."
    };
    const char *const json_directions[] = {
        "\"direction\":\"android_to_pc\"",
        "\"direction\":\"pc_to_android\"",
        "\"direction\":\"bidirectional\""
    };
    const char *const detail_directions[] = {
        "Direction   : Android→PC",
        "Direction   : PC→Android",
        "Direction   : PC↔Android"
    };
    FILE *history;
    TrainlogSyncHistoryEntry entry;
    TrainlogSyncReport report;
    size_t index;

    CHECK(trainlog_sync_history_parse_line(
        "sy_legacy\t02/01/2026 03:04\t0\tPC→Android visible seulement dans le résumé\n",
        &entry
    ));
    CHECK(!entry.direction_known);
    CHECK(strcmp(trainlog_sync_history_direction_label(&entry), "direction inconnue") == 0);
    CHECK(trainlog_sync_history_parse_line(
        "02/01/2026 03:04\t1\tAndroid→PC ancien\n",
        &entry
    ));
    CHECK(!entry.direction_known && entry.sync_id[0] == '\0');
    CHECK(!trainlog_sync_history_parse_line(
        "sy_bad\t02/01/2026 03:04\t0\tx\téchec\n",
        &entry
    ));
    CHECK(!trainlog_sync_history_parse_line(
        "sy_bad\t02/01/2026 03:04\t0\ta\téchec\textra\n",
        &entry
    ));

    CHECK(mkdtemp(temporary) != NULL);
    CHECK(setenv("XDG_DATA_HOME", temporary, 1) == 0);

    for (index = 0U; index < 3U; ++index) {
        initialize_failure(&report, ids[index], directions[index], errors[index]);
        CHECK(trainlog_sync_record_local_run(
            TRAINLOG_SYNC_TRIGGER_TUI,
            &report
        ));

        CHECK(snprintf(
            run_path,
            sizeof(run_path),
            "%s/trainlog/sync_runs/%s.json",
            temporary,
            ids[index]
        ) > 0);
        CHECK(read_text(run_path, text, sizeof(text)));
        CHECK(strstr(text, json_directions[index]) != NULL);
        CHECK(strstr(text, errors[index]) != NULL);

        CHECK(snprintf(
            run_path,
            sizeof(run_path),
            "%s/trainlog/sync_runs/%s.txt",
            temporary,
            ids[index]
        ) > 0);
        CHECK(read_text(run_path, text, sizeof(text)));
        CHECK(strstr(text, detail_directions[index]) != NULL);
        CHECK(strstr(text, errors[index]) != NULL);
    }

    CHECK(snprintf(
        history_path,
        sizeof(history_path),
        "%s/trainlog/sync_history.log",
        temporary
    ) > 0);
    history = fopen(history_path, "rb");
    CHECK(history != NULL);
    for (index = 0U; index < 3U; ++index) {
        CHECK(fgets(line, sizeof(line), history) != NULL);
        CHECK(trainlog_sync_history_parse_line(line, &entry));
        CHECK(entry.direction_known);
        CHECK(entry.direction == directions[index]);
        CHECK(strcmp(entry.summary, errors[index]) == 0);
    }
    CHECK(fgets(line, sizeof(line), history) == NULL);
    CHECK(fclose(history) == 0);

    for (index = 0U; index < 3U; ++index) {
        (void)snprintf(run_path, sizeof(run_path), "%s/trainlog/sync_runs/%s.json", temporary, ids[index]);
        CHECK(unlink(run_path) == 0);
        (void)snprintf(run_path, sizeof(run_path), "%s/trainlog/sync_runs/%s.txt", temporary, ids[index]);
        CHECK(unlink(run_path) == 0);
    }
    CHECK(unlink(history_path) == 0);
    (void)snprintf(run_path, sizeof(run_path), "%s/trainlog/sync_runs", temporary);
    CHECK(rmdir(run_path) == 0);
    (void)snprintf(run_path, sizeof(run_path), "%s/trainlog", temporary);
    CHECK(rmdir(run_path) == 0);
    CHECK(rmdir(temporary) == 0);

    puts("PASS sync history directions");
    return 0;
}
