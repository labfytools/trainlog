#include <stdio.h>
#include <string.h>

#include "trainlog/status.h"
#include "trainlog/usb.h"

#define CHECK(condition)                                      \
    do {                                                      \
        if (!(condition)) {                                   \
            (void)fprintf(                                    \
                stderr,                                       \
                "CHECK failed: %s:%d: %s\n",                  \
                __FILE__,                                     \
                __LINE__,                                     \
                #condition                                    \
            );                                                \
            return 1;                                         \
        }                                                     \
    } while (0)

#define TEST_CAPACITY 32U

static int test_argument_validation(void)
{
    size_t count = 0U;
    TrainlogUsbDevice device;

    CHECK(
        trainlog_usb_list_mtp_devices(
            NULL,
            1U,
            &count
        ) ==
        TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    CHECK(
        trainlog_usb_list_mtp_devices(
            &device,
            1U,
            NULL
        ) ==
        TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    return 0;
}

static int test_live_enumeration(void)
{
    TrainlogUsbDevice
        devices[TEST_CAPACITY];

    size_t count_only = 0U;
    size_t count = 0U;
    size_t index;
    size_t other;

    CHECK(
        trainlog_usb_list_mtp_devices(
            NULL,
            0U,
            &count_only
        ) ==
        TRAINLOG_STATUS_OK
    );

    CHECK(
        count_only <=
        TEST_CAPACITY
    );

    CHECK(
        trainlog_usb_list_mtp_devices(
            devices,
            TEST_CAPACITY,
            &count
        ) ==
        TRAINLOG_STATUS_OK
    );

    CHECK(count == count_only);

    for (index = 0U;
         index < count;
         ++index) {
        CHECK(
            devices[index].mtp
        );

        CHECK(
            devices[index].syspath[0] !=
            '\0'
        );

        CHECK(
            strstr(
                devices[index].syspath,
                ":1."
            ) == NULL
        );

        for (other = index + 1U;
             other < count;
             ++other) {
            CHECK(
                devices[index].bus_number !=
                    devices[other].bus_number ||
                devices[index].device_number !=
                    devices[other].device_number
            );
        }
    }

    return 0;
}

int main(void)
{
    CHECK(
        test_argument_validation() == 0
    );

    CHECK(
        test_live_enumeration() == 0
    );

    return 0;
}
