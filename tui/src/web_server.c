#include "trainlog/web_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <microhttpd.h>
#include <netinet/in.h>
#include <sys/random.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <yyjson.h>

#include "trainlog/web_dashboard.h"
#include "trainlog/web_analysis.h"
#include "trainlog/web_exercises.h"
#include "trainlog/web_prepared_items.h"
#include "trainlog/web_programs.h"
#include "trainlog/web_session_deletions.h"
#include "trainlog/web_sessions.h"
#include "trainlog/web_sleep.h"
#include "trainlog/dashboard_layout.h"
#include "trainlog/web_preferences.h"
#include "web_assets.h"

#define TRAINLOG_HTTP_CONNECTION_LIMIT 32U
#define TRAINLOG_HTTP_PER_IP_LIMIT 32U
#define TRAINLOG_HTTP_CONNECTION_MEMORY_LIMIT (16U * 1024U)
#define TRAINLOG_HTTP_HEADER_LIMIT (8U * 1024U)
#define TRAINLOG_HTTP_BODY_LIMIT TRAINLOG_WEB_PROGRAM_BYTES_MAX
#define TRAINLOG_HTTP_LEGACY_BODY_LIMIT (4U * 1024U)
#define TRAINLOG_HTTP_TIMEOUT_SECONDS 10U
#define TRAINLOG_HTTP_BACKLOG 32U
#define TRAINLOG_HTTP_POLL_MAX_MS 250ULL
#define TRAINLOG_SYNC_ACCEPTED_JSON_CAPACITY 256U
#define TRAINLOG_EXERCISE_REVISION_CAPACITY 72U

typedef struct TrainlogWebContext {
    uint16_t port;
    TrainlogDatabase *database;
    const char *database_path;
    char state_path[PATH_MAX + 32U];
    pid_t worker_pid;
    char csrf_token[65];
    bool invalid_layout_reported;
    bool invalid_preferences_reported;
} TrainlogWebContext;

typedef struct TrainlogHttpRequestState {
    size_t body_size;
    bool body_too_large;
    char body[TRAINLOG_HTTP_BODY_LIMIT + 1U];
} TrainlogHttpRequestState;

static bool valid_sync_id(const char *value) {
    return value != NULL && strlen(value) == 39U && strncmp(value, "sy_", 3U) == 0 &&
           uuid_parse(value + 3U, (unsigned char[16]){0}) == 0;
}

static bool valid_exercise_id(const char *value) {
    return value != NULL && strlen(value) == 39U && strncmp(value, "ex_", 3U) == 0 &&
           uuid_parse(value + 3U, (unsigned char[16]){0}) == 0;
}

typedef struct TrainlogAnalysisArguments {
    bool valid;
    unsigned int period_count;
    unsigned int exercise_count;
    unsigned int metric_count;
    unsigned int page_count;
} TrainlogAnalysisArguments;

static enum MHD_Result validate_analysis_argument(void *context,
                                                  enum MHD_ValueKind kind,
                                                  const char *key,
                                                  const char *value) {
    TrainlogAnalysisArguments *arguments = context;
    unsigned int *count = NULL;
    (void)kind;
    (void)value;
    if (strcmp(key, "period") == 0) {
        count = &arguments->period_count;
    } else if (strcmp(key, "exercise_id") == 0) {
        count = &arguments->exercise_count;
    } else if (strcmp(key, "metric") == 0) {
        count = &arguments->metric_count;
    } else if (strcmp(key, "page") == 0) {
        count = &arguments->page_count;
    } else {
        arguments->valid = false;
        return MHD_NO;
    }
    ++*count;
    if (*count > 1U) {
        arguments->valid = false;
        return MHD_NO;
    }
    return MHD_YES;
}

static bool parse_analysis_period(const char *value, TrainlogWebAnalysisPeriod *period) {
    if (value == NULL || strcmp(value, "30d") == 0) {
        *period = TRAINLOG_WEB_ANALYSIS_30_DAYS;
        return true;
    }
    if (strcmp(value, "7d") == 0) {
        *period = TRAINLOG_WEB_ANALYSIS_7_DAYS;
        return true;
    }
    if (strcmp(value, "14d") == 0) {
        *period = TRAINLOG_WEB_ANALYSIS_14_DAYS;
        return true;
    }
    if (strcmp(value, "21d") == 0) {
        *period = TRAINLOG_WEB_ANALYSIS_21_DAYS;
        return true;
    }
    if (strcmp(value, "90d") == 0) {
        *period = TRAINLOG_WEB_ANALYSIS_90_DAYS;
        return true;
    }
    if (strcmp(value, "all") == 0) {
        *period = TRAINLOG_WEB_ANALYSIS_ALL;
        return true;
    }
    return false;
}

static bool valid_measurement_metric(const char *value) {
    static const char *const VALUES[] = {"weight",
                                         "neck",
                                         "shoulders",
                                         "chest",
                                         "waist",
                                         "hips",
                                         "left_arm",
                                         "right_arm",
                                         "left_forearm",
                                         "right_forearm",
                                         "left_thigh",
                                         "right_thigh",
                                         "left_calf",
                                         "right_calf"};
    size_t index;
    for (index = 0U; index < sizeof(VALUES) / sizeof(VALUES[0]); ++index) {
        if (strcmp(value, VALUES[index]) == 0) {
            return true;
        }
    }
    return false;
}

bool trainlog_web_sync_accepted_serialize(const char *request_id,
                                          const char *run_id,
                                          char *output,
                                          size_t capacity,
                                          size_t *output_size) {
    int written;

    if (output == NULL || capacity == 0U || output_size == NULL) {
        return false;
    }
    output[0] = '\0';
    *output_size = 0U;
    if (!valid_sync_id(request_id) || !valid_sync_id(run_id)) {
        return false;
    }
    written = snprintf(output,
                       capacity,
                       "{\"api_version\":1,\"phase\":\"requested\",\"result\":\"running\","
                       "\"progress_revision\":1,\"request_id\":\"%s\",\"run_id\":\"%s\","
                       "\"status_url\":\"/api/v1/sync/status\"}\n",
                       request_id,
                       run_id);
    if (written < 0 || (size_t)written >= capacity) {
        output[0] = '\0';
        return false;
    }
    *output_size = (size_t)written;
    return true;
}

static void web_diagnostic(char *output, size_t capacity, const char *message) {
    if (output != NULL && capacity > 0U) {
        (void)snprintf(output, capacity, "%s", message);
    }
}

static enum MHD_Result queue_layout_json(struct MHD_Connection *connection,
                                         unsigned int status,
                                         const char *body,
                                         uint64_t revision,
                                         const char *csrf_token) {
    struct MHD_Response *response;
    enum MHD_Result result;
    char etag[32];
    response = MHD_create_response_from_buffer(strlen(body), (void *)body, MHD_RESPMEM_MUST_COPY);
    if (response == NULL) {
        return MHD_NO;
    }
    (void)snprintf(etag, sizeof(etag), "\"%llu\"", (unsigned long long)revision);
    if (MHD_add_response_header(
            response, MHD_HTTP_HEADER_CONTENT_TYPE, "application/json; charset=utf-8") != MHD_YES ||
        MHD_add_response_header(response, MHD_HTTP_HEADER_CACHE_CONTROL, "no-store") != MHD_YES ||
        MHD_add_response_header(response, MHD_HTTP_HEADER_X_CONTENT_TYPE_OPTIONS, "nosniff") !=
            MHD_YES ||
        MHD_add_response_header(response,
                                "Content-Security-Policy",
                                "default-src 'none'; frame-ancestors 'none'; base-uri 'none'") !=
            MHD_YES ||
        MHD_add_response_header(response, MHD_HTTP_HEADER_ETAG, etag) != MHD_YES ||
        MHD_add_response_header(response, "X-Trainlog-CSRF-Token", csrf_token) != MHD_YES ||
        MHD_add_response_header(response, MHD_HTTP_HEADER_CONNECTION, "close") != MHD_YES) {
        MHD_destroy_response(response);
        return MHD_NO;
    }
    result = MHD_queue_response(connection, status, response);
    MHD_destroy_response(response);
    return result;
}

static enum MHD_Result queue_json(struct MHD_Connection *connection,
                                  unsigned int status,
                                  const char *body,
                                  const char *allow) {
    struct MHD_Response *response;
    enum MHD_Result result;
    response = MHD_create_response_from_buffer(strlen(body), (void *)body, MHD_RESPMEM_MUST_COPY);
    if (response == NULL) {
        return MHD_NO;
    }
    if (MHD_add_response_header(
            response, MHD_HTTP_HEADER_CONTENT_TYPE, "application/json; charset=utf-8") != MHD_YES ||
        MHD_add_response_header(response, MHD_HTTP_HEADER_X_CONTENT_TYPE_OPTIONS, "nosniff") !=
            MHD_YES ||
        MHD_add_response_header(response, MHD_HTTP_HEADER_CACHE_CONTROL, "no-store") != MHD_YES ||
        MHD_add_response_header(response,
                                "Content-Security-Policy",
                                "default-src 'none'; frame-ancestors 'none'; base-uri 'none'") !=
            MHD_YES ||
        MHD_add_response_header(response, MHD_HTTP_HEADER_CONNECTION, "close") != MHD_YES) {
        MHD_destroy_response(response);
        return MHD_NO;
    }
    if (allow != NULL &&
        MHD_add_response_header(response, MHD_HTTP_HEADER_ALLOW, allow) != MHD_YES) {
        MHD_destroy_response(response);
        return MHD_NO;
    }
    result = MHD_queue_response(connection, status, response);
    MHD_destroy_response(response);
    return result;
}

static enum MHD_Result queue_sync_json_sized(struct MHD_Connection *connection,
                                             unsigned int status,
                                             const char *body,
                                             size_t body_size,
                                             const char *csrf_token,
                                             const char *location) {
    struct MHD_Response *response =
        MHD_create_response_from_buffer(body_size, (void *)body, MHD_RESPMEM_MUST_COPY);
    enum MHD_Result result;
    if (response == NULL) {
        return MHD_NO;
    }
    if (MHD_add_response_header(
            response, MHD_HTTP_HEADER_CONTENT_TYPE, "application/json; charset=utf-8") != MHD_YES ||
        MHD_add_response_header(response, MHD_HTTP_HEADER_CACHE_CONTROL, "no-store") != MHD_YES ||
        MHD_add_response_header(response, MHD_HTTP_HEADER_X_CONTENT_TYPE_OPTIONS, "nosniff") !=
            MHD_YES ||
        MHD_add_response_header(response,
                                "Content-Security-Policy",
                                "default-src 'none'; frame-ancestors 'none'; base-uri 'none'") !=
            MHD_YES ||
        MHD_add_response_header(response, "X-Trainlog-CSRF-Token", csrf_token) != MHD_YES ||
        (location != NULL &&
         MHD_add_response_header(response, MHD_HTTP_HEADER_LOCATION, location) != MHD_YES)) {
        MHD_destroy_response(response);
        return MHD_NO;
    }
    result = MHD_queue_response(connection, status, response);
    MHD_destroy_response(response);
    return result;
}

