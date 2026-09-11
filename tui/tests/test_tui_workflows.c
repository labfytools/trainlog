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
    size_t dashboard_dot_count;
    size_t dashboard_block_count;
    int maximum_draw_row;
    int maximum_draw_column;
    int stats_progression_row;
    int stats_progression_column;
    int stats_measurements_row;
    int stats_measurements_column;
    int stats_frequency_row;
    int stats_frequency_column;
    int stats_frequency_bottom_row;
    int stats_distribution_row;
    struct {
        int row;
        int column;
        char text[32];
    } printed[256];
    size_t printed_count;
    struct {
        int row;
        int column;
        uint32_t codepoint;
    } drawn[512];
    size_t drawn_count;
};

struct TrainlogPanel { int unused; };
struct TrainlogSurface { TrainlogTerminal *terminal; };

#define CHECK(condition) do { if (!(condition)) {                           \
    (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n",                  \
        __FILE__, __LINE__, #condition); return false; } } while (0)

static size_t text_occurrences(const char *text, const char *needle)
{
    size_t count = 0U;
    size_t length = strlen(needle);
    const char *cursor = text;
    if (length == 0U) return 0U;
    while ((cursor = strstr(cursor, needle)) != NULL) {
        ++count;
        cursor += length;
    }
    return count;
}

static int rendered_text_column(const TrainlogTerminal *terminal, const char *text)
{
    size_t index;
    for (index = 0U; index < terminal->printed_count; ++index)
        if (strcmp(terminal->printed[index].text, text) == 0)
            return terminal->printed[index].column;
    return -1;
}

static int rendered_draw_column(const TrainlogTerminal *terminal, uint32_t codepoint,
                                size_t occurrence)
{
    size_t index;
    for (index = 0U; index < terminal->drawn_count; ++index)
        if (terminal->drawn[index].codepoint == codepoint && occurrence-- == 0U)
            return terminal->drawn[index].column;
    return -1;
}

static bool rendered_has_draw_column(const TrainlogTerminal *terminal,
                                     uint32_t codepoint, int column)
{
    size_t index;
    for (index = 0U; index < terminal->drawn_count; ++index)
        if (terminal->drawn[index].codepoint == codepoint &&
            terminal->drawn[index].column == column) return true;
    return false;
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
        char *start = surface->terminal->output + surface->terminal->output_used;
        int written = vsnprintf(surface->terminal->output + surface->terminal->output_used,
            sizeof(surface->terminal->output) - surface->terminal->output_used,
            format, arguments);
        if (strstr(start, "Progression globale") != NULL) {
            surface->terminal->stats_progression_row = row;
            surface->terminal->stats_progression_column = column;
        }
        if (strstr(start, "Mensurations") != NULL) {
            surface->terminal->stats_measurements_row = row;
            surface->terminal->stats_measurements_column = column;
        }
        if (strstr(start, "Fréquence") != NULL) {
            surface->terminal->stats_frequency_row = row;
            surface->terminal->stats_frequency_column = column;
        }
        if (strstr(start, "Répartition du catalogue") != NULL)
            surface->terminal->stats_distribution_row = row;
        if ((strncmp(start, "J−", strlen("J−")) == 0 ||
             strncmp(start, "P−", strlen("P−")) == 0 ||
             strncmp(start, "M−", strlen("M−")) == 0 ||
             strcmp(start, "actuel") == 0) &&
            row > surface->terminal->stats_frequency_bottom_row)
            surface->terminal->stats_frequency_bottom_row = row;
        if (written > 0 && (size_t)written < sizeof(surface->terminal->output) -
            surface->terminal->output_used) {
            size_t copy = (size_t)written < sizeof(surface->terminal->printed[0].text) - 1U
                ? (size_t)written : sizeof(surface->terminal->printed[0].text) - 1U;
            if (surface->terminal->printed_count <
                sizeof(surface->terminal->printed) / sizeof(surface->terminal->printed[0])) {
                size_t slot = surface->terminal->printed_count++;
                surface->terminal->printed[slot].row = row;
                surface->terminal->printed[slot].column = column;
                (void)memcpy(surface->terminal->printed[slot].text, start, copy);
                surface->terminal->printed[slot].text[copy] = '\0';
            }
            surface->terminal->output_used += (size_t)written;
        }
    }
    va_end(arguments); (void)row; (void)column;
}
void trainlog_surface_draw(TrainlogSurface *surface, int row, int column,
    uint32_t codepoint)
{
    if (surface != NULL && surface->terminal != NULL) {
        if (surface->terminal->drawn_count <
            sizeof(surface->terminal->drawn) / sizeof(surface->terminal->drawn[0])) {
            size_t slot = surface->terminal->drawn_count++;
            surface->terminal->drawn[slot].row = row;
            surface->terminal->drawn[slot].column = column;
            surface->terminal->drawn[slot].codepoint = codepoint;
        }
        ++surface->terminal->surface_draw_count;
        if (codepoint == 0x00b7U) ++surface->terminal->dashboard_dot_count;
        if (codepoint == 0x2588U) ++surface->terminal->dashboard_block_count;
        if (row > surface->terminal->maximum_draw_row)
            surface->terminal->maximum_draw_row = row;
        if (column > surface->terminal->maximum_draw_column)
            surface->terminal->maximum_draw_column = column;
        if (row < 0 || row >= surface->terminal->rows || column < 0 ||
            column >= surface->terminal->columns)
            surface->terminal->coordinate_overflow = true;
    }
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

static bool dashboard_insert_completed_set_dose(TrainlogDatabase *database,
    const char *session_id, const char *started_at, const char *ended_at,
    TrainlogLoadMode load_mode, const char *equipment_id, double weight, int dose)
{
    TrainlogSetInput set = {dose, 0, true, weight};
    TrainlogSessionExerciseInput occurrence;
    TrainlogSessionInput session;
    (void)memset(&occurrence, 0, sizeof(occurrence));
    (void)memset(&session, 0, sizeof(session));
    (void)snprintf(occurrence.exercise_id, sizeof(occurrence.exercise_id), "%s",
        "ex_33333333-3333-4333-8333-333333333333");
    (void)snprintf(occurrence.equipment_id, sizeof(occurrence.equipment_id), "%s",
        equipment_id);
    occurrence.recording_mode = TRAINLOG_RECORDING_SETS;
    occurrence.load_mode = load_mode;
    occurrence.rest_seconds = 60;
    occurrence.target_sets = 2;
    occurrence.target_reps = 10;
    occurrence.target_has_weight = true;
    occurrence.target_weight_kg = 999.0;
    occurrence.sets = &set;
    occurrence.set_count = 1U;
    (void)snprintf(session.session_id, sizeof(session.session_id), "%s", session_id);
    (void)snprintf(session.started_at, sizeof(session.started_at), "%s", started_at);
    (void)snprintf(session.ended_at, sizeof(session.ended_at), "%s", ended_at);
    session.session_type = TRAINLOG_SESSION_TRAINING;
    session.exercises = &occurrence;
    session.exercise_count = 1U;
    return trainlog_database_insert_session(database, &session) == TRAINLOG_STATUS_OK;
}

static bool dashboard_insert_completed_set(TrainlogDatabase *database,
    const char *session_id, const char *started_at, const char *ended_at,
    TrainlogLoadMode load_mode, const char *equipment_id, double weight)
{
    return dashboard_insert_completed_set_dose(database, session_id, started_at,
        ended_at, load_mode, equipment_id, weight, 8);
}

static bool dashboard_insert_explicit_max_at(TrainlogDatabase *database,
    const char *session_id, const char *started_at, const char *entry_id,
    double weight)
{
    TrainlogSessionExerciseInput occurrence;
    TrainlogSessionInput session;
    (void)memset(&occurrence, 0, sizeof(occurrence));
    (void)memset(&session, 0, sizeof(session));
    (void)snprintf(occurrence.exercise_id, sizeof(occurrence.exercise_id), "%s",
        "ex_33333333-3333-4333-8333-333333333333");
    (void)snprintf(occurrence.equipment_id, sizeof(occurrence.equipment_id), "%s",
        "leg_press");
    (void)snprintf(occurrence.entry_id, sizeof(occurrence.entry_id), "%s", entry_id);
    occurrence.recording_mode = TRAINLOG_RECORDING_SETS;
    occurrence.load_mode = TRAINLOG_LOAD_NONE;
    occurrence.has_max_weight = true;
    occurrence.max_weight_kg = weight;
    (void)snprintf(session.session_id, sizeof(session.session_id), "%s", session_id);
    (void)snprintf(session.started_at, sizeof(session.started_at), "%s", started_at);
    session.session_type = TRAINLOG_SESSION_MAX_TEST;
    session.exercises = &occurrence;
    session.exercise_count = 1U;
    return trainlog_database_insert_session(database, &session) == TRAINLOG_STATUS_OK;
}

static bool dashboard_insert_explicit_max(TrainlogDatabase *database)
{
    return dashboard_insert_explicit_max_at(database,
        "se_55555555-5555-4555-8555-555555555555", "2026-09-11T11:00:00Z",
        "sxe_55555555-5555-4555-8555-555555555555", 120.0);
}

static bool dashboard_insert_continuous_actual(TrainlogDatabase *database)
{
    TrainlogSessionExerciseInput occurrence;
    TrainlogSessionInput session;
    (void)memset(&occurrence, 0, sizeof(occurrence));
    (void)memset(&session, 0, sizeof(session));
    (void)snprintf(occurrence.exercise_id, sizeof(occurrence.exercise_id), "%s",
        "ex_77777777-7777-4777-8777-777777777777");
    occurrence.recording_mode = TRAINLOG_RECORDING_CONTINUOUS;
    occurrence.continuous_duration_seconds = 1800;
    (void)snprintf(session.session_id, sizeof(session.session_id), "%s",
        "se_77777777-7777-4777-8777-777777777777");
    (void)snprintf(session.started_at, sizeof(session.started_at), "%s",
        "2026-09-08T08:00:00Z");
    session.session_type = TRAINLOG_SESSION_TRAINING;
    session.exercises = &occurrence;
    session.exercise_count = 1U;
    return trainlog_database_insert_session(database, &session) == TRAINLOG_STATUS_OK;
}

static bool dashboard_insert_plan_only(TrainlogDatabase *database)
{
    TrainlogSessionExerciseInput occurrence;
    TrainlogSessionInput session;
    (void)memset(&occurrence, 0, sizeof(occurrence));
    (void)memset(&session, 0, sizeof(session));
    (void)snprintf(occurrence.exercise_id, sizeof(occurrence.exercise_id), "%s",
        "ex_33333333-3333-4333-8333-333333333333");
    (void)snprintf(occurrence.equipment_id, sizeof(occurrence.equipment_id), "%s",
        "leg_press");
    occurrence.recording_mode = TRAINLOG_RECORDING_SETS;
    occurrence.load_mode = TRAINLOG_LOAD_EXTERNAL;
    occurrence.target_sets = 3;
    occurrence.target_reps = 10;
    occurrence.target_has_weight = true;
    occurrence.target_weight_kg = 999.0;
    (void)snprintf(session.session_id, sizeof(session.session_id), "%s",
        "se_88888888-8888-4888-8888-888888888888");
    (void)snprintf(session.started_at, sizeof(session.started_at), "%s",
        "2026-09-07T08:00:00Z");
    session.session_type = TRAINLOG_SESSION_TRAINING;
    session.exercises = &occurrence;
    session.exercise_count = 1U;
    return trainlog_database_insert_session(database, &session) == TRAINLOG_STATUS_OK;
}

static void dashboard_prepare_render(TrainlogAppContext *app,
    TrainlogTerminal *terminal, TrainlogSurface *surface, int columns, int rows,
    int content_width, int content_height)
{
    (void)memset(terminal, 0, sizeof(*terminal));
    surface->terminal = terminal;
    app->terminal = terminal;
    app->content = surface;
    terminal->columns = columns;
    terminal->rows = rows;
    app->layout.columns = columns;
    app->layout.rows = rows;
    app->layout.content.width = content_width;
    app->layout.content.height = content_height;
}

static bool dashboard_utc_timestamp_at(time_t value, char output[TRAINLOG_TIMESTAMP_MAX + 1U])
{
    struct tm utc;
    return gmtime_r(&value, &utc) != NULL && strftime(output,
        TRAINLOG_TIMESTAMP_MAX + 1U, "%Y-%m-%dT%H:%M:%SZ", &utc) > 0U;
}

static bool test_stats_dashboard_sparse_series_names_frequency_and_footer(void)
{
    TrainlogAppContext app;
    TrainlogTerminal terminal;
    TrainlogSurface surface;
    size_t index;
    (void)memset(&app, 0, sizeof(app));
    app.dashboard.has_body = true;
    app.dashboard.body_metric = 0U;
    app.dashboard.body_count = 1U;
    app.dashboard.body[0].value = 83.70;
    app.dashboard.completed_session_count = 4U;
    app.dashboard.performed_set_count = 9U;
    app.dashboard.distinct_exercise_count = 3U;
    app.dashboard.explicit_max_count = 1U;
    dashboard_prepare_period_buckets(&app.dashboard, 1000, 971);
    app.dashboard.weeks[5].sessions = 4U;
    app.dashboard.weeks[5].maxima = 1U;
    app.dashboard.weeks[5].working_improvements = 2U;
    app.dashboard.weeks[5].max_improvements = 1U;

    /* CONTRACT: the selector changes the shared projection used by progress
     * and frequency; it is not a label over an invariant six-week chart. */
    {
        static const TrainlogStatisticsPeriod periods[] = {
            TRAINLOG_STATS_7_DAYS, TRAINLOG_STATS_30_DAYS,
            TRAINLOG_STATS_90_DAYS, TRAINLOG_STATS_YEAR, TRAINLOG_STATS_ALL
        };
        static const size_t counts[] = {7U, 6U, 6U, 12U, 12U};
        static const int64_t spans[] = {1, 5, 15, 31, 84};
        size_t period_index;
        for (period_index = 0U; period_index < 5U; ++period_index) {
            app.dashboard.period = periods[period_index];
            dashboard_prepare_period_buckets(&app.dashboard, 1000, 0);
            CHECK(app.dashboard.week_count == counts[period_index]);
            CHECK(app.dashboard.weeks[0].span_days == spans[period_index]);
            CHECK(strcmp(app.dashboard.weeks[
                app.dashboard.week_count - 1U].label, "actuel") == 0);
        }
        app.dashboard.period = TRAINLOG_STATS_30_DAYS;
        dashboard_prepare_period_buckets(&app.dashboard, 1000, 971);
        app.dashboard.weeks[5].sessions = 4U;
        app.dashboard.weeks[5].maxima = 1U;
        app.dashboard.weeks[5].working_improvements = 2U;
        app.dashboard.weeks[5].max_improvements = 1U;
    }

    /* A lone measurement is text-only; event blocks are factual period-bucket
     * counts and the accessible totals expose their meaning. */
    dashboard_prepare_render(&app, &terminal, &surface, 120, 35, 96, 29);
    app_shell_render_dashboard(&app);
    CHECK(strstr(terminal.output, "Période  7j  [30j]  90j  1an  Tout") != NULL);
    CHECK(strstr(terminal.output, "1:7j") == NULL);
    CHECK(strstr(terminal.output,
        "Séances 4   Séries 9   Exercices pratiqués 3   MAX 1") != NULL);
    CHECK(terminal.stats_distribution_row > terminal.stats_frequency_bottom_row);
    CHECK(strstr(terminal.output, "travail 2 · MAX 1") != NULL);
    CHECK(strstr(terminal.output, "83,70 kg · 1 relevé") != NULL);
    CHECK(terminal.dashboard_block_count > 0U);

    /* Frequency geometry has one shared x coordinate per period bucket.
     * The end buckets also prove a wide chart uses its useful span. */
    (void)memset(&app.dashboard.weeks, 0, sizeof(app.dashboard.weeks));
    dashboard_prepare_period_buckets(&app.dashboard, 1000, 971);
    for (index = 0U; index < 6U; ++index) {
        app.dashboard.weeks[index].sessions = 11U + index;
        app.dashboard.weeks[index].maxima = 1U;
    }
    dashboard_prepare_render(&app, &terminal, &surface, 120, 35, 96, 29);
    app_shell_dashboard_frequency_chart(&app, &app.dashboard, 11, 4, 88, 7);
    for (index = 0U; index < 6U; ++index) {
        char count[8];
        const char *label = app.dashboard.weeks[index].label;
        int label_column;
        int x;
        (void)snprintf(count, sizeof(count), "%zu", 11U + index);
        label_column = rendered_text_column(&terminal, label);
        x = label_column + (index + 1U == 6U ? 3 : 1);
        CHECK(rendered_text_column(&terminal, count) == x);
        CHECK(rendered_draw_column(&terminal, 0x25c6U, index) == x);
        CHECK(rendered_has_draw_column(&terminal, 0x2588U, x));
    }
    CHECK(rendered_text_column(&terminal, "actuel") -
        rendered_text_column(&terminal, "P−5") >= 75);

    /* One actual measurement is a value, not a fabricated line. */
    (void)memset(&terminal, 0, sizeof(terminal));
    surface.terminal = &terminal;
    app_shell_dashboard_sparkline(&app, &app.dashboard.body[0].value, 1U,
        4, 4, 24, 2);
    CHECK(terminal.surface_draw_count == 0U);

    app.dashboard.body[1] = app.dashboard.body[0];
    app.dashboard.body[1].value = 82.90;
    app.dashboard.body_count = 2U;
    {
        static const int columns[] = {100, 80, 72};
        static const int rows[] = {30, 24, 20};
        static const int widths[] = {76, 80, 72};
        static const int heights[] = {24, 20, 16};
        size_t geometry;
        for (geometry = 0U; geometry < 3U; ++geometry) {
            dashboard_prepare_render(&app, &terminal, &surface,
                columns[geometry], rows[geometry], widths[geometry],
                heights[geometry]);
            app_shell_render_dashboard(&app);
            CHECK(strstr(terminal.output, "Progression globale") != NULL);
            CHECK(strstr(terminal.output, "Mensurations") != NULL);
            CHECK(strstr(terminal.output, "Fréquence") != NULL);
            CHECK(strchr(terminal.output, '#') == NULL);
            CHECK(strchr(terminal.output, '*') == NULL);
            CHECK(terminal.dashboard_block_count > 0U);
            CHECK(!terminal.coordinate_overflow && !terminal.text_overflow);
            CHECK(terminal.maximum_draw_row < heights[geometry]);
            CHECK(terminal.stats_distribution_row >
                terminal.stats_frequency_bottom_row);
            if (columns[geometry] >= 100) {
                CHECK(terminal.stats_progression_row ==
                    terminal.stats_measurements_row);
                CHECK(terminal.stats_measurements_column >
                    terminal.stats_progression_column);
                CHECK(terminal.stats_frequency_row >
                    terminal.stats_progression_row);
            } else {
                CHECK(terminal.stats_progression_column ==
                    terminal.stats_measurements_column);
                CHECK(terminal.stats_measurements_row >
                    terminal.stats_progression_row);
            }
        }
    }

    /* No classified performance events still occupies the selected-period
     * timeline, with dots and an explicit explanation. */
    (void)memset(&app.dashboard.weeks, 0, sizeof(app.dashboard.weeks));
    dashboard_prepare_period_buckets(&app.dashboard, 1000, 971);
    app.dashboard.body_count = 1U;
    dashboard_prepare_render(&app, &terminal, &surface, 72, 20, 72, 16);
    app_shell_render_dashboard(&app);
    CHECK(strstr(terminal.output, "données insuffisantes") != NULL);
    CHECK(terminal.dashboard_dot_count >= 6U);
    CHECK(terminal.dashboard_block_count == 0U);
    CHECK(!terminal.coordinate_overflow && terminal.maximum_draw_row < 16);

    /* A non-zero frequency bucket alone must produce a bar, independently of
     * performance and measurement marks. */
    app.dashboard.weeks[5].sessions = 3U;
    app.dashboard.weeks[5].maxima = 1U;
    dashboard_prepare_render(&app, &terminal, &surface, 72, 20, 72, 16);
    app_shell_dashboard_frequency_chart(&app, &app.dashboard, 3, 4, 64, 3);
    CHECK(terminal.dashboard_block_count > 0U);
    CHECK(terminal.maximum_draw_row < 16);

    CHECK(strcmp(trainlog_exercise_name_catalog_lookup(
        "ex_01ff06dd-ad00-46ee-9b46-ed32cabedbef"),
        "Adduction de hanche assise") == 0);

    (void)memset(&app, 0, sizeof(app));
    trainlog_navigation_init(&app.navigation);
    app.navigation.current.route = TRAINLOG_ROUTE_STATS;
    app_shell_actions(&app);
    CHECK(trainlog_actions_find_key(&app.actions, TRAINLOG_KEY_F7) != NULL);
    dashboard_prepare_render(&app, &terminal, &surface, 80, 24, 80, 20);
    app.footer = &surface;
    app_shell_render_footer(&app);
    CHECK(text_occurrences(terminal.output, "F6 Navigation") == 1U);
    CHECK(text_occurrences(terminal.output, "F7 Actions") == 1U);
    return true;
}

static bool test_stats_dashboard_uses_real_exact_time_snapshots_responsively(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogAppContext app;
    TrainlogTerminal terminal;
    TrainlogSurface surface;
    TrainlogBodyObservationInput body;
    size_t sessions = 0U;
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(database,
        "ex_33333333-3333-4333-8333-333333333333", "Presse test",
        "presse test", TRAINLOG_TRACKING_REPS, TRAINLOG_RECORDING_SETS,
        (TrainlogExerciseDataFields)0) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(database,
        "ex_77777777-7777-4777-8777-777777777777", "Marche test",
        "marche test", TRAINLOG_TRACKING_DURATION, TRAINLOG_RECORDING_CONTINUOUS,
        (TrainlogExerciseDataFields)0) == TRAINLOG_STATUS_OK);
    /* Raw text says 23:00 is later, but +15:00 makes it the older instant. */
    CHECK(dashboard_insert_completed_set(database,
        "se_11111111-1111-4111-8111-111111111111",
        "2026-09-10T23:00:00+15:00", "2026-09-10T23:30:00+15:00",
        TRAINLOG_LOAD_EXTERNAL, "leg_press", 10.0));
    CHECK(dashboard_insert_completed_set(database,
        "se_22222222-2222-4222-8222-222222222222",
        "2026-09-10t09:00:00z", "2026-09-10t09:30:00z",
        TRAINLOG_LOAD_EXTERNAL, "leg_press", 20.0));
    CHECK(dashboard_insert_completed_set(database,
        "se_44444444-4444-4444-8444-444444444444",
        "2026-09-11T10:00:00+00:00", "2026-09-11T10:30:00+00:00",
        TRAINLOG_LOAD_ASSISTANCE, "assisted_dip_chin_machine", 5.0));
    /* A: ended_at is lifecycle metadata, not the gate for actual history. */
    CHECK(dashboard_insert_completed_set(database,
        "se_66666666-6666-4666-8666-666666666666",
        "2026-09-09T10:00:00Z", "", TRAINLOG_LOAD_EXTERNAL,
        "leg_press", 30.0));
    /* B: the explicit MAX helper also persists ended_at NULL. */
    CHECK(dashboard_insert_explicit_max(database));
    /* C is actual continuous history; D has only targets and must disappear. */
    CHECK(dashboard_insert_continuous_actual(database));
    CHECK(dashboard_insert_plan_only(database));
    (void)memset(&body, 0, sizeof(body));
    (void)snprintf(body.observation_id, sizeof(body.observation_id), "%s",
        "bo_11111111-1111-4111-8111-111111111111");
    (void)snprintf(body.observed_at, sizeof(body.observed_at), "%s",
        "2026-09-10T23:00:00+15:00");
    body.has_body_weight = true;
    body.body_weight_kg = 80.0;
    CHECK(trainlog_database_insert_body_observation(database, &body) ==
        TRAINLOG_STATUS_OK);
    (void)memset(&body, 0, sizeof(body));
    (void)snprintf(body.observation_id, sizeof(body.observation_id), "%s",
        "bo_33333333-3333-4333-8333-333333333333");
    (void)snprintf(body.observed_at, sizeof(body.observed_at), "%s",
        "2026-09-11T12:00:00Z");
    body.has_waist = true;
    body.waist_cm = 85.0;
    CHECK(trainlog_database_insert_body_observation(database, &body) ==
        TRAINLOG_STATUS_OK);
    (void)memset(&body, 0, sizeof(body));
    (void)snprintf(body.observation_id, sizeof(body.observation_id), "%s",
        "bo_22222222-2222-4222-8222-222222222222");
    (void)snprintf(body.observed_at, sizeof(body.observed_at), "%s",
        "2026-09-10t09:00:00z");
    body.has_body_weight = true;
    body.body_weight_kg = 81.0;
    CHECK(trainlog_database_insert_body_observation(database, &body) ==
        TRAINLOG_STATUS_OK);

    (void)memset(&app, 0, sizeof(app));
    app.database = database;
    app_shell_load_dashboard(&app);
    CHECK(!app.dashboard.error);
    CHECK(app.dashboard.has_performance && app.dashboard.performance_count == 3U);
    CHECK(app.dashboard.performance[2].weight_kg == 20.0);
    CHECK(app.dashboard.has_explicit_max && app.dashboard.max_weight_kg == 120.0);
    CHECK(app.dashboard.has_body && app.dashboard.body_count == 1U);
    CHECK(app.dashboard.body_metric == 4U && app.dashboard.body[0].value == 85.0);
    for (size_t index = 0U; index < app.dashboard.week_count; ++index)
        sessions += app.dashboard.weeks[index].sessions;
    CHECK(sessions == 6U);
    CHECK(app.dashboard.completed_session_count == 6U);
    CHECK(app.dashboard.performed_set_count == 4U);
    CHECK(app.dashboard.distinct_exercise_count == 2U);
    CHECK(app.dashboard.explicit_max_count == 1U);

    (void)memset(&terminal, 0, sizeof(terminal));
    surface.terminal = &terminal;
    app.terminal = &terminal;
    app.content = &surface;
    terminal.columns = 120;
    terminal.rows = 35;
    app.layout.columns = 120;
    app.layout.content.width = 90;
    app.layout.content.height = 29;
    app_shell_render_dashboard(&app);
    CHECK(strstr(terminal.output, "Séances 6") != NULL);
    CHECK(strstr(terminal.output, "Mensurations") != NULL);
    CHECK(strstr(terminal.output, "Fréquence") != NULL);
    CHECK(!terminal.coordinate_overflow && !terminal.text_overflow);

    (void)memset(&terminal, 0, sizeof(terminal));
    surface.terminal = &terminal;
    terminal.columns = 100;
    terminal.rows = 30;
    app.layout.columns = 100;
    app.layout.content.width = 76;
    app.layout.content.height = 24;
    app_shell_render_dashboard(&app);
    CHECK(strstr(terminal.output, "Progression globale") != NULL);
    CHECK(strstr(terminal.output, "Mensurations") != NULL);
    CHECK(strstr(terminal.output, "Fréquence") != NULL);
    CHECK(!terminal.coordinate_overflow && !terminal.text_overflow);

    (void)memset(&terminal, 0, sizeof(terminal));
    surface.terminal = &terminal;
    terminal.columns = 80;
    terminal.rows = 24;
    app.layout.columns = 80;
    app.layout.content.width = 80;
    app.layout.content.height = 20;
    app_shell_render_dashboard(&app);
    CHECK(strstr(terminal.output, "Progression") != NULL);
    CHECK(strstr(terminal.output, "Mensurations") != NULL);
    CHECK(strstr(terminal.output, "Fréquence") != NULL);
    CHECK(!terminal.coordinate_overflow && !terminal.text_overflow);

    (void)memset(&terminal, 0, sizeof(terminal));
    surface.terminal = &terminal;
    terminal.columns = 72;
    terminal.rows = 20;
    app.layout.columns = 72;
    app.layout.content.width = 72;
    app.layout.content.height = 16;
    app_shell_render_dashboard(&app);
    CHECK(strstr(terminal.output, "Progression") != NULL);
    CHECK(strstr(terminal.output, "Mensurations") != NULL);
    CHECK(strstr(terminal.output, "Fréquence") != NULL);
    CHECK(!terminal.coordinate_overflow && !terminal.text_overflow);
    trainlog_navigation_init(&app.navigation);
    app.navigation.current.route = TRAINLOG_ROUTE_STATS;
    app.layout.usable = true;
    app.focus = TRAINLOG_FOCUS_CONTENT;
    app_shell_dispatch(&app, '1');
    CHECK(app.dashboard.period == TRAINLOG_STATS_7_DAYS);
    app.content_selected = 2U;
    app_shell_primary(&app);
    CHECK(app.navigation.current.route == TRAINLOG_ROUTE_SESSIONS_COMPLETED);
    trainlog_database_close(database);
    return true;
}

