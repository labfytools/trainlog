#ifndef TRAINLOG_EXERCISE_NAME_CATALOG_H
#define TRAINLOG_EXERCISE_NAME_CATALOG_H

/**
 * @file exercise_name_catalog.h
 * @brief Shared presentation names keyed by stable exercise identity.
 */

/**
 * Return the current shared display name for a known stable exercise ID.
 *
 * The returned pointer has static storage duration. Unknown IDs return NULL so
 * callers can preserve the user-owned database label for custom exercises.
 */
const char *trainlog_exercise_name_catalog_lookup(const char *exercise_id);

#endif
