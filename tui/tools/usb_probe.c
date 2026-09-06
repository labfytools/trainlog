#include <stdio.h>

#include "trainlog/status.h"
#include "trainlog/usb.h"

#define PROBE_CAPACITY 16U

int main(void)
{
    TrainlogUsbDevice
        devices[PROBE_CAPACITY];

    size_t count = 0U;
    size_t index;
    TrainlogStatus status;

    status =
        trainlog_usb_list_mtp_devices(
            devices,
            PROBE_CAPACITY,
            &count
        );

    if (status != TRAINLOG_STATUS_OK) {
        (void)fprintf(
            stderr,
            "MTP discovery failed: %d\n",
            (int)status
        );

        return 1;
    }

    (void)printf(
        "MTP devices: %zu\n",
        count
    );

    for (index = 0U;
         index < count;
         ++index) {
        const TrainlogUsbDevice *device =
            &devices[index];

        (void)printf(
            "[%zu] %s / %s\n",
            index,
            device->vendor[0] != '\0'
                ? device->vendor
                : "(unknown vendor)",
            device->model[0] != '\0'
                ? device->model
                : "(unknown model)"
        );

        (void)printf(
            "    usb=%03u:%03u  vid:pid=%04x:%04x\n",
            device->bus_number,
            device->device_number,
            device->vendor_id,
            device->product_id
        );

        (void)printf(
            "    serial=%s\n",
            device->serial[0] != '\0'
                ? device->serial
                : "(none)"
        );

        (void)printf(
            "    syspath=%s\n",
            device->syspath[0] != '\0'
                ? device->syspath
                : "(none)"
        );
    }

    return 0;
}
