/**
 * @file test_catalog.c
 * @brief Frozen v1 exercise normalization tests.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "trainlog/catalog.h"

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

static bool test_normalization(void)
{
    char first[512];
    char second[512];
    char third[512];

    CHECK(
        trainlog_catalog_normalize_name(
            "Presse   à cuisses",
            first,
            sizeof(first)
        ) == TRAINLOG_STATUS_OK
    );
    CHECK(
        trainlog_catalog_normalize_name(
            " presse à cuisses ",
            second,
            sizeof(second)
        ) == TRAINLOG_STATUS_OK
    );
    CHECK(
        trainlog_catalog_normalize_name(
            "PRESSE À CUISSES",
            third,
            sizeof(third)
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(strcmp(first, second) == 0);
    CHECK(strcmp(second, third) == 0);

    return true;
}

static bool test_blank_rejected(void)
{
    char normalized[64];

    CHECK(
        trainlog_catalog_normalize_name(
            " \t \n ",
            normalized,
            sizeof(normalized)
        ) == TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    return true;
}

static bool test_profiled_creation(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogExercise walk;
    TrainlogExercise exercises[4];
    size_t count = 0U;
    size_t index;
    bool found = false;

    CHECK(
        trainlog_database_open(
            ":memory:",
            &database
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        trainlog_catalog_create_exercise_profiled(
            database,
            "Marche",
            TRAINLOG_TRACKING_DURATION,
            TRAINLOG_RECORDING_CONTINUOUS,
            TRAINLOG_EXERCISE_DATA_SPEED_KMH,
            &walk
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        walk.recording_mode ==
        TRAINLOG_RECORDING_CONTINUOUS
    );

    CHECK(
        walk.tracking_mode ==
        TRAINLOG_TRACKING_DURATION
    );

    CHECK(
        walk.data_fields ==
        TRAINLOG_EXERCISE_DATA_SPEED_KMH
    );

    CHECK(
        trainlog_catalog_create_exercise_profiled(
            database,
            "Profil invalide",
            TRAINLOG_TRACKING_REPS,
            TRAINLOG_RECORDING_CONTINUOUS,
            0U,
            &walk
        ) == TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    CHECK(
        trainlog_database_list_exercises(
            database,
            exercises,
            4U,
            &count
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(count == 1U);

    for (index = 0U; index < count; ++index) {
        if (strcmp(
                exercises[index].name,
                "Marche"
            ) == 0) {
            CHECK(
                exercises[index].recording_mode ==
                TRAINLOG_RECORDING_CONTINUOUS
            );

            CHECK(
                exercises[index].tracking_mode ==
                TRAINLOG_TRACKING_DURATION
            );

            CHECK(
                exercises[index].data_fields ==
                TRAINLOG_EXERCISE_DATA_SPEED_KMH
            );

            found = true;
        }
    }

    CHECK(found);

    trainlog_database_close(database);
    return true;
}

int main(void)
{
    CHECK(test_normalization());
    (void)printf("PASS normalization\n");

    CHECK(test_blank_rejected());
    (void)printf("PASS blank_rejected\n");

    CHECK(test_profiled_creation());
    (void)printf("PASS profiled_creation\n");

    return 0;
}