static enum MHD_Result queue_sync_json(struct MHD_Connection *connection,
                                       unsigned int status,
                                       const char *body,
                                       const char *csrf_token,
                                       const char *location) {
    return queue_sync_json_sized(connection, status, body, strlen(body), csrf_token, location);
}

static bool read_bounded_file(const char *path, char *output, size_t capacity) {
    int descriptor;
    struct stat info;
    size_t used = 0U;
    if (path == NULL || output == NULL || capacity < 2U) {
        return false;
    }
    descriptor = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor < 0) {
        return false;
    }
    if (fstat(descriptor, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0 ||
        (uintmax_t)info.st_size >= (uintmax_t)capacity) {
        (void)close(descriptor);
        return false;
    }
    while (used < (size_t)info.st_size) {
        ssize_t count = read(descriptor, output + used, (size_t)info.st_size - used);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            (void)close(descriptor);
            return false;
        }
        used += (size_t)count;
    }
    output[used] = '\0';
    (void)close(descriptor);
    return true;
}

static bool sync_state_active(const char *json, char request_id[64]) {
    yyjson_doc *document = yyjson_read(json, strlen(json), 0U);
    yyjson_val *root;
    yyjson_val *phase;
    yyjson_val *request;
    const char *text;
    bool active = false;
    if (document == NULL) {
        return false;
    }
    root = yyjson_doc_get_root(document);
    phase = yyjson_obj_get(root, "phase");
    request = yyjson_obj_get(root, "request_id");
    if (yyjson_is_str(request) && strlen(yyjson_get_str(request)) < 64U) {
        (void)snprintf(request_id, 64U, "%s", yyjson_get_str(request));
    }
    if (yyjson_is_str(phase)) {
        text = yyjson_get_str(phase);
        active = strcmp(text, "requested") == 0 ||
                 strcmp(text, "waiting_android_publication") == 0 || strcmp(text, "running") == 0 ||
                 strcmp(text, "local_import_committed") == 0 || strcmp(text, "published") == 0 ||
                 strcmp(text, "waiting_acknowledgement") == 0 || strcmp(text, "peer_consumed") == 0;
    }
    yyjson_doc_free(document);
    return active;
}

static bool valid_sync_request(const char *body, size_t size, char request_id[64]) {
    yyjson_doc *document = yyjson_read(body, size, 0U);
    yyjson_val *root;
    yyjson_val *request;
    yyjson_val *trigger;
    const char *id;
    if (document == NULL) {
        return false;
    }
    root = yyjson_doc_get_root(document);
    if (!yyjson_is_obj(root) || yyjson_obj_size(root) != 2U) {
        yyjson_doc_free(document);
        return false;
    }
    request = yyjson_obj_get(root, "request_id");
    trigger = yyjson_obj_get(root, "trigger");
    if (!yyjson_is_str(request) || !yyjson_is_str(trigger) ||
        strcmp(yyjson_get_str(trigger), "web") != 0) {
        yyjson_doc_free(document);
        return false;
    }
    id = yyjson_get_str(request);
    if (strlen(id) != 39U || strncmp(id, "sy_", 3U) != 0 ||
        uuid_parse(id + 3U, (unsigned char[16]){0}) != 0) {
        yyjson_doc_free(document);
        return false;
    }
    (void)snprintf(request_id, 64U, "%s", id);
    yyjson_doc_free(document);
    return true;
}

static void generate_sync_run_id(char run_id[64]) {
    uuid_t identifier;
    char uuid[37];
    uuid_generate_random(identifier);
    uuid_unparse_lower(identifier, uuid);
    (void)snprintf(run_id, 64U, "sy_%s", uuid);
}

static bool write_initial_sync_state(const TrainlogWebContext *context,
                                     const char *request_id,
                                     const char *run_id,
                                     char output[2048]) {
    char temporary[PATH_MAX + 64U];
    char timestamp[40];
    time_t now = time(NULL);
    struct tm broken_down;
    int descriptor;
    int written;
    ssize_t count;
    if (now == (time_t)-1 || gmtime_r(&now, &broken_down) == NULL ||
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &broken_down) == 0U) {
        return false;
    }
    written = snprintf(
        output,
        2048U,
        "{\"ai_midpoint\":{\"result\":\"not_configured\"},\"ai_post_sync\":{\"result\":\"not_"
        "configured\"},"
        "\"diagnostic\":\"\",\"domains\":{},\"drafts\":[],\"effective_mode\":\"full_generation_"
        "v1\","
        "\"finished_at\":null,\"inbound_generation_id\":null,\"missing_capabilities\":[],"
        "\"outbound_generation_id\":null,\"phase\":\"requested\",\"producer_peer_id\":null,"
        "\"consumer_peer_id\":null,\"progress_revision\":1,\"request_id\":\"%s\","
        "\"requested_mode\":\"full_generation_v1\",\"result\":\"running\",\"run_id\":\"%s\","
        "\"sessions_reconciled\":null,\"started_at\":\"%s\",\"trigger\":\"web\",\"updated_at\":\"%"
        "s\"}\n",
        request_id,
        run_id,
        timestamp,
        timestamp);
    if (written < 0 || written >= 2048) {
        return false;
    }
    if (snprintf(temporary, sizeof(temporary), "%s.new.%ld", context->state_path, (long)getpid()) <
        0) {
        return false;
    }
    descriptor = open(temporary, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (descriptor < 0) {
        return false;
    }
    count = write(descriptor, output, (size_t)written);
    if (count != written || fsync(descriptor) != 0 || close(descriptor) != 0 ||
        rename(temporary, context->state_path) != 0) {
        (void)close(descriptor);
        (void)unlink(temporary);
        return false;
    }
    return true;
}

static bool launch_sync_worker(TrainlogWebContext *context,
                               const char *config,
                               const char *run_id,
                               const char *request_id) {
    char orchestrator[PATH_MAX];
    const char *tools = getenv("TRAINLOG_SYNC_TOOLS_DIR");
    int path_length;
    if (tools == NULL) {
        tools = TRAINLOG_TOOLS_DIR;
    }
    /* CONTRACT: deployment may relocate the fixed shipped helper directory,
     * but an HTTP request can never select this executable or its arguments. */
    path_length = snprintf(orchestrator, sizeof(orchestrator), "%s/sync_orchestrator.py", tools);
    if (tools[0] != '/' || path_length < 0 || (size_t)path_length >= sizeof(orchestrator)) {
        return false;
    }
    pid_t child = fork();
    if (child < 0) {
        return false;
    }
    if (child == 0) {
        (void)setpgid(0, 0);
        execlp("python3",
               "python3",
               orchestrator,
               "--database",
               context->database_path,
               "--state",
               context->state_path,
               "--config",
               config,
               "--run-id",
               run_id,
               "--request-id",
               request_id,
               (char *)NULL);
        _exit(127);
    }
    context->worker_pid = child;
    return true;
}

static enum MHD_Result queue_asset(struct MHD_Connection *connection,
                                   const TrainlogWebAsset *asset) {
    static const char CSP[] = "default-src 'self'; script-src 'self'; "
                              "style-src 'self'; connect-src 'self'; img-src 'self'; "
                              "object-src 'none'; base-uri 'none'; frame-ancestors 'none'";
    const char *cache = asset->immutable ? "public, max-age=31536000, immutable" : "no-cache";
    struct MHD_Response *response =
        MHD_create_response_from_buffer(asset->size, (void *)asset->bytes, MHD_RESPMEM_PERSISTENT);
    enum MHD_Result result;
    if (response == NULL) {
        return MHD_NO;
    }
    if (MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, asset->mime) != MHD_YES ||
        MHD_add_response_header(response, MHD_HTTP_HEADER_X_CONTENT_TYPE_OPTIONS, "nosniff") !=
            MHD_YES ||
        MHD_add_response_header(response, MHD_HTTP_HEADER_CACHE_CONTROL, cache) != MHD_YES ||
        MHD_add_response_header(response, MHD_HTTP_HEADER_ETAG, asset->etag) != MHD_YES ||
        MHD_add_response_header(response, "Content-Security-Policy", CSP) != MHD_YES ||
        MHD_add_response_header(response, MHD_HTTP_HEADER_CONNECTION, "close") != MHD_YES) {
        MHD_destroy_response(response);
        return MHD_NO;
    }
    result = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    return result;
}

static bool is_ui_route(const char *url) {
    static const char *const routes[] = {"/", "/analyse", "/programmes", "/seances", "/exercices"};
    size_t index;
    for (index = 0U; index < sizeof(routes) / sizeof(routes[0]); ++index) {
        if (strcmp(url, routes[index]) == 0) {
            return true;
        }
    }
    /* CONTRACT: Sessions detail identities are client-side path segments. A
     * direct load must receive the SPA, while the /api guard below remains
     * authoritative for every adapter route. */
    if (strncmp(url, "/seances/", strlen("/seances/")) == 0) {
        return true;
    }
    return false;
}

static bool valid_host(const TrainlogWebContext *context, const char *host) {
    char expected[32];
    int written;
    if (host == NULL) {
        return false;
    }
    if (strcmp(host, "127.0.0.1") == 0) {
        return true;
    }
    written = snprintf(expected, sizeof(expected), "127.0.0.1:%u", (unsigned int)context->port);
    return written > 0 && (size_t)written < sizeof(expected) && strcmp(host, expected) == 0;
}

static bool declared_body_too_large(struct MHD_Connection *connection) {
    const char *value =
        MHD_lookup_connection_value(connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_CONTENT_LENGTH);
    char *end = NULL;
    unsigned long long size;
    if (value == NULL) {
        return false;
    }
    errno = 0;
    size = strtoull(value, &end, 10);
    return errno != 0 || end == value || *end != '\0' || size > TRAINLOG_HTTP_BODY_LIMIT;
}

static bool valid_origin(const TrainlogWebContext *context, const char *origin) {
    char direct[48];
    if (origin == NULL) {
        return false;
    }
    (void)snprintf(direct, sizeof(direct), "http://127.0.0.1:%u", (unsigned int)context->port);
    return strcmp(origin, direct) == 0 || strcmp(origin, "http://trainlog.perf") == 0;
}

static bool valid_csrf(const TrainlogWebContext *context, const char *provided) {
    size_t index;
    unsigned char difference = 0U;
    if (provided == NULL || strlen(provided) != 64U) {
        return false;
    }
    for (index = 0U; index < 64U; ++index) {
        difference |= (unsigned char)(provided[index] ^ context->csrf_token[index]);
    }
    return difference == 0U;
}

static bool generate_csrf_token(char output[65]) {
    unsigned char random_bytes[32];
    size_t used = 0U;
    size_t index;
    while (used < sizeof(random_bytes)) {
        ssize_t count = getrandom(random_bytes + used, sizeof(random_bytes) - used, 0);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            return false;
        }
        used += (size_t)count;
    }
    for (index = 0U; index < sizeof(random_bytes); ++index) {
        (void)snprintf(output + index * 2U, 3U, "%02x", random_bytes[index]);
    }
    return true;
}

static bool parse_if_match(const char *value, uint64_t *revision) {
    char *end = NULL;
    unsigned long long parsed;
    if (value == NULL || value[0] != '"') {
        return false;
    }
    errno = 0;
    parsed = strtoull(value + 1, &end, 10);
    if (errno != 0 || end == value + 1 || end[0] != '"' || end[1] != '\0' ||
        parsed > TRAINLOG_DASHBOARD_LAYOUT_MAX_REVISION) {
        return false;
    }
    *revision = (uint64_t)parsed;
    return true;
}