static bool test_stats_dashboard_uses_represented_local_weeks(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogAppContext app;
    TrainlogTimestampKey march;
    TrainlogTimestampKey february;
    CHECK(trainlog_timestamp_parse("2026-03-01T00:30:00+02:00", 25U, &march));
    CHECK(trainlog_timestamp_parse("2026-02-28T22:30:00Z", 20U, &february));
    CHECK(trainlog_timestamp_compare(&march, &february) == 0);
    CHECK(march.local_day == february.local_day + 1);

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(database,
        "ex_33333333-3333-4333-8333-333333333333", "Presse test",
        "presse test", TRAINLOG_TRACKING_REPS, TRAINLOG_RECORDING_SETS,
        (TrainlogExerciseDataFields)0) == TRAINLOG_STATUS_OK);
    CHECK(dashboard_insert_completed_set(database,
        "se_70000000-0000-4000-8000-000000000001",
        "2025-12-29T00:30:00+02:00", "2025-12-29T01:00:00+02:00",
        TRAINLOG_LOAD_EXTERNAL, "leg_press", 10.0));
    CHECK(dashboard_insert_completed_set(database,
        "se_70000000-0000-4000-8000-000000000002",
        "2025-12-28T22:30:00Z", "2025-12-28T23:00:00Z",
        TRAINLOG_LOAD_EXTERNAL, "leg_press", 20.0));
    CHECK(dashboard_insert_completed_set(database,
        "se_70000000-0000-4000-8000-000000000003",
        "2026-01-01T00:30:00+14:00", "2026-01-01T01:00:00+14:00",
        TRAINLOG_LOAD_EXTERNAL, "leg_press", 30.0));
    CHECK(dashboard_insert_completed_set(database,
        "se_70000000-0000-4000-8000-000000000004",
        "2026-01-04T23:30:00-02:00", "2026-01-04T23:45:00-02:00",
        TRAINLOG_LOAD_EXTERNAL, "leg_press", 40.0));

    (void)memset(&app, 0, sizeof(app));
    app.database = database;
    app.dashboard.period = TRAINLOG_STATS_ALL;
    app_shell_load_dashboard(&app);
    CHECK(!app.dashboard.error);
    CHECK(app.dashboard.week_count >= 1U &&
        app.dashboard.week_count <= DASHBOARD_BUCKET_COUNT);
    /* Tout projects the complete represented history instead of silently
     * retaining the former six-week viewport. */
    {
        size_t projected = 0U;
        for (size_t index = 0U; index < app.dashboard.week_count; ++index)
            projected += app.dashboard.weeks[index].sessions;
        CHECK(projected == 4U);
    }
    CHECK(app.dashboard.completed_session_count == 4U);
    trainlog_database_close(database);
    return true;
}

