#ifndef TRAINLOG_DATABASE_INTERNAL_H
#define TRAINLOG_DATABASE_INTERNAL_H

/* Source-private SQLite handle layout. Public callers retain the opaque
 * TrainlogDatabase declaration from database.h; STATS uses this only for its
 * file-local read-model query and exports no new database symbol or ABI. */
#include <sqlite3.h>

struct TrainlogDatabase {
    sqlite3 *connection;
    unsigned int read_snapshot_depth;
};

#endif
