/**
 * @file terminal.c
 * @brief Explicit, bounded Notcurses adapter used only by the desktop TUI.
 */

#include "trainlog/terminal.h"

#include <stdarg.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <notcurses/notcurses.h>

struct TrainlogTerminal {
    struct notcurses *notcurses;
    struct ncplane *plane;
    TrainlogTextStyle style;
    int pushed_key;
};

struct TrainlogPanel {
    TrainlogTerminal *terminal;
    int height;
    int width;
    int top;
    int left;
};

static void terminal_apply_style(TrainlogTerminal *terminal)
{
    unsigned red = 205U;
    unsigned green = 214U;
    unsigned blue = 244U;
    TrainlogColorRole role;

    if (terminal == NULL || terminal->plane == NULL) {
        return;
    }

    role = (TrainlogColorRole)((terminal->style >> 8U) & 0xffU);
    switch (role) {
    case TRAINLOG_COLOR_ACCENT: red = 148U; green = 226U; blue = 213U; break;
    case TRAINLOG_COLOR_SUCCESS: red = 166U; green = 227U; blue = 161U; break;
    case TRAINLOG_COLOR_WARNING: red = 249U; green = 226U; blue = 175U; break;
    case TRAINLOG_COLOR_ERROR: red = 243U; green = 139U; blue = 168U; break;
    case TRAINLOG_COLOR_MUTED: red = 137U; green = 180U; blue = 250U; break;
    case TRAINLOG_COLOR_GRAPH: red = 245U; green = 194U; blue = 231U; break;
    case TRAINLOG_COLOR_DEFAULT:
    default: break;
    }
    (void)ncplane_set_fg_rgb8(terminal->plane, red, green, blue);
    /* CONTRACT: selection remains visible without relying only on foreground
     * color.  A role-aware surface fill survives terminals with weak color
     * contrast while ordinary drawing uses the canonical dark background. */
    if ((terminal->style & TRAINLOG_TEXT_REVERSE) != 0U) {
        (void)ncplane_set_bg_rgb8(terminal->plane, 49U, 50U, 68U);
    } else {
        (void)ncplane_set_bg_rgb8(terminal->plane, 30U, 30U, 46U);
    }
    ncplane_set_styles(terminal->plane,
                       (terminal->style & TRAINLOG_TEXT_BOLD) != 0U ? NCSTYLE_BOLD : 0U);
}

TrainlogTerminal *trainlog_terminal_create(void)
{
    notcurses_options options = {0};
    TrainlogTerminal *terminal = calloc(1U, sizeof(*terminal));

    if (terminal == NULL) {
        return NULL;
    }
    options.flags = NCOPTION_SUPPRESS_BANNERS;
    terminal->notcurses = notcurses_core_init(&options, NULL);
    if (terminal->notcurses == NULL) {
        free(terminal);
        return NULL;
    }
    terminal->plane = notcurses_stdplane(terminal->notcurses);
    if (terminal->plane == NULL) {
        (void)notcurses_stop(terminal->notcurses);
        free(terminal);
        return NULL;
    }
    (void)ncplane_set_bg_rgb8(terminal->plane, 30U, 30U, 46U);
    terminal->pushed_key = TRAINLOG_KEY_NONE;
    terminal_apply_style(terminal);
    return terminal;
}

void trainlog_terminal_destroy(TrainlogTerminal *terminal)
{
    if (terminal == NULL) {
        return;
    }
    if (terminal->notcurses != NULL) {
        (void)notcurses_stop(terminal->notcurses);
    }
    free(terminal);
}

int trainlog_terminal_rows(const TrainlogTerminal *terminal)
{
    unsigned rows = 0U;
    if (terminal != NULL && terminal->plane != NULL) {
        ncplane_dim_yx(terminal->plane, &rows, NULL);
    }
    return rows <= (unsigned)INT_MAX ? (int)rows : 0;
}

int trainlog_terminal_columns(const TrainlogTerminal *terminal)
{
    unsigned columns = 0U;
    if (terminal != NULL && terminal->plane != NULL) {
        ncplane_dim_yx(terminal->plane, NULL, &columns);
    }
    return columns <= (unsigned)INT_MAX ? (int)columns : 0;
}

