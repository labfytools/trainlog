/**
 * @file test_database.c
 * @brief Black-box tests for Trainlog persistence.
 */

#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "trainlog/database.h"
#include "trainlog/id.h"

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

static bool test_database_open_and_schema(void)
{
    TrainlogDatabase *database = NULL;
    int schema_version = 0;
    int foreign_keys = 0;

    CHECK(
        trainlog_database_open(":memory:", &database) ==
        TRAINLOG_STATUS_OK
    );
    CHECK(
        trainlog_database_schema_version(database, &schema_version) ==
        TRAINLOG_STATUS_OK
    );
    CHECK(schema_version == TRAINLOG_DATABASE_SCHEMA_VERSION);
    CHECK(
        trainlog_database_foreign_keys_enabled(database, &foreign_keys) ==
        TRAINLOG_STATUS_OK
    );
    CHECK(foreign_keys == 1);

    trainlog_database_close(database);
    return true;
}

static bool test_database_open_diagnostic(void)
{
    TrainlogDatabase *database = NULL;
    char diagnostic[256];

    /* CONTRACT: a launcher must retain the stable status code while showing
     * the SQLite operation that blocked access to the user's real database. */
    CHECK(
        trainlog_database_open_with_diagnostic(
            "/",
            &database,
            diagnostic,
            sizeof(diagnostic)
        ) == TRAINLOG_STATUS_DATABASE_ERROR
    );
    CHECK(database == NULL);
    CHECK(strstr(diagnostic, "open database:") != NULL);

    return true;
}

