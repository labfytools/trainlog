#include "trainlog/generation_mtp.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <yyjson.h>

typedef struct Selection {
    unsigned int bus;
    unsigned int device;
    uint32_t storage;
    uint32_t root;
} Selection;

static void diag(char *output, size_t size, const char *value)
{
    if (output != NULL && size > 0U) (void)snprintf(output, size, "%s", value);
}

static bool safe_leaf(const char *name)
{
    return name != NULL && name[0] != '\0' && strcmp(name, ".") != 0 &&
        strcmp(name, "..") != 0 && strchr(name, '/') == NULL && strchr(name, '\\') == NULL;
}

static TrainlogStatus children(const TrainlogGenerationMtpIo *io, const Selection *selection,
    uint32_t parent, TrainlogMtpEntry *entries, size_t *count)
{
    return io->children(selection->bus, selection->device, selection->storage,
        parent, entries, TRAINLOG_GENERATION_MTP_MAX_CHILDREN, count);
}

static TrainlogStatus one_named(const TrainlogGenerationMtpIo *io, const Selection *selection,
    uint32_t parent, const char *name, bool folder, TrainlogMtpEntry *result)
{
    TrainlogMtpEntry entries[TRAINLOG_GENERATION_MTP_MAX_CHILDREN];
    size_t count = 0U, index, matches = 0U;
    TrainlogStatus status = children(io, selection, parent, entries, &count);
    if (status != TRAINLOG_STATUS_OK) return status;
    for (index = 0U; index < count; ++index) {
        if (entries[index].folder == folder && strcmp(entries[index].name, name) == 0) {
            *result = entries[index]; ++matches;
        }
    }
    if (matches > 1U) return TRAINLOG_STATUS_CONFLICT;
    return matches == 1U ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_NOT_FOUND;
}

static bool peer_document_matches(const char *path, const char *expected)
{
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    char buffer[16385];
    ssize_t count;
    yyjson_doc *document;
    yyjson_val *root, *capabilities, *item;
    size_t index, maximum;
    bool manifest = false, acknowledgement = false, valid;
    if (fd < 0) return false;
    count = read(fd, buffer, sizeof(buffer));
    (void)close(fd);
    if (count <= 0 || (size_t)count >= sizeof(buffer)) return false;
    buffer[count] = '\0';
    document = yyjson_read(buffer, (size_t)count, YYJSON_READ_NOFLAG);
    if (document == NULL) return false;
    root = yyjson_doc_get_root(document);
    capabilities = yyjson_obj_get(root, "capabilities");
    valid = yyjson_is_obj(root) && yyjson_obj_size(root) == 4U &&
        yyjson_equals_str(yyjson_obj_get(root, "format"), "trainlog-sync-peer") &&
        yyjson_is_uint(yyjson_obj_get(root, "version")) &&
        yyjson_get_uint(yyjson_obj_get(root, "version")) == 1U &&
        yyjson_equals_str(yyjson_obj_get(root, "peer_id"), expected) &&
        yyjson_is_arr(capabilities);
    if (valid) {
        yyjson_arr_foreach(capabilities, index, maximum, item) {
            if (!yyjson_is_str(item)) { valid = false; break; }
            if (yyjson_equals_str(item, "generation-manifest-v1")) manifest = true;
            if (yyjson_equals_str(item, "generation-ack-v1")) acknowledgement = true;
        }
    }
    yyjson_doc_free(document);
    return valid && manifest && acknowledgement;
}

