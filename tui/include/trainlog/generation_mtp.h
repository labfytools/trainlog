#ifndef TRAINLOG_GENERATION_MTP_H
#define TRAINLOG_GENERATION_MTP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "trainlog/mtp.h"
#include "trainlog/status.h"
#include "trainlog/usb.h"

#define TRAINLOG_GENERATION_MTP_MAX_DEVICES 8U
#define TRAINLOG_GENERATION_MTP_MAX_STORAGES 8U
#define TRAINLOG_GENERATION_MTP_MAX_CHILDREN 512U
#define TRAINLOG_GENERATION_MTP_MAX_DEPTH 5U
#define TRAINLOG_GENERATION_MTP_MAX_TOTAL_BYTES (512ULL * 1024ULL * 1024ULL)

typedef struct TrainlogGenerationMtpIo {
    TrainlogStatus (*devices)(TrainlogUsbDevice *, size_t, size_t *);
    TrainlogStatus (*storages)(unsigned int, unsigned int, TrainlogMtpStorage *, size_t, size_t *);
    TrainlogStatus (*children)(unsigned int, unsigned int, uint32_t, uint32_t, TrainlogMtpEntry *, size_t, size_t *);
    TrainlogStatus (*ensure_folder)(unsigned int, unsigned int, uint32_t, uint32_t, const char *, uint32_t *, bool *);
    TrainlogStatus (*receive)(unsigned int, unsigned int, uint32_t, const char *);
    TrainlogStatus (*send)(unsigned int, unsigned int, uint32_t, uint32_t, const char *, const char *, uint32_t *);
    TrainlogStatus (*remove)(unsigned int, unsigned int, uint32_t);
    TrainlogStatus (*rename)(unsigned int, unsigned int, uint32_t, const char *);
} TrainlogGenerationMtpIo;

/** Mirror remote generation-protocol objects to or from a private local root. */
TrainlogStatus trainlog_generation_mtp_pull(const TrainlogGenerationMtpIo *io,
    const char *expected_peer_id, const char *local_root, char *diagnostic, size_t diagnostic_size);
TrainlogStatus trainlog_generation_mtp_push(const TrainlogGenerationMtpIo *io,
    const char *expected_peer_id, const char *local_root, char *diagnostic, size_t diagnostic_size);
const TrainlogGenerationMtpIo *trainlog_generation_mtp_production_io(void);

#endif
