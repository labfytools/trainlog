#ifndef TRAINLOG_PATHS_H
#define TRAINLOG_PATHS_H

#include <stddef.h>

/* Resolve and create the private XDG data directory, then return trainlog.db.
 * The caller owns output; truncation and non-directory collisions fail. */
int trainlog_paths_database(char *output, size_t output_capacity,
    char *diagnostic, size_t diagnostic_capacity);

#endif
