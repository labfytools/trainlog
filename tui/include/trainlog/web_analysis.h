/* Trainlog Web Analyse V1 bounded Core read model. */
#ifndef TRAINLOG_WEB_ANALYSIS_H
#define TRAINLOG_WEB_ANALYSIS_H

#include <stddef.h>
#include <stdint.h>

#include "trainlog/database.h"
#include "trainlog/status.h"

#define TRAINLOG_WEB_ANALYSIS_JSON_CAPACITY (256U * 1024U)
#define TRAINLOG_WEB_ANALYSIS_EXERCISES_MAX 128U
#define TRAINLOG_WEB_ANALYSIS_POINTS_MAX 90U

typedef enum TrainlogWebAnalysisPeriod {
    TRAINLOG_WEB_ANALYSIS_7_DAYS = 7,
    TRAINLOG_WEB_ANALYSIS_14_DAYS = 14,
    TRAINLOG_WEB_ANALYSIS_21_DAYS = 21,
    TRAINLOG_WEB_ANALYSIS_30_DAYS = 30,
    TRAINLOG_WEB_ANALYSIS_90_DAYS = 90,
    TRAINLOG_WEB_ANALYSIS_ALL = 0
} TrainlogWebAnalysisPeriod;

typedef struct TrainlogWebAnalysisQuery {
    TrainlogWebAnalysisPeriod period;
    int64_t reference_unix_second;
    const char *exercise_id;
    const char *measurement_metric;
} TrainlogWebAnalysisQuery;

/* WHY: Dashboard and Analyse must not independently reinterpret history.
 * CONTRACT: the Core filters by the inclusive UTC interval and returns one
 * bounded, typed JSON projection; absent observations are JSON null, never
 * fabricated zeroes. exercise_id and metric are borrowed for this call.
 * INVARIANT: MAX values come only from max_results and BODY ZONE counts are
 * recorded primary-zone exposures, not physiological percentages. */
TrainlogStatus trainlog_web_analysis_json(TrainlogDatabase *database,
                                          const TrainlogWebAnalysisQuery *query,
                                          char *output,
                                          size_t capacity,
                                          size_t *output_size);

#endif
