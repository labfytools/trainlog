#include <stdbool.h>
#include <stdio.h>

#include "trainlog/bodyviz.h"

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

static bool test_bodyviz(void)
{
    double value = 0.0;

    CHECK(
        trainlog_body_index100(
            50.0,
            55.0,
            &value
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(value > 109.99);
    CHECK(value < 110.01);

    CHECK(
        trainlog_body_percent_change(
            100.0,
            96.0,
            &value
        ) == TRAINLOG_STATUS_OK
    );

    CHECK(value > -4.01);
    CHECK(value < -3.99);

    CHECK(
        trainlog_body_index100(
            0.0,
            50.0,
            &value
        ) == TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    return true;
}

int main(void)
{
    CHECK(test_bodyviz());
    (void)printf("PASS bodyviz\n");
    return 0;
}
