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
int trainlog_terminal_get_key(TrainlogTerminal *terminal)
{
    int key = terminal->event_index < terminal->event_count
        ? terminal->events[terminal->event_index++] : TRAINLOG_KEY_NONE;
    if (key == TRAINLOG_KEY_RESIZE) {
        terminal->rows = 20;
        terminal->columns = 72;
    }
    return key;
}
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

static size_t generator_zone_events(const char *zone_id, int *events)
{
    size_t index;
    for (index = 0U; index < trainlog_body_zone_catalog_count(); ++index) {
        const TrainlogBodyZone *zone = trainlog_body_zone_catalog_at(index);
        if (zone != NULL && strcmp(zone->zone_id, zone_id) == 0) {
            size_t event;
            for (event = 0U; event < index; ++event) events[event] = TRAINLOG_KEY_DOWN;
            events[index] = TRAINLOG_KEY_ENTER;
            return index + 1U;
        }
    }
    return 0U;
}

static bool seed_generator_leg_press(TrainlogDatabase *database)
{
    return trainlog_database_insert_exercise_profiled(database,
        "ex_b432623f-bfe9-4daf-a653-60ec7fdffbde", "Leg press", "leg press",
        TRAINLOG_TRACKING_REPS, TRAINLOG_RECORDING_SETS, 0U) == TRAINLOG_STATUS_OK;
}

static bool test_generator_dashboard_entry_and_policy_choices(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogTerminal terminal;
    const TrainlogSessionGenerationGoalPolicy *goal = NULL;
    int minutes = 0;
    const int dashboard_events[] = {'g'};
    const int goal_events[] = {TRAINLOG_KEY_DOWN, TRAINLOG_KEY_ENTER};
    const int duration_events[] = {TRAINLOG_KEY_DOWN, TRAINLOG_KEY_ENTER};
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    script(&terminal, dashboard_events, sizeof(dashboard_events) / sizeof(dashboard_events[0]));
    terminal.rows = 20; terminal.columns = 72; tui_terminal = &terminal;
    CHECK(screen_dashboard(database) == DASHBOARD_GENERATE_SESSION);
    CHECK(strstr(terminal.output, "g générer une séance") != NULL);
    CHECK(!terminal.coordinate_overflow && !terminal.text_overflow);
    script(&terminal, goal_events, sizeof(goal_events) / sizeof(goal_events[0]));
    CHECK(generator_choose_goal(&goal));
    CHECK(goal == &trainlog_session_generation_policy_v1.goals[1]);
    script(&terminal, duration_events, sizeof(duration_events) / sizeof(duration_events[0]));
    CHECK(generator_choose_duration(&minutes));
    CHECK(minutes == trainlog_session_generation_policy_v1.duration_presets_minutes[1]);
    tui_terminal = NULL; trainlog_database_close(database); return true;
}

static bool test_generator_plan_drafts_have_no_actual_sets(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogGeneratedSession generated;
    TrainlogSessionDraftExercise drafts[1];
    char summary[160];
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(seed_generator_leg_press(database));
    (void)memset(&generated, 0, sizeof(generated));
    generated.exercise_count = 1U;
    (void)snprintf(generated.exercises[0].exercise_id,
        sizeof(generated.exercises[0].exercise_id), "%s",
        "ex_b432623f-bfe9-4daf-a653-60ec7fdffbde");
    (void)snprintf(generated.exercises[0].equipment_id,
        sizeof(generated.exercises[0].equipment_id), "%s", "leg_press");
    generated.exercises[0].target_sets = 3;
    generated.exercises[0].target_repetitions = 10;
    generated.exercises[0].rest_seconds = 120;
    generated.exercises[0].planned_load_mode = TRAINLOG_LOAD_NONE;
    CHECK(generator_build_drafts(database, &generated, drafts));
    CHECK(drafts[0].input.target_sets == 3 && drafts[0].input.target_reps == 10);
    CHECK(drafts[0].input.rest_seconds == 120);
    CHECK(drafts[0].input.set_count == 0U && drafts[0].input.sets == drafts[0].sets);
    draft_set_summary(&drafts[0], summary, sizeof(summary));
    CHECK(strstr(summary, "Plan 3×10") != NULL);
    CHECK(strstr(summary, "0 réalisée") != NULL);
    trainlog_database_close(database); return true;
}

static bool test_generator_preview_scrolls_at_minimum_terminal(void)
{
    TrainlogGeneratedSession generated;
    TrainlogGeneratorPreviewItem items[TRAINLOG_GENERATOR_MAX_SELECTED];
    TrainlogTerminal terminal;
    size_t index;
    const int events[] = {TRAINLOG_KEY_RESIZE, TRAINLOG_KEY_DOWN, TRAINLOG_KEY_DOWN,
        TRAINLOG_KEY_DOWN, TRAINLOG_KEY_DOWN, TRAINLOG_KEY_DOWN, 'q'};
    (void)memset(&generated, 0, sizeof(generated));
    (void)memset(items, 0, sizeof(items));
    generated.exercise_count = TRAINLOG_GENERATOR_MAX_SELECTED;
    generated.estimated_duration_seconds = 1800;
    for (index = 0U; index < generated.exercise_count; ++index) {
        generated.exercises[index].target_sets = 3;
        generated.exercises[index].target_repetitions = 10;
        generated.exercises[index].rest_seconds = 120;
        (void)snprintf(items[index].exercise_name, sizeof(items[index].exercise_name), "Exercice %zu", index + 1U);
        (void)snprintf(items[index].equipment_name, sizeof(items[index].equipment_name), "Machine %zu", index + 1U);
        (void)snprintf(items[index].zone_name, sizeof(items[index].zone_name), "Cuisses");
        (void)snprintf(items[index].movement_name, sizeof(items[index].movement_name), "Extension du genou");
    }
    script(&terminal, events, sizeof(events) / sizeof(events[0]));
    terminal.rows = 30; terminal.columns = 100; tui_terminal = &terminal;
    CHECK(!generator_preview(&generated, items));
    CHECK(strstr(terminal.output, "Exercice 6") != NULL);
    CHECK(strstr(terminal.output, "aucune prescription numérique") != NULL);
    CHECK(!terminal.coordinate_overflow && !terminal.text_overflow);
    tui_terminal = NULL; return true;
}

