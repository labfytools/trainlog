#include <stdio.h>
#include <string.h>
#include "trainlog/sync.h"

#define CHECK(value) do { if (!(value)) { fprintf(stderr, "FAIL line %d\n", __LINE__); return 1; } } while (0)

int main(void)
{
    TrainlogSyncDirectionPlan plan;
    TrainlogSyncReport report = {0};
    TrainlogMtpEntry entries[5] = {0};
    size_t selected = 0U;
    plan = trainlog_sync_direction_plan(TRAINLOG_SYNC_ANDROID_TO_PC);
    CHECK(plan.receive_android && !plan.publish_android);
    plan = trainlog_sync_direction_plan(TRAINLOG_SYNC_PC_TO_ANDROID);
    CHECK(!plan.receive_android && plan.publish_android);
    plan = trainlog_sync_direction_plan(TRAINLOG_SYNC_BIDIRECTIONAL);
    CHECK(plan.receive_android && plan.publish_android);

    (void)snprintf(entries[0].name, sizeof(entries[0].name), "%s",
                   "trainlog-mobile-export-v2.json");
    entries[0].item_id = 20U;
    entries[0].modification_unix_seconds = 100U;
    (void)snprintf(entries[1].name, sizeof(entries[1].name), "%s",
                   "trainlog-mobile-export-v2 (25).json");
    entries[1].item_id = 30U;
    entries[1].modification_unix_seconds = 200U;
    (void)snprintf(entries[2].name, sizeof(entries[2].name), "%s",
                   "trainlog-mobile-export-v2 (26).json");
    entries[2].item_id = 10U;
    entries[2].modification_unix_seconds = 200U;
    (void)snprintf(entries[3].name, sizeof(entries[3].name), "%s",
                   "trainlog-mobile-export-v2 (oops).json");
    entries[3].item_id = 1U;
    entries[3].modification_unix_seconds = 999U;
    (void)snprintf(entries[4].name, sizeof(entries[4].name), "%s",
                   "trainlog-mobile-export-v1.json");
    entries[4].item_id = 2U;
    entries[4].modification_unix_seconds = 999U;
    CHECK(trainlog_sync_select_mobile_export(entries, 5U, &selected));
    CHECK(selected == 2U);

    entries[0].modification_unix_seconds = 0U;
    entries[1].modification_unix_seconds = 0U;
    entries[2].modification_unix_seconds = 0U;
    CHECK(trainlog_sync_select_mobile_export(entries, 5U, &selected));
    CHECK(selected == 2U);
    CHECK(!trainlog_sync_select_mobile_export(NULL, 0U, &selected));

    (void)snprintf(entries[0].name, sizeof(entries[0].name), "%s",
                   "trainlog-mobile-equipment-definitions-v1.json");
    entries[0].item_id = 40U;
    entries[0].modification_unix_seconds = 10U;
    (void)snprintf(entries[1].name, sizeof(entries[1].name), "%s",
                   "trainlog-mobile-equipment-definitions-v1 (4).json");
    entries[1].item_id = 41U;
    entries[1].modification_unix_seconds = 20U;
    CHECK(trainlog_sync_select_android_artifact(
        entries,
        2U,
        "trainlog-mobile-equipment-definitions-v1.json",
        &selected));
    CHECK(selected == 1U);
    CHECK(!trainlog_sync_select_android_artifact(entries, 2U,
                                                 "not-json", &selected));

    (void)snprintf(entries[0].name, sizeof(entries[0].name), "%s",
                   "trainlog-equipment-associations-v1.json");
    entries[0].item_id = 50U;
    entries[0].modification_unix_seconds = 10U;
    (void)snprintf(entries[1].name, sizeof(entries[1].name), "%s",
                   "trainlog-equipment-associations-v1 (31).json");
    entries[1].item_id = 51U;
    entries[1].modification_unix_seconds = 30U;
    CHECK(trainlog_sync_select_android_artifact(
        entries,
        2U,
        "trainlog-equipment-associations-v1.json",
        &selected));
    CHECK(selected == 1U);

    report.success = true;
    report.direction = TRAINLOG_SYNC_BIDIRECTIONAL;
    report.exercises_reconciled = 1U;
    report.exercises_skipped = 8U;
    report.sessions_skipped = 1U;
    report.catalog_published = 15U;
    trainlog_sync_build_summary(&report);
    CHECK(strstr(report.summary, "+0 exercice(s)") != NULL);
    CHECK(strstr(report.summary, "+1 exercice(s)") == NULL);

    report.exercises_imported = 1U;
    trainlog_sync_build_summary(&report);
    CHECK(strstr(report.summary, "+1 exercice(s)") != NULL);
    puts("PASS sync direction plan");
    return 0;
}
