#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "trainlog/generation_mtp.h"

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "CHECK %s:%d: %s\n", __FILE__, __LINE__, #x);                          \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
typedef struct Object {
    uint32_t id, parent;
    bool folder;
    char name[256];
    char data[4096];
    size_t size;
} Object;
static Object objects[64];
static size_t object_count;
static unsigned int device_count = 1U;
static char send_order[1024];
static void add(uint32_t id, uint32_t parent, bool folder, const char *name, const char *data) {
    Object *o = &objects[object_count++];
    memset(o, 0, sizeof(*o));
    o->id = id;
    o->parent = parent;
    o->folder = folder;
    snprintf(o->name, sizeof(o->name), "%s", name);
    if (data) {
        o->size = strlen(data);
        memcpy(o->data, data, o->size);
    }
}
static void reset(void) {
    object_count = 0U;
    device_count = 1U;
    send_order[0] = '\0';
    add(1, UINT32_MAX, true, "Documents", NULL);
    add(2, 1, true, "Trainlog", NULL);
    add(3,
        2,
        false,
        "android-peer-v1.json",
        "{\"format\":\"trainlog-sync-peer\",\"version\":1,\"peer_id\":\"peer_11111111-1111-4111-"
        "8111-111111111111\",\"capabilities\":[\"generation-manifest-v1\",\"generation-ack-v1\"]}");
}
static TrainlogStatus devices(TrainlogUsbDevice *out, size_t cap, size_t *count) {
    size_t i;
    if (cap < device_count) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    *count = device_count;
    for (i = 0; i < device_count; i++) {
        memset(&out[i], 0, sizeof(out[i]));
        out[i].bus_number = 1U;
        out[i].device_number = (unsigned int)i + 1U;
    }
    return TRAINLOG_STATUS_OK;
}
static TrainlogStatus
storages(unsigned int bus, unsigned int dev, TrainlogMtpStorage *out, size_t cap, size_t *count) {
    (void)bus;
    if (cap < 1U) {
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    out->storage_id = dev;
    *count = 1U;
    return TRAINLOG_STATUS_OK;
}
static TrainlogStatus children(unsigned int bus,
                               unsigned int dev,
                               uint32_t storage,
                               uint32_t parent,
                               TrainlogMtpEntry *out,
                               size_t cap,
                               size_t *count) {
    size_t i, n = 0;
    (void)bus;
    (void)storage;
    for (i = 0; i < object_count; i++) {
        if (objects[i].parent == parent && (dev == 1U || objects[i].id <= 3U)) {
            if (n >= cap) {
                return TRAINLOG_STATUS_INVALID_ARGUMENT;
            }
            memset(&out[n], 0, sizeof(out[n]));
            out[n].item_id = objects[i].id;
            out[n].parent_id = parent;
            out[n].storage_id = dev;
            out[n].folder = objects[i].folder;
            out[n].size_bytes = objects[i].size;
            snprintf(out[n].name, sizeof(out[n].name), "%s", objects[i].name);
            n++;
        }
    }
    *count = n;
    return TRAINLOG_STATUS_OK;
}
static TrainlogStatus ensure_folder(unsigned int b,
                                    unsigned int d,
                                    uint32_t s,
                                    uint32_t p,
                                    const char *name,
                                    uint32_t *id,
                                    bool *created) {
    size_t i;
    (void)b;
    (void)d;
    (void)s;
    for (i = 0; i < object_count; i++) {
        if (objects[i].parent == p && objects[i].folder && strcmp(objects[i].name, name) == 0) {
            *id = objects[i].id;
            *created = false;
            return TRAINLOG_STATUS_OK;
        }
    }
    *id = (uint32_t)(100U + object_count);
    *created = true;
    add(*id, p, true, name, NULL);
    return TRAINLOG_STATUS_OK;
}
static Object *find(uint32_t id) {
    size_t i;
    for (i = 0; i < object_count; i++) {
        if (objects[i].id == id) {
            return &objects[i];
        }
    }
    return NULL;
}
static TrainlogStatus receive(unsigned int b, unsigned int d, uint32_t id, const char *path) {
    FILE *f;
    Object *o = find(id);
    size_t bytes;
    (void)b;
    (void)d;
    if (!o) {
        return TRAINLOG_STATUS_NOT_FOUND;
    }
    f = fopen(path, "wb");
    if (!f) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    bytes = id == 9U ? 3U : o->size;
    if (fwrite(o->data, 1, bytes, f) != bytes) {
        fclose(f);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    return fclose(f) == 0 ? TRAINLOG_STATUS_OK : TRAINLOG_STATUS_SYSTEM_ERROR;
}
static TrainlogStatus send_file(unsigned int b,
                                unsigned int d,
                                uint32_t s,
                                uint32_t p,
                                const char *path,
                                const char *name,
                                uint32_t *id) {
    FILE *f;
    Object *o;
    long size;
    (void)b;
    (void)d;
    (void)s;
    f = fopen(path, "rb");
    if (!f) {
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    rewind(f);
    if (size < 0 || size > (long)sizeof(objects[0].data)) {
        fclose(f);
        return TRAINLOG_STATUS_INVALID_ARGUMENT;
    }
    *id = (uint32_t)(100U + object_count);
    add(*id, p, false, name, NULL);
    o = &objects[object_count - 1];
    o->size = (size_t)size;
    if (fread(o->data, 1, o->size, f) != o->size) {
        fclose(f);
        return TRAINLOG_STATUS_SYSTEM_ERROR;
    }
    fclose(f);
    strncat(send_order, name, sizeof(send_order) - strlen(send_order) - 2U);
    strncat(send_order, "\n", sizeof(send_order) - strlen(send_order) - 1U);
    return TRAINLOG_STATUS_OK;
}
static TrainlogStatus remove_object(unsigned int b, unsigned int d, uint32_t id) {
    Object *o = find(id);
    (void)b;
    (void)d;
    if (!o) {
        return TRAINLOG_STATUS_NOT_FOUND;
    }
    o->name[0] = '\0';
    return TRAINLOG_STATUS_OK;
}
static TrainlogStatus rename_object(unsigned int b, unsigned int d, uint32_t id, const char *name) {
    Object *o = find(id);
    (void)b;
    (void)d;
    if (!o) {
        return TRAINLOG_STATUS_NOT_FOUND;
    }
    snprintf(o->name, sizeof(o->name), "%s", name);
    return TRAINLOG_STATUS_OK;
}
static const TrainlogGenerationMtpIo io = {
    .devices = devices,
    .storages = storages,
    .children = children,
    .ensure_folder = ensure_folder,
    .receive = receive,
    .send = send_file,
    .remove = remove_object,
    .rename = rename_object,
};
static int write_file(const char *path, const char *value) {
    FILE *f = fopen(path, "wb");
    CHECK(f != NULL);
    CHECK(fwrite(value, 1, strlen(value), f) == strlen(value));
    CHECK(fclose(f) == 0);
    return 0;
}
static int test_pull_and_manifest_last(void) {
    char root[] = "/tmp/trainlog-generation-mtp-XXXXXX", path[512], diag[256] = {0};
    char *manifest;
    reset();
    CHECK(mkdtemp(root) != NULL);
    add(4, 2, true, "android-objects", NULL);
    add(5, 4, true, "generations", NULL);
    add(6, 5, true, "gen_11111111-1111-4111-8111-111111111111", NULL);
    add(7, 6, false, "payload.json", "payload");
    add(8, 6, false, "manifest.json", "manifest");
    CHECK(trainlog_generation_mtp_pull(
              &io, "peer_11111111-1111-4111-8111-111111111111", root, diag, sizeof(diag)) ==
          TRAINLOG_STATUS_OK);
    snprintf(path,
             sizeof(path),
             "%s/android-objects/generations/gen_11111111-1111-4111-8111-111111111111/payload.json",
             root);
    CHECK(access(path, R_OK) == 0);
    snprintf(path, sizeof(path), "%s/out", root);
    CHECK(mkdir(path, 0700) == 0);
    char gen[512];
    snprintf(gen, sizeof(gen), "%s/generation", path);
    CHECK(mkdir(gen, 0700) == 0);
    char artifact[512];
    snprintf(artifact, sizeof(artifact), "%s/artifact.json", gen);
    CHECK(write_file(artifact, "a") == 0);
    char marker[512];
    snprintf(marker, sizeof(marker), "%s/manifest.json", gen);
    CHECK(write_file(marker, "m") == 0);
    CHECK(trainlog_generation_mtp_push(
              &io, "peer_11111111-1111-4111-8111-111111111111", root, diag, sizeof(diag)) ==
          TRAINLOG_STATUS_OK);
    manifest = strstr(send_order, "manifest.json\n");
    CHECK(manifest != NULL);
    CHECK(strstr(send_order, "artifact.json\n") < manifest);
    return 0;
}
static int test_ambiguity_and_truncation(void) {
    char root[] = "/tmp/trainlog-generation-mtp-error-XXXXXX", diag[256] = {0}, path[512];
    TrainlogStatus status;
    Object *peer;
    reset();
    CHECK(mkdtemp(root) != NULL);
    device_count = 2U;
    CHECK(trainlog_generation_mtp_pull(
              &io, "peer_11111111-1111-4111-8111-111111111111", root, diag, sizeof(diag)) ==
          TRAINLOG_STATUS_CONFLICT);
    reset();
    peer = find(3);
    CHECK(peer != NULL);
    snprintf(peer->data,
             sizeof(peer->data),
             "not-json containing peer_11111111-1111-4111-8111-111111111111 generation-manifest-v1 "
             "generation-ack-v1");
    peer->size = strlen(peer->data);
    CHECK(trainlog_generation_mtp_pull(
              &io, "peer_11111111-1111-4111-8111-111111111111", root, diag, sizeof(diag)) ==
          TRAINLOG_STATUS_NOT_FOUND);
    reset();
    add(9, 2, false, "bad.bin", "abc");
    find(9)->size = 99U;
    status = trainlog_generation_mtp_pull(
        &io, "peer_11111111-1111-4111-8111-111111111111", root, diag, sizeof(diag));
    CHECK(status != TRAINLOG_STATUS_OK);
    snprintf(path, sizeof(path), "%s/bad.bin", root);
    CHECK(access(path, F_OK) != 0);
    return 0;
}
int main(void) {
    CHECK(test_pull_and_manifest_last() == 0);
    CHECK(test_ambiguity_and_truncation() == 0);
    return 0;
}
