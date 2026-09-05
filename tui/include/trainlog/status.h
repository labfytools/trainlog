#ifndef TRAINLOG_STATUS_H
#define TRAINLOG_STATUS_H

/**
 * @file status.h
 * @brief Common status values returned by Trainlog core APIs.
 */

/**
 * @brief Result codes used by non-trivial Trainlog core operations.
 *
 * Callers must not infer SQLite error numbers from this enum. The persistence
 * layer deliberately translates backend-specific failures into the small,
 * stable status surface needed by the application layer.
 */
typedef enum TrainlogStatus {
    TRAINLOG_STATUS_OK = 0,
    TRAINLOG_STATUS_INVALID_ARGUMENT,
    TRAINLOG_STATUS_SYSTEM_ERROR,
    TRAINLOG_STATUS_DATABASE_ERROR,
    TRAINLOG_STATUS_SCHEMA_UNSUPPORTED,
    TRAINLOG_STATUS_CONFLICT,
    TRAINLOG_STATUS_NOT_FOUND
} TrainlogStatus;

#endif