static bool test_stats_dashboard_current_week_remains_zero_after_past_activity(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogAppContext app;
    TrainlogTerminal terminal;
    TrainlogSurface surface;
    char started_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    char ended_at[TRAINLOG_TIMESTAMP_MAX + 1U];
    time_t now = time(NULL);

    CHECK(now != (time_t)-1);
    CHECK(dashboard_utc_timestamp_at(now - 8 * 24 * 60 * 60, started_at));
    CHECK(dashboard_utc_timestamp_at(now - 8 * 24 * 60 * 60 + 3600, ended_at));
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(database,
        "ex_33333333-3333-4333-8333-333333333333", "Presse test",
        "presse test", TRAINLOG_TRACKING_REPS, TRAINLOG_RECORDING_SETS,
        (TrainlogExerciseDataFields)0) == TRAINLOG_STATUS_OK);
    CHECK(dashboard_insert_completed_set(database,
        "se_70000000-0000-4000-8000-000000000005", started_at, ended_at,
        TRAINLOG_LOAD_EXTERNAL, "leg_press", 20.0));
    (void)memset(&app, 0, sizeof(app));
    app.database = database;
    app.dashboard.period = TRAINLOG_STATS_ALL;
    app_shell_load_dashboard(&app);
    CHECK(!app.dashboard.error);
    CHECK(app.dashboard.week_count >= 1U &&
        app.dashboard.week_count <= DASHBOARD_BUCKET_COUNT);
    CHECK(app.dashboard.weeks[app.dashboard.week_count - 1U].sessions == 0U);
    dashboard_prepare_render(&app, &terminal, &surface, 80, 24, 80, 20);
    app_shell_dashboard_frequency_chart(&app, &app.dashboard, 3, 4, 64, 3);
    CHECK(terminal.dashboard_block_count > 0U);
    CHECK(strstr(terminal.output, "actuel 0 séances") != NULL);
    CHECK(strstr(terminal.output, "P−") != NULL);
    CHECK(strstr(terminal.output, "actuel") != NULL);
    trainlog_database_close(database);
    return true;
}

