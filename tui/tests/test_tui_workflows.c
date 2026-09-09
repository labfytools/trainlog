/**
 * @file test_tui_workflows.c
 * @brief Scripted regressions through the production TUI workflow functions.
 */

#include <stdarg.h>

/* The workflows remain file-private in production. Including their translation
 * unit lets this test drive the exact code paths with the terminal port mocked,
 * without adding a public test API to Trainlog. */
#include "../src/tui.c"

struct TrainlogTerminal {
    int events[128];
    size_t event_count;
    size_t event_index;
    char output[32768];
    size_t output_used;
    int rows;
    int columns;
    bool coordinate_overflow;
    bool text_overflow;
};

struct TrainlogPanel { int unused; };

#define CHECK(condition) do { if (!(condition)) {                           \
    (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n",                  \
        __FILE__, __LINE__, #condition); return false; } } while (0)

static void script(TrainlogTerminal *terminal, const int *events, size_t count)
{
    (void)memset(terminal, 0, sizeof(*terminal));
    (void)memcpy(terminal->events, events, count * sizeof(events[0]));
    terminal->event_count = count;
    terminal->rows = 24;
    terminal->columns = 80;
}

TrainlogTerminal *trainlog_terminal_create(void) { return NULL; }
void trainlog_terminal_destroy(TrainlogTerminal *terminal) { (void)terminal; }
int trainlog_terminal_rows(const TrainlogTerminal *terminal) { return terminal->rows; }
int trainlog_terminal_columns(const TrainlogTerminal *terminal) { return terminal->columns; }
void trainlog_terminal_erase(TrainlogTerminal *terminal) { (void)terminal; }
void trainlog_terminal_render(TrainlogTerminal *terminal) { (void)terminal; }
void trainlog_terminal_style_on(TrainlogTerminal *terminal, TrainlogTextStyle style) { (void)terminal; (void)style; }
void trainlog_terminal_style_off(TrainlogTerminal *terminal, TrainlogTextStyle style) { (void)terminal; (void)style; }
void trainlog_terminal_printf(TrainlogTerminal *terminal, int row, int column,
                              const char *format, ...)
{
    va_list arguments;
    int written;
    if (row < 0 || row >= terminal->rows || column < 0 || column >= terminal->columns)
        terminal->coordinate_overflow = true;
    if (terminal->output_used >= sizeof(terminal->output)) return;
    va_start(arguments, format);
    written = vsnprintf(terminal->output + terminal->output_used,
        sizeof(terminal->output) - terminal->output_used, format, arguments);
    va_end(arguments);
    if (written > 0 && (size_t)written < sizeof(terminal->output) - terminal->output_used) {
        const char *text = terminal->output + terminal->output_used;
        const char *at = text;
        int cells = 0;
        while (*at != '\0') {
            utf8proc_int32_t codepoint;
            utf8proc_ssize_t bytes = utf8proc_iterate((const utf8proc_uint8_t *)at,
                -1, &codepoint);
            int codepoint_cells;
            if (bytes <= 0) { bytes = 1; codepoint = (unsigned char)*at; }
            codepoint_cells = utf8proc_charwidth(codepoint);
            cells += codepoint_cells < 0 ? 1 : codepoint_cells;
            at += bytes;
        }
        if (column + cells >= terminal->columns) terminal->text_overflow = true;
        terminal->output_used += (size_t)written;
        terminal->output[terminal->output_used++] = '\n';
        terminal->output[terminal->output_used] = '\0';
    }
}
void trainlog_terminal_putn(TrainlogTerminal *terminal, const char *text, size_t length) { (void)terminal; (void)text; (void)length; }
void trainlog_terminal_move(TrainlogTerminal *terminal, int row, int column) { (void)terminal; (void)row; (void)column; }
void trainlog_terminal_cursor_yx(const TrainlogTerminal *terminal, int *row, int *column) { (void)terminal; if (row) *row = 0; if (column) *column = 40; }
void trainlog_terminal_clear_to_end(TrainlogTerminal *terminal) { (void)terminal; }
void trainlog_terminal_cursor_visible(TrainlogTerminal *terminal, bool visible) { (void)terminal; (void)visible; }
void trainlog_terminal_draw(TrainlogTerminal *terminal, int row, int column, uint32_t codepoint) { (void)terminal; (void)row; (void)column; (void)codepoint; }
void trainlog_terminal_box(TrainlogTerminal *terminal, int top, int left, int bottom, int right) { (void)terminal; (void)top; (void)left; (void)bottom; (void)right; }
int trainlog_terminal_get_key(TrainlogTerminal *terminal) { return terminal->event_index < terminal->event_count ? terminal->events[terminal->event_index++] : TRAINLOG_KEY_NONE; }
bool trainlog_terminal_read_unicode(TrainlogTerminal *terminal, int *codepoint, char utf8[5])
{
    int value = trainlog_terminal_get_key(terminal);
    if (value == TRAINLOG_KEY_NONE) return false;
    *codepoint = value;
    utf8[0] = value >= 0 && value < 128 ? (char)value : '\0';
    utf8[1] = '\0';
    return true;
}
bool trainlog_terminal_push_key(TrainlogTerminal *terminal, int key) { (void)terminal; (void)key; return false; }
TrainlogPanel *tui_panel_create(TrainlogTerminal *terminal, int height, int width, int top, int left) { (void)terminal; (void)height; (void)width; (void)top; (void)left; return NULL; }
void tui_panel_destroy(TrainlogPanel *panel) { (void)panel; }
void tui_panel_box(TrainlogPanel *panel) { (void)panel; }
void tui_panel_style_on(TrainlogPanel *panel, TrainlogTextStyle style) { (void)panel; (void)style; }
void tui_panel_style_off(TrainlogPanel *panel, TrainlogTextStyle style) { (void)panel; (void)style; }
void tui_panel_print(TrainlogPanel *panel, int row, int column, const char *format, ...) { (void)panel; (void)row; (void)column; (void)format; }
void tui_panel_commit(TrainlogPanel *panel) { (void)panel; }

static bool test_assistance_creation_labels(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogExercise created;
    TrainlogSessionExerciseInput input;
    TrainlogSessionDraftExercise draft;
    TrainlogSetInput sets[4];
    TrainlogTerminal terminal;
    const int planning_events[] = {
        TRAINLOG_KEY_ENTER, TRAINLOG_KEY_ENTER,
        '3', '\n', '2', '0', '\n', '\n', '\n', '\n'
    };
    const int table_events[] = {
        'a', '5', '\n', TRAINLOG_KEY_RIGHT, TRAINLOG_KEY_ENTER,
        '1', '5', '\n', 'f'
    };

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_catalog_create_exercise_profiled(database, "Tractions assistées",
        TRAINLOG_TRACKING_REPS, TRAINLOG_RECORDING_SETS, 0U, &created) ==
        TRAINLOG_STATUS_OK);
    script(&terminal, planning_events,
        sizeof(planning_events) / sizeof(planning_events[0]));
    tui_terminal = &terminal;
    CHECK(build_session_exercise(database, TRAINLOG_SESSION_TRAINING, &input,
        sets, sizeof(sets) / sizeof(sets[0])));
    CHECK(input.load_mode == TRAINLOG_LOAD_ASSISTANCE);
    CHECK(input.target_weight_kg == 20.0 && input.set_count == 0U);
    CHECK(strstr(terminal.output, "Assistance cible kg") != NULL);
    CHECK(strstr(terminal.output, "Séries réalisées") == NULL);
    CHECK(strstr(terminal.output, "Séries réellement faites") == NULL);
    CHECK(strstr(terminal.output, "Charge cible kg") == NULL);

    (void)memset(&draft, 0, sizeof(draft));
    draft.input = input;
    draft.input.sets = draft.sets;
    draft.tracking_mode = TRAINLOG_TRACKING_REPS;
    (void)snprintf(draft.name, sizeof(draft.name), "%s", created.name);
    script(&terminal, table_events,
        sizeof(table_events) / sizeof(table_events[0]));
    draft_edit_sets(&draft);
    CHECK(draft.input.set_count == 1U);
    CHECK(draft.sets[0].reps == 5);
    CHECK(draft.sets[0].reps != draft.input.target_reps);
    CHECK(draft.sets[0].has_weight && draft.sets[0].weight_kg == 15.0);
    CHECK(strstr(terminal.output, "Assistance") != NULL);
    CHECK(strstr(terminal.output, "Charge kg (vide") == NULL);
    tui_terminal = NULL;
    trainlog_database_close(database);
    return true;
}

