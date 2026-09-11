#ifndef TRAINLOG_APP_SHELL_H
#define TRAINLOG_APP_SHELL_H

/**
 * @file app_shell.h
 * @brief Trainlog-owned state contracts for the persistent desktop shell.
 */

#include <stdbool.h>
#include <stddef.h>

#define TRAINLOG_SHELL_MIN_COLUMNS 72
#define TRAINLOG_SHELL_MIN_ROWS 20
#define TRAINLOG_SHELL_SIDEBAR_COLUMNS 22
#define TRAINLOG_SHELL_OVERLAY_LIMIT 3U
#define TRAINLOG_SHELL_ROUTE_DEPTH 8U
#define TRAINLOG_SHELL_ACTION_LIMIT 24U
#define TRAINLOG_SHELL_SEARCH_CAPACITY 201U
#define TRAINLOG_SHELL_STABLE_ID_CAPACITY 80U

typedef struct TrainlogRect {
    int y;
    int x;
    int height;
    int width;
} TrainlogRect;

typedef struct TrainlogShellLayout {
    int rows;
    int columns;
    bool usable;
    bool sidebar_visible;
    bool sidebar_expanded;
    TrainlogRect header;
    TrainlogRect sidebar;
    TrainlogRect separator;
    TrainlogRect content;
    TrainlogRect footer;
} TrainlogShellLayout;

typedef enum TrainlogAppRoute {
    TRAINLOG_ROUTE_HOME = 0,
    TRAINLOG_ROUTE_SESSIONS,
    TRAINLOG_ROUTE_SESSION_CURRENT,
    TRAINLOG_ROUTE_SESSION_GENERATOR,
    TRAINLOG_ROUTE_SESSION_MANUAL,
    TRAINLOG_ROUTE_SESSIONS_COMPLETED,
    TRAINLOG_ROUTE_SESSION_DETAIL,
    TRAINLOG_ROUTE_EXERCISES,
    TRAINLOG_ROUTE_EXERCISE_DETAIL,
    TRAINLOG_ROUTE_EXERCISE_KNOWLEDGE,
    TRAINLOG_ROUTE_EXERCISE_PERFORMANCE,
    TRAINLOG_ROUTE_EXERCISE_MAX,
    TRAINLOG_ROUTE_EQUIPMENT,
    TRAINLOG_ROUTE_EQUIPMENT_DETAIL,
    TRAINLOG_ROUTE_STATS,
    TRAINLOG_ROUTE_STATS_EXERCISE,
    TRAINLOG_ROUTE_BODY,
    TRAINLOG_ROUTE_BODY_DETAIL,
    TRAINLOG_ROUTE_BODY_METRIC,
    TRAINLOG_ROUTE_BODY_TRENDS,
    TRAINLOG_ROUTE_BODY_GLOBAL,
    TRAINLOG_ROUTE_BODY_ANALYTICS,
    TRAINLOG_ROUTE_MAX,
    TRAINLOG_ROUTE_SYNC,
    TRAINLOG_ROUTE_SETTINGS,
    TRAINLOG_ROUTE_COUNT
} TrainlogAppRoute;

typedef enum TrainlogFocusTarget {
    TRAINLOG_FOCUS_NAVIGATION = 0,
    TRAINLOG_FOCUS_SEARCH,
    TRAINLOG_FOCUS_CONTENT,
    TRAINLOG_FOCUS_EDITOR,
    TRAINLOG_FOCUS_ACTIONS,
    TRAINLOG_FOCUS_OVERLAY
} TrainlogFocusTarget;

typedef enum TrainlogIntent {
    TRAINLOG_INTENT_NONE = 0,
    TRAINLOG_INTENT_OPEN_ROUTE,
    TRAINLOG_INTENT_OPEN_NAVIGATION,
    TRAINLOG_INTENT_OPEN_ACTIONS,
    TRAINLOG_INTENT_OPEN_HELP,
    TRAINLOG_INTENT_OPEN_SEARCH,
    TRAINLOG_INTENT_PRIMARY,
    TRAINLOG_INTENT_BACK,
    TRAINLOG_INTENT_QUIT,
    TRAINLOG_INTENT_DISCARD,
    TRAINLOG_INTENT_SAVE,
    TRAINLOG_INTENT_REFRESH
} TrainlogIntent;

typedef struct TrainlogRouteTarget {
    TrainlogAppRoute route;
    char stable_id[TRAINLOG_SHELL_STABLE_ID_CAPACITY];
} TrainlogRouteTarget;

typedef struct TrainlogNavigationState {
    TrainlogRouteTarget current;
    TrainlogRouteTarget back[TRAINLOG_SHELL_ROUTE_DEPTH];
    size_t back_count;
} TrainlogNavigationState;

