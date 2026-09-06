#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "trainlog/mtp.h"
#include "trainlog/status.h"
#include "trainlog/usb.h"

#define DEVICE_CAPACITY 16U
#define STORAGE_CAPACITY 16U

static int create_probe_file(
    char path[64]
)
{
    static const char content[] =
        "Trainlog MTP write probe\n"
        "If you can read this file on Android, USB/MTP write works.\n";

    int fd;
    ssize_t written;

    (void)snprintf(
        path,
        64U,
        "%s",
        "/tmp/trainlog-mtp-probe-XXXXXX"
    );

    fd = mkstemp(path);

    if (fd < 0) {
        return -1;
    }

    written =
        write(
            fd,
            content,
            sizeof(content) - 1U
        );

    if (written !=
        (ssize_t)(sizeof(content) - 1U)) {
        (void)close(fd);
        (void)unlink(path);
        return -1;
    }

    if (close(fd) != 0) {
        (void)unlink(path);
        return -1;
    }

    return 0;
}

int main(void)
{
    TrainlogUsbDevice
        devices[DEVICE_CAPACITY];

    TrainlogMtpStorage
        storages[STORAGE_CAPACITY];

    size_t device_count = 0U;
    size_t storage_count = 0U;
    uint32_t folder_id = 0U;
    uint32_t item_id = 0U;
    bool created = false;
    char local_path[64];
    TrainlogStatus status;

    status =
        trainlog_usb_list_mtp_devices(
            devices,
            DEVICE_CAPACITY,
            &device_count
        );

    if (status != TRAINLOG_STATUS_OK ||
        device_count == 0U) {
        (void)fprintf(
            stderr,
            "No physical MTP device available.\n"
        );
        return 1;
    }

    status =
        trainlog_mtp_list_storages(
            devices[0].bus_number,
            devices[0].device_number,
            storages,
            STORAGE_CAPACITY,
            &storage_count
        );

    if (status != TRAINLOG_STATUS_OK ||
        storage_count == 0U) {
        (void)fprintf(
            stderr,
            "No MTP storage available.\n"
        );
        return 1;
    }

    (void)printf(
        "Using %s / %s\n",
        devices[0].vendor,
        devices[0].model
    );

    (void)printf(
        "Storage: %s (0x%08x)\n",
        storages[0].description,
        storages[0].storage_id
    );

    status =
        trainlog_mtp_ensure_root_folder(
            devices[0].bus_number,
            devices[0].device_number,
            storages[0].storage_id,
            "Trainlog",
            &folder_id,
            &created
        );

    if (status != TRAINLOG_STATUS_OK) {
        (void)fprintf(
            stderr,
            "Unable to ensure Trainlog folder: %d\n",
            (int)status
        );
        return 1;
    }

    (void)printf(
        "Trainlog folder id=0x%08x (%s)\n",
        folder_id,
        created
            ? "created"
            : "already present"
    );

    if (create_probe_file(local_path) != 0) {
        (void)fprintf(
            stderr,
            "Unable to create local probe file.\n"
        );
        return 1;
    }

    status =
        trainlog_mtp_send_text_file(
            devices[0].bus_number,
            devices[0].device_number,
            storages[0].storage_id,
            folder_id,
            local_path,
            "trainlog-probe.txt",
            &item_id
        );

    (void)unlink(local_path);

    if (status != TRAINLOG_STATUS_OK) {
        (void)fprintf(
            stderr,
            "Unable to send probe file: %d\n",
            (int)status
        );
        return 1;
    }

    (void)printf(
        "Uploaded Trainlog/trainlog-probe.txt "
        "item_id=0x%08x\n",
        item_id
    );

    return 0;
}
