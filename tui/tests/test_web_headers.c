/* Standalone public-header compilation contract for the Web infrastructure. */
#include "trainlog/cli.h"
#include "trainlog/paths.h"
#include "trainlog/web_server.h"
#include "trainlog/web_dashboard.h"

int main(void)
{
    TrainlogCliOptions options = {TRAINLOG_RUN_TUI, 8080U};
    TrainlogWebDashboardQuery query = {0};
    return options.mode == TRAINLOG_RUN_TUI && options.port == 8080U &&
        query.reference_unix_second == 0 ? 0 : 1;
}
