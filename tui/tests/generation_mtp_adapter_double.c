#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "trainlog/generation_mtp.h"
#include "bounded_path.h"

typedef struct Node {
    uint32_t id, parent;
    bool folder;
    char path[1024], name[256];
    uint64_t size;
} Node;
static Node nodes[2048];
static size_t node_count;
static const char *remote;
static bool scan_failed;
static uint32_t stable_path_id(const char *path) {
    const unsigned char *cursor = (const unsigned char *)path;
    uint32_t value = UINT32_C(2166136261);
    while (*cursor != '\0') {
        value ^= (uint32_t)*cursor++;
        value *= UINT32_C(16777619);
    }
    return value == 0U || value == UINT32_MAX ? value ^ UINT32_C(0x80000000) : value;
}

static uint32_t scan(const char *path, uint32_t parent) {
    DIR *d;
    struct dirent *e;
    uint32_t self;
    Node *n;
    struct stat st;
    if (lstat(path, &st) != 0) {
        if (errno != ENOENT) {
            scan_failed = true;
        }
        return 0U;
    }
    if (node_count >= sizeof(nodes) / sizeof(nodes[0])) {
        scan_failed = true;
        return 0U;
    }
    self = stable_path_id(path);
    for (size_t index = 0U; index < node_count; index++) {
        if (nodes[index].id == self && strcmp(nodes[index].path, path) != 0) {
            scan_failed = true;
            return 0U;
        }
    }
    n = &nodes[node_count++];
    n->id = self;
    n->parent = parent;
    n->folder = S_ISDIR(st.st_mode);
    n->size = (uint64_t)st.st_size;
    snprintf(n->path, sizeof(n->path), "%s", path);
    const char *slash = strrchr(path, '/');
    snprintf(n->name, sizeof(n->name), "%s", slash ? slash + 1 : path);
    if (n->folder && (d = opendir(path)) != NULL) {
        while ((e = readdir(d))) {
            if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) {
                char child[1024];
                if (!trainlog_test_join_path(child, sizeof(child), path, e->d_name)) {
                    scan_failed = true;
                    break;
                }
                scan(child, self);
            }
        }
        closedir(d);
    } else if (n->folder && errno != ENOENT) {
        scan_failed = true;
    }
    return self;
}
static bool rebuild(void) {
    node_count = 0U;
    scan_failed = false;
    return scan(remote, UINT32_MAX) != 0U && !scan_failed;
}
static Node *node(uint32_t id) {
    size_t i;
    for (i = 0; i < node_count; i++) {
        if (nodes[i].id == id) {
            return &nodes[i];
        }
    }
    return NULL;
}
static TrainlogStatus devices(TrainlogUsbDevice *o, size_t c, size_t *n) {
    if (c < 1U) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    memset(o, 0, sizeof(*o));
    o->bus_number = 1;
    o->device_number = 1;
    *n = 1;
    return TRAINLOG_STATUS_OK;
}
static TrainlogStatus
storages(unsigned int b, unsigned int d, TrainlogMtpStorage *o, size_t c, size_t *n) {
    (void)b;
    (void)d;
    if (c < 1U) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    memset(o, 0, sizeof(*o));
    o->storage_id = 1;
    *n = 1;
    return TRAINLOG_STATUS_OK;
}
static TrainlogStatus children(unsigned int b,
                               unsigned int d,
                               uint32_t s,
                               uint32_t p,
                               TrainlogMtpEntry *o,
                               size_t c,
                               size_t *n) {
    size_t i, k = 0;
    (void)b;
    (void)d;
    (void)s;
    if (!rebuild()) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    if (p == UINT32_MAX) {
        p = nodes[0].id;
    }
    for (i = 0; i < node_count; i++) {
        if (nodes[i].parent == p) {
            if (k >= c) {
                return TRAINLOG_STATUS_INVALID_ARGUMENT;
            }
            memset(&o[k], 0, sizeof(o[k]));
            o[k].item_id = nodes[i].id;
            o[k].parent_id = p;
            o[k].storage_id = 1;
            o[k].folder = nodes[i].folder;
            o[k].size_bytes = nodes[i].size;
            snprintf(o[k].name, sizeof(o[k].name), "%s", nodes[i].name);
            k++;
        }
    }
    *n = k;
    return TRAINLOG_STATUS_OK;
}
static TrainlogStatus ensure(unsigned int b,
                             unsigned int d,
                             uint32_t s,
                             uint32_t p,
                             const char *name,
                             uint32_t *id,
                             bool *created) {
    Node *parent;
    char path[1024];
    (void)b;
    (void)d;
    (void)s;
    if (!rebuild()) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    parent = node(p);
    if (!parent) {
        return TRAINLOG_STATUS_NOT_FOUND;
    }
    if (!trainlog_test_join_path(path, sizeof(path), parent->path, name)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    if (mkdir(path, 0700) == 0) {
        *created = true;
    } else if (access(path, F_OK) == 0) {
        *created = false;
    } else {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    if (!rebuild()) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    size_t i;
    for (i = 0; i < node_count; i++) {
        if (strcmp(nodes[i].path, path) == 0) {
            *id = nodes[i].id;
            return TRAINLOG_STATUS_OK;
        }
    }
    return TRAINLOG_STATUS_SYSTEM_ERROR;
}
static int copy(const char *a, const char *b) {
    FILE *in = fopen(a, "rb"), *out = fopen(b, "wb");
    char buf[8192];
    size_t n;
    if (!in || !out) {
        return -1;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            return -1;
        }
    }
    return fclose(in) | fclose(out);
}
static TrainlogStatus receive(unsigned int b, unsigned int d, uint32_t id, const char *path) {
    Node *n;
    (void)b;
    (void)d;
    if (!rebuild()) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    n = node(id);
    return n && copy(n->path, path) == 0 ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_SYSTEM_ERROR;
}
static TrainlogStatus send_file(unsigned int b,
                                unsigned int d,
                                uint32_t s,
                                uint32_t p,
                                const char *local,
                                const char *name,
                                uint32_t *id) {
    Node *parent;
    char path[1024];
    (void)b;
    (void)d;
    (void)s;
    if (!rebuild()) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    parent = node(p);
    if (!parent) {
        return TRAINLOG_STATUS_NOT_FOUND;
    }
    if (!trainlog_test_join_path(path, sizeof(path), parent->path, name)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    if (copy(local, path) != 0) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    if (!rebuild()) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    size_t i;
    for (i = 0; i < node_count; i++) {
        if (strcmp(nodes[i].path, path) == 0) {
            *id = nodes[i].id;
            return TRAINLOG_STATUS_OK;
        }
    }
    return TRAINLOG_STATUS_SYSTEM_ERROR;
}
static TrainlogStatus remove_object(unsigned int b, unsigned int d, uint32_t id) {
    Node *n;
    (void)b;
    (void)d;
    if (!rebuild()) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    n = node(id);
    return n && unlink(n->path) == 0 ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_SYSTEM_ERROR;
}
static TrainlogStatus rename_object(unsigned int b, unsigned int d, uint32_t id, const char *name) {
    Node *n;
    char parent[1024];
    char path[1024];
    char *slash;
    (void)b;
    (void)d;
    if (!rebuild()) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    n = node(id);
    if (!n) {
        return TRAINLOG_STATUS_NOT_FOUND;
    }
    if (strlen(n->path) >= sizeof(parent)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    (void)memcpy(parent, n->path, strlen(n->path) + 1U);
    slash = strrchr(parent, '/');
    if (!slash) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    *slash = '\0';
    if (!trainlog_test_join_path(path, sizeof(path), parent, name)) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    return rename(n->path, path) == 0 ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_SYSTEM_ERROR;
}
static const TrainlogGenerationMtpIo io = {
    .devices = devices,
    .storages = storages,
    .children = children,
    .ensure_folder = ensure,
    .receive = receive,
    .send = send_file,
    .remove = remove_object,
    .rename = rename_object,
};
int main(int argc, char **argv) {
    char diag[1024] = {0};
    TrainlogStatus status;
    remote = getenv("TRAINLOG_MTP_DOUBLE_ROOT");
    if (argc != 4 || !remote) {
        return 64;
    }
    if (strcmp(argv[1], "pull") == 0) {
        status = trainlog_generation_mtp_pull(&io, argv[2], argv[3], diag, sizeof(diag));
    } else if (strcmp(argv[1], "push") == 0) {
        status = trainlog_generation_mtp_push(&io, argv[2], argv[3], diag, sizeof(diag));
    } else {
        return 64;
    }
    if (status != TRAINLOG_STATUS_OK) {
        fprintf(stderr, "double failed %d %s\n", status, diag);
        return 2;
    }
    return 0;
}
