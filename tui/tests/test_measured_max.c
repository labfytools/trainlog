/**
 * @file test_measured_max.c
 * @brief Explicit measured-max and working-load regression tests.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "trainlog/database.h"
#include "trainlog/measured_max.h"

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

static bool add_exercise(
    TrainlogDatabase *database,
    const char *id,
    const char *name
)
{
    return
        trainlog_database_insert_exercise(
            database,
            id,
            name,
            name,
            TRAINLOG_TRACKING_REPS
        ) == TRAINLOG_STATUS_OK;
}

static bool insert_reps_session(
    TrainlogDatabase *database,
    const char *session_id,
    const char *started_at,
    TrainlogSessionType session_type,
    const char *exercise_id,
    TrainlogLoadMode load_mode,
    const int *reps,
    const double *weights,
    size_t set_count
)
{
    TrainlogSetInput sets[4];
    TrainlogSessionExerciseInput exercise;
    TrainlogSessionInput session;
    size_t index;

    if (
        database == NULL ||
        session_id == NULL ||
        started_at == NULL ||
        exercise_id == NULL ||
        reps == NULL ||
        set_count == 0U ||
        set_count > 4U
    ) {
        return false;
    }

    (void)memset(
        sets,
        0,
        sizeof(sets)
    );

    for (
        index = 0U;
        index < set_count;
        ++index
    ) {
        sets[index].reps =
            reps[index];

        if (
            load_mode !=
            TRAINLOG_LOAD_NONE
        ) {
            if (weights == NULL) {
                return false;
            }

            sets[index].has_weight = true;
            sets[index].weight_kg =
                weights[index];
        }
    }

    (void)memset(
        &exercise,
        0,
        sizeof(exercise)
    );

    (void)snprintf(
        exercise.exercise_id,
        sizeof(exercise.exercise_id),
        "%s",
        exercise_id
    );

    exercise.recording_mode =
        TRAINLOG_RECORDING_SETS;

    exercise.load_mode =
        load_mode;

    exercise.rest_seconds = 120;
    exercise.target_sets =
        (int)set_count;
    exercise.target_reps = 1;

    if (
        load_mode !=
        TRAINLOG_LOAD_NONE
    ) {
        exercise.target_has_weight = true;
        exercise.target_weight_kg =
            weights[0];
    }

    exercise.sets = sets;
    exercise.set_count = set_count;

    (void)memset(
        &session,
        0,
        sizeof(session)
    );

    (void)snprintf(
        session.session_id,
        sizeof(session.session_id),
        "%s",
        session_id
    );

    (void)snprintf(
        session.started_at,
        sizeof(session.started_at),
        "%s",
        started_at
    );

    (void)snprintf(
        session.ended_at,
        sizeof(session.ended_at),
        "%s",
        "2026-09-06T21:00:00+02:00"
    );

    session.session_type =
        session_type;

    session.exercises =
        &exercise;

    session.exercise_count = 1U;

    return
        trainlog_database_insert_session(
            database,
            &session
        ) == TRAINLOG_STATUS_OK;
}

static bool test_measured_max_semantics(void)
{
    TrainlogDatabase *database = NULL;

    TrainlogExercisePerformancePoint
        points[16];

    TrainlogMeasuredMaxSummary summary;

    size_t count = 0U;
    double working = 0.0;

    const int one_rep[] = {1};
    const int failed_then_one[] = {0, 1};
    const int reps_20[] = {20};
    const int reps_15[] = {15};

    const double training_140[] = {140.0};
    const double external_110[] = {110.0};
    const double external_latest[] = {
        115.0,
        105.0,
    };
    const double assistance_30[] = {30.0};
    const double assistance_35[] = {35.0};

    CHECK(
        trainlog_database_open(
            ":memory:",
            &database
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        add_exercise(
            database,
            "ex_measured_external",
            "measured external"
        )
    );

    CHECK(
        add_exercise(
            database,
            "ex_measured_assistance",
            "measured assistance"
        )
    );

    CHECK(
        add_exercise(
            database,
            "ex_measured_none",
            "measured none"
        )
    );

    /*
     * A stronger ordinary training set must never become a measured maximum.
     */
    CHECK(
        insert_reps_session(
            database,
            "se_training_140",
            "2026-09-01T18:00:00+02:00",
            TRAINLOG_SESSION_TRAINING,
            "ex_measured_external",
            TRAINLOG_LOAD_EXTERNAL,
            one_rep,
            training_140,
            1U
        )
    );

    CHECK(
        insert_reps_session(
            database,
            "se_max_external_old",
            "2026-09-02T18:00:00+02:00",
            TRAINLOG_SESSION_MAX_TEST,
            "ex_measured_external",
            TRAINLOG_LOAD_EXTERNAL,
            one_rep,
            external_110,
            1U
        )
    );

    /*
     * 115 kg x 0 is a failed attempt. The newest successful measured result is
     * 105 kg while the older historical measured record remains 110 kg.
     */
    CHECK(
        insert_reps_session(
            database,
            "se_max_external_new",
            "2026-09-03T18:00:00+02:00",
            TRAINLOG_SESSION_MAX_TEST,
            "ex_measured_external",
            TRAINLOG_LOAD_EXTERNAL,
            failed_then_one,
            external_latest,
            2U
        )
    );

    CHECK(
        trainlog_database_list_exercise_performance(
            database,
            "ex_measured_external",
            points,
            16U,
            &count
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(count == 3U);

    CHECK(
        points[0].session_type ==
        TRAINLOG_SESSION_MAX_TEST
    );

    CHECK(
        points[2].session_type ==
        TRAINLOG_SESSION_TRAINING
    );

    CHECK(
        trainlog_measured_max_summarize(
            points,
            count,
            &summary
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(summary.test_count == 2U);
    CHECK(summary.successful_test_count == 2U);
    CHECK(summary.found);
    CHECK(summary.has_record);

    CHECK(
        strcmp(
            summary.current.session_id,
            "se_max_external_new"
        ) == 0
    );

    CHECK(
        summary.current.weight_kg >
        104.99
    );

    CHECK(
        summary.current.weight_kg <
        105.01
    );

    CHECK(
        summary.current.metric_value ==
        1
    );

    CHECK(
        summary.record.weight_kg >
        109.99
    );

    CHECK(
        summary.record.weight_kg <
        110.01
    );

    CHECK(
        trainlog_measured_max_working_load(
            &summary.current,
            80.0,
            2.5,
            &working
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        working > 84.99 &&
        working < 85.01
    );

    CHECK(
        insert_reps_session(
            database,
            "se_max_assistance_old",
            "2026-09-02T19:00:00+02:00",
            TRAINLOG_SESSION_MAX_TEST,
            "ex_measured_assistance",
            TRAINLOG_LOAD_ASSISTANCE,
            one_rep,
            assistance_30,
            1U
        )
    );

    CHECK(
        insert_reps_session(
            database,
            "se_max_assistance_new",
            "2026-09-03T19:00:00+02:00",
            TRAINLOG_SESSION_MAX_TEST,
            "ex_measured_assistance",
            TRAINLOG_LOAD_ASSISTANCE,
            one_rep,
            assistance_35,
            1U
        )
    );

    CHECK(
        trainlog_database_list_exercise_performance(
            database,
            "ex_measured_assistance",
            points,
            16U,
            &count
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        trainlog_measured_max_summarize(
            points,
            count,
            &summary
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        summary.current.weight_kg >
        34.99
    );

    CHECK(
        summary.current.weight_kg <
        35.01
    );

    CHECK(
        summary.record.weight_kg >
        29.99
    );

    CHECK(
        summary.record.weight_kg <
        30.01
    );

    CHECK(
        trainlog_measured_max_working_load(
            &summary.current,
            80.0,
            2.5,
            &working
        ) == TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    CHECK(
        insert_reps_session(
            database,
            "se_max_none_old",
            "2026-09-02T20:00:00+02:00",
            TRAINLOG_SESSION_MAX_TEST,
            "ex_measured_none",
            TRAINLOG_LOAD_NONE,
            reps_20,
            NULL,
            1U
        )
    );

    CHECK(
        insert_reps_session(
            database,
            "se_max_none_new",
            "2026-09-03T20:00:00+02:00",
            TRAINLOG_SESSION_MAX_TEST,
            "ex_measured_none",
            TRAINLOG_LOAD_NONE,
            reps_15,
            NULL,
            1U
        )
    );

    CHECK(
        trainlog_database_list_exercise_performance(
            database,
            "ex_measured_none",
            points,
            16U,
            &count
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        trainlog_measured_max_summarize(
            points,
            count,
            &summary
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        summary.current.metric_value ==
        15
    );

    CHECK(
        summary.record.metric_value ==
        20
    );

    trainlog_database_close(
        database
    );

    return true;
}

int main(void)
{
    CHECK(
        test_measured_max_semantics()
    );

    (void)printf(
        "PASS measured_max\n"
    );

    return 0;
}
