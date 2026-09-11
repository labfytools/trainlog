/**
 * @file test_terminal_input.c
 * @brief Deterministic tests for the Notcurses input lifecycle boundary.
 */

#include <stdbool.h>
#include <stdio.h>

#include <notcurses/notcurses.h>

#include "trainlog/terminal.h"

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            (void)fprintf(                                                   \
                stderr,                                                      \
                "CHECK failed at %s:%d: %s\n",                             \
                __FILE__,                                                    \
                __LINE__,                                                    \
                #condition                                                   \
            );                                                               \
            return false;                                                    \
        }                                                                    \
    } while (0)

static bool test_event_type_policy(void)
{
    int key = 12345;

    CHECK(trainlog_terminal_translate_input(
        NCKEY_RIGHT, TRAINLOG_INPUT_UNKNOWN, false, &key));
    CHECK(key == TRAINLOG_KEY_RIGHT);

    CHECK(trainlog_terminal_translate_input(
        NCKEY_RIGHT, TRAINLOG_INPUT_PRESS, false, &key));
    CHECK(key == TRAINLOG_KEY_RIGHT);

    CHECK(trainlog_terminal_translate_input(
        NCKEY_RIGHT, TRAINLOG_INPUT_REPEAT, false, &key));
    CHECK(key == TRAINLOG_KEY_RIGHT);

    key = 12345;
    CHECK(!trainlog_terminal_translate_input(
        NCKEY_RIGHT, TRAINLOG_INPUT_RELEASE, false, &key));
    CHECK(key == 12345);

    CHECK(!trainlog_terminal_translate_input(
        NCKEY_RIGHT, (TrainlogInputEventType)99, false, &key));
    CHECK(key == 12345);

    return true;
}

static bool test_key_translation(void)
{
    int key = TRAINLOG_KEY_NONE;

    CHECK(trainlog_terminal_translate_input(
        NCKEY_TAB, TRAINLOG_INPUT_PRESS, false, &key));
    CHECK(key == TRAINLOG_KEY_TAB);

    CHECK(trainlog_terminal_translate_input(
        NCKEY_TAB, TRAINLOG_INPUT_PRESS, true, &key));
    CHECK(key == TRAINLOG_KEY_SHIFT_TAB);

    CHECK(trainlog_terminal_translate_input(
        NCKEY_F06, TRAINLOG_INPUT_PRESS, false, &key));
    CHECK(key == TRAINLOG_KEY_F6);

    CHECK(trainlog_terminal_translate_input(
        NCKEY_F07, TRAINLOG_INPUT_REPEAT, false, &key));
    CHECK(key == TRAINLOG_KEY_F7);

    CHECK(trainlog_terminal_translate_input(
        27U, TRAINLOG_INPUT_PRESS, false, &key));
    CHECK(key == TRAINLOG_KEY_ESCAPE);

    CHECK(trainlog_terminal_translate_input(
        0x00e9U, TRAINLOG_INPUT_REPEAT, false, &key));
    CHECK(key == 0x00e9);

    CHECK(!trainlog_terminal_translate_input(
        0U, TRAINLOG_INPUT_PRESS, false, &key));
    CHECK(!trainlog_terminal_translate_input(
        UINT32_MAX, TRAINLOG_INPUT_PRESS, false, &key));
    CHECK(!trainlog_terminal_translate_input(
        NCKEY_RIGHT, TRAINLOG_INPUT_PRESS, false, NULL));

    return true;
}

int main(void)
{
    CHECK(test_event_type_policy());
    (void)printf("PASS terminal_input_event_type_policy\n");

    CHECK(test_key_translation());
    (void)printf("PASS terminal_input_key_translation\n");

    return 0;
}