static bool test_generated_ids(void)
{
    char first[TRAINLOG_GENERATED_ID_CAPACITY];
    char second[TRAINLOG_GENERATED_ID_CAPACITY];
    char sync_id[TRAINLOG_GENERATED_ID_CAPACITY];
    char entry_id[TRAINLOG_GENERATED_ID_CAPACITY];
    char v1_exact_capacity[2U + 1U + TRAINLOG_UUID_TEXT_LENGTH + 1U];

    CHECK(
        trainlog_id_generate("ex", first, sizeof(first)) ==
        TRAINLOG_STATUS_OK
    );
    CHECK(
        trainlog_id_generate("ex", second, sizeof(second)) ==
        TRAINLOG_STATUS_OK
    );
    CHECK(strncmp(first, "ex_", 3U) == 0);
    CHECK(strlen(first) == 2U + 1U + TRAINLOG_UUID_TEXT_LENGTH);
    CHECK(strcmp(first, second) != 0);
    CHECK(first[3U + 14U] == '4');
    /* ABI behavior: the new longer occurrence prefix must not invalidate the
     * exact buffer size historically sufficient for two-character prefixes. */
    CHECK(
        trainlog_id_generate(
            "se",
            v1_exact_capacity,
            sizeof(v1_exact_capacity)
        ) == TRAINLOG_STATUS_OK
    );

    /* TRAINLOG_SYNC_ID_PREFIX_TEST */
    CHECK(
        trainlog_id_generate(
            "sy",
            sync_id,
            sizeof(sync_id)
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        strncmp(
            sync_id,
            "sy_",
            3U
        ) == 0
    );

    CHECK(
        sync_id[3U + 14U] ==
        '4'
    );

    CHECK(
        trainlog_id_generate(
            "sxe",
            entry_id,
            sizeof(entry_id)
        ) == TRAINLOG_STATUS_OK
    );
    CHECK(strncmp(entry_id, "sxe_", 4U) == 0);
    CHECK(strlen(entry_id) == TRAINLOG_GENERATED_ID_CAPACITY - 1U);
    CHECK(entry_id[4U + 14U] == '4');

    return true;
}

static bool seed_exercise(TrainlogDatabase *database)
{
    return trainlog_database_insert_exercise(
        database,
        "ex_test",
        "Presse à cuisses",
        "presse à cuisses",
        TRAINLOG_TRACKING_REPS
    ) == TRAINLOG_STATUS_OK;
}

static bool test_session_insert(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogSetInput sets[3];
    TrainlogSessionExerciseInput exercise;
    TrainlogSessionInput session;
    TrainlogSessionSummary persisted_session;
    TrainlogPersistedExerciseDetail persisted_exercises[1];
    size_t count = 0U;

    CHECK(
        trainlog_database_open(":memory:", &database) ==
        TRAINLOG_STATUS_OK
    );
    CHECK(seed_exercise(database));

    (void)memset(sets, 0, sizeof(sets));
    sets[0].reps = 5;
    sets[0].has_weight = true;
    sets[0].weight_kg = 32.5;
    sets[1] = sets[0];
    sets[1].weight_kg = 0.0;
    sets[2] = sets[0];
    sets[2].has_weight = false;
    sets[2].weight_kg = 0.0;

    (void)memset(&exercise, 0, sizeof(exercise));
    (void)snprintf(
        exercise.exercise_id,
        sizeof(exercise.exercise_id),
        "%s",
        "ex_test"
    );
    exercise.load_mode = TRAINLOG_LOAD_EXTERNAL;
    exercise.rest_seconds = 60;
    exercise.target_sets = 3;
    exercise.target_reps = 5;
    exercise.target_has_weight = true;
    exercise.target_weight_kg = 80.0;
    exercise.sets = sets;
    exercise.set_count = 3U;

    (void)memset(&session, 0, sizeof(session));
    (void)snprintf(
        session.session_id,
        sizeof(session.session_id),
        "%s",
        "se_test"
    );
    (void)snprintf(
        session.started_at,
        sizeof(session.started_at),
        "%s",
        "2026-09-05T18:00:00+02:00"
    );
    (void)snprintf(
        session.ended_at,
        sizeof(session.ended_at),
        "%s",
        "2026-09-05T19:00:00+02:00"
    );
    session.exercises = &exercise;
    session.exercise_count = 1U;

    CHECK(
        trainlog_database_insert_session(database, &session) ==
        TRAINLOG_STATUS_OK
    );
    CHECK(
        trainlog_database_insert_session(database, &session) ==
        TRAINLOG_STATUS_CONFLICT
    );
    CHECK(
        trainlog_database_session_count(database, &count) ==
        TRAINLOG_STATUS_OK
    );
    CHECK(count == 1U);
    CHECK(
        trainlog_database_get_session_details(
            database,
            "se_test",
            &persisted_session,
            persisted_exercises,
            1U,
            &count
        ) == TRAINLOG_STATUS_OK
    );
    CHECK(count == 1U);
    /* Regression: local occurrences are `sxe`, never synchronization runs. */
    CHECK(strncmp(persisted_exercises[0].entry_id, "sxe_", 4U) == 0);
    CHECK(persisted_exercises[0].actual_set_count == 3U);
    CHECK(persisted_exercises[0].actual_sets[0].has_weight);
    CHECK(persisted_exercises[0].actual_sets[0].weight_kg == 32.5);
    CHECK(persisted_exercises[0].actual_sets[1].has_weight);
    CHECK(persisted_exercises[0].actual_sets[1].weight_kg == 0.0);
    CHECK(!persisted_exercises[0].actual_sets[2].has_weight);

    trainlog_database_free_session_details(persisted_exercises, count);

    (void)snprintf(session.session_id, sizeof(session.session_id), "%s",
        "se_negative_weight");
    sets[0].weight_kg = -0.5;
    CHECK(trainlog_database_insert_session(database, &session) ==
        TRAINLOG_STATUS_INVALID_ARGUMENT);
    (void)snprintf(session.session_id, sizeof(session.session_id), "%s",
        "se_nonfinite_weight");
    sets[0].weight_kg = INFINITY;
    CHECK(trainlog_database_insert_session(database, &session) ==
        TRAINLOG_STATUS_INVALID_ARGUMENT);
    CHECK(trainlog_database_session_count(database, &count) ==
        TRAINLOG_STATUS_OK);
    CHECK(count == 1U);

    trainlog_database_close(database);
    return true;
}

static bool test_body_weight_history(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogBodyObservationInput observation;
    TrainlogWeightPoint points[4];
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
        "bo_test"
    );
    (void)snprintf(
        observation.observed_at,
        sizeof(observation.observed_at),
        "%s",
        "2026-09-05T07:00:00+02:00"
    );
    observation.has_body_weight = true;
    observation.body_weight_kg = 82.4;

    CHECK(
        trainlog_database_insert_body_observation(
            database,
            &observation
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        trainlog_database_list_weight_points(
            database,
            points,
            4U,
            &count
        ) == TRAINLOG_STATUS_OK
    );
    CHECK(count == 1U);
    CHECK(points[0].body_weight_kg > 82.39);
    CHECK(points[0].body_weight_kg < 82.41);

    trainlog_database_close(database);
    return true;
}

static bool test_transaction_rollback(void)
{
    TrainlogDatabase *database = NULL;
    size_t count = 0U;

    CHECK(
        trainlog_database_open(":memory:", &database) ==
        TRAINLOG_STATUS_OK
    );
    CHECK(trainlog_database_begin(database) == TRAINLOG_STATUS_OK);
    CHECK(seed_exercise(database));
    CHECK(trainlog_database_rollback(database) == TRAINLOG_STATUS_OK);
    CHECK(
        trainlog_database_exercise_count(database, &count) ==
        TRAINLOG_STATUS_OK
    );
    CHECK(count == 0U);

    trainlog_database_close(database);
    return true;
}

struct TestCase {
    const char *name;
    bool (*function)(void);
};

int main(void)
{
    static const struct TestCase tests[] = {
        {"database_open_and_schema", test_database_open_and_schema},
        {"database_open_diagnostic", test_database_open_diagnostic},
        {"generated_ids", test_generated_ids},
        {"session_insert", test_session_insert},
        {"body_weight_history", test_body_weight_history},
        {"transaction_rollback", test_transaction_rollback},
    };
    size_t index;

    for (index = 0U; index < sizeof(tests) / sizeof(tests[0]); ++index) {
        if (!tests[index].function()) {
            (void)fprintf(stderr, "FAIL %s\n", tests[index].name);
            return 1;
        }
        (void)printf("PASS %s\n", tests[index].name);
    }

    return 0;
}
