/**
 * @file mtp.c
 * @brief libmtp-backed storage access.
 */

#include "trainlog/mtp.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <libmtp.h>

static void mtp_copy_text(
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

static bool mtp_raw_device_matches(
    const LIBMTP_raw_device_t *device,
    unsigned int bus_number,
    unsigned int device_number
)
{
    if (device == NULL ||
        device_number > UINT8_MAX) {
        return false;
    }

    return
        device->bus_location ==
            (uint32_t)bus_number &&
        device->devnum ==
            (uint8_t)device_number;
}

static TrainlogStatus mtp_detect_raw_devices(
    LIBMTP_raw_device_t **output_devices,
    int *output_count
)
{
    LIBMTP_error_number_t error;

    if (output_devices == NULL ||
        output_count == NULL) {
        return
            TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    *output_devices = NULL;
    *output_count = 0;

    LIBMTP_Init();

    error =
        LIBMTP_Detect_Raw_Devices(
            output_devices,
            output_count
        );

    switch (error) {
    case LIBMTP_ERROR_NONE:
        return TRAINLOG_STATUS_OK;

    case LIBMTP_ERROR_NO_DEVICE_ATTACHED:
        return TRAINLOG_STATUS_NOT_FOUND;

    case LIBMTP_ERROR_MEMORY_ALLOCATION:
        return TRAINLOG_STATUS_SYSTEM_ERROR;

    case LIBMTP_ERROR_GENERAL:
    case LIBMTP_ERROR_PTP_LAYER:
    case LIBMTP_ERROR_USB_LAYER:
    case LIBMTP_ERROR_STORAGE_FULL:
    case LIBMTP_ERROR_CONNECTING:
    case LIBMTP_ERROR_CANCELLED:
    default:
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
}

TrainlogStatus trainlog_mtp_list_storages(
    unsigned int bus_number,
    unsigned int device_number,
    TrainlogMtpStorage *output,
    size_t capacity,
    size_t *output_count
)
{
    LIBMTP_raw_device_t *raw_devices = NULL;
    LIBMTP_mtpdevice_t *device = NULL;
    LIBMTP_devicestorage_t *storage;
    TrainlogStatus status;
    int raw_count = 0;
    int index;
    size_t discovered = 0U;

    if (output_count == NULL ||
        (output == NULL &&
         capacity != 0U) ||
        device_number > UINT8_MAX) {
        return
            TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    *output_count = 0U;

    status =
        mtp_detect_raw_devices(
            &raw_devices,
            &raw_count
        );

    if (status != TRAINLOG_STATUS_OK) {
        free(raw_devices);
        return status;
    }

    for (index = 0;
         index < raw_count;
         ++index) {
        if (mtp_raw_device_matches(
                &raw_devices[index],
                bus_number,
                device_number
            )) {
            device =
                LIBMTP_Open_Raw_Device(
                    &raw_devices[index]
                );

            break;
        }
    }

    if (device == NULL) {
        free(raw_devices);
        return TRAINLOG_STATUS_NOT_FOUND;
    }

    if (LIBMTP_Get_Storage(
            device,
            LIBMTP_STORAGE_SORTBY_NOTSORTED
        ) != 0) {
        LIBMTP_Clear_Errorstack(device);
        LIBMTP_Release_Device(device);
        free(raw_devices);

        return
            TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    storage = device->storage;

    while (storage != NULL) {
        if (output != NULL &&
            discovered < capacity) {
            TrainlogMtpStorage *item =
                &output[discovered];

            (void)memset(
                item,
                0,
                sizeof(*item)
            );

            item->storage_id =
                storage->id;

            item->storage_type =
                storage->StorageType;

            item->filesystem_type =
                storage->FilesystemType;

            item->access_capability =
                storage->AccessCapability;

            item->max_capacity_bytes =
                storage->MaxCapacity;

            item->free_space_bytes =
                storage->FreeSpaceInBytes;

            item->free_space_objects =
                storage->FreeSpaceInObjects;

            mtp_copy_text(
                item->description,
                sizeof(item->description),
                storage->StorageDescription
            );

            mtp_copy_text(
                item->volume_identifier,
                sizeof(item->volume_identifier),
                storage->VolumeIdentifier
            );
        }

        ++discovered;
        storage = storage->next;
    }

    *output_count =
        discovered;

    if (output != NULL &&
        discovered > capacity) {
        status =
            TRAINLOG_STATUS_INVALID_ARGUMENT;
    } else {
        status =
            TRAINLOG_STATUS_OK;
    }

    LIBMTP_Release_Device(device);
    free(raw_devices);

    return status;
}

static TrainlogStatus mtp_open_exact_device(
    unsigned int bus_number,
    unsigned int device_number,
    LIBMTP_mtpdevice_t **output_device
)
{
    LIBMTP_raw_device_t *raw_devices = NULL;
    LIBMTP_mtpdevice_t *device = NULL;
    TrainlogStatus status;
    int raw_count = 0;
    int index;

    if (output_device == NULL ||
        device_number > UINT8_MAX) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    *output_device = NULL;

    status =
        mtp_detect_raw_devices(
            &raw_devices,
            &raw_count
        );

    if (status != TRAINLOG_STATUS_OK) {
        free(raw_devices);
        return status;
    }

    for (index = 0;
         index < raw_count;
         ++index) {
        if (mtp_raw_device_matches(
                &raw_devices[index],
                bus_number,
                device_number
            )) {
            device =
                LIBMTP_Open_Raw_Device(
                    &raw_devices[index]
                );
            break;
        }
    }

    free(raw_devices);

    if (device == NULL) {
        return TRAINLOG_STATUS_NOT_FOUND;
    }

    *output_device = device;

    return TRAINLOG_STATUS_OK;
}

static TrainlogStatus mtp_open_exact_device_uncached(
    unsigned int bus_number,
    unsigned int device_number,
    LIBMTP_mtpdevice_t **output_device
)
{
    LIBMTP_raw_device_t *raw_devices = NULL;
    LIBMTP_mtpdevice_t *device = NULL;
    TrainlogStatus status;
    int raw_count = 0;
    int index;

    if (output_device == NULL ||
        device_number > UINT8_MAX) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    *output_device = NULL;

    status =
        mtp_detect_raw_devices(
            &raw_devices,
            &raw_count
        );

    if (status != TRAINLOG_STATUS_OK) {
        free(raw_devices);
        return status;
    }

    for (index = 0;
         index < raw_count;
         ++index) {
        if (mtp_raw_device_matches(
                &raw_devices[index],
                bus_number,
                device_number
            )) {
            device =
                LIBMTP_Open_Raw_Device_Uncached(
                    &raw_devices[index]
                );
            break;
        }
    }

    free(raw_devices);

    if (device == NULL) {
        return TRAINLOG_STATUS_NOT_FOUND;
    }

    *output_device = device;

    return TRAINLOG_STATUS_OK;
}

static LIBMTP_folder_t *mtp_find_root_folder(
    LIBMTP_folder_t *folders,
    uint32_t storage_id,
    const char *folder_name
)
{
    LIBMTP_folder_t *folder;

    if (folder_name == NULL) {
        return NULL;
    }

    for (folder = folders;
         folder != NULL;
         folder = folder->sibling) {
        if (folder->storage_id == storage_id &&
            folder->name != NULL &&
            strcmp(
                folder->name,
                folder_name
            ) == 0) {
            return folder;
        }
    }

    return NULL;
}

static char *mtp_duplicate_text(
    const char *text
)
{
    size_t length;
    char *copy;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);

    copy =
        malloc(
            length + 1U
        );

    if (copy == NULL) {
        return NULL;
    }

    (void)memcpy(
        copy,
        text,
        length + 1U
    );

    return copy;
}

TrainlogStatus trainlog_mtp_ensure_root_folder(
    unsigned int bus_number,
    unsigned int device_number,
    uint32_t storage_id,
    const char *folder_name,
    uint32_t *output_folder_id,
    bool *output_created
)
{
    LIBMTP_mtpdevice_t *device = NULL;
    LIBMTP_folder_t *folders = NULL;
    LIBMTP_folder_t *existing;
    TrainlogStatus status;
    uint32_t folder_id;
    char mutable_name[256];

    if (folder_name == NULL ||
        folder_name[0] == '\0' ||
        output_folder_id == NULL ||
        output_created == NULL ||
        strlen(folder_name) >= sizeof(mutable_name)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    *output_folder_id = 0U;
    *output_created = false;

    status =
        mtp_open_exact_device(
            bus_number,
            device_number,
            &device
        );

    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }

    folders =
        LIBMTP_Get_Folder_List_For_Storage(
            device,
            storage_id
        );

    existing =
        mtp_find_root_folder(
            folders,
            storage_id,
            folder_name
        );

    if (existing != NULL) {
        *output_folder_id =
            existing->folder_id;

        if (folders != NULL) {
            LIBMTP_destroy_folder_t(folders);
        }

        LIBMTP_Release_Device(device);

        return TRAINLOG_STATUS_OK;
    }

    (void)snprintf(
        mutable_name,
        sizeof(mutable_name),
        "%s",
        folder_name
    );

    folder_id =
        LIBMTP_Create_Folder(
            device,
            mutable_name,
            UINT32_MAX,
            storage_id
        );

    if (folders != NULL) {
        LIBMTP_destroy_folder_t(folders);
    }

    if (folder_id == 0U) {
        LIBMTP_Clear_Errorstack(device);
        LIBMTP_Release_Device(device);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    *output_folder_id = folder_id;
    *output_created = true;

    LIBMTP_Release_Device(device);

    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_mtp_send_text_file(
    unsigned int bus_number,
    unsigned int device_number,
    uint32_t storage_id,
    uint32_t parent_folder_id,
    const char *local_path,
    const char *remote_filename,
    uint32_t *output_item_id
)
{
    struct stat file_stat;
    LIBMTP_mtpdevice_t *device = NULL;
    LIBMTP_file_t *metadata = NULL;
    TrainlogStatus status;
    int rc;

    if (local_path == NULL ||
        local_path[0] == '\0' ||
        remote_filename == NULL ||
        remote_filename[0] == '\0' ||
        output_item_id == NULL) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    *output_item_id = 0U;

    if (stat(
            local_path,
            &file_stat
        ) != 0 ||
        file_stat.st_size < 0) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    status =
        mtp_open_exact_device(
            bus_number,
            device_number,
            &device
        );

    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }

    metadata =
        LIBMTP_new_file_t();

    if (metadata == NULL) {
        LIBMTP_Release_Device(device);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    metadata->filename =
        mtp_duplicate_text(
            remote_filename
        );

    if (metadata->filename == NULL) {
        LIBMTP_destroy_file_t(metadata);
        LIBMTP_Release_Device(device);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    metadata->parent_id =
        parent_folder_id;

    metadata->storage_id =
        storage_id;

    metadata->filesize =
        (uint64_t)file_stat.st_size;

    metadata->filetype =
        LIBMTP_FILETYPE_TEXT;

    rc =
        LIBMTP_Send_File_From_File(
            device,
            local_path,
            metadata,
            NULL,
            NULL
        );

    if (rc != 0) {
        LIBMTP_Clear_Errorstack(device);
        LIBMTP_destroy_file_t(metadata);
        LIBMTP_Release_Device(device);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    *output_item_id =
        metadata->item_id;

    LIBMTP_destroy_file_t(metadata);
    LIBMTP_Release_Device(device);

    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_mtp_list_folder(
    unsigned int bus_number,
    unsigned int device_number,
    uint32_t storage_id,
    uint32_t parent_folder_id,
    TrainlogMtpEntry *output,
    size_t capacity,
    size_t *output_count
)
{
    LIBMTP_mtpdevice_t *device = NULL;
    LIBMTP_file_t *items = NULL;
    LIBMTP_file_t *item;
    TrainlogStatus status;
    size_t discovered = 0U;

    if (output_count == NULL ||
        (output == NULL &&
         capacity != 0U)) {
        return
            TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    *output_count = 0U;

    status =
        mtp_open_exact_device_uncached(
            bus_number,
            device_number,
            &device
        );

    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }

    items =
        LIBMTP_Get_Files_And_Folders(
            device,
            storage_id,
            parent_folder_id
        );

    for (item = items;
         item != NULL;
         item = item->next) {
        if (output != NULL &&
            discovered < capacity) {
            TrainlogMtpEntry *entry =
                &output[discovered];

            (void)memset(
                entry,
                0,
                sizeof(*entry)
            );

            entry->item_id =
                item->item_id;

            entry->parent_id =
                item->parent_id;

            entry->storage_id =
                item->storage_id;

            entry->size_bytes =
                item->filesize;

            entry->folder =
                item->filetype ==
                LIBMTP_FILETYPE_FOLDER;

            mtp_copy_text(
                entry->name,
                sizeof(entry->name),
                item->filename
            );
        }

        ++discovered;
    }

    if (items != NULL) {
        LIBMTP_destroy_file_t(
            items
        );
    }

    LIBMTP_Release_Device(
        device
    );

    *output_count =
        discovered;

    if (output != NULL &&
        discovered > capacity) {
        return
            TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    return
        TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_mtp_receive_file(
    unsigned int bus_number,
    unsigned int device_number,
    uint32_t item_id,
    const char *local_path
)
{
    LIBMTP_mtpdevice_t *device = NULL;
    TrainlogStatus status;
    int rc;

    if (item_id == 0U ||
        local_path == NULL ||
        local_path[0] == '\0') {
        return
            TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    status =
        mtp_open_exact_device_uncached(
            bus_number,
            device_number,
            &device
        );

    if (status != TRAINLOG_STATUS_OK) {
        return status;
    }

    rc =
        LIBMTP_Get_File_To_File(
            device,
            item_id,
            local_path,
            NULL,
            NULL
        );

    if (rc != 0) {
        LIBMTP_Clear_Errorstack(
            device
        );

        LIBMTP_Release_Device(
            device
        );

        return
            TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    LIBMTP_Release_Device(
        device
    );

    return
        TRAINLOG_STATUS_OK;
}
