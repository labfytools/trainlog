/**
 * @file test_body_observation_edit.c
 * @brief Body observation list/detail/update persistence tests.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "trainlog/database.h"

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            (void)fprintf(                                                   \
                stderr,                                                      \
                "CHECK failed at %s:%d: %s\n",                               \
                __FILE__,                                                    \
                __LINE__,                                                    \
                #condition                                                   \
            );                                                               \
            return false;                                                    \
        }                                                                    \
    } while (0)

static bool test_body_observation_edit(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogBodyObservationInput first;
    TrainlogBodyObservationInput second;
    TrainlogBodyObservationRecord records[4];
    TrainlogBodyObservationRecord loaded;
    size_t count = 0U;

    CHECK(
        trainlog_database_open(
            ":memory:",
            &database
        ) == TRAINLOG_STATUS_OK
    );

    (void)memset(&first, 0, sizeof(first));

    (void)snprintf(
        first.observation_id,
        sizeof(first.observation_id),
        "%s",
        "bo_first"
    );

    (void)snprintf(
        first.observed_at,
        sizeof(first.observed_at),
        "%s",
        "2026-08-01T08:00:00+02:00"
    );

    first.has_body_weight = true;
    first.body_weight_kg = 85.0;
    first.has_waist = true;
    first.waist_cm = 98.0;

    CHECK(
        trainlog_database_insert_body_observation(
            database,
            &first
        ) == TRAINLOG_STATUS_OK
    );

    (void)memset(&second, 0, sizeof(second));

    (void)snprintf(
        second.observation_id,
        sizeof(second.observation_id),
        "%s",
        "bo_second"
    );

    (void)snprintf(
        second.observed_at,
        sizeof(second.observed_at),
        "%s",
        "2026-09-01T08:00:00+02:00"
    );

    second.has_body_weight = true;
    second.body_weight_kg = 84.0;
    second.has_waist = true;
    second.waist_cm = 96.0;
    second.has_chest = true;
    second.chest_cm = 104.0;

    CHECK(
        trainlog_database_insert_body_observation(
            database,
            &second
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        trainlog_database_list_body_observations(
            database,
            records,
            4U,
            &count
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(count == 2U);

    CHECK(
        strcmp(
            records[0].observation_id,
            "bo_second"
        ) == 0
    );

    CHECK(
        strcmp(
            records[1].observation_id,
            "bo_first"
        ) == 0
    );

    second.body_weight_kg = 83.5;
    second.has_waist = false;
    second.waist_cm = 0.0;
    second.has_chest = true;
    second.chest_cm = 105.0;

    CHECK(
        trainlog_database_update_body_observation(
            database,
            &second
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        trainlog_database_get_body_observation(
            database,
            "bo_second",
            &loaded
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        strcmp(
            loaded.observed_at,
            "2026-09-01T08:00:00+02:00"
        ) == 0
    );

    CHECK(loaded.has_body_weight);
    CHECK(loaded.body_weight_kg > 83.49);
    CHECK(loaded.body_weight_kg < 83.51);
    CHECK(!loaded.has_waist);
    CHECK(loaded.has_chest);
    CHECK(loaded.chest_cm > 104.99);
    CHECK(loaded.chest_cm < 105.01);

    (void)memset(&second, 0, sizeof(second));

    (void)snprintf(
        second.observation_id,
        sizeof(second.observation_id),
        "%s",
        "bo_second"
    );

    CHECK(
        trainlog_database_update_body_observation(
            database,
            &second
        ) == TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    trainlog_database_close(database);
    return true;
}

int main(void)
{
    CHECK(test_body_observation_edit());

    (void)printf(
        "PASS body_observation_edit\n"
    );

    return 0;
}