static bool test_duration_creation_starts_empty(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogExercise created;
    TrainlogSessionExerciseInput input;
    TrainlogSetInput sets[4];
    TrainlogTerminal terminal;
    const int planning_events[] = {
        TRAINLOG_KEY_ENTER, TRAINLOG_KEY_ENTER,
        TRAINLOG_KEY_ENTER, TRAINLOG_KEY_ENTER,
        TRAINLOG_KEY_ENTER, TRAINLOG_KEY_ENTER
    };

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_catalog_create_exercise_profiled(database, "Gainage",
        TRAINLOG_TRACKING_DURATION, TRAINLOG_RECORDING_SETS, 0U, &created) ==
        TRAINLOG_STATUS_OK);
    script(&terminal, planning_events,
        sizeof(planning_events) / sizeof(planning_events[0]));
    tui_terminal = &terminal;
    CHECK(build_session_exercise(database, TRAINLOG_SESSION_TRAINING, &input,
        sets, sizeof(sets) / sizeof(sets[0])));
    CHECK(input.target_duration_seconds == 45);
    CHECK(input.set_count == 0U);
    CHECK(strstr(terminal.output, "Séries réellement faites") == NULL);
    CHECK(strstr(terminal.output, "Durée réalisée") == NULL);
    tui_terminal = NULL;
    trainlog_database_close(database);
    return true;
}