static bool
parse_page_number(const char *value, size_t default_value, size_t maximum, size_t *out) {
    char *end = NULL;
    unsigned long long parsed;

    if (value == NULL) {
        *out = default_value;
        return true;
    }
    errno = 0;
    parsed = strtoull(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed > maximum) {
        return false;
    }
    *out = (size_t)parsed;
    return true;
}

static enum MHD_Result queue_owned_sessions_json(struct MHD_Connection *connection,
                                                 TrainlogStatus status,
                                                 char *json,
                                                 size_t json_size) {
    enum MHD_Result result;

    if (status == TRAINLOG_STATUS_NOT_FOUND) {
        free(json);
        return queue_json(connection, MHD_HTTP_NOT_FOUND, "{\"error\":\"not_found\"}\n", NULL);
    }
    if (status != TRAINLOG_STATUS_OK || json == NULL || json_size == 0U) {
        free(json);
        return queue_json(connection,
                          MHD_HTTP_INTERNAL_SERVER_ERROR,
                          "{\"error\":\"sessions_unavailable\"}\n",
                          NULL);
    }
    (void)json_size;
    result = queue_json(connection, MHD_HTTP_OK, json, NULL);
    free(json);
    return result;
}

static enum MHD_Result queue_owned_programs_json(struct MHD_Connection *connection,
                                                 TrainlogStatus status,
                                                 char *json,
                                                 size_t json_size) {
    if (status == TRAINLOG_STATUS_NOT_FOUND) {
        free(json);
        return queue_json(connection, MHD_HTTP_NOT_FOUND, "{\"error\":\"not_found\"}\n", NULL);
    }
    if (status == TRAINLOG_STATUS_CONFLICT) {
        free(json);
        return queue_json(
            connection, MHD_HTTP_CONFLICT, "{\"error\":\"program_conflict\"}\n", NULL);
    }
    if (status == TRAINLOG_STATUS_INVALID_ARGUMENT) {
        free(json);
        return queue_json(
            connection, MHD_HTTP_UNPROCESSABLE_CONTENT, "{\"error\":\"invalid_program\"}\n", NULL);
    }
    return queue_owned_sessions_json(connection, status, json, json_size);
}

static bool program_mutation_allowed(TrainlogWebContext *context,
                                     struct MHD_Connection *connection) {
    const char *origin = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Origin");
    const char *csrf =
        MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Trainlog-CSRF-Token");
    const char *content_type =
        MHD_lookup_connection_value(connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_CONTENT_TYPE);

    return valid_origin(context, origin) && valid_csrf(context, csrf) && content_type != NULL &&
           strcmp(content_type, "application/json") == 0;
}

static enum MHD_Result
handle_program_list(TrainlogWebContext *context, struct MHD_Connection *connection, bool is_get) {
    const char *search;
    const char *state;
    char *json = NULL;
    size_t json_size = 0U;
    size_t offset;
    size_t limit;
    TrainlogStatus status;

    if (!is_get) {
        return queue_json(
            connection, MHD_HTTP_METHOD_NOT_ALLOWED, "{\"error\":\"method_not_allowed\"}\n", "GET");
    }
    search = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "search");
    state = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "state");
    if (!parse_page_number(MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "offset"),
                           0U,
                           1000000U,
                           &offset) ||
        !parse_page_number(MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "limit"),
                           24U,
                           TRAINLOG_WEB_SESSIONS_PAGE_MAX,
                           &limit) ||
        limit == 0U) {
        return queue_json(connection, MHD_HTTP_BAD_REQUEST, "{\"error\":\"invalid_page\"}\n", NULL);
    }
    status = trainlog_web_programs_list_json(
        context->database, search, state, offset, limit, &json, &json_size);
    return queue_owned_programs_json(connection, status, json, json_size);
}

static enum MHD_Result handle_program_import(TrainlogWebContext *context,
                                             struct MHD_Connection *connection,
                                             const char *url,
                                             const char *method,
                                             TrainlogHttpRequestState *request) {
    char *json = NULL;
    size_t json_size = 0U;
    bool commit = strcmp(url, "/api/v1/sessions/programs/import") == 0;
    TrainlogStatus status;

    if (strcmp(method, MHD_HTTP_METHOD_POST) != 0) {
        return queue_json(connection,
                          MHD_HTTP_METHOD_NOT_ALLOWED,
                          "{\"error\":\"method_not_allowed\"}\n",
                          "POST");
    }
    if (!program_mutation_allowed(context, connection)) {
        return queue_json(
            connection, MHD_HTTP_FORBIDDEN, "{\"error\":\"mutation_forbidden\"}\n", NULL);
    }
    status = trainlog_web_programs_import_json(
        context->database, request->body, request->body_size, commit, &json, &json_size);
    return queue_owned_programs_json(connection, status, json, json_size);
}

static bool
copy_path_component(char *output, size_t output_capacity, const char *input, size_t input_length) {
    if (input_length == 0U || input_length >= output_capacity) {
        return false;
    }
    (void)memcpy(output, input, input_length);
    output[input_length] = '\0';
    return true;
}

static bool parse_quoted_revision(const char *header, char *output, size_t output_capacity) {
    size_t length = header == NULL ? 0U : strlen(header);

    if (length < 3U || header[0] != '"' || header[length - 1U] != '"') {
        return false;
    }
    return copy_path_component(output, output_capacity, header + 1, length - 2U);
}

static enum MHD_Result handle_program_archive(TrainlogWebContext *context,
                                              struct MHD_Connection *connection,
                                              const char *identity,
                                              size_t identity_length,
                                              const char *method) {
    static const char SUFFIX[] = "/archive";
    const char *request_id =
        MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Trainlog-Request-ID");
    const char *if_match =
        MHD_lookup_connection_value(connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_IF_MATCH);
    char program_id[TRAINLOG_ID_MAX + 1U];
    char revision[TRAINLOG_ID_MAX + 1U];
    char *json = NULL;
    size_t json_size = 0U;
    size_t program_length = identity_length - strlen(SUFFIX);
    TrainlogStatus status;

    if (strcmp(method, MHD_HTTP_METHOD_POST) != 0) {
        return queue_json(connection,
                          MHD_HTTP_METHOD_NOT_ALLOWED,
                          "{\"error\":\"method_not_allowed\"}\n",
                          "POST");
    }
    if (!program_mutation_allowed(context, connection) || request_id == NULL ||
        request_id[0] == '\0' ||
        !copy_path_component(program_id, sizeof(program_id), identity, program_length) ||
        !parse_quoted_revision(if_match, revision, sizeof(revision))) {
        return queue_json(connection,
                          MHD_HTTP_PRECONDITION_REQUIRED,
                          "{\"error\":\"precondition_required\"}\n",
                          NULL);
    }
    status = trainlog_web_programs_archive_json(
        context->database, program_id, revision, request_id, &json, &json_size);
    return queue_owned_programs_json(connection, status, json, json_size);
}

static enum MHD_Result handle_program_delete(TrainlogWebContext *context,
                                             struct MHD_Connection *connection,
                                             const char *identity,
                                             size_t identity_length,
                                             const char *method) {
    static const char SUFFIX[] = "/delete";
    const char *request_id =
        MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Trainlog-Request-ID");
    const char *if_match =
        MHD_lookup_connection_value(connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_IF_MATCH);
    char program_id[TRAINLOG_ID_MAX + 1U];
    char revision[TRAINLOG_ID_MAX + 1U];
    char *json = NULL;
    size_t json_size = 0U;
    size_t program_length = identity_length - strlen(SUFFIX);
    TrainlogStatus status;

    if (strcmp(method, MHD_HTTP_METHOD_POST) != 0) {
        return queue_json(connection,
                          MHD_HTTP_METHOD_NOT_ALLOWED,
                          "{\"error\":\"method_not_allowed\"}\n",
                          "POST");
    }
    if (!program_mutation_allowed(context, connection)) {
        return queue_json(
            connection, MHD_HTTP_FORBIDDEN, "{\"error\":\"mutation_forbidden\"}\n", NULL);
    }
    if (request_id == NULL || request_id[0] == '\0' || strlen(request_id) > 128U ||
        !copy_path_component(program_id, sizeof(program_id), identity, program_length) ||
        !parse_quoted_revision(if_match, revision, sizeof(revision))) {
        return queue_json(connection,
                          MHD_HTTP_PRECONDITION_REQUIRED,
                          "{\"error\":\"precondition_required\"}\n",
                          NULL);
    }
    status = trainlog_web_programs_delete_json(
        context->database, program_id, revision, request_id, &json, &json_size);
    return queue_owned_programs_json(connection, status, json, json_size);
}

static enum MHD_Result handle_program_prepare(TrainlogWebContext *context,
                                              struct MHD_Connection *connection,
                                              const char *identity,
                                              const char *sessions_marker,
                                              const char *method) {
    static const char SUFFIX[] = "/prepare";
    const char *request_id =
        MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Trainlog-Request-ID");
    const char *session_id = sessions_marker + strlen("/sessions/");
    size_t session_length = strlen(session_id);
    char program_id[TRAINLOG_ID_MAX + 1U];
    char program_session_id[TRAINLOG_ID_MAX + 1U];
    char *json = NULL;
    size_t json_size = 0U;
    TrainlogStatus status;

    if (session_length <= strlen(SUFFIX) ||
        strcmp(session_id + session_length - strlen(SUFFIX), SUFFIX) != 0 ||
        strcmp(method, MHD_HTTP_METHOD_POST) != 0) {
        return queue_json(connection,
                          MHD_HTTP_METHOD_NOT_ALLOWED,
                          "{\"error\":\"method_not_allowed\"}\n",
                          "POST");
    }
    session_length -= strlen(SUFFIX);
    if (!program_mutation_allowed(context, connection) || request_id == NULL ||
        request_id[0] == '\0' ||
        !copy_path_component(
            program_id, sizeof(program_id), identity, (size_t)(sessions_marker - identity)) ||
        !copy_path_component(
            program_session_id, sizeof(program_session_id), session_id, session_length)) {
        return queue_json(
            connection, MHD_HTTP_FORBIDDEN, "{\"error\":\"mutation_forbidden\"}\n", NULL);
    }
    status = trainlog_web_programs_prepare_json(
        context->database, program_id, program_session_id, request_id, &json, &json_size);
    return queue_owned_programs_json(connection, status, json, json_size);
}

