#include <stdbool.h>
#include <stdio.h>

#include "trainlog/reps.h"

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

static bool test_repeat(void)
{
    int values[16];
    size_t count = 0U;
    size_t index;

    CHECK(
        trainlog_reps_parse_sequence(
            "5x10",
            values,
            16U,
            &count
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(count == 5U);

    for (
        index = 0U;
        index < count;
        ++index
    ) {
        CHECK(values[index] == 10);
    }

    return true;
}

static bool test_explicit(void)
{
    static const int expected[] = {
        4, 5, 6, 7, 8, 9, 10,
        9, 8, 7, 6, 5, 4
    };

    int values[16];
    size_t count = 0U;
    size_t index;

    CHECK(
        trainlog_reps_parse_sequence(
            "4,5,6,7,8,9,10,9,8,7,6,5,4",
            values,
            16U,
            &count
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        count ==
        sizeof(expected) /
            sizeof(expected[0])
    );

    for (
        index = 0U;
        index < count;
        ++index
    ) {
        CHECK(
            values[index] ==
            expected[index]
        );
    }

    return true;
}

static bool test_pyramid(void)
{
    static const int expected[] = {
        4, 5, 6, 7, 8, 9, 10,
        9, 8, 7, 6, 5, 4
    };

    int values[16];
    size_t count = 0U;
    size_t index;

    CHECK(
        trainlog_reps_parse_sequence(
            "4..10..4",
            values,
            16U,
            &count
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(
        count ==
        sizeof(expected) /
            sizeof(expected[0])
    );

    for (
        index = 0U;
        index < count;
        ++index
    ) {
        CHECK(
            values[index] ==
            expected[index]
        );
    }

    return true;
}

static bool test_invalid(void)
{
    int values[4];
    size_t count = 0U;

    CHECK(
        trainlog_reps_parse_sequence(
            "5x10",
            values,
            4U,
            &count
        ) ==
        TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    CHECK(
        trainlog_reps_parse_sequence(
            "10..4..10",
            values,
            4U,
            &count
        ) ==
        TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    CHECK(
        trainlog_reps_parse_sequence(
            "4,,5",
            values,
            4U,
            &count
        ) ==
        TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    return true;
}

int main(void)
{
    CHECK(test_repeat());
    CHECK(test_explicit());
    CHECK(test_pyramid());
    CHECK(test_invalid());

    (void)printf(
        "PASS reps_sequence\n"
    );

    return 0;
}
