/**
 * @file test_app_shell.c
 * @brief APP_SHELL_V1 state and geometry contract regressions.
 */

#include <stdio.h>
#include <string.h>

#include "trainlog/app_shell.h"
#include "trainlog/terminal.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
        __FILE__, __LINE__, #value); return false; } } while (0)

static bool test_geometry(void)
{
    static const int sizes[][2] = {
        {120, 35}, {120, 31}, {100, 30}, {100, 25}, {80, 24}, {72, 20}
    };
    size_t index;
    for (index = 0U; index < sizeof(sizes) / sizeof(sizes[0]); ++index) {
        TrainlogShellLayout layout;
        TrainlogRect overlay;
        trainlog_shell_layout_compute(sizes[index][0], sizes[index][1], &layout);
        CHECK(layout.usable);
        CHECK(layout.header.y == 0 && layout.header.height == 2);
        CHECK(layout.content.y == 2 && layout.content.height == sizes[index][1] - 4);
        CHECK(layout.footer.y == sizes[index][1] - 2 && layout.footer.height == 2);
        CHECK(layout.content.y + layout.content.height == layout.footer.y);
        CHECK(layout.sidebar_visible ==
            (sizes[index][0] >= 100 && sizes[index][1] >= 26));
        CHECK(layout.sidebar_expanded ==
            (sizes[index][0] >= 120 && sizes[index][1] >= 32));
        if (layout.sidebar_visible) {
            CHECK(layout.sidebar.width == 22 && layout.separator.x == 22);
            CHECK(layout.content.x == 23);
        } else CHECK(layout.content.x == 0);
        overlay = trainlog_shell_overlay_rect(&layout, 80, 29);
        CHECK(trainlog_shell_rect_contains(&(TrainlogRect){2, 0,
            sizes[index][1] - 4, sizes[index][0]}, &overlay));
        CHECK(overlay.y + overlay.height <= layout.footer.y);
    }
    {
        TrainlogShellLayout layout;
        trainlog_shell_layout_compute(99, 40, &layout);
        CHECK(!layout.sidebar_visible);
        trainlog_shell_layout_compute(160, 25, &layout);
        CHECK(!layout.sidebar_visible);
        trainlog_shell_layout_compute(71, 20, &layout);
        CHECK(!layout.usable);
    }
    return true;
}

static bool test_navigation_and_actions(void)
{
    TrainlogNavigationState navigation;
    TrainlogActionModel actions;
    TrainlogAction action = {"open.exercises", '3', "Exercices", true, 20U,
        TRAINLOG_INTENT_OPEN_ROUTE, TRAINLOG_ROUTE_EXERCISES};
    const TrainlogAction *found;
    trainlog_navigation_init(&navigation);
    CHECK(trainlog_navigation_open(&navigation, TRAINLOG_ROUTE_EXERCISES,
        "ex_11111111-1111-4111-8111-111111111111"));
    CHECK(navigation.current.route == TRAINLOG_ROUTE_EXERCISES);
    CHECK(trainlog_navigation_open(&navigation, TRAINLOG_ROUTE_STATS_EXERCISE,
        navigation.current.stable_id));
    CHECK(trainlog_navigation_back(&navigation));
    CHECK(navigation.current.route == TRAINLOG_ROUTE_EXERCISES);
    CHECK(strcmp(navigation.current.stable_id,
        "ex_11111111-1111-4111-8111-111111111111") == 0);
    trainlog_actions_clear(&actions);
    CHECK(trainlog_actions_add(&actions, &action));
    CHECK(!trainlog_actions_add(&actions, &action));
    CHECK(actions.count == 1U);
    action.identifier = "help"; action.key = '?'; action.label = "Aide";
    action.priority = 10U; action.intent = TRAINLOG_INTENT_OPEN_HELP;
    CHECK(trainlog_actions_add(&actions, &action));
    found = trainlog_actions_find_key(&actions, '3');
    CHECK(found != NULL && found->intent == TRAINLOG_INTENT_OPEN_ROUTE);
    CHECK(strcmp(trainlog_actions_at_priority(&actions, 0U)->identifier, "help") == 0);
    actions.items[0].enabled = false;
    CHECK(trainlog_actions_find_key(&actions, '3') == NULL);
    return true;
}

static bool test_overlay_isolation_and_focus(void)
{
    TrainlogOverlayStack stack;
    TrainlogFocusTarget focus = TRAINLOG_FOCUS_NAVIGATION;
    char selected[TRAINLOG_SHELL_STABLE_ID_CAPACITY];
    trainlog_overlays_init(&stack);
    CHECK(trainlog_overlays_push(&stack, TRAINLOG_OVERLAY_NAVIGATION,
        TRAINLOG_FOCUS_CONTENT, "se_1"));
    CHECK(trainlog_overlays_push(&stack, TRAINLOG_OVERLAY_CONFIRMATION,
        TRAINLOG_FOCUS_OVERLAY, "se_1"));
    CHECK(trainlog_overlays_push(&stack, TRAINLOG_OVERLAY_HELP,
        TRAINLOG_FOCUS_OVERLAY, "se_1"));
    CHECK(!trainlog_overlays_push(&stack, TRAINLOG_OVERLAY_HELP,
        TRAINLOG_FOCUS_OVERLAY, "se_1"));
    CHECK(trainlog_overlays_top(&stack)->type == TRAINLOG_OVERLAY_HELP);
    CHECK(trainlog_overlays_pop(&stack, &focus, selected));
    CHECK(focus == TRAINLOG_FOCUS_OVERLAY && strcmp(selected, "se_1") == 0);
    CHECK(trainlog_overlays_top(&stack)->type == TRAINLOG_OVERLAY_CONFIRMATION);
    return true;
}

