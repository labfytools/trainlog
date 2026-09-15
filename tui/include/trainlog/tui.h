/*
 * Trainlog tui interface.
 *
 * Declares the module boundary and ownership contract; implementation and persistence remain in their owning modules.
 */
#ifndef TRAINLOG_TUI_H
#define TRAINLOG_TUI_H

/**
 * @file tui.h
 * @brief Interactive Notcurses entry point.
 */

#include "trainlog/database.h"

int trainlog_tui_run(TrainlogDatabase *database);

#endif
