#include "trainlog/web_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <microhttpd.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>

#include "trainlog/web_dashboard.h"
#include "web_assets.h"

#define TRAINLOG_HTTP_CONNECTION_LIMIT 32U
#define TRAINLOG_HTTP_PER_IP_LIMIT 32U
#define TRAINLOG_HTTP_CONNECTION_MEMORY_LIMIT (16U * 1024U)
#define TRAINLOG_HTTP_HEADER_LIMIT (8U * 1024U)
#define TRAINLOG_HTTP_BODY_LIMIT (4U * 1024U)
#define TRAINLOG_HTTP_TIMEOUT_SECONDS 10U
#define TRAINLOG_HTTP_BACKLOG 32U
#define TRAINLOG_HTTP_POLL_MAX_MS 250ULL

typedef struct TrainlogWebContext {
    uint16_t port;
    TrainlogDatabase *database;
} TrainlogWebContext;

typedef struct TrainlogHttpRequestState {
    size_t body_size;
    bool body_too_large;
} TrainlogHttpRequestState;

static void web_diagnostic(char *output, size_t capacity, const char *message)
{
    if (output != NULL && capacity > 0U)
        (void)snprintf(output, capacity, "%s", message);
}

static enum MHD_Result queue_json(struct MHD_Connection *connection,
    unsigned int status, const char *body, const char *allow)
{
    struct MHD_Response *response;
    enum MHD_Result result;
    response = MHD_create_response_from_buffer(strlen(body), (void *)body,
        MHD_RESPMEM_MUST_COPY);
    if (response == NULL) return MHD_NO;
    if (MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE,
            "application/json; charset=utf-8") != MHD_YES ||
        MHD_add_response_header(response,
            MHD_HTTP_HEADER_X_CONTENT_TYPE_OPTIONS, "nosniff") != MHD_YES ||
        MHD_add_response_header(response, MHD_HTTP_HEADER_CACHE_CONTROL,
            "no-store") != MHD_YES ||
        MHD_add_response_header(response, "Content-Security-Policy",
            "default-src 'none'; frame-ancestors 'none'; base-uri 'none'") !=
            MHD_YES ||
        MHD_add_response_header(response, MHD_HTTP_HEADER_CONNECTION,
            "close") != MHD_YES) {
        MHD_destroy_response(response);
        return MHD_NO;
    }
    if (allow != NULL && MHD_add_response_header(response,
            MHD_HTTP_HEADER_ALLOW, allow) != MHD_YES) {
        MHD_destroy_response(response);
        return MHD_NO;
    }
    result = MHD_queue_response(connection, status, response);
    MHD_destroy_response(response);
    return result;
}

static enum MHD_Result queue_asset(struct MHD_Connection *connection,
    const TrainlogWebAsset *asset)
{
    static const char CSP[] = "default-src 'self'; script-src 'self'; "
        "style-src 'self'; connect-src 'self'; img-src 'self'; "
        "object-src 'none'; base-uri 'none'; frame-ancestors 'none'";
    const char *cache = asset->immutable
        ? "public, max-age=31536000, immutable" : "no-cache";
    struct MHD_Response *response = MHD_create_response_from_buffer(asset->size,
        (void *)asset->bytes, MHD_RESPMEM_PERSISTENT);
    enum MHD_Result result;
    if (response == NULL) return MHD_NO;
    if (MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE,
            asset->mime) != MHD_YES ||
        MHD_add_response_header(response,
            MHD_HTTP_HEADER_X_CONTENT_TYPE_OPTIONS, "nosniff") != MHD_YES ||
        MHD_add_response_header(response, MHD_HTTP_HEADER_CACHE_CONTROL,
            cache) != MHD_YES ||
        MHD_add_response_header(response, MHD_HTTP_HEADER_ETAG,
            asset->etag) != MHD_YES ||
        MHD_add_response_header(response, "Content-Security-Policy", CSP) !=
            MHD_YES ||
        MHD_add_response_header(response, MHD_HTTP_HEADER_CONNECTION,
            "close") != MHD_YES) {
        MHD_destroy_response(response);
        return MHD_NO;
    }
    result = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    return result;
}

static bool is_ui_route(const char *url)
{
    static const char *const routes[] = {
        "/", "/analyse", "/programmes", "/seances", "/exercices"
    };
    size_t index;
    for (index = 0U; index < sizeof(routes) / sizeof(routes[0]); ++index)
        if (strcmp(url, routes[index]) == 0) return true;
    return false;
}

static bool valid_host(const TrainlogWebContext *context, const char *host)
{
    char expected[32];
    int written;
    if (host == NULL) return false;
    if (strcmp(host, "127.0.0.1") == 0) return true;
    written = snprintf(expected, sizeof(expected), "127.0.0.1:%u",
        (unsigned int)context->port);
    return written > 0 && (size_t)written < sizeof(expected) &&
        strcmp(host, expected) == 0;
}