void trainlog_terminal_erase(TrainlogTerminal *terminal)
{
    if (terminal != NULL && terminal->plane != NULL) {
        ncplane_erase(terminal->plane);
        terminal_apply_style(terminal);
    }
}

void trainlog_terminal_render(TrainlogTerminal *terminal)
{
    if (terminal != NULL && terminal->notcurses != NULL) {
        (void)notcurses_render(terminal->notcurses);
    }
}

void trainlog_terminal_style_on(TrainlogTerminal *terminal, TrainlogTextStyle style)
{
    if (terminal != NULL) {
        terminal->style |= style;
        terminal_apply_style(terminal);
    }
}

void trainlog_terminal_style_off(TrainlogTerminal *terminal, TrainlogTextStyle style)
{
    if (terminal != NULL) {
        terminal->style &= ~style;
        terminal_apply_style(terminal);
    }
}

void trainlog_terminal_printf(TrainlogTerminal *terminal, int row, int column,
                              const char *format, ...)
{
    va_list arguments;
    va_list copy;
    int count;
    char *text;

    if (terminal == NULL || terminal->plane == NULL || format == NULL ||
        row < 0 || column < 0 || row >= trainlog_terminal_rows(terminal) ||
        column >= trainlog_terminal_columns(terminal)) {
        return;
    }
    va_start(arguments, format);
    va_copy(copy, arguments);
    count = vsnprintf(NULL, 0U, format, copy);
    va_end(copy);
    if (count < 0 || (size_t)count > SIZE_MAX - 1U) {
        va_end(arguments);
        return;
    }
    text = malloc((size_t)count + 1U);
    if (text != NULL) {
        (void)vsnprintf(text, (size_t)count + 1U, format, arguments);
        (void)ncplane_putstr_yx(terminal->plane, row, column, text);
        free(text);
    }
    va_end(arguments);
}

void trainlog_terminal_putn(TrainlogTerminal *terminal, const char *text, size_t length)
{
    if (terminal == NULL || terminal->plane == NULL || text == NULL || length > (size_t)INT_MAX) {
        return;
    }
    (void)ncplane_putnstr(terminal->plane, length, text);
}

void trainlog_terminal_move(TrainlogTerminal *terminal, int row, int column)
{
    if (terminal != NULL && terminal->plane != NULL && row >= 0 && column >= 0) {
        (void)ncplane_cursor_move_yx(terminal->plane, row, column);
    }
}

void trainlog_terminal_cursor_yx(const TrainlogTerminal *terminal, int *row, int *column)
{
    unsigned y = 0U;
    unsigned x = 0U;
    if (terminal != NULL && terminal->plane != NULL) {
        ncplane_cursor_yx(terminal->plane, &y, &x);
    }
    if (row != NULL) { *row = y <= (unsigned)INT_MAX ? (int)y : 0; }
    if (column != NULL) { *column = x <= (unsigned)INT_MAX ? (int)x : 0; }
}

void trainlog_terminal_clear_to_end(TrainlogTerminal *terminal)
{
    if (terminal != NULL && terminal->plane != NULL) {
        (void)ncplane_erase_region(terminal->plane, -1, -1, 0, INT_MAX);
    }
}

void trainlog_terminal_cursor_visible(TrainlogTerminal *terminal, bool visible)
{
    if (terminal == NULL || terminal->notcurses == NULL) { return; }
    if (visible) {
        (void)notcurses_cursor_enable(terminal->notcurses, -1, -1);
    } else {
        (void)notcurses_cursor_disable(terminal->notcurses);
    }
}

