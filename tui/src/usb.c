#include "trainlog/usb.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libudev.h>

static const char *usb_property_prefer(
    struct udev_device *device,
    const char *preferred,
    const char *fallback
)
{
    const char *value =
        udev_device_get_property_value(
            device,
            preferred
        );

    if (value != NULL &&
        value[0] != '\0') {
        return value;
    }

    return
        udev_device_get_property_value(
            device,
            fallback
        );
}

static void usb_copy_text(
    char *output,
    size_t capacity,
    const char *value
)
{
    if (output == NULL ||
        capacity == 0U) {
        return;
    }

    if (value == NULL) {
        output[0] = '\0';
        return;
    }

    (void)snprintf(
        output,
        capacity,
        "%s",
        value
    );
}

static bool usb_parse_unsigned(
    const char *text,
    int base,
    unsigned int *output
)
{
    char *end = NULL;
    unsigned long value;

    if (text == NULL ||
        output == NULL ||
        text[0] == '\0') {
        return false;
    }

    errno = 0;

    value =
        strtoul(
            text,
            &end,
            base
        );

    if (errno != 0 ||
        end == text ||
        *end != '\0' ||
        value > (unsigned long)UINT_MAX) {
        return false;
    }

    *output =
        (unsigned int)value;

    return true;
}

static bool usb_device_is_physical_mtp(
    struct udev_device *device
)
{
    const char *devtype;
    const char *mtp;

    if (device == NULL) {
        return false;
    }

    devtype =
        udev_device_get_devtype(
            device
        );

    if (devtype == NULL ||
        strcmp(
            devtype,
            "usb_device"
        ) != 0) {
        return false;
    }

    mtp =
        udev_device_get_property_value(
            device,
            "ID_MTP_DEVICE"
        );

    return
        mtp != NULL &&
        strcmp(mtp, "1") == 0;
}

static void usb_fill_device(
    struct udev_device *device,
    TrainlogUsbDevice *output
)
{
    const char *vendor;
    const char *model;
    const char *serial;
    const char *vendor_id;
    const char *product_id;
    const char *bus;
    const char *devnum;
    const char *syspath;

    (void)memset(
        output,
        0,
        sizeof(*output)
    );

    output->mtp = true;

    vendor =
        usb_property_prefer(
            device,
            "ID_VENDOR_FROM_DATABASE",
            "ID_VENDOR"
        );

    model =
        usb_property_prefer(
            device,
            "ID_MODEL_FROM_DATABASE",
            "ID_MODEL"
        );

    serial =
        udev_device_get_property_value(
            device,
            "ID_SERIAL_SHORT"
        );

    vendor_id =
        udev_device_get_property_value(
            device,
            "ID_VENDOR_ID"
        );

    product_id =
        udev_device_get_property_value(
            device,
            "ID_MODEL_ID"
        );

    bus =
        udev_device_get_property_value(
            device,
            "BUSNUM"
        );

    devnum =
        udev_device_get_property_value(
            device,
            "DEVNUM"
        );

    if (bus == NULL) {
        bus =
            udev_device_get_sysattr_value(
                device,
                "busnum"
            );
    }

    if (devnum == NULL) {
        devnum =
            udev_device_get_sysattr_value(
                device,
                "devnum"
            );
    }

    syspath =
        udev_device_get_syspath(
            device
        );

    usb_copy_text(
        output->vendor,
        sizeof(output->vendor),
        vendor
    );

    usb_copy_text(
        output->model,
        sizeof(output->model),
        model
    );

    usb_copy_text(
        output->serial,
        sizeof(output->serial),
        serial
    );

    usb_copy_text(
        output->syspath,
        sizeof(output->syspath),
        syspath
    );

    (void)usb_parse_unsigned(
        vendor_id,
        16,
        &output->vendor_id
    );

    (void)usb_parse_unsigned(
        product_id,
        16,
        &output->product_id
    );

    (void)usb_parse_unsigned(
        bus,
        10,
        &output->bus_number
    );

    (void)usb_parse_unsigned(
        devnum,
        10,
        &output->device_number
    );
}

TrainlogStatus trainlog_usb_list_mtp_devices(
    TrainlogUsbDevice *output,
    size_t capacity,
    size_t *output_count
)
{
    struct udev *udev = NULL;
    struct udev_enumerate *enumerate = NULL;
    struct udev_list_entry *devices;
    struct udev_list_entry *entry;
    size_t discovered = 0U;
    TrainlogStatus status =
        TRAINLOG_STATUS_OK;

    if (output_count == NULL ||
        (output == NULL &&
         capacity != 0U)) {
        return
            TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    *output_count = 0U;

    udev = udev_new();

    if (udev == NULL) {
        return
            TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    enumerate =
        udev_enumerate_new(udev);

    if (enumerate == NULL) {
        udev_unref(udev);
        return
            TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    if (udev_enumerate_add_match_subsystem(
            enumerate,
            "usb"
        ) < 0 ||
        udev_enumerate_scan_devices(
            enumerate
        ) < 0) {
        status =
            TRAINLOG_STATUS_SYSTEM_ERROR;
        goto cleanup;
    }

    devices =
        udev_enumerate_get_list_entry(
            enumerate
        );

    udev_list_entry_foreach(
        entry,
        devices
    ) {
        const char *syspath =
            udev_list_entry_get_name(
                entry
            );

        struct udev_device *device;

        if (syspath == NULL) {
            continue;
        }

        device =
            udev_device_new_from_syspath(
                udev,
                syspath
            );

        if (device == NULL) {
            continue;
        }

        if (usb_device_is_physical_mtp(
                device
            )) {
            if (output != NULL &&
                discovered < capacity) {
                usb_fill_device(
                    device,
                    &output[discovered]
                );
            }

            ++discovered;
        }

        udev_device_unref(
            device
        );
    }

    *output_count =
        discovered;

    if (output != NULL &&
        discovered > capacity) {
        status =
            TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

cleanup:
    udev_enumerate_unref(
        enumerate
    );

    udev_unref(
        udev
    );

    return status;
}
