#include <stdio.h>
#include <string.h>
#include "trainlog/generation_mtp.h"

int main(int argc, char **argv) {
    char diagnostic[1024] = {0};
    TrainlogStatus status;
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        (void)puts("usage: trainlog-generation-mtp-adapter pull|push EXPECTED_PEER LOCAL_ROOT");
        return 0;
    }
    if (argc != 4) {
        return 64;
    }
    if (strcmp(argv[1], "pull") == 0) {
        status = trainlog_generation_mtp_pull(trainlog_generation_mtp_production_io(),
                                              argv[2],
                                              argv[3],
                                              diagnostic,
                                              sizeof(diagnostic));
    } else if (strcmp(argv[1], "push") == 0) {
        status = trainlog_generation_mtp_push(trainlog_generation_mtp_production_io(),
                                              argv[2],
                                              argv[3],
                                              diagnostic,
                                              sizeof(diagnostic));
    } else {
        return 64;
    }
    if (status != TRAINLOG_STATUS_OK) {
        (void)fprintf(
            stderr, "MTP adapter failed status=%d diagnostic=%s\n", (int)status, diagnostic);
        return 2;
    }
    return 0;
}
