#include <limits.h>
#include <stdio.h>

#include "trainlog/mtp.h"
#include "trainlog/status.h"

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

static int test_argument_validation(void)
{
    size_t count = 0U;
    TrainlogMtpStorage storage;

    CHECK(
        trainlog_mtp_list_storages(
            1U,
            1U,
            NULL,
            1U,
            &count
        ) ==
        TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    CHECK(
        trainlog_mtp_list_storages(
            1U,
            1U,
            &storage,
            1U,
            NULL
        ) ==
        TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    CHECK(
        trainlog_mtp_list_storages(
            1U,
            UINT_MAX,
            &storage,
            1U,
            &count
        ) ==
        TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    return 0;
}

static int test_exchange_argument_validation(void)
{
    uint32_t folder_id = 0U;
    uint32_t item_id = 0U;
    bool created = false;

    CHECK(
        trainlog_mtp_ensure_root_folder(
            1U,
            1U,
            1U,
            NULL,
            &folder_id,
            &created
        ) ==
        TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    CHECK(
        trainlog_mtp_ensure_root_folder(
            1U,
            1U,
            1U,
            "",
            &folder_id,
            &created
        ) ==
        TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    CHECK(
        trainlog_mtp_send_text_file(
            1U,
            1U,
            1U,
            1U,
            NULL,
            "probe.txt",
            &item_id
        ) ==
        TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    CHECK(
        trainlog_mtp_send_text_file(
            1U,
            1U,
            1U,
            1U,
            "/definitely/not/present/trainlog-probe",
            "probe.txt",
            &item_id
        ) ==
        TRAINLOG_STATUS_SYSTEM_ERROR
    );

    return 0;
}

static int test_roundtrip_argument_validation(void)
{
    size_t count = 0U;
    TrainlogMtpEntry entry;

    CHECK(
        trainlog_mtp_list_folder(
            1U,
            1U,
            1U,
            1U,
            NULL,
            1U,
            &count
        ) ==
        TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    CHECK(
        trainlog_mtp_list_folder(
            1U,
            1U,
            1U,
            1U,
            &entry,
            1U,
            NULL
        ) ==
        TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    CHECK(
        trainlog_mtp_receive_file(
            1U,
            1U,
            0U,
            "/tmp/trainlog-invalid"
        ) ==
        TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    CHECK(
        trainlog_mtp_receive_file(
            1U,
            1U,
            1U,
            NULL
        ) ==
        TRAINLOG_STATUS_INVALID_ARGUMENT
    );

    return 0;
}

int main(void)
{
    CHECK(
        test_argument_validation() == 0
    );

    CHECK(
        test_exchange_argument_validation() == 0
    );

    CHECK(
        test_roundtrip_argument_validation() == 0
    );

    return 0;
}
