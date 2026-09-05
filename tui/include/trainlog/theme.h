#ifndef TRAINLOG_THEME_H
#define TRAINLOG_THEME_H

#include <curses.h>

/**
 * @file theme.h
 * @brief Centralized ncurses color roles for the Trainlog TUI.
 */

typedef enum TrainlogColorRole {
    TRAINLOG_COLOR_DEFAULT = 0,
    TRAINLOG_COLOR_ACCENT = 1,
    TRAINLOG_COLOR_SUCCESS = 2,
    TRAINLOG_COLOR_WARNING = 3,
    TRAINLOG_COLOR_ERROR = 4,
    TRAINLOG_COLOR_MUTED = 5,
    TRAINLOG_COLOR_GRAPH = 6
} TrainlogColorRole;

void trainlog_theme_initialize(void);
attr_t trainlog_theme_attribute(TrainlogColorRole role);

#endif