static bool test_append_requires_actual_and_rolls_back(void)
{
    TrainlogSessionDraftExercise draft;
    TrainlogSessionDraftExercise duration_draft;
    TrainlogTerminal terminal;
    const int accepted[] = {'7', '\n'};
    const int duration_accepted[] = {'1', ':', '3', '0', '\n'};
    const int cancelled[] = {27};

    (void)memset(&draft, 0, sizeof(draft));
    draft.input.recording_mode = TRAINLOG_RECORDING_SETS;
    draft.tracking_mode = TRAINLOG_TRACKING_REPS;
    draft.input.target_reps = 12;
    draft.input.set_count = 1U;
    draft.sets[0].reps = 5;

    script(&terminal, accepted, sizeof(accepted) / sizeof(accepted[0]));
    tui_terminal = &terminal;
    CHECK(draft_append_incomplete_set(&draft));
    CHECK(draft.input.set_count == 2U);
    CHECK(draft.sets[0].reps == 5);
    CHECK(draft.sets[1].reps == 7);
    CHECK(draft.sets[1].reps != draft.input.target_reps);

    script(&terminal, cancelled, sizeof(cancelled) / sizeof(cancelled[0]));
    CHECK(!draft_append_incomplete_set(&draft));
    CHECK(draft.input.set_count == 2U);
    CHECK(draft.sets[0].reps == 5 && draft.sets[1].reps == 7);

    (void)memset(&duration_draft, 0, sizeof(duration_draft));
    duration_draft.input.recording_mode = TRAINLOG_RECORDING_SETS;
    duration_draft.tracking_mode = TRAINLOG_TRACKING_DURATION;
    duration_draft.input.target_duration_seconds = 45;
    script(&terminal, duration_accepted,
        sizeof(duration_accepted) / sizeof(duration_accepted[0]));
    CHECK(draft_append_incomplete_set(&duration_draft));
    CHECK(duration_draft.input.set_count == 1U);
    CHECK(duration_draft.sets[0].duration_seconds == 90);
    CHECK(duration_draft.sets[0].duration_seconds !=
        duration_draft.input.target_duration_seconds);

    script(&terminal, cancelled, sizeof(cancelled) / sizeof(cancelled[0]));
    CHECK(!draft_append_incomplete_set(&duration_draft));
    CHECK(duration_draft.input.set_count == 1U);
    CHECK(duration_draft.sets[0].duration_seconds == 90);
    tui_terminal = NULL;
    return true;
}

static bool test_empty_sets_cannot_finish(void)
{
    TrainlogSessionDraftExercise draft;
    TrainlogTerminal terminal;
    TrainlogDatabase *database = NULL;
    size_t count = 1U;
    const int events[] = {'f', 'x', 'q'};

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    (void)memset(&draft, 0, sizeof(draft));
    draft.input.recording_mode = TRAINLOG_RECORDING_SETS;
    draft.tracking_mode = TRAINLOG_TRACKING_REPS;
    (void)snprintf(draft.name, sizeof(draft.name), "Squat");
    script(&terminal, events, sizeof(events) / sizeof(events[0]));
    tui_terminal = &terminal;
    CHECK(!edit_session_draft(database, &draft, &count,
        TRAINLOG_SESSION_TRAINING));
    CHECK(count == 1U && draft.input.set_count == 0U);
    CHECK(strstr(terminal.output,
        "Ajoutez au moins une série réalisée pour Squat.") != NULL);
    tui_terminal = NULL;
    trainlog_database_close(database);
    return true;
}

static bool test_knowledge_scrolls_long_lists_at_minimum_terminal(void)
{
    TrainlogExercise exercise;
    TrainlogTerminal terminal;
    const int events[] = {
        TRAINLOG_KEY_PAGE_DOWN, TRAINLOG_KEY_PAGE_DOWN, TRAINLOG_KEY_PAGE_DOWN,
        TRAINLOG_KEY_PAGE_DOWN, 'b'
    };

    (void)memset(&exercise, 0, sizeof(exercise));
    (void)snprintf(exercise.exercise_id, sizeof(exercise.exercise_id), "%s",
        "ex_a72fa713-4b0e-431d-95e2-42d95beb77b1");
    (void)snprintf(exercise.name, sizeof(exercise.name), "%s", "Lat pull");
    script(&terminal, events, sizeof(events) / sizeof(events[0]));
    terminal.rows = 20;
    terminal.columns = 72;
    tui_terminal = &terminal;
    screen_exercise_knowledge(&exercise);
    CHECK(!terminal.coordinate_overflow);
    CHECK(!terminal.text_overflow);
    CHECK(strstr(terminal.output, "Supra-épineux") != NULL);
    CHECK(strstr(terminal.output, "Sources :") != NULL);
    tui_terminal = NULL;
    return true;
}

int main(void)
{
    if (!test_assistance_creation_labels() ||
        !test_duration_creation_starts_empty() ||
        !test_append_requires_actual_and_rolls_back() ||
        !test_empty_sets_cannot_finish() ||
        !test_knowledge_scrolls_long_lists_at_minimum_terminal()) return 1;
    (void)printf("PASS tui_workflows\n");
    return 0;
}
