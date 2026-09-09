#ifndef TRAINLOG_TRAINING_CONTEXT_H
#define TRAINLOG_TRAINING_CONTEXT_H

/** @file training_context.h Read-only runtime/scientific exercise composition. */

#include "trainlog/database.h"
#include "trainlog/training_knowledge.h"

typedef struct TrainlogTrainingOccurrenceView {
    TrainlogExerciseOccurrence occurrence;
    size_t set_offset;
    size_t set_count;
    bool sets_have_more;
    int next_set_position;
} TrainlogTrainingOccurrenceView;

typedef struct TrainlogTrainingExerciseContext {
    TrainlogExercise exercise;
    TrainlogExerciseBodyZone *persisted_zones;
    size_t persisted_zone_count;
    const TrainlogExerciseKnowledge *knowledge; /* borrowed process-lifetime data */
    const TrainlogEquipmentKnowledge **compatible_equipment; /* owned pointer array */
    size_t compatible_equipment_count;
    TrainlogLatestExplicitMax latest_max;
    TrainlogTrainingOccurrenceView *occurrences;
    size_t occurrence_count;
    bool occurrences_have_more;
    TrainlogExerciseOccurrenceCursor next_occurrence;
    TrainlogOccurrenceSet *sets;
    size_t set_count;
} TrainlogTrainingExerciseContext;

/**
 * Compose one exact runtime ID. occurrence_limit is 1..32 and set_preview_limit
 * is 1..64 per occurrence. The caller owns output allocations and must call
 * release after success. Unknown runtime IDs return NOT_FOUND; missing knowledge
 * is valid and represented by NULL. One nested-safe database read snapshot
 * covers the call. No catalog relationship fabricates occurrence history.
 */
TrainlogStatus trainlog_training_exercise_context_load(
    TrainlogDatabase *database,
    const char *exercise_id,
    size_t occurrence_limit,
    size_t set_preview_limit,
    TrainlogTrainingExerciseContext *output
);

void trainlog_training_exercise_context_release(TrainlogTrainingExerciseContext *context);

#endif
