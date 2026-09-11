#ifndef TRAINLOG_TERMINAL_H
#define TRAINLOG_TERMINAL_H

/**
 * @file terminal.h
 * @brief Small explicit Notcurses terminal boundary for the desktop TUI.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "trainlog/theme.h"

typedef struct TrainlogTerminal TrainlogTerminal;
typedef struct TrainlogPanel TrainlogPanel;
typedef struct TrainlogSurface TrainlogSurface;

typedef enum TrainlogKey {
    TRAINLOG_KEY_NONE = -1,
    TRAINLOG_KEY_UP = -1001,
    TRAINLOG_KEY_DOWN,
    TRAINLOG_KEY_LEFT,
    TRAINLOG_KEY_RIGHT,
    TRAINLOG_KEY_ENTER,
    TRAINLOG_KEY_TAB,
    TRAINLOG_KEY_ESCAPE,
    TRAINLOG_KEY_BACKSPACE,
    TRAINLOG_KEY_DELETE,
    TRAINLOG_KEY_HOME,
    TRAINLOG_KEY_END,
    TRAINLOG_KEY_PAGE_UP,
    TRAINLOG_KEY_PAGE_DOWN,
    TRAINLOG_KEY_RESIZE,
    TRAINLOG_KEY_F1,
    TRAINLOG_KEY_F2,
    TRAINLOG_KEY_F3,
    TRAINLOG_KEY_F4,
    TRAINLOG_KEY_F5,
    TRAINLOG_KEY_F6,
    TRAINLOG_KEY_F7,
    TRAINLOG_KEY_SHIFT_TAB
} TrainlogKey;

/* Trainlog-owned input lifecycle. UNKNOWN is the legacy terminal event form;
 * PRESS and REPEAT are actionable, while RELEASE is never a user action. */
typedef enum TrainlogInputEventType {
    TRAINLOG_INPUT_UNKNOWN = 0,
    TRAINLOG_INPUT_PRESS,
    TRAINLOG_INPUT_REPEAT,
    TRAINLOG_INPUT_RELEASE
} TrainlogInputEventType;

/* WHY: a run owns its terminal so cleanup is deterministic and no application
 * terminal state leaks across a later run in the same process. */
TrainlogTerminal *trainlog_terminal_create(void);
void trainlog_terminal_destroy(TrainlogTerminal *terminal);
int trainlog_terminal_rows(const TrainlogTerminal *terminal);
int trainlog_terminal_columns(const TrainlogTerminal *terminal);
void trainlog_terminal_erase(TrainlogTerminal *terminal);
void trainlog_terminal_render(TrainlogTerminal *terminal);
void trainlog_terminal_style_on(TrainlogTerminal *terminal, TrainlogTextStyle style);
void trainlog_terminal_style_off(TrainlogTerminal *terminal, TrainlogTextStyle style);
void trainlog_terminal_printf(TrainlogTerminal *terminal, int row, int column,
                              const char *format, ...)
    __attribute__((format(printf, 4, 5)));
void trainlog_terminal_putn(TrainlogTerminal *terminal, const char *text, size_t length);
void trainlog_terminal_move(TrainlogTerminal *terminal, int row, int column);
void trainlog_terminal_cursor_yx(const TrainlogTerminal *terminal, int *row, int *column);
void trainlog_terminal_clear_to_end(TrainlogTerminal *terminal);
void trainlog_terminal_cursor_visible(TrainlogTerminal *terminal, bool visible);
void trainlog_terminal_draw(TrainlogTerminal *terminal, int row, int column, uint32_t codepoint);
void trainlog_terminal_box(TrainlogTerminal *terminal, int top, int left,
                           int bottom, int right);
bool trainlog_terminal_translate_input(uint32_t id,
                                       TrainlogInputEventType event_type,
                                       bool shifted,
                                       int *key);
int trainlog_terminal_get_key(TrainlogTerminal *terminal);
bool trainlog_terminal_read_unicode(TrainlogTerminal *terminal, int *codepoint,
                                    char utf8[5]);
bool trainlog_terminal_push_key(TrainlogTerminal *terminal, int key);
/* Refreshes Notcurses geometry after a resize event. */
bool trainlog_terminal_refresh_geometry(TrainlogTerminal *terminal);

/* WHY: shell chrome and overlays have independent Notcurses ownership. The
 * rectangle is validated against the standard plane at create/resize time,
 * and every write is clipped by this adapter rather than parent inheritance. */
TrainlogSurface *trainlog_surface_create(TrainlogTerminal *terminal,
                                         const char *name,
                                         int top, int left,
                                         int height, int width);
bool trainlog_surface_set_rect(TrainlogSurface *surface,
                               int top, int left,
                               int height, int width);
void trainlog_surface_destroy(TrainlogSurface *surface);
void trainlog_surface_erase(TrainlogSurface *surface);
void trainlog_surface_set_role(TrainlogSurface *surface,
                               TrainlogColorRole foreground,
                               unsigned background_rgb,
                               TrainlogTextStyle style);
void trainlog_surface_printf(TrainlogSurface *surface,
                             int row, int column,
                             const char *format, ...)
    __attribute__((format(printf, 4, 5)));
void trainlog_surface_draw(TrainlogSurface *surface,
                           int row, int column, uint32_t codepoint);
void trainlog_surface_move_top(TrainlogSurface *surface);

/* Private screen-port helpers. Panels are lightweight coordinate views over
 * the one standard plane, not independently owned terminal surfaces. */
TrainlogPanel *tui_panel_create(TrainlogTerminal *terminal, int height, int width,
                                int top, int left);
void tui_panel_destroy(TrainlogPanel *panel);
void tui_panel_box(TrainlogPanel *panel);
void tui_panel_style_on(TrainlogPanel *panel, TrainlogTextStyle style);
void tui_panel_style_off(TrainlogPanel *panel, TrainlogTextStyle style);
void tui_panel_print(TrainlogPanel *panel, int row, int column, const char *format, ...)
    __attribute__((format(printf, 4, 5)));
void tui_panel_commit(TrainlogPanel *panel);

#endif