static bool declared_body_too_large(struct MHD_Connection *connection)
{
    const char *value = MHD_lookup_connection_value(connection,
        MHD_HEADER_KIND, MHD_HTTP_HEADER_CONTENT_LENGTH);
    char *end = NULL;
    unsigned long long size;
    if (value == NULL) return false;
    errno = 0;
    size = strtoull(value, &end, 10);
    return errno != 0 || end == value || *end != '\0' ||
        size > TRAINLOG_HTTP_BODY_LIMIT;
}

static enum MHD_Result handle_request(void *closure,
    struct MHD_Connection *connection, const char *url, const char *method,
    const char *version, const char *upload_data, size_t *upload_data_size,
    void **request_closure)
{
    static const char HEALTH[] =
        "{\"api_version\":1,\"status\":\"ok\",\"product\":\"trainlog\","
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
        if (state == NULL) return MHD_NO;
        state->body_too_large = declared_body_too_large(connection);
        *request_closure = state;
        return MHD_YES;
    }
    if (*upload_data_size > 0U) {
        if (*upload_data_size > TRAINLOG_HTTP_BODY_LIMIT -
                (state->body_size > TRAINLOG_HTTP_BODY_LIMIT
                    ? TRAINLOG_HTTP_BODY_LIMIT : state->body_size))
            state->body_too_large = true;
        else state->body_size += *upload_data_size;
        (void)upload_data;
        *upload_data_size = 0U;
        return MHD_YES;
    }
    header_info = MHD_get_connection_info(connection,
        MHD_CONNECTION_INFO_REQUEST_HEADER_SIZE);
    if (header_info == NULL || header_info->header_size >
            TRAINLOG_HTTP_HEADER_LIMIT)
        return queue_json(connection, MHD_HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE,
            "{\"error\":\"headers_too_large\"}\n", NULL);
    if (state->body_too_large)
        return queue_json(connection, MHD_HTTP_CONTENT_TOO_LARGE,
            "{\"error\":\"body_too_large\"}\n", NULL);
    host = MHD_lookup_connection_value(connection, MHD_HEADER_KIND,
        MHD_HTTP_HEADER_HOST);
    if (!valid_host(context, host))
        return queue_json(connection, MHD_HTTP_BAD_REQUEST,
            "{\"error\":\"invalid_host\"}\n", NULL);
    is_get = strcmp(method, MHD_HTTP_METHOD_GET) == 0;
    is_head = strcmp(method, MHD_HTTP_METHOD_HEAD) == 0;
    if (strcmp(url, "/api/v1/health") == 0) {
        if (!is_get)
            return queue_json(connection, MHD_HTTP_METHOD_NOT_ALLOWED,
                "{\"error\":\"method_not_allowed\"}\n", "GET");
        return queue_json(connection, MHD_HTTP_OK, HEALTH, NULL);
    }
    if (strcmp(url, "/api/v1/dashboard") == 0) {
        TrainlogWebDashboardQuery query;
        TrainlogWebDashboardSnapshot snapshot;
        char json[TRAINLOG_WEB_JSON_CAPACITY];
        size_t json_size;
        time_t now;
        if (!is_get)
            return queue_json(connection, MHD_HTTP_METHOD_NOT_ALLOWED,
                "{\"error\":\"method_not_allowed\"}\n", "GET");
        now = time(NULL);
        if (now == (time_t)-1 || (int64_t)now != (int64_t)(time_t)now)
            return queue_json(connection, MHD_HTTP_INTERNAL_SERVER_ERROR,
                "{\"error\":\"clock_unavailable\"}\n", NULL);
        query.reference_unix_second = (int64_t)now;
        if (trainlog_web_dashboard_load(context->database, &query,
                &snapshot) != TRAINLOG_STATUS_OK ||
            trainlog_web_dashboard_serialize(&snapshot, json, sizeof(json),
                &json_size) != TRAINLOG_STATUS_OK || json_size == 0U)
            return queue_json(connection, MHD_HTTP_INTERNAL_SERVER_ERROR,
                "{\"error\":\"dashboard_unavailable\"}\n", NULL);
        return queue_json(connection, MHD_HTTP_OK, json, NULL);
    }
    /* INVARIANT: API paths never fall through to the SPA index. */
    if (strncmp(url, "/api/", 5U) == 0)
        return queue_json(connection, MHD_HTTP_NOT_FOUND,
            "{\"error\":\"not_found\"}\n", NULL);
    if (strstr(url, "..") != NULL)
        return queue_json(connection, MHD_HTTP_NOT_FOUND,
            "{\"error\":\"not_found\"}\n", NULL);
    asset = is_ui_route(url) ? trainlog_web_index_asset()
        : trainlog_web_asset_find(url);
    if (asset == NULL)
        return queue_json(connection, MHD_HTTP_NOT_FOUND,
            "{\"error\":\"not_found\"}\n", NULL);
    if (!is_get && !is_head)
        return queue_json(connection, MHD_HTTP_METHOD_NOT_ALLOWED,
            "{\"error\":\"method_not_allowed\"}\n", "GET, HEAD");
    return queue_asset(connection, asset);
}

