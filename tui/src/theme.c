/**
 * @file theme.c
 * @brief Centralized Trainlog semantic text styles.
 */

#include "trainlog/theme.h"

TrainlogTextStyle trainlog_theme_style(TrainlogColorRole role)
{
    return ((TrainlogTextStyle)role << 8U);
}
