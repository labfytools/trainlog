#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trainlog/mtp.h"
#include "trainlog/status.h"
#include "trainlog/usb.h"

#define MAX_DEVICES 8U
#define MAX_STORAGES 8U
#define MAX_ENTRIES 512U

static TrainlogStatus find_child_folder(
    const TrainlogUsbDevice *device,
    uint32_t storage_id,
    uint32_t parent_id,
    const char *name,
    uint32_t *output_id
)
{
    TrainlogMtpEntry
        entries[MAX_ENTRIES];

    size_t count = 0U;
    size_t index;
    TrainlogStatus status;

    if (device == NULL ||
        name == NULL ||
        output_id == NULL) {
        return
            TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    status =
        trainlog_mtp_list_folder(
            device->bus_number,
            device->device_number,
            storage_id,
            parent_id,
            entries,
            MAX_ENTRIES,
            &count
        );

    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }

    for (index = 0U;
         index < count;
         ++index) {
        if (entries[index].folder &&
            strcmp(
                entries[index].name,
                name
            ) == 0) {
            *output_id =
                entries[index].item_id;

            return
                TRAINLOG_STATUS_OK;
        }
    }

    return TRAINLOG_STATUS_NOT_FOUND;
}

static TrainlogStatus find_child_file(
    const TrainlogUsbDevice *device,
    uint32_t storage_id,
    uint32_t parent_id,
    const char *name,
    uint32_t *output_id,
    uint64_t *output_size
)
{
    TrainlogMtpEntry
        entries[MAX_ENTRIES];

    size_t count = 0U;
    size_t index;
    TrainlogStatus status;

    if (device == NULL ||
        name == NULL ||
        output_id == NULL ||
        output_size == NULL) {
        return
            TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    status =
        trainlog_mtp_list_folder(
            device->bus_number,
            device->device_number,
            storage_id,
            parent_id,
            entries,
            MAX_ENTRIES,
            &count
        );

    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }

    for (index = 0U;
         index < count;
         ++index) {
        if (!entries[index].folder &&
            strcmp(
                entries[index].name,
                name
            ) == 0) {
            *output_id =
                entries[index].item_id;

            *output_size =
                entries[index].size_bytes;

            return
                TRAINLOG_STATUS_OK;
        }
    }

    return TRAINLOG_STATUS_NOT_FOUND;
}

static int validate_download(
    const char *path
)
{
    FILE *file;
    char buffer[4096];
    size_t used;

    file =
        fopen(
            path,
            "rb"
        );

    if (file == NULL) {
        return 1;
    }

    used =
        fread(
            buffer,
            1U,
            sizeof(buffer) - 1U,
            file
        );

    if (ferror(file) != 0) {
        (void)fclose(file);
        return 1;
    }

    buffer[used] = '\0';

    if (fclose(file) != 0) {
        return 1;
    }

    if (strstr(
            buffer,
            "\"format\":\"trainlog-mobile-export\""
        ) == NULL ||
        strstr(
            buffer,
            "\"version\":1"
        ) == NULL) {
        return 1;
    }

    return 0;
}

int main(void)
{
    TrainlogUsbDevice
        devices[MAX_DEVICES];

    TrainlogMtpStorage
        storages[MAX_STORAGES];

    size_t device_count = 0U;
    size_t storage_count = 0U;
    uint32_t download_id = 0U;
    uint32_t trainlog_id = 0U;
    uint32_t export_id = 0U;
    uint64_t export_size = 0U;

    const char *local_path =
        "/tmp/trainlog-mobile-export-v1.json";

    TrainlogStatus status;

    status =
        trainlog_usb_list_mtp_devices(
            devices,
            MAX_DEVICES,
            &device_count
        );

    if (status != TRAINLOG_STATUS_OK ||
        device_count == 0U) {
        (void)fprintf(
            stderr,
            "MTP device not found\n"
        );

        return 1;
    }

    status =
        trainlog_mtp_list_storages(
            devices[0].bus_number,
            devices[0].device_number,
            storages,
            MAX_STORAGES,
            &storage_count
        );

    if (status != TRAINLOG_STATUS_OK ||
        storage_count == 0U) {
        (void)fprintf(
            stderr,
            "MTP storage not found\n"
        );

        return 1;
    }

    status =
        find_child_folder(
            &devices[0],
            storages[0].storage_id,
            UINT32_MAX,
            "Download",
            &download_id
        );

    if (status != TRAINLOG_STATUS_OK) {
        (void)fprintf(
            stderr,
            "Download folder not found\n"
        );

        return 1;
    }

    status =
        find_child_folder(
            &devices[0],
            storages[0].storage_id,
            download_id,
            "Trainlog",
            &trainlog_id
        );

    if (status != TRAINLOG_STATUS_OK) {
        (void)fprintf(
            stderr,
            "Download/Trainlog not found\n"
        );

        return 1;
    }

    status =
        find_child_file(
            &devices[0],
            storages[0].storage_id,
            trainlog_id,
            "trainlog-mobile-export-v1.json",
            &export_id,
            &export_size
        );

    if (status != TRAINLOG_STATUS_OK) {
        (void)fprintf(
            stderr,
            "mobile export not found\n"
        );

        return 1;
    }

    status =
        trainlog_mtp_receive_file(
            devices[0].bus_number,
            devices[0].device_number,
            export_id,
            local_path
        );

    if (status != TRAINLOG_STATUS_OK) {
        (void)fprintf(
            stderr,
            "MTP receive failed\n"
        );

        return 1;
    }

    if (validate_download(
            local_path
        ) != 0) {
        (void)fprintf(
            stderr,
            "export validation failed\n"
        );

        return 1;
    }

    (void)printf(
        "MOBILE_EXPORT_MTP=PASS\n"
    );

    (void)printf(
        "device=%s %s\n",
        devices[0].vendor,
        devices[0].model
    );

    (void)printf(
        "remote=Download/Trainlog/"
        "trainlog-mobile-export-v1.json\n"
    );

    (void)printf(
        "size=%llu\n",
        (unsigned long long)
            export_size
    );

    (void)printf(
        "local=%s\n",
        local_path
    );

    return 0;
}
