/**
 * @file mtp_probe.c
 * @brief Manual end-to-end USB -> MTP storage probe.
 */

#include <inttypes.h>
#include <stdio.h>

#include "trainlog/mtp.h"
#include "trainlog/status.h"
#include "trainlog/usb.h"

#define DEVICE_CAPACITY 16U
#define STORAGE_CAPACITY 16U

static double bytes_to_gib(
    uint64_t bytes
)
{
    return
        (double)bytes /
        (1024.0 * 1024.0 * 1024.0);
}

int main(void)
{
    TrainlogUsbDevice
        devices[DEVICE_CAPACITY];

    size_t device_count = 0U;
    size_t device_index;
    TrainlogStatus status;

    status =
        trainlog_usb_list_mtp_devices(
            devices,
            DEVICE_CAPACITY,
            &device_count
        );

    if (status != TRAINLOG_STATUS_OK) {
        (void)fprintf(
            stderr,
            "USB MTP discovery failed: %d\n",
            (int)status
        );

        return 1;
    }

    (void)printf(
        "Physical MTP devices: %zu\n",
        device_count
    );

    for (device_index = 0U;
         device_index < device_count;
         ++device_index) {
        TrainlogMtpStorage
            storages[STORAGE_CAPACITY];

        const TrainlogUsbDevice *usb =
            &devices[device_index];

        size_t storage_count = 0U;
        size_t storage_index;

        (void)printf(
            "\n[%zu] %s / %s\n",
            device_index,
            usb->vendor[0] != '\0'
                ? usb->vendor
                : "(unknown vendor)",
            usb->model[0] != '\0'
                ? usb->model
                : "(unknown model)"
        );

        (void)printf(
            "    usb=%03u:%03u  vid:pid=%04x:%04x\n",
            usb->bus_number,
            usb->device_number,
            usb->vendor_id,
            usb->product_id
        );

        status =
            trainlog_mtp_list_storages(
                usb->bus_number,
                usb->device_number,
                storages,
                STORAGE_CAPACITY,
                &storage_count
            );

        if (status != TRAINLOG_STATUS_OK) {
            (void)printf(
                "    libmtp open/storage failed: %d\n",
                (int)status
            );

            continue;
        }

        (void)printf(
            "    storages=%zu\n",
            storage_count
        );

        for (storage_index = 0U;
             storage_index < storage_count;
             ++storage_index) {
            const TrainlogMtpStorage *storage =
                &storages[storage_index];

            (void)printf(
                "    storage[%zu] id=0x%08" PRIx32 "\n",
                storage_index,
                storage->storage_id
            );

            (void)printf(
                "        description=%s\n",
                storage->description[0] != '\0'
                    ? storage->description
                    : "(none)"
            );

            (void)printf(
                "        volume=%s\n",
                storage->volume_identifier[0] != '\0'
                    ? storage->volume_identifier
                    : "(none)"
            );

            (void)printf(
                "        capacity=%.2f GiB  free=%.2f GiB\n",
                bytes_to_gib(
                    storage->max_capacity_bytes
                ),
                bytes_to_gib(
                    storage->free_space_bytes
                )
            );

            (void)printf(
                "        access=0x%04x  "
                "storage_type=0x%04x  "
                "fs=0x%04x\n",
                (unsigned int)
                    storage->access_capability,
                (unsigned int)
                    storage->storage_type,
                (unsigned int)
                    storage->filesystem_type
            );
        }
    }

    return 0;
}
