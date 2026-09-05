#ifndef TRAINLOG_BODYVIZ_H
#define TRAINLOG_BODYVIZ_H

#include "trainlog/status.h"

TrainlogStatus trainlog_body_index100(
    double baseline,
    double value,
    double *output_index
);

TrainlogStatus trainlog_body_percent_change(
    double baseline,
    double value,
    double *output_percent
);

#endif