static bool test_search_utf8(void)
{
    TrainlogSearchState search;
    char fill[2] = {'a', '\0'};
    size_t index;
    trainlog_search_init(&search);
    search.open = true; search.focused = true;
    CHECK(trainlog_search_insert(&search, "é", strlen("é")));
    CHECK(trainlog_search_insert(&search, "界", strlen("界")));
    trainlog_search_home(&search); trainlog_search_right(&search);
    CHECK(search.cursor == strlen("é"));
    trainlog_search_left(&search); CHECK(search.cursor == 0U);
    trainlog_search_end(&search);
    CHECK(trainlog_search_backspace(&search));
    CHECK(strcmp(search.text, "é") == 0 && search.bytes == strlen("é"));
    CHECK(!trainlog_search_escape(&search));
    CHECK(search.open && search.focused && search.bytes == 0U);
    CHECK(trainlog_search_escape(&search));
    CHECK(!search.open && !search.focused);
    trainlog_search_init(&search);
    for (index = 0U; index < 200U; ++index)
        CHECK(trainlog_search_insert(&search, fill, 1U));
    CHECK(!trainlog_search_insert(&search, fill, 1U));
    CHECK(search.capacity_error && search.bytes == 200U);
    trainlog_search_init(&search);
    CHECK(trainlog_search_insert(&search, "e", 1U));
    CHECK(trainlog_search_insert(&search, "\xcc\x81", 2U));
    CHECK(trainlog_search_insert(&search, "x", 1U));
    trainlog_search_left(&search);
    CHECK(search.cursor == 3U);
    trainlog_search_left(&search);
    CHECK(search.cursor == 0U);
    trainlog_search_end(&search);
    CHECK(trainlog_search_backspace(&search));
    CHECK(strcmp(search.text, "e\xcc\x81") == 0);
    CHECK(trainlog_search_backspace(&search));
    CHECK(search.bytes == 0U && search.cursor == 0U);
    return true;
}

static bool test_stable_selection_and_resize(void)
{
    const char *first[] = {"a", "b", "c", "d", "e"};
    const char *filtered[] = {"b", "d", "e"};
    TrainlogListState list;
    trainlog_list_init(&list);
    trainlog_list_set_items(&list, first, 5U, 2U, false, true);
    trainlog_list_move(&list, first, 3);
    CHECK(strcmp(list.selected_id, "d") == 0 && list.viewport_start == 2U);
    trainlog_list_set_items(&list, filtered, 3U, 1U, false, true);
    CHECK(strcmp(list.selected_id, "d") == 0 && list.selected_index == 1U);
    trainlog_list_set_items(&list, filtered, 3U, 3U, true, false);
    CHECK(strcmp(list.selected_id, "d") == 0 && list.viewport_start == 0U);
    return true;
}

static bool test_list_label_clipping(void)
{
    char output[64];
    trainlog_shell_format_list_label("Développé couché très long", 12,
        output, sizeof(output));
    CHECK(strcmp(output, "Développé c…") == 0);
    trainlog_shell_format_list_label("Court", 12, output, sizeof(output));
    CHECK(strcmp(output, "Court") == 0);
    trainlog_shell_format_list_label("界界界", 5, output, sizeof(output));
    CHECK(strcmp(output, "界界…") == 0);
    return true;
}

static bool test_draft_and_generator_guards(void)
{
    TrainlogDurabilityState state = {0};
    CHECK(trainlog_shell_leave_decision(&state, false) == TRAINLOG_LEAVE_ALLOW);
    state.session_draft = true; state.session_dirty = true;
    CHECK(trainlog_shell_leave_decision(&state, false) == TRAINLOG_LEAVE_ALLOW);
    CHECK(trainlog_shell_leave_decision(&state, true) == TRAINLOG_LEAVE_CONFIRM_DISCARD);
    state.generator_preview = true; state.generator_preview_dirty = true;
    CHECK(trainlog_shell_leave_decision(&state, false) == TRAINLOG_LEAVE_CONFIRM_KEEP);
    trainlog_shell_discard_transient(&state);
    CHECK(trainlog_shell_leave_decision(&state, true) == TRAINLOG_LEAVE_ALLOW);
    return true;
}

static bool test_shared_form_adapter(void)
{
    TrainlogFormField field;
    trainlog_form_init(&field, "12");
    CHECK(field.active && strcmp(field.text, "12") == 0);
    CHECK(trainlog_form_handle(&field, TRAINLOG_KEY_HOME) ==
        TRAINLOG_FORM_IGNORED);
    CHECK(trainlog_form_handle(&field, 0x00e9) == TRAINLOG_FORM_EDITED);
    CHECK(strcmp(field.text, "é12") == 0);
    CHECK(trainlog_form_handle(&field, TRAINLOG_KEY_TAB) ==
        TRAINLOG_FORM_NEXT);
    CHECK(trainlog_form_handle(&field, TRAINLOG_KEY_F6) ==
        TRAINLOG_FORM_OPEN_NAVIGATION);
    CHECK(trainlog_form_handle(&field, TRAINLOG_KEY_F7) ==
        TRAINLOG_FORM_OPEN_ACTIONS);
    CHECK(trainlog_form_handle(&field, TRAINLOG_KEY_ESCAPE) ==
        TRAINLOG_FORM_CANCEL);
    return true;
}

int main(void)
{
    if (!test_geometry() || !test_navigation_and_actions() ||
        !test_overlay_isolation_and_focus() || !test_search_utf8() ||
        !test_stable_selection_and_resize() ||
        !test_list_label_clipping() ||
        !test_draft_and_generator_guards() ||
        !test_shared_form_adapter()) return 1;
    (void)printf("PASS app_shell\n");
    return 0;
}
