/**
 * @file test_tui_workflows.c
 * @brief Scripted regressions through the production TUI workflow functions.
 */

#include <errno.h>
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
    size_t surface_draw_count;
};

struct TrainlogPanel { int unused; };
struct TrainlogSurface { TrainlogTerminal *terminal; };

#define CHECK(condition) do { if (!(condition)) {                           \
    (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n",                  \
        __FILE__, __LINE__, #condition); return false; } } while (0)

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
bool trainlog_terminal_refresh_geometry(TrainlogTerminal *terminal)
{ (void)terminal; return true; }
TrainlogPanel *tui_panel_create(TrainlogTerminal *terminal, int height, int width, int top, int left) { (void)terminal; (void)height; (void)width; (void)top; (void)left; return NULL; }
void tui_panel_destroy(TrainlogPanel *panel) { (void)panel; }
void tui_panel_box(TrainlogPanel *panel) { (void)panel; }
void tui_panel_style_on(TrainlogPanel *panel, TrainlogTextStyle style) { (void)panel; (void)style; }
void tui_panel_style_off(TrainlogPanel *panel, TrainlogTextStyle style) { (void)panel; (void)style; }
void tui_panel_print(TrainlogPanel *panel, int row, int column, const char *format, ...) { (void)panel; (void)row; (void)column; (void)format; }
void tui_panel_commit(TrainlogPanel *panel) { (void)panel; }
TrainlogSurface *trainlog_surface_create(TrainlogTerminal *terminal,
    const char *name, int top, int left, int height, int width)
{
    static TrainlogSurface surface;
    (void)name; (void)top; (void)left; (void)height; (void)width;
    surface.terminal = terminal; return &surface;
}
bool trainlog_surface_set_rect(TrainlogSurface *surface, int top, int left,
    int height, int width)
{
    (void)surface; (void)top; (void)left; (void)height; (void)width; return true;
}
void trainlog_surface_destroy(TrainlogSurface *surface) { (void)surface; }
void trainlog_surface_erase(TrainlogSurface *surface) { (void)surface; }
void trainlog_surface_set_role(TrainlogSurface *surface,
    TrainlogColorRole foreground, unsigned background_rgb, TrainlogTextStyle style)
{
    (void)surface; (void)foreground; (void)background_rgb; (void)style;
}
void trainlog_surface_printf(TrainlogSurface *surface, int row, int column,
    const char *format, ...)
{
    va_list arguments;
    if (surface == NULL || surface->terminal == NULL) return;
    va_start(arguments, format);
    if (surface->terminal->output_used < sizeof(surface->terminal->output)) {
        int written = vsnprintf(surface->terminal->output + surface->terminal->output_used,
            sizeof(surface->terminal->output) - surface->terminal->output_used,
            format, arguments);
        if (written > 0 && (size_t)written < sizeof(surface->terminal->output) -
            surface->terminal->output_used) surface->terminal->output_used += (size_t)written;
    }
    va_end(arguments); (void)row; (void)column;
}
void trainlog_surface_draw(TrainlogSurface *surface, int row, int column,
    uint32_t codepoint)
{
    if (surface != NULL && surface->terminal != NULL)
        ++surface->terminal->surface_draw_count;
    (void)row; (void)column; (void)codepoint;
}
void trainlog_surface_move_top(TrainlogSurface *surface) { (void)surface; }

static bool test_knowledge_scrolls_long_lists_at_minimum_terminal(void)
{
    TrainlogAppContext app;
    TrainlogTerminal terminal;
    TrainlogSurface surface;
    size_t page;
    (void)memset(&app, 0, sizeof(app));
    (void)snprintf(app.exercise_detail.exercise_id,
        sizeof(app.exercise_detail.exercise_id), "%s",
        "ex_a72fa713-4b0e-431d-95e2-42d95beb77b1");
    (void)snprintf(app.exercise_detail.name, sizeof(app.exercise_detail.name),
        "%s", "Lat pull");
    (void)memset(&terminal, 0, sizeof(terminal));
    terminal.rows = 20;
    terminal.columns = 72;
    surface.terminal = &terminal;
    app.terminal = &terminal;
    app.content = &surface;
    app.layout.usable = true;
    app.layout.content.height = 16;
    app.layout.content.width = 72;
    app.focus = TRAINLOG_FOCUS_CONTENT;
    app.navigation.current.route = TRAINLOG_ROUTE_EXERCISE_KNOWLEDGE;
    for (page = 0U; page < 5U; ++page) {
        app_shell_render_content(&app);
        app_shell_dispatch(&app, TRAINLOG_KEY_PAGE_DOWN);
    }
    CHECK(!terminal.coordinate_overflow);
    CHECK(!terminal.text_overflow);
    CHECK(strstr(terminal.output, "Supra-épineux") != NULL);
    CHECK(strstr(terminal.output, "Sources :") != NULL);
    return true;
}

static bool seed_generator_leg_press(TrainlogDatabase *database)
{
    return trainlog_database_insert_exercise_profiled(database,
        "ex_b432623f-bfe9-4daf-a653-60ec7fdffbde", "Leg press", "leg press",
        TRAINLOG_TRACKING_REPS, TRAINLOG_RECORDING_SETS, 0U) == TRAINLOG_STATUS_OK;
}

static bool test_shell_session_navigation_preserves_run_draft(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogAppContext app;
    size_t sessions = 99U;
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    (void)memset(&app, 0, sizeof(app));
    app.database = database;
    trainlog_navigation_init(&app.navigation);
    app.navigation.current.route = TRAINLOG_ROUTE_SESSION_CURRENT;
    app.session.has_draft = true;
    app.session.dirty = true;
    app.session.draft_count = 1U;
    (void)snprintf(app.session.drafts[0].input.exercise_id,
        sizeof(app.session.drafts[0].input.exercise_id), "%s",
        "ex_11111111-1111-4111-8111-111111111111");
    app_shell_open_route(&app, TRAINLOG_ROUTE_EXERCISES);
    CHECK(app.navigation.current.route == TRAINLOG_ROUTE_EXERCISES);
    CHECK(app.session.has_draft && app.session.draft_count == 1U);
    CHECK(strcmp(app.session.drafts[0].input.exercise_id,
        "ex_11111111-1111-4111-8111-111111111111") == 0);
    CHECK(trainlog_database_session_count(database, &sessions) == TRAINLOG_STATUS_OK);
    CHECK(sessions == 0U);
    trainlog_database_close(database);
    return true;
}

static bool test_shell_generator_keep_and_accept_has_no_actuals(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogAppContext app;
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(seed_generator_leg_press(database));
    (void)memset(&app, 0, sizeof(app));
    app.database = database;
    trainlog_navigation_init(&app.navigation);
    app.navigation.current.route = TRAINLOG_ROUTE_SESSION_GENERATOR;
    app.session.phase = TRAINLOG_SESSION_GENERATOR_PREVIEW;
    app.session.generated_preview = true;
    app.session.generated.exercise_count = 1U;
    (void)snprintf(app.session.generated.exercises[0].exercise_id,
        sizeof(app.session.generated.exercises[0].exercise_id), "%s",
        "ex_b432623f-bfe9-4daf-a653-60ec7fdffbde");
    (void)snprintf(app.session.generated.exercises[0].equipment_id,
        sizeof(app.session.generated.exercises[0].equipment_id), "%s", "leg_press");
    app.session.generated.exercises[0].target_sets = 3;
    app.session.generated.exercises[0].target_repetitions = 10;
    app.session.generated.exercises[0].rest_seconds = 120;
    app_shell_open_route(&app, TRAINLOG_ROUTE_EXERCISES);
    CHECK(app.navigation.current.route == TRAINLOG_ROUTE_SESSION_GENERATOR);
    CHECK(app.session.phase == TRAINLOG_SESSION_CONFIRM_LEAVE);
    CHECK(app_shell_dispatch_session(&app, TRAINLOG_KEY_ENTER));
    CHECK(app.navigation.current.route == TRAINLOG_ROUTE_EXERCISES);
    CHECK(app.session.generated_preview);
    app.navigation.current.route = TRAINLOG_ROUTE_SESSION_GENERATOR;
    app.session.phase = TRAINLOG_SESSION_GENERATOR_PREVIEW;
    CHECK(session_controller_accept_generator(&app));
    CHECK(app.session.has_draft && app.session.draft_count == 1U);
    CHECK(app.session.drafts[0].input.target_sets == 3);
    CHECK(app.session.drafts[0].input.set_count == 0U);
    trainlog_database_close(database);
    return true;
}

static bool test_shell_session_forms_keep_plan_and_actuals_separate(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogAppContext app;
    TrainlogSessionDraftExercise *draft;
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    (void)memset(&app, 0, sizeof(app)); app.database = database;
    trainlog_navigation_init(&app.navigation);
    app.navigation.current.route = TRAINLOG_ROUTE_SESSION_CURRENT;
    app.session.has_draft = true; app.session.draft_count = 1U;
    app.session.phase = TRAINLOG_SESSION_DRAFT;
    draft = &app.session.drafts[0];
    draft->tracking_mode = TRAINLOG_TRACKING_REPS;
    draft->input.recording_mode = TRAINLOG_RECORDING_SETS;
    draft->input.target_sets = 3; draft->input.target_reps = 10;
    draft_bind_input(draft);
    CHECK(app_shell_dispatch_session(&app, 'e'));
    CHECK(app_shell_dispatch_session(&app, 'a'));
    CHECK(app_shell_dispatch_session(&app, '8'));
    CHECK(app_shell_dispatch_session(&app, TRAINLOG_KEY_ENTER));
    CHECK(draft->input.set_count == 1U && draft->sets[0].reps == 8);
    CHECK(draft->input.target_sets == 3 && draft->input.target_reps == 10);
    trainlog_database_close(database); return true;
}

static bool test_shell_continuous_supplemental_fields_have_no_fake_set(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogAppContext app;
    TrainlogSessionDraftExercise *draft;
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    (void)memset(&app, 0, sizeof(app)); app.database = database;
    app.navigation.current.route = TRAINLOG_ROUTE_SESSION_CURRENT;
    app.session.has_draft = true; app.session.draft_count = 1U;
    app.session.phase = TRAINLOG_SESSION_ACTUALS;
    draft = &app.session.drafts[0];
    draft->tracking_mode = TRAINLOG_TRACKING_DURATION;
    draft->input.recording_mode = TRAINLOG_RECORDING_CONTINUOUS;
    draft->input.data_fields = TRAINLOG_EXERCISE_DATA_SPEED_KMH |
        TRAINLOG_EXERCISE_DATA_DISTANCE_KM;
    draft_bind_input(draft);
    CHECK(app_shell_dispatch_session(&app, 'e'));
    CHECK(app_shell_dispatch_session(&app, '9'));
    CHECK(app_shell_dispatch_session(&app, '0'));
    CHECK(app_shell_dispatch_session(&app, TRAINLOG_KEY_ENTER));
    CHECK(app_shell_dispatch_session(&app, 's'));
    CHECK(app_shell_dispatch_session(&app, '1'));
    CHECK(app_shell_dispatch_session(&app, '2'));
    CHECK(app_shell_dispatch_session(&app, TRAINLOG_KEY_ENTER));
    CHECK(app_shell_dispatch_session(&app, 'k'));
    CHECK(app_shell_dispatch_session(&app, '2'));
    CHECK(app_shell_dispatch_session(&app, '.'));
    CHECK(app_shell_dispatch_session(&app, '5'));
    CHECK(app_shell_dispatch_session(&app, TRAINLOG_KEY_ENTER));
    CHECK(draft->input.continuous_duration_seconds == 90);
    CHECK(draft->input.continuous_has_speed && draft->input.continuous_speed_kmh == 12.0);
    CHECK(draft->input.continuous_has_distance && draft->input.continuous_distance_km == 2.5);
    CHECK(draft->input.set_count == 0U && !draft->input.has_max_weight);
    trainlog_database_close(database); return true;
}

static bool test_shell_generator_dose_edit_drops_stale_load_provenance(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogAppContext app;
    TrainlogGeneratedExercise *generated;
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    (void)memset(&app, 0, sizeof(app)); app.database = database;
    app.navigation.current.route = TRAINLOG_ROUTE_SESSION_GENERATOR;
    app.session.generation_zone = trainlog_body_zone_catalog_lookup("thighs");
    app.session.generation_goal = &trainlog_session_generation_policy_v1.goals[0];
    app.session.generation_duration_minutes = 30;
    CHECK(seed_generator_leg_press(database));
    CHECK(session_controller_generate(&app));
    CHECK(app.session.generated.exercise_count == 1U);
    generated = &app.session.generated.exercises[0];
    generated->target_sets = 3; generated->target_repetitions = 10;
    generated->has_target_weight = true; generated->target_weight_kg = 80.0;
    (void)snprintf(generated->load_source_session_id,
        sizeof(generated->load_source_session_id), "se_source");
    CHECK(app_shell_dispatch_session(&app, 'e'));
    CHECK(app_shell_dispatch_session(&app, TRAINLOG_KEY_BACKSPACE));
    CHECK(app_shell_dispatch_session(&app, '4'));
    CHECK(app_shell_dispatch_session(&app, TRAINLOG_KEY_ENTER));
    CHECK(app_shell_dispatch_session(&app, TRAINLOG_KEY_BACKSPACE));
    CHECK(app_shell_dispatch_session(&app, TRAINLOG_KEY_BACKSPACE));
    CHECK(app_shell_dispatch_session(&app, '1'));
    CHECK(app_shell_dispatch_session(&app, '2'));
    CHECK(app_shell_dispatch_session(&app, TRAINLOG_KEY_ENTER));
    CHECK(generated->target_sets == 4 && generated->target_repetitions == 12);
    CHECK(!generated->has_target_weight && generated->load_source_session_id[0] == '\0');
    trainlog_database_close(database); return true;
}

static bool test_shell_inline_equipment_creation_resumes_occurrence(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogAppContext app;
    const char *name = "Cable maison";
    const char *type = "cable";
    size_t index;
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    (void)memset(&app, 0, sizeof(app)); app.database = database;
    app.navigation.current.route = TRAINLOG_ROUTE_SESSION_CURRENT;
    app.session.has_draft = true; app.session.draft_count = 1U;
    app.session.phase = TRAINLOG_SESSION_EQUIPMENT_PICKER;
    CHECK(app_shell_dispatch_session(&app, 'n'));
    for (index = 0U; name[index] != '\0'; ++index)
        CHECK(app_shell_dispatch_session(&app, (unsigned char)name[index]));
    CHECK(app_shell_dispatch_session(&app, TRAINLOG_KEY_ENTER));
    CHECK(app_shell_dispatch_session(&app, TRAINLOG_KEY_ENTER));
    for (index = 0U; type[index] != '\0'; ++index)
        CHECK(app_shell_dispatch_session(&app, (unsigned char)type[index]));
    CHECK(app_shell_dispatch_session(&app, TRAINLOG_KEY_ENTER));
    CHECK(app_shell_dispatch_session(&app, TRAINLOG_KEY_ENTER));
    CHECK(app.session.phase == TRAINLOG_SESSION_EQUIPMENT_PICKER);
    CHECK(app.session.equipment_selected > 0U);
    CHECK(app_shell_dispatch_session(&app, TRAINLOG_KEY_ENTER));
    CHECK(app.session.phase == TRAINLOG_SESSION_DRAFT);
    CHECK(app.session.drafts[0].input.equipment_id[0] != '\0');
    CHECK(app.session.drafts[0].input.load_mode == TRAINLOG_LOAD_EXTERNAL);
    trainlog_database_close(database); return true;
}

static bool test_shell_equipment_catalog_create_preserves_invalid_form(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogAppContext app;
    TrainlogCustomEquipment items[2];
    size_t count = 0U;
    const char *name = "Poulie personnelle";
    const char *type = "cable";
    size_t index;
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    (void)memset(&app, 0, sizeof(app));
    app.database = database;
    app.navigation.current.route = TRAINLOG_ROUTE_EQUIPMENT;
    app.layout.content.height = 20;
    trainlog_list_init(&app.list);
    equipment_controller_start(&app);
    CHECK(app_shell_dispatch_equipment_controller(&app, TRAINLOG_KEY_ENTER));
    CHECK(app.equipment_controller.phase == TRAINLOG_EQUIPMENT_NAME);
    CHECK(app.equipment_controller.form.active);
    CHECK(strstr(app.equipment_controller.message, "obligatoire") != NULL);
    for (index = 0U; name[index] != '\0'; ++index)
        CHECK(app_shell_dispatch_equipment_controller(&app,
            (unsigned char)name[index]));
    app_shell_open_route(&app, TRAINLOG_ROUTE_HOME);
    CHECK(app.navigation.current.route == TRAINLOG_ROUTE_EQUIPMENT);
    CHECK(app.equipment_controller.phase == TRAINLOG_EQUIPMENT_CONFIRM_DISCARD);
    CHECK(strcmp(app.equipment_controller.form.text, name) == 0);
    CHECK(app_shell_dispatch_equipment_controller(&app, '0'));
    CHECK(app.equipment_controller.phase == TRAINLOG_EQUIPMENT_NAME);
    CHECK(app_shell_dispatch_equipment_controller(&app, TRAINLOG_KEY_ENTER));
    CHECK(app_shell_dispatch_equipment_controller(&app, TRAINLOG_KEY_ENTER));
    for (index = 0U; type[index] != '\0'; ++index)
        CHECK(app_shell_dispatch_equipment_controller(&app,
            (unsigned char)type[index]));
    CHECK(app_shell_dispatch_equipment_controller(&app, TRAINLOG_KEY_ENTER));
    CHECK(app.equipment_controller.phase == TRAINLOG_EQUIPMENT_LOAD);
    CHECK(app_shell_dispatch_equipment_controller(&app, '3'));
    CHECK(app_shell_dispatch_equipment_controller(&app, TRAINLOG_KEY_ENTER));
    CHECK(app.equipment_controller.phase == TRAINLOG_EQUIPMENT_MESSAGE);
    CHECK(trainlog_database_list_custom_equipment(database, items, 2U,
        &count) == TRAINLOG_STATUS_OK);
    CHECK(count == 1U);
    CHECK(strcmp(items[0].display_name, name) == 0);
    CHECK(strcmp(items[0].equipment_type, type) == 0);
    CHECK(strcmp(items[0].load_semantics, "assistance") == 0);
    trainlog_database_close(database);
    return true;
}

static bool test_shell_inline_exercise_creation_and_stable_edit(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogAppContext app;
    TrainlogExercise created;
    TrainlogExercise listed[4];
    size_t count = 0U;
    const char *name = "Course inclinée";
    size_t index;
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    (void)memset(&app, 0, sizeof(app));
    app.database = database;
    app.navigation.current.route = TRAINLOG_ROUTE_SESSION_CURRENT;
    app.session.phase = TRAINLOG_SESSION_EXERCISE_PICKER;
    exercise_controller_start_create(&app, true);
    for (index = 0U; name[index] != '\0'; ++index)
        CHECK(app_shell_dispatch_exercise_controller(&app,
            (unsigned char)name[index]));
    CHECK(app_shell_dispatch_exercise_controller(&app, TRAINLOG_KEY_ENTER));
    CHECK(app_shell_dispatch_exercise_controller(&app, '2'));
    CHECK(app_shell_dispatch_exercise_controller(&app, TRAINLOG_KEY_ENTER));
    CHECK(app_shell_dispatch_exercise_controller(&app, '2'));
    CHECK(app_shell_dispatch_exercise_controller(&app, TRAINLOG_KEY_ENTER));
    CHECK(app_shell_dispatch_exercise_controller(&app, '1'));
    CHECK(app_shell_dispatch_exercise_controller(&app, TRAINLOG_KEY_ENTER));
    CHECK(app_shell_dispatch_exercise_controller(&app, '0'));
    CHECK(app_shell_dispatch_exercise_controller(&app, TRAINLOG_KEY_ENTER));
    CHECK(app.exercise_controller.phase == TRAINLOG_EXERCISE_ZONES);
    CHECK(app_shell_dispatch_exercise_controller(&app, TRAINLOG_KEY_ENTER));
    CHECK(app.exercise_controller.phase == TRAINLOG_EXERCISE_IDLE);
    CHECK(app.session.phase == TRAINLOG_SESSION_EXERCISE_PICKER);
    CHECK(app.session.picker_count == 1U);
    created = app.session.picker[app.session.picker_selected];
    CHECK(created.recording_mode == TRAINLOG_RECORDING_CONTINUOUS);
    CHECK(created.tracking_mode == TRAINLOG_TRACKING_DURATION);
    CHECK((created.data_fields & TRAINLOG_EXERCISE_DATA_SPEED_KMH) != 0U);
    app.exercise_detail = created;
    exercise_controller_start_edit(&app);
    trainlog_form_init(&app.exercise_controller.form, "Course renommée");
    app.exercise_controller.dirty = true;
    CHECK(app_shell_dispatch_exercise_controller(&app, TRAINLOG_KEY_ENTER));
    CHECK(app.exercise_controller.phase == TRAINLOG_EXERCISE_ZONES);
    CHECK(app_shell_dispatch_exercise_controller(&app, TRAINLOG_KEY_ENTER));
    CHECK(strcmp(app.exercise_detail.exercise_id, created.exercise_id) == 0);
    CHECK(strcmp(app.exercise_detail.name, "Course renommée") == 0);
    CHECK(trainlog_database_list_exercises(database, listed, 4U,
        &count) == TRAINLOG_STATUS_OK);
    CHECK(count == 1U && strcmp(listed[0].exercise_id, created.exercise_id) == 0);
    trainlog_database_close(database);
    return true;
}

static bool test_shell_navigation_focus_consumes_input(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogAppContext app;
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    (void)memset(&app, 0, sizeof(app));
    app.database = database;
    app.layout.usable = true;
    app.layout.sidebar_visible = true;
    app.layout.content.height = 20;
    app.focus = TRAINLOG_FOCUS_CONTENT;
    trainlog_navigation_init(&app.navigation);
    app_shell_dispatch(&app, TRAINLOG_KEY_F6);
    CHECK(app.focus == TRAINLOG_FOCUS_NAVIGATION);
    app_shell_dispatch(&app, TRAINLOG_KEY_ESCAPE);
    CHECK(app.focus == TRAINLOG_FOCUS_CONTENT);
    CHECK(app.navigation.current.route == TRAINLOG_ROUTE_HOME);
    app_shell_dispatch(&app, TRAINLOG_KEY_SHIFT_TAB);
    CHECK(app.focus == TRAINLOG_FOCUS_SEARCH);
    app_shell_dispatch(&app, TRAINLOG_KEY_SHIFT_TAB);
    CHECK(app.focus == TRAINLOG_FOCUS_NAVIGATION);
    app.navigation_selected = 3U;
    app_shell_dispatch(&app, TRAINLOG_KEY_ENTER);
    CHECK(app.navigation.current.route == TRAINLOG_ROUTE_EQUIPMENT);
    CHECK(app.focus == TRAINLOG_FOCUS_CONTENT);
    trainlog_database_close(database);
    return true;
}

static bool test_shell_actions_overlay_executes_registered_search_action(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogAppContext app;
    const TrainlogAction *search_action;
    TrainlogCustomEquipment equipment;
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    (void)memset(&equipment, 0, sizeof(equipment));
    generate_custom_equipment_uuid(equipment.equipment_id);
    (void)snprintf(equipment.display_name, sizeof(equipment.display_name),
        "overlay search needle");
    (void)snprintf(equipment.equipment_type, sizeof(equipment.equipment_type),
        "machine");
    (void)snprintf(equipment.load_semantics, sizeof(equipment.load_semantics),
        "external");
    CHECK(trainlog_database_create_custom_equipment(database, &equipment) ==
        TRAINLOG_STATUS_OK);
    (void)memset(&app, 0, sizeof(app));
    app.database = database;
    app.layout.usable = true;
    app.layout.content.height = 20;
    app.focus = TRAINLOG_FOCUS_CONTENT;
    trainlog_navigation_init(&app.navigation);
    app.navigation.current.route = TRAINLOG_ROUTE_EQUIPMENT;
    trainlog_search_init(&app.search);
    app_shell_refresh_list(&app);
    app_shell_actions(&app);
    search_action = trainlog_actions_find_key(&app.actions, '/');
    CHECK(search_action != NULL);
    CHECK(strcmp(search_action->identifier, "list.search") == 0);
    CHECK(search_action->intent == TRAINLOG_INTENT_OPEN_SEARCH);
    app_shell_dispatch(&app, TRAINLOG_KEY_F7);
    CHECK(trainlog_overlays_top(&app.overlays)->type == TRAINLOG_OVERLAY_ACTIONS);
    app_shell_dispatch(&app, TRAINLOG_KEY_DOWN);
    app_shell_dispatch(&app, TRAINLOG_KEY_ENTER);
    CHECK(app.focus == TRAINLOG_FOCUS_SEARCH);
    CHECK(app.search.open && app.search.focused);
    app_shell_dispatch(&app, 'n');
    app_shell_dispatch(&app, 'e');
    app_shell_dispatch(&app, 'e');
    app_shell_dispatch(&app, 'd');
    app_shell_dispatch(&app, 'l');
    app_shell_dispatch(&app, 'e');
    CHECK(strcmp(app.search.text, "needle") == 0);
    CHECK(app.loaded_count == 1U);
    CHECK(strcmp(app.equipment[0].display_name, "overlay search needle") == 0);
    app_shell_dispatch(&app, TRAINLOG_KEY_ESCAPE);
    CHECK(app.search.focused && app.search.bytes == 0U);
    CHECK(app.loaded_count > 1U);
    app_shell_dispatch(&app, TRAINLOG_KEY_ESCAPE);
    CHECK(!app.search.focused && app.focus == TRAINLOG_FOCUS_CONTENT);
    trainlog_database_close(database);
    return true;
}

static bool test_shell_compact_navigation_focus_contract(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogAppContext app;
    TrainlogTerminal terminal = {0};
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    (void)memset(&app, 0, sizeof(app));
    terminal.columns = 120; terminal.rows = 35;
    app.database = database; app.terminal = &terminal;
    app.focus = TRAINLOG_FOCUS_CONTENT;
    trainlog_navigation_init(&app.navigation);
    CHECK(app_shell_layout(&app));
    app_shell_dispatch(&app, TRAINLOG_KEY_F6);
    CHECK(app.focus == TRAINLOG_FOCUS_NAVIGATION);
    app.navigation_selected = 3U;
    terminal.columns = 100; terminal.rows = 25;
    CHECK(app_shell_layout(&app));
    CHECK(!app.layout.sidebar_visible && app.focus == TRAINLOG_FOCUS_NAVIGATION);
    app_shell_dispatch(&app, TRAINLOG_KEY_ENTER);
    CHECK(trainlog_overlays_top(&app.overlays)->type == TRAINLOG_OVERLAY_NAVIGATION);
    app_shell_dispatch(&app, TRAINLOG_KEY_ESCAPE);
    CHECK(app.focus == TRAINLOG_FOCUS_NAVIGATION && app.navigation_selected == 3U);
    app_shell_dispatch(&app, TRAINLOG_KEY_SHIFT_TAB);
    CHECK(app.focus == TRAINLOG_FOCUS_ACTIONS);
    app_shell_dispatch(&app, TRAINLOG_KEY_SHIFT_TAB);
    CHECK(app.focus == TRAINLOG_FOCUS_CONTENT);
    app_shell_dispatch(&app, TRAINLOG_KEY_SHIFT_TAB);
    CHECK(app.focus == TRAINLOG_FOCUS_SEARCH && app.search.focused);
    app_shell_dispatch(&app, TRAINLOG_KEY_SHIFT_TAB);
    CHECK(app.focus == TRAINLOG_FOCUS_NAVIGATION && !app.search.focused);
    app_shell_dispatch(&app, TRAINLOG_KEY_TAB);
    CHECK(app.focus == TRAINLOG_FOCUS_SEARCH && app.search.focused);
    app.navigation.current.route = TRAINLOG_ROUTE_EQUIPMENT;
    app.search.open = true;
    app_shell_dispatch(&app, TRAINLOG_KEY_F6);
    CHECK(trainlog_overlays_top(&app.overlays)->type == TRAINLOG_OVERLAY_NAVIGATION);
    app_shell_dispatch(&app, TRAINLOG_KEY_ESCAPE);
    CHECK(app.focus == TRAINLOG_FOCUS_SEARCH && app.search.focused);
    app.focus = TRAINLOG_FOCUS_ACTIONS; app.search.focused = false;
    app_shell_dispatch(&app, TRAINLOG_KEY_ENTER);
    CHECK(trainlog_overlays_top(&app.overlays)->type == TRAINLOG_OVERLAY_ACTIONS);
    trainlog_database_close(database);
    return true;
}

static bool test_shell_detail_back_restores_stable_list_selection(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogAppContext app;
    char selected_id[TRAINLOG_SHELL_STABLE_ID_CAPACITY];
    size_t selected_index;
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    (void)memset(&app, 0, sizeof(app));
    app.database = database;
    app.layout.content.height = 16;
    trainlog_navigation_init(&app.navigation);
    app.navigation.current.route = TRAINLOG_ROUTE_EQUIPMENT;
    trainlog_list_init(&app.list);
    trainlog_search_init(&app.search);
    app_shell_refresh_list(&app);
    CHECK(app.loaded_count > 1U);
    trainlog_list_move(&app.list, app.stable_ids, 1);
    (void)snprintf(selected_id, sizeof(selected_id), "%s",
        app.list.selected_id);
    selected_index = app.list.selected_index;
    app_shell_primary(&app);
    CHECK(app.navigation.current.route == TRAINLOG_ROUTE_EQUIPMENT_DETAIL);
    CHECK(app_shell_back(&app));
    CHECK(app.navigation.current.route == TRAINLOG_ROUTE_EQUIPMENT);
    CHECK(strcmp(app.list.selected_id, selected_id) == 0);
    CHECK(app.list.selected_index == selected_index);
    trainlog_database_close(database);
    return true;
}

static int sync_probe_calls;
static int sync_run_calls;
static TrainlogSyncDirection sync_last_direction;
static TrainlogAppContext *sync_running_app;
static char sync_journal_root[PATH_MAX + 1U];

static TrainlogStatus mock_sync_probe(TrainlogSyncDeviceInfo *output)
{
    ++sync_probe_calls;
    (void)memset(output, 0, sizeof(*output));
    output->connected = true;
    output->storage_ready = true;
    (void)snprintf(output->device.vendor, sizeof(output->device.vendor), "Test");
    (void)snprintf(output->device.model, sizeof(output->device.model), "Phone");
    return TRAINLOG_STATUS_OK;
}

static TrainlogStatus mock_sync_run(TrainlogSyncTrigger trigger,
    bool require_request, TrainlogSyncDirection direction,
    TrainlogSyncReport *output)
{
    char directory[PATH_MAX + 1U];
    char history_path[PATH_MAX + 1U];
    FILE *history;
    int run_number;
    CHECK(trigger == TRAINLOG_SYNC_TRIGGER_TUI);
    CHECK(!require_request);
    ++sync_run_calls;
    run_number = sync_run_calls;
    sync_last_direction = direction;
    CHECK(sync_running_app != NULL && sync_running_app->sync_controller.running);
    CHECK(strstr(sync_running_app->terminal->output,
        "Synchronisation en cours") != NULL);
    /* A re-entrant launch attempt is consumed while the sole run is active. */
    CHECK(app_shell_dispatch_sync(sync_running_app, 's'));
    CHECK(app_shell_dispatch_sync(sync_running_app, TRAINLOG_KEY_ENTER));
    CHECK(sync_run_calls == run_number);
    (void)memset(output, 0, sizeof(*output));
    output->success = run_number != 2;
    output->direction = direction;
    (void)snprintf(output->sync_id, sizeof(output->sync_id),
        "sy_00000000-0000-4000-8000-%012d", run_number);
    (void)snprintf(output->summary, sizeof(output->summary), "%s",
        output->success ? "sync test réussie" : "sync test échouée");
    if (!output->success)
        (void)snprintf(output->error, sizeof(output->error), "%s",
            "Appareil déconnecté pendant la synchronisation.");
    CHECK(snprintf(directory, sizeof(directory), "%s/trainlog",
        sync_journal_root) > 0);
    CHECK(mkdir(directory, 0700) == 0 || errno == EEXIST);
    CHECK(snprintf(history_path, sizeof(history_path), "%s/sync_history.log",
        directory) > 0);
    history = fopen(history_path, "ab");
    CHECK(history != NULL);
    CHECK(fprintf(history, "%s\t11/09/2026 12:%02d\t%d\tb\t%s\n",
        output->sync_id, run_number, output->success ? 1 : 0,
        output->summary) > 0);
    CHECK(fclose(history) == 0);
    return output->success ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_SYSTEM_ERROR;
}

static bool test_shell_body_controller_preserves_identity_and_failed_edit(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogAppContext app;
    TrainlogBodyObservationRecord records[4];
    size_t count = 0U;
    size_t field;
    char id[TRAINLOG_ID_MAX + 1U];
    char observed_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    (void)memset(&app, 0, sizeof(app));
    app.database = database;
    app.layout.usable = true;
    app.layout.content.height = 16;
    trainlog_navigation_init(&app.navigation);
    app.navigation.current.route = TRAINLOG_ROUTE_BODY;
    body_controller_start_add(&app);
    CHECK(app.body_controller.phase == TRAINLOG_BODY_FORM);
    CHECK(app_shell_dispatch_body_controller(&app, '8'));
    CHECK(app_shell_dispatch_body_controller(&app, '0'));
    for (field = 0U; field < 14U; ++field)
        CHECK(app_shell_dispatch_body_controller(&app, TRAINLOG_KEY_ENTER));
    CHECK(app.body_controller.phase == TRAINLOG_BODY_MESSAGE);
    CHECK(trainlog_database_list_body_observations(database, records, 4U,
        &count) == TRAINLOG_STATUS_OK);
    CHECK(count == 1U && records[0].has_body_weight &&
        records[0].body_weight_kg == 80.0);
    (void)snprintf(id, sizeof(id), "%s", records[0].observation_id);
    (void)snprintf(observed_at, sizeof(observed_at), "%s", records[0].observed_at);
    (void)memset(&app.body_controller, 0, sizeof(app.body_controller));
    CHECK(body_controller_start_edit(&app, id));
    trainlog_form_init(&app.body_controller.form, "-");
    CHECK(app_shell_dispatch_body_controller(&app, TRAINLOG_KEY_ENTER));
    for (field = 1U; field < 14U; ++field)
        CHECK(app_shell_dispatch_body_controller(&app, TRAINLOG_KEY_ENTER));
    CHECK(app.body_controller.phase == TRAINLOG_BODY_FORM);
    CHECK(strstr(app.body_controller.message, "au moins une mesure") != NULL);
    CHECK(trainlog_database_get_body_observation(database, id, &records[0]) ==
        TRAINLOG_STATUS_OK);
    CHECK(records[0].has_body_weight && records[0].body_weight_kg == 80.0);
    trainlog_form_init(&app.body_controller.form, "81");
    app.body_controller.field = 0U;
    CHECK(app_shell_dispatch_body_controller(&app, TRAINLOG_KEY_ENTER));
    for (field = 1U; field < 14U; ++field)
        CHECK(app_shell_dispatch_body_controller(&app, TRAINLOG_KEY_ENTER));
    CHECK(app.body_controller.phase == TRAINLOG_BODY_MESSAGE);
    CHECK(trainlog_database_get_body_observation(database, id, &records[0]) ==
        TRAINLOG_STATUS_OK);
    CHECK(strcmp(records[0].observation_id, id) == 0);
    CHECK(strcmp(records[0].observed_at, observed_at) == 0);
    CHECK(records[0].body_weight_kg == 81.0);
    (void)memset(&app.body_controller, 0, sizeof(app.body_controller));
    app.navigation.current.route = TRAINLOG_ROUTE_BODY;
    app_shell_open_route(&app, TRAINLOG_ROUTE_BODY_METRIC);
    CHECK(trainlog_database_list_body_observations(database, records, 4U,
        &count) == TRAINLOG_STATUS_OK);
    CHECK(count == 1U && strcmp(records[0].observation_id, id) == 0 &&
        records[0].body_weight_kg == 81.0);
    trainlog_database_close(database);
    return true;
}

static bool test_shell_sync_f7_runs_bidirectional_and_refreshes_history(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogAppContext app;
    TrainlogTerminal terminal = {0};
    char temporary[] = "/tmp/trainlog-tui-sync-XXXXXX";
    const char *old_data_home = getenv("XDG_DATA_HOME");
    char *saved_data_home = old_data_home != NULL ? strdup(old_data_home) : NULL;
    size_t index;
    size_t sync_action_index = SIZE_MAX;
    size_t sync_action_count = 0U;
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(mkdtemp(temporary) != NULL);
    CHECK(setenv("XDG_DATA_HOME", temporary, 1) == 0);
    (void)snprintf(sync_journal_root, sizeof(sync_journal_root), "%s", temporary);
    (void)memset(&app, 0, sizeof(app));
    app.database = database;
    terminal.columns = 120;
    terminal.rows = 35;
    app.terminal = &terminal;
    trainlog_navigation_init(&app.navigation);
    trainlog_overlays_init(&app.overlays);
    trainlog_sync_screen_state_init(&app.sync_controller.action);
    app.sync_controller.probe = mock_sync_probe;
    app.sync_controller.run = mock_sync_run;
    sync_probe_calls = 0;
    sync_run_calls = 0;
    sync_running_app = &app;
    app_shell_open_route(&app, TRAINLOG_ROUTE_SYNC);
    CHECK(app_shell_layout(&app));
    CHECK(sync_probe_calls == 0 && sync_run_calls == 0);
    app_shell_actions(&app);
    for (index = 0U; index < app.actions.count; ++index) {
        const TrainlogAction *action = &app.actions.items[index];
        CHECK(strstr(action->label, "Android→PC") == NULL);
        CHECK(strstr(action->label, "PC→Android") == NULL);
        if (strcmp(action->identifier, "sync.now") == 0) {
            ++sync_action_count;
            sync_action_index = index;
            CHECK(strcmp(action->label,
                "s Synchroniser maintenant · PC↔Android") == 0);
        }
    }
    CHECK(sync_action_count == 1U && sync_action_index != SIZE_MAX);

    /* Actual user path: Synchronisation screen -> F7 -> registered action. */
    app_shell_dispatch(&app, TRAINLOG_KEY_F7);
    CHECK(trainlog_overlays_top(&app.overlays) != NULL);
    app.overlays.items[app.overlays.count - 1U].selected = sync_action_index;
    app_shell_dispatch(&app, TRAINLOG_KEY_ENTER);
    CHECK(app.sync_controller.action.confirming && sync_run_calls == 0);
    CHECK(app_shell_dispatch_sync(&app, TRAINLOG_KEY_ESCAPE));
    CHECK(!app.sync_controller.action.confirming && sync_run_calls == 0);

    app_shell_dispatch(&app, TRAINLOG_KEY_F7);
    app.overlays.items[app.overlays.count - 1U].selected = sync_action_index;
    app_shell_dispatch(&app, TRAINLOG_KEY_ENTER);
    CHECK(sync_run_calls == 0);
    CHECK(app_shell_dispatch_sync(&app, TRAINLOG_KEY_ENTER));
    CHECK(sync_run_calls == 1);
    CHECK(sync_last_direction == TRAINLOG_SYNC_BIDIRECTIONAL);
    CHECK(sync_probe_calls == 1);
    CHECK(!app.sync_controller.running && app.sync_controller.has_report);
    CHECK(app.sync_controller.report.success);
    CHECK(app.sync_controller.history_count == 1U);
    terminal.output_used = 0U; terminal.output[0] = '\0';
    app_shell_render_sync(&app);
    CHECK(strstr(terminal.output, "sync test réussie") != NULL);

    /* Repeating the action creates the next legitimate journal event. */
    app_shell_dispatch(&app, 's');
    CHECK(app.sync_controller.action.confirming);
    app_shell_dispatch(&app, TRAINLOG_KEY_ENTER);
    CHECK(sync_run_calls == 2);
    CHECK(sync_last_direction == TRAINLOG_SYNC_BIDIRECTIONAL);
    CHECK(!app.sync_controller.report.success);
    CHECK(app.sync_controller.history_count == 2U);
    terminal.output_used = 0U; terminal.output[0] = '\0';
    app_shell_render_sync(&app);
    CHECK(strstr(terminal.output,
        "Appareil déconnecté pendant la synchronisation.") != NULL);
    CHECK(app_shell_dispatch_sync(&app, 'r'));
    CHECK(sync_probe_calls == 3 && sync_run_calls == 2);
    if (saved_data_home != NULL) {
        CHECK(setenv("XDG_DATA_HOME", saved_data_home, 1) == 0);
        free(saved_data_home);
    } else CHECK(unsetenv("XDG_DATA_HOME") == 0);
    {
        char history_path[PATH_MAX + 1U];
        char directory[PATH_MAX + 1U];
        CHECK(snprintf(history_path, sizeof(history_path),
            "%s/trainlog/sync_history.log", temporary) > 0);
        CHECK(snprintf(directory, sizeof(directory), "%s/trainlog",
            temporary) > 0);
        CHECK(unlink(history_path) == 0);
        CHECK(rmdir(directory) == 0);
        CHECK(rmdir(temporary) == 0);
    }
    sync_running_app = NULL;
    trainlog_database_close(database);
    return true;
}

static bool test_shell_sync_history_neutralizes_legacy_directions(void)
{
    TrainlogAppContext app;
    TrainlogTerminal terminal = {0};
    TrainlogSurface surface = {0};
    char temporary[] = "/tmp/trainlog-tui-sync-legacy-XXXXXX";
    char directory[PATH_MAX + 1U];
    char history_path[PATH_MAX + 1U];
    char runs_directory[PATH_MAX + 1U];
    char detail_path[PATH_MAX + 1U];
    const char *old_data_home = getenv("XDG_DATA_HOME");
    char *saved_data_home = old_data_home != NULL ? strdup(old_data_home) : NULL;
    FILE *history;
    CHECK(mkdtemp(temporary) != NULL);
    CHECK(snprintf(directory, sizeof(directory), "%s/trainlog", temporary) > 0);
    CHECK(mkdir(directory, 0700) == 0);
    CHECK(snprintf(history_path, sizeof(history_path), "%s/sync_history.log",
        directory) > 0);
    CHECK(snprintf(runs_directory, sizeof(runs_directory), "%s/sync_runs",
        directory) > 0);
    CHECK(mkdir(runs_directory, 0700) == 0);
    history = fopen(history_path, "wb");
    CHECK(history != NULL);
    CHECK(fprintf(history,
        "sy_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa\t09/09/2026 10:11\t1\ta\tAndroid→PC import ancien\n"
        "sy_pppppppp-pppp-4ppp-8ppp-pppppppppppp\t10/09/2026 11:12\t0\tp\tPC→Android publication ancienne\n") > 0);
    CHECK(fclose(history) == 0);
    CHECK(snprintf(detail_path, sizeof(detail_path), "%s/%s.txt", runs_directory,
        "sy_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa") > 0);
    history = fopen(detail_path, "wb");
    CHECK(history != NULL);
    CHECK(fputs("Direction: Android→PC\nAndroid→PC import details\n", history) >= 0);
    CHECK(fclose(history) == 0);
    CHECK(snprintf(detail_path, sizeof(detail_path), "%s/%s.txt", runs_directory,
        "sy_pppppppp-pppp-4ppp-8ppp-pppppppppppp") > 0);
    history = fopen(detail_path, "wb");
    CHECK(history != NULL);
    CHECK(fputs("Direction: PC→Android\nPC→Android publication details\n", history) >= 0);
    CHECK(fclose(history) == 0);
    CHECK(setenv("XDG_DATA_HOME", temporary, 1) == 0);

    (void)memset(&app, 0, sizeof(app));
    terminal.columns = 120;
    terminal.rows = 35;
    surface.terminal = &terminal;
    app.terminal = &terminal;
    app.content = &surface;
    app.layout.content.width = 120;
    app.layout.content.height = 30;
    trainlog_navigation_init(&app.navigation);
    app.navigation.current.route = TRAINLOG_ROUTE_SYNC;
    trainlog_sync_screen_state_init(&app.sync_controller.action);
    sync_history_load(app.sync_controller.history, SYNC_HISTORY_CAPACITY,
        &app.sync_controller.history_count);
    CHECK(app.sync_controller.history_count == 2U);
    app_shell_render_sync(&app);
    CHECK(strstr(terminal.output, "09/09/2026 10:11") != NULL);
    CHECK(strstr(terminal.output, "10/09/2026 11:12") != NULL);
    CHECK(strstr(terminal.output, "entrée historique") != NULL);
    CHECK(strstr(terminal.output, "PC↔Android") != NULL);
    CHECK(strstr(terminal.output, "Android→PC") == NULL);
    CHECK(strstr(terminal.output, "PC→Android") == NULL);

    /* Actual keyboard path: select and open each retained legacy artifact. */
    for (app.sync_controller.selected = 0U;
         app.sync_controller.selected < app.sync_controller.history_count;
         ++app.sync_controller.selected) {
        terminal.output_used = 0U;
        terminal.output[0] = '\0';
        CHECK(app_shell_dispatch_sync(&app, TRAINLOG_KEY_ENTER));
        CHECK(app.sync_controller.showing_detail);
        app_shell_render_sync(&app);
        CHECK(strstr(terminal.output,
            "Synchronisation historique PC↔Android") != NULL);
        CHECK(strstr(terminal.output, "Horodatage") != NULL);
        CHECK(strstr(terminal.output, "État") != NULL);
        CHECK(strstr(terminal.output, "Android→PC") == NULL);
        CHECK(strstr(terminal.output, "PC→Android") == NULL);
        app.sync_controller.showing_detail = false;
    }

    if (saved_data_home != NULL) {
        CHECK(setenv("XDG_DATA_HOME", saved_data_home, 1) == 0);
        free(saved_data_home);
    } else CHECK(unsetenv("XDG_DATA_HOME") == 0);
    CHECK(unlink(history_path) == 0);
    CHECK(snprintf(detail_path, sizeof(detail_path), "%s/%s.txt", runs_directory,
        "sy_aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa") > 0);
    CHECK(unlink(detail_path) == 0);
    CHECK(snprintf(detail_path, sizeof(detail_path), "%s/%s.txt", runs_directory,
        "sy_pppppppp-pppp-4ppp-8ppp-pppppppppppp") > 0);
    CHECK(unlink(detail_path) == 0);
    CHECK(rmdir(runs_directory) == 0);
    CHECK(rmdir(directory) == 0);
    CHECK(rmdir(temporary) == 0);
    return true;
}

static bool test_equipment_collector_finds_match_after_first_pages(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogCustomEquipment equipment;
    TrainlogResolvedEquipment result[2];
    bool truncated = false;
    bool failed = false;
    size_t index;
    size_t count;
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    for (index = 0U; index < 140U; ++index) {
        (void)memset(&equipment, 0, sizeof(equipment));
        generate_custom_equipment_uuid(equipment.equipment_id);
        (void)snprintf(equipment.display_name, sizeof(equipment.display_name),
            index == 139U ? "zz needle-late" : "custom %03zu", index);
        (void)snprintf(equipment.equipment_type,
            sizeof(equipment.equipment_type), "machine");
        (void)snprintf(equipment.load_semantics,
            sizeof(equipment.load_semantics), "external");
        CHECK(trainlog_database_create_custom_equipment(database, &equipment) ==
            TRAINLOG_STATUS_OK);
    }
    count = equipment_collect(database, "needle-late", result, 2U,
        &truncated, &failed);
    CHECK(!failed && !truncated && count == 1U);
    CHECK(strcmp(result[0].display_name, "zz needle-late") == 0);
    trainlog_database_close(database);
    return true;
}

static bool test_body_graphs_draw_inside_minimum_content_surface(void)
{
    TrainlogAppContext app;
    TrainlogTerminal terminal;
    TrainlogSurface surface;
    TrainlogDashboardMonth previous;
    TrainlogDashboardMonth current;
    long current_key;
    (void)memset(&app, 0, sizeof(app));
    (void)memset(&terminal, 0, sizeof(terminal));
    terminal.rows = 20;
    terminal.columns = 72;
    surface.terminal = &terminal;
    app.terminal = &terminal;
    app.content = &surface;
    app.layout.content.height = 16;
    app.layout.content.width = 72;
    app.body_global_initialized = true;
    app.body_global_enabled[0] = true;
    app.body_global_series[0].symbol = GLOBAL_BODY_SYMBOLS[0];
    app.body_global_series[0].role = GLOBAL_BODY_ROLES[0];
    app.body_global_series[0].baseline = 80.0;
    app.body_global_series[0].count = 2U;
    CHECK(dashboard_current_month_key(&current_key));
    dashboard_month_from_key(current_key - 1L, &previous);
    dashboard_month_from_key(current_key, &current);
    (void)snprintf(app.body_global_series[0].points[0].observed_at,
        sizeof(app.body_global_series[0].points[0].observed_at),
        "%04d-%02d-01T08:00:00Z", previous.year, previous.month);
    app.body_global_series[0].points[0].value = 80.0;
    (void)snprintf(app.body_global_series[0].points[1].observed_at,
        sizeof(app.body_global_series[0].points[1].observed_at),
        "%04d-%02d-01T08:00:00Z", current.year, current.month);
    app.body_global_series[0].points[1].value = 81.0;
    (void)snprintf(app.body_global_dates[0], sizeof(app.body_global_dates[0]),
        "%s", app.body_global_series[0].points[0].observed_at);
    (void)snprintf(app.body_global_dates[1], sizeof(app.body_global_dates[1]),
        "%s", app.body_global_series[0].points[1].observed_at);
    app.body_global_date_count = 2U;
    app_shell_render_body_global(&app);
    CHECK(terminal.surface_draw_count > 0U);
    terminal.surface_draw_count = 0U;
    app_shell_render_body_trends(&app);
    CHECK(terminal.surface_draw_count > 0U);
    return true;
}

static bool test_exercise_merge_action_search_confirm_selects_target(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogAppContext app;
    char resolved[TRAINLOG_ID_MAX + 1U];
    size_t action_index;
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise(database,
        "ex_11111111-1111-4111-8111-111111111111", "Source curl",
        "source curl", TRAINLOG_TRACKING_REPS) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise(database,
        "ex_22222222-2222-4222-8222-222222222222", "Target curl",
        "target curl", TRAINLOG_TRACKING_REPS) == TRAINLOG_STATUS_OK);
    (void)memset(&app, 0, sizeof(app));
    app.database = database;
    trainlog_navigation_init(&app.navigation);
    trainlog_overlays_init(&app.overlays);
    app.navigation.current.route = TRAINLOG_ROUTE_EXERCISE_DETAIL;
    (void)snprintf(app.exercise_detail.exercise_id,
        sizeof(app.exercise_detail.exercise_id), "%s",
        "ex_11111111-1111-4111-8111-111111111111");
    (void)snprintf(app.exercise_detail.name, sizeof(app.exercise_detail.name),
        "%s", "Source curl");
    app.exercise_detail.tracking_mode = TRAINLOG_TRACKING_REPS;
    app.exercise_detail.recording_mode = TRAINLOG_RECORDING_SETS;
    app_shell_actions(&app);
    for (action_index = 0U; action_index < app.actions.count; ++action_index)
        if (strcmp(app.actions.items[action_index].identifier,
                "exercise.merge") == 0) break;
    CHECK(action_index < app.actions.count);
    (void)trainlog_overlays_push(&app.overlays, TRAINLOG_OVERLAY_ACTIONS,
        TRAINLOG_FOCUS_CONTENT, app.exercise_detail.exercise_id);
    app.overlays.items[0].selected = action_index;
    app_shell_dispatch_overlay(&app, TRAINLOG_KEY_ENTER);
    CHECK(trainlog_overlays_top(&app.overlays)->type ==
        TRAINLOG_OVERLAY_EXERCISE_MERGE);
    CHECK(app.exercise_controller.phase == TRAINLOG_EXERCISE_MERGE_PICKER);
    app_shell_dispatch_overlay(&app, 't');
    app_shell_dispatch_overlay(&app, 'a');
    app_shell_dispatch_overlay(&app, 'r');
    CHECK(exercise_merge_match_count(&app.exercise_controller) == 1U);
    app_shell_dispatch_overlay(&app, TRAINLOG_KEY_ENTER);
    CHECK(app.exercise_controller.phase == TRAINLOG_EXERCISE_MERGE_CONFIRM);
    app_shell_dispatch_overlay(&app, TRAINLOG_KEY_DOWN);
    app_shell_dispatch_overlay(&app, TRAINLOG_KEY_ENTER);
    CHECK(app.exercise_controller.phase == TRAINLOG_EXERCISE_MERGE_RESULT);
    CHECK(strcmp(app.exercise_detail.exercise_id,
        "ex_22222222-2222-4222-8222-222222222222") == 0);
    CHECK(strcmp(app.navigation.current.stable_id,
        "ex_22222222-2222-4222-8222-222222222222") == 0);
    CHECK(trainlog_database_resolve_exercise_id(database,
        "ex_11111111-1111-4111-8111-111111111111", resolved,
        sizeof(resolved)) == TRAINLOG_STATUS_OK);
    CHECK(strcmp(resolved, "ex_22222222-2222-4222-8222-222222222222") == 0);
    app_shell_dispatch_overlay(&app, TRAINLOG_KEY_ENTER);
    CHECK(app.overlays.count == 0U);
    CHECK(app.exercise_controller.phase == TRAINLOG_EXERCISE_IDLE);
    trainlog_database_close(database);
    return true;
}

