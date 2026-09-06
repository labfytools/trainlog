#ifndef TRAINLOG_USB_H
#define TRAINLOG_USB_H

#include <stdbool.h>
#include <stddef.h>

#include "trainlog/status.h"

#define TRAINLOG_USB_VENDOR_MAX 127U
#define TRAINLOG_USB_MODEL_MAX 127U
#define TRAINLOG_USB_SERIAL_MAX 255U
#define TRAINLOG_USB_SYSPATH_MAX 511U

typedef struct TrainlogUsbDevice {
    unsigned int bus_number;
    unsigned int device_number;
    unsigned int vendor_id;
    unsigned int product_id;
    bool mtp;
    char vendor[TRAINLOG_USB_VENDOR_MAX + 1U];
    char model[TRAINLOG_USB_MODEL_MAX + 1U];
    char serial[TRAINLOG_USB_SERIAL_MAX + 1U];
    char syspath[TRAINLOG_USB_SYSPATH_MAX + 1U];
} TrainlogUsbDevice;

TrainlogStatus trainlog_usb_list_mtp_devices(
    TrainlogUsbDevice *output,
    size_t capacity,
    size_t *output_count
);

#endif