static bool test_stats_dashboard_exact_dose_strict_later_and_invalid_rows(void)
{
    TrainlogDatabase *database = NULL;
    TrainlogAppContext app;
    TrainlogTerminal terminal;
    TrainlogSurface surface;
    size_t working = 0U;
    size_t maxima = 0U;
    size_t index;
    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise_profiled(database,
        "ex_33333333-3333-4333-8333-333333333333", "Presse parité",
        "presse parite", TRAINLOG_TRACKING_REPS, TRAINLOG_RECORDING_SETS,
        (TrainlogExerciseDataFields)0) == TRAINLOG_STATUS_OK);
    CHECK(dashboard_insert_completed_set_dose(database,
        "se_90000000-0000-4000-8000-000000000001", "2026-09-08T08:00:00Z",
        "", TRAINLOG_LOAD_EXTERNAL, "leg_press", 100.0, 5));
    CHECK(sqlite3_exec(database->connection,
        "INSERT INTO performed_sets(session_exercise_row_id,position,reps,weight_kg) "
        "SELECT id,1,10,80.0 FROM session_exercises WHERE "
        "session_row_id=(SELECT id FROM sessions WHERE "
        "session_id='se_90000000-0000-4000-8000-000000000001');",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(dashboard_insert_completed_set_dose(database,
        "se_90000000-0000-4000-8000-000000000002", "2026-09-09T08:00:00Z",
        "", TRAINLOG_LOAD_EXTERNAL, "leg_press", 90.0, 10));
    CHECK(dashboard_insert_completed_set_dose(database,
        "se_90000000-0000-4000-8000-000000000003", "2026-09-10T08:00:00Z",
        "", TRAINLOG_LOAD_EXTERNAL, "leg_press", 50.0, 12));
    CHECK(dashboard_insert_completed_set_dose(database,
        "se_90000000-0000-4000-8000-000000000004",
        "2026-09-10T09:00:00+01:00", "", TRAINLOG_LOAD_EXTERNAL,
        "leg_press", 60.0, 12));
    CHECK(dashboard_insert_explicit_max_at(database,
        "se_90000000-0000-4000-8000-000000000005", "2026-09-10T10:00:00Z",
        "sxe_90000000-0000-4000-8000-000000000005", 100.0));
    CHECK(dashboard_insert_explicit_max_at(database,
        "se_90000000-0000-4000-8000-000000000006",
        "2026-09-10T11:00:00+01:00",
        "sxe_90000000-0000-4000-8000-000000000006", 110.0));
    /* INVARIANT: one occurrence may own independent work and MAX facts. */
    CHECK(sqlite3_exec(database->connection,
        "INSERT INTO performed_sets(session_exercise_row_id,position,reps,weight_kg) "
        "SELECT id,0,15,70.0 FROM session_exercises WHERE "
        "entry_id='sxe_90000000-0000-4000-8000-000000000005';",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(dashboard_insert_completed_set_dose(database,
        "se_90000000-0000-4000-8000-000000000007", "2026-09-11T08:00:00Z",
        "", TRAINLOG_LOAD_EXTERNAL, "leg_press", 999.0, 10));
    CHECK(sqlite3_exec(database->connection,
        "UPDATE sessions SET started_at='legacy-invalid' WHERE "
        "session_id='se_90000000-0000-4000-8000-000000000007';",
        NULL, NULL, NULL) == SQLITE_OK);
    CHECK(dashboard_insert_completed_set_dose(database,
        "se_90000000-0000-4000-8000-000000000008", "2026-09-11T09:00:00Z",
        "", TRAINLOG_LOAD_EXTERNAL, "leg_press", 1.0, 20));
    CHECK(sqlite3_exec(database->connection,
        "UPDATE performed_sets SET weight_kg=NULL WHERE session_exercise_row_id="
        "(SELECT se.id FROM session_exercises se JOIN sessions s ON "
        "s.id=se.session_row_id WHERE "
        "s.session_id='se_90000000-0000-4000-8000-000000000008');",
        NULL, NULL, NULL) == SQLITE_OK);

    (void)memset(&app, 0, sizeof(app));
    app.database = database;
    app.dashboard.period = TRAINLOG_STATS_ALL;
    app_shell_load_dashboard(&app);
    CHECK(!app.dashboard.error && app.dashboard.invalid_data);
    CHECK(app.dashboard.completed_session_count == 7U);
    CHECK(app.dashboard.performed_set_count == 7U);
    CHECK(app.dashboard.explicit_max_count == 2U);
    CHECK(app.dashboard.distinct_exercise_count == 1U);
    for (index = 0U; index < app.dashboard.week_count; ++index) {
        working += app.dashboard.weeks[index].working_improvements;
        maxima += app.dashboard.weeks[index].max_improvements;
    }
    CHECK(working == 1U && maxima == 0U);
    dashboard_prepare_render(&app, &terminal, &surface, 100, 30, 76, 24);
    app_shell_render_dashboard(&app);
    CHECK(strstr(terminal.output, "Données historiques invalides ignorées.") != NULL);
    CHECK(strstr(terminal.output, "Progression globale") != NULL);
    CHECK(strstr(terminal.output, "Mensurations") != NULL);
    CHECK(strstr(terminal.output, "Fréquence") != NULL);
    trainlog_database_close(database);
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

static bool test_stats_detail_charts_and_global_zone_projection(void)
{
    TrainlogAppContext app;
    TrainlogTerminal terminal;
    TrainlogSurface surface;
    TrainlogBodyMetricPoint body[2];
    TrainlogDatabase *database = NULL;
    size_t index;
    bool chest = false;
    bool back = false;
    bool unclassified = false;
    const char *const chest_zone[] = {NULL};
    const char *const back_zone[] = {NULL};
    (void)memset(&app, 0, sizeof(app));
    dashboard_prepare_render(&app, &terminal, &surface, 72, 20, 72, 16);

    /* The sparse-state contract is deliberately exercised through the exact
     * detail painters: empty and singleton series have prose only, while two
     * real observations acquire Unicode points/segments rather than ASCII. */
    app_shell_draw_performance_graph(&app, NULL, TRAINLOG_LOAD_EXTERNAL,
        TRAINLOG_TRACKING_REPS, 5, 4, false);
    CHECK(strstr(terminal.output, "Aucun point exploitable") != NULL);
    CHECK(terminal.surface_draw_count == 0U);
    (void)memset(&terminal, 0, sizeof(terminal)); surface.terminal = &terminal;
    app.performance_count = 1U;
    app.performance_points[0].has_performance = 1;
    app.performance_points[0].load_mode = TRAINLOG_LOAD_EXTERNAL;
    app.performance_points[0].weight_kg = 40.0;
    app.performance_points[0].metric_value = 8;
    (void)snprintf(app.performance_points[0].started_at,
        sizeof(app.performance_points[0].started_at), "%s", "2026-09-01T08:00:00Z");
    app_shell_draw_performance_graph(&app, NULL, TRAINLOG_LOAD_EXTERNAL,
        TRAINLOG_TRACKING_REPS, 5, 4, false);
    CHECK(strstr(terminal.output, "1 point") != NULL);
    CHECK(strstr(terminal.output, "tendance indisponible") != NULL);
    CHECK(terminal.surface_draw_count == 0U);
    app.performance_count = 2U;
    app.performance_points[1] = app.performance_points[0];
    app.performance_points[1].weight_kg = 45.0;
    (void)snprintf(app.performance_points[1].started_at,
        sizeof(app.performance_points[1].started_at), "%s", "2026-09-02T08:00:00Z");
    (void)memset(&terminal, 0, sizeof(terminal)); surface.terminal = &terminal;
    app_shell_draw_performance_graph(&app, NULL, TRAINLOG_LOAD_EXTERNAL,
        TRAINLOG_TRACKING_REPS, 5, 4, false);
    CHECK(rendered_draw_column(&terminal, 0x25cfU, 0U) >= 0);
    for (index = 0U; index < terminal.drawn_count; ++index)
        CHECK(terminal.drawn[index].codepoint != (uint32_t)'*' &&
            terminal.drawn[index].codepoint != (uint32_t)'.' &&
            terminal.drawn[index].codepoint != (uint32_t)'O');

    (void)memset(body, 0, sizeof(body));
    (void)memset(&terminal, 0, sizeof(terminal)); surface.terminal = &terminal;
    app_shell_draw_body_points(&app, body, 0U, 5, 4, "kg");
    CHECK(strstr(terminal.output, "Aucune donnée") != NULL);
    body[0].value = 80.0;
    (void)snprintf(body[0].observed_at, sizeof(body[0].observed_at), "%s",
        "2026-09-01T08:00:00Z");
    (void)memset(&terminal, 0, sizeof(terminal)); surface.terminal = &terminal;
    app_shell_draw_body_points(&app, body, 1U, 5, 4, "kg");
    CHECK(strstr(terminal.output, "1 relevé") != NULL);
    CHECK(terminal.surface_draw_count == 0U);
    body[1] = body[0]; body[1].value = 79.0;
    (void)snprintf(body[1].observed_at, sizeof(body[1].observed_at), "%s",
        "2026-09-02T08:00:00Z");
    (void)memset(&terminal, 0, sizeof(terminal)); surface.terminal = &terminal;
    app_shell_draw_body_points(&app, body, 2U, 5, 4, "kg");
    CHECK(rendered_draw_column(&terminal, 0x25cfU, 0U) >= 0);

    CHECK(trainlog_database_open(":memory:", &database) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise(database,
        "ex_10000000-0000-4000-8000-000000000001", "Source", "source",
        TRAINLOG_TRACKING_REPS) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise(database,
        "ex_10000000-0000-4000-8000-000000000002", "Canonique", "canonique",
        TRAINLOG_TRACKING_REPS) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise(database,
        "ex_10000000-0000-4000-8000-000000000003", "Dos test", "dos test",
        TRAINLOG_TRACKING_REPS) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_insert_exercise(database,
        "ex_10000000-0000-4000-8000-000000000004", "Sans zone", "sans zone",
        TRAINLOG_TRACKING_REPS) == TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_replace_exercise_body_zones(database,
        "ex_10000000-0000-4000-8000-000000000001", "chest", chest_zone, 0U) ==
        TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_replace_exercise_body_zones(database,
        "ex_10000000-0000-4000-8000-000000000002", "chest", chest_zone, 0U) ==
        TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_replace_exercise_body_zones(database,
        "ex_10000000-0000-4000-8000-000000000003", "back", back_zone, 0U) ==
        TRAINLOG_STATUS_OK);
    CHECK(trainlog_database_merge_exercises(database,
        "ex_10000000-0000-4000-8000-000000000001",
        "ex_10000000-0000-4000-8000-000000000002") == TRAINLOG_STATUS_OK);
    (void)memset(&app, 0, sizeof(app)); app.database = database;
    app.dashboard.period = TRAINLOG_STATS_ALL;
    app_shell_load_dashboard(&app);
    CHECK(!app.dashboard.error && app.dashboard.zone_count == 3U);
    for (index = 0U; index < app.dashboard.zone_count; ++index) {
        chest = chest || (strcmp(app.dashboard.zones[index].label, "Pectoraux") == 0 &&
            app.dashboard.zones[index].count == 1U);
        back = back || (strcmp(app.dashboard.zones[index].label, "Dos") == 0 &&
            app.dashboard.zones[index].count == 1U);
        unclassified = unclassified ||
            (strcmp(app.dashboard.zones[index].label, "Non classés") == 0 &&
             app.dashboard.zones[index].count == 1U);
    }
    CHECK(chest && back && unclassified);

    /* Regression: uneven adjacent bars used to appear as one solid staircase.
     * Exercise the real dashboard allocation with the reported distribution,
     * and inspect cells rather than accepting matching label text. */
    {
        static const size_t counts[] = {8U, 6U, 5U, 2U, 2U, 2U, 1U, 1U};
        static const int columns[] = {120, 100, 80, 72};
        static const int rows[] = {35, 30, 24, 20};
        static const int widths[] = {96, 76, 80, 72};
        static const int heights[] = {29, 24, 20, 16};
        static const int chart_tops[] = {19, 17, 15, 15};
        static const int chart_rows[] = {8, 5, 5, 1};
        static const size_t visible[] = {8U, 4U, 4U, 1U};
        size_t geometry;
        app.dashboard.zone_count = sizeof(counts) / sizeof(counts[0]);
        for (index = 0U; index < app.dashboard.zone_count; ++index) {
            (void)snprintf(app.dashboard.zones[index].label,
                sizeof(app.dashboard.zones[index].label), "Zone %zu", index + 1U);
            app.dashboard.zones[index].count = counts[index];
        }
        for (geometry = 0U; geometry < 4U; ++geometry) {
            int chart_width = widths[geometry] - 8;
            int label_width = chart_width >= 64 ? 20 :
                chart_width >= 48 ? 16 : 12;
            int bar_width = chart_width - label_width - 6;
            int bar_left = 4 + label_width + 1;
            int count_column = 4 + label_width + 2 + bar_width;
            int stride = columns[geometry] >= 100 &&
                sizeof(counts) / sizeof(counts[0]) <=
                    (size_t)((chart_rows[geometry] + 1) / 2) ? 2 : 1;
            size_t bucket;
            dashboard_prepare_render(&app, &terminal, &surface,
                columns[geometry], rows[geometry], widths[geometry],
                heights[geometry]);
            app_shell_render_dashboard(&app);
            CHECK(strstr(terminal.output, "Répartition du catalogue") != NULL);
            CHECK(!terminal.coordinate_overflow && !terminal.text_overflow);
            CHECK(terminal.stats_distribution_row >
                terminal.stats_frequency_bottom_row);
            for (bucket = 0U; bucket < visible[geometry]; ++bucket) {
                int expected_row = chart_tops[geometry] + (int)bucket * stride;
                int expected_blocks = (int)((counts[bucket] *
                    (size_t)bar_width + counts[0] - 1U) / counts[0]);
                size_t drawn;
                size_t printed;
                size_t blocks = 0U;
                bool found_count = false;
                char count_text[8];
                (void)snprintf(count_text, sizeof(count_text), "%zu", counts[bucket]);
                for (printed = 0U; printed < terminal.printed_count; ++printed)
                    if (terminal.printed[printed].row == expected_row &&
                        terminal.printed[printed].column == count_column &&
                        strcmp(terminal.printed[printed].text, count_text) == 0)
                        found_count = true;
                for (drawn = 0U; drawn < terminal.drawn_count; ++drawn) {
                    const int drawn_row = terminal.drawn[drawn].row;
                    const int drawn_column = terminal.drawn[drawn].column;
                    const uint32_t glyph = terminal.drawn[drawn].codepoint;
                    if (drawn_row < chart_tops[geometry] ||
                        drawn_row >= chart_tops[geometry] + chart_rows[geometry])
                        continue;
                    CHECK(glyph != (uint32_t)'#' && glyph != (uint32_t)'*' &&
                        glyph != (uint32_t)'+' && glyph != 0x2588U);
                    if (drawn_row == expected_row && glyph == 0x2586U) {
                        CHECK(drawn_column >= bar_left &&
                            drawn_column < bar_left + expected_blocks);
                        ++blocks;
                    }
                }
                CHECK(found_count && blocks == (size_t)expected_blocks);
            }
            {
                size_t drawn;
                for (drawn = 0U; drawn < terminal.drawn_count; ++drawn) {
                    int relative_row = terminal.drawn[drawn].row -
                        chart_tops[geometry];
                    if (relative_row < 0 || relative_row >= chart_rows[geometry] ||
                        terminal.drawn[drawn].codepoint != 0x2586U) continue;
                    CHECK(relative_row % stride == 0);
                    CHECK((size_t)(relative_row / stride) < visible[geometry]);
                }
            }
            if (stride == 2) {
                size_t gap;
                for (gap = 0U; gap + 1U < visible[geometry]; ++gap) {
                    size_t drawn;
                    int gap_row = chart_tops[geometry] + (int)gap * 2 + 1;
                    for (drawn = 0U; drawn < terminal.drawn_count; ++drawn)
                        CHECK(terminal.drawn[drawn].row != gap_row ||
                            terminal.drawn[drawn].codepoint != 0x2586U);
                }
            }
        }
        /* A tall wide chart has room for all eight categories plus seven
         * gaps. Verify that the enhancement really leaves those gap cells
         * empty instead of reconnecting the old staircase. */
        dashboard_prepare_render(&app, &terminal, &surface, 120, 35, 96, 29);
        app_shell_dashboard_zone_chart(&app, &app.dashboard, 0, 4, 88, 15);
        for (index = 0U; index < 8U; ++index) {
            size_t drawn;
            for (drawn = 0U; drawn < terminal.drawn_count; ++drawn)
                if (terminal.drawn[drawn].row == (int)index * 2)
                    CHECK(terminal.drawn[drawn].codepoint == 0x2586U);
            if (index + 1U < 8U)
                for (drawn = 0U; drawn < terminal.drawn_count; ++drawn)
                    CHECK(terminal.drawn[drawn].row != (int)index * 2 + 1 ||
                        terminal.drawn[drawn].codepoint != 0x2586U);
        }
    }
    for (index = 0U; index < 4U; ++index) {
        static const int columns[] = {120, 100, 80, 72};
        static const int rows[] = {35, 30, 24, 20};
        static const int widths[] = {96, 76, 80, 72};
        static const int heights[] = {29, 24, 20, 16};
        dashboard_prepare_render(&app, &terminal, &surface, columns[index], rows[index],
            widths[index], heights[index]);
        app_shell_render_dashboard(&app);
        CHECK(strstr(terminal.output, "Répartition du catalogue") != NULL);
        CHECK(rendered_draw_column(&terminal, 0x2586U, 0U) >= 0);
        CHECK(!terminal.coordinate_overflow && !terminal.text_overflow);
        CHECK(terminal.stats_distribution_row >
            terminal.stats_frequency_bottom_row);
    }
    trainlog_navigation_init(&app.navigation);
    app.navigation.current.route = TRAINLOG_ROUTE_STATS;
    app.content_selected = 3U;
    app_shell_primary(&app);
    CHECK(app.navigation.current.route == TRAINLOG_ROUTE_EXERCISES);
    trainlog_database_close(database);
    return true;
}

static bool test_body_profile_snapshot_and_evolution_are_separate(void)
{
    TrainlogAppContext app;
    TrainlogTerminal terminal;
    TrainlogSurface surface;
    const char *ids[2];
    size_t geometry;
    size_t drawn;
    static const int columns[] = {120, 100, 80, 72};
    static const int rows[] = {35, 30, 24, 20};
    static const int widths[] = {96, 76, 80, 72};
    static const int heights[] = {29, 24, 20, 16};
    (void)memset(&app, 0, sizeof(app));
    app.navigation.current.route = TRAINLOG_ROUTE_BODY;
    app.focus = TRAINLOG_FOCUS_CONTENT;
    app.layout.usable = true;
    app.loaded_count = 2U;
    (void)snprintf(app.body_records[0].observation_id,
        sizeof(app.body_records[0].observation_id), "%s", "bo_latest");
    (void)snprintf(app.body_records[0].observed_at,
        sizeof(app.body_records[0].observed_at), "%s", "2026-09-10T08:00:00Z");
    app.body_records[0].has_body_weight = true;
    app.body_records[0].body_weight_kg = 80.0;
    app.body_records[0].has_chest = true;
    app.body_records[0].chest_cm = 100.0;
    app.body_records[0].has_waist = true;
    app.body_records[0].waist_cm = 85.0;
    app.body_records[1] = app.body_records[0];
    (void)snprintf(app.body_records[1].observation_id,
        sizeof(app.body_records[1].observation_id), "%s", "bo_older");
    (void)snprintf(app.body_records[1].observed_at,
        sizeof(app.body_records[1].observed_at), "%s", "2026-08-10T08:00:00Z");
    app.body_records[1].body_weight_kg = 78.0;
    app.body_records[1].chest_cm = 90.0;
    app.body_records[1].waist_cm = 75.0;
    ids[0] = app.body_records[0].observation_id;
    ids[1] = app.body_records[1].observation_id;
    trainlog_list_init(&app.list);
    trainlog_list_set_items(&app.list, ids, 2U, 8U, true, false);

    app.body_metric_selected = 0U;
    app.body_global_series[0].count = 1U;
    app.body_global_series[0].points[0].value = 80.0;
    (void)snprintf(app.body_global_series[0].points[0].observed_at,
        sizeof(app.body_global_series[0].points[0].observed_at), "%s",
        "2026-09-10T08:00:00Z");
    app.body_global_series[4].count = 1U;
    app.body_global_series[4].points[0].value = 85.0;
    (void)snprintf(app.body_global_series[4].points[0].observed_at,
        sizeof(app.body_global_series[4].points[0].observed_at), "%s",
        "2026-09-10T08:00:00Z");

    dashboard_prepare_render(&app, &terminal, &surface, 72, 20, 72, 16);
    app_shell_render_body_profile(&app);
    CHECK(strstr(terminal.output, "Poids : 80.00 kg") != NULL);
    CHECK(strstr(terminal.output, "Poitrine") != NULL);
    CHECK(strstr(terminal.output, "100.0 cm") != NULL);
    CHECK(strstr(terminal.output, "Tour de taille") != NULL);
    CHECK(strstr(terminal.output, "85.0 cm") != NULL);
    CHECK(strstr(terminal.output, "Épaules") == NULL);
    CHECK(strstr(terminal.output, "1 relevé · tendance indisponible") != NULL);
    CHECK(rendered_draw_column(&terminal, 0x2586U, 0U) >= 0);
    for (drawn = 0U; drawn < terminal.drawn_count; ++drawn) {
        CHECK(terminal.drawn[drawn].row != 4);
        CHECK(terminal.drawn[drawn].codepoint != (uint32_t)'*' &&
            terminal.drawn[drawn].codepoint != (uint32_t)'.' &&
            terminal.drawn[drawn].codepoint != (uint32_t)'O');
    }

    app.body_global_series[0].count = 2U;
    app.body_global_series[0].points[1].value = 80.0;
    (void)snprintf(app.body_global_series[0].points[1].observed_at,
        sizeof(app.body_global_series[0].points[1].observed_at), "%s",
        "2026-09-10T08:00:00Z");
    app.body_global_series[0].points[0].value = 78.0;
    (void)snprintf(app.body_global_series[0].points[0].observed_at,
        sizeof(app.body_global_series[0].points[0].observed_at), "%s",
        "2026-08-10T08:00:00Z");
    dashboard_prepare_render(&app, &terminal, &surface, 80, 24, 80, 20);
    app_shell_render_body_profile(&app);
    CHECK(strstr(terminal.output, "2026-08-10") != NULL);
    CHECK(strstr(terminal.output, "2026-09-10") != NULL);
    CHECK(rendered_draw_column(&terminal, 0x25cfU, 0U) >= 0);

    app_shell_dispatch(&app, TRAINLOG_KEY_DOWN);
    CHECK(app.list.selected_index == 1U);
    dashboard_prepare_render(&app, &terminal, &surface, 80, 24, 80, 20);
    app_shell_render_body_profile(&app);
    CHECK(strstr(terminal.output, "Poids : 78.00 kg") != NULL);
    CHECK(strstr(terminal.output, "90.0 cm") != NULL);
    CHECK(strstr(terminal.output, "100.0 cm") == NULL);

    app.body_metric_selected = 0U;
    app_shell_move_available_body_metric(&app, 1);
    CHECK(app.body_metric_selected == 4U);
    for (geometry = 0U; geometry < 4U; ++geometry) {
        dashboard_prepare_render(&app, &terminal, &surface, columns[geometry],
            rows[geometry], widths[geometry], heights[geometry]);
        app_shell_render_body_profile(&app);
        CHECK(strstr(terminal.output, "PROFIL ACTUEL") != NULL);
        CHECK(strstr(terminal.output, "ÉVOLUTION DANS LE TEMPS") != NULL);
        CHECK(!terminal.coordinate_overflow && !terminal.text_overflow);
    }
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
        !test_stats_dashboard_sparse_series_names_frequency_and_footer() ||
        !test_stats_dashboard_uses_real_exact_time_snapshots_responsively() ||
        !test_stats_dashboard_uses_represented_local_weeks() ||
        !test_stats_dashboard_current_week_remains_zero_after_past_activity() ||
        !test_stats_dashboard_exact_dose_strict_later_and_invalid_rows() ||
        !test_body_graphs_draw_inside_minimum_content_surface() ||
        !test_body_profile_snapshot_and_evolution_are_separate() ||
        !test_stats_detail_charts_and_global_zone_projection()) return 1;
    (void)printf("PASS tui_workflows\n");
    return 0;
}
