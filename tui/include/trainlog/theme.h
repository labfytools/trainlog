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
    TRAINLOG_COLOR_GRAPH = 6,
    TRAINLOG_COLOR_INFO = 7,
    TRAINLOG_COLOR_NOTICE = 8
} TrainlogColorRole;

/*
 * CONTRACT: styles are terminal-library-independent semantic values.  Screen
 * code never owns a palette index or an ncurses attribute.
 */
typedef uint32_t TrainlogTextStyle;

#define TRAINLOG_TEXT_NORMAL ((TrainlogTextStyle)0U)
#define TRAINLOG_TEXT_BOLD ((TrainlogTextStyle)0x0001U)
#define TRAINLOG_TEXT_REVERSE ((TrainlogTextStyle)0x0002U)

/* APP_SHELL_V1 platform tokens. Values are RGB, independent of Notcurses. */
#define TRAINLOG_RGB_CRUST 0x11111bU
#define TRAINLOG_RGB_MANTLE 0x181825U
#define TRAINLOG_RGB_BASE 0x1e1e2eU
#define TRAINLOG_RGB_SURFACE0 0x313244U
#define TRAINLOG_RGB_SURFACE1 0x45475aU
#define TRAINLOG_RGB_TEXT 0xcdd6f4U
#define TRAINLOG_RGB_SUBTEXT 0xbac2deU
#define TRAINLOG_RGB_LAVENDER 0xb4befeU
#define TRAINLOG_RGB_SUCCESS 0xa6e3a1U
#define TRAINLOG_RGB_WARNING 0xf9e2afU
#define TRAINLOG_RGB_ERROR 0xf38ba8U
#define TRAINLOG_RGB_INFO 0x89b4faU
#define TRAINLOG_RGB_NOTICE 0xfab387U

TrainlogTextStyle trainlog_theme_style(TrainlogColorRole role);

#endif
