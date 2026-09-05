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

int main(void)
{
    CHECK(test_normalization());
    (void)printf("PASS normalization\n");

    CHECK(test_blank_rejected());
    (void)printf("PASS blank_rejected\n");

    return 0;
}
