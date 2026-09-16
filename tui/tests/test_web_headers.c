/* Standalone public-header compilation contract for the Web infrastructure. */
#include "trainlog/cli.h"
#include "trainlog/paths.h"
#include "trainlog/web_server.h"

int main(void)
{
    TrainlogCliOptions options = {TRAINLOG_RUN_TUI, 8080U};
    return options.mode == TRAINLOG_RUN_TUI && options.port == 8080U ? 0 : 1;
}