static enum MHD_Result handle_program_request(TrainlogWebContext *context,
                                              struct MHD_Connection *connection,
                                              const char *url,
                                              const char *method,
                                              bool is_get,
                                              TrainlogHttpRequestState *request) {
    static const char PREFIX[] = "/api/v1/sessions/program/";
    static const char ARCHIVE_SUFFIX[] = "/archive";
    static const char DELETE_SUFFIX[] = "/delete";
    const char *identity;
    const char *sessions_marker;
    char *json = NULL;
    size_t json_size = 0U;
    size_t identity_length;
    TrainlogStatus status;

    if (strcmp(url, "/api/v1/sessions/programs") == 0) {
        return handle_program_list(context, connection, is_get);
    }
    if (strcmp(url, "/api/v1/sessions/programs/validate") == 0 ||
        strcmp(url, "/api/v1/sessions/programs/import") == 0) {
        return handle_program_import(context, connection, url, method, request);
    }
    if (strncmp(url, PREFIX, strlen(PREFIX)) != 0) {
        return queue_json(connection, MHD_HTTP_NOT_FOUND, "{\"error\":\"not_found\"}\n", NULL);
    }
    identity = url + strlen(PREFIX);
    identity_length = strlen(identity);
    sessions_marker = strstr(identity, "/sessions/");
    if (identity_length > strlen(ARCHIVE_SUFFIX) &&
        strcmp(identity + identity_length - strlen(ARCHIVE_SUFFIX), ARCHIVE_SUFFIX) == 0) {
        return handle_program_archive(context, connection, identity, identity_length, method);
    }
    if (identity_length > strlen(DELETE_SUFFIX) &&
        strcmp(identity + identity_length - strlen(DELETE_SUFFIX), DELETE_SUFFIX) == 0) {
        return handle_program_delete(context, connection, identity, identity_length, method);
    }
    if (sessions_marker != NULL) {
        return handle_program_prepare(context, connection, identity, sessions_marker, method);
    }
    if (!is_get || identity_length == 0U || identity_length > TRAINLOG_ID_MAX) {
        return queue_json(
            connection, MHD_HTTP_METHOD_NOT_ALLOWED, "{\"error\":\"method_not_allowed\"}\n", "GET");
    }
    status = trainlog_web_programs_detail_json(context->database, identity, &json, &json_size);
    if (status == TRAINLOG_STATUS_CONFLICT) {
        free(json);
        return queue_json(connection, MHD_HTTP_GONE, "{\"error\":\"program_deleted\"}\n", NULL);
    }
    return queue_owned_programs_json(connection, status, json, json_size);
}

static enum MHD_Result queue_owned_exercises_json(struct MHD_Connection *connection,
                                                  TrainlogStatus status,
                                                  char *json,
                                                  size_t json_size,
                                                  const char *conflict_error) {
    if (status == TRAINLOG_STATUS_NOT_FOUND) {
        free(json);
        return queue_json(
            connection, MHD_HTTP_NOT_FOUND, "{\"error\":\"exercise_not_found\"}\n", NULL);
    }
    if (status == TRAINLOG_STATUS_CONFLICT) {
        free(json);
        return queue_json(connection,
                          MHD_HTTP_CONFLICT,
                          conflict_error == NULL ? "{\"error\":\"exercise_conflict\"}\n"
                                                 : conflict_error,
                          NULL);
    }
    if (status == TRAINLOG_STATUS_INVALID_ARGUMENT) {
        free(json);
        return queue_json(
            connection, MHD_HTTP_UNPROCESSABLE_CONTENT, "{\"error\":\"invalid_exercise\"}\n", NULL);
    }
    if (status != TRAINLOG_STATUS_OK || json == NULL || json_size == 0U) {
        free(json);
        return queue_json(connection,
                          MHD_HTTP_INTERNAL_SERVER_ERROR,
                          "{\"error\":\"exercises_unavailable\"}\n",
                          NULL);
    }
    (void)json_size;
    {
        enum MHD_Result result = queue_json(connection, MHD_HTTP_OK, json, NULL);
        free(json);
        return result;
    }
}

static enum MHD_Result handle_exercises_request(TrainlogWebContext *context,
                                                struct MHD_Connection *connection,
                                                const char *url,
                                                const char *method,
                                                bool is_get,
                                                TrainlogHttpRequestState *request) {
    static const char COLLECTION[] = "/api/v1/exercises";
    static const char DETAIL_PREFIX[] = "/api/v1/exercise/";
    char *json = NULL;
    size_t json_size = 0U;
    const char *request_id;
    TrainlogStatus status;

    if (strcmp(url, "/api/v1/exercise-zones") == 0) {
        if (!is_get) {
            return queue_json(connection,
                              MHD_HTTP_METHOD_NOT_ALLOWED,
                              "{\"error\":\"method_not_allowed\"}\n",
                              "GET");
        }
        status = trainlog_web_exercises_zones_json(&json, &json_size);
        return queue_owned_exercises_json(connection, status, json, json_size, NULL);
    }
    if (strcmp(url, COLLECTION) == 0) {
        if (is_get) {
            TrainlogWebExercisesQuery query;
            const char *unclassified;
            if (!parse_page_number(
                    MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "offset"),
                    0U,
                    1000000U,
                    &query.offset) ||
                !parse_page_number(
                    MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "limit"),
                    24U,
                    TRAINLOG_WEB_EXERCISES_PAGE_MAX,
                    &query.limit) ||
                query.limit == 0U) {
                return queue_json(
                    connection, MHD_HTTP_BAD_REQUEST, "{\"error\":\"invalid_page\"}\n", NULL);
            }
            query.search = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "search");
            query.profile =
                MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "profile");
            query.zone_id =
                MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "zone_id");
            unclassified =
                MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "unclassified");
            query.unclassified = unclassified != NULL && strcmp(unclassified, "true") == 0;
            status = trainlog_web_exercises_list_json(context->database, &query, &json, &json_size);
            return queue_owned_exercises_json(connection, status, json, json_size, NULL);
        }
        if (strcmp(method, MHD_HTTP_METHOD_POST) != 0) {
            return queue_json(connection,
                              MHD_HTTP_METHOD_NOT_ALLOWED,
                              "{\"error\":\"method_not_allowed\"}\n",
                              "GET, POST");
        }
        if (!program_mutation_allowed(context, connection)) {
            return queue_json(
                connection, MHD_HTTP_FORBIDDEN, "{\"error\":\"mutation_forbidden\"}\n", NULL);
        }
        request_id =
            MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Trainlog-Request-ID");
        if (request_id == NULL || request_id[0] == '\0' || strlen(request_id) > 128U) {
            return queue_json(
                connection, MHD_HTTP_BAD_REQUEST, "{\"error\":\"invalid_request_id\"}\n", NULL);
        }
        status = trainlog_web_exercises_create_json(
            context->database, request->body, request->body_size, &json, &json_size);
        return queue_owned_exercises_json(connection, status, json, json_size, NULL);
    }
    if (strncmp(url, DETAIL_PREFIX, strlen(DETAIL_PREFIX)) == 0) {
        const char *exercise_id = url + strlen(DETAIL_PREFIX);
        const char *if_match;
        char revision[TRAINLOG_EXERCISE_REVISION_CAPACITY];
        if (exercise_id[0] == '\0' || strlen(exercise_id) > TRAINLOG_ID_MAX ||
            strchr(exercise_id, '/') != NULL) {
            return queue_json(
                connection, MHD_HTTP_BAD_REQUEST, "{\"error\":\"invalid_identity\"}\n", NULL);
        }
        if (is_get) {
            status = trainlog_web_exercises_detail_json(
                context->database, exercise_id, &json, &json_size);
            return queue_owned_exercises_json(connection, status, json, json_size, NULL);
        }
        if (strcmp(method, MHD_HTTP_METHOD_PUT) != 0 &&
            strcmp(method, MHD_HTTP_METHOD_DELETE) != 0) {
            return queue_json(connection,
                              MHD_HTTP_METHOD_NOT_ALLOWED,
                              "{\"error\":\"method_not_allowed\"}\n",
                              "GET, PUT, DELETE");
        }
        if (!program_mutation_allowed(context, connection)) {
            return queue_json(
                connection, MHD_HTTP_FORBIDDEN, "{\"error\":\"mutation_forbidden\"}\n", NULL);
        }
        request_id =
            MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Trainlog-Request-ID");
        if (request_id == NULL || request_id[0] == '\0' || strlen(request_id) > 128U) {
            return queue_json(
                connection, MHD_HTTP_BAD_REQUEST, "{\"error\":\"invalid_request_id\"}\n", NULL);
        }
        if_match =
            MHD_lookup_connection_value(connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_IF_MATCH);
        if (!parse_quoted_revision(if_match, revision, sizeof(revision))) {
            return queue_json(connection,
                              MHD_HTTP_PRECONDITION_REQUIRED,
                              "{\"error\":\"precondition_required\"}\n",
                              NULL);
        }
        if (strcmp(method, MHD_HTTP_METHOD_PUT) == 0) {
            status = trainlog_web_exercises_update_json(context->database,
                                                        exercise_id,
                                                        revision,
                                                        request->body,
                                                        request->body_size,
                                                        &json,
                                                        &json_size);
            return queue_owned_exercises_json(connection,
                                              status,
                                              json,
                                              json_size,
                                              "{\"error\":\"exercise_revision_conflict\"}\n");
        }
        if (request->body_size != 0U) {
            return queue_json(
                connection, MHD_HTTP_BAD_REQUEST, "{\"error\":\"unexpected_body\"}\n", NULL);
        }
        status = trainlog_web_exercises_retire_json(
            context->database, exercise_id, revision, &json, &json_size);
        if (status == TRAINLOG_STATUS_INVALID_ARGUMENT) {
            free(json);
            return queue_json(connection,
                              MHD_HTTP_CONFLICT,
                              "{\"error\":\"exercise_retirement_forbidden\"}\n",
                              NULL);
        }
        return queue_owned_exercises_json(
            connection, status, json, json_size, "{\"error\":\"exercise_revision_conflict\"}\n");
    }
    return queue_json(connection, MHD_HTTP_NOT_FOUND, "{\"error\":\"not_found\"}\n", NULL);
}

static enum MHD_Result queue_session_deletion_result(struct MHD_Connection *connection,
                                                     TrainlogStatus status,
                                                     char *json,
                                                     size_t json_size) {
    if (status == TRAINLOG_STATUS_CONFLICT) {
        free(json);
        return queue_json(
            connection, MHD_HTTP_CONFLICT, "{\"error\":\"deletion_conflict\"}\n", NULL);
    }
    if (status == TRAINLOG_STATUS_NOT_FOUND) {
        free(json);
        return queue_json(connection, MHD_HTTP_NOT_FOUND, "{\"error\":\"not_found\"}\n", NULL);
    }
    if (status == TRAINLOG_STATUS_INVALID_ARGUMENT) {
        free(json);
        return queue_json(
            connection, MHD_HTTP_UNPROCESSABLE_CONTENT, "{\"error\":\"invalid_deletion\"}\n", NULL);
    }
    return queue_owned_sessions_json(connection, status, json, json_size);
}

