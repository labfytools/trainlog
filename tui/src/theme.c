/**
 * @file theme.c
 * @brief Centralized Trainlog ncurses colors.
 */

#include "trainlog/theme.h"

#include <curses.h>

void trainlog_theme_initialize(void)
{
    if (!has_colors()) {
        return;
    }

    start_color();
    use_default_colors();

    init_pair(TRAINLOG_COLOR_ACCENT, COLOR_CYAN, -1);
    init_pair(TRAINLOG_COLOR_SUCCESS, COLOR_GREEN, -1);
    init_pair(TRAINLOG_COLOR_WARNING, COLOR_YELLOW, -1);
    init_pair(TRAINLOG_COLOR_ERROR, COLOR_RED, -1);
    init_pair(TRAINLOG_COLOR_MUTED, COLOR_BLUE, -1);
    init_pair(TRAINLOG_COLOR_GRAPH, COLOR_MAGENTA, -1);
}

attr_t trainlog_theme_attribute(TrainlogColorRole role)
{
    if (!has_colors() || role == TRAINLOG_COLOR_DEFAULT) {
        return A_NORMAL;
    }

    return COLOR_PAIR((short)role);
}
