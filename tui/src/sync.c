/**
 * @file sync.c
 * @brief Shared direct-MTP Trainlog synchronization engine.
 */

#include "trainlog/sync.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "trainlog/id.h"
#include "trainlog/timeutil.h"

#define SYNC_DEVICE_CAPACITY 8U
#define SYNC_STORAGE_CAPACITY 8U
#define SYNC_ENTRY_CAPACITY 256U
#define SYNC_TOOL_OUTPUT_MAX 4095U
#define SYNC_REQUEST_TEXT_MAX 4095U

static const char *const MOBILE_EXPORT_NAME =
    "trainlog-mobile-export-v1.json";

static const char *const PC_CATALOG_NAME =
    "trainlog-pc-catalog-v1.json";

static const char *const SYNC_REQUEST_NAME =
    "trainlog-sync-request-v1.json";

static const char *const SYNC_RECEIPT_NAME =
    "trainlog-sync-receipt-v1.json";

static const char *const MOBILE_EXPORT_LOCAL =
    "/tmp/trainlog-mobile-export-v1.json";

static const char *const PC_CATALOG_LOCAL =
    "/tmp/trainlog-pc-catalog-v1.json";

static const char *const SYNC_REQUEST_LOCAL =
    "/tmp/trainlog-sync-request-v1.json";

static const char *const SYNC_RECEIPT_LOCAL =
    "/tmp/trainlog-sync-receipt-v1.json";

static const char *const MOBILE_IMPORT_RESULT =
    "/tmp/trainlog-mobile-import-result.txt";

static const char *const PC_CATALOG_RESULT =
    "/tmp/trainlog-pc-catalog-result.txt";

typedef struct SyncSilence {
    int saved_stdout;
    int saved_stderr;
    int null_fd;
} SyncSilence;

static bool ensure_directory(
    const char *path
)
{
    if (
        path == NULL ||
        path[0] == '\0'
    ) {
        return false;
    }

    if (
        mkdir(
            path,
            0700
        ) == 0
    ) {
        return true;
    }

    return errno == EEXIST;
}

static bool sync_data_root(
    char *output,
    size_t output_size
)
{
    const char *data_home =
        getenv(
            "XDG_DATA_HOME"
        );

    const char *home =
        getenv(
            "HOME"
        );

    int written;

    if (
        output == NULL ||
        output_size == 0U
    ) {
        return false;
    }

    if (
        data_home != NULL &&
        data_home[0] != '\0'
    ) {
        written =
            snprintf(
                output,
                output_size,
                "%s/trainlog",
                data_home
            );
    } else if (
        home != NULL &&
        home[0] != '\0'
    ) {
        char local_dir[
            PATH_MAX + 1U
        ];

        char share_dir[
            PATH_MAX + 1U
        ];

        written =
            snprintf(
                local_dir,
                sizeof(local_dir),
                "%s/.local",
                home
            );

        if (
            written < 0 ||
            (size_t)written >=
                sizeof(local_dir) ||
            !ensure_directory(
                local_dir
            )
        ) {
            return false;
        }

        written =
            snprintf(
                share_dir,
                sizeof(share_dir),
                "%s/share",
                local_dir
            );

        if (
            written < 0 ||
            (size_t)written >=
                sizeof(share_dir) ||
            !ensure_directory(
                share_dir
            )
        ) {
            return false;
        }

        written =
            snprintf(
                output,
                output_size,
                "%s/trainlog",
                share_dir
            );
    } else {
        return false;
    }

    if (
        written < 0 ||
        (size_t)written >=
            output_size
    ) {
        return false;
    }

    return
        ensure_directory(
            output
        );
}

static bool sync_runs_directory(
    char *output,
    size_t output_size
)
{
    char root[
        PATH_MAX + 1U
    ];

    int written;

    if (
        !sync_data_root(
            root,
            sizeof(root)
        )
    ) {
        return false;
    }

    written =
        snprintf(
            output,
            output_size,
            "%s/sync_runs",
            root
        );

    if (
        written < 0 ||
        (size_t)written >=
            output_size
    ) {
        return false;
    }

    return
        ensure_directory(
            output
        );
}

static bool sync_data_file(
    const char *name,
    char *output,
    size_t output_size
)
{
    char root[
        PATH_MAX + 1U
    ];

    int written;

    if (
        name == NULL ||
        !sync_data_root(
            root,
            sizeof(root)
        )
    ) {
        return false;
    }

    written =
        snprintf(
            output,
            output_size,
            "%s/%s",
            root,
            name
        );

    return
        written >= 0 &&
        (size_t)written <
            output_size;
}

static int sync_lock_open(
    bool blocking
)
{
    char path[
        PATH_MAX + 1U
    ];

    int descriptor;
    int operation =
        LOCK_EX;

    if (
        !sync_data_file(
            "sync.lock",
            path,
            sizeof(path)
        )
    ) {
        return -1;
    }

    descriptor =
        open(
            path,
            O_RDWR |
                O_CREAT |
                O_CLOEXEC,
            0600
        );

    if (descriptor < 0) {
        return -1;
    }

    if (!blocking) {
        operation |=
            LOCK_NB;
    }

    if (
        flock(
            descriptor,
            operation
        ) != 0
    ) {
        (void)close(
            descriptor
        );

        return -1;
    }

    return descriptor;
}

static void sync_lock_close(
    int descriptor
)
{
    if (descriptor < 0) {
        return;
    }

    (void)flock(
        descriptor,
        LOCK_UN
    );

    (void)close(
        descriptor
    );
}

