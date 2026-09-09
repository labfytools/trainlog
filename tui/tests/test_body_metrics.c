/**
 * @file test_body_metrics.c
 * @brief Generic body-metric history tests.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "trainlog/database.h"

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n",             \
                __FILE__, __LINE__, #condition);                             \
            return false;                                                    \
        }                                                                    \
    } while (0)

static bool test_body_metrics(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogBodyObservationInput observation;
    TrainlogBodyMetricPoint points[8];
    TrainlogBodyPairPoint pair;
    size_t count = 0U;

    CHECK(
        trainlog_database_open(":memory:", &database) ==
        TRAINLOG_STATUS_OK
    );

    (void)memset(&observation, 0, sizeof(observation));

    (void)snprintf(
        observation.observation_id,
        sizeof(observation.observation_id),
        "%s",
        "bo_metrics"
    );

    (void)snprintf(
        observation.observed_at,
        sizeof(observation.observed_at),
        "%s",
        "2026-09-05T08:00:00+02:00"
    );

    observation.has_body_weight = true;
    observation.body_weight_kg = 84.1;
    observation.has_waist = true;
    observation.waist_cm = 96.4;
    observation.has_left_arm = true;
    observation.left_arm_cm = 34.2;
    observation.has_right_arm = true;
    observation.right_arm_cm = 34.8;

    CHECK(
        trainlog_database_insert_body_observation(
            database,
            &observation
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        trainlog_database_list_body_metric_points(
            database,
            TRAINLOG_BODY_METRIC_WAIST,
            points,
            8U,
            &count
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(count == 1U);
    CHECK(points[0].value > 96.39);
    CHECK(points[0].value < 96.41);

    CHECK(
        trainlog_database_latest_body_pair(
            database,
            TRAINLOG_BODY_METRIC_LEFT_ARM,
            TRAINLOG_BODY_METRIC_RIGHT_ARM,
            &pair
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(pair.found);
    CHECK(pair.left_value > 34.19);
    CHECK(pair.left_value < 34.21);
    CHECK(pair.right_value > 34.79);
    CHECK(pair.right_value < 34.81);

    trainlog_database_close(database);
    return true;
}

int main(void)
{
    if (!test_body_metrics()) {
        return 1;
    }
    (void)printf("PASS body_metrics\n");
    return 0;
}