int main(void)
{
    if (!test_knowledge_scrolls_long_lists_at_minimum_terminal() ||
        !test_shell_session_navigation_preserves_run_draft() ||
        !test_shell_generator_keep_and_accept_has_no_actuals() ||
        !test_shell_session_forms_keep_plan_and_actuals_separate() ||
        !test_shell_continuous_supplemental_fields_have_no_fake_set() ||
        !test_shell_generator_dose_edit_drops_stale_load_provenance() ||
        !test_shell_inline_equipment_creation_resumes_occurrence() ||
        !test_shell_equipment_catalog_create_preserves_invalid_form() ||
        !test_shell_inline_exercise_creation_and_stable_edit() ||
        !test_shell_navigation_focus_consumes_input() ||
        !test_shell_actions_overlay_executes_registered_search_action() ||
        !test_shell_compact_navigation_focus_contract() ||
        !test_shell_detail_back_restores_stable_list_selection() ||
        !test_shell_body_controller_preserves_identity_and_failed_edit() ||
        !test_shell_sync_f7_runs_bidirectional_and_refreshes_history() ||
        !test_shell_sync_history_neutralizes_legacy_directions() ||
        !test_equipment_collector_finds_match_after_first_pages() ||
        !test_exercise_merge_action_search_confirm_selects_target() ||
        !test_body_graphs_draw_inside_minimum_content_surface()) return 1;
    (void)printf("PASS tui_workflows\n");
    return 0;
}
