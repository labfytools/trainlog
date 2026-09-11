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
        NCKEY_RIGHT, 0U, TRAINLOG_INPUT_UNKNOWN, false, false, false, false, &key));
    CHECK(key == TRAINLOG_KEY_RIGHT);

    CHECK(trainlog_terminal_translate_input(
        NCKEY_RIGHT, 0U, TRAINLOG_INPUT_PRESS, false, false, false, false, &key));
    CHECK(key == TRAINLOG_KEY_RIGHT);

    CHECK(trainlog_terminal_translate_input(
        NCKEY_RIGHT, 0U, TRAINLOG_INPUT_REPEAT, false, false, false, false, &key));
    CHECK(key == TRAINLOG_KEY_RIGHT);

    key = 12345;
    CHECK(!trainlog_terminal_translate_input(
        NCKEY_RIGHT, 0U, TRAINLOG_INPUT_RELEASE, false, false, false, false, &key));
    CHECK(key == 12345);

    CHECK(!trainlog_terminal_translate_input(
        NCKEY_RIGHT, 0U, (TrainlogInputEventType)99, false, false, false, false, &key));
    CHECK(key == 12345);

    return true;
}

static bool test_key_translation(void)
{
    int key = TRAINLOG_KEY_NONE;

    CHECK(trainlog_terminal_translate_input(
        NCKEY_TAB, 0U, TRAINLOG_INPUT_PRESS, false, false, false, false, &key));
    CHECK(key == TRAINLOG_KEY_TAB);

    CHECK(trainlog_terminal_translate_input(
        NCKEY_TAB, 0U, TRAINLOG_INPUT_PRESS, true, false, false, false, &key));
    CHECK(key == TRAINLOG_KEY_SHIFT_TAB);

    CHECK(trainlog_terminal_translate_input(
        NCKEY_F06, 0U, TRAINLOG_INPUT_PRESS, false, false, false, false, &key));
    CHECK(key == TRAINLOG_KEY_F6);

    CHECK(trainlog_terminal_translate_input(
        NCKEY_F07, 0U, TRAINLOG_INPUT_REPEAT, false, false, false, false, &key));
    CHECK(key == TRAINLOG_KEY_F7);

    CHECK(trainlog_terminal_translate_input(
        27U, 0U, TRAINLOG_INPUT_PRESS, false, false, false, false, &key));
    CHECK(key == TRAINLOG_KEY_ESCAPE);

    CHECK(trainlog_terminal_translate_input(
        0x00e9U, 0x00e9U, TRAINLOG_INPUT_REPEAT, false, false, false, false, &key));
    CHECK(key == 0x00e9);

    CHECK(!trainlog_terminal_translate_input(
        0U, 0U, TRAINLOG_INPUT_PRESS, false, false, false, false, &key));
    CHECK(!trainlog_terminal_translate_input(
        UINT32_MAX, 0U, TRAINLOG_INPUT_PRESS, false, false, false, false, &key));
    CHECK(!trainlog_terminal_translate_input(
        NCKEY_RIGHT, 0U, TRAINLOG_INPUT_PRESS, false, false, false, false, NULL));

    /* French AZERTY commonly needs Shift to yield '/'. eff_text, rather than
     * the physical id, is the action character and must reach list.search. */
    CHECK(trainlog_terminal_translate_input(
        ':', '/', TRAINLOG_INPUT_PRESS, true, false, false, false, &key));
    CHECK(key == '/');
    CHECK(trainlog_terminal_translate_input(
        '/', '/', TRAINLOG_INPUT_PRESS, false, false, false, false, &key));
    CHECK(key == '/');
    CHECK(!trainlog_terminal_translate_input(
        ':', '/', TRAINLOG_INPUT_PRESS, true, true, false, false, &key));
    CHECK(!trainlog_terminal_translate_input(
        ':', '/', TRAINLOG_INPUT_PRESS, true, false, true, false, &key));

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