static enum MHD_Result handle_session_deletion(TrainlogWebContext *context,
                                               struct MHD_Connection *connection,
                                               const char *url,
                                               const char *method) {
    static const char PREFIX[] = "/api/v1/sessions/";
    const char *request_id =
        MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Trainlog-Request-ID");
    const char *if_match =
        MHD_lookup_connection_value(connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_IF_MATCH);
    const char *kind = url + strlen(PREFIX);
    const char *separator = strchr(kind, '/');
    char kind_buffer[32];
    char identity[TRAINLOG_ID_MAX + 1U];
    char revision[TRAINLOG_ID_MAX + 1U];
    char *json = NULL;
    size_t json_size = 0U;
    TrainlogStatus status;

    if (strcmp(method, MHD_HTTP_METHOD_DELETE) != 0) {
        return queue_json(connection,
                          MHD_HTTP_METHOD_NOT_ALLOWED,
                          "{\"error\":\"method_not_allowed\"}\n",
                          "DELETE");
    }
    if (separator == NULL || separator[1] == '\0' ||
        !copy_path_component(kind_buffer, sizeof(kind_buffer), kind, (size_t)(separator - kind)) ||
        !copy_path_component(identity, sizeof(identity), separator + 1, strlen(separator + 1))) {
        return queue_json(
            connection, MHD_HTTP_BAD_REQUEST, "{\"error\":\"invalid_identity\"}\n", NULL);
    }
    if (strcmp(kind_buffer, "proposal") != 0 && strcmp(kind_buffer, "draft") != 0 &&
        strcmp(kind_buffer, "history") != 0) {
        return queue_json(connection, MHD_HTTP_NOT_FOUND, "{\"error\":\"not_found\"}\n", NULL);
    }
    if (!program_mutation_allowed(context, connection) || request_id == NULL ||
        request_id[0] == '\0' || strlen(request_id) > 128U) {
        return queue_json(
            connection, MHD_HTTP_FORBIDDEN, "{\"error\":\"mutation_forbidden\"}\n", NULL);
    }
    if (!parse_quoted_revision(if_match, revision, sizeof(revision))) {
        return queue_json(connection,
                          MHD_HTTP_PRECONDITION_REQUIRED,
                          "{\"error\":\"precondition_required\"}\n",
                          NULL);
    }
    status = trainlog_web_session_delete_json(
        context->database, kind_buffer, identity, revision, request_id, &json, &json_size);
    return queue_session_deletion_result(connection, status, json, json_size);
}

