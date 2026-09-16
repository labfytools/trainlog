/* Standalone public-header compilation contract. */
#include "trainlog/dashboard.h"

int main(void)
{
    TrainlogDashboardQuery query = {TRAINLOG_DASHBOARD_30_DAYS, 0};
    TrainlogDashboardSnapshot snapshot = {0};
    return query.reference_unix_second == 0 && !snapshot.error ? 0 : 1;
}