typedef struct TrainlogAction {
    const char *identifier;
    int key;
    const char *label;
    bool enabled;
    unsigned priority;
    TrainlogIntent intent;
    TrainlogAppRoute route;
} TrainlogAction;

typedef struct TrainlogActionModel {
    TrainlogAction items[TRAINLOG_SHELL_ACTION_LIMIT];
    size_t count;
} TrainlogActionModel;

typedef enum TrainlogOverlayType {
    TRAINLOG_OVERLAY_NAVIGATION = 0,
    TRAINLOG_OVERLAY_ACTIONS,
    TRAINLOG_OVERLAY_HELP,
    TRAINLOG_OVERLAY_CONFIRMATION,
    TRAINLOG_OVERLAY_RECOVERY,
    TRAINLOG_OVERLAY_EXERCISE_MERGE
} TrainlogOverlayType;

typedef struct TrainlogOverlay {
    TrainlogOverlayType type;
    TrainlogFocusTarget restore_focus;
    char restore_stable_id[TRAINLOG_SHELL_STABLE_ID_CAPACITY];
    size_t selected;
} TrainlogOverlay;

typedef struct TrainlogOverlayStack {
    TrainlogOverlay items[TRAINLOG_SHELL_OVERLAY_LIMIT];
    size_t count;
} TrainlogOverlayStack;

typedef struct TrainlogListState {
    char selected_id[TRAINLOG_SHELL_STABLE_ID_CAPACITY];
    size_t selected_index;
    size_t viewport_start;
    size_t visible_rows;
    size_t item_count;
    bool total_known;
    bool more_available;
} TrainlogListState;

typedef struct TrainlogSearchState {
    char text[TRAINLOG_SHELL_SEARCH_CAPACITY];
    size_t bytes;
    size_t cursor;
    bool open;
    bool focused;
    bool capacity_error;
} TrainlogSearchState;

typedef enum TrainlogFormResult {
    TRAINLOG_FORM_IGNORED = 0,
    TRAINLOG_FORM_EDITED,
    TRAINLOG_FORM_SUBMIT,
    TRAINLOG_FORM_CANCEL,
    TRAINLOG_FORM_NEXT,
    TRAINLOG_FORM_PREVIOUS,
    TRAINLOG_FORM_OPEN_NAVIGATION,
    TRAINLOG_FORM_OPEN_ACTIONS
} TrainlogFormResult;

typedef struct TrainlogFormField {
    char text[TRAINLOG_SHELL_SEARCH_CAPACITY];
    size_t bytes;
    size_t cursor;
    bool active;
    bool capacity_error;
} TrainlogFormField;

typedef struct TrainlogDurabilityState {
    bool session_draft;
    bool session_dirty;
    bool generator_configuration_dirty;
    bool generator_preview;
    bool generator_preview_dirty;
    bool transient_form_dirty;
} TrainlogDurabilityState;

typedef enum TrainlogLeaveDecision {
    TRAINLOG_LEAVE_ALLOW = 0,
    TRAINLOG_LEAVE_CONFIRM_KEEP,
    TRAINLOG_LEAVE_CONFIRM_DISCARD
} TrainlogLeaveDecision;

/* WHY: one geometry policy keeps renderers free of terminal-specific bounds.
 * CONTRACT: NULL layout is ignored; otherwise output is fully reset. Usable
 * requires at least 72x20; sidebar thresholds are 100x26 and 120x32. */
void trainlog_shell_layout_compute(int columns, int rows,
                                   TrainlogShellLayout *layout);
/* CONTRACT: returns an all-zero rectangle for NULL/unusable layout; preferred
 * nonpositive/oversize dimensions are clamped. Result is a value copy. */
TrainlogRect trainlog_shell_overlay_rect(const TrainlogShellLayout *layout,
                                         int preferred_width,
                                         int preferred_height);
/* CONTRACT: false for NULL or geometrically invalid rectangles; no mutation. */
bool trainlog_shell_rect_contains(const TrainlogRect *outer,
                                  const TrainlogRect *inner);

/* CONTRACT: NULL is ignored. init selects Home. open copies at most 79 bytes
 * of stable_id (NULL means empty), keeps at most eight back entries, and
 * returns false only for NULL navigation. back mutates only when history exists. */
void trainlog_navigation_init(TrainlogNavigationState *navigation);
bool trainlog_navigation_open(TrainlogNavigationState *navigation,
                              TrainlogAppRoute route,
                              const char *stable_id);
bool trainlog_navigation_back(TrainlogNavigationState *navigation);