static enum MHD_Result handle_request(void *closure,
                                      struct MHD_Connection *connection,
                                      const char *url,
                                      const char *method,
                                      const char *version,
                                      const char *upload_data,
                                      size_t *upload_data_size,
                                      void **request_closure) {
    static const char HEALTH[] = "{\"api_version\":1,\"status\":\"ok\",\"product\":\"trainlog\","
                                 "\"version\":\"" TRAINLOG_VERSION "\"}\n";
    TrainlogWebContext *context = closure;
    TrainlogHttpRequestState *state = *request_closure;
    const union MHD_ConnectionInfo *header_info;
    const TrainlogWebAsset *asset;
    const char *host;
    bool is_get;
    bool is_head;
    (void)version;
    if (state == NULL) {
        state = calloc(1U, sizeof(*state));
        if (state == NULL) {
            return MHD_NO;
        }
        state->body_too_large = declared_body_too_large(connection);
        *request_closure = state;
        return MHD_YES;
    }
    if (*upload_data_size > 0U) {
        if (*upload_data_size >
            TRAINLOG_HTTP_BODY_LIMIT - (state->body_size > TRAINLOG_HTTP_BODY_LIMIT
                                            ? TRAINLOG_HTTP_BODY_LIMIT
                                            : state->body_size)) {
            state->body_too_large = true;
        } else {
            (void)memcpy(state->body + state->body_size, upload_data, *upload_data_size);
            state->body_size += *upload_data_size;
            state->body[state->body_size] = '\0';
        }
        *upload_data_size = 0U;
        return MHD_YES;
    }
    header_info = MHD_get_connection_info(connection, MHD_CONNECTION_INFO_REQUEST_HEADER_SIZE);
    if (header_info == NULL || header_info->header_size > TRAINLOG_HTTP_HEADER_LIMIT) {
        return queue_json(connection,
                          MHD_HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE,
                          "{\"error\":\"headers_too_large\"}\n",
                          NULL);
    }
    if (state->body_too_large ||
        (strncmp(url, "/api/v1/sessions/", strlen("/api/v1/sessions/")) != 0 &&
         state->body_size > TRAINLOG_HTTP_LEGACY_BODY_LIMIT)) {
        return queue_json(
            connection, MHD_HTTP_CONTENT_TOO_LARGE, "{\"error\":\"body_too_large\"}\n", NULL);
    }
    host = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_HOST);
    if (!valid_host(context, host)) {
        return queue_json(connection, MHD_HTTP_BAD_REQUEST, "{\"error\":\"invalid_host\"}\n", NULL);
    }
    is_get = strcmp(method, MHD_HTTP_METHOD_GET) == 0;
    is_head = strcmp(method, MHD_HTTP_METHOD_HEAD) == 0;
    if (strcmp(url, "/api/v1/health") == 0) {
        if (!is_get) {
            return queue_json(connection,
                              MHD_HTTP_METHOD_NOT_ALLOWED,
                              "{\"error\":\"method_not_allowed\"}\n",
                              "GET");
        }
        return queue_json(connection, MHD_HTTP_OK, HEALTH, NULL);
    }
    if (strcmp(url, "/api/v1/sync/status") == 0) {
        static const char IDLE_DISABLED[] =
            "{\"api_version\":1,\"enabled\":false,\"phase\":\"idle\","
            "\"result\":\"disabled\",\"last_success\":null}\n";
        static const char IDLE_ENABLED[] = "{\"api_version\":1,\"enabled\":true,\"phase\":\"idle\","
                                           "\"result\":\"idle\",\"last_success\":null}\n";
        char status_json[65537];
        if (!is_get) {
            return queue_json(connection,
                              MHD_HTTP_METHOD_NOT_ALLOWED,
                              "{\"error\":\"method_not_allowed\"}\n",
                              "GET");
        }
        if (!read_bounded_file(context->state_path, status_json, sizeof(status_json))) {
            return queue_sync_json(connection,
                                   MHD_HTTP_OK,
                                   getenv("TRAINLOG_SYNC_GENERATION_CONFIG") == NULL ? IDLE_DISABLED
                                                                                     : IDLE_ENABLED,
                                   context->csrf_token,
                                   NULL);
        }
        return queue_sync_json(connection, MHD_HTTP_OK, status_json, context->csrf_token, NULL);
    }
    if (strcmp(url, "/api/v1/sync") == 0) {
        const char *origin;
        const char *csrf;
        const char *content_type;
        const char *config = getenv("TRAINLOG_SYNC_GENERATION_CONFIG");
        char request_id[64] = "";
        char existing_id[64] = "";
        char existing[65537];
        char initial[2048];
        char run_id[64];
        char accepted[TRAINLOG_SYNC_ACCEPTED_JSON_CAPACITY];
        size_t accepted_size;
        if (strcmp(method, MHD_HTTP_METHOD_POST) != 0) {
            return queue_json(connection,
                              MHD_HTTP_METHOD_NOT_ALLOWED,
                              "{\"error\":\"method_not_allowed\"}\n",
                              "POST");
        }
        origin = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Origin");
        csrf = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Trainlog-CSRF-Token");
        if (!valid_origin(context, origin) || !valid_csrf(context, csrf)) {
            return queue_json(
                connection, MHD_HTTP_FORBIDDEN, "{\"error\":\"mutation_forbidden\"}\n", NULL);
        }
        content_type =
            MHD_lookup_connection_value(connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_CONTENT_TYPE);
        if (content_type == NULL || strcmp(content_type, "application/json") != 0) {
            return queue_json(connection,
                              MHD_HTTP_UNSUPPORTED_MEDIA_TYPE,
                              "{\"error\":\"unsupported_media_type\"}\n",
                              NULL);
        }
        if (!valid_sync_request(state->body, state->body_size, request_id)) {
            return queue_json(
                connection, MHD_HTTP_BAD_REQUEST, "{\"error\":\"invalid_sync_request\"}\n", NULL);
        }
        if (config == NULL || config[0] != '/' || strlen(config) >= PATH_MAX) {
            return queue_json(
                connection, MHD_HTTP_CONFLICT, "{\"error\":\"full_generation_disabled\"}\n", NULL);
        }
        if (read_bounded_file(context->state_path, existing, sizeof(existing))) {
            bool active = sync_state_active(existing, existing_id);
            if (strcmp(existing_id, request_id) == 0) {
                return queue_sync_json(connection,
                                       active ? MHD_HTTP_ACCEPTED : MHD_HTTP_OK,
                                       existing,
                                       context->csrf_token,
                                       "/api/v1/sync/status");
            }
            if (active) {
                return queue_json(
                    connection, MHD_HTTP_CONFLICT, "{\"error\":\"sync_already_running\"}\n", NULL);
            }
        }
        generate_sync_run_id(run_id);
        /* INVARIANT: a request is made durable only after its complete 202 response is known to
         * fit, so no admitted job can become unreportable because of response truncation. */
        if (!trainlog_web_sync_accepted_serialize(
                request_id, run_id, accepted, sizeof(accepted), &accepted_size) ||
            !write_initial_sync_state(context, request_id, run_id, initial) ||
            !launch_sync_worker(context, config, run_id, request_id)) {
            return queue_json(connection,
                              MHD_HTTP_INTERNAL_SERVER_ERROR,
                              "{\"error\":\"sync_worker_start_failed\"}\n",
                              NULL);
        }
        return queue_sync_json_sized(connection,
                                     MHD_HTTP_ACCEPTED,
                                     accepted,
                                     accepted_size,
                                     context->csrf_token,
                                     "/api/v1/sync/status");
    }
    if (strcmp(url, "/api/v1/analysis") == 0) {
        TrainlogAnalysisArguments arguments = {.valid = true};
        TrainlogWebAnalysisQuery query;
        const char *period;
        const char *exercise_id;
        const char *metric;
        const char *page;
        char json[TRAINLOG_WEB_ANALYSIS_JSON_CAPACITY];
        size_t json_size = 0U;
        time_t now;
        if (!is_get) {
            return queue_json(connection,
                              MHD_HTTP_METHOD_NOT_ALLOWED,
                              "{\"error\":\"method_not_allowed\"}\n",
                              "GET");
        }
        (void)MHD_get_connection_values(
            connection, MHD_GET_ARGUMENT_KIND, validate_analysis_argument, &arguments);
        period = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "period");
        exercise_id = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "exercise_id");
        metric = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "metric");
        page = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "page");
        if (!arguments.valid || !parse_analysis_period(period, &query.period) ||
            (exercise_id != NULL && !valid_exercise_id(exercise_id)) ||
            (metric != NULL && !valid_measurement_metric(metric)) ||
            (page != NULL && strcmp(page, "1") != 0)) {
            return queue_json(
                connection, MHD_HTTP_BAD_REQUEST, "{\"error\":\"invalid_analysis_query\"}\n", NULL);
        }
        now = time(NULL);
        if (now == (time_t)-1 || (int64_t)now != (int64_t)(time_t)now) {
            return queue_json(connection,
                              MHD_HTTP_INTERNAL_SERVER_ERROR,
                              "{\"error\":\"clock_unavailable\"}\n",
                              NULL);
        }
        query.reference_unix_second = (int64_t)now;
        query.exercise_id = exercise_id;
        query.measurement_metric = metric == NULL ? "weight" : metric;
        if (trainlog_web_analysis_json(context->database, &query, json, sizeof(json), &json_size) !=
                TRAINLOG_STATUS_OK ||
            json_size == 0U) {
            return queue_json(connection,
                              MHD_HTTP_INTERNAL_SERVER_ERROR,
                              "{\"error\":\"analysis_unavailable\"}\n",
                              NULL);
        }
        return queue_json(connection, MHD_HTTP_OK, json, NULL);
    }
    if (strcmp(url, "/api/v1/sleep-diary") == 0) {
        char *json = NULL;
        size_t json_size = 0U;
        TrainlogStatus status;
        enum MHD_Result queued;
        if (is_get) {
            const char *start =
                MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "start_date");
            const char *end =
                MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "end_date");
            size_t limit;
            if (!parse_page_number(
                    MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "limit"),
                    90U,
                    3660U,
                    &limit) ||
                limit == 0U) {
                return queue_json(connection,
                                  MHD_HTTP_BAD_REQUEST,
                                  "{\"error\":\"invalid_sleep_query\"}\n",
                                  NULL);
            }
            status = trainlog_web_sleep_list_json(
                context->database, start, end, limit, &json, &json_size);
        } else if (strcmp(method, MHD_HTTP_METHOD_POST) == 0 ||
                   strcmp(method, MHD_HTTP_METHOD_DELETE) == 0) {
            if (!program_mutation_allowed(context, connection)) {
                return queue_json(
                    connection, MHD_HTTP_FORBIDDEN, "{\"error\":\"mutation_forbidden\"}\n", NULL);
            }
            status = strcmp(method, MHD_HTTP_METHOD_POST) == 0
                         ? trainlog_web_sleep_save_json(
                               context->database, state->body, state->body_size, &json, &json_size)
                         : trainlog_web_sleep_delete_request_json(
                               context->database, state->body, state->body_size, &json, &json_size);
        } else {
            return queue_json(connection,
                              MHD_HTTP_METHOD_NOT_ALLOWED,
                              "{\"error\":\"method_not_allowed\"}\n",
                              "GET, POST, DELETE");
        }
        if (status == TRAINLOG_STATUS_CONFLICT) {
            free(json);
            return queue_json(
                connection, MHD_HTTP_CONFLICT, "{\"error\":\"revision_conflict\"}\n", NULL);
        }
        if (status == TRAINLOG_STATUS_INVALID_ARGUMENT) {
            free(json);
            return queue_json(connection,
                              MHD_HTTP_UNPROCESSABLE_CONTENT,
                              "{\"error\":\"invalid_sleep_diary\"}\n",
                              NULL);
        }
        if (status != TRAINLOG_STATUS_OK || json == NULL || json_size == 0U) {
            free(json);
            return queue_json(connection,
                              MHD_HTTP_INTERNAL_SERVER_ERROR,
                              "{\"error\":\"sleep_diary_unavailable\"}\n",
                              NULL);
        }
        queued = queue_json(connection, MHD_HTTP_OK, json, NULL);
        free(json);
        return queued;
    }
    if (strcmp(url, "/api/v1/sleep-medications") == 0) {
        char *json = NULL;
        size_t json_size = 0U;
        TrainlogStatus status;
        enum MHD_Result queued;
        if (is_get) {
            status = trainlog_web_medication_list_json(context->database, &json, &json_size);
        } else if (strcmp(method, MHD_HTTP_METHOD_POST) == 0) {
            if (!program_mutation_allowed(context, connection)) {
                return queue_json(
                    connection, MHD_HTTP_FORBIDDEN, "{\"error\":\"mutation_forbidden\"}\n", NULL);
            }
            status = trainlog_web_medication_save_json(
                context->database, state->body, state->body_size, &json, &json_size);
        } else {
            return queue_json(connection,
                              MHD_HTTP_METHOD_NOT_ALLOWED,
                              "{\"error\":\"method_not_allowed\"}\n",
                              "GET, POST");
        }
        if (status == TRAINLOG_STATUS_CONFLICT) {
            free(json);
            return queue_json(
                connection, MHD_HTTP_CONFLICT, "{\"error\":\"revision_conflict\"}\n", NULL);
        }
        if (status == TRAINLOG_STATUS_INVALID_ARGUMENT) {
            free(json);
            return queue_json(connection,
                              MHD_HTTP_UNPROCESSABLE_CONTENT,
                              "{\"error\":\"invalid_sleep_medication\"}\n",
                              NULL);
        }
        if (status != TRAINLOG_STATUS_OK || json == NULL || json_size == 0U) {
            free(json);
            return queue_json(connection,
                              MHD_HTTP_INTERNAL_SERVER_ERROR,
                              "{\"error\":\"sleep_medications_unavailable\"}\n",
                              NULL);
        }
        queued = queue_json(connection, MHD_HTTP_OK, json, NULL);
        free(json);
        return queued;
    }
    if (strcmp(url, "/api/v1/dashboard") == 0) {
        TrainlogWebDashboardQuery query;
        TrainlogWebDashboardSnapshot snapshot;
        char json[TRAINLOG_WEB_JSON_CAPACITY];
        size_t json_size;
        time_t now;
        if (!is_get) {
            return queue_json(connection,
                              MHD_HTTP_METHOD_NOT_ALLOWED,
                              "{\"error\":\"method_not_allowed\"}\n",
                              "GET");
        }
        now = time(NULL);
        if (now == (time_t)-1 || (int64_t)now != (int64_t)(time_t)now) {
            return queue_json(connection,
                              MHD_HTTP_INTERNAL_SERVER_ERROR,
                              "{\"error\":\"clock_unavailable\"}\n",
                              NULL);
        }
        query.reference_unix_second = (int64_t)now;
        if (trainlog_web_dashboard_load(context->database, &query, &snapshot) !=
                TRAINLOG_STATUS_OK ||
            trainlog_web_dashboard_serialize(&snapshot, json, sizeof(json), &json_size) !=
                TRAINLOG_STATUS_OK ||
            json_size == 0U) {
            return queue_json(connection,
                              MHD_HTTP_INTERNAL_SERVER_ERROR,
                              "{\"error\":\"dashboard_unavailable\"}\n",
                              NULL);
        }
        return queue_json(connection, MHD_HTTP_OK, json, NULL);
    }
    if (strcmp(url, "/api/v1/prepared-items") == 0) {
        char json[TRAINLOG_WEB_PREPARED_ITEMS_JSON_CAPACITY];
        size_t json_size = 0U;
        TrainlogWebPreparedItems items;

        if (!is_get) {
            return queue_json(connection,
                              MHD_HTTP_METHOD_NOT_ALLOWED,
                              "{\"error\":\"method_not_allowed\"}\n",
                              "GET");
        }
        if (trainlog_web_prepared_items_load(context->database, &items) != TRAINLOG_STATUS_OK ||
            trainlog_web_prepared_items_serialize(&items, json, sizeof(json), &json_size) !=
                TRAINLOG_STATUS_OK) {
            return queue_json(connection,
                              MHD_HTTP_INTERNAL_SERVER_ERROR,
                              "{\"error\":\"prepared_items_unavailable\"}\n",
                              NULL);
        }
        return queue_json(connection, MHD_HTTP_OK, json, NULL);
    }
    if (strncmp(url, "/api/v1/exercise", strlen("/api/v1/exercise")) == 0) {
        return handle_exercises_request(context, connection, url, method, is_get, state);
    }
    if (strncmp(url, "/api/v1/sessions/program", strlen("/api/v1/sessions/program")) == 0) {
        return handle_program_request(context, connection, url, method, is_get, state);
    }
    if (strncmp(url, "/api/v1/sessions", strlen("/api/v1/sessions")) == 0) {
        static const struct {
            const char *path;
            TrainlogWebSessionCollection collection;
        } collections[] = {
            {"/api/v1/sessions/preparations", TRAINLOG_WEB_SESSION_PREPARATIONS},
            {"/api/v1/sessions/proposals", TRAINLOG_WEB_SESSION_PROPOSALS},
            {"/api/v1/sessions/drafts", TRAINLOG_WEB_SESSION_DRAFTS},
            {"/api/v1/sessions/history", TRAINLOG_WEB_SESSION_HISTORY},
        };
        const char *offset_value;
        const char *limit_value;
        const char *search;
        char *json = NULL;
        size_t json_size = 0U;
        size_t offset;
        size_t limit;
        size_t index;

        if (strcmp(method, MHD_HTTP_METHOD_DELETE) == 0 &&
            strncmp(url,
                    "/api/v1/sessions/preparation/",
                    strlen("/api/v1/sessions/preparation/")) != 0) {
            return handle_session_deletion(context, connection, url, method);
        }

        if (!is_get && (strcmp(url, "/api/v1/sessions/preparations") == 0 ||
                        strncmp(url,
                                "/api/v1/sessions/preparation/",
                                strlen("/api/v1/sessions/preparation/")) == 0)) {
            const char *origin = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Origin");
            const char *csrf =
                MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Trainlog-CSRF-Token");
            const char *content_type = MHD_lookup_connection_value(
                connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_CONTENT_TYPE);
            const char *request_id =
                MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Trainlog-Request-ID");
            const char *if_match =
                MHD_lookup_connection_value(connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_IF_MATCH);
            const char *identity = NULL;
            const char *expected = "";
            char expected_buffer[TRAINLOG_ID_MAX + 1U];
            char identity_buffer[TRAINLOG_ID_MAX + 1U];
            bool delivery = false;
            bool withdrawal = false;
            TrainlogStatus status;

            if ((strcmp(url, "/api/v1/sessions/preparations") == 0 &&
                 strcmp(method, MHD_HTTP_METHOD_POST) != 0) ||
                (strcmp(url, "/api/v1/sessions/preparations") != 0 &&
                 strcmp(method, MHD_HTTP_METHOD_PUT) != 0 &&
                 strcmp(method, MHD_HTTP_METHOD_POST) != 0 &&
                 strcmp(method, MHD_HTTP_METHOD_DELETE) != 0)) {
                return queue_json(connection,
                                  MHD_HTTP_METHOD_NOT_ALLOWED,
                                  "{\"error\":\"method_not_allowed\"}\n",
                                  "POST, PUT, DELETE");
            }
            if (!valid_origin(context, origin) || !valid_csrf(context, csrf)) {
                return queue_json(
                    connection, MHD_HTTP_FORBIDDEN, "{\"error\":\"mutation_forbidden\"}\n", NULL);
            }
            if (content_type == NULL || strcmp(content_type, "application/json") != 0 ||
                request_id == NULL || request_id[0] == '\0' || strlen(request_id) > 128U) {
                return queue_json(connection,
                                  MHD_HTTP_BAD_REQUEST,
                                  "{\"error\":\"invalid_mutation_headers\"}\n",
                                  NULL);
            }
            if (strcmp(url, "/api/v1/sessions/preparations") != 0) {
                size_t length;
                identity = url + strlen("/api/v1/sessions/preparation/");
                length = strlen(identity);
                if (length > strlen("/deliver") &&
                    strcmp(identity + length - strlen("/deliver"), "/deliver") == 0) {
                    size_t identity_length = length - strlen("/deliver");
                    if (identity_length == 0U || identity_length > TRAINLOG_ID_MAX) {
                        return queue_json(connection,
                                          MHD_HTTP_BAD_REQUEST,
                                          "{\"error\":\"invalid_identity\"}\n",
                                          NULL);
                    }
                    (void)memcpy(identity_buffer, identity, identity_length);
                    identity_buffer[identity_length] = '\0';
                    identity = identity_buffer;
                    delivery = true;
                }
                withdrawal = !delivery && strcmp(method, MHD_HTTP_METHOD_DELETE) == 0;
                if ((delivery && strcmp(method, MHD_HTTP_METHOD_POST) != 0) ||
                    (!delivery && !withdrawal && strcmp(method, MHD_HTTP_METHOD_PUT) != 0)) {
                    return queue_json(connection,
                                      MHD_HTTP_METHOD_NOT_ALLOWED,
                                      "{\"error\":\"method_not_allowed\"}\n",
                                      delivery ? "POST" : "PUT, DELETE");
                }
                if (if_match == NULL || if_match[0] != '"') {
                    return queue_json(connection,
                                      MHD_HTTP_PRECONDITION_REQUIRED,
                                      "{\"error\":\"precondition_required\"}\n",
                                      NULL);
                }
                length = strlen(if_match);
                if (length < 3U || length - 2U > TRAINLOG_ID_MAX || if_match[length - 1U] != '"') {
                    return queue_json(connection,
                                      MHD_HTTP_BAD_REQUEST,
                                      "{\"error\":\"invalid_revision\"}\n",
                                      NULL);
                }
                (void)memcpy(expected_buffer, if_match + 1, length - 2U);
                expected_buffer[length - 2U] = '\0';
                expected = expected_buffer;
            }
            if (delivery) {
                status = trainlog_web_sessions_deliver_json(
                    context->database, identity, expected, request_id, &json, &json_size);
            } else if (withdrawal) {
                status = trainlog_web_sessions_withdraw_json(
                    context->database, identity, expected, request_id, &json, &json_size);
            } else {
                status = trainlog_web_sessions_save_json(context->database,
                                                         identity,
                                                         expected,
                                                         request_id,
                                                         state->body,
                                                         state->body_size,
                                                         &json,
                                                         &json_size);
            }
            if (status == TRAINLOG_STATUS_CONFLICT) {
                free(json);
                return queue_json(
                    connection, MHD_HTTP_CONFLICT, "{\"error\":\"revision_conflict\"}\n", NULL);
            }
            if (status == TRAINLOG_STATUS_NOT_FOUND) {
                free(json);
                return queue_json(
                    connection, MHD_HTTP_NOT_FOUND, "{\"error\":\"not_found\"}\n", NULL);
            }
            if (status == TRAINLOG_STATUS_INVALID_ARGUMENT) {
                free(json);
                return queue_json(connection,
                                  MHD_HTTP_UNPROCESSABLE_CONTENT,
                                  "{\"error\":\"invalid_preparation\"}\n",
                                  NULL);
            }
            return queue_owned_sessions_json(connection, status, json, json_size);
        }
        if (!is_get) {
            return queue_json(connection,
                              MHD_HTTP_METHOD_NOT_ALLOWED,
                              "{\"error\":\"method_not_allowed\"}\n",
                              "GET");
        }
        offset_value = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "offset");
        limit_value = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "limit");
        search = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "search");
        if (!parse_page_number(offset_value, 0U, 1000000U, &offset) ||
            !parse_page_number(limit_value, 24U, TRAINLOG_WEB_SESSIONS_PAGE_MAX, &limit) ||
            limit == 0U) {
            return queue_json(
                connection, MHD_HTTP_BAD_REQUEST, "{\"error\":\"invalid_page\"}\n", NULL);
        }
        if (strcmp(url, "/api/v1/sessions/catalog") == 0) {
            TrainlogStatus status = trainlog_web_sessions_catalog_json(
                context->database, search, offset, limit, &json, &json_size);
            return queue_owned_sessions_json(connection, status, json, json_size);
        }
        for (index = 0U; index < sizeof(collections) / sizeof(collections[0]); ++index) {
            if (strcmp(url, collections[index].path) == 0) {
                TrainlogWebSessionsPageQuery query = {
                    collections[index].collection, offset, limit, search};
                TrainlogStatus status =
                    trainlog_web_sessions_list_json(context->database, &query, &json, &json_size);
                return queue_owned_sessions_json(connection, status, json, json_size);
            }
        }
        {
            static const char DETAIL_PREFIX[] = "/api/v1/sessions/";
            const char *kind = url + strlen(DETAIL_PREFIX);
            const char *separator = strchr(kind, '/');
            char kind_buffer[32];

            if (separator != NULL && separator[1] != '\0' &&
                (size_t)(separator - kind) < sizeof(kind_buffer)) {
                TrainlogStatus status;

                (void)memcpy(kind_buffer, kind, (size_t)(separator - kind));
                kind_buffer[separator - kind] = '\0';
                status = trainlog_web_sessions_detail_json(
                    context->database, kind_buffer, separator + 1, &json, &json_size);
                return queue_owned_sessions_json(connection, status, json, json_size);
            }
        }
        return queue_json(connection, MHD_HTTP_NOT_FOUND, "{\"error\":\"not_found\"}\n", NULL);
    }
    if (strcmp(url, "/api/v1/web-preferences") == 0) {
        TrainlogWebPreferences preferences;
        TrainlogWebPreferences saved;
        TrainlogWebPreferencesSource source;
        TrainlogWebPreferencesResult preferences_result;
        char json[TRAINLOG_WEB_PREFERENCES_JSON_CAPACITY];
        size_t json_size;
        uint64_t expected;
        const char *origin;
        const char *csrf;
        const char *if_match;

        if (is_get) {
            preferences_result = trainlog_web_preferences_load(&preferences, &source);
            if (preferences_result != TRAINLOG_WEB_PREFERENCES_OK ||
                !trainlog_web_preferences_serialize(
                    &preferences, source, true, json, sizeof(json), &json_size)) {
                return queue_json(connection,
                                  MHD_HTTP_INTERNAL_SERVER_ERROR,
                                  "{\"error\":\"preferences_unavailable\"}\n",
                                  NULL);
            }
            if (source == TRAINLOG_WEB_PREFERENCES_INVALID_PERSISTED &&
                !context->invalid_preferences_reported) {
                (void)fprintf(stderr,
                              "Trainlog Web : préférences persistées invalides, défaut utilisé\n");
                context->invalid_preferences_reported = true;
            }
            return queue_layout_json(
                connection, MHD_HTTP_OK, json, preferences.revision, context->csrf_token);
        }
        if (strcmp(method, MHD_HTTP_METHOD_PUT) != 0) {
            return queue_json(connection,
                              MHD_HTTP_METHOD_NOT_ALLOWED,
                              "{\"error\":\"method_not_allowed\"}\n",
                              "GET, PUT");
        }
        origin = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Origin");
        csrf = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Trainlog-CSRF-Token");
        if (!valid_origin(context, origin) || !valid_csrf(context, csrf)) {
            return queue_json(
                connection, MHD_HTTP_FORBIDDEN, "{\"error\":\"mutation_forbidden\"}\n", NULL);
        }
        if_match =
            MHD_lookup_connection_value(connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_IF_MATCH);
        if (!parse_if_match(if_match, &expected)) {
            return queue_json(connection,
                              MHD_HTTP_PRECONDITION_REQUIRED,
                              "{\"error\":\"precondition_required\"}\n",
                              NULL);
        }
        {
            const char *content_type = MHD_lookup_connection_value(
                connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_CONTENT_TYPE);
            if (content_type == NULL || strcmp(content_type, "application/json") != 0) {
                return queue_json(connection,
                                  MHD_HTTP_UNSUPPORTED_MEDIA_TYPE,
                                  "{\"error\":\"unsupported_media_type\"}\n",
                                  NULL);
            }
        }
        preferences_result =
            trainlog_web_preferences_parse(state->body, state->body_size, &preferences);
        if (preferences_result != TRAINLOG_WEB_PREFERENCES_OK || preferences.revision != expected) {
            return queue_json(
                connection, MHD_HTTP_BAD_REQUEST, "{\"error\":\"invalid_preferences\"}\n", NULL);
        }
        preferences_result = trainlog_web_preferences_save(&preferences, expected, &saved);
        if (preferences_result == TRAINLOG_WEB_PREFERENCES_CONFLICT) {
            return queue_json(connection,
                              MHD_HTTP_PRECONDITION_FAILED,
                              "{\"error\":\"revision_conflict\"}\n",
                              NULL);
        }
        if (preferences_result != TRAINLOG_WEB_PREFERENCES_OK ||
            !trainlog_web_preferences_serialize(
                &saved, TRAINLOG_WEB_PREFERENCES_PERSISTED, true, json, sizeof(json), &json_size)) {
            return queue_json(connection,
                              MHD_HTTP_INTERNAL_SERVER_ERROR,
                              "{\"error\":\"preferences_save_failed\"}\n",
                              NULL);
        }
        return queue_layout_json(
            connection, MHD_HTTP_OK, json, saved.revision, context->csrf_token);
    }
    if (strcmp(url, "/api/v1/dashboard-layout") == 0) {
        TrainlogDashboardLayout layout;
        TrainlogDashboardLayout saved;
        TrainlogDashboardLayoutSource source;
        TrainlogDashboardLayoutResult layout_result;
        char json[TRAINLOG_DASHBOARD_LAYOUT_JSON_CAPACITY];
        size_t json_size;
        uint64_t expected;
        const char *origin;
        const char *csrf;
        const char *if_match;
        /* WHY: loopback alone does not stop a hostile webpage from submitting
         * requests to a local service. CONTRACT: every mutation needs exact
         * trusted Origin, the startup-random header token and an If-Match ETag.
         * INVARIANT: proxy Host rewriting remains strict and no CORS is added. */
        if (is_get) {
            layout_result = trainlog_dashboard_layout_load(&layout, &source);
            if (layout_result != TRAINLOG_DASHBOARD_LAYOUT_OK ||
                !trainlog_dashboard_layout_serialize(
                    &layout, source, true, json, sizeof(json), &json_size) ||
                json_size == 0U) {
                return queue_json(connection,
                                  MHD_HTTP_INTERNAL_SERVER_ERROR,
                                  "{\"error\":\"layout_unavailable\"}\n",
                                  NULL);
            }
            if (source == TRAINLOG_DASHBOARD_LAYOUT_INVALID_PERSISTED &&
                !context->invalid_layout_reported) {
                (void)fprintf(stderr,
                              "Trainlog Web : agencement persisté invalide, défaut utilisé\n");
                context->invalid_layout_reported = true;
            }
            return queue_layout_json(
                connection, MHD_HTTP_OK, json, layout.revision, context->csrf_token);
        }
        if (strcmp(method, MHD_HTTP_METHOD_PUT) != 0 &&
            strcmp(method, MHD_HTTP_METHOD_DELETE) != 0) {
            return queue_json(connection,
                              MHD_HTTP_METHOD_NOT_ALLOWED,
                              "{\"error\":\"method_not_allowed\"}\n",
                              "GET, PUT, DELETE");
        }
        origin = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Origin");
        csrf = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Trainlog-CSRF-Token");
        if (!valid_origin(context, origin) || !valid_csrf(context, csrf)) {
            return queue_json(
                connection, MHD_HTTP_FORBIDDEN, "{\"error\":\"mutation_forbidden\"}\n", NULL);
        }
        if_match =
            MHD_lookup_connection_value(connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_IF_MATCH);
        if (!parse_if_match(if_match, &expected)) {
            return queue_json(connection,
                              MHD_HTTP_PRECONDITION_REQUIRED,
                              "{\"error\":\"precondition_required\"}\n",
                              NULL);
        }
        if (strcmp(method, MHD_HTTP_METHOD_DELETE) == 0) {
            if (state->body_size != 0U) {
                return queue_json(
                    connection, MHD_HTTP_BAD_REQUEST, "{\"error\":\"unexpected_body\"}\n", NULL);
            }
            layout_result = trainlog_dashboard_layout_delete(expected);
            if (layout_result == TRAINLOG_DASHBOARD_LAYOUT_CONFLICT) {
                return queue_json(connection,
                                  MHD_HTTP_PRECONDITION_FAILED,
                                  "{\"error\":\"revision_conflict\"}\n",
                                  NULL);
            }
            if (layout_result != TRAINLOG_DASHBOARD_LAYOUT_OK) {
                return queue_json(connection,
                                  MHD_HTTP_INTERNAL_SERVER_ERROR,
                                  "{\"error\":\"layout_delete_failed\"}\n",
                                  NULL);
            }
            trainlog_dashboard_layout_default(&layout);
            if (!trainlog_dashboard_layout_serialize(&layout,
                                                     TRAINLOG_DASHBOARD_LAYOUT_DEFAULT,
                                                     true,
                                                     json,
                                                     sizeof(json),
                                                     &json_size)) {
                return queue_json(connection,
                                  MHD_HTTP_INTERNAL_SERVER_ERROR,
                                  "{\"error\":\"layout_unavailable\"}\n",
                                  NULL);
            }
            return queue_layout_json(connection, MHD_HTTP_OK, json, 0U, context->csrf_token);
        }
        {
            const char *content_type = MHD_lookup_connection_value(
                connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_CONTENT_TYPE);
            if (content_type == NULL || strcmp(content_type, "application/json") != 0) {
                return queue_json(connection,
                                  MHD_HTTP_UNSUPPORTED_MEDIA_TYPE,
                                  "{\"error\":\"unsupported_media_type\"}\n",
                                  NULL);
            }
        }
        layout_result = trainlog_dashboard_layout_parse(state->body, state->body_size, &layout);
        if (layout_result != TRAINLOG_DASHBOARD_LAYOUT_OK || layout.revision != expected) {
            return queue_json(
                connection, MHD_HTTP_BAD_REQUEST, "{\"error\":\"invalid_layout\"}\n", NULL);
        }
        layout_result = trainlog_dashboard_layout_save(&layout, expected, &saved);
        if (layout_result == TRAINLOG_DASHBOARD_LAYOUT_CONFLICT) {
            return queue_json(connection,
                              MHD_HTTP_PRECONDITION_FAILED,
                              "{\"error\":\"revision_conflict\"}\n",
                              NULL);
        }
        if (layout_result != TRAINLOG_DASHBOARD_LAYOUT_OK ||
            !trainlog_dashboard_layout_serialize(&saved,
                                                 TRAINLOG_DASHBOARD_LAYOUT_PERSISTED,
                                                 true,
                                                 json,
                                                 sizeof(json),
                                                 &json_size)) {
            return queue_json(connection,
                              MHD_HTTP_INTERNAL_SERVER_ERROR,
                              "{\"error\":\"layout_save_failed\"}\n",
                              NULL);
        }
        return queue_layout_json(
            connection, MHD_HTTP_OK, json, saved.revision, context->csrf_token);
    }
    /* INVARIANT: API paths never fall through to the SPA index. */
    if (strncmp(url, "/api/", 5U) == 0) {
        return queue_json(connection, MHD_HTTP_NOT_FOUND, "{\"error\":\"not_found\"}\n", NULL);
    }
    if (strstr(url, "..") != NULL) {
        return queue_json(connection, MHD_HTTP_NOT_FOUND, "{\"error\":\"not_found\"}\n", NULL);
    }
    asset = is_ui_route(url) ? trainlog_web_index_asset() : trainlog_web_asset_find(url);
    if (asset == NULL) {
        return queue_json(connection, MHD_HTTP_NOT_FOUND, "{\"error\":\"not_found\"}\n", NULL);
    }
    if (!is_get && !is_head) {
        return queue_json(connection,
                          MHD_HTTP_METHOD_NOT_ALLOWED,
                          "{\"error\":\"method_not_allowed\"}\n",
                          "GET, HEAD");
    }
    return queue_asset(connection, asset);
}

