#ifndef TRAINLOG_THEME_H
#define TRAINLOG_THEME_H

#include <stdint.h>

/**
 * @file theme.h
 * @brief Centralized true-color semantic roles for the Trainlog TUI.
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

/*
 * CONTRACT: styles are terminal-library-independent semantic values.  Screen
 * code never owns a palette index or an ncurses attribute.
 */
typedef uint32_t TrainlogTextStyle;

#define TRAINLOG_TEXT_NORMAL ((TrainlogTextStyle)0U)
#define TRAINLOG_TEXT_BOLD ((TrainlogTextStyle)0x0001U)
#define TRAINLOG_TEXT_REVERSE ((TrainlogTextStyle)0x0002U)

TrainlogTextStyle trainlog_theme_style(TrainlogColorRole role);

#endif