void trainlog_terminal_draw(TrainlogTerminal *terminal, int row, int column, uint32_t codepoint)
{
    if (terminal != NULL && terminal->plane != NULL && row >= 0 && column >= 0 &&
        row < trainlog_terminal_rows(terminal) && column < trainlog_terminal_columns(terminal)) {
        nccell cell = NCCELL_TRIVIAL_INITIALIZER;
        /* Notcurses returns the UTF-8 byte count here (not zero) on success.
         * Keeping every non-negative result is essential for Unicode frames. */
        if (nccell_load_ucs32(terminal->plane, &cell, codepoint) >= 0) {
            (void)ncplane_putc_yx(terminal->plane, row, column, &cell);
            nccell_release(terminal->plane, &cell);
        }
    }
}

void trainlog_terminal_box(TrainlogTerminal *terminal, int top, int left, int bottom, int right)
{
    int column;
    int row;
    if (terminal == NULL || top < 0 || left < 0 || bottom <= top || right <= left) { return; }
    for (column = left + 1; column < right; ++column) {
        trainlog_terminal_draw(terminal, top, column, 0x2500U);
        trainlog_terminal_draw(terminal, bottom, column, 0x2500U);
    }
    for (row = top + 1; row < bottom; ++row) {
        trainlog_terminal_draw(terminal, row, left, 0x2502U);
        trainlog_terminal_draw(terminal, row, right, 0x2502U);
    }
    trainlog_terminal_draw(terminal, top, left, 0x250cU);
    trainlog_terminal_draw(terminal, top, right, 0x2510U);
    trainlog_terminal_draw(terminal, bottom, left, 0x2514U);
    trainlog_terminal_draw(terminal, bottom, right, 0x2518U);
}

static int terminal_key(uint32_t id, bool shifted)
{
    if (id == NCKEY_TAB && shifted) {
        return TRAINLOG_KEY_SHIFT_TAB;
    }

    switch (id) {
    case NCKEY_UP: return TRAINLOG_KEY_UP; case NCKEY_DOWN: return TRAINLOG_KEY_DOWN;
    case NCKEY_LEFT: return TRAINLOG_KEY_LEFT; case NCKEY_RIGHT: return TRAINLOG_KEY_RIGHT;
    case NCKEY_ENTER: return TRAINLOG_KEY_ENTER; case NCKEY_TAB: return TRAINLOG_KEY_TAB;
    case NCKEY_BACKSPACE: return TRAINLOG_KEY_BACKSPACE;
    case NCKEY_DEL: return TRAINLOG_KEY_DELETE; case NCKEY_HOME: return TRAINLOG_KEY_HOME;
    case NCKEY_END: return TRAINLOG_KEY_END; case NCKEY_PGUP: return TRAINLOG_KEY_PAGE_UP;
    case NCKEY_PGDOWN: return TRAINLOG_KEY_PAGE_DOWN; case NCKEY_RESIZE: return TRAINLOG_KEY_RESIZE;
    case NCKEY_F01: return TRAINLOG_KEY_F1; case NCKEY_F02: return TRAINLOG_KEY_F2;
    case NCKEY_F03: return TRAINLOG_KEY_F3; case NCKEY_F04: return TRAINLOG_KEY_F4;
    case NCKEY_F05: return TRAINLOG_KEY_F5; default: return (int)id;
    }
}

bool trainlog_terminal_translate_input(uint32_t id,
                                       TrainlogInputEventType event_type,
                                       bool shifted,
                                       int *key)
{
    if (key == NULL || id == 0U || id == UINT32_MAX) {
        return false;
    }

    switch (event_type) {
    case TRAINLOG_INPUT_UNKNOWN:
    case TRAINLOG_INPUT_PRESS:
    case TRAINLOG_INPUT_REPEAT:
        *key = terminal_key(id, shifted);
        return true;
    case TRAINLOG_INPUT_RELEASE:
    default:
        return false;
    }
}

static TrainlogInputEventType terminal_event_type(ncintype_e event_type)
{
    switch (event_type) {
    case NCTYPE_UNKNOWN: return TRAINLOG_INPUT_UNKNOWN;
    case NCTYPE_PRESS: return TRAINLOG_INPUT_PRESS;
    case NCTYPE_REPEAT: return TRAINLOG_INPUT_REPEAT;
    case NCTYPE_RELEASE: return TRAINLOG_INPUT_RELEASE;
    default: return TRAINLOG_INPUT_RELEASE;
    }
}