/* CONTRACT: clear ignores NULL. add copies the action struct, but borrows its
 * identifier and label strings for the model lifetime; it rejects NULL strings,
 * duplicate stable identifiers and the 24-action bound. Lookup returns a
 * borrowed item or NULL. Priority
 * ordering is enabled actions by ascending priority, then insertion order. */
void trainlog_actions_clear(TrainlogActionModel *model);
bool trainlog_actions_add(TrainlogActionModel *model,
                          const TrainlogAction *action);
const TrainlogAction *trainlog_actions_find_key(const TrainlogActionModel *model,
                                                int key);
const TrainlogAction *trainlog_actions_at_priority(const TrainlogActionModel *model,
                                                   size_t position);

/* CONTRACT: init ignores NULL. push copies up to 79 stable-ID bytes and fails
 * for NULL/full (three overlays) stacks. top borrows an item until the stack
 * mutates. pop fails when empty; on success it mutates the stack and writes
 * only non-NULL outputs, whose stable-ID buffer must hold 80 bytes. */
void trainlog_overlays_init(TrainlogOverlayStack *stack);
bool trainlog_overlays_push(TrainlogOverlayStack *stack,
                            TrainlogOverlayType type,
                            TrainlogFocusTarget restore_focus,
                            const char *restore_stable_id);
const TrainlogOverlay *trainlog_overlays_top(const TrainlogOverlayStack *stack);
bool trainlog_overlays_pop(TrainlogOverlayStack *stack,
                           TrainlogFocusTarget *restore_focus,
                           char restore_stable_id[TRAINLOG_SHELL_STABLE_ID_CAPACITY]);

/* CONTRACT: list state never owns stable_ids: callers keep every supplied
 * string and array alive through the matching move/set operation. set accepts
 * zero items (clears selection), treats visible_rows zero as one, and copies a
 * selected stable ID with the 79-byte truncation rule. move ignores NULL,
 * empty, or out-of-range state and clamps deltas at list bounds. */
void trainlog_list_init(TrainlogListState *list);
void trainlog_list_set_items(TrainlogListState *list,
                             const char *const *stable_ids,
                             size_t count,
                             size_t visible_rows,
                             bool total_known,
                             bool more_available);
void trainlog_list_move(TrainlogListState *list,
                        const char *const *stable_ids,
                        int delta);

/* CONTRACT: value is borrowed and NULL means empty. output must be non-NULL
 * with nonzero capacity or is untouched; otherwise it is NUL-terminated.
 * maximum_cells <= 0 yields empty output. Invalid UTF-8 is copied bytewise;
 * output stays complete UTF-8 where possible and uses a final ellipsis when
 * truncated by display cells or capacity. */
void trainlog_shell_format_list_label(const char *value,
                                      int maximum_cells,
                                      char *output,
                                      size_t output_capacity);

/* CONTRACT: search/form text is owned inline storage, capped at 200 UTF-8
 * bytes plus NUL. init ignores NULL; insert borrows utf8 for this call and
 * rejects NULL, invalid UTF-8, overflow, and partial codepoints without
 * mutation except capacity_error. Cursor operations ignore NULL and preserve
 * UTF-8 boundaries. Escape clears nonempty text (false) or closes empty input
 * (true). Form init copies/truncates initial input; handle ignores NULL and
 * returns the exact result enum, including F6/F7 shell requests. */
void trainlog_search_init(TrainlogSearchState *search);
bool trainlog_search_insert(TrainlogSearchState *search,
                            const char *utf8,
                            size_t length);
bool trainlog_search_backspace(TrainlogSearchState *search);
void trainlog_search_home(TrainlogSearchState *search);
void trainlog_search_end(TrainlogSearchState *search);
void trainlog_search_left(TrainlogSearchState *search);
void trainlog_search_right(TrainlogSearchState *search);
/* Returns true when Escape closes the empty editor; non-empty Escape clears. */
bool trainlog_search_escape(TrainlogSearchState *search);

void trainlog_form_init(TrainlogFormField *field, const char *initial_value);
TrainlogFormResult trainlog_form_handle(TrainlogFormField *field, int key);

/* CONTRACT: title returns a borrowed static string, including a fallback for
 * invalid routes. section returns a route value without mutation. Leave
 * decision reads state only (NULL allows leaving); explicit discard selects
 * discard confirmation when transient durable state exists. discard ignores
 * NULL and clears only transient in-memory flags: it performs no DB write. */
const char *trainlog_route_title(TrainlogAppRoute route);
TrainlogAppRoute trainlog_route_section(TrainlogAppRoute route);
TrainlogLeaveDecision trainlog_shell_leave_decision(
    const TrainlogDurabilityState *state,
    bool explicit_discard);
void trainlog_shell_discard_transient(TrainlogDurabilityState *state);

#endif