static bool test_generator_recency_continue_and_cancel(void)
{
    TrainlogBodyZoneRecentExposure exposure;
    TrainlogTerminal terminal;
    const TrainlogBodyZone *zone = trainlog_body_zone_catalog_lookup("back");
    const int continue_events[] = {'c'};
    const int cancel_events[] = {'q'};
    (void)memset(&exposure, 0, sizeof(exposure));
    exposure.warning_level = TRAINLOG_GENERATION_WARNING_WARNING;
    exposure.within_24h.primary_set_count = 2U;
    exposure.within_72h.secondary_set_count = 4U;
    exposure.has_latest = true;
    (void)snprintf(exposure.latest_started_at, sizeof(exposure.latest_started_at),
        "%s", "2026-09-10T08:00:00.250+02:00");
    CHECK(zone != NULL);
    script(&terminal, continue_events, sizeof(continue_events) / sizeof(continue_events[0]));
    terminal.rows = 20; terminal.columns = 72; tui_terminal = &terminal;
    CHECK(generator_confirm_exposure(zone, &exposure) == 1);
    CHECK(strstr(terminal.output, "Dos travaillé récemment") != NULL);
    CHECK(strstr(terminal.output, "2 séries principales") != NULL);
    CHECK(!terminal.coordinate_overflow && !terminal.text_overflow);
    script(&terminal, cancel_events, sizeof(cancel_events) / sizeof(cancel_events[0]));
    CHECK(generator_confirm_exposure(zone, &exposure) == -1);
    tui_terminal = NULL; return true;
}

static bool test_generator_cancel_preview_writes_nothing(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogTerminal terminal;
    int events[32];
    size_t count = generator_zone_events("thighs", events);
    size_t sessions = 99U;
    CHECK(count > 0U);
    events[count++] = TRAINLOG_KEY_ENTER;
    events[count++] = TRAINLOG_KEY_ENTER;
    events[count++] = 'q';
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(seed_generator_leg_press(database));
    script(&terminal, events, count); tui_terminal = &terminal;
    screen_session_generator(database);
    CHECK(trainlog_database_session_count(database, &sessions) == TRAINLOG_STATUS_OK);
    CHECK(sessions == 0U);
    CHECK(strstr(terminal.output, "Aperçu de la séance générée") != NULL);
    tui_terminal = NULL; trainlog_database_close(database); return true;
}

static bool test_generator_accept_requires_and_persists_actual(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogTerminal terminal;
    int events[48];
    size_t count = generator_zone_events("thighs", events);
    size_t sessions = 0U;
    CHECK(count > 0U);
    events[count++] = TRAINLOG_KEY_ENTER;
    events[count++] = TRAINLOG_KEY_ENTER;
    events[count++] = 'a';
    events[count++] = TRAINLOG_KEY_ENTER;
    events[count++] = 'a'; events[count++] = '8'; events[count++] = '\n';
    events[count++] = 'f'; events[count++] = 'f'; events[count++] = 'x';
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(seed_generator_leg_press(database));
    script(&terminal, events, count); tui_terminal = &terminal;
    screen_session_generator(database);
    CHECK(trainlog_database_session_count(database, &sessions) == TRAINLOG_STATUS_OK);
    CHECK(sessions == 1U);
    CHECK(strstr(terminal.output, "Valeurs réellement effectuées") != NULL);
    CHECK(strstr(terminal.output, "Séance générée et réalisée enregistrée") != NULL);
    tui_terminal = NULL; trainlog_database_close(database); return true;
}

static bool test_generator_empty_knowledge_cannot_save(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogTerminal terminal;
    const int events[] = {TRAINLOG_KEY_ENTER, TRAINLOG_KEY_ENTER,
        TRAINLOG_KEY_ENTER, 'x'};
    size_t sessions = 99U;
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    script(&terminal, events, sizeof(events) / sizeof(events[0]));
    tui_terminal = &terminal; screen_session_generator(database);
    CHECK(trainlog_database_session_count(database, &sessions) == TRAINLOG_STATUS_OK);
    CHECK(sessions == 0U);
    CHECK(strstr(terminal.output, "Pas assez d’exercices résolus") != NULL);
    CHECK(strstr(terminal.output, "rien ne peut être enregistré") != NULL);
    tui_terminal = NULL; trainlog_database_close(database); return true;
}

int main(void)
{
    if (!test_assistance_creation_labels() ||
        !test_duration_creation_starts_empty() ||
        !test_append_requires_actual_and_rolls_back() ||
        !test_empty_sets_cannot_finish() ||
        !test_knowledge_scrolls_long_lists_at_minimum_terminal() ||
        !test_generator_dashboard_entry_and_policy_choices() ||
        !test_generator_plan_drafts_have_no_actual_sets() ||
        !test_generator_preview_scrolls_at_minimum_terminal() ||
        !test_generator_recency_continue_and_cancel() ||
        !test_generator_cancel_preview_writes_nothing() ||
        !test_generator_accept_requires_and_persists_actual() ||
        !test_generator_empty_knowledge_cannot_save()) return 1;
    (void)printf("PASS tui_workflows\n");
    return 0;
}