static bool terminal_read_input(TrainlogTerminal *terminal,
                                int *key,
                                char utf8[5])
{
    ncinput input;

    if (terminal == NULL || terminal->notcurses == NULL || key == NULL) {
        return false;
    }

    for (;;) {
        uint32_t id = notcurses_get_blocking(terminal->notcurses, &input);

        if (id == 0U || id == UINT32_MAX) {
            return false;
        }
        if (!trainlog_terminal_translate_input(id,
                                               terminal_event_type(input.evtype),
                                               ncinput_shift_p(&input),
                                               key)) {
            continue;
        }
        if (utf8 != NULL) {
            (void)snprintf(utf8, 5U, "%s", input.utf8);
        }
        return true;
    }
}

int trainlog_terminal_get_key(TrainlogTerminal *terminal)
{
    int key;
    if (terminal == NULL || terminal->notcurses == NULL) { return TRAINLOG_KEY_NONE; }
    if (terminal->pushed_key != TRAINLOG_KEY_NONE) {
        int pushed_key = terminal->pushed_key;
        terminal->pushed_key = TRAINLOG_KEY_NONE;
        return pushed_key;
    }
    return terminal_read_input(terminal, &key, NULL) ? key : TRAINLOG_KEY_NONE;
}

bool trainlog_terminal_read_unicode(TrainlogTerminal *terminal, int *codepoint, char utf8[5])
{
    if (terminal == NULL || terminal->notcurses == NULL || codepoint == NULL || utf8 == NULL) { return false; }
    return terminal_read_input(terminal, codepoint, utf8);
}

bool trainlog_terminal_push_key(TrainlogTerminal *terminal, int key)
{
    if (terminal == NULL || terminal->pushed_key != TRAINLOG_KEY_NONE) {
        return false;
    }
    terminal->pushed_key = key;
    return true;
}

TrainlogPanel *tui_panel_create(TrainlogTerminal *terminal, int height, int width,
                                int top, int left)
{
    TrainlogPanel *panel;
    if (terminal == NULL || height < 2 || width < 2 || top < 0 || left < 0 ||
        top > trainlog_terminal_rows(terminal) - height ||
        left > trainlog_terminal_columns(terminal) - width) {
        return NULL;
    }
    panel = malloc(sizeof(*panel));
    if (panel != NULL) {
        *panel = (TrainlogPanel){ terminal, height, width, top, left };
    }
    return panel;
}

void tui_panel_destroy(TrainlogPanel *panel) { free(panel); }

void tui_panel_box(TrainlogPanel *panel)
{
    if (panel != NULL) {
        trainlog_terminal_box(panel->terminal, panel->top, panel->left,
                              panel->top + panel->height - 1,
                              panel->left + panel->width - 1);
    }
}

void tui_panel_style_on(TrainlogPanel *panel, TrainlogTextStyle style)
{ if (panel != NULL) { trainlog_terminal_style_on(panel->terminal, style); } }

void tui_panel_style_off(TrainlogPanel *panel, TrainlogTextStyle style)
{ if (panel != NULL) { trainlog_terminal_style_off(panel->terminal, style); } }

void tui_panel_print(TrainlogPanel *panel, int row, int column, const char *format, ...)
{
    va_list arguments;
    va_list copy;
    int count;
    char *text;
    if (panel == NULL || format == NULL || row < 0 || column < 0 || row >= panel->height || column >= panel->width) { return; }
    va_start(arguments, format);
    va_copy(copy, arguments);
    count = vsnprintf(NULL, 0U, format, copy);
    va_end(copy);
    if (count < 0) { va_end(arguments); return; }
    text = malloc((size_t)count + 1U);
    if (text != NULL) {
        (void)vsnprintf(text, (size_t)count + 1U, format, arguments);
        trainlog_terminal_printf(panel->terminal, panel->top + row, panel->left + column, "%s", text);
        free(text);
    }
    va_end(arguments);
}

void tui_panel_commit(TrainlogPanel *panel) { (void)panel; }
