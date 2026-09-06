#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "trainlog/mtp.h"
#include "trainlog/status.h"
#include "trainlog/usb.h"

#define DEVICE_CAPACITY 16U
#define STORAGE_CAPACITY 16U
#define ENTRY_CAPACITY 128U

static const char EXPECTED_CONTENT[] =
    "Trainlog MTP write probe\n"
    "If you can read this file on Android, USB/MTP write works.\n";

static int verify_file(
    const char *path
)
{
    FILE *stream;
    char buffer[256];
    size_t read_size;

    stream =
        fopen(
            path,
            "rb"
        );

    if (stream == NULL) {
        return -1;
    }

    read_size =
        fread(
            buffer,
            1U,
            sizeof(buffer),
            stream
        );

    if (ferror(stream) != 0) {
        (void)fclose(stream);
        return -1;
    }

    if (fclose(stream) != 0) {
        return -1;
    }

    if (read_size !=
        sizeof(EXPECTED_CONTENT) - 1U) {
        return -1;
    }

    if (memcmp(
            buffer,
            EXPECTED_CONTENT,
            read_size
        ) != 0) {
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

    TrainlogMtpEntry
        entries[ENTRY_CAPACITY];

    size_t device_count = 0U;
    size_t storage_count = 0U;
    size_t entry_count = 0U;
    size_t index;
    uint32_t folder_id = 0U;
    uint32_t probe_item_id = 0U;
    bool created = false;
    char local_path[] =
        "/tmp/trainlog-mtp-readback-XXXXXX";
    int fd;
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
            "Unable to locate Trainlog folder: %d\n",
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

    status =
        trainlog_mtp_list_folder(
            devices[0].bus_number,
            devices[0].device_number,
            storages[0].storage_id,
            folder_id,
            entries,
            ENTRY_CAPACITY,
            &entry_count
        );

    if (status != TRAINLOG_STATUS_OK) {
        (void)fprintf(
            stderr,
            "Unable to list Trainlog folder: %d\n",
            (int)status
        );
        return 1;
    }

    (void)printf(
        "Trainlog entries: %zu\n",
        entry_count
    );

    for (index = 0U;
         index < entry_count;
         ++index) {
        (void)printf(
            "  %s  id=0x%08x  %s  size=%llu\n",
            entries[index].folder
                ? "[DIR] "
                : "[FILE]",
            entries[index].item_id,
            entries[index].name,
            (unsigned long long)
                entries[index].size_bytes
        );

        if (!entries[index].folder &&
            strcmp(
                entries[index].name,
                "trainlog-probe.txt"
            ) == 0) {
            probe_item_id =
                entries[index].item_id;
        }
    }

    if (probe_item_id == 0U) {
        (void)fprintf(
            stderr,
            "trainlog-probe.txt not found.\n"
        );
        return 1;
    }

    fd =
        mkstemp(
            local_path
        );

    if (fd < 0) {
        (void)fprintf(
            stderr,
            "Unable to create local readback file.\n"
        );
        return 1;
    }

    if (close(fd) != 0) {
        (void)unlink(local_path);
        return 1;
    }

    status =
        trainlog_mtp_receive_file(
            devices[0].bus_number,
            devices[0].device_number,
            probe_item_id,
            local_path
        );

    if (status != TRAINLOG_STATUS_OK) {
        (void)unlink(local_path);

        (void)fprintf(
            stderr,
            "Unable to download probe file: %d\n",
            (int)status
        );

        return 1;
    }

    if (verify_file(
            local_path
        ) != 0) {
        (void)unlink(local_path);

        (void)fprintf(
            stderr,
            "Readback content mismatch.\n"
        );

        return 1;
    }

    (void)unlink(
        local_path
    );

    (void)printf(
        "ROUNDTRIP=PASS "
        "Trainlog/trainlog-probe.txt\n"
    );

    return 0;
}
