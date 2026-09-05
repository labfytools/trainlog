#ifndef TRAINLOG_TUI_H
#define TRAINLOG_TUI_H

/**
 * @file tui.h
 * @brief Interactive ncurses entry point.
 */

#include "trainlog/database.h"

int trainlog_tui_run(TrainlogDatabase *database);

#endif
