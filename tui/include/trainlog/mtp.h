#ifndef TRAINLOG_MTP_H
#define TRAINLOG_MTP_H

/**
 * @file mtp.h
 * @brief MTP device access and storage enumeration.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "trainlog/status.h"

#define TRAINLOG_MTP_STORAGE_TEXT_MAX 255U

typedef struct TrainlogMtpStorage {
    uint32_t storage_id;
    uint16_t storage_type;
    uint16_t filesystem_type;
    uint16_t access_capability;
    uint64_t max_capacity_bytes;
    uint64_t free_space_bytes;
    uint64_t free_space_objects;
    char description[TRAINLOG_MTP_STORAGE_TEXT_MAX + 1U];
    char volume_identifier[TRAINLOG_MTP_STORAGE_TEXT_MAX + 1U];
} TrainlogMtpStorage;

/**
 * @brief Open one exact MTP device and list its storage areas.
 *
 * bus_number/device_number come from TrainlogUsbDevice.
 *
 * Passing output=NULL with capacity=0 is a count-only query.
 * Insufficient output capacity is rejected rather than silently truncated.
 */
TrainlogStatus trainlog_mtp_list_storages(
    unsigned int bus_number,
    unsigned int device_number,
    TrainlogMtpStorage *output,
    size_t capacity,
    size_t *output_count
);

/**
 * @brief Find or create one root folder on an exact MTP storage.
 */
TrainlogStatus trainlog_mtp_ensure_root_folder(
    unsigned int bus_number,
    unsigned int device_number,
    uint32_t storage_id,
    const char *folder_name,
    uint32_t *output_folder_id,
    bool *output_created
);

/**
 * @brief Upload one local text file into an exact MTP folder.
 */
TrainlogStatus trainlog_mtp_send_text_file(
    unsigned int bus_number,
    unsigned int device_number,
    uint32_t storage_id,
    uint32_t parent_folder_id,
    const char *local_path,
    const char *remote_filename,
    uint32_t *output_item_id
);

#define TRAINLOG_MTP_ENTRY_NAME_MAX 255U

typedef struct TrainlogMtpEntry {
    uint32_t item_id;
    uint32_t parent_id;
    uint32_t storage_id;
    uint64_t size_bytes;
    uint64_t modification_unix_seconds;
    bool folder;
    char name[TRAINLOG_MTP_ENTRY_NAME_MAX + 1U];
} TrainlogMtpEntry;

/**
 * @brief List direct children of one MTP folder.
 *
 * Passing output=NULL with capacity=0 is a count-only query.
 * Insufficient capacity is rejected rather than silently truncated.
 */
TrainlogStatus trainlog_mtp_list_folder(
    unsigned int bus_number,
    unsigned int device_number,
    uint32_t storage_id,
    uint32_t parent_folder_id,
    TrainlogMtpEntry *output,
    size_t capacity,
    size_t *output_count
);

/**
 * @brief Download one MTP object to a local file.
 */
TrainlogStatus trainlog_mtp_receive_file(
    unsigned int bus_number,
    unsigned int device_number,
    uint32_t item_id,
    const char *local_path
);

TrainlogStatus trainlog_mtp_delete_object(
    unsigned int bus_number,
    unsigned int device_number,
    uint32_t item_id
);

#endif
