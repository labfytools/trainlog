/**
 * @file test_duration.c
 * @brief Human duration parser/formatter tests.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "trainlog/duration.h"

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

static bool expect_parse(const char *text, int expected)
{
    int seconds = -1;

    CHECK(
        trainlog_duration_parse(text, &seconds) ==
        TRAINLOG_STATUS_OK
    );
    CHECK(seconds == expected);
    return true;
}

static bool test_parsing(void)
{
    int scratch = 0;

    CHECK(expect_parse("90", 90));
    CHECK(expect_parse("90s", 90));
    CHECK(expect_parse("1:30", 90));
    CHECK(expect_parse("1m30", 90));
    CHECK(expect_parse("1m30s", 90));
    CHECK(expect_parse("2m", 120));
    CHECK(expect_parse("45s", 45));
    CHECK(expect_parse("2:05", 125));
    CHECK(expect_parse("  2m  ", 120));

    CHECK(
        trainlog_duration_parse("1:60", &scratch) ==
        TRAINLOG_STATUS_INVALID_ARGUMENT
    );
    CHECK(
        trainlog_duration_parse("1m90", &scratch) ==
        TRAINLOG_STATUS_INVALID_ARGUMENT
    );
    CHECK(
        trainlog_duration_parse("1 m 30", &scratch) ==
        TRAINLOG_STATUS_INVALID_ARGUMENT
    );
    CHECK(
        trainlog_duration_parse("", &scratch) ==
        TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    return true;
}

static bool test_formatting(void)
{
    char buffer[64];

    CHECK(
        trainlog_duration_format(45, buffer, sizeof(buffer)) ==
        TRAINLOG_STATUS_OK
    );
    CHECK(strcmp(buffer, "45 s") == 0);

    CHECK(
        trainlog_duration_format(60, buffer, sizeof(buffer)) ==
        TRAINLOG_STATUS_OK
    );
    CHECK(strcmp(buffer, "1 min") == 0);

    CHECK(
        trainlog_duration_format(90, buffer, sizeof(buffer)) ==
        TRAINLOG_STATUS_OK
    );
    CHECK(strcmp(buffer, "1 min 30 s") == 0);

    CHECK(
        trainlog_duration_format(125, buffer, sizeof(buffer)) ==
        TRAINLOG_STATUS_OK
    );
    CHECK(strcmp(buffer, "2 min 5 s") == 0);

    return true;
}

int main(void)
{
    CHECK(test_parsing());
    (void)printf("PASS duration_parsing\n");

    CHECK(test_formatting());
    (void)printf("PASS duration_formatting\n");

    return 0;
}