static void request_completed(void *closure, struct MHD_Connection *connection,
    void **request_closure, enum MHD_RequestTerminationCode termination)
{
    (void)closure;
    (void)connection;
    (void)termination;
    free(*request_closure);
    *request_closure = NULL;
}

static enum MHD_Result accept_loopback_only(void *closure,
    const struct sockaddr *address, socklen_t address_length)
{
    const struct sockaddr_in *ipv4;
    (void)closure;
    if (address == NULL || address_length < (socklen_t)sizeof(*ipv4) ||
        address->sa_family != AF_INET) return MHD_NO;
    ipv4 = (const struct sockaddr_in *)address;
    return ipv4->sin_addr.s_addr == htonl(INADDR_LOOPBACK) ? MHD_YES : MHD_NO;
}

int trainlog_web_server_run(TrainlogDatabase *database, uint16_t port,
    const volatile sig_atomic_t *stop_requested,
    char *diagnostic, size_t diagnostic_capacity)
{
    struct sockaddr_in bind_address;
    struct MHD_Daemon *daemon;
    TrainlogWebContext context;
    int result = 0;
    if (database == NULL || stop_requested == NULL) return -1;
    web_diagnostic(diagnostic, diagnostic_capacity, "");
    if (!trainlog_web_assets_available()) {
        web_diagnostic(diagnostic, diagnostic_capacity,
            "support frontend absent de ce binaire");
        return -1;
    }
    (void)memset(&bind_address, 0, sizeof(bind_address));
    bind_address.sin_family = AF_INET;
    bind_address.sin_port = htons(port);
    bind_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    context.port = port;
    context.database = database;
    errno = 0;
    daemon = MHD_start_daemon(MHD_USE_ERROR_LOG | MHD_USE_NO_THREAD_SAFETY,
        0U, accept_loopback_only, NULL, handle_request, &context,
        MHD_OPTION_SOCK_ADDR_LEN, (socklen_t)sizeof(bind_address), &bind_address,
        MHD_OPTION_CONNECTION_LIMIT, TRAINLOG_HTTP_CONNECTION_LIMIT,
        MHD_OPTION_PER_IP_CONNECTION_LIMIT, TRAINLOG_HTTP_PER_IP_LIMIT,
        MHD_OPTION_CONNECTION_MEMORY_LIMIT,
            (size_t)TRAINLOG_HTTP_CONNECTION_MEMORY_LIMIT,
        MHD_OPTION_CONNECTION_TIMEOUT, TRAINLOG_HTTP_TIMEOUT_SECONDS,
        MHD_OPTION_LISTEN_BACKLOG_SIZE, TRAINLOG_HTTP_BACKLOG,
        MHD_OPTION_NOTIFY_COMPLETED, request_completed, NULL,
        MHD_OPTION_END);
    if (daemon == NULL) {
        char message[256];
        (void)snprintf(message, sizeof(message),
            "impossible d'écouter sur 127.0.0.1:%u%s%s",
            (unsigned int)port, errno == 0 ? "" : " : ",
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
        FD_ZERO(&read_set);
        FD_ZERO(&write_set);
        FD_ZERO(&except_set);
        if (MHD_get_fdset(daemon, &read_set, &write_set, &except_set,
                &maximum_fd) != MHD_YES) {
            web_diagnostic(diagnostic, diagnostic_capacity,
                "impossible de préparer la boucle HTTP");
            result = -1;
            break;
        }
        if (MHD_get_timeout(daemon, &timeout_ms) != MHD_YES ||
            timeout_ms > TRAINLOG_HTTP_POLL_MAX_MS)
            timeout_ms = TRAINLOG_HTTP_POLL_MAX_MS;
        timeout.tv_sec = (time_t)(timeout_ms / 1000ULL);
        timeout.tv_usec = (suseconds_t)((timeout_ms % 1000ULL) * 1000ULL);
        selected = select(maximum_fd + 1, &read_set, &write_set, &except_set,
            &timeout);
        if (selected < 0) {
            if (errno == EINTR) continue;
            web_diagnostic(diagnostic, diagnostic_capacity,
                "échec de la boucle HTTP locale");
            result = -1;
            break;
        }
        if (*stop_requested == 0 && MHD_run_from_select(daemon, &read_set,
                &write_set, &except_set) != MHD_YES) {
            web_diagnostic(diagnostic, diagnostic_capacity,
                "échec du traitement HTTP local");
            result = -1;
            break;
        }
    }
    MHD_stop_daemon(daemon);
    return result;
}