static bool sync_silence_begin(
    SyncSilence *silence
)
{
    if (silence == NULL) {
        return false;
    }

    silence->saved_stdout = -1;
    silence->saved_stderr = -1;
    silence->null_fd = -1;

    (void)fflush(stdout);
    (void)fflush(stderr);

    silence->saved_stdout =
        dup(
            STDOUT_FILENO
        );

    silence->saved_stderr =
        dup(
            STDERR_FILENO
        );

    silence->null_fd =
        open(
            "/dev/null",
            O_WRONLY |
                O_CLOEXEC
        );

    if (
        silence->saved_stdout < 0 ||
        silence->saved_stderr < 0 ||
        silence->null_fd < 0
    ) {
        return false;
    }

    if (
        dup2(
            silence->null_fd,
            STDOUT_FILENO
        ) < 0 ||
        dup2(
            silence->null_fd,
            STDERR_FILENO
        ) < 0
    ) {
        return false;
    }

    return true;
}

static void sync_silence_end(
    SyncSilence *silence
)
{
    if (silence == NULL) {
        return;
    }

    (void)fflush(stdout);
    (void)fflush(stderr);

    if (
        silence->saved_stdout >= 0
    ) {
        (void)dup2(
            silence->saved_stdout,
            STDOUT_FILENO
        );

        (void)close(
            silence->saved_stdout
        );
    }

    if (
        silence->saved_stderr >= 0
    ) {
        (void)dup2(
            silence->saved_stderr,
            STDERR_FILENO
        );

        (void)close(
            silence->saved_stderr
        );
    }

    if (
        silence->null_fd >= 0
    ) {
        (void)close(
            silence->null_fd
        );
    }

    silence->saved_stdout = -1;
    silence->saved_stderr = -1;
    silence->null_fd = -1;
}