static TrainlogStatus select_peer(const TrainlogGenerationMtpIo *io, const char *expected,
    const char *local_root, Selection *output, char *diagnostic, size_t diagnostic_size)
{
    TrainlogUsbDevice devices[TRAINLOG_GENERATION_MTP_MAX_DEVICES];
    TrainlogMtpStorage storages[TRAINLOG_GENERATION_MTP_MAX_STORAGES];
    size_t device_count = 0U, storage_count, di, si, matches = 0U;
    char peer_path[1024];
    TrainlogStatus status;
    if (snprintf(peer_path, sizeof(peer_path), "%s/.peer-candidate", local_root) < 0 ||
        strlen(local_root) + strlen("/.peer-candidate") >= sizeof(peer_path)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    status = io->devices(devices, TRAINLOG_GENERATION_MTP_MAX_DEVICES, &device_count);
    if (status != TRAINLOG_STATUS_OK || device_count == 0U) {
        diag(diagnostic, diagnostic_size, "no MTP device available"); return TRAINLOG_STATUS_NOT_FOUND;
    }
    for (di = 0U; di < device_count; ++di) {
        status = io->storages(devices[di].bus_number, devices[di].device_number,
            storages, TRAINLOG_GENERATION_MTP_MAX_STORAGES, &storage_count);
        if (status != TRAINLOG_STATUS_OK) continue;
        for (si = 0U; si < storage_count; ++si) {
            Selection candidate = {devices[di].bus_number, devices[di].device_number,
                storages[si].storage_id, 0U};
            TrainlogMtpEntry documents, root, peer;
            if (one_named(io, &candidate, UINT32_MAX, "Documents", true, &documents) != TRAINLOG_STATUS_OK ||
                one_named(io, &candidate, documents.item_id, "Trainlog", true, &root) != TRAINLOG_STATUS_OK) continue;
            candidate.root = root.item_id;
            if (one_named(io, &candidate, candidate.root, "android-peer-v1.json", false, &peer) != TRAINLOG_STATUS_OK) continue;
            (void)unlink(peer_path);
            if (peer.size_bytes > 16384U || io->receive(candidate.bus, candidate.device,
                    peer.item_id, peer_path) != TRAINLOG_STATUS_OK ||
                !peer_document_matches(peer_path, expected)) continue;
            *output = candidate; ++matches;
        }
    }
    (void)unlink(peer_path);
    if (matches == 0U) { diag(diagnostic, diagnostic_size, "expected Android peer not found"); return TRAINLOG_STATUS_NOT_FOUND; }
    if (matches > 1U) { diag(diagnostic, diagnostic_size, "ambiguous compatible Android peers"); return TRAINLOG_STATUS_CONFLICT; }
    return TRAINLOG_STATUS_OK;
}

static TrainlogStatus mkdir_private(const char *path)
{
    if (mkdir(path, 0700) == 0 || errno == EEXIST) return TRAINLOG_STATUS_OK;
    return TRAINLOG_STATUS_SYSTEM_ERROR;
}

static TrainlogStatus pull_folder(const TrainlogGenerationMtpIo *io, const Selection *selection,
    uint32_t parent, const char *local, unsigned int depth, uint64_t *total, size_t *objects)
{
    TrainlogMtpEntry entries[TRAINLOG_GENERATION_MTP_MAX_CHILDREN];
    size_t count = 0U, index;
    TrainlogStatus status;
    if (depth > TRAINLOG_GENERATION_MTP_MAX_DEPTH) return TRAINLOG_STATUS_INVALID_ARGUMENT;
    status = mkdir_private(local); if (status != TRAINLOG_STATUS_OK) return status;
    status = children(io, selection, parent, entries, &count); if (status != TRAINLOG_STATUS_OK) return status;
    if (*objects > TRAINLOG_GENERATION_MTP_MAX_CHILDREN - count) return TRAINLOG_STATUS_INVALID_ARGUMENT;
    *objects += count;
    for (index = 0U; index < count; ++index) {
        char path[1024], temporary[1060]; int written;
        written = snprintf(path, sizeof(path), "%s/%s", local, entries[index].name);
        if (!safe_leaf(entries[index].name) || written < 0 || (size_t)written >= sizeof(path)) return TRAINLOG_STATUS_INVALID_ARGUMENT;
        if (entries[index].folder) {
            status = pull_folder(io, selection, entries[index].item_id, path, depth + 1U, total, objects);
        } else {
            struct stat value;
            if (entries[index].size_bytes > TRAINLOG_GENERATION_MTP_MAX_TOTAL_BYTES ||
                *total > TRAINLOG_GENERATION_MTP_MAX_TOTAL_BYTES - entries[index].size_bytes) return TRAINLOG_STATUS_INVALID_ARGUMENT;
            *total += entries[index].size_bytes;
            written = snprintf(temporary, sizeof(temporary), "%s.part.%ld", path, (long)getpid());
            if (written < 0 || (size_t)written >= sizeof(temporary)) return TRAINLOG_STATUS_INVALID_ARGUMENT;
            (void)unlink(temporary);
            status = io->receive(selection->bus, selection->device, entries[index].item_id, temporary);
            if (status == TRAINLOG_STATUS_OK && (stat(temporary, &value) != 0 || value.st_size < 0 ||
                    (uint64_t)value.st_size != entries[index].size_bytes || rename(temporary, path) != 0)) status = TRAINLOG_STATUS_SYSTEM_ERROR;
            if (status != TRAINLOG_STATUS_OK) (void)unlink(temporary);
        }
        if (status != TRAINLOG_STATUS_OK) return status;
    }
    return TRAINLOG_STATUS_OK;
}

static bool same_files(const char *left, const char *right)
{
    int a = open(left, O_RDONLY | O_CLOEXEC), b = open(right, O_RDONLY | O_CLOEXEC);
    char ab[8192], bb[8192]; ssize_t ac = 0, bc; bool same = true;
    if (a < 0 || b < 0) same = false;
    while (same && (ac = read(a, ab, sizeof(ab))) > 0) {
        bc = read(b, bb, (size_t)ac); if (bc != ac || memcmp(ab, bb, (size_t)ac) != 0) same = false;
    }
    if (same && (ac < 0 || read(b, bb, 1U) != 0)) same = false;
    if (a >= 0) (void)close(a); if (b >= 0) (void)close(b); return same;
}

static TrainlogStatus push_file(const TrainlogGenerationMtpIo *io, const Selection *selection,
    uint32_t parent, const char *local, const char *name)
{
    TrainlogMtpEntry existing; TrainlogStatus status;
    char comparison[1060], temporary_name[256]; uint32_t uploaded = 0U;
    status = one_named(io, selection, parent, name, false, &existing);
    if (status == TRAINLOG_STATUS_OK) {
        (void)snprintf(comparison, sizeof(comparison), "%s.compare.%ld", local, (long)getpid());
        (void)unlink(comparison);
        status = io->receive(selection->bus, selection->device, existing.item_id, comparison);
        if (status != TRAINLOG_STATUS_OK) return status;
        if (same_files(local, comparison)) { (void)unlink(comparison); return TRAINLOG_STATUS_OK; }
        (void)unlink(comparison);
        (void)snprintf(temporary_name, sizeof(temporary_name), ".trainlog-%ld-%s", (long)getpid(), name);
        status = io->send(selection->bus, selection->device, selection->storage, parent, local, temporary_name, &uploaded);
        if (status != TRAINLOG_STATUS_OK) return status;
        (void)snprintf(comparison, sizeof(comparison), "%s.verify.%ld", local, (long)getpid());
        (void)unlink(comparison);
        status = io->receive(selection->bus, selection->device, uploaded, comparison);
        if (status != TRAINLOG_STATUS_OK || !same_files(local, comparison)) {
            (void)unlink(comparison); (void)io->remove(selection->bus, selection->device, uploaded);
            return status == TRAINLOG_STATUS_OK ? TRAINLOG_STATUS_CONFLICT : status;
        }
        (void)unlink(comparison);
        status = io->remove(selection->bus, selection->device, existing.item_id);
        if (status == TRAINLOG_STATUS_OK) status = io->rename(selection->bus, selection->device, uploaded, name);
        return status;
    }
    if (status != TRAINLOG_STATUS_NOT_FOUND) return status;
    status = io->send(selection->bus, selection->device, selection->storage, parent, local, name, &uploaded);
    if (status != TRAINLOG_STATUS_OK) return status;
    (void)snprintf(comparison, sizeof(comparison), "%s.verify.%ld", local, (long)getpid());
    (void)unlink(comparison);
    status = io->receive(selection->bus, selection->device, uploaded, comparison);
    if (status != TRAINLOG_STATUS_OK || !same_files(local, comparison)) {
        (void)unlink(comparison); (void)io->remove(selection->bus, selection->device, uploaded);
        return status == TRAINLOG_STATUS_OK ? TRAINLOG_STATUS_CONFLICT : status;
    }
    (void)unlink(comparison);
    return TRAINLOG_STATUS_OK;
}

static TrainlogStatus push_folder(const TrainlogGenerationMtpIo *io, const Selection *selection,
    uint32_t parent, const char *local, unsigned int depth, bool manifests)
{
    DIR *directory; struct dirent *entry; TrainlogStatus status = TRAINLOG_STATUS_OK;
    if (depth > TRAINLOG_GENERATION_MTP_MAX_DEPTH || (directory = opendir(local)) == NULL) return TRAINLOG_STATUS_SYSTEM_ERROR;
    while ((entry = readdir(directory)) != NULL && status == TRAINLOG_STATUS_OK) {
        char path[1024]; struct stat value; uint32_t child; bool created;
        if (!safe_leaf(entry->d_name) || entry->d_name[0] == '.') continue;
        if ((strcmp(entry->d_name, "manifest.json") == 0) != manifests) continue;
        int written = snprintf(path, sizeof(path), "%s/%s", local, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(path) || lstat(path, &value) != 0 || S_ISLNK(value.st_mode)) { status = TRAINLOG_STATUS_INVALID_ARGUMENT; break; }
        if (S_ISDIR(value.st_mode)) {
            if (manifests) continue;
            status = io->ensure_folder(selection->bus, selection->device, selection->storage, parent, entry->d_name, &child, &created);
            if (status == TRAINLOG_STATUS_OK) status = push_folder(io, selection, child, path, depth + 1U, false);
            if (status == TRAINLOG_STATUS_OK) status = push_folder(io, selection, child, path, depth + 1U, true);
        } else if (S_ISREG(value.st_mode)) status = push_file(io, selection, parent, path, entry->d_name);
    }
    (void)closedir(directory); return status;
}

static TrainlogStatus validate_local_folder(const char *local, unsigned int depth,
    uint64_t *total, size_t *objects)
{
    DIR *directory;
    struct dirent *entry;
    TrainlogStatus status = TRAINLOG_STATUS_OK;
    if (depth > TRAINLOG_GENERATION_MTP_MAX_DEPTH || (directory = opendir(local)) == NULL)
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    while ((entry = readdir(directory)) != NULL && status == TRAINLOG_STATUS_OK) {
        char path[1024]; struct stat value; int written;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (!safe_leaf(entry->d_name) || entry->d_name[0] == '.') { status = TRAINLOG_STATUS_INVALID_ARGUMENT; break; }
        written = snprintf(path, sizeof(path), "%s/%s", local, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(path) || lstat(path, &value) != 0 ||
            S_ISLNK(value.st_mode)) { status = TRAINLOG_STATUS_INVALID_ARGUMENT; break; }
        if (*objects == TRAINLOG_GENERATION_MTP_MAX_CHILDREN) { status = TRAINLOG_STATUS_INVALID_ARGUMENT; break; }
        ++*objects;
        if (S_ISDIR(value.st_mode)) status = validate_local_folder(path, depth + 1U, total, objects);
        else if (S_ISREG(value.st_mode)) {
            if (value.st_size < 0 || (uint64_t)value.st_size > TRAINLOG_GENERATION_MTP_MAX_TOTAL_BYTES - *total)
                status = TRAINLOG_STATUS_INVALID_ARGUMENT;
            else *total += (uint64_t)value.st_size;
        } else status = TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    (void)closedir(directory);
    return status;
}

TrainlogStatus trainlog_generation_mtp_pull(const TrainlogGenerationMtpIo *io,
    const char *expected, const char *local, char *diagnostic, size_t diagnostic_size)
{
    Selection selection; uint64_t total = 0U; size_t objects = 0U; TrainlogStatus status;
    if (io == NULL || expected == NULL || local == NULL) return TRAINLOG_STATUS_INVALID_ARGUMENT;
    status = mkdir_private(local); if (status == TRAINLOG_STATUS_OK) status = select_peer(io, expected, local, &selection, diagnostic, diagnostic_size);
    if (status == TRAINLOG_STATUS_OK) status = pull_folder(io, &selection, selection.root, local, 0U, &total, &objects);
    return status;
}

TrainlogStatus trainlog_generation_mtp_push(const TrainlogGenerationMtpIo *io,
    const char *expected, const char *local, char *diagnostic, size_t diagnostic_size)
{
    Selection selection; TrainlogStatus status; uint64_t total = 0U; size_t objects = 0U;
    if (io == NULL || expected == NULL || local == NULL) return TRAINLOG_STATUS_INVALID_ARGUMENT;
    status = validate_local_folder(local, 0U, &total, &objects);
    if (status == TRAINLOG_STATUS_OK) status = select_peer(io, expected, local, &selection, diagnostic, diagnostic_size);
    if (status == TRAINLOG_STATUS_OK) status = push_folder(io, &selection, selection.root, local, 0U, false);
    if (status == TRAINLOG_STATUS_OK) status = push_folder(io, &selection, selection.root, local, 0U, true);
    return status;
}

static const TrainlogGenerationMtpIo production = {
    trainlog_usb_list_mtp_devices, trainlog_mtp_list_storages, trainlog_mtp_list_folder,
    trainlog_mtp_ensure_folder, trainlog_mtp_receive_file, trainlog_mtp_send_text_file,
    trainlog_mtp_delete_object, trainlog_mtp_rename_object
};
const TrainlogGenerationMtpIo *trainlog_generation_mtp_production_io(void) { return &production; }
