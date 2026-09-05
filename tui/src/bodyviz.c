#include "trainlog/bodyviz.h"

#include <math.h>
#include <stddef.h>

TrainlogStatus trainlog_body_index100(
    double baseline,
    double value,
    double *output_index
)
{
    if (output_index == NULL ||
        !isfinite(baseline) ||
        !isfinite(value) ||
        baseline <= 0.0 ||
        value <= 0.0) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    *output_index = (value / baseline) * 100.0;
    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_body_percent_change(
    double baseline,
    double value,
    double *output_percent
)
{
    double index;
    TrainlogStatus status;

    if (output_percent == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    status = trainlog_body_index100(
        baseline,
        value,
        &index
    );

    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }

    *output_percent = index - 100.0;
    return TRAINLOG_STATUS_OK;
}