static TrainlogStatus sync_probe_unlocked(
    TrainlogSyncDeviceInfo *output
)
{
    TrainlogUsbDevice
        devices[SYNC_DEVICE_CAPACITY];

    TrainlogMtpStorage
        storages[SYNC_STORAGE_CAPACITY];

    size_t device_count = 0U;
    size_t storage_count = 0U;
    TrainlogStatus status;

    if (output == NULL) {
        return
            TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    (void)memset(
        output,
        0,
        sizeof(*output)
    );

    status =
        trainlog_usb_list_mtp_devices(
            devices,
            SYNC_DEVICE_CAPACITY,
            &device_count
        );

    if (
        status !=
            TRAINLOG_STATUS_OK ||
        device_count == 0U
    ) {
        return
            TRAINLOG_STATUS_NOT_FOUND;
    }

    output->connected = true;
    output->device = devices[0];

    status =
        trainlog_mtp_list_storages(
            output->device.bus_number,
            output->device.device_number,
            storages,
            SYNC_STORAGE_CAPACITY,
            &storage_count
        );

    if (
        status !=
            TRAINLOG_STATUS_OK ||
        storage_count == 0U
    ) {
        return
            TRAINLOG_STATUS_NOT_FOUND;
    }

    output->storage_ready = true;
    output->storage = storages[0];

    return TRAINLOG_STATUS_OK;
}

TrainlogStatus trainlog_sync_probe(
    TrainlogSyncDeviceInfo *output
)
{
    SyncSilence silence;
    int lock_fd;
    TrainlogStatus status;

    if (output == NULL) {
        return
            TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    lock_fd =
        sync_lock_open(
            false
        );

    if (lock_fd < 0) {
        return TRAINLOG_STATUS_CONFLICT;
    }

    if (
        !sync_silence_begin(
            &silence
        )
    ) {
        sync_silence_end(
            &silence
        );

        sync_lock_close(
            lock_fd
        );

        return
            TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    status =
        sync_probe_unlocked(
            output
        );

    sync_silence_end(
        &silence
    );

    sync_lock_close(
        lock_fd
    );

    return status;
}

static TrainlogStatus sync_find_child(
    const TrainlogSyncDeviceInfo *device,
    uint32_t parent_id,
    const char *name,
    bool folder,
    uint32_t *output_id,
    uint64_t *output_size
)
{
    TrainlogMtpEntry
        entries[SYNC_ENTRY_CAPACITY];

    size_t count = 0U;
    size_t index;
    TrainlogStatus status;

    if (
        device == NULL ||
        name == NULL ||
        output_id == NULL ||
        output_size == NULL ||
        !device->connected ||
        !device->storage_ready
    ) {
        return
            TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    *output_id = 0U;
    *output_size = 0U;

    status =
        trainlog_mtp_list_folder(
            device->device.bus_number,
            device->device.device_number,
            device->storage.storage_id,
            parent_id,
            entries,
            SYNC_ENTRY_CAPACITY,
            &count
        );

    if (
        status !=
        TRAINLOG_STATUS_OK
    ) {
        return status;
    }

    for (
        index = 0U;
        index < count;
        ++index
    ) {
        if (
            entries[index].folder ==
                folder &&
            strcmp(
                entries[index].name,
                name
            ) == 0
        ) {
            *output_id =
                entries[index].item_id;

            *output_size =
                entries[index]
                    .size_bytes;

            return
                TRAINLOG_STATUS_OK;
        }
    }

    return TRAINLOG_STATUS_NOT_FOUND;
}

static TrainlogStatus sync_find_exchange_folder(
    const TrainlogSyncDeviceInfo *device,
    uint32_t *output_folder_id
)
{
    uint32_t download_id = 0U;
    uint64_t ignored_size = 0U;
    TrainlogStatus status;

    if (
        output_folder_id == NULL
    ) {
        return
            TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    status =
        sync_find_child(
            device,
            UINT32_MAX,
            "Download",
            true,
            &download_id,
            &ignored_size
        );

    if (
        status !=
        TRAINLOG_STATUS_OK
    ) {
        return status;
    }

    return
        sync_find_child(
            device,
            download_id,
            "Trainlog",
            true,
            output_folder_id,
            &ignored_size
        );
}

static TrainlogStatus sync_receive_named(
    const TrainlogSyncDeviceInfo *device,
    uint32_t folder_id,
    const char *name,
    const char *local_path,
    uint64_t *output_size
)
{
    uint32_t item_id = 0U;
    uint64_t size_bytes = 0U;
    TrainlogStatus status;

    if (
        name == NULL ||
        local_path == NULL
    ) {
        return
            TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    status =
        sync_find_child(
            device,
            folder_id,
            name,
            false,
            &item_id,
            &size_bytes
        );

    if (
        status !=
        TRAINLOG_STATUS_OK
    ) {
        return status;
    }

    status =
        trainlog_mtp_receive_file(
            device->device.bus_number,
            device->device.device_number,
            item_id,
            local_path
        );

    if (
        status ==
            TRAINLOG_STATUS_OK &&
        output_size != NULL
    ) {
        *output_size =
            size_bytes;
    }

    return status;
}

static TrainlogStatus sync_publish_named(
    const TrainlogSyncDeviceInfo *device,
    uint32_t folder_id,
    const char *local_path,
    const char *remote_name
)
{
    uint32_t existing_id = 0U;
    uint32_t uploaded_id = 0U;
    uint64_t ignored_size = 0U;
    TrainlogStatus status;

    status =
        sync_find_child(
            device,
            folder_id,
            remote_name,
            false,
            &existing_id,
            &ignored_size
        );

    if (
        status ==
        TRAINLOG_STATUS_OK
    ) {
        status =
            trainlog_mtp_delete_object(
                device->device.bus_number,
                device->device.device_number,
                existing_id
            );

        if (
            status !=
            TRAINLOG_STATUS_OK
        ) {
            return status;
        }
    } else if (
        status !=
        TRAINLOG_STATUS_NOT_FOUND
    ) {
        return status;
    }

    return
        trainlog_mtp_send_text_file(
            device->device.bus_number,
            device->device.device_number,
            device->storage.storage_id,
            folder_id,
            local_path,
            remote_name,
            &uploaded_id
        );
}

static bool sync_resolve_repo_tool(
    const char *tool_name,
    char *output,
    size_t output_size
)
{
    char executable[
        PATH_MAX + 1U
    ];

    ssize_t length;
    char *slash;
    int level;
    int written;

    if (
        tool_name == NULL ||
        output == NULL ||
        output_size == 0U
    ) {
        return false;
    }

    length =
        readlink(
            "/proc/self/exe",
            executable,
            PATH_MAX
        );

    if (
        length <= 0 ||
        (size_t)length >=
            sizeof(executable)
    ) {
        return false;
    }

    executable[
        (size_t)length
    ] = '\0';

    for (
        level = 0;
        level < 3;
        ++level
    ) {
        slash =
            strrchr(
                executable,
                '/'
            );

        if (
            slash == NULL ||
            slash == executable
        ) {
            return false;
        }

        *slash = '\0';
    }

    written =
        snprintf(
            output,
            output_size,
            "%s/tools/%s",
            executable,
            tool_name
        );

    return
        written >= 0 &&
        (size_t)written <
            output_size &&
        access(
            output,
            R_OK
        ) == 0;
}

static bool sync_read_text(
    const char *path,
    char *output,
    size_t output_size
)
{
    FILE *file;
    size_t used;

    if (
        path == NULL ||
        output == NULL ||
        output_size < 2U
    ) {
        return false;
    }

    file =
        fopen(
            path,
            "rb"
        );

    if (file == NULL) {
        return false;
    }

    used =
        fread(
            output,
            1U,
            output_size - 1U,
            file
        );

    if (
        ferror(file) != 0
    ) {
        (void)fclose(file);
        return false;
    }

    output[used] = '\0';

    return
        fclose(file) == 0;
}

static TrainlogStatus sync_run_python_tool(
    const char *tool_name,
    const char *argument,
    const char *result_path,
    char *output,
    size_t output_size
)
{
    char tool[
        PATH_MAX + 1U
    ];

    pid_t child;
    int child_status;
    int result_fd;

    if (
        tool_name == NULL ||
        argument == NULL ||
        result_path == NULL ||
        output == NULL ||
        output_size < 2U
    ) {
        return
            TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    output[0] = '\0';

    if (
        !sync_resolve_repo_tool(
            tool_name,
            tool,
            sizeof(tool)
        )
    ) {
        return
            TRAINLOG_STATUS_NOT_FOUND;
    }

    result_fd =
        open(
            result_path,
            O_WRONLY |
                O_CREAT |
                O_TRUNC |
                O_CLOEXEC,
            0600
        );

    if (result_fd < 0) {
        return
            TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    child = fork();

    if (child < (pid_t)0) {
        (void)close(
            result_fd
        );

        return
            TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    if (child == (pid_t)0) {
        if (
            dup2(
                result_fd,
                STDOUT_FILENO
            ) < 0 ||
            dup2(
                result_fd,
                STDERR_FILENO
            ) < 0
        ) {
            _exit(126);
        }

        (void)close(
            result_fd
        );

        execlp(
            "python3",
            "python3",
            tool,
            argument,
            (char *)NULL
        );

        _exit(127);
    }

    (void)close(
        result_fd
    );

    if (
        waitpid(
            child,
            &child_status,
            0
        ) < (pid_t)0
    ) {
        return
            TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    (void)sync_read_text(
        result_path,
        output,
        output_size
    );

    if (
        !WIFEXITED(
            child_status
        ) ||
        WEXITSTATUS(
            child_status
        ) != 0
    ) {
        return
            TRAINLOG_STATUS_DATABASE_ERROR;
    }

    return
        TRAINLOG_STATUS_OK;
}

static size_t sync_report_value(
    const char *text,
    const char *name
)
{
    const char *position;
    char *end = NULL;
    unsigned long long value;

    if (
        text == NULL ||
        name == NULL
    ) {
        return 0U;
    }

    position =
        strstr(
            text,
            name
        );

    if (position == NULL) {
        return 0U;
    }

    position +=
        strlen(name);

    if (*position != '=') {
        return 0U;
    }

    ++position;

    value =
        strtoull(
            position,
            &end,
            10
        );

    if (
        end == position ||
        value >
            (unsigned long long)
                SIZE_MAX
    ) {
        return 0U;
    }

    return
        (size_t)value;
}

static void sync_last_nonempty_line(
    const char *text,
    char *output,
    size_t output_size
)
{
    const char *cursor;
    const char *line_start;
    size_t best_length = 0U;
    const char *best = NULL;

    if (
        output == NULL ||
        output_size == 0U
    ) {
        return;
    }

    output[0] = '\0';

    if (text == NULL) {
        return;
    }

    cursor = text;
    line_start = text;

    for (;;) {
        if (
            *cursor == '\n' ||
            *cursor == '\0'
        ) {
            size_t length =
                (size_t)(
                    cursor -
                    line_start
                );

            while (
                length > 0U &&
                (
                    line_start[length - 1U] ==
                        '\r' ||
                    line_start[length - 1U] ==
                        ' ' ||
                    line_start[length - 1U] ==
                        '\t'
                )
            ) {
                --length;
            }

            if (length > 0U) {
                best = line_start;
                best_length = length;
            }

            if (*cursor == '\0') {
                break;
            }

            line_start =
                cursor + 1;
        }

        ++cursor;
    }

    if (best != NULL) {
        size_t copy_length =
            best_length <
                output_size - 1U
                ? best_length
                : output_size - 1U;

        (void)memcpy(
            output,
            best,
            copy_length
        );

        output[copy_length] = '\0';
    }
}

static bool sync_json_string(
    const char *text,
    const char *key,
    char *output,
    size_t output_size
)
{
    char pattern[128];
    const char *position;
    const char *quote;
    const char *end;
    int written;
    size_t length;

    if (
        text == NULL ||
        key == NULL ||
        output == NULL ||
        output_size < 2U
    ) {
        return false;
    }

    written =
        snprintf(
            pattern,
            sizeof(pattern),
            "\"%s\"",
            key
        );

    if (
        written < 0 ||
        (size_t)written >=
            sizeof(pattern)
    ) {
        return false;
    }

    position =
        strstr(
            text,
            pattern
        );

    if (position == NULL) {
        return false;
    }

    position +=
        strlen(pattern);

    position =
        strchr(
            position,
            ':'
        );

    if (position == NULL) {
        return false;
    }

    ++position;

    while (
        *position == ' ' ||
        *position == '\t' ||
        *position == '\r' ||
        *position == '\n'
    ) {
        ++position;
    }

    if (*position != '"') {
        return false;
    }

    quote = position + 1;

    end =
        strchr(
            quote,
            '"'
        );

    if (end == NULL) {
        return false;
    }

    length =
        (size_t)(
            end -
            quote
        );

    if (
        length == 0U ||
        length >= output_size
    ) {
        return false;
    }

    (void)memcpy(
        output,
        quote,
        length
    );

    output[length] = '\0';

    return true;
}

static bool sync_parse_request(
    const char *text,
    char *output_request_id,
    size_t output_size
)
{
    char format[64];

    if (
        text == NULL ||
        output_request_id == NULL ||
        output_size < 2U ||
        !sync_json_string(
            text,
            "format",
            format,
            sizeof(format)
        ) ||
        strcmp(
            format,
            "trainlog-sync-request"
        ) != 0 ||
        strstr(
            text,
            "\"version\":1"
        ) == NULL
    ) {
        return false;
    }

    return
        sync_json_string(
            text,
            "request_id",
            output_request_id,
            output_size
        );
}

static bool sync_last_request_matches(
    const char *request_id
)
{
    char path[
        PATH_MAX + 1U
    ];

    char saved[
        TRAINLOG_ID_MAX + 2U
    ];

    size_t length;

    if (
        request_id == NULL ||
        request_id[0] == '\0' ||
        !sync_data_file(
            "sync_last_request.txt",
            path,
            sizeof(path)
        ) ||
        !sync_read_text(
            path,
            saved,
            sizeof(saved)
        )
    ) {
        return false;
    }

    length =
        strlen(saved);

    while (
        length > 0U &&
        (
            saved[length - 1U] ==
                '\n' ||
            saved[length - 1U] ==
                '\r'
        )
    ) {
        --length;
    }

    saved[length] = '\0';

    return
        strcmp(
            saved,
            request_id
        ) == 0;
}

static bool sync_save_last_request(
    const char *request_id
)
{
    char path[
        PATH_MAX + 1U
    ];

    FILE *file;

    if (
        request_id == NULL ||
        request_id[0] == '\0' ||
        !sync_data_file(
            "sync_last_request.txt",
            path,
            sizeof(path)
        )
    ) {
        return false;
    }

    file =
        fopen(
            path,
            "wb"
        );

    if (file == NULL) {
        return false;
    }

    if (
        fprintf(
            file,
            "%s\n",
            request_id
        ) < 0
    ) {
        (void)fclose(file);
        return false;
    }

    return
        fclose(file) == 0;
}

static const char *sync_trigger_text(
    TrainlogSyncTrigger trigger
)
{
    switch (trigger) {
    case TRAINLOG_SYNC_TRIGGER_ANDROID:
        return "android";

    case TRAINLOG_SYNC_TRIGGER_DAEMON:
        return "daemon";

    case TRAINLOG_SYNC_TRIGGER_TUI:
    default:
        return "tui";
    }
}

static bool sync_local_timestamp(
    char output[17]
)
{
    time_t now =
        time(NULL);

    struct tm local_time;

    if (
        now == (time_t)-1 ||
        localtime_r(
            &now,
            &local_time
        ) == NULL
    ) {
        return false;
    }

    return
        strftime(
            output,
            17U,
            "%d/%m/%Y %H:%M",
            &local_time
        ) != 0U;
}

static bool sync_json_write_escaped(
    FILE *file,
    const char *text
)
{
    const unsigned char *cursor;

    if (
        file == NULL ||
        text == NULL
    ) {
        return false;
    }

    if (
        fputc(
            '"',
            file
        ) == EOF
    ) {
        return false;
    }

    cursor =
        (const unsigned char *)text;

    while (*cursor != 0U) {
        switch (*cursor) {
        case '"':
            if (
                fputs(
                    "\\\"",
                    file
                ) == EOF
            ) {
                return false;
            }
            break;

        case '\\':
            if (
                fputs(
                    "\\\\",
                    file
                ) == EOF
            ) {
                return false;
            }
            break;

        case '\n':
            if (
                fputs(
                    "\\n",
                    file
                ) == EOF
            ) {
                return false;
            }
            break;

        case '\r':
            if (
                fputs(
                    "\\r",
                    file
                ) == EOF
            ) {
                return false;
            }
            break;

        case '\t':
            if (
                fputs(
                    "\\t",
                    file
                ) == EOF
            ) {
                return false;
            }
            break;

        default:
            if (
                fputc(
                    (int)*cursor,
                    file
                ) == EOF
            ) {
                return false;
            }
            break;
        }

        ++cursor;
    }

    return
        fputc(
            '"',
            file
        ) != EOF;
}

static bool sync_write_receipt(
    const TrainlogSyncReport *report
)
{
    FILE *file;

    if (
        report == NULL ||
        report->request_id[0] == '\0'
    ) {
        return false;
    }

    file =
        fopen(
            SYNC_RECEIPT_LOCAL,
            "wb"
        );

    if (file == NULL) {
        return false;
    }

    (void)fputs(
        "{\n  \"format\":\"trainlog-sync-receipt\",\n"
        "  \"version\":1,\n"
        "  \"request_id\":",
        file
    );

    if (
        !sync_json_write_escaped(
            file,
            report->request_id
        )
    ) {
        (void)fclose(file);
        return false;
    }

    (void)fputs(
        ",\n  \"sync_id\":",
        file
    );

    if (
        !sync_json_write_escaped(
            file,
            report->sync_id
        )
    ) {
        (void)fclose(file);
        return false;
    }

    (void)fputs(
        ",\n  \"status\":",
        file
    );

    if (
        !sync_json_write_escaped(
            file,
            report->success
                ? "success"
                : "failure"
        )
    ) {
        (void)fclose(file);
        return false;
    }

    (void)fputs(
        ",\n  \"summary\":",
        file
    );

    if (
        !sync_json_write_escaped(
            file,
            report->summary
        )
    ) {
        (void)fclose(file);
        return false;
    }

    (void)fprintf(
        file,
        ",\n  \"android_to_pc\":{"
        "\"exercises_imported\":%zu,"
        "\"exercises_reconciled\":%zu,"
        "\"exercises_skipped\":%zu,"
        "\"sessions_imported\":%zu,"
        "\"sessions_skipped\":%zu,"
        "\"body_imported\":%zu,"
        "\"body_skipped\":%zu"
        "},\n"
        "  \"pc_to_android\":{"
        "\"catalog_published\":%zu"
        "}\n}\n",
        report->exercises_imported,
        report->exercises_reconciled,
        report->exercises_skipped,
        report->sessions_imported,
        report->sessions_skipped,
        report->body_imported,
        report->body_skipped,
        report->catalog_published
    );

    return
        fclose(file) == 0;
}

static bool sync_record_run(
    TrainlogSyncTrigger trigger,
    const TrainlogSyncReport *report
)
{
    char directory[
        PATH_MAX + 1U
    ];

    char json_path[
        PATH_MAX + 1U
    ];

    char detail_path[
        PATH_MAX + 1U
    ];

    char history_path[
        PATH_MAX + 1U
    ];

    char local_timestamp[17];

    FILE *json_file;
    FILE *detail_file;
    FILE *history_file;
    int written;

    if (
        report == NULL ||
        report->sync_id[0] == '\0' ||
        !sync_runs_directory(
            directory,
            sizeof(directory)
        ) ||
        !sync_data_file(
            "sync_history.log",
            history_path,
            sizeof(history_path)
        ) ||
        !sync_local_timestamp(
            local_timestamp
        )
    ) {
        return false;
    }

    written =
        snprintf(
            json_path,
            sizeof(json_path),
            "%s/%s.json",
            directory,
            report->sync_id
        );

    if (
        written < 0 ||
        (size_t)written >=
            sizeof(json_path)
    ) {
        return false;
    }

    written =
        snprintf(
            detail_path,
            sizeof(detail_path),
            "%s/%s.txt",
            directory,
            report->sync_id
        );

    if (
        written < 0 ||
        (size_t)written >=
            sizeof(detail_path)
    ) {
        return false;
    }

    json_file =
        fopen(
            json_path,
            "wb"
        );

    if (json_file == NULL) {
        return false;
    }

    (void)fputs(
        "{\n"
        "  \"format\":\"trainlog-sync-run\",\n"
        "  \"version\":1,\n"
        "  \"sync_id\":",
        json_file
    );

    if (
        !sync_json_write_escaped(
            json_file,
            report->sync_id
        )
    ) {
        (void)fclose(json_file);
        return false;
    }

    (void)fputs(
        ",\n  \"started_at\":",
        json_file
    );

    if (
        !sync_json_write_escaped(
            json_file,
            report->started_at
        )
    ) {
        (void)fclose(json_file);
        return false;
    }

    (void)fputs(
        ",\n  \"trigger\":",
        json_file
    );

    if (
        !sync_json_write_escaped(
            json_file,
            sync_trigger_text(
                trigger
            )
        )
    ) {
        (void)fclose(json_file);
        return false;
    }

    (void)fputs(
        ",\n  \"status\":",
        json_file
    );

    if (
        !sync_json_write_escaped(
            json_file,
            report->success
                ? "success"
                : "failure"
        )
    ) {
        (void)fclose(json_file);
        return false;
    }

    (void)fputs(
        ",\n  \"request_id\":",
        json_file
    );

    if (
        !sync_json_write_escaped(
            json_file,
            report->request_id
        )
    ) {
        (void)fclose(json_file);
        return false;
    }

    (void)fputs(
        ",\n  \"summary\":",
        json_file
    );

    if (
        !sync_json_write_escaped(
            json_file,
            report->summary
        )
    ) {
        (void)fclose(json_file);
        return false;
    }

    (void)fprintf(
        json_file,
        ",\n  \"android_to_pc\":{"
        "\"exercises_imported\":%zu,"
        "\"exercises_reconciled\":%zu,"
        "\"exercises_skipped\":%zu,"
        "\"sessions_imported\":%zu,"
        "\"sessions_skipped\":%zu,"
        "\"body_imported\":%zu,"
        "\"body_skipped\":%zu"
        "},\n"
        "  \"pc_to_android\":{"
        "\"catalog_published\":%zu"
        "}\n}\n",
        report->exercises_imported,
        report->exercises_reconciled,
        report->exercises_skipped,
        report->sessions_imported,
        report->sessions_skipped,
        report->body_imported,
        report->body_skipped,
        report->catalog_published
    );

    if (
        fclose(
            json_file
        ) != 0
    ) {
        return false;
    }

    detail_file =
        fopen(
            detail_path,
            "wb"
        );

    if (detail_file == NULL) {
        return false;
    }

    (void)fprintf(
        detail_file,
        "SYNC %s\n\n"
        "Déclencheur : %s\n"
        "Début       : %s\n"
        "État        : %s\n",
        report->sync_id,
        sync_trigger_text(
            trigger
        ),
        local_timestamp,
        report->success
            ? "succès"
            : "échec"
    );

    if (
        report->request_id[0] != '\0'
    ) {
        (void)fprintf(
            detail_file,
            "Requête     : %s\n",
            report->request_id
        );
    }

    (void)fprintf(
        detail_file,
        "\nANDROID → PC\n"
        "  Exercices importés       +%zu\n"
        "  Exercices réconciliés    +%zu\n"
        "  Exercices déjà présents   %zu\n"
        "  Séances importées        +%zu\n"
        "  Séances déjà présentes    %zu\n"
        "  Mensurations importées   +%zu\n"
        "  Mensurations déjà présentes %zu\n"
        "\nPC → ANDROID\n"
        "  Catalogue publié          %zu exercice(s)\n"
        "\nRésumé\n"
        "  %s\n",
        report->exercises_imported,
        report->exercises_reconciled,
        report->exercises_skipped,
        report->sessions_imported,
        report->sessions_skipped,
        report->body_imported,
        report->body_skipped,
        report->catalog_published,
        report->summary
    );

    if (
        report->error[0] != '\0'
    ) {
        (void)fprintf(
            detail_file,
            "\nErreur\n  %s\n",
            report->error
        );
    }

    if (
        fclose(
            detail_file
        ) != 0
    ) {
        return false;
    }

    history_file =
        fopen(
            history_path,
            "ab"
        );

    if (history_file == NULL) {
        return false;
    }

    (void)fprintf(
        history_file,
        "%s\t%s\t%d\t%.*s\n",
        report->sync_id,
        local_timestamp,
        report->success
            ? 1
            : 0,
        (int)TRAINLOG_SYNC_SUMMARY_MAX,
        report->summary
    );

    return
        fclose(
            history_file
        ) == 0;
}

static void sync_build_summary(
    TrainlogSyncReport *report
)
{
    if (report == NULL) {
        return;
    }

    if (report->success) {
        (void)snprintf(
            report->summary,
            sizeof(report->summary),
            "Android→PC +%zu séance(s), +%zu exercice(s), +%zu mesure(s) · PC→Android catalogue %zu exercice(s)",
            report->sessions_imported,
            report->exercises_imported +
                report->exercises_reconciled,
            report->body_imported,
            report->catalog_published
        );
    } else {
        (void)snprintf(
            report->summary,
            sizeof(report->summary),
            "%s",
            report->error[0] != '\0'
                ? report->error
                : "Synchronisation échouée."
        );
    }
}

static TrainlogStatus sync_prepare_run_identity(
    TrainlogSyncReport *report
)
{
    if (
        report == NULL ||
        trainlog_id_generate(
            "sy",
            report->sync_id,
            sizeof(report->sync_id)
        ) != TRAINLOG_STATUS_OK ||
        trainlog_time_now_rfc3339(
            report->started_at,
            sizeof(report->started_at)
        ) != TRAINLOG_STATUS_OK
    ) {
        return
            TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    return TRAINLOG_STATUS_OK;
}

static void sync_parse_import_report(
    const char *text,
    TrainlogSyncReport *report
)
{
    if (
        text == NULL ||
        report == NULL
    ) {
        return;
    }

    report->exercises_imported =
        sync_report_value(
            text,
            "exercises_imported"
        );

    report->exercises_reconciled =
        sync_report_value(
            text,
            "exercises_reconciled"
        );

    report->exercises_skipped =
        sync_report_value(
            text,
            "exercises_skipped"
        );

    report->sessions_imported =
        sync_report_value(
            text,
            "sessions_imported"
        );

    report->sessions_skipped =
        sync_report_value(
            text,
            "sessions_skipped"
        );

    report->body_imported =
        sync_report_value(
            text,
            "body_imported"
        );

    report->body_skipped =
        sync_report_value(
            text,
            "body_skipped"
        );
}

/* TRAINLOG_SYNCD_NO_UNKNOWN_ERRORS */
TrainlogStatus trainlog_sync_run(
    TrainlogSyncTrigger trigger,
    bool require_request,
    TrainlogSyncReport *output
)
{
    TrainlogSyncDeviceInfo device;
    SyncSilence silence;
    TrainlogStatus status;
    TrainlogStatus final_status =
        TRAINLOG_STATUS_OK;

    char tool_output[
        SYNC_TOOL_OUTPUT_MAX + 1U
    ];

    char request_text[
        SYNC_REQUEST_TEXT_MAX + 1U
    ];

    uint32_t folder_id = 0U;
    uint64_t ignored_size = 0U;
    int lock_fd = -1;
    bool silence_active = false;
    bool run_started = false;
    bool receipt_published = false;

    if (output == NULL) {
        return
            TRAINLOG_STATUS_INVALID_ARGUMENT;
    }

    (void)memset(
        output,
        0,
        sizeof(*output)
    );

    lock_fd =
        sync_lock_open(
            !require_request
        );

    if (lock_fd < 0) {
        (void)snprintf(
            output->error,
            sizeof(output->error),
            "%s",
            "Une autre synchronisation est déjà en cours."
        );

        return TRAINLOG_STATUS_CONFLICT;
    }

    if (
        !sync_silence_begin(
            &silence
        )
    ) {
        sync_silence_end(
            &silence
        );

        sync_lock_close(
            lock_fd
        );

        (void)snprintf(
            output->error,
            sizeof(output->error),
            "%s",
            "Impossible d'isoler la sortie MTP."
        );

        return
            TRAINLOG_STATUS_SYSTEM_ERROR;
    }

    silence_active = true;

    status =
        sync_probe_unlocked(
            &device
        );

    if (
        status !=
        TRAINLOG_STATUS_OK
    ) {
        (void)snprintf(
            output->error,
            sizeof(output->error),
            "Détection MTP échouée (status=%d).",
            (int)status
        );

        final_status = status;
        goto done;
    }

    status =
        sync_find_exchange_folder(
            &device,
            &folder_id
        );

    if (
        status !=
        TRAINLOG_STATUS_OK
    ) {
        (void)snprintf(
            output->error,
            sizeof(output->error),
            "Résolution de Download/Trainlog échouée (status=%d).",
            (int)status
        );

        final_status = status;
        goto done;
    }

    if (require_request) {
        status =
            sync_receive_named(
                &device,
                folder_id,
                SYNC_REQUEST_NAME,
                SYNC_REQUEST_LOCAL,
                &ignored_size
            );

        if (
            status ==
            TRAINLOG_STATUS_NOT_FOUND
        ) {
            final_status =
                TRAINLOG_STATUS_NOT_FOUND;
            goto done;
        }

        if (
            status !=
            TRAINLOG_STATUS_OK
        ) {
            (void)snprintf(
                output->error,
                sizeof(output->error),
                "Lecture MTP de la requête Android échouée (status=%d).",
                (int)status
            );

            final_status = status;
            goto done;
        }

        if (
            !sync_read_text(
                SYNC_REQUEST_LOCAL,
                request_text,
                sizeof(request_text)
            )
        ) {
            (void)snprintf(
                output->error,
                sizeof(output->error),
                "%s",
                "Lecture locale de la requête Android échouée."
            );

            final_status =
                TRAINLOG_STATUS_SYSTEM_ERROR;
            goto done;
        }

        if (
            !sync_parse_request(
                request_text,
                output->request_id,
                sizeof(output->request_id)
            )
        ) {
            (void)snprintf(
                output->error,
                sizeof(output->error),
                "%s",
                "JSON de requête Android invalide."
            );

            final_status =
                TRAINLOG_STATUS_SYSTEM_ERROR;
            goto done;
        }

        output->request_present = true;

        if (
            sync_last_request_matches(
                output->request_id
            )
        ) {
            final_status =
                TRAINLOG_STATUS_NOT_FOUND;
            goto done;
        }
    }

    status =
        sync_prepare_run_identity(
            output
        );

    if (
        status !=
        TRAINLOG_STATUS_OK
    ) {
        (void)snprintf(
            output->error,
            sizeof(output->error),
            "Création de l'identité de synchronisation échouée (status=%d).",
            (int)status
        );

        final_status = status;
        goto done;
    }

    run_started = true;

    status =
        sync_receive_named(
            &device,
            folder_id,
            MOBILE_EXPORT_NAME,
            MOBILE_EXPORT_LOCAL,
            &ignored_size
        );

    if (
        status !=
        TRAINLOG_STATUS_OK
    ) {
        (void)snprintf(
            output->error,
            sizeof(output->error),
            "%s",
            "Android→PC : snapshot mobile introuvable ou illisible."
        );

        final_status = status;
        goto finalize;
    }

    status =
        sync_run_python_tool(
            "import_mobile_export.py",
            MOBILE_EXPORT_LOCAL,
            MOBILE_IMPORT_RESULT,
            tool_output,
            sizeof(tool_output)
        );

    if (
        status !=
        TRAINLOG_STATUS_OK ||
        strstr(
            tool_output,
            "MOBILE_IMPORT=PASS"
        ) == NULL
    ) {
        char useful[
            TRAINLOG_SYNC_ERROR_MAX + 1U
        ];

        sync_last_nonempty_line(
            tool_output,
            useful,
            sizeof(useful)
        );

        (void)snprintf(
            output->error,
            sizeof(output->error),
            "Android→PC : %s",
            useful[0] != '\0'
                ? useful
                : "import mobile échoué"
        );

        final_status =
            TRAINLOG_STATUS_DATABASE_ERROR;
        goto finalize;
    }

    sync_parse_import_report(
        tool_output,
        output
    );

    status =
        sync_run_python_tool(
            "export_pc_catalog.py",
            PC_CATALOG_LOCAL,
            PC_CATALOG_RESULT,
            tool_output,
            sizeof(tool_output)
        );

    if (
        status !=
        TRAINLOG_STATUS_OK ||
        strstr(
            tool_output,
            "PC_CATALOG_EXPORT=PASS"
        ) == NULL
    ) {
        char useful[
            TRAINLOG_SYNC_ERROR_MAX + 1U
        ];

        sync_last_nonempty_line(
            tool_output,
            useful,
            sizeof(useful)
        );

        (void)snprintf(
            output->error,
            sizeof(output->error),
            "PC→Android : %s",
            useful[0] != '\0'
                ? useful
                : "export catalogue échoué"
        );

        final_status =
            TRAINLOG_STATUS_SYSTEM_ERROR;
        goto finalize;
    }

    output->catalog_published =
        sync_report_value(
            tool_output,
            "exercises"
        );

    status =
        sync_publish_named(
            &device,
            folder_id,
            PC_CATALOG_LOCAL,
            PC_CATALOG_NAME
        );

    if (
        status !=
        TRAINLOG_STATUS_OK
    ) {
        (void)snprintf(
            output->error,
            sizeof(output->error),
            "%s",
            "PC→Android : publication MTP du catalogue échouée."
        );

        final_status = status;
        goto finalize;
    }

    output->success = true;
    final_status =
        TRAINLOG_STATUS_OK;

finalize:
    sync_build_summary(
        output
    );

    if (
        output->request_present
    ) {
        if (
            sync_write_receipt(
                output
            )
        ) {
            status =
                sync_publish_named(
                    &device,
                    folder_id,
                    SYNC_RECEIPT_LOCAL,
                    SYNC_RECEIPT_NAME
                );

            if (
                status ==
                TRAINLOG_STATUS_OK
            ) {
                receipt_published = true;
            }
        }

        if (!receipt_published) {
            output->success = false;

            (void)snprintf(
                output->error,
                sizeof(output->error),
                "%s",
                "PC→Android : impossible de publier le reçu de synchronisation."
            );

            final_status =
                TRAINLOG_STATUS_SYSTEM_ERROR;

            sync_build_summary(
                output
            );
        }
    }

    (void)sync_record_run(
        trigger,
        output
    );

    if (
        output->request_present &&
        receipt_published
    ) {
        (void)sync_save_last_request(
            output->request_id
        );
    }

done:
    if (silence_active) {
        sync_silence_end(
            &silence
        );
    }

    sync_lock_close(
        lock_fd
    );

    if (
        run_started &&
        !output->success &&
        output->error[0] == '\0'
    ) {
        (void)snprintf(
            output->error,
            sizeof(output->error),
            "%s",
            "Synchronisation échouée."
        );
    }

    return final_status;
}