static void request_completed(void *closure,
                              struct MHD_Connection *connection,
                              void **request_closure,
                              enum MHD_RequestTerminationCode termination) {
    (void)closure;
    (void)connection;
    (void)termination;
    free(*request_closure);
    *request_closure = NULL;
}

static enum MHD_Result
accept_loopback_only(void *closure, const struct sockaddr *address, socklen_t address_length) {
    const struct sockaddr_in *ipv4;
    (void)closure;
    if (address == NULL || address_length < (socklen_t)sizeof(*ipv4) ||
        address->sa_family != AF_INET) {
        return MHD_NO;
    }
    ipv4 = (const struct sockaddr_in *)address;
    return ipv4->sin_addr.s_addr == htonl(INADDR_LOOPBACK) ? MHD_YES : MHD_NO;
}

int trainlog_web_server_run(TrainlogDatabase *database,
                            const char *database_path,
                            uint16_t port,
                            const volatile sig_atomic_t *stop_requested,
                            char *diagnostic,
                            size_t diagnostic_capacity) {
    struct sockaddr_in bind_address;
    struct MHD_Daemon *daemon;
    TrainlogWebContext context;
    int result = 0;
    if (database == NULL || database_path == NULL || database_path[0] != '/' ||
        stop_requested == NULL) {
        return -1;
    }
    web_diagnostic(diagnostic, diagnostic_capacity, "");
    if (!trainlog_web_assets_available()) {
        web_diagnostic(diagnostic, diagnostic_capacity, "support frontend absent de ce binaire");
        return -1;
    }
    (void)memset(&bind_address, 0, sizeof(bind_address));
    bind_address.sin_family = AF_INET;
    bind_address.sin_port = htons(port);
    bind_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    context.port = port;
    context.database = database;
    context.database_path = database_path;
    context.worker_pid = -1;
    if (snprintf(
            context.state_path, sizeof(context.state_path), "%s.sync-run.json", database_path) <
            0 ||
        strlen(context.state_path) >= sizeof(context.state_path) - 1U) {
        web_diagnostic(diagnostic, diagnostic_capacity, "chemin d'état sync trop long");
        return -1;
    }
    context.invalid_layout_reported = false;
    context.invalid_preferences_reported = false;
    if (!generate_csrf_token(context.csrf_token)) {
        web_diagnostic(diagnostic, diagnostic_capacity, "impossible de générer la protection CSRF");
        return -1;
    }
    errno = 0;
    daemon = MHD_start_daemon(MHD_USE_ERROR_LOG | MHD_USE_NO_THREAD_SAFETY,
                              0U,
                              accept_loopback_only,
                              NULL,
                              handle_request,
                              &context,
                              MHD_OPTION_SOCK_ADDR_LEN,
                              (socklen_t)sizeof(bind_address),
                              &bind_address,
                              MHD_OPTION_CONNECTION_LIMIT,
                              TRAINLOG_HTTP_CONNECTION_LIMIT,
                              MHD_OPTION_PER_IP_CONNECTION_LIMIT,
                              TRAINLOG_HTTP_PER_IP_LIMIT,
                              MHD_OPTION_CONNECTION_MEMORY_LIMIT,
                              (size_t)TRAINLOG_HTTP_CONNECTION_MEMORY_LIMIT,
                              MHD_OPTION_CONNECTION_TIMEOUT,
                              TRAINLOG_HTTP_TIMEOUT_SECONDS,
                              MHD_OPTION_LISTEN_BACKLOG_SIZE,
                              TRAINLOG_HTTP_BACKLOG,
                              MHD_OPTION_NOTIFY_COMPLETED,
                              request_completed,
                              NULL,
                              MHD_OPTION_END);
    if (daemon == NULL) {
        char message[256];
        (void)snprintf(message,
                       sizeof(message),
                       "impossible d'écouter sur 127.0.0.1:%u%s%s",
                       (unsigned int)port,
                       errno == 0 ? "" : " : ",
                       errno == 0 ? "" : strerror(errno));
        web_diagnostic(diagnostic, diagnostic_capacity, message);
        return -1;
    }
    (void)printf("Trainlog Web : http://127.0.0.1:%u/\n", (unsigned int)port);
    (void)fflush(stdout);
    while (*stop_requested == 0) {
        fd_set read_set;
        fd_set write_set;
        fd_set except_set;
        struct timeval timeout;
        unsigned long long timeout_ms = TRAINLOG_HTTP_POLL_MAX_MS;
        int maximum_fd = 0;
        int selected;
        if (context.worker_pid > 0) {
            int worker_status;
            if (waitpid(context.worker_pid, &worker_status, WNOHANG) == context.worker_pid) {
                context.worker_pid = -1;
            }
        }
        FD_ZERO(&read_set);
        FD_ZERO(&write_set);
        FD_ZERO(&except_set);
        if (MHD_get_fdset(daemon, &read_set, &write_set, &except_set, &maximum_fd) != MHD_YES) {
            web_diagnostic(
                diagnostic, diagnostic_capacity, "impossible de préparer la boucle HTTP");
            result = -1;
            break;
        }
        if (MHD_get_timeout(daemon, &timeout_ms) != MHD_YES ||
            timeout_ms > TRAINLOG_HTTP_POLL_MAX_MS) {
            timeout_ms = TRAINLOG_HTTP_POLL_MAX_MS;
        }
        timeout.tv_sec = (time_t)(timeout_ms / 1000ULL);
        timeout.tv_usec = (suseconds_t)((timeout_ms % 1000ULL) * 1000ULL);
        selected = select(maximum_fd + 1, &read_set, &write_set, &except_set, &timeout);
        if (selected < 0) {
            if (errno == EINTR) {
                continue;
            }
            web_diagnostic(diagnostic, diagnostic_capacity, "échec de la boucle HTTP locale");
            result = -1;
            break;
        }
        if (*stop_requested == 0 &&
            MHD_run_from_select(daemon, &read_set, &write_set, &except_set) != MHD_YES) {
            web_diagnostic(diagnostic, diagnostic_capacity, "échec du traitement HTTP local");
            result = -1;
            break;
        }
    }
    MHD_stop_daemon(daemon);
    if (context.worker_pid > 0) {
        int worker_status;
        pid_t waited = waitpid(context.worker_pid, &worker_status, WNOHANG);
        if (waited == 0) {
            size_t attempt;
            struct timespec delay = {0, 100000000L};
            (void)kill(-context.worker_pid, SIGTERM);
            for (attempt = 0U; attempt < 50U; ++attempt) {
                waited = waitpid(context.worker_pid, &worker_status, WNOHANG);
                if (waited == context.worker_pid || waited < 0) {
                    break;
                }
                (void)nanosleep(&delay, NULL);
            }
            if (waited == 0) {
                (void)kill(-context.worker_pid, SIGKILL);
                (void)waitpid(context.worker_pid, &worker_status, 0);
            }
        }
    }
    return result;
}
